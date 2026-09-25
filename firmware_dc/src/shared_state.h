// =============================================================================
//  shared_state.h — Datos compartidos entre tareas, protegidos por mutex
// =============================================================================
//  Regla de propiedad (quién escribe qué):
//    - cmd[]         : TaskMotion (y TaskControl al re-sincronizar tras HOME,
//                      falla o calibración).
//    - req_*         : cualquier tarea "pide"; TaskControl ejecuta y limpia.
//                      Así las operaciones que tocan el encoder o el PID
//                      (poner en cero, cambiar ganancias, reset de falla)
//                      ocurren SIEMPRE en el contexto de TaskControl, entre
//                      dos muestras, sin carreras.
//    - st[], fault[] : TaskControl.
//    - motion_*      : TaskMotion.
//  Las secciones críticas son copias de unos cuantos bytes; TaskControl toma
//  el mutex con timeout de 1 tick y, si no lo obtiene, usa los últimos valores
//  en lugar de bloquearse.
// =============================================================================
#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config.h"
#include "pid.h"

enum FaultCode : uint8_t {
    FAULT_NONE = 0,
    FAULT_STALL,        // motor bloqueado: PWM alto sin movimiento
    FAULT_FOLLOW        // error de seguimiento excesivo (¿encoder invertido?)
};

struct AxisCmd {
    float    pos_ref;   // posición de referencia (°)
    float    vel_ref;   // velocidad de referencia (°/s), para feed-forward
};

struct AxisStatus {
    float    pos_ref;   // referencia que usó el PID en este ciclo
    float    pos;       // posición medida (°)
    float    err;       // pos_ref - pos
    float    vel;       // velocidad medida (°/s)
    int16_t  pwm;       // PWM aplicado con signo
    int64_t  counts;    // cuentas crudas del encoder
};

struct SharedState {
    // ---- TaskMotion -> TaskControl
    AxisCmd    cmd[NUM_AXES];
    uint32_t   cmd_seq;               // se incrementa en cada escritura de cmd

    // ---- Peticiones -> TaskControl
    uint8_t    req_zero_mask;         // bit i: poner eje i en cero (HOME)
    bool       req_gains[NUM_AXES];   // hay ganancias nuevas en gains[]
    PIDGains   gains[NUM_AXES];
    bool       req_fault_reset;
    int8_t     cal_axis;              // -1 = sin calibración; eje en CAL (sin par)
    bool       req_cal_zero;          // poner en cero el eje en calibración
    float      cpr[NUM_AXES];         // cuentas por vuelta del eje de salida
    int        pwm_limit;
    int8_t     ol_axis;               // -1 = normal; eje en lazo abierto (FRIC)
    int        ol_pwm;                // PWM de lazo abierto

    // ---- TaskControl -> todos
    AxisStatus st[NUM_AXES];
    FaultCode  fault[NUM_AXES];
    uint32_t   control_cycles;
    uint32_t   control_overruns;      // ciclos en que vTaskDelayUntil ya iba tarde

    // ---- TaskMotion -> todos
    bool       motion_busy;
    uint32_t   moves_done;            // contador de órdenes terminadas
    float      move_max_err[NUM_AXES];// |error| máximo durante el último movimiento
    float      accel;                 // ACCEL actual (°/s²)
};

extern SharedState       g_state;
extern SemaphoreHandle_t g_stateMutex;

void shared_state_init();

// Bloqueo de conveniencia. Devuelve false si no se obtuvo en 'wait'.
static inline bool state_lock(TickType_t wait = portMAX_DELAY) {
    return xSemaphoreTake(g_stateMutex, wait) == pdTRUE;
}
static inline void state_unlock() { xSemaphoreGive(g_stateMutex); }
