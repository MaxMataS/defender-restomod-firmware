/**
 * @file    can_sim_config.h
 * @brief   Configuracion del simulador de trafico CAN - Nodo 3 (herramienta de banco)
 * @note    Programa: [Marca] - Defender Restomod
 *          Documento relacionado: DOC-PH1-PROTO-001 (protocolo interno STM32<->Pi)
 *          Este nodo NO va en el vehiculo de produccion.
 *
 * ============================================================================
 *  ADVERTENCIA IMPORTANTE - LEER ANTES DE USAR
 * ============================================================================
 *  Los IDs de CAN definidos abajo son PLACEHOLDERS (marcadores de posicion).
 *  NO son los identificadores reales del bus GMLAN del LS3/6L80.
 *
 *  Los IDs reales de GM son propietarios y varian segun anio/ECU/config.
 *  Deben obtenerse capturando trafico real del vehiculo donante con el
 *  analizador PEAK-System PCAN-USB Pro FD (ya contemplado en el BOM,
 *  item B.4) una vez que el motor este disponible para pruebas.
 *
 *  Hasta entonces, este simulador sirve para validar:
 *   - Que el Nodo 1 recibe, filtra y parsea tramas CAN correctamente
 *   - Que el formato de salida UART hacia el Pi cumple DOC-PH1-PROTO-001
 *   - Que el timing y los timeouts definidos en el protocolo funcionan
 *
 *  NO valida que los IDs/formato coincidan con el LS3/6L80 real. Ese paso
 *  ocurre en la Fase 1 -> Fase 2, cuando se capture trafico real.
 * ============================================================================
 */

#ifndef CAN_SIM_CONFIG_H
#define CAN_SIM_CONFIG_H

#include <stdint.h>

/* ---------------------------------------------------------------------- *
 *  IDs de CAN simulados (PLACEHOLDER - reemplazar con captura real)
 * ---------------------------------------------------------------------- */
#define CANSIM_ID_RPM_SPEED        0x1A0u   /* PLACEHOLDER: RPM + velocidad combinados */
#define CANSIM_ID_ENGINE_TEMP      0x1A1u   /* PLACEHOLDER: temp. refrigerante + presion aceite */
#define CANSIM_ID_TRANS_STATUS     0x1A2u   /* PLACEHOLDER: marcha activa (6L80 / TCM) */
#define CANSIM_ID_DTC              0x1A3u   /* PLACEHOLDER: codigo de falla activo */

/* ---------------------------------------------------------------------- *
 *  Parametros de simulacion
 * ---------------------------------------------------------------------- */
#define CANSIM_RPM_MIN              800u     /* ralenti */
#define CANSIM_RPM_MAX              5500u    /* limite superior del ciclo simulado */
#define CANSIM_RPM_STEP              50u     /* incremento por tick del generador */

#define CANSIM_SPEED_MIN_KMH_X10      0      /* 0.0 km/h */
#define CANSIM_SPEED_MAX_KMH_X10   1200      /* 120.0 km/h */

#define CANSIM_COOLANT_TEMP_C        90      /* temperatura fija simulada, grados C */
#define CANSIM_OIL_PRESSURE_PSI      45      /* presion fija simulada, PSI */

/* Periodos de envio (ms) - deben ser razonablemente cercanos a lo que
 * produciria el bus real, para probar timing/timeouts del protocolo */
#define CANSIM_PERIOD_RPM_SPEED_MS    50u    /* ~20 Hz, igual que MSG_ID 0x10/0x11 del protocolo */
#define CANSIM_PERIOD_ENGINE_MS      500u    /* ~2 Hz, igual que MSG_ID 0x12/0x13 */
#define CANSIM_PERIOD_TRANS_MS       500u    /* ~2 Hz, igual que MSG_ID 0x14 */

/* Ciclo de prueba: patron de aceleracion/desaceleracion simulado, en vez
 * de un valor fijo, para ejercitar el parser del Nodo 1 con datos que
 * cambian (mas representativo que un valor constante) */
typedef enum {
    CANSIM_PROFILE_RAMP_UP_DOWN = 0,  /* rampa sube y baja entre RPM_MIN y RPM_MAX */
    CANSIM_PROFILE_IDLE_ONLY,         /* se queda fijo en ralenti, util para pruebas de timeout */
    CANSIM_PROFILE_FAULT_INJECT       /* inyecta un DTC simulado a mitad de ciclo */
} CanSim_Profile_t;

#define CANSIM_ACTIVE_PROFILE  CANSIM_PROFILE_RAMP_UP_DOWN

#endif /* CAN_SIM_CONFIG_H */
