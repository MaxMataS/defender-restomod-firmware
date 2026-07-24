# DOC-PH1-PROTO-001 — Protocolo de Datos Interno

**Título:** Formato de trama UART/SPI entre nodos STM32 y Raspberry Pi
**Fase del programa:** Fase 1 — Viabilidad, Presupuesto y Adquisición
**Documentos relacionados:** DOC-PH1-ARQ-001 (Arquitectura Técnica del Sistema)
**Revisión:** 0.2 — Corregido catálogo de mensajes del Nodo 2
**Fecha:** 23 de julio de 2026
**Clasificación:** Confidencial — Uso Interno

## Historial de Revisiones

| Rev. | Fecha | Cambios |
|------|-------|---------|
| 0.1 | 19 jul 2026 | Emisión inicial. Define trama base, catálogo de mensajes v1 y checksum. |
| 0.2 | 23 jul 2026 | Auditoría de consistencia: se corrige la Sección 4.2 — el catálogo de mensajes del Nodo 2 estaba desactualizado (mencionaba sensores de presión de aceite redundante, temp. diferencial y suspensión neumática, previos a la decisión de usar IMU + TPMS + temperatura de cabina, sin aire). Se redefine con los sensores realmente definidos en DOC-PH1-BOM-001 Sección 6.1 (Sistema F). |

---

## 1. Alcance

Este documento define el formato exacto de los datos que viajan entre los nodos STM32 (capa de adquisición CAN) y las Raspberry Pi (capa de cómputo), según lo establecido en DOC-PH1-ARQ-001. Cubre:

- Parámetros físicos de la interfaz (UART como enlace primario)
- Estructura de trama
- Catálogo de mensajes con sus IDs, tamaños y unidades
- Cálculo de checksum
- Manejo de errores y timeouts
- Formato del heartbeat entre Raspberry Pi #1 y #2

No cubre el protocolo CAN del vehículo (GMLAN) en sí — eso lo decodifica internamente cada nodo STM32 antes de reempaquetarlo en el formato aquí definido.

## 2. Parámetros Físicos de la Interfaz

| Parámetro | Valor |
|---|---|
| Interfaz primaria | UART |
| Baudrate | 115200 |
| Formato | 8N1 (8 bits de datos, sin paridad, 1 bit de parada) |
| Interfaz de respaldo | SPI (modo 0, 1 MHz) — reservada para si UART satura en fases posteriores |
| Nivel lógico | 3.3V (STM32 y Raspberry Pi son nativamente compatibles, sin necesidad de level shifter) |

## 3. Estructura de Trama

Toda trama sigue este formato fijo:

| Campo | Tamaño | Descripción |
|---|---|---|
| `START` | 1 byte | Byte de sincronización, siempre `0xAA` |
| `NODE_ID` | 1 byte | Identifica el nodo origen (ver Sección 3.1) |
| `MSG_ID` | 1 byte | Identifica el tipo de dato (ver Sección 4) |
| `LEN` | 1 byte | Longitud del payload en bytes (0–32) |
| `PAYLOAD` | 0–32 bytes | Datos del mensaje, formato según Sección 4 |
| `CHECKSUM` | 1 byte | XOR de todos los bytes desde `NODE_ID` hasta el final del `PAYLOAD` (ver Sección 5) |
| `END` | 1 byte | Byte de cierre, siempre `0x55` |

**Tamaño total de trama:** 6 bytes fijos + longitud de payload (máximo 38 bytes).

### 3.1 Identificadores de Nodo (`NODE_ID`)

| Valor | Nodo |
|---|---|
| `0x01` | STM32 Nodo 1 (Parser GMLAN motor + transmisión) |
| `0x02` | STM32 Nodo 2 (Fusión de sensores propios) |
| `0x03` | STM32 Nodo 3 (Simulador de banco — **solo Fase 1, no producción**) |
| `0x10` | Raspberry Pi #1 (Clúster — usado solo en heartbeat, ver Sección 8) |
| `0x20` | Raspberry Pi #2 (Infoentretenimiento — usado solo en heartbeat) |

## 4. Catálogo de Mensajes (`MSG_ID`)

### 4.1 Mensajes del Nodo 1 (GMLAN motor + transmisión) — dominio crítico

| `MSG_ID` | Nombre | Payload | Formato | Rango / Unidad | Frecuencia |
|---|---|---|---|---|---|
| `0x10` | RPM | 2 bytes | uint16, big-endian | 0–8000 rpm | 20 Hz |
| `0x11` | Velocidad | 2 bytes | uint16, big-endian | 0–2000 (km/h × 10) | 20 Hz |
| `0x12` | Temp. refrigerante | 1 byte | int8 | -40 a 150 °C | 2 Hz |
| `0x13` | Presión de aceite | 1 byte | uint8 | 0–150 PSI | 2 Hz |
| `0x14` | Marcha activa | 1 byte | uint8 | 0=P, 1=R, 2=N, 3–8=D1–D6 | 2 Hz (o al cambiar) |
| `0x15` | Código de falla activo | 2 bytes | uint16 | Código DTC crudo del bus GMLAN | Al detectarse |

### 4.2 Mensajes del Nodo 2 (sensores propios de marca) — dominio mixto

Sensores definidos en DOC-PH1-BOM-001 Sección 6.1 (Sistema F): IMU BNO085, kit TPMS 4 ruedas, sensor de temperatura/humedad de cabina SHT31-D. No incluye suspensión neumática (descartada, ver DOC-000-BRAND-001 Sección 3.1).

