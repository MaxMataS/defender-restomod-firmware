/**
 * @file    can_sim.c
 * @brief   Implementacion del simulador de trafico CAN - Nodo 3 (banco)
 *
 * Genera tramas CAN periodicas que imitan (en timing y estructura general)
 * lo que produciria el bus GMLAN del LS3/6L80. Los IDs y el layout exacto
 * del payload son PLACEHOLDER - ver advertencia en can_sim_config.h.
 *
 * Este archivo asume una API de HAL FDCAN estandar (familia STM32G4,
 * paquete HAL tipico generado por CubeMX). Revisar y ajustar las llamadas
 * a HAL_FDCAN_* si la version de HAL del proyecto difiere.
 */

#include "can_sim.h"
#include "can_sim_config.h"
#include <string.h>

/* ---------------------------------------------------------------------- *
 *  Estado interno
 * ---------------------------------------------------------------------- */
typedef struct {
    uint32_t last_send_rpm_speed_ms;
    uint32_t last_send_engine_ms;
    uint32_t last_send_trans_ms;

    uint16_t current_rpm;
    int8_t   ramp_direction;   /* +1 = subiendo, -1 = bajando */

    bool     fault_pending;
    uint16_t fault_code;
} CanSim_State_t;

static CanSim_State_t s_state;

/* ---------------------------------------------------------------------- *
 *  Helpers internos de envio
 * ---------------------------------------------------------------------- */

/**
 * @brief Arma y encola una trama FDCAN estandar (11 bits) en el FIFO de TX.
 * @return true si HAL acepto la trama en el FIFO
 */
static bool CanSim_SendFrame(uint32_t id, const uint8_t *data, uint8_t len)
{
    FDCAN_TxHeaderTypeDef TxHeader = {0};

    TxHeader.Identifier          = id;
    TxHeader.IdType               = FDCAN_STANDARD_ID;
    TxHeader.TxFrameType          = FDCAN_DATA_FRAME;
    TxHeader.DataLength           = FDCAN_DLC_BYTES_8; /* ajustar segun 'len' real si se usa DLC variable */
    TxHeader.ErrorStateIndicator  = FDCAN_ESI_ACTIVE;
    TxHeader.BitRateSwitch        = FDCAN_BRS_OFF;     /* CAN clasico, no CAN-FD, para imitar GMLAN */
    TxHeader.FDFormat             = FDCAN_CLASSIC_CAN;
    TxHeader.TxEventFifoControl   = FDCAN_NO_TX_EVENTS;
    TxHeader.MessageMarker        = 0;

    uint8_t payload[8] = {0};
    if (len > 8) {
        len = 8;
    }
    memcpy(payload, data, len);

    if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &TxHeader, payload) != HAL_OK) {
        return false;
    }
    return true;
}

/* ---------------------------------------------------------------------- *
 *  Generadores de payload por tipo de mensaje
 * ---------------------------------------------------------------------- */

static void CanSim_UpdateRampProfile(void)
{
    if (CANSIM_ACTIVE_PROFILE == CANSIM_PROFILE_IDLE_ONLY) {
        s_state.current_rpm = CANSIM_RPM_MIN;
        return;
    }

    s_state.current_rpm += (s_state.ramp_direction * CANSIM_RPM_STEP);

    if (s_state.current_rpm >= CANSIM_RPM_MAX) {
        s_state.current_rpm = CANSIM_RPM_MAX;
        s_state.ramp_direction = -1;
    } else if (s_state.current_rpm <= CANSIM_RPM_MIN) {
        s_state.current_rpm = CANSIM_RPM_MIN;
        s_state.ramp_direction = 1;
    }
}

static void CanSim_SendRpmSpeed(void)
{
    CanSim_UpdateRampProfile();

    uint32_t rpm_range = (CANSIM_RPM_MAX - CANSIM_RPM_MIN);
    uint32_t rpm_offset = (s_state.current_rpm - CANSIM_RPM_MIN);
    uint16_t speed_x10 = (uint16_t)(
        (uint32_t)CANSIM_SPEED_MAX_KMH_X10 * rpm_offset / rpm_range
    );

    uint8_t data[4];
    data[0] = (uint8_t)(s_state.current_rpm >> 8);
    data[1] = (uint8_t)(s_state.current_rpm & 0xFF);
    data[2] = (uint8_t)(speed_x10 >> 8);
    data[3] = (uint8_t)(speed_x10 & 0xFF);

    CanSim_SendFrame(CANSIM_ID_RPM_SPEED, data, sizeof(data));
}

static void CanSim_SendEngineStatus(void)
{
    uint8_t data[2];
    data[0] = (uint8_t)(int8_t)CANSIM_COOLANT_TEMP_C;
    data[1] = (uint8_t)CANSIM_OIL_PRESSURE_PSI;

    CanSim_SendFrame(CANSIM_ID_ENGINE_TEMP, data, sizeof(data));
}

static void CanSim_SendTransStatus(void)
{
    uint8_t gear = 3; /* D1, valor fijo simulado por ahora */
    uint8_t data[1] = { gear };

    CanSim_SendFrame(CANSIM_ID_TRANS_STATUS, data, sizeof(data));

    if (CANSIM_ACTIVE_PROFILE == CANSIM_PROFILE_FAULT_INJECT &&
        s_state.current_rpm > ((CANSIM_RPM_MIN + CANSIM_RPM_MAX) / 2) &&
        !s_state.fault_pending) {
        CanSim_InjectFault(0x0217u); /* PLACEHOLDER: codigo DTC de ejemplo */
    }
}

static void CanSim_SendPendingFault(void)
{
    if (!s_state.fault_pending) {
        return;
    }

    uint8_t data[2];
    data[0] = (uint8_t)(s_state.fault_code >> 8);
    data[1] = (uint8_t)(s_state.fault_code & 0xFF);

    if (CanSim_SendFrame(CANSIM_ID_DTC, data, sizeof(data))) {
        s_state.fault_pending = false;
    }
}

/* ---------------------------------------------------------------------- *
 *  API publica
 * ---------------------------------------------------------------------- */

bool CanSim_Init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    s_state.current_rpm    = CANSIM_RPM_MIN;
    s_state.ramp_direction = 1;

    if (HAL_FDCAN_ConfigGlobalFilter(&hfdcan1,
                                      FDCAN_ACCEPT_IN_RX_FIFO0,
                                      FDCAN_ACCEPT_IN_RX_FIFO0,
                                      FDCAN_REJECT_REMOTE,
                                      FDCAN_REJECT_REMOTE) != HAL_OK) {
        return false;
    }

    if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK) {
        return false;
    }

    return true;
}

void CanSim_Task(uint32_t tick_ms)
{
    if ((tick_ms - s_state.last_send_rpm_speed_ms) >= CANSIM_PERIOD_RPM_SPEED_MS) {
        s_state.last_send_rpm_speed_ms = tick_ms;
        CanSim_SendRpmSpeed();
    }

    if ((tick_ms - s_state.last_send_engine_ms) >= CANSIM_PERIOD_ENGINE_MS) {
        s_state.last_send_engine_ms = tick_ms;
        CanSim_SendEngineStatus();
    }

    if ((tick_ms - s_state.last_send_trans_ms) >= CANSIM_PERIOD_TRANS_MS) {
        s_state.last_send_trans_ms = tick_ms;
        CanSim_SendTransStatus();
    }

    CanSim_SendPendingFault();
}

void CanSim_InjectFault(uint16_t dtc_code)
{
    s_state.fault_code    = dtc_code;
    s_state.fault_pending = true;
}