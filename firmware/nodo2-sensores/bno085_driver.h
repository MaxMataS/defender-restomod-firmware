/**
 * @file    bno085_driver.h
 * @brief   Capa de aplicacion sobre la libreria SH-2 oficial: inicializa
 *          el sensor, habilita el reporte de orientacion, y expone
 *          pitch/roll ya convertidos desde el cuaternion.
 * @note    Requiere sh2.h y sh2_SensorValue.h de la libreria oficial
 *          (https://github.com/ceva-dsp/sh2), mas sh2_hal_stm32.h/.c de
 *          este mismo proyecto.
 *
 * Reemplaza el stub BNO085_ReadEulerAngles() que ten\u00eda sensor_reader.c
 * con una integracion real - misma firma de funcion, para que el resto
 * del Nodo 2 no necesite cambios.
 */

#ifndef BNO085_DRIVER_H
#define BNO085_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Inicializa el BNO085: abre la sesion SH-2 sobre el HAL de
 *        sh2_hal_stm32.c, y habilita el reporte "Rotation Vector" a
 *        100 Hz (submuestreado por SensorReader_Task a los 10 Hz que
 *        pide sensor_config.h - SENSCFG_RATE_IMU_HZ).
 * @retval true si la sesion SH-2 se abrio y el reporte se configuro
 *         correctamente.
 */
bool BNO085_Init(void);

/**
 * @brief Debe llamarse periodicamente desde el loop principal (o desde
 *        SensorReader_Task) - internamente llama a sh2_service(), que
 *        procesa los datos pendientes y dispara el callback de eventos
 *        registrado en BNO085_Init().
 */
void BNO085_Service(void);

/**
 * @brief Devuelve el ultimo pitch/roll calculado a partir del
 *        cuaternion mas reciente recibido del sensor. Misma firma que
 *        el stub anterior en sensor_reader.c, para integracion directa.
 * @retval true si hay al menos una lectura valida disponible
 * @retval false si el sensor nunca ha entregado un reporte (recien
 *         inicializado, o desconectado)
 */
bool BNO085_ReadEulerAngles(int16_t *out_pitch_x10, int16_t *out_roll_x10);

#endif /* BNO085_DRIVER_H */
