/* =============================================================================
 * homing.h  -  Rutina de referenciado de dos pasadas
 * =============================================================================
 *
 * Sin encoders, el cero absoluto del robot lo define el final de carrera. La
 * calidad de ese cero es la calidad de todo el sistema, asi que se busca dos
 * veces: una rapida para localizarlo y otra lenta para fijarlo. A 150 Hz la
 * dispersion de un microswitch mecanico baja mas de un orden de magnitud
 * respecto de una aproximacion rapida.
 * ---------------------------------------------------------------------------*/

#pragma once
#include <Arduino.h>
#include "types.h"

typedef enum {
    HOMING_OK          = 0,
    HOMING_TIMEOUT     = 1,   /* recorrio HOMING_MAX_TRAVEL_DEG sin tocar    */
    HOMING_ABORTED     = 2,   /* llego un STOP a mitad de la rutina          */
    HOMING_STUCK       = 3    /* el switch sigue pisado tras el retroceso    */
} HomingResult_t;

/* Referencia un solo eje.
 *
 *  set_zero = true   -> rutina normal: al tocar en la pasada lenta se le
 *                       asigna al eje su angulo de home y el robot queda
 *                       referenciado.
 *
 *  set_zero = false  -> MODO MEDICION, usado por el comando TEST. Hace la
 *                       misma aproximacion pero NO modifica la posicion:
 *                       escribe en *measured_steps la lectura del contador en
 *                       el instante del disparo. Comparada con la posicion de
 *                       home esperada, esa diferencia ES la perdida de pasos
 *                       acumulada. Es la unica forma de medirla sin encoder.
 */
HomingResult_t homing_axis(uint8_t axis, bool set_zero, int32_t *measured_steps);

/* Referencia los dos ejes. El eje 2 (codo) va PRIMERO, para que el brazo se
 * pliegue antes de que el hombro barra el espacio de trabajo. */
HomingResult_t homing_all(void);

const char *homing_result_name(HomingResult_t r);
