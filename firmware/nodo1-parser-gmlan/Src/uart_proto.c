/**
 * @file    uart_proto.c
 * @brief   Implementacion del codificador de tramas UART (DOC-PH1-PROTO-001)
 */

#include "uart_proto.h"
#include <string.h>

uint8_t Proto_ComputeChecksum(uint8_t node_id,
                               uint8_t msg_id,
                               uint8_t len,
                               const uint8_t *payload)
{
    uint8_t checksum = node_id;
    checksum ^= msg_id;
    checksum ^= len;

    for (uint8_t i = 0; i < len; i++) {
        checksum ^= payload[i];
    }

    return checksum;
}

bool Proto_SendFrame(UART_HandleTypeDef *huart,
                      uint8_t node_id,
                      uint8_t msg_id,
                      const uint8_t *payload,
                      uint8_t len,
                      uint32_t timeout_ms)
{
    if (len > PROTO_MAX_PAYLOAD_LEN) {
        return false;
    }
    if (len > 0 && payload == NULL) {
        return false;
    }

    uint8_t frame[PROTO_MAX_FRAME_LEN];
    uint8_t idx = 0;

    frame[idx++] = PROTO_START_BYTE;
    frame[idx++] = node_id;
    frame[idx++] = msg_id;
    frame[idx++] = len;

    if (len > 0) {
        memcpy(&frame[idx], payload, len);
        idx += len;
    }

    frame[idx++] = Proto_ComputeChecksum(node_id, msg_id, len, payload);
    frame[idx++] = PROTO_END_BYTE;

    if (HAL_UART_Transmit(huart, frame, idx, timeout_ms) != HAL_OK) {
        return false;
    }

    return true;
}
