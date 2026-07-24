/**
 * @file    sensor_reader.h
 * @brief   Interfaz publica del Nodo 2 (fusion de sensores propios)
 * @note    Requiere que main.c ya haya inicializado I2C1 (MX_I2C1_Init),
 *          USART hacia Pi 1 y Pi 2, y el USART del TPMS (MX_USARTx_Init)
 *          via CubeMX antes de llamar SensorReader_Init().
 *
 * A diferencia del Nodo 1 (dirigido por interrupcion de CAN), este nodo
 * es de tipo POLLING: no hay un evento externo que dispare la lectura,
 * los sensores hay que consultarlos activamente. Por eso SensorReader_Task()
 * debe llamarse repetidamente desde el loop principal (igual que
 * CanSim_Task() en el Nodo 3), y el propio modulo controla internamente
 * a que tasa lee cada sensor usando HAL_GetTick().
 */

#ifndef SENSOR_READER_H
#define SENSOR_READER_H

#include "i2c.h"     /* generado por CubeMX: expone hi2c1 */
#include "usart.h"   /* generado por CubeMX: expone huart2/huart3/huart4 */
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Inicializa los sensores I2C (BNO085, SHT31-D) y arranca la
 *        recepcion por interrupcion del UART del TPMS.
 * @retval true si toda la inicializacion fue exitosa
 * @retval false si algun sensor no respondio en el bus I2C (revisar
 *         cableado/direccion antes de continuar)
 */
bool SensorReader_Init(void);

/**
 * @brief Debe llamarse repetidamente desde el loop principal de main().
 *        Internamente decide, segun HAL_GetTick(), si toca leer y
 *        transmitir cada sensor (IMU a 10 Hz, cabina y TPMS a 1 Hz —
 *        ver sensor_config.h). No bloquea si todavia no toca leer nada.
 */
void SensorReader_Task(void);

/**
 * @brief Diagnostico: true si el ultimo intento de lectura del BNO085
 *        fallo (sensor desconectado o direccion I2C incorrecta).
 */
bool SensorReader_IsImuFaulted(void);

/**
 * @brief Diagnostico: true si el ultimo intento de lectura del SHT31-D
 *        fallo.
 */
bool SensorReader_IsCabinSensorFaulted(void);

#endif /* SENSOR_READER_H */
