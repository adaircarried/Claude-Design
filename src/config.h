/* =============================================================================
 * config.h  -  Constantes de configuracion del brazo RR de 2 GDL
 * =============================================================================
 *
 * TODO LO AJUSTABLE VIVE AQUI. Ningun otro archivo debe tener numeros magicos.
 *
 * Este archivo NO incluye <Arduino.h> a proposito: kinematics.cpp lo incluye y
 * se compila tambien para el entorno "native" (tests en la PC, sin ESP32).
 * Si agregas algo dependiente de Arduino aqui, rompes los tests unitarios.
 * ---------------------------------------------------------------------------*/

#pragma once
#include <stdint.h>

/* ===========================================================================
 * 1. ASIGNACION DE PINES  (ESP32 DevKit V1 / ESP32-WROOM-32)
 * ===========================================================================
 * Restricciones respetadas:
 *   - GPIO 6..11  -> conectados a la flash SPI, INUTILIZABLES.
 *   - GPIO 34..39 -> solo entrada, SIN pull-up interno (no sirven para finales
 *                    de carrera con pull-up, por eso usamos 16 y 17).
 *   - GPIO 0,2,12,15 -> pines de arranque (strapping). GPIO 12 es el mas
 *                    peligroso: si esta en alto al arrancar, el ESP32 fija el
 *                    voltaje de la flash en 1.8 V y no bootea. Ninguno se usa.
 *   - GPIO 1 (TX) y 3 (RX) -> consola serial, no tocar.
 * -------------------------------------------------------------------------*/
#define PIN_A1_PUL          25      /* Motor 1 (hombro) - pulso            */
#define PIN_A1_DIR          26      /* Motor 1 (hombro) - direccion        */
#define PIN_A2_PUL          32      /* Motor 2 (codo)   - pulso            */
#define PIN_A2_DIR          33      /* Motor 2 (codo)   - direccion        */
#define PIN_ENABLE          27      /* ENA compartido, ACTIVO EN BAJO      */
#define PIN_LIMIT_A1        16      /* Final de carrera eje 1 (pull-up)    */
#define PIN_LIMIT_A2        17      /* Final de carrera eje 2 (pull-up)    */
#define PIN_GRIPPER_SERVO   13      /* SG90 de la pinza - RESERVADO P3     */

/* Finales de carrera NORMALMENTE CERRADOS (NC), cableados GPIO -- GND con
 * pull-up interno activo:
 *      sin accionar -> contacto cerrado -> el pin ve GND       -> LOW
 *      accionado    -> contacto abierto -> el pull-up lo sube  -> HIGH
 * Es el cableado a prueba de fallos: un cable cortado se lee como "accionado"
 * y detiene el movimiento en vez de dejar que el eje se estrelle.
 * VERIFICA ESTO CON MULTIMETRO ANTES DEL PRIMER HOMING (ver README). */
#define LIMIT_ACTIVE_LEVEL  1       /* 1 = HIGH significa accionado */

/* ===========================================================================
 * 2. RESOLUCION DE LA TRANSMISION
 * ===========================================================================
 *   200 pasos/vuelta (motor 1.8 deg)  x  5 (reductor)  x  16 (micropaso)
 *   = 16000 micropasos por vuelta de ARTICULACION
 *   = 44.4444... micropasos por grado de articulacion
 *
 * OJO: 44.444 NO es entero. Por eso la posicion se guarda SIEMPRE en
 * micropasos (int32_t) y los grados son solo la representacion de entrada y
 * salida. Si acumularas movimientos relativos en grados, el error de redondeo
 * se sumaria movimiento a movimiento hasta desalinear el brazo.
 * -------------------------------------------------------------------------*/
#define MOTOR_STEPS_PER_REV     200.0f
#define GEARBOX_RATIO           5.0f
#define MICROSTEPS              16.0f
#define STEPS_PER_JOINT_REV     (MOTOR_STEPS_PER_REV * GEARBOX_RATIO * MICROSTEPS) /* 16000 */
#define STEPS_PER_DEG           (STEPS_PER_JOINT_REV / 360.0f)                     /* 44.4444 */
#define DEG_PER_STEP            (1.0f / STEPS_PER_DEG)                             /* 0.0225 deg */

