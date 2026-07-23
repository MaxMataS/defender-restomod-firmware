/**
 * @file    uart_proto.h
 * @brief   Codificador de tramas UART segun DOC-PH1-PROTO-001, rev. 0.1
 * @note    Programa: [Marca] - Defender Restomod
 *          Documento relacionado: DOC-PH1-PROTO-001 (protocolo interno STM32<->Pi)
 *          Modulo compartido: pensado para usarse tanto en Nodo 1 (GMLAN)
 *          como en Nodo 2 (sensores propios) sin duplicar logica de trama.
 *
 * Implementa unicamente la CAPA DE TRANSPORTE (armado de trama, checksum,
 * envio por UART). NO conoce el significado de los MSG_ID — eso lo decide
 * quien llama (can_parser.c, sensor_reader.c, etc.), pasando el NODE_ID,
 * MSG_ID y payload ya calculados.
 */

#ifndef UART_PROTO_H
#define UART_PROTO_H

#include "usart.h"   /* generado por CubeMX: expone el huart_* a usar */
#include <stdint.h>
#include <stdbool.h>

/* ---------------------------------------------------------------------- *
 *  Constantes de trama (DOC-PH1-PROTO-001, Seccion 3)
 * ---------------------------------------------------------------------- */
#define PROTO_START_BYTE        0xAAu
#define PROTO_END_BYTE           0x55u
#define PROTO_MAX_PAYLOAD_LEN     32u   /* Seccion 3: LEN va 0-32 */
#define PROTO_FRAME_OVERHEAD_LEN   6u   /* START+NODE_ID+MSG_ID+LEN+CHECKSUM+END */
#define PROTO_MAX_FRAME_LEN  (PROTO_FRAME_OVERHEAD_LEN + PROTO_MAX_PAYLOAD_LEN)

/* NODE_ID (Seccion 3.1) — el propio nodo se identifica con una sola de estas */
#define PROTO_NODE_ID_NODO1      0x01u  /* Parser GMLAN motor + transmision */
#define PROTO_NODE_ID_NODO2      0x02u  /* Fusion de sensores propios */
#define PROTO_NODE_ID_NODO3      0x03u  /* Simulador de banco — solo Fase 1 */
#define PROTO_NODE_ID_PI1        0x10u  /* Raspberry Pi #1 (Cluster) */
#define PROTO_NODE_ID_PI2        0x20u  /* Raspberry Pi #2 (Infoentretenimiento) */

/**
 * @brief Arma una trama completa segun DOC-PH1-PROTO-001 Seccion 3 y la
 *        transmite por el UART indicado (bloqueante, HAL_UART_Transmit).
 *
 * @param huart     handle HAL del UART a usar (ej. &huart2 hacia Pi #1)
 * @param node_id   NODE_ID del nodo que envia (usar PROTO_NODE_ID_*)
 * @param msg_id    MSG_ID del mensaje (ver catalogo en DOC-PH1-PROTO-001 Sec. 4)
 * @param payload   puntero al payload (puede ser NULL si len == 0)
 * @param len       longitud del payload en bytes (0-32, ver PROTO_MAX_PAYLOAD_LEN)
 * @param timeout_ms timeout para HAL_UART_Transmit
 *
 * @retval true si la trama se armo y transmitio correctamente
 * @retval false si len excede PROTO_MAX_PAYLOAD_LEN o si HAL_UART_Transmit fallo
 */
bool Proto_SendFrame(UART_HandleTypeDef *huart,
                      uint8_t node_id,
                      uint8_t msg_id,
                      const uint8_t *payload,
                      uint8_t len,
                      uint32_t timeout_ms);

/**
 * @brief Calcula el checksum XOR de una trama segun DOC-PH1-PROTO-001 Sec. 5
 *        (XOR de NODE_ID..fin de PAYLOAD, sin incluir START ni CHECKSUM).
 *        Expuesto para uso en pruebas unitarias / validacion en banco.
 */
uint8_t Proto_ComputeChecksum(uint8_t node_id,
                               uint8_t msg_id,
                               uint8_t len,
                               const uint8_t *payload);

#endif /* UART_PROTO_H */
