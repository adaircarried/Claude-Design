// =============================================================================
//  motor.h — Etapa de potencia L298N con PWM por LEDC (hardware)
// =============================================================================
#pragma once
#include <Arduino.h>
#include "config.h"

void motor_init();

// pwm con signo en [-PWM_MAX, PWM_MAX]. 0 => salida apagada (coast).
void motor_set(uint8_t axis, int pwm);

// Salida desconectada: el eje gira libre a mano (CAL, falla).
void motor_coast(uint8_t axis);
