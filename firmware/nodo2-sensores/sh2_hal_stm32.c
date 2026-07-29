/**
 * @file    sh2_hal_stm32.c
 * @brief   Implementacion del HAL SH-2 sobre I2C de STM32 (ver .h para
 *          el requisito de descargar la libreria SH-2 oficial primero).
 *
 * Referencia de la interfaz esperada por CEVA (sh2_hal.h de la libreria
 * oficial, resumen segun documentacion publica del proyecto):
 *
 *   typedef struct sh2_Hal_s {
 *       int (*open)(sh2_Hal_t *self);
 *       void (*close)(sh2_Hal_t *self);
 *       int (*read)(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t_us);
 *       int (*write)(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len);
 *       uint32_t (*getTimeUs)(sh2_Hal_t *self);
 *   } sh2_Hal_t;
 *
 * IMPORTANTE: verificar esta firma contra sh2_hal.h realmente descargado
 * antes de compilar - puede diferir ligeramente entre versiones del SDK.
 */

#include "sh2_hal_stm32.h"
#include <string.h>

/* ---------------------------------------------------------------------- *
 *  Timer de microsegundos para getTimeUs()
 *
 *  El driver SH-2 usa timestamps para reconstruir la tasa de muestreo
 *  real del sensor - HAL_GetTick() (resolucion 1ms) es insuficiente.
 *  Se asume un TIM de 32 bits (ej. TIM2 en la mayoria de STM32G4)
 *  configurado por CubeMX en modo "Internal Clock", prescaler tal que
 *  el contador incremente 1 por microsegundo, sin necesidad de
 *  interrupcion (se lee el registro CNT directamente).
 *
 *  AJUSTAR htim2 y el prescaler segun la configuracion real de CubeMX.
 * ---------------------------------------------------------------------- */
extern TIM_HandleTypeDef htim2; /* generado por CubeMX, PSC configurado a 1us/tick */

static bool s_data_ready = false;

/* ---------------------------------------------------------------------- *
 *  Callbacks requeridos por sh2_Hal_t
 * ---------------------------------------------------------------------- */

static int SH2Hal_Open(sh2_Hal_t *self)
{
    (void)self;

    /* Secuencia de reset segun datasheet BNO085: mantener RESET bajo,
     * luego liberar y esperar a que el sensor este listo (indicado por
     * el primer pulso de INT, que se maneja via SH2Hal_OnInterrupt) */
    HAL_GPIO_WritePin(BNO085_RESET_GPIO_Port, BNO085_RESET_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(BNO085_RESET_GPIO_Port, BNO085_RESET_Pin, GPIO_PIN_SET);
    HAL_Delay(150); /* margen amplio de arranque, ver datasheet Seccion 5.2 */

    s_data_ready = false;
    return 0; /* 0 = exito, por convencion del driver SH-2 */
}

static void SH2Hal_Close(sh2_Hal_t *self)
{
    (void)self;
    HAL_GPIO_WritePin(BNO085_RESET_GPIO_Port, BNO085_RESET_Pin, GPIO_PIN_RESET);
}

static int SH2Hal_Read(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t_us)
{
    (void)self;

    if (!s_data_ready) {
        return 0; /* sin datos pendientes, 0 bytes leidos */
    }

    /* Los primeros 2 bytes de cualquier lectura SHTP son el largo total
     * del paquete (little-endian, bit 15 = flag de continuacion) - se
     * lee primero un header corto para saber cuanto pedir despues.
     * Implementacion simplificada: se asume que 'len' ya viene
     * dimensionado por el driver SH-2 para cubrir el paquete completo,
     * segun el contrato documentado de sh2_hal_read(). */
    if (HAL_I2C_Master_Receive(&hi2c1, BNO085_I2C_ADDR << 1, pBuffer, len, 50) != HAL_OK) {
        return 0;
    }

    if (t_us != NULL) {
        *t_us = __HAL_TIM_GET_COUNTER(&htim2);
    }

    s_data_ready = false;
    return (int)len;
}

static int SH2Hal_Write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len)
{
    (void)self;

    if (HAL_I2C_Master_Transmit(&hi2c1, BNO085_I2C_ADDR << 1, pBuffer, len, 50) != HAL_OK) {
        return 0;
    }
    return (int)len;
}

static uint32_t SH2Hal_GetTimeUs(sh2_Hal_t *self)
{
    (void)self;
    return __HAL_TIM_GET_COUNTER(&htim2);
}

/* ---------------------------------------------------------------------- *
 *  Instancia estatica y API publica
 * ---------------------------------------------------------------------- */

static sh2_Hal_t s_hal_instance = {
    .open = SH2Hal_Open,
    .close = SH2Hal_Close,
    .read = SH2Hal_Read,
    .write = SH2Hal_Write,
    .getTimeUs = SH2Hal_GetTimeUs,
};

sh2_Hal_t *SH2Hal_GetInstance(void)
{
    return &s_hal_instance;
}

void SH2Hal_OnInterrupt(void)
{
    /* Llamado desde HAL_GPIO_EXTI_Callback (main.c o stm32g4xx_it.c)
     * cuando BNO085_INT_Pin cambia a bajo - el sensor tiene un reporte
     * listo para leer. NO se lee aqui directamente (estamos en
     * contexto de interrupcion) - solo se marca la bandera; sh2_service()
     * debe llamarse periodicamente desde el loop principal, que a su
     * vez invoca SH2Hal_Read() cuando corresponda. */
    s_data_ready = true;
}
