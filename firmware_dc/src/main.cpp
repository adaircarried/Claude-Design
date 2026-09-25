// =============================================================================
//  main.cpp — Arranque: estado compartido, periféricos, cola y tareas
// =============================================================================
//  Reparto de tareas (ver config.h):
//
//   Núcleo 1 (APP_CPU) — tiempo real
//     TaskControl   prio 6   5 ms  PID de ambos ejes, fallas, PWM
//     TaskMotion    prio 5  10 ms  cola -> perfil trapezoidal -> referencias
//
//   Núcleo 0 (PRO_CPU) — comunicación (aquí vivirá el stack WiFi/BT)
//     TaskComms     prio 2  evento  consola serial -> Setpoint_t -> cola
//     TaskTelemetry prio 1 100 ms  reporte de estado
//
//  Ninguna tarea hace espera activa: todas ceden CPU con vTaskDelayUntil,
//  vTaskDelay o bloqueándose en cola/mutex, así el watchdog de la tarea IDLE
//  nunca se dispara.
//
//  Para el siguiente parcial (protocolo binario): se agrega una tarea (o se
//  reemplaza TaskComms) que decodifique tramas y llame a motion_submit() con
//  un Setpoint_t. Control, perfiles y cinemática no cambian.
// =============================================================================
#include <Arduino.h>
#include "config.h"
#include "shared_state.h"
#include "storage.h"
#include "encoder.h"
#include "motor.h"
#include "motion.h"
#include "control.h"
#include "comms.h"
#include "telemetry.h"
#include "console.h"

void setup() {
    // Primero apagar la etapa de potencia (los pines arrancan flotando)
    motor_init();

    Serial.begin(SERIAL_BAUD);
    delay(200);
    console_init();
    shared_state_init();
    storage_load(g_state.cpr, g_state.gains);
    storage_load_motion(g_state.pwm_limit, g_state.speed_pct, g_state.accel);
    for (uint8_t i = 0; i < NUM_AXES; i++) g_state.req_gains[i] = true;

    encoder_init();
    motion_init();

    xTaskCreatePinnedToCore(task_control,   "TaskControl",   STACK_CONTROL,   nullptr, PRIO_CONTROL,   nullptr, CORE_CONTROL);
    xTaskCreatePinnedToCore(task_motion,    "TaskMotion",    STACK_MOTION,    nullptr, PRIO_MOTION,    nullptr, CORE_CONTROL);
    xTaskCreatePinnedToCore(task_comms,     "TaskComms",     STACK_COMMS,     nullptr, PRIO_COMMS,     nullptr, CORE_COMMS);
    xTaskCreatePinnedToCore(task_telemetry, "TaskTelemetry", STACK_TELEMETRY, nullptr, PRIO_TELEMETRY, nullptr, CORE_COMMS);
}

// El loop() de Arduino no se usa: todo corre en tareas. Se elimina su tarea
// para que no ocupe CPU en el núcleo 1.
void loop() {
    vTaskDelete(nullptr);
}
