/**
 * @file    can_parser_config.h
 * @brief   Configuracion del parser CAN del Nodo 1 (GMLAN motor + transmision)
 * @note    Programa: [Marca] - Defender Restomod
 *          Documentos relacionados: DOC-PH1-ARQ-001 (Arquitectura),
 *          DOC-PH1-PROTO-001 (protocolo interno STM32<->Pi)
 *
 * ============================================================================
 *  ADVERTENCIA IMPORTANTE - LEER ANTES DE USAR
 * ============================================================================
 *  Los IDs de CAN definidos abajo son PLACEHOLDERS (marcadores de posicion),
 *  identicos a los que usa el simulador del Nodo 3
 *  (firmware/nodo3-simulador-banco/Inc/can_sim_config.h) a proposito, para
 *  que este parser pueda validarse en banco contra el Nodo 3 sin cambios.
 *
 *  NO son los identificadores reales del bus GMLAN del Holley Terminator X
 *  Max / TCM interno del 6L80. Los IDs reales deben capturarse con el
 *  analizador PEAK-System PCAN-USB Pro FD (BOM item B.4) una vez que el
 *  motor este disponible para pruebas (ver DOC-PH1-ARQ-001 Seccion 3,
 *  punto abierto).
 *
 *  Al momento de migrar a IDs reales, actualizar UNICAMENTE este archivo.
 *  El resto del parser (can_parser.c) no depende de los valores exactos.
 * ============================================================================
 */

#ifndef CAN_PARSER_CONFIG_H
#define CAN_PARSER_CONFIG_H

#include <stdint.h>

/* ---------------------------------------------------------------------- *
 *  IDs de CAN esperados (PLACEHOLDER - deben igualar can_sim_config.h
 *  durante Fase 1; reemplazar con captura real antes de Fase 2)
 * ---------------------------------------------------------------------- */
#define CANPARSE_ID_RPM_SPEED        0x1A0u  /* RPM + velocidad combinados */
#define CANPARSE_ID_ENGINE_TEMP      0x1A1u  /* temp. refrigerante + presion aceite */
#define CANPARSE_ID_TRANS_STATUS     0x1A2u  /* marcha activa (6L80 / TCM) */
#define CANPARSE_ID_DTC              0x1A3u  /* codigo de falla activo */

/* ---------------------------------------------------------------------- *
 *  UART de salida hacia Raspberry Pi #1 (Cluster) — DOC-PH1-PROTO-001 Sec. 2
 *  Ajustar el handle segun la asignacion real de pines en CubeMX.
 * ---------------------------------------------------------------------- */
#define CANPARSE_UART_HANDLE          huart2
#define CANPARSE_UART_TX_TIMEOUT_MS   20u   /* margen amplio a 115200 baud */

#endif /* CAN_PARSER_CONFIG_H */
