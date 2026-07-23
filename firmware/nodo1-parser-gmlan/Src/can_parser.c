/**
 * @file    can_parser.c
 * @brief   Implementacion del parser CAN del Nodo 1 (GMLAN motor + transmision)
 *
 * Decodifica las 4 tramas CAN definidas en can_parser_config.h y las
 * reempaqueta segun DOC-PH1-PROTO-001 antes de reenviarlas por UART a la
 * Raspberry Pi #1. Una trama CAN puede generar MAS DE UNA trama UART si
 * el protocolo interno separa campos que el bus CAN combina en un solo
 * mensaje (ej. RPM+velocidad viajan juntos en CAN pero son MSG_ID 0x10 y
 * 0x11 por separado en DOC-PH1-PROTO-001 Seccion 4.1).
 *
 * Este archivo asume una API de HAL FDCAN estandar (familia STM32G4,
 * paquete HAL tipico generado por CubeMX), igual que can_sim.c del Nodo 3.
 */

#include "can_parser.h"
#include "can_parser_config.h"
#include "uart_proto.h"
#include <string.h>

/* ---------------------------------------------------------------------- *
 *  MSG_ID del protocolo interno — DOC-PH1-PROTO-001 Seccion 4.1
 *  (dominio critico: mensajes del Nodo 1)
 * ---------------------------------------------------------------------- */
#define PROTO_MSG_ID_RPM              0x10u
#define PROTO_MSG_ID_SPEED            0x11u
#define PROTO_MSG_ID_COOLANT_TEMP     0x12u
#define PROTO_MSG_ID_OIL_PRESSURE     0x13u
#define PROTO_MSG_ID_GEAR             0x14u
#define PROTO_MSG_ID_DTC              0x15u

/* ---------------------------------------------------------------------- *
 *  Estado interno / contadores de diagnostico
 * ---------------------------------------------------------------------- */
static uint32_t s_rx_frame_count    = 0;
static uint32_t s_unknown_id_count  = 0;

/* ---------------------------------------------------------------------- *
 *  Handlers de decodificacion por ID de CAN
 * ---------------------------------------------------------------------- */

/**
 * @brief CANPARSE_ID_RPM_SPEED: payload de 4 bytes = RPM(u16 BE) + Speed(u16 BE)
 *        (ver can_sim.c: CanSim_SendRpmSpeed). Genera DOS tramas UART.
 */
static void CanParser_HandleRpmSpeed(const uint8_t *data, uint8_t len)
{
    if (len < 4) {
        return; /* trama corta/corrupta, se descarta silenciosamente */
    }

    /* Los primeros 2 bytes ya vienen en big-endian, igual que el formato
     * que espera DOC-PH1-PROTO-001 para MSG_ID 0x10 (uint16 big-endian) */
    Proto_SendFrame(&CANPARSE_UART_HANDLE, PROTO_NODE_ID_NODO1,
                     PROTO_MSG_ID_RPM, &data[0], 2,
                     CANPARSE_UART_TX_TIMEOUT_MS);

    Proto_SendFrame(&CANPARSE_UART_HANDLE, PROTO_NODE_ID_NODO1,
                     PROTO_MSG_ID_SPEED, &data[2], 2,
                     CANPARSE_UART_TX_TIMEOUT_MS);
}

/**
 * @brief CANPARSE_ID_ENGINE_TEMP: payload de 2 bytes = temp(int8) + presion(uint8)
 *        (ver can_sim.c: CanSim_SendEngineStatus). Genera DOS tramas UART.
 */
static void CanParser_HandleEngineStatus(const uint8_t *data, uint8_t len)
{
    if (len < 2) {
        return;
    }

    Proto_SendFrame(&CANPARSE_UART_HANDLE, PROTO_NODE_ID_NODO1,
                     PROTO_MSG_ID_COOLANT_TEMP, &data[0], 1,
                     CANPARSE_UART_TX_TIMEOUT_MS);

    Proto_SendFrame(&CANPARSE_UART_HANDLE, PROTO_NODE_ID_NODO1,
                     PROTO_MSG_ID_OIL_PRESSURE, &data[1], 1,
                     CANPARSE_UART_TX_TIMEOUT_MS);
}

/**
 * @brief CANPARSE_ID_TRANS_STATUS: payload de 1 byte = marcha activa
 *        (ver can_sim.c: CanSim_SendTransStatus). Mapeo directo 1:1.
 */
static void CanParser_HandleTransStatus(const uint8_t *data, uint8_t len)
{
    if (len < 1) {
        return;
    }

    Proto_SendFrame(&CANPARSE_UART_HANDLE, PROTO_NODE_ID_NODO1,
                     PROTO_MSG_ID_GEAR, &data[0], 1,
                     CANPARSE_UART_TX_TIMEOUT_MS);
}