/* Backlash del reductor: 15 arcmin = 0.25 deg = ~11 micropasos.
 * ES EL FACTOR DOMINANTE DE PRECISION, no el micropaso:
 *   1 micropaso  -> 0.0225 deg -> 0.14 mm a 350 mm de alcance
 *   backlash     -> 0.25   deg -> 1.53 mm a 350 mm de alcance
 * Conclusion practica: aproxima siempre las poses desde la MISMA direccion.
 * La compensacion por software esta desactivada por defecto porque introduce
 * errores peores si el valor no esta bien medido (ver README). */
#define BACKLASH_DEG            0.25f
#define BACKLASH_COMPENSATION   0       /* 0 = desactivada (recomendado en P1) */

/* Sentido de giro. Si un eje se mueve al reves de lo esperado, invierte aqui
 * en vez de recablear el motor (NUNCA desconectes un motor con el driver
 * energizado: destruye el TB6600). */
#define A1_DIR_HIGH_COUNTS_UP   true
#define A2_DIR_HIGH_COUNTS_UP   true

/* El TB6600 exige ~5 us de establecimiento de DIR antes del flanco de PUL.
 * 20 us da margen de sobra para los optoacopladores, que son lentos. */
#define DIR_CHANGE_DELAY_US     20

/* ===========================================================================
 * 3. GEOMETRIA
 * ===========================================================================*/
#define LINK_L1_MM              200.0f  /* eje hombro -> eje codo     */
#define LINK_L2_MM              150.0f  /* eje codo   -> punta efector */

/* Espacio de trabajo util: anillo entre R_MIN y R_MAX.
 * El limite teorico es [|l1-l2|, l1+l2] = [50, 350] mm, pero se recorta con
 * margen: en r = 350 el brazo esta completamente estirado y la cinematica
 * inversa es singular (acos de un argumento que roza 1.0, cond. numerica
 * pesima y velocidades articulares que se disparan). En r = 50 pasa lo mismo
 * con el codo totalmente plegado. */
#define WS_R_MIN_MM             60.0f
#define WS_R_MAX_MM             340.0f

/* El brazo opera en plano VERTICAL. Este limite evita que el efector baje
 * mas alla de la mesa. AJUSTA ESTE VALOR AL MONTAR EL ROBOT. */
#define WS_Y_MIN_MM             (-250.0f)

/* ===========================================================================
 * 4. LIMITES ARTICULARES        <<< AJUSTAR AL MONTAR EL MECANISMO >>>
 * ===========================================================================
 * Valores conservadores de arranque. Convencion: q1 = 0 con el brazo apuntando
 * horizontalmente hacia +X; q2 = 0 con el antebrazo alineado con el brazo.
 * Angulos positivos = antihorario visto desde el lado del motor.
 * -------------------------------------------------------------------------*/
#define A1_MIN_DEG              (-90.0f)
#define A1_MAX_DEG              ( 90.0f)
#define A2_MIN_DEG              (-135.0f)
#define A2_MAX_DEG              ( 135.0f)

/* ===========================================================================
 * 5. PERFILES DE MOVIMIENTO
 * ===========================================================================
 * Velocidades en micropasos/segundo (Hz de pulso). Referencia:
 *   3000 Hz / 44.44 = 67.5 deg/s de articulacion
 *                   = 0.94 rev/s de articulacion
 *                   = 56 RPM en el eje del motor  (muy holgado para un NEMA17)
 * El limite real no es el motor: es el par disponible contra la gravedad y la
 * inercia del brazo. Empieza bajo y sube midiendo con el comando TEST.
 * -------------------------------------------------------------------------*/
#define AXIS_MAX_SPEED_HZ       3000U   /* a SPEED 100 */
#define AXIS_MIN_SPEED_HZ       50U     /* piso: FastAccelStepper rechaza 0 */
#define AXIS_MAX_ACCEL          8000U   /* micropasos/s^2 (~0.37 s a v_max) */
#define JOG_SPEED_HZ            800U    /* JOG siempre lento, es manual      */
#define JOG_ACCEL               4000U

