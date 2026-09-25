// =============================================================================
//  console.h — Salida serial segura entre tareas
//  Varias tareas (Comms, Telemetry, Motion) imprimen; un mutex evita que las
//  líneas se mezclen. TaskControl NUNCA imprime.
// =============================================================================
#pragma once
#include <Arduino.h>

void console_init();
void con_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
// Para bloques de varias líneas que deben salir juntos
void con_lock();
void con_unlock();
