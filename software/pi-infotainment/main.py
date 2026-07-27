#!/usr/bin/env python3
"""
main.py (pi-infotainment)
Servicio de infoentretenimiento - dominio NO critico (BOM Sistema E,
DOC-PH1-ARQ-001 Seccion 7).

Programa: [Marca] - Defender Restomod

Este archivo cubre la PLOMERIA de datos (recepcion de temperatura/humedad
de cabina, GPS, heartbeat) - NO implementa la interfaz grafica completa
de infoentretenimiento (menus, navegador de medios, etc.), que es un
proyecto de UI aparte fuera del alcance de esta pasada. Se deja como
TODO explicito al final de este archivo.

La camara de reversa (BOM item E.5) usa una senal fisica directa del
cable de luz de reversa del vehiculo (ver reverse_trigger.py), no el
protocolo interno STM32<->Pi - el mensaje de marcha (MSG_ID 0x14) es
dominio critico y solo se envia a Pi 1, por eso Pi 2 no depende de el
para saber cuando activar la camara.
"""

import threading
import time

import config
from proto_reader import ProtoReader, MsgId, decode_i8, decode_u8
from heartbeat import HeartbeatLink
from proto_reader import NodeId
from gps_reader import GpsReader
from reverse_trigger import ReverseTrigger

CABIN_TIMEOUT_S = 2.0  # dato no critico, timeout mas relajado que el de Pi 1


class CabinState:
    def __init__(self):
        self.temp_c = None
        self.humidity_pct = None
        self.last_update = 0.0

    def is_stale(self):
        if self.last_update == 0.0:
            return True
        return (time.monotonic() - self.last_update) > CABIN_TIMEOUT_S


def sensor_reader_thread(cabin: CabinState, stop_event: threading.Event):
    """Solo procesa los MSG_ID no criticos del Nodo 2 (temp/humedad de
    cabina) - ignora silenciosamente cualquier otro mensaje que llegue
    por este puerto, ya que los criticos van dirigidos a Pi 1."""
    reader = ProtoReader(config.SENSOR_PORT, baudrate=config.SENSOR_BAUDRATE)
    try:
        for frame in reader.read_frames():
            if stop_event.is_set():
                break
            if frame.msg_id == MsgId.CABIN_TEMP:
                cabin.temp_c = decode_i8(frame.payload)
                cabin.last_update = time.monotonic()
            elif frame.msg_id == MsgId.CABIN_HUMIDITY:
                cabin.humidity_pct = decode_u8(frame.payload)
                cabin.last_update = time.monotonic()
    finally:
        reader.close()


def main():
    stop_event = threading.Event()
    cabin = CabinState()

    reader_thread = threading.Thread(
        target=sensor_reader_thread, args=(cabin, stop_event), daemon=True)
    reader_thread.start()

    heartbeat = HeartbeatLink(config.HEARTBEAT_PORT, own_node_id=NodeId.PI2,
                               baudrate=config.HEARTBEAT_BAUDRATE)
    heartbeat.start()

    gps = GpsReader(config.GPS_PORT, baudrate=config.GPS_BAUDRATE)
    gps.start()

    def _on_reverse_change(active: bool):
        # TODO(software): reemplazar este print por la activacion real
        # del feed de la camara (BOM E.5 + E.6, adaptador de captura USB)
        # una vez que exista la UI de infoentretenimiento.
        print(f"[REVERSA] {'ACTIVADA - mostrar camara' if active else 'desactivada'}")

    reverse_trigger = ReverseTrigger(on_change=_on_reverse_change)

    print("[Marca] Infoentretenimiento - servicio iniciado")
    print("Presiona Ctrl+C para detener (modo consola de banco)")

    try:
        while True:
            status_parts = []

            if cabin.is_stale():
                status_parts.append("Cabina: NO DATA")
            else:
                status_parts.append(f"Cabina: {cabin.temp_c}C / {cabin.humidity_pct}%RH")

            if gps.fix.is_stale() or not gps.fix.fix_valid:
                status_parts.append("GPS: sin fix")
            else:
                status_parts.append(
                    f"GPS: {gps.fix.latitude:.5f},{gps.fix.longitude:.5f} "
                    f"{gps.fix.speed_kmh:.0f}km/h"
                )

            status_parts.append(f"Pi1: {'OK' if heartbeat.is_peer_alive() else 'SIN RESPUESTA'}")
            status_parts.append(f"Reversa: {'SI' if reverse_trigger.is_reverse_active else 'no'}")

            print(" | ".join(status_parts))
            time.sleep(1.0)

    except KeyboardInterrupt:
        pass
    finally:
        stop_event.set()
        heartbeat.stop()
        gps.stop()
        reverse_trigger.close()
        print("\nServicio detenido.")


# ---------------------------------------------------------------------------
# TODO(software): Audio (HiFiBerry DAC2 Pro, BOM E.3)
#   La reproduccion de audio en si es mayormente configuracion de ALSA a
#   nivel de sistema operativo (overlay de dtoverlay en config.txt +
#   asound.conf), no codigo de aplicacion. Falta: integrar un reproductor
#   (ej. mpv o python-vlc) controlado desde la futura UI de infoentreten-
#   imiento, y el control de volumen fisico si se agrega uno.
#
# RESUELTO (23 jul 2026): Camara de reversa (BOM E.5) - trigger de marcha
#   Se decidio usar la senal fisica directa del cable de luz de reversa
#   del vehiculo (opcion c), implementada en reverse_trigger.py, en vez
#   de depender del protocolo interno STM32<->Pi. Falta unicamente:
#   conectar la activacion real del feed de la camara (hoy solo imprime
#   en consola) una vez que exista la UI de infoentretenimiento.
#
# TODO(software): interfaz grafica completa de infoentretenimiento
#   Este archivo solo mantiene el estado (cabina, GPS, heartbeat) en
#   consola para pruebas de banco. Falta la UI real (t\u00e1ctil, men\u00fas,
#   mapa, reproductor) sobre el panel/pantalla de Pi 2.
# ---------------------------------------------------------------------------

if __name__ == '__main__':
    main()
