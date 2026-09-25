// =============================================================================
//  control.h — TaskControl: PID de posición de ambos ejes a 200 Hz (núcleo 1)
// =============================================================================
#pragma once
#include <stdint.h>
void task_control(void *arg);
const char *fault_name(uint8_t code);
