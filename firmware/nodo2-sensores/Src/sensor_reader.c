/**
 * @file    sensor_reader.c
 * @brief   Implementacion del Nodo 2 (fusion de sensores propios)
 *
 * Lee los 3 sensores definidos en DOC-PH1-BOM-001 Seccion 6.1 y los
 * reempaqueta segun DOC-PH1-PROTO-001 rev. 0.2 Seccion 4.2 antes de
 * reenviarlos por UART — datos criticos (IMU, TPMS) a Pi 1, dato no
 * critico (temperatura/humedad de cabina) a Pi 2.
 *
 * IMPORTANTE — dos sensores requieren trabajo adicional antes de
 * producción, marcado explicitamente con TODO en el codigo:
 *
 *   1. BNO085 (IMU): este sensor es un "sensor hub" con protocolo SHTP
 *      propio, no un simple sensor I2C de registros planos. Leer
 *      angulos de Euler correctamente requiere la libreria oficial
 *      SH-2 de CEVA/Hillcrest (o el port de Adafruit/SparkFun). Este
 *      archivo NO reimplementa ese protocolo — expone el punto de
 *      integracion (BNO085_ReadEulerAngles) con un TODO claro.
 *
 *   2. TPMS (canbustpms.com): el proveedor no publica el formato de
 *      trama de su modulo RS232/TTL. Este archivo SI implementa la
 *      recepcion por interrupcion con buffer circular (funcional y
 *      lista para usarse), pero el parseo de la trama real
 *      (TPMS_ParseFrame) tiene un TODO hasta conseguir la hoja de
 *      datos — ver DOC-PH1-BOM-001 Seccion 9, punto pendiente.
 *
 * El SHT31-D (temperatura/humedad de cabina) SI esta completo y
 * correcto — es un sensor I2C simple y bien documentado por Sensirion.
 */

#include "sensor_reader.h"
#include "sensor_config.h"
#include "uart_proto.h"
#include <string.h>

/* ---------------------------------------------------------------------- *
 *  MSG_ID del protocolo interno — DOC-PH1-PROTO-001 rev. 0.2 Seccion 4.2
 * ---------------------------------------------------------------------- */
#define PROTO_MSG_ID_PITCH           0x20u
#define PROTO_MSG_ID_ROLL            0x21u
#define PROTO_MSG_ID_CABIN_TEMP      0x22u
#define PROTO_MSG_ID_CABIN_HUMIDITY  0x23u
#define PROTO_MSG_ID_TPMS_FL         0x24u
#define PROTO_MSG_ID_TPMS_FR         0x25u
#define PROTO_MSG_ID_TPMS_RL         0x26u
#define PROTO_MSG_ID_TPMS_RR         0x27u

/* ---------------------------------------------------------------------- *
 *  Comandos SHT31-D (hoja de datos Sensirion, medicion de un solo disparo,
 *  alta repetibilidad, sin estiramiento de reloj)
 * ---------------------------------------------------------------------- */
#define SHT31_CMD_MEASURE_HIGHREP_MSB   0x2Cu
#define SHT31_CMD_MEASURE_HIGHREP_LSB   0x06u

/* ---------------------------------------------------------------------- *
 *  Estado interno
 * ---------------------------------------------------------------------- */
static bool s_imu_faulted   = false;
static bool s_cabin_faulted = false;

static uint32_t s_last_imu_tick   = 0;
static uint32_t s_last_cabin_tick = 0;
static uint32_t s_last_tpms_tick  = 0;

static uint8_t  s_tpms_rx_buf[SENSCFG_TPMS_RX_BUF_LEN];
static uint8_t  s_tpms_rx_byte; /* recibe 1 byte a la vez por interrupcion */

typedef struct {
    uint8_t  pressure_psi;
    int8_t   temp_c;
    bool     valid;
} TpmsWheelData_t;

static TpmsWheelData_t s_tpms_wheel[4]; /* 0=FL, 1=FR, 2=RL, 3=RR */

/* ========================================================================
 *  SHT31-D — temperatura/humedad de cabina (implementacion completa)
 * ======================================================================== */

/**
 * @brief Dispara una medicion de un solo disparo y lee el resultado.
 *        NOTA: por simplicidad no se valida el CRC de 8 bits que el
 *        sensor adjunta a cada palabra (bytes 3 y 6 de la respuesta) —
 *        aceptable para Fase 1 de banco; agregar validacion de CRC
 *        antes de produccion si se detectan lecturas erraticas.
 */
