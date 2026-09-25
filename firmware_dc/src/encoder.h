// =============================================================================
//  encoder.h — Lectura de encoders por PCNT (hardware), sin interrupciones
//              por flanco. Solo TaskControl llama a estas funciones.
// =============================================================================
#pragma once
#include <Arduino.h>
#include "config.h"

void    encoder_init();

// Debe llamarse una vez por ciclo de control. dir_hint = signo del último PWM
// aplicado (+1 / -1); solo se usa en ejes ENC_SINGLE para asignar dirección.
void    encoder_update(uint8_t axis, int8_t dir_hint);

// Cuentas acumuladas con signo (ya invertidas según ENC_INVERT).
int64_t encoder_count(uint8_t axis);

// Pone el conteo en cero (HOME / CAL).
void    encoder_zero(uint8_t axis);
