// =============================================================================
//  storage.h — Persistencia en NVS (librería Preferences)
// =============================================================================
#pragma once
#include "config.h"
#include "pid.h"

// Carga lo guardado sobre los valores recibidos (si no hay nada, no los toca).
void storage_load(float cpr[NUM_AXES], PIDGains gains[NUM_AXES]);
void storage_save_cpr(uint8_t axis, float cpr);
void storage_save_gains(const PIDGains gains[NUM_AXES]);
// LIMIT, SPEED y ACCEL (se guardan con SAVE)
void storage_load_motion(int &pwm_limit, uint16_t &speed_pct, float &accel);
void storage_save_motion(int pwm_limit, uint16_t speed_pct, float accel);
void storage_clear();
