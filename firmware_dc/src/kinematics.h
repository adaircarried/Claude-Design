// =============================================================================
//  kinematics.h — Cinemática del manipulador planar RR
//  Ángulos en GRADOS en la interfaz (lo que usa el resto del firmware),
//  distancias en mm. Internamente se trabaja en radianes.
// =============================================================================
#pragma once
#include <Arduino.h>

void kin_forward(float q1_deg, float q2_deg, float &x, float &y);

// Devuelve false si (x, y) está fuera del anillo de trabajo o la solución
// viola los límites articulares. elbow_up elige el signo de q2.
bool kin_inverse(float x, float y, bool elbow_up, float &q1_deg, float &q2_deg);

bool kin_joints_valid(float q1_deg, float q2_deg);
bool kin_in_workspace(float x, float y);

// Mensaje del último motivo de rechazo (para la consola).
const char *kin_last_error();