static bool SHT31_ReadMeasurement(int8_t *out_temp_c, uint8_t *out_humidity_pct)
{
    uint8_t cmd[2] = { SHT31_CMD_MEASURE_HIGHREP_MSB, SHT31_CMD_MEASURE_HIGHREP_LSB };
    uint8_t raw[6];

    if (HAL_I2C_Master_Transmit(&SENSCFG_I2C_HANDLE, SENSCFG_SHT31_I2C_ADDR << 1,
                                 cmd, sizeof(cmd), 50) != HAL_OK) {
        return false;
    }

    /* Alta repetibilidad tarda hasta 15 ms en el sensor (hoja de datos) */
    HAL_Delay(16);

    if (HAL_I2C_Master_Receive(&SENSCFG_I2C_HANDLE, SENSCFG_SHT31_I2C_ADDR << 1,
                                raw, sizeof(raw), 50) != HAL_OK) {
        return false;
    }

    uint16_t raw_temp = ((uint16_t)raw[0] << 8) | raw[1];
    uint16_t raw_hum  = ((uint16_t)raw[3] << 8) | raw[4];

    /* Formulas de conversion, hoja de datos Sensirion SHT3x Seccion 4.13 */
    float temp_c    = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);
    float humidity   = 100.0f * ((float)raw_hum / 65535.0f);

    if (temp_c < -40.0f) temp_c = -40.0f;
    if (temp_c > 85.0f)  temp_c = 85.0f;
    if (humidity < 0.0f)   humidity = 0.0f;
    if (humidity > 100.0f) humidity = 100.0f;

    *out_temp_c       = (int8_t)temp_c;
    *out_humidity_pct = (uint8_t)humidity;

    return true;
}

/* ========================================================================
 *  BNO085 — IMU / pitch+roll (punto de integracion, NO implementacion
 *  completa del protocolo SHTP)
 * ======================================================================== */

/**
 * @brief TODO: reemplazar este cuerpo con una llamada real a la libreria
 *        SH-2 (CEVA/Hillcrest) inicializada sobre I2C, solicitando el
 *        reporte "Rotation Vector" o "Game Rotation Vector" y
 *        convirtiendo el cuaternion resultante a angulos de Euler
 *        pitch/roll. Mientras tanto, esta funcion solo verifica que el
 *        sensor responda en el bus (HAL_I2C_IsDeviceReady) para que el
 *        resto del pipeline (protocolo, Pi 1) pueda probarse end-to-end
 *        con valores en cero, sin bloquear el desarrollo del resto del
 *        Nodo 2 a la espera de integrar el driver del fabricante.
 */
static bool BNO085_ReadEulerAngles(int16_t *out_pitch_x10, int16_t *out_roll_x10)
{
    if (HAL_I2C_IsDeviceReady(&SENSCFG_I2C_HANDLE, SENSCFG_BNO085_I2C_ADDR << 1,
                               2, 10) != HAL_OK) {
        return false;
    }

    /* TODO(firmware): integrar libreria SH-2 real. Valores en cero por
     * ahora — NO representan inclinacion real del vehiculo. */
    *out_pitch_x10 = 0;
    *out_roll_x10  = 0;

    return true;
}

/* ========================================================================
 *  TPMS — recepcion por interrupcion + parseo (parseo pendiente)
 * ======================================================================== */

/**
 * @brief Callback de HAL — se dispara por cada byte recibido del modulo
 *        TPMS. Simplemente lo acumula; el parseo real de trama ocurre
 *        en TPMS_ParseFrame() cuando se detecte un patron valido.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    static uint16_t s_rx_idx = 0;

    if (huart->Instance != SENSCFG_TPMS_UART_HANDLE.Instance) {
        return;
    }

    if (s_rx_idx < SENSCFG_TPMS_RX_BUF_LEN) {
        s_tpms_rx_buf[s_rx_idx++] = s_tpms_rx_byte;
    } else {
        s_rx_idx = 0; /* buffer lleno sin trama valida detectada, reinicia */
    }

    /* TODO(firmware): una vez conocido el formato real de trama de
     * canbustpms.com (byte de sincronizacion, longitud, checksum propio),
     * detectar aqui el cierre de trama y llamar a TPMS_ParseFrame()
     * con el contenido acumulado, luego resetear s_rx_idx = 0. */

    HAL_UART_Receive_IT(&SENSCFG_TPMS_UART_HANDLE, &s_tpms_rx_byte, 1);
}

/**
 * @brief TODO: implementar el parseo real una vez se tenga la hoja de
 *        datos del proveedor. Firma ya preparada para llenar
 *        s_tpms_wheel[0..3] con presion (PSI) y temperatura (°C) por
 *        rueda y marcar .valid = true.
 */
static void TPMS_ParseFrame(const uint8_t *frame, uint16_t len)
{
    (void)frame;
    (void)len;
    /* TODO(firmware): parseo pendiente — ver comentario de archivo. */
}

/* ========================================================================
 *  Envio por protocolo interno
 * ======================================================================== */

