# Nodo 3 — Simulador de Tráfico CAN (herramienta de banco)

**Documento relacionado:** [DOC-PH1-PROTO-001](../../docs/proto/DOC-PH1-PROTO-001.md)
**Placa objetivo:** STM32 Nucleo-G474RE
**Estado:** En desarrollo — Rev. 0.1

> ⚠️ **Este nodo no se instala en el vehículo de producción.** Su única función
> es generar tráfico CAN sintético para validar el Nodo 1 (parser GMLAN) y el
> software de la Raspberry Pi #1 antes de conectar el arnés real del LS3/6L80.

## ⚠️ Advertencia sobre los IDs de CAN

Los identificadores CAN usados en este simulador (`can_sim_config.h`) son
**marcadores de posición**, no los IDs reales del bus GMLAN de GM. Los IDs
reales son propietarios y deben capturarse con el analizador PCAN-USB Pro FD
(BOM ítem B.4) una vez que el motor esté disponible. Ver el encabezado de
`can_sim_config.h` para el detalle completo.

## Qué hace este firmware

- Genera tramas CAN clásicas (no CAN-FD) a intervalos que imitan la frecuencia
  esperada del bus real (20 Hz para RPM/velocidad, 2 Hz para temperatura,
  presión de aceite y marcha — mismos rangos que `DOC-PH1-PROTO-001`).
- Simula un ciclo de RPM en rampa (sube y baja entre ralentí y un límite
  superior), para ejercitar al Nodo 1 con datos que cambian, no valores fijos.
- Permite forzar la inyección de un código de falla (DTC) simulado, para
  probar el manejo de errores del Nodo 1 sin depender de un evento real.

## Estructura de archivos

- `Inc/can_sim_config.h` — IDs de CAN, parámetros de simulación (placeholders)
- `Inc/can_sim.h` — interfaz pública del módulo
- `Src/can_sim.c` — lógica del simulador

Estos archivos **no son un proyecto CubeIDE completo** — están pensados para
integrarse dentro de un proyecto ya generado con STM32CubeMX para la
Nucleo-G474RE, con el periférico **FDCAN1** habilitado en modo *Classic CAN*.

## Pasos de integración

1. En STM32CubeMX, crea un proyecto para la Nucleo-G474RE y habilita FDCAN1
   en modo Classic CAN (bit rate 500 kbit/s, típico de GMLAN de alta
   velocidad — confirmar contra la captura real cuando esté disponible).
2. Genera el código — esto crea `fdcan.h`/`fdcan.c` con la variable global
   `hfdcan1` que `can_sim.h` espera encontrar.
3. Copia `Inc/can_sim_config.h` e `Inc/can_sim.h` a la carpeta `Inc/` del
   proyecto generado, y `Src/can_sim.c` a la carpeta `Src/`.
4. En `main.c`, agrega:

```c
#include "can_sim.h"

/* Dentro de main(), después de MX_FDCAN1_Init(): */
if (!CanSim_Init()) {
    Error_Handler();
}

/* Dentro del while(1) del loop principal: */
CanSim_Task(HAL_GetTick());
```

5. Compila y flashea. El LED de actividad de FDCAN (si está mapeado en tu
   configuración de CubeMX) debería parpadear a ~20 Hz.

## Próximos pasos

- [ ] Reemplazar los IDs placeholder con captura real del bus GMLAN
      (requiere PEAK-System PCAN-USB Pro FD + motor donante disponible).
- [ ] Definir el layout exacto de payload de cada mensaje una vez conocido
      el formato real (actualmente son suposiciones documentadas inline).
- [ ] Conectar la salida CAN de este nodo a la entrada CAN del Nodo 1 y
      confirmar que el Nodo 1 produce tramas UART válidas según
      `DOC-PH1-PROTO-001` en el osciloscopio/analizador lógico.
