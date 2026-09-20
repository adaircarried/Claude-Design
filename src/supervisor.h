/* =============================================================================
 * supervisor.h  -  Estado global, limites por software y paro de emergencia
 * =============================================================================
 *
 * TaskSupervisor es la tarea de MAYOR prioridad del nucleo 1. Su trabajo no es
 * mover nada: es vigilar. Corre cada 50 ms y comprueba que el robot siga
 * dentro de sus limites, que ningun final de carrera este pisado cuando no
 * deberia, y que el paro solicitado por el usuario se atienda de inmediato.
 *
 * Es tambien el dueño del estado global: una sola tarea escribe RobotState_t,
 * todas las demas lo leen. Eso evita tener que proteger el estado con un mutex.
 *
 * PARCIAL 3: los limites de espacio de trabajo por software que exige el
 * criterio de seguridad ya estan aqui. No hay que agregarlos despues.
 * ---------------------------------------------------------------------------*/

#pragma once
#include <Arduino.h>
#include "types.h"

void TaskSupervisor(void *pv);
void supervisor_init(void);

/* --- Estado y fallos --------------------------------------------------- */
RobotState_t sv_get_state(void);
void         sv_set_state(RobotState_t s);
FaultCode_t  sv_get_fault(void);
void         sv_set_fault(FaultCode_t f);
void         sv_clear_fault(void);
const char  *sv_state_name(RobotState_t s);
const char  *sv_fault_name(FaultCode_t f);

/* --- Referenciado ------------------------------------------------------ */
bool sv_is_homed(void);
void sv_set_homed(bool h);

/* ---------------------------------------------------------------------------
 * PARO FUERA DE BANDA  -  el mecanismo mas importante de este archivo
 * ---------------------------------------------------------------------------
 * sv_request_abort() NO encola nada. Levanta una bandera y vacia la cola de
 * setpoints. TaskMotion consulta sv_abort_requested() cada MOTION_TICK_MS
 * (5 ms) dentro de CUALQUIER movimiento en curso, incluido el homing y cada
 * segmento de un MOVL. Por eso un STOP se atiende en milisegundos aunque haya
 * un movimiento largo en ejecucion.
 *
 * Es seguro llamarla desde cualquier tarea y desde cualquier nucleo: la
 * bandera es un bool volatil (escritura atomica en el ESP32) y xQueueReset()
 * es seguro entre tareas.
 * -------------------------------------------------------------------------*/
void sv_request_abort(FaultCode_t reason);
bool sv_abort_requested(void);
void sv_clear_abort(void);

/* --- Finales de carrera ------------------------------------------------ */
/* Lectura con anti-rebote por muestreo: exige HOMING_DEBOUNCE_READS lecturas
 * consecutivas iguales. Los microswitches mecanicos rebotan entre 1 y 10 ms;
 * sin esto el homing se dispararia por un rebote y el cero saldria movido. */
bool sv_limit_triggered(uint8_t axis /*1 o 2*/);
bool sv_limit_raw(uint8_t axis);

/* --- Etapa de potencia ------------------------------------------------- */
/* ATENCION: sv_drivers_enable(false) libera el par de retencion y EL BRAZO
 * CAE POR GRAVEDAD (opera en plano vertical). Solo se llama desde el comando
 * DISABLE, nunca desde STOP. */
void sv_drivers_enable(bool on);
bool sv_drivers_enabled(void);
