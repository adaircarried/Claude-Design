// =============================================================================
//  config.h — Pines, constantes físicas y valores por defecto
// =============================================================================
//  Todo lo que depende del hardware concreto vive aquí. Si cambias un motor,
//  un pin o la geometría del brazo, solo se toca este archivo.
//
//  Hardware de referencia:
//    - ESP-32S NodeMCU (módulo ESP-32S = ESP32-D0WD, igual al WROOM-32)
//    - 2x GM25-370 12 V, 140 rpm (reducción ~1:34) con encoder Hall
//    - Driver TB6612FNG (MOSFET), VM = 12 V, VCC = 3.3 V, tierra común
// =============================================================================
#pragma once
#include <Arduino.h>

// ----------------------------------------------------------------------------
//  Ejes
// ----------------------------------------------------------------------------
#define NUM_AXES 2
#define AXIS_1   0   // J1, hombro (q1)
#define AXIS_2   1   // J2, codo   (q2)

// ----------------------------------------------------------------------------
//  Pines TB6612FNG (verificados contra las restricciones del ESP32 clásico:
//  ninguno está en 6-11 (flash), 34-39 (solo entrada) ni es de arranque
//  salvo GPIO 14, que emite PWM brevemente al arrancar: inofensivo porque
//  GPIO 4 (STBY) tiene pull-down interno en reset -> driver apagado.)
// ----------------------------------------------------------------------------
static const uint8_t PIN_PWM[NUM_AXES] = {25, 14};   // PWMA, PWMB
static const uint8_t PIN_INA[NUM_AXES] = {26, 32};   // AIN1, BIN1
static const uint8_t PIN_INB[NUM_AXES] = {27, 33};   // AIN2, BIN2
// STBY en bajo apaga ambos canales (salidas en alta impedancia). Se usa como
// corte duro ante una falla. GPIO 4 no es pin de arranque.
#define PIN_STBY 4

// ----------------------------------------------------------------------------
//  Pines de encoder (canal A = amarillo, canal B = verde)
//  GPIO 16/17 son libres en el ESP-32S NodeMCU (sin PSRAM). En un módulo WROVER están
//  ocupados por la PSRAM: si fuera el caso, mover a 4/5 o 21/22.
// ----------------------------------------------------------------------------
static const uint8_t PIN_ENC_A[NUM_AXES] = {16, 18};
static const uint8_t PIN_ENC_B[NUM_AXES] = {17, 19};

// ----------------------------------------------------------------------------
//  Modo del encoder por eje
//    ENC_QUADRATURE : A y B funcionan -> PCNT x4, dirección real.
//    ENC_SINGLE     : solo funciona el canal A (cable verde roto) -> PCNT x2
//                     sobre A, y la DIRECCIÓN SE INFIERE del signo del PWM
//                     aplicado. Ver README, sección "Encoder de un canal".
//  Recomendación: el motor con el encoder dañado va en J2 (codo). Un error
//  en J1 se amplifica por el brazo completo (350 mm); en J2 solo por 150 mm.
// ----------------------------------------------------------------------------
enum EncoderMode : uint8_t { ENC_QUADRATURE = 0, ENC_SINGLE = 1 };
static const EncoderMode ENC_MODE[NUM_AXES] = {ENC_QUADRATURE, ENC_SINGLE};

// Inversión de sentido por eje (ver README, "Prueba de sentido").
//  MOTOR_INVERT: invierte el sentido del PWM aplicado (equivale a cruzar los
//                cables del motor en la bornera).
//  ENC_INVERT  : invierte el signo de la lectura del encoder (solo tiene
//                efecto en modo cuadratura).
static const bool MOTOR_INVERT[NUM_AXES] = {false, false};
static const bool ENC_INVERT[NUM_AXES]   = {false, false};

// Filtro de glitches del PCNT en ciclos de APB (80 MHz -> 12.5 ns). 1000 ~ 12.5 us.
// Motor a ~4700 rpm * 11 PPR = ~860 Hz por canal (periodo ~1.1 ms): hay margen.
#define ENC_FILTER_APB_CYCLES 1000

// Cuentas por vuelta del EJE DE SALIDA por defecto (antes de calibrar).
// Medido con SPIN en los motores reales: J1 = 1973 cuentas/vuelta en x4,
// que corresponde a encoder de 11 PPR con reducción ~1:45 (11*4*45 = 1980).
//   cuadratura x4 : 11 * 4 * 45 = 1980
//   un canal  x2 : 11 * 2 * 45 =  990
// CAL / SPIN + CPR miden el valor real y lo guardan en NVS (Preferences).
static const float DEFAULT_CPR[NUM_AXES] = {1980.0f, 990.0f};

// ----------------------------------------------------------------------------
//  PWM (periférico LEDC)
// ----------------------------------------------------------------------------
#define PWM_FREQ_HZ     20000          // audible < 20 kHz; TB6612 soporta hasta 100 kHz
#define PWM_RES_BITS    10
#define PWM_MAX         1023           // 2^10 - 1
static const uint8_t PWM_CHANNEL[NUM_AXES] = {0, 1};

// Límite de PWM aplicado (protección térmica y de corriente). El TB6612 da
// 1.2 A continuos por canal (3.2 A pico) y el GM25-370 consume ~1.1 A
// bloqueado a 12 V: 90 % deja margen. La caída del TB6612 es solo ~0.5 V.
#define PWM_LIMIT_DEFAULT  920

