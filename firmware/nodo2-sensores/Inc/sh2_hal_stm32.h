/**
 * @file    sh2_hal_stm32.h
 * @brief   Implementacion del HAL que requiere la libreria SH-2 (CEVA/
 *          Hillcrest) para el BNO085, sobre I2C sencillo sobre STM32 HAL.
 * @note    Programa: [Marca] - Defender Restomod
 *
 * ============================================================================
 *  REQUISITO PREVIO - LEER ANTES DE COMPILAR
 * ============================================================================
 *  Este archivo NO reimplementa el driver del BNO085 - implementa
 *  UNICAMENTE la capa de acceso a hardware (HAL) que la libreria oficial
 *  SH-2 de CEVA espera recibir. Es necesario descargar por separado el
 *  codigo fuente de esa libreria:
 *
 *      https://github.com/ceva-dsp/sh2
 *
 *  y agregar la carpeta /sh2 de ese repositorio al proyecto (Inc y Src).
 *  Este archivo asume que sh2.h, sh2_hal.h, sh2_SensorValue.h, shtp.h,
 *  etc. ya estan disponibles en el include path.
 *
 *  Las firmas de sh2_Hal_t documentadas aqui corresponden a la version
 *  publica del driver al momento de escribir esto - verificar contra el
 *  sh2_hal.h real descargado, ya que puede haber cambiado entre
 *  versiones del SDK de CEVA.
 * ============================================================================
 */

#ifndef SH2_HAL_STM32_H
#define SH2_HAL_STM32_H

#include "sh2_hal.h"   /* de la libreria SH-2 oficial, no de este proyecto */
#include "i2c.h"       /* generado por CubeMX: expone hi2c1 */
#include <stdint.h>
#include <stdbool.h>

/* ---------------------------------------------------------------------- *
 *  Pines de control del BNO085 (ajustar segun asignacion real en CubeMX)
 * ---------------------------------------------------------------------- */
#define BNO085_RESET_GPIO_Port   GPIOB
#define BNO085_RESET_Pin         GPIO_PIN_4
#define BNO085_INT_GPIO_Port     GPIOB
#define BNO085_INT_Pin           GPIO_PIN_5   /* INT activo en bajo (open-drain) */

#define BNO085_I2C_ADDR          0x4Au   /* ver sensor_config.h del Nodo 2 */

/**
 * @brief Devuelve un puntero a la estructura sh2_Hal_t ya inicializada
 *        con los callbacks de este archivo (open/close/read/write/
 *        getTimeUs), lista para pasarse a sh2_open().
 */
sh2_Hal_t *SH2Hal_GetInstance(void);

/**
 * @brief Debe llamarse desde el handler de interrupcion EXTI real
 *        (HAL_GPIO_EXTI_Callback) cuando se detecte el flanco de
 *        BNO085_INT_Pin. Marca internamente que hay datos listos para
 *        leer - el propio driver SH-2 decide cuando llamar a read().
 */
void SH2Hal_OnInterrupt(void);

#endif /* SH2_HAL_STM32_H */