| `MSG_ID` | Nombre | Payload | Formato | Rango / Unidad | Frecuencia | Dominio |
|---|---|---|---|---|---|---|
| `0x20` | Pitch (inclinación longitudinal) | 2 bytes | int16, big-endian | -900 a 900 (grados × 10) | 10 Hz | Crítico → Pi 1 |
| `0x21` | Roll (inclinación lateral) | 2 bytes | int16, big-endian | -900 a 900 (grados × 10) | 10 Hz | Crítico → Pi 1 |
| `0x22` | Temperatura de cabina | 1 byte | int8 | -40 a 85 °C | 1 Hz | No crítico → Pi 2 |
| `0x23` | Humedad de cabina | 1 byte | uint8 | 0–100 %RH | 1 Hz | No crítico → Pi 2 |
| `0x24` | Presión + temp. neumático — Delantero Izq. (FL) | 2 bytes | uint8 PSI + int8 °C | 0–150 PSI / -40 a 85 °C | 1 Hz | Crítico → Pi 1 |
| `0x25` | Presión + temp. neumático — Delantero Der. (FR) | 2 bytes | uint8 PSI + int8 °C | 0–150 PSI / -40 a 85 °C | 1 Hz | Crítico → Pi 1 |
| `0x26` | Presión + temp. neumático — Trasero Izq. (RL) | 2 bytes | uint8 PSI + int8 °C | 0–150 PSI / -40 a 85 °C | 1 Hz | Crítico → Pi 1 |
| `0x27` | Presión + temp. neumático — Trasero Der. (RR) | 2 bytes | uint8 PSI + int8 °C | 0–150 PSI / -40 a 85 °C | 1 Hz | Crítico → Pi 1 |

### 4.3 Reservado para expansión

Los rangos `0x28`–`0x2F` (Nodo 2) y `0x30`–`0x3F` (Nodo 1) quedan reservados para mensajes futuros sin necesidad de romper compatibilidad con el software ya escrito.

## 5. Cálculo de Checksum

El checksum es un **XOR simple** de todos los bytes desde `NODE_ID` hasta el final del `PAYLOAD` (no incluye `START` ni el propio `CHECKSUM`).
> **Nota de diseño:** se eligió XOR simple para la Rev. 0.1 por su bajo costo computacional en el STM32. Si en pruebas de banco se detecta una tasa de error no despreciable, una revisión futura de este documento migrará a CRC-8 (polinomio `0x07`), que detecta más patrones de error a cambio de un costo de cómputo ligeramente mayor.

## 6. Ejemplo de Trama Anotado

Trama de RPM = 3200 rpm, enviada por el Nodo 1:
| Byte | Valor | Significado |
|---|---|---|
| `AA` | START | Sincronización |
| `01` | NODE_ID | STM32 Nodo 1 |
| `10` | MSG_ID | RPM |
| `02` | LEN | 2 bytes de payload |
| `0C 80` | PAYLOAD | 0x0C80 = 3200 (uint16 big-endian) |
| `9E` | CHECKSUM | `01 ^ 10 ^ 02 ^ 0C ^ 80` = `9E` |
| `55` | END | Cierre de trama |

## 7. Manejo de Errores y Timeouts

- **Checksum inválido:** la trama se descarta silenciosamente en el receptor (Raspberry Pi). No se solicita retransmisión — el siguiente ciclo de envío (20 Hz para datos críticos) llega en 50 ms, suficientemente rápido para no afectar la UX del clúster.
- **Timeout de nodo:** si la Raspberry Pi #1 no recibe ningún mensaje válido del Nodo 1 durante más de **500 ms**, el clúster de instrumentos debe mostrar un estado de "dato no disponible" (ej. agujas en cero con indicador visual de falla), nunca congelar el último valor conocido sin aviso. El mismo criterio aplica a los mensajes críticos del Nodo 2 (`0x20`-`0x21`, `0x24`-`0x27`).
- **Pérdida de sincronización:** si el receptor detecta un `START` (`0xAA`) en una posición inesperada dentro del buffer, descarta bytes hasta encontrar el siguiente `START` válido seguido de una trama con checksum correcto.

## 8. Heartbeat entre Raspberry Pi #1 y #2

Enlace UART aislado de baja velocidad (9600 baudios, suficiente para este propósito), independiente del enlace STM32 → Pi.

| Campo | Valor |
|---|---|
| Frecuencia de envío | 1 Hz |
| `NODE_ID` | `0x10` (si lo envía Pi #1) o `0x20` (si lo envía Pi #2) |
| `MSG_ID` | `0x00` |
| `PAYLOAD` | 1 byte — contador incremental (0–255, con overflow) |

Si la Raspberry Pi #1 no recibe un heartbeat de la Pi #2 durante más de **3 segundos**, puede asumir que el dominio de infoentretenimiento no está respondiendo y, opcionalmente, mostrar un indicador de estado — pero **nunca debe bloquear ni degradar el renderizado del clúster de instrumentos** por esta condición.

## 9. Próximos Pasos

- [ ] Validar este protocolo con tráfico sintético generado por el STM32 Nodo 3 (herramienta de banco) antes de conectar el arnés real.
- [ ] Evaluar migración de checksum XOR → CRC-8 según resultados de la prueba de banco.
- [ ] Implementar el parser de este protocolo en el software de la Raspberry Pi #1 (`software/pi-cluster/`) y #2 (`software/pi-infotainment/`).
- [ ] Escribir el firmware del Nodo 2 siguiendo el catálogo de la Sección 4.2 (en curso).