/**
 * @brief CANPARSE_ID_DTC: payload de 2 bytes = codigo DTC (uint16 BE)
 *        (ver can_sim.c: CanSim_SendPendingFault). Mapeo directo 1:1.
 *        A diferencia de los demas, este mensaje NO es periodico — solo
 *        llega cuando el Nodo 3 (o el bus real) inyecta una falla.
 */
static void CanParser_HandleDtc(const uint8_t *data, uint8_t len)
{
    if (len < 2) {
        return;
    }

    Proto_SendFrame(&CANPARSE_UART_HANDLE, PROTO_NODE_ID_NODO1,
                     PROTO_MSG_ID_DTC, &data[0], 2,
                     CANPARSE_UART_TX_TIMEOUT_MS);
}

/* ---------------------------------------------------------------------- *
 *  Despachador central: ID de CAN -> handler
 * ---------------------------------------------------------------------- */
static void CanParser_Dispatch(uint32_t can_id, const uint8_t *data, uint8_t len)
{
    switch (can_id) {
        case CANPARSE_ID_RPM_SPEED:
            CanParser_HandleRpmSpeed(data, len);
            break;
        case CANPARSE_ID_ENGINE_TEMP:
            CanParser_HandleEngineStatus(data, len);
            break;
        case CANPARSE_ID_TRANS_STATUS:
            CanParser_HandleTransStatus(data, len);
            break;
        case CANPARSE_ID_DTC:
            CanParser_HandleDtc(data, len);
            break;
        default:
            s_unknown_id_count++;
            break;
    }
}

/* ---------------------------------------------------------------------- *
 *  Callback de HAL — se dispara por interrupcion cuando llega una trama
 *  nueva a FIFO0. Reemplaza (override) la version debil que provee HAL.
 * ---------------------------------------------------------------------- */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    if (hfdcan->Instance != hfdcan1.Instance) {
        return; /* este proyecto solo usa FDCAN1, pero se valida por seguridad */
    }

    if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) == 0) {
        return;
    }

    FDCAN_RxHeaderTypeDef RxHeader;
    uint8_t RxData[8];

    if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &RxHeader, RxData) != HAL_OK) {
        return;
    }

    s_rx_frame_count++;

    /* DataLength de HAL viene codificado (FDCAN_DLC_BYTES_x) — para CAN
     * clasico (no FD) coincide 1:1 con bytes reales hasta 8, por eso
     * basta este shift; si se migra a CAN-FD con DLC>8 hay que mapear
     * la tabla completa de FDCAN_DLC_BYTES_*. */
    uint8_t len = (uint8_t)(RxHeader.DataLength >> 16);
    if (len > 8) {
        len = 8;
    }

    CanParser_Dispatch(RxHeader.Identifier, RxData, len);
}

/* ---------------------------------------------------------------------- *
 *  API publica
 * ---------------------------------------------------------------------- */

bool CanParser_Init(void)
{
    s_rx_frame_count   = 0;
    s_unknown_id_count = 0;

    /* Filtro de rango: acepta unicamente CANPARSE_ID_RPM_SPEED..DTC
     * (0x1A0-0x1A3, ver can_parser_config.h), todo lo demas se rechaza
     * en hardware para no gastar ciclos de CPU descartando tramas ajenas. */
    FDCAN_FilterTypeDef sFilterConfig = {0};
    sFilterConfig.IdType       = FDCAN_STANDARD_ID;
    sFilterConfig.FilterIndex  = 0;
    sFilterConfig.FilterType   = FDCAN_FILTER_RANGE;
    sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    sFilterConfig.FilterID1    = CANPARSE_ID_RPM_SPEED;  /* 0x1A0, extremo bajo */
    sFilterConfig.FilterID2    = CANPARSE_ID_DTC;         /* 0x1A3, extremo alto */

    if (HAL_FDCAN_ConfigFilter(&hfdcan1, &sFilterConfig) != HAL_OK) {
        return false;
    }

    /* Todo lo que no matchee el filtro anterior se rechaza (no se acepta
     * por default ni se reenvia a FIFO1) */
    if (HAL_FDCAN_ConfigGlobalFilter(&hfdcan1,
                                      FDCAN_REJECT,
                                      FDCAN_REJECT,
                                      FDCAN_REJECT_REMOTE,
                                      FDCAN_REJECT_REMOTE) != HAL_OK) {
        return false;
    }

    if (HAL_FDCAN_ActivateNotification(&hfdcan1,
                                        FDCAN_IT_RX_FIFO0_NEW_MESSAGE,
                                        0) != HAL_OK) {
        return false;
    }

    if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK) {
        return false;
    }

    return true;
}

uint32_t CanParser_GetRxFrameCount(void)
{
    return s_rx_frame_count;
}

uint32_t CanParser_GetUnknownIdCount(void)
{
    return s_unknown_id_count;
}
