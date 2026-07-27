"""
reverse_trigger.py (pi-infotainment)
Deteccion de marcha atras via senal fisica directa del cable de luz de
reversa del vehiculo, aislada opticamente hacia un GPIO de Pi 2.

Programa: [Marca] - Defender Restomod

DECISION DE ARQUITECTURA (23 jul 2026): la camara de reversa (BOM E.5)
necesita saber cuando el vehiculo esta en marcha atras. Ese dato viene
del vehiculo (12V en el cable de luz de reversa cuando la palanca esta
en R), NO del protocolo interno STM32<->Pi ni del MSG_ID de marcha
(0x14), que es dominio critico dirigido unicamente a Pi 1. Se opto
deliberadamente por leer la senal fisica del vehiculo en vez de
depender de que Nodo 1/Pi 1 esten funcionando: la camara de reversa
debe activarse incluso si el resto de la arquitectura de datos fallara.

Hardware requerido (agregar a BOM como item nuevo si no esta ya):
  Modulo aislador optico 12V->3.3V (ej. "The Pi Hut 4-Channel Level
  Converter 12V to 3.3V", o equivalente PC817/EL817). El cable de luz
  de reversa del vehiculo NUNCA debe conectarse directo a un GPIO -
  12V destruiria el pin de inmediato.
"""

import time
from typing import Callable, Optional

from gpiozero import DigitalInputDevice

# Pin BCM donde llega la salida ya aislada/convertida a 3.3V del modulo
# optoacoplador. Ajustar segun cableado real.
REVERSE_GPIO_PIN = 17

# Debounce: el cable de reversa puede tener rebote electrico al conmutar
# el relevador de la palanca de cambios - se ign305an cambios mas rapidos
# que esto para evitar activaciones falsas de la camara.
DEBOUNCE_S = 0.05


class ReverseTrigger:
    """
    Envuelve un DigitalInputDevice de gpiozero configurado para el modulo
    optoacoplador. NOTA sobre polaridad: la mayoria de estos modulos
    invierten la senal (12V presente -> salida en 0 logico). Ajustar
    active_state segun el modulo especifico que se compre - probar en
    banco antes de confiar en el resultado.
    """

    def __init__(self, pin: int = REVERSE_GPIO_PIN,
                 active_when_high: bool = False,
                 on_change: Optional[Callable[[bool], None]] = None):
        self._device = DigitalInputDevice(
            pin, pull_up=(not active_when_high), bounce_time=DEBOUNCE_S)
        self._active_when_high = active_when_high
        self._on_change = on_change

        if on_change is not None:
            self._device.when_activated = self._handle_activated
            self._device.when_deactivated = self._handle_deactivated

    def _handle_activated(self):
        if self._on_change:
            self._on_change(self._active_when_high)

    def _handle_deactivated(self):
        if self._on_change:
            self._on_change(not self._active_when_high)

    @property
    def is_reverse_active(self) -> bool:
        raw = self._device.is_active
        return raw if self._active_when_high else not raw

    def close(self):
        self._device.close()


if __name__ == '__main__':
    # Prueba de banco standalone: corre este archivo solo para verificar
    # que el modulo optoacoplador y el cableado funcionan antes de
    # integrarlo al resto del sistema.
    print("Prueba de banco - reverse_trigger.py")
    print(f"Escuchando GPIO {REVERSE_GPIO_PIN}. Ctrl+C para salir.\n")

    def _print_change(active: bool):
        print(f"[{time.strftime('%H:%M:%S')}] Reversa: {'ACTIVA' if active else 'inactiva'}")

    trigger = ReverseTrigger(on_change=_print_change)
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        trigger.close()
        print("\nPrueba terminada.")