// ----------------------------------------------------------------------------
//  Lazo de control
// ----------------------------------------------------------------------------
#define CONTROL_PERIOD_MS 5            // 200 Hz
#define MOTION_PERIOD_MS  10           // 100 Hz
#define TELEMETRY_PERIOD_MS 100
#define CONTROL_DT        (CONTROL_PERIOD_MS / 1000.0f)

// Ganancias por defecto: salida en cuentas de PWM (0..1023) por grado de error.
// Elegidas con una simulación del GM25-370 (modelo de 1er orden + fricción de
// Coulomb) siguiendo el perfil trapezoidal; ver README "Sintonía del PID".
#define PID_KP_DEFAULT   60.0f         // 5° de error -> 300 de PWM (~30 %)
#define PID_KI_DEFAULT   20.0f
#define PID_KD_DEFAULT   1.0f
// Feed-forward de velocidad (PWM por °/s). El motor da ~840 °/s con PWM 1023
// => ideal ~1.2. Se usa 1.0 (subcompensa, seguro). Sin él, el integrador
// tiene que "cargar" durante el movimiento y provoca sobrepaso al llegar.
#define PID_KFF_DEFAULT  1.0f
#define PID_DEADBAND_DEFAULT 0.5f      // grados
#define PID_PWM_MIN_DEFAULT  0         // compensación de fricción estática
#define PID_ILIMIT_FRAC  0.5f          // el integral aporta como máx. 50 % del PWM
#define PID_D_FILTER_ALPHA 0.2f        // filtro pasa-bajas del derivativo (0..1)

// ----------------------------------------------------------------------------
//  Perfil de movimiento
//  GM25-370 12 V / 140 rpm: sin carga ~840 °/s a 12 V (el TB6612 casi no
//  tiene caída). SPEED 100 % se fija muy por debajo para tener
//  margen de par y que el PID pueda seguir el perfil.
// ----------------------------------------------------------------------------
#define VMAX_JOINT_DEG_S     180.0f    // SPEED 100 % en articulares
#define ACCEL_DEFAULT_DEG_S2 360.0f    // ACCEL por defecto
#define ACCEL_MIN_DEG_S2     10.0f
#define ACCEL_MAX_DEG_S2     3000.0f
#define SPEED_PCT_DEFAULT    30
#define JOG_SPEED_FACTOR     0.3f      // JOG a velocidad reducida (30 % de SPEED)
#define VMAX_LINEAR_MM_S     200.0f    // SPEED 100 % en MOVL
// MOVL usa ACCEL * (VMAX_LINEAR / VMAX_JOINT) en mm/s² para conservar la proporción

// ----------------------------------------------------------------------------
//  Geometría del robot (planar RR)
// ----------------------------------------------------------------------------
#define L1_MM 200.0f
#define L2_MM 150.0f
#define WS_RMIN_MM (L1_MM - L2_MM)     // 50 mm
#define WS_RMAX_MM (L1_MM + L2_MM)     // 350 mm
#define WS_MARGIN_MM 2.0f              // se evita el borde exacto (singularidad)

// Solución de la cinemática inversa: codo arriba => q2 negativo con la
// convención estándar (q1 antihorario desde +X). Cambiar a false para codo abajo.
#define ELBOW_UP true

// Límites articulares (grados) respecto al HOME (brazo estirado sobre +X).
static const float JOINT_MIN_DEG[NUM_AXES] = {-170.0f, -150.0f};
static const float JOINT_MAX_DEG[NUM_AXES] = { 170.0f,  150.0f};

// ----------------------------------------------------------------------------
//  Diagnóstico: detección de motor bloqueado y error de seguimiento
// ----------------------------------------------------------------------------
// Bloqueo: PWM alto + velocidad casi nula durante un tiempo -> corta salida.
#define STALL_PWM_FRAC   0.60f         // |PWM| > 60 % de PWM_LIMIT
#define STALL_VEL_DEG_S  3.0f          // velocidad medida < 3 °/s
#define STALL_TIME_MS    800
// Error de seguimiento excesivo. Caso típico: motor o encoder con el sentido
// invertido -> realimentación positiva -> el eje se desboca a PWM máximo.
// Con perfil trapezoidal el error normal es de unos pocos grados (simulado:
// < 6° a SPEED 100), así que 20° sostenidos 100 ms solo ocurren si algo está
// mal. 100 ms a ~700 °/s son ~70° de recorrido antes de cortar: pruebe
// primero con LIMIT 400 y el brazo sin carga.
#define FOLLOW_ERR_DEG   20.0f
#define FOLLOW_TIME_MS   100

// ----------------------------------------------------------------------------
//  FreeRTOS: núcleo, prioridad y stack de cada tarea
//  Núcleo 0 = PRO_CPU (WiFi/BT cuando se usen). Núcleo 1 = APP_CPU (control).
// ----------------------------------------------------------------------------
#define CORE_CONTROL 1
#define CORE_COMMS   0
#define PRIO_CONTROL   6
#define PRIO_MOTION    5
#define PRIO_COMMS     2
#define PRIO_TELEMETRY 1
#define STACK_CONTROL   4096
#define STACK_MOTION    4096
#define STACK_COMMS     8192
#define STACK_TELEMETRY 4096

#define SETPOINT_QUEUE_LEN 16

// ----------------------------------------------------------------------------
//  Consola
// ----------------------------------------------------------------------------
#define SERIAL_BAUD 115200
#define TEST_SETTLE_MS 300             // espera tras llegar antes de medir error final
#define SWEEP_AMPLITUDE_DEG 45.0f      // amplitud usada por SWEEP
