"""
config.py (pi-cluster)
Configuracion de puertos serie y pantalla.

Ajustar SENSOR_PORT y HEARTBEAT_PORT segun la asignacion real de UARTs
de la Raspberry Pi (BOM item A.3 usa GPIO UART, confirmar en /dev/serial
o dtoverlay de config.txt).
"""

# Puerto donde llegan las tramas de datos de Nodo 1 y Nodo 2
SENSOR_PORT = '/dev/ttyAMA0'
SENSOR_BAUDRATE = 115200

# Puerto del heartbeat hacia/desde Pi 2 (enlace aparte, ver
# DOC-PH1-PROTO-001 Seccion 8)
HEARTBEAT_PORT = '/dev/ttyAMA1'
HEARTBEAT_BAUDRATE = 9600

# Panel LESOWN M141-A01T (BOM item C.1) - SOLO PROTOTIPO, no produccion
SCREEN_WIDTH = 1920
SCREEN_HEIGHT = 550
FULLSCREEN = True
TARGET_FPS = 30