static void SensorReader_SendImu(void)
{
    int16_t pitch_x10, roll_x10;

    if (!BNO085_ReadEulerAngles(&pitch_x10, &roll_x10)) {
        s_imu_faulted = true;
        return;
    }
    s_imu_faulted = false;

    uint8_t payload_pitch[2] = { (uint8_t)(pitch_x10 >> 8), (uint8_t)(pitch_x10 & 0xFF) };
    uint8_t payload_roll[2]  = { (uint8_t)(roll_x10 >> 8),  (uint8_t)(roll_x10 & 0xFF) };

    Proto_SendFrame(&SENSCFG_UART_PI1_HANDLE, PROTO_NODE_ID_NODO2,
                     PROTO_MSG_ID_PITCH, payload_pitch, 2,
                     SENSCFG_UART_TX_TIMEOUT_MS);
    Proto_SendFrame(&SENSCFG_UART_PI1_HANDLE, PROTO_NODE_ID_NODO2,
                     PROTO_MSG_ID_ROLL, payload_roll, 2,
                     SENSCFG_UART_TX_TIMEOUT_MS);
}

static void SensorReader_SendCabin(void)
{
    int8_t  temp_c;
    uint8_t humidity_pct;

    if (!SHT31_ReadMeasurement(&temp_c, &humidity_pct)) {
        s_cabin_faulted = true;
        return;
    }
    s_cabin_faulted = false;

    uint8_t payload_temp[1] = { (uint8_t)temp_c };
    uint8_t payload_hum[1]  = { humidity_pct };

    /* Dominio NO critico: va a Pi 2, no a Pi 1 (ver DOC-PH1-ARQ-001 Sec. 4) */
    Proto_SendFrame(&SENSCFG_UART_PI2_HANDLE, PROTO_NODE_ID_NODO2,
                     PROTO_MSG_ID_CABIN_TEMP, payload_temp, 1,
                     SENSCFG_UART_TX_TIMEOUT_MS);
    Proto_SendFrame(&SENSCFG_UART_PI2_HANDLE, PROTO_NODE_ID_NODO2,
                     PROTO_MSG_ID_CABIN_HUMIDITY, payload_hum, 1,
                     SENSCFG_UART_TX_TIMEOUT_MS);
}

static void SensorReader_SendTpms(void)
{
    static const uint8_t msg_ids[4] = {
        PROTO_MSG_ID_TPMS_FL, PROTO_MSG_ID_TPMS_FR,
        PROTO_MSG_ID_TPMS_RL, PROTO_MSG_ID_TPMS_RR
    };

    for (uint8_t i = 0; i < 4; i++) {
        if (!s_tpms_wheel[i].valid) {
            continue; /* aun sin parser real (ver TODO), no se envia dato falso */
        }

        uint8_t payload[2] = {
            s_tpms_wheel[i].pressure_psi,
            (uint8_t)s_tpms_wheel[i].temp_c
        };

        Proto_SendFrame(&SENSCFG_UART_PI1_HANDLE, PROTO_NODE_ID_NODO2,
                         msg_ids[i], payload, 2,
                         SENSCFG_UART_TX_TIMEOUT_MS);
    }
}

/* ========================================================================
 *  API publica
 * ======================================================================== */

bool SensorReader_Init(void)
{
    s_imu_faulted   = false;
    s_cabin_faulted = false;
    memset(s_tpms_wheel, 0, sizeof(s_tpms_wheel));

    s_last_imu_tick   = HAL_GetTick();
    s_last_cabin_tick = HAL_GetTick();
    s_last_tpms_tick  = HAL_GetTick();

    bool imu_ok   = (HAL_I2C_IsDeviceReady(&SENSCFG_I2C_HANDLE,
                                            SENSCFG_BNO085_I2C_ADDR << 1, 2, 10) == HAL_OK);
    bool cabin_ok = (HAL_I2C_IsDeviceReady(&SENSCFG_I2C_HANDLE,
                                            SENSCFG_SHT31_I2C_ADDR << 1, 2, 10) == HAL_OK);

    /* Arranca la recepcion por interrupcion del TPMS (byte a byte) */
    HAL_UART_Receive_IT(&SENSCFG_TPMS_UART_HANDLE, &s_tpms_rx_byte, 1);

    return imu_ok && cabin_ok;
}

void SensorReader_Task(void)
{
    uint32_t now = HAL_GetTick();

    if ((now - s_last_imu_tick) >= (1000u / SENSCFG_RATE_IMU_HZ)) {
        s_last_imu_tick = now;
        SensorReader_SendImu();
    }

    if ((now - s_last_cabin_tick) >= (1000u / SENSCFG_RATE_CABIN_HZ)) {
        s_last_cabin_tick = now;
        SensorReader_SendCabin();
    }

    if ((now - s_last_tpms_tick) >= (1000u / SENSCFG_RATE_TPMS_HZ)) {
        s_last_tpms_tick = now;
        SensorReader_SendTpms();
    }
}

bool SensorReader_IsImuFaulted(void)
{
    return s_imu_faulted;
}

bool SensorReader_IsCabinSensorFaulted(void)
{
    return s_cabin_faulted;
}
