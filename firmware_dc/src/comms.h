// =============================================================================
//  comms.h — TaskComms: consola serial de texto (núcleo 0, prioridad 2)
// =============================================================================
//  Es UN productor de Setpoint_t entre otros posibles. Traduce texto a
//  Setpoint_t y llama a motion_submit(); nunca toca motores ni el PID directo.
// =============================================================================
#pragma once
void task_comms(void *arg);
