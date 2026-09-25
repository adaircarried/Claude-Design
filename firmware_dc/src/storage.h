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
void storage_clear();
