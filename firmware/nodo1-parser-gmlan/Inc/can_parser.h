/**
 * @file    can_parser.h
 * @brief   Interfaz publica del parser CAN del Nodo 1 (GMLAN motor + transmision)
 * @note    Requiere que main.c ya haya inicializado FDCAN1 (MX_FDCAN1_Init)
 *          y el UART hacia Pi #1 (MX_USARTx_UART_Init) via CubeMX antes de
 *          llamar CanParser_Init().
 *
 * Flujo de datos (ver DOC-PH1-ARQ-001 Seccion 3):
 *   Bus CAN (Holley Terminator X Max) -> FDCAN1 RX -> CanParser (este modulo)
 *   -> Proto_SendFrame (uart_proto.c) -> UART -> Raspberry Pi #1 (Cluster)
 *
 * Este modulo es dirigido por interrupcion: cada trama CAN valida que llega
 * dispara CanParser_RxFifo0Callback (override de HAL_FDCAN_RxFifo0Callback),
 * que decodifica y reenvia de inmediato por UART. No requiere ser llamado
 * periodicamente desde el loop principal (a diferencia del simulador del
 * Nodo 3, que si es de tipo polling porque el genera el trafico).
 */

#ifndef CAN_PARSER_H
#define CAN_PARSER_H

#include "fdcan.h"   /* generado por CubeMX: expone hfdcan1 */
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Inicializa el filtro de FDCAN1 para aceptar unicamente los IDs
 *        definidos en can_parser_config.h, activa la notificacion por
 *        interrupcion de FIFO0, y arranca el periferico.
 * @retval true si toda la configuracion e inicio de FDCAN fueron exitosos
 */
bool CanParser_Init(void);

/**
 * @brief Contador de tramas CAN validas recibidas desde el arranque.
 *        Expuesto para diagnostico en banco (ej. imprimir por otro UART
 *        de debug, o verificar en un breakpoint que el flujo de datos
 *        efectivamente esta llegando).
 */
uint32_t CanParser_GetRxFrameCount(void);

/**
 * @brief Contador de tramas CAN recibidas con un ID no reconocido (fuera
 *        del catalogo de can_parser_config.h). Un numero creciente indica
 *        que el filtro de FDCAN esta dejando pasar mas de lo esperado, o
 *        que hay trafico inesperado en el bus.
 */
uint32_t CanParser_GetUnknownIdCount(void);

#endif /* CAN_PARSER_H */
