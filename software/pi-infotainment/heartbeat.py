"""
heartbeat.py (pi-infotainment)
Version simetrica de pi-cluster/heartbeat.py, con NodeId.PI2 como rol
propio por defecto. Ver DOC-PH1-PROTO-001 Seccion 8 para el protocolo
completo - la logica es identica, solo cambia que NODE_ID se usa para
identificarse a si mismo.
"""

import threading
import time

import serial

from proto_reader import START_BYTE, END_BYTE, NodeId, MsgId, compute_checksum

HEARTBEAT_INTERVAL_S = 1.0
HEARTBEAT_TIMEOUT_S = 3.0


class HeartbeatLink:
    def __init__(self, port: str, own_node_id: int = NodeId.PI2,
                 baudrate: int = 9600):
        self._ser = serial.Serial(port, baudrate=baudrate, timeout=0.1)
        self._own_node_id = own_node_id
        self._own_counter = 0
        self._last_peer_heartbeat = 0.0
        self._lock = threading.Lock()
        self._stop = threading.Event()

    def start(self):
        threading.Thread(target=self._tx_loop, daemon=True).start()
        threading.Thread(target=self._rx_loop, daemon=True).start()

    def stop(self):
        self._stop.set()

    def is_peer_alive(self) -> bool:
        with self._lock:
            if self._last_peer_heartbeat == 0.0:
                return False
            return (time.monotonic() - self._last_peer_heartbeat) < HEARTBEAT_TIMEOUT_S

    def _tx_loop(self):
        while not self._stop.is_set():
            self._send_heartbeat()
            time.sleep(HEARTBEAT_INTERVAL_S)

    def _send_heartbeat(self):
        payload = bytes([self._own_counter & 0xFF])
        checksum = compute_checksum(self._own_node_id, MsgId.HEARTBEAT,
                                     len(payload), payload)
        frame = bytes([START_BYTE, self._own_node_id, MsgId.HEARTBEAT,
                        len(payload)]) + payload + bytes([checksum, END_BYTE])
        self._ser.write(frame)
        self._own_counter = (self._own_counter + 1) & 0xFF

    def _rx_loop(self):
        buf = bytearray()
        while not self._stop.is_set():
            chunk = self._ser.read(16)
            if not chunk:
                continue
            buf.extend(chunk)
            idx = buf.find(bytes([START_BYTE]))
            if idx == -1:
                buf.clear()
                continue
            if idx > 0:
                del buf[:idx]
            if len(buf) < 7:
                continue
            node_id, msg_id, length = buf[1], buf[2], buf[3]
            if msg_id == MsgId.HEARTBEAT and length == 1 and buf[6] == END_BYTE:
                with self._lock:
                    self._last_peer_heartbeat = time.monotonic()
            del buf[:7]
