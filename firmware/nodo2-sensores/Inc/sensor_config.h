/**
 * @file    sensor_config.h
 * @brief   Configuracion del Nodo 2 (sensores propios de marca)
 * @note    Programa: [Marca] - Defender Restomod
 *          Documentos relacionados: DOC-PH1-ARQ-001 (Arquitectura),
 *          DOC-PH1-PROTO-001 rev. 0.2 (protocolo interno, Seccion 4.2)
 *
 * Sensores definidos en DOC-PH1-BOM-001 Seccion 6.1 (Sistema F):
 *   - F.1  IMU BNO085 (Adafruit #4754)          -> pitch/roll
 *   - F.2  Kit TPMS 4 ruedas + receptor RS232   -> presion/temp por rueda
 *   - F.3  SHT31-D (Adafruit #2857)             -> temperatura/humedad cabina
 *
 * NO incluye sensores de suspension neumatica (altura, presion de aire) —
 * descartados junto con la decision de usar coilover en vez de aire
 * (ver DOC-000-BRAND-001 Seccion 3.1).
 */

#ifndef SENSOR_CONFIG_H
#define SENSOR_CONFIG_H

#include <stdint.h>

/* ---------------------------------------------------------------------- *
 *  I2C — IMU (BNO085) y sensor de temperatura/humedad (SHT31-D)
 *  Ambos son STEMMA QT / Qwiic, comparten el mismo bus I2C sin conflicto
 *  de direccion.
 * ---------------------------------------------------------------------- */
#define SENSCFG_I2C_HANDLE          hi2c1

#define SENSCFG_BNO085_I2C_ADDR     0x4Au   /* direccion por defecto (ADR a GND) */
#define SENSCFG_SHT31_I2C_ADDR      0x44u   /* direccion por defecto (ADDR a GND) */

/* ---------------------------------------------------------------------- *
 *  UART — recepcion del kit TPMS (F.2)
 *  ADVERTENCIA: canbustpms.com no publica el formato exacto de trama de
 *  su modulo RS232/TTL en documentacion publica. sensor_reader.c
 *  implementa un buffer circular de recepcion listo para usarse, pero
 *  el parseo de la trama real (TPMS_ParseFrame) queda con un TODO
 *  explicito hasta obtener la hoja de datos del proveedor.
 * ---------------------------------------------------------------------- */
#define SENSCFG_TPMS_UART_HANDLE    huart3
#define SENSCFG_TPMS_RX_BUF_LEN     64u

/* ---------------------------------------------------------------------- *
 *  UART de salida hacia Raspberry Pi — mismo enlace que usa Nodo 1
 *  (ver can_parser_config.h). Nodo 2 usa un UART fisico distinto en el
 *  STM32, pero el mismo protocolo (uart_proto.h) y el mismo destino
 *  logico (Pi 1 para datos criticos, Pi 2 para no criticos).
 * ---------------------------------------------------------------------- */
#define SENSCFG_UART_PI1_HANDLE      huart2   /* datos criticos: pitch/roll, TPMS */
#define SENSCFG_UART_PI2_HANDLE      huart4   /* dato no critico: temp/humedad cabina */
#define SENSCFG_UART_TX_TIMEOUT_MS   20u

/* ---------------------------------------------------------------------- *
 *  Tasas de actualizacion (DOC-PH1-PROTO-001 Seccion 4.2)
 * ---------------------------------------------------------------------- */
#define SENSCFG_RATE_IMU_HZ          10u   /* pitch/roll, MSG_ID 0x20/0x21 */
#define SENSCFG_RATE_CABIN_HZ         1u   /* temp/humedad, MSG_ID 0x22/0x23 */
#define SENSCFG_RATE_TPMS_HZ          1u   /* presion/temp neumatico, 0x24-0x27 */

#endif /* SENSOR_CONFIG_H */
