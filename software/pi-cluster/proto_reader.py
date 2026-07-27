"""
proto_reader.py
Lector del protocolo interno STM32 <-> Raspberry Pi (DOC-PH1-PROTO-001, rev. 0.2)

Programa: [Marca] - Defender Restomod
Documento relacionado: DOC-PH1-PROTO-001 (protocolo interno)

Es el lado RECEPTOR del protocolo cuyo lado emisor esta implementado en
firmware/*/Src/uart_proto.c. Implementa la maquina de estados para
encontrar tramas validas dentro del flujo de bytes de un puerto serie,
igual que describe DOC-PH1-PROTO-001 Seccion 7 (perdida de sincronizacion:
descartar bytes hasta encontrar el siguiente START valido con checksum
correcto).

Este mismo modulo se usa tanto en pi-cluster/ como en pi-infotainment/ -
es identico en ambos, duplicado por simplicidad en vez de empaquetado
como libreria compartida (se puede refactorizar mas adelante si el
proyecto crece).
"""

import struct
import time
from dataclasses import dataclass
from enum import IntEnum

import serial

# ---------------------------------------------------------------------------
# Constantes de trama (DOC-PH1-PROTO-001 Seccion 3)
# ---------------------------------------------------------------------------
START_BYTE = 0xAA
END_BYTE = 0x55
MAX_PAYLOAD_LEN = 32


class NodeId(IntEnum):
    """DOC-PH1-PROTO-001 Seccion 3.1"""
    NODO1 = 0x01
    NODO2 = 0x02
    NODO3 = 0x03  # solo Fase 1, no producción
    PI1 = 0x10
    PI2 = 0x20


class MsgId(IntEnum):
    """DOC-PH1-PROTO-001 Seccion 4 (rev. 0.2)"""
    HEARTBEAT = 0x00
    # --- Nodo 1: dominio critico (Seccion 4.1) ---
    RPM = 0x10
    SPEED = 0x11
    COOLANT_TEMP = 0x12
    OIL_PRESSURE = 0x13
    GEAR = 0x14
    DTC = 0x15
    # --- Nodo 2: dominio mixto (Seccion 4.2) ---
    PITCH = 0x20
    ROLL = 0x21
    CABIN_TEMP = 0x22
    CABIN_HUMIDITY = 0x23
    TPMS_FL = 0x24
    TPMS_FR = 0x25
    TPMS_RL = 0x26
    TPMS_RR = 0x27


@dataclass
class Frame:
    node_id: int
    msg_id: int
    payload: bytes
    received_at: float  # time.monotonic(), para calculo de timeouts (Sec. 7)


def compute_checksum(node_id: int, msg_id: int, length: int, payload: bytes) -> int:
    """XOR de NODE_ID..PAYLOAD, igual que Proto_ComputeChecksum en el firmware."""
    checksum = node_id ^ msg_id ^ length
    for b in payload:
        checksum ^= b
    return checksum & 0xFF


class ProtoReader:
    """
    Envuelve un puerto serie y entrega tramas validas ya parseadas.

    Uso tipico:
        reader = ProtoReader('/dev/ttyAMA0', baudrate=115200)
        for frame in reader.read_frames():
            ...procesar frame...

    read_frames() es un generador infinito (bloqueante en I/O de puerto
    serie) pensado para correr en su propio hilo o proceso.
    """

    def __init__(self, port: str, baudrate: int = 115200, timeout: float = 0.05):
        self._ser = serial.Serial(port, baudrate=baudrate, timeout=timeout)
        self._buf = bytearray()

    def close(self):
        self._ser.close()

    def read_frames(self):
        """Generador infinito de Frame validos. Descarta silenciosamente
        cualquier byte que no forme parte de una trama valida (Sec. 7)."""
        while True:
            chunk = self._ser.read(64)
            if chunk:
                self._buf.extend(chunk)
            frame = self._try_extract_frame()
            if frame is not None:
                yield frame

    def _try_extract_frame(self):
        """Busca y extrae UNA trama valida del buffer interno, si existe.
        Descarta bytes basura antes del primer START encontrado."""
        # Descarta bytes hasta el siguiente START
        start_idx = self._buf.find(bytes([START_BYTE]))
        if start_idx == -1:
            self._buf.clear()
            return None
        if start_idx > 0:
            del self._buf[:start_idx]

        # Se necesitan al menos 6 bytes (overhead minimo) mas LEN para saber
        # el tamano total de la trama
        if len(self._buf) < 4:
            return None  # esperar mas datos

        node_id = self._buf[1]
        msg_id = self._buf[2]
        length = self._buf[3]

        if length > MAX_PAYLOAD_LEN:
            # LEN corrupto/imposible - descarta el START actual y reintenta
            del self._buf[0:1]
            return None

        total_len = 4 + length + 2  # header + payload + checksum + END
        if len(self._buf) < total_len:
            return None  # trama incompleta, esperar mas bytes

        payload = bytes(self._buf[4:4 + length])
        checksum_rx = self._buf[4 + length]
        end_byte = self._buf[5 + length]

        checksum_calc = compute_checksum(node_id, msg_id, length, payload)

        if end_byte != END_BYTE or checksum_rx != checksum_calc:
            # Trama invalida - descarta solo el START y reintenta desde el
            # siguiente byte (podria haber otro START mas adelante)
            del self._buf[0:1]
            return None

        # Trama valida - consumela del buffer
        del self._buf[0:total_len]
        return Frame(node_id=node_id, msg_id=msg_id, payload=payload,
                     received_at=time.monotonic())


# ---------------------------------------------------------------------------
# Helpers de decodificacion por tipo de mensaje (DOC-PH1-PROTO-001 Seccion 4)
# ---------------------------------------------------------------------------

def decode_u16_be(payload: bytes) -> int:
    return struct.unpack('>H', payload[:2])[0]


def decode_i16_be(payload: bytes) -> int:
    return struct.unpack('>h', payload[:2])[0]


def decode_i8(payload: bytes) -> int:
    return struct.unpack('>b', payload[:1])[0]


def decode_u8(payload: bytes) -> int:
    return payload[0]


def decode_tpms(payload: bytes) -> tuple:
    """Retorna (presion_psi: int, temp_c: int) - Seccion 4.2"""
    pressure = payload[0]
    temp = struct.unpack('>b', payload[1:2])[0]
    return pressure, temp
