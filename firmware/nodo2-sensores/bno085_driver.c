/**
 * @file    bno085_driver.c
 * @brief   Implementacion del driver de aplicacion BNO085 sobre SH-2.
 *
 * ADVERTENCIA: los nombres exactos de tipos/funciones de sh2.h y
 * sh2_SensorValue.h (SH2_ROTATION_VECTOR, sh2_SensorValue_t,
 * sh2_SensorEvent_t, etc.) corresponden a la API publica documentada
 * del SDK de CEVA al momento de escribir esto. Verificar contra los
 * headers realmente descargados antes de compilar - el SDK ha tenido
 * cambios menores de nombres entre versiones.
 */

#include "bno085_driver.h"
#include "sh2_hal_stm32.h"
#include "sh2.h"
#include "sh2_SensorValue.h"
#include <math.h>
#include <string.h>

/* Tasa de reporte solicitada AL SENSOR (submuestreada despues por
 * SensorReader_Task a los 10 Hz reales que necesita el protocolo -
 * pedir mas resolucion de la que se usa reduce el aliasing del filtro
 * de fusion interno del BNO085, es una practica recomendada del
 * fabricante, no un descuido). */
#define BNO085_REPORT_INTERVAL_US   10000u  /* 100 Hz */

typedef struct {
    float pitch_deg;
    float roll_deg;
    bool  valid;
} EulerCache_t;

static EulerCache_t s_euler_cache = {0};
static bool s_initialized = false;

/**
 * @brief Convierte un cuaternion (w,x,y,z) a pitch/roll en grados.
 *        Formulas estandar de aeroespacio (rotacion ZYX), yaw se
 *        descarta porque el protocolo interno (DOC-PH1-PROTO-001
 *        Seccion 4.2) solo pide pitch y roll.
 */
static void QuaternionToEuler(float w, float x, float y, float z,
                               float *out_pitch_deg, float *out_roll_deg)
{
    /* roll (rotacion en X) */
    float sinr_cosp = 2.0f * (w * x + y * z);
    float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
    *out_roll_deg = atan2f(sinr_cosp, cosr_cosp) * (180.0f / (float)M_PI);

    /* pitch (rotacion en Y), con clamp para evitar NaN de asinf() en
     * los polos por errores de redondeo de punto flotante */
    float sinp = 2.0f * (w * y - z * x);
    if (sinp > 1.0f)  sinp = 1.0f;
    if (sinp < -1.0f) sinp = -1.0f;
    *out_pitch_deg = asinf(sinp) * (180.0f / (float)M_PI);
}

/**
 * @brief Callback de eventos de sensor, requerido por sh2_setSensorCallback().
 *        Se invoca desde dentro de sh2_service() cuando llega un reporte
 *        nuevo del sensor - en este caso, solo nos interesa
 *        SH2_ROTATION_VECTOR.
 */
static void SensorEventCallback(void *cookie, sh2_SensorEvent_t *event)
{
    (void)cookie;

    sh2_SensorValue_t value;
    if (sh2_decodeSensorEvent(&value, event) != SH2_OK) {
        return;
    }

    if (value.sensorId != SH2_ROTATION_VECTOR) {
        return;
    }

    float pitch_deg, roll_deg;
    QuaternionToEuler(
        value.un.rotationVector.real,
        value.un.rotationVector.i,
        value.un.rotationVector.j,
        value.un.rotationVector.k,
        &pitch_deg, &roll_deg
    );

    s_euler_cache.pitch_deg = pitch_deg;
    s_euler_cache.roll_deg = roll_deg;
    s_euler_cache.valid = true;
}

bool BNO085_Init(void)
{
    memset(&s_euler_cache, 0, sizeof(s_euler_cache));

    sh2_Hal_t *hal = SH2Hal_GetInstance();

    if (sh2_open(hal, NULL, NULL) != SH2_OK) {
        return false;
    }

    if (sh2_setSensorCallback(SensorEventCallback, NULL) != SH2_OK) {
        sh2_close();
        return false;
    }

    sh2_SensorConfig_t config = {0};
    config.reportInterval_us = BNO085_REPORT_INTERVAL_US;

    if (sh2_setSensorConfig(SH2_ROTATION_VECTOR, &config) != SH2_OK) {
        sh2_close();
        return false;
    }

    s_initialized = true;
    return true;
}

void BNO085_Service(void)
{
    if (!s_initialized) {
        return;
    }
    sh2_service();
}

bool BNO085_ReadEulerAngles(int16_t *out_pitch_x10, int16_t *out_roll_x10)
{
    if (!s_initialized || !s_euler_cache.valid) {
        return false;
    }

    *out_pitch_x10 = (int16_t)(s_euler_cache.pitch_deg * 10.0f);
    *out_roll_x10  = (int16_t)(s_euler_cache.roll_deg * 10.0f);
    return true;
}
