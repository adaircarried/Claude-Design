// =============================================================================
//  setpoint.h — CONTRATO entre quien da órdenes y quien mueve el robot
// =============================================================================
//  Esta es la única interfaz que un productor de órdenes necesita conocer.
//  Hoy la llena la consola de texto (comms.cpp). En el siguiente parcial la
//  llenará un parser de protocolo binario: bastará con que ese parser arme un
//  Setpoint_t y llame a motion_submit(). TaskMotion y TaskControl no cambian.
// =============================================================================
#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

typedef enum {
    MOTION_JOG,      // un eje a la vez, velocidad reducida, INCREMENTAL (estilo FANUC)
    MOTION_MOVJ,     // ambos ejes coordinados, llegan juntos (absoluto, grados)
    MOTION_MOVL,     // interpolación lineal en cartesiano (absoluto, mm)
    MOTION_HOME,     // fija el cero en la posición actual
    MOTION_STOP,     // parada con desaceleración (se adelanta a la cola)
    MOTION_JOGC      // jog cartesiano incremental (target_a = dX mm, target_b = dY mm)
} MotionType_t;

typedef struct {
    MotionType_t type;
    float        target_a;    // q1 en grados, o X en mm si es MOVL
    float        target_b;    // q2 en grados, o Y en mm si es MOVL
    uint16_t     speed_pct;   // 1 a 100
    uint8_t      axis_id;     // solo para JOG (0 = J1, 1 = J2)
} Setpoint_t;

extern QueueHandle_t setpointQueue;

// Encola una orden. STOP se inserta al frente para que no espere turno.
// Devuelve false si la cola está llena. Seguro de llamar desde cualquier tarea.
bool motion_submit(const Setpoint_t &sp);
