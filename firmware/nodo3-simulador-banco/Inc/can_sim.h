/**
 * @file    can_sim.h
 * @brief   Interfaz publica del simulador de trafico CAN - Nodo 3
 * @note    Requiere que main.c ya haya inicializado el periferico FDCAN1
 *          via CubeMX (MX_FDCAN1_Init) antes de llamar CanSim_Init().
 */

#ifndef CAN_SIM_H
#define CAN_SIM_H

#include "fdcan.h"   /* generado por CubeMX: expone hfdcan1 */
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Inicializa el estado interno del simulador y arranca el filtro
 *        de FDCAN en modo normal (loopback externo hacia el Nodo 1).
 * @retval true si la inicializacion y el arranque de FDCAN fueron exitosos
 */
bool CanSim_Init(void);

/**
 * @brief Debe llamarse periodicamente desde el loop principal (o desde un
 *        timer de 1ms). Internamente decide, segun los periodos definidos
 *        en can_sim_config.h, cuando toca enviar cada tipo de trama.
 * @param tick_ms  contador de milisegundos transcurridos desde el arranque
 *                 (ej. HAL_GetTick())
 */
void CanSim_Task(uint32_t tick_ms);

/**
 * @brief Fuerza el envio inmediato de un DTC simulado, util para pruebas
 *        manuales del manejo de fallas del Nodo 1 sin esperar el perfil
 *        CANSIM_PROFILE_FAULT_INJECT.
 * @param dtc_code  codigo de falla de 16 bits a inyectar
 */
void CanSim_InjectFault(uint16_t dtc_code);

#endif /* CAN_SIM_H */
