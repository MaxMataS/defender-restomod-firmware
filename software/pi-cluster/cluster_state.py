"""
cluster_state.py
Estado compartido del cluster de instrumentos (Pi 1).

Programa: [Marca] - Defender Restomod
Documento relacionado: DOC-PH1-PROTO-001 Seccion 7 (manejo de timeouts)

Mantiene el ultimo valor conocido de cada sensor critico, junto con el
timestamp de su ultima actualizacion. gauges.py consulta este estado en
cada frame de render para decidir si mostrar el valor real o el estado
de "dato no disponible" exigido por el protocolo quen un nodo deja de
responder.
"""

import time
from dataclasses import dataclass, field
from typing import Optional

# Timeout critico segun DOC-PH1-PROTO-001 Seccion 7: 500 ms sin mensaje
# valido de un nodo -> el cluster debe mostrar "dato no disponible", nunca
# congelar el ultimo valor sin avisar.
CRITICAL_TIMEOUT_S = 0.5


@dataclass
class SensorValue:
    """Un valor de sensor individual con su marca de tiempo de llegada."""
    value: Optional[float] = None
    last_update: float = field(default_factory=lambda: 0.0)

    def is_stale(self, timeout_s: float = CRITICAL_TIMEOUT_S) -> bool:
        if self.value is None:
            return True
        return (time.monotonic() - self.last_update) > timeout_s

    def update(self, value):
        self.value = value
        self.last_update = time.monotonic()


class ClusterState:
    """
    Estado completo del cluster - un SensorValue por cada mensaje critico
    de DOC-PH1-PROTO-001 Secciones 4.1 y 4.2 (dominio critico unicamente,
    los no criticos -temperatura/humedad de cabina- se manejan en
    pi-infotainment, no aqui).
    """

    def __init__(self):
        # Nodo 1 - motor y transmision
        self.rpm = SensorValue()
        self.speed = SensorValue()
        self.coolant_temp = SensorValue()
        self.oil_pressure = SensorValue()
        self.gear = SensorValue()
        self.dtc = SensorValue()  # no periodico, solo al detectarse falla

        # Nodo 2 - sensores propios, dominio critico
        self.pitch = SensorValue()
        self.roll = SensorValue()
        self.tpms_fl = SensorValue()  # tupla (presion_psi, temp_c)
        self.tpms_fr = SensorValue()
        self.tpms_rl = SensorValue()
        self.tpms_rr = SensorValue()

        # Salud de Nodo 1 y Nodo 2 en su conjunto (para diagnostico en
        # pantalla, no solo por mensaje individual)
        self.last_nodo1_frame = SensorValue()
        self.last_nodo2_frame = SensorValue()

    def apply_frame(self, frame):
        """Despacha un Frame ya parseado hacia el SensorValue que le
        corresponde. Import local de proto_reader para evitar dependencia
        circular en el punto de entrada del modulo."""
        from proto_reader import MsgId, NodeId, decode_u16_be, decode_i16_be, \
            decode_i8, decode_u8, decode_tpms

        if frame.node_id == NodeId.NODO1:
            self.last_nodo1_frame.update(True)
        elif frame.node_id == NodeId.NODO2:
            self.last_nodo2_frame.update(True)

        if frame.msg_id == MsgId.RPM:
            self.rpm.update(decode_u16_be(frame.payload))
        elif frame.msg_id == MsgId.SPEED:
            self.speed.update(decode_u16_be(frame.payload) / 10.0)  # km/h x10
        elif frame.msg_id == MsgId.COOLANT_TEMP:
            self.coolant_temp.update(decode_i8(frame.payload))
        elif frame.msg_id == MsgId.OIL_PRESSURE:
            self.oil_pressure.update(decode_u8(frame.payload))
        elif frame.msg_id == MsgId.GEAR:
            self.gear.update(decode_u8(frame.payload))
        elif frame.msg_id == MsgId.DTC:
            self.dtc.update(decode_u16_be(frame.payload))
        elif frame.msg_id == MsgId.PITCH:
            self.pitch.update(decode_i16_be(frame.payload) / 10.0)  # grados x10
        elif frame.msg_id == MsgId.ROLL:
            self.roll.update(decode_i16_be(frame.payload) / 10.0)
        elif frame.msg_id == MsgId.TPMS_FL:
            self.tpms_fl.update(decode_tpms(frame.payload))
        elif frame.msg_id == MsgId.TPMS_FR:
            self.tpms_fr.update(decode_tpms(frame.payload))
        elif frame.msg_id == MsgId.TPMS_RL:
            self.tpms_rl.update(decode_tpms(frame.payload))
        elif frame.msg_id == MsgId.TPMS_RR:
            self.tpms_rr.update(decode_tpms(frame.payload))
        # MsgId.HEARTBEAT se maneja aparte en heartbeat.py, no aqui
