/* =============================================================================
 * comms.h  -  TaskComms: la unica fuente de setpoints del Parcial 1
 * =============================================================================
 *
 * ESTE ES EL ARCHIVO QUE SE REESCRIBE EN EL PARCIAL 2.
 *
 * Toda la logica de "como llegan las ordenes" esta confinada aqui. El parser
 * binario con CRC del Parcial 2 reemplaza el parser de texto de comms.cpp y
 * NADA MAS del sistema cambia: ni motion, ni kinematics, ni homing, ni
 * supervisor, ni telemetry. Ese es el objetivo de toda la arquitectura.
 *
 * El buffer circular de recepcion ya esta implementado y en uso, porque el
 * Parcial 2 lo exige explicitamente ("parseo en tiempo real con buffer
 * circular, sin latencia perceptible") y porque asi la ruta de recepcion ya
 * queda probada con trafico real desde ahora.
 * ---------------------------------------------------------------------------*/

#pragma once
#include <Arduino.h>

void comms_init(void);
void TaskComms(void *pv);
