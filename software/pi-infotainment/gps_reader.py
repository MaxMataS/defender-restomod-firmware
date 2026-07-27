"""
gps_reader.py (pi-infotainment)
Lector del GPS SparkFun NEO-M9N (BOM item E.1).

Parsea las sentencias NMEA GGA (posicion/altitud) y RMC (velocidad/rumbo)
mas comunes. El NEO-M9N habla NMEA por UART a 9600 baudios por defecto de
fabrica - se puede subir la tasa de refresco configurando el modulo con
u-center, fuera del alcance de este archivo.

No requiere protocolo interno propio (DOC-PH1-PROTO-001) porque el GPS
se consume directamente dentro de Pi 2, no se retransmite a otro nodo.
"""

import threading
import time
from dataclasses import dataclass
from typing import Optional

import serial


@dataclass
class GpsFix:
    latitude: Optional[float] = None
    longitude: Optional[float] = None
    speed_kmh: Optional[float] = None
    course_deg: Optional[float] = None
    fix_valid: bool = False
    last_update: float = 0.0

    def is_stale(self, timeout_s: float = 2.0) -> bool:
        if self.last_update == 0.0:
            return True
        return (time.monotonic() - self.last_update) > timeout_s


def _nmea_to_decimal(raw: str, direction: str) -> Optional[float]:
    """Convierte formato NMEA ddmm.mmmm a grados decimales."""
    if not raw:
        return None
    try:
        if direction in ('N', 'S'):
            degrees = float(raw[:2])
            minutes = float(raw[2:])
        else:  # E, W - longitud tiene 3 digitos de grados
            degrees = float(raw[:3])
            minutes = float(raw[3:])
        decimal = degrees + minutes / 60.0
        if direction in ('S', 'W'):
            decimal = -decimal
        return decimal
    except (ValueError, IndexError):
        return None


class GpsReader:
    """Lee continuamente el puerto serie del NEO-M9N y mantiene el
    ultimo GpsFix conocido. Corre en su propio hilo, igual que
    ProtoReader, para no bloquear el hilo principal."""

    def __init__(self, port: str, baudrate: int = 9600):
        self._ser = serial.Serial(port, baudrate=baudrate, timeout=0.5)
        self.fix = GpsFix()
        self._stop = threading.Event()

    def start(self):
        threading.Thread(target=self._read_loop, daemon=True).start()

    def stop(self):
        self._stop.set()

    def _read_loop(self):
        while not self._stop.is_set():
            try:
                line = self._ser.readline().decode('ascii', errors='ignore').strip()
            except serial.SerialException:
                continue
            if not line.startswith('$'):
                continue
            if 'RMC' in line:
                self._parse_rmc(line)
            elif 'GGA' in line:
                self._parse_gga(line)

    def _parse_rmc(self, line: str):
        # $GPRMC/$GNRMC,time,status,lat,N/S,lon,E/W,speed_knots,course,date,...
        fields = line.split(',')
        if len(fields) < 10:
            return
        status = fields[2]
        if status != 'A':  # 'A' = valido, 'V' = invalido
            self.fix.fix_valid = False
            return
        lat = _nmea_to_decimal(fields[3], fields[4])
        lon = _nmea_to_decimal(fields[5], fields[6])
        try:
            speed_knots = float(fields[7]) if fields[7] else 0.0
            course = float(fields[8]) if fields[8] else None
        except ValueError:
            speed_knots, course = 0.0, None

        self.fix.latitude = lat
        self.fix.longitude = lon
        self.fix.speed_kmh = speed_knots * 1.852  # nudos -> km/h
        self.fix.course_deg = course
        self.fix.fix_valid = True
        self.fix.last_update = time.monotonic()

    def _parse_gga(self, line: str):
        # Complementa RMC con calidad de fix; posicion ya viene de RMC
        fields = line.split(',')
        if len(fields) < 7:
            return
        try:
            fix_quality = int(fields[6]) if fields[6] else 0
        except ValueError:
            fix_quality = 0
        self.fix.fix_valid = fix_quality > 0
        self.fix.last_update = time.monotonic()