/* MOVL: avance del efector en linea recta */
#define MOVL_MAX_FEED_MMS       80.0f   /* mm/s a SPEED 100 */
#define MOVL_SEGMENT_TICKS_MS   20      /* periodo de actualizacion del target */
#define MOVL_VALIDATE_SAMPLES   40      /* puntos de la recta a validar antes  */
                                        /* de empezar a moverse                */

/* ===========================================================================
 * 6. HOMING
 * ===========================================================================
 * Rutina de dos pasadas: aproximacion rapida, retroceso, aproximacion lenta.
 * La segunda pasada es la que fija el cero: a baja velocidad la dispersion del
 * microswitch mecanico baja de ~0.3 mm a ~0.02 mm.
 * -------------------------------------------------------------------------*/
#define HOMING_FAST_HZ          600U
#define HOMING_SLOW_HZ          150U
#define HOMING_ACCEL            3000U
#define HOMING_BACKOFF_DEG      5.0f    /* retroceso entre pasadas */
#define HOMING_MAX_TRAVEL_DEG   200.0f  /* si recorre mas que esto sin tocar el
                                           switch -> fallo. Sin esto, un switch
                                           mal cableado hace que el eje empuje
                                           contra el tope mecanico hasta que
                                           algo se rompe. */
#define HOMING_DEBOUNCE_READS   3       /* lecturas consecutivas para validar */
#define HOMING_POLL_MS          2

/* Direccion de busqueda del cero: -1 hacia angulos negativos, +1 positivos. */
#define A1_HOMING_DIR           (-1)
#define A2_HOMING_DIR           ( 1)

/* Angulo que se asigna al eje en el instante en que toca su final de carrera.
 * Con estos valores el brazo queda plegado y apuntando hacia abajo al hacer
 * home, que es la pose segura de arranque. */
#define A1_HOME_DEG             (-90.0f)
#define A2_HOME_DEG             ( 135.0f)

/* El eje 2 (codo) hace home PRIMERO para que el brazo se pliegue antes de que
 * el hombro barra el espacio de trabajo y se lleve algo por delante. */

/* ===========================================================================
 * 7. TAREAS DE FREERTOS
 * ===========================================================================
 * El nucleo 0 aloja el stack de WiFi/BT del ESP32. TODO lo relacionado con
 * movimiento va al nucleo 1 para que una rafaga de red no introduzca jitter
 * en el control.
 * -------------------------------------------------------------------------*/
#define CORE_MOTION             1
#define CORE_COMMS              0

#define TASK_SUPERVISOR_PRIO    6
#define TASK_MOTION_PRIO        5
#define TASK_COMMS_PRIO         2
#define TASK_TELEMETRY_PRIO     1

#define TASK_SUPERVISOR_STACK   3072
#define TASK_MOTION_STACK       4096
#define TASK_COMMS_STACK        8192
#define TASK_TELEMETRY_STACK    4096

#define SUPERVISOR_PERIOD_MS    50
#define TELEMETRY_PERIOD_MS     100
#define MOTION_TICK_MS          5       /* ver comentario en motion.cpp sobre
                                           por que TaskMotion hace polling y no
                                           bloqueo puro en la cola */

#define SETPOINT_QUEUE_LEN      8

/* ===========================================================================
 * 8. COMUNICACION
 * ===========================================================================*/
#define SERIAL_BAUD             115200
#define RX_RING_SIZE            512     /* buffer circular de recepcion.
                                           Sobredimensionado a proposito: en el
                                           Parcial 2 lo reutiliza el parser
                                           binario sin tocar nada mas. */
#define CMD_LINE_MAX            96

/* ===========================================================================
 * 9. SEGURIDAD
 * ===========================================================================*/
/* STOP desacelera y MANTIENE PAR DE RETENCION. No libera ENA.
 * Razon: el brazo trabaja en plano vertical; soltar los drivers lo deja caer
 * por gravedad y puede destrozar la pinza, la ficha o el propio mecanismo.
 * Para liberar los motores hay un comando explicito y separado: DISABLE. */
#define ENABLE_ACTIVE_LEVEL     0       /* ENA del TB6600 es activo en bajo */
#define STOP_DECEL_MULTIPLIER   3       /* la desaceleracion del STOP es 3x la
                                           normal: frena rapido pero sin perder
                                           pasos por un corte brusco */
