/* =============================================================================
 * motion.h  -  TaskMotion: el unico modulo que le habla a los motores
 * =============================================================================
 *
 * EL CONTRATO CENTRAL DEL PROYECTO
 * --------------------------------
 * TaskMotion no sabe de donde vienen las ordenes. Solo consume Setpoint_t de
 * setpointQueue. Eso es lo que permite que este archivo NO se toque en los
 * tres parciales:
 *
 *      Parcial 1 -> la cola la llena la consola de texto
 *      Parcial 2 -> la llena el parser binario con CRC
 *      Parcial 3 -> la llena la logica de juego
 *
 * Si algun dia necesitas modificar motion.cpp para agregar una fuente de
 * comandos, algo se hizo mal: la fuente nueva debe encolar, no llamar.
 * ---------------------------------------------------------------------------*/

#pragma once
#include <Arduino.h>
#include <FastAccelStepper.h>
#include "types.h"

/* La cola. Se crea en main.cpp; la declaran aqui los dos extremos. */
extern QueueHandle_t setpointQueue;

void motion_init(void);
void TaskMotion(void *pv);

/* Acceso a los objetos de FastAccelStepper. Lo usa homing.cpp, que necesita
 * control directo de bajo nivel durante el referenciado. axis = 1 o 2. */
FastAccelStepper *motion_stepper(uint8_t axis);

/* Posicion actual. La fuente de verdad son los MICROPASOS; los grados son una
 * conversion de conveniencia (ver nota sobre 44.444 en config.h). */
void motion_get_steps(int32_t *s1, int32_t *s2);
void motion_get_joints(float *q1, float *q2);

/* Velocidad global, 1..100 %. Afecta a MOVJ y MOVL, no al JOG ni al homing,
 * que son siempre lentos a proposito. */
void     motion_set_speed_pct(uint16_t pct);
uint16_t motion_get_speed_pct(void);

void          motion_set_elbow(ElbowConfig_t e);
ElbowConfig_t motion_get_elbow(void);

bool motion_is_busy(void);

/* Frena ambos ejes con rampa de desaceleracion reforzada, manteniendo el par
 * de retencion. NO libera ENA (ver config.h, seccion 9). */
void motion_decelerate_stop(void);
