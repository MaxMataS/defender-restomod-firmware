"""
config.py (pi-infotainment)
Configuracion de puertos serie.
"""

# Datos no criticos de Nodo 2 (temperatura/humedad de cabina)
SENSOR_PORT = '/dev/ttyAMA0'
SENSOR_BAUDRATE = 115200

# Heartbeat hacia/desde Pi 1
HEARTBEAT_PORT = '/dev/ttyAMA1'
HEARTBEAT_BAUDRATE = 9600

# GPS SparkFun NEO-M9N (BOM item E.1)
GPS_PORT = '/dev/ttyAMA2'
GPS_BAUDRATE = 9600
