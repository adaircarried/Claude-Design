// =============================================================================
//  telemetry.cpp
// =============================================================================
//  Formato "etiqueta:valor" separado por comas, una línea cada 100 ms:
//    r1:10.00,q1:9.87,e1:0.13,pwm1:230,r2:...,q2:...,e2:...,pwm2:...
//  El Serial Plotter de Arduino IDE 2 lo grafica directamente (cada etiqueta
//  es una serie) y en un monitor serie normal se sigue leyendo bien.
//  Es la prioridad más baja del sistema: si algo tiene que ceder, es esto.
// =============================================================================
#include "telemetry.h"
#include "config.h"
#include "shared_state.h"
#include "console.h"

static volatile bool s_on = false;

void telemetry_enable(bool on) { s_on = on; }
bool telemetry_enabled()       { return s_on; }

void task_telemetry(void *arg) {
    const TickType_t period = pdMS_TO_TICKS(TELEMETRY_PERIOD_MS);
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&last_wake, period);
        if (!s_on) continue;
        AxisStatus st[NUM_AXES];
        uint8_t    flt = 0;
        state_lock();
        memcpy(st, g_state.st, sizeof(st));
        for (uint8_t i = 0; i < NUM_AXES; i++) flt |= (g_state.fault[i] != FAULT_NONE) << i;
        state_unlock();
        con_printf("r1:%.2f,q1:%.2f,e1:%.2f,pwm1:%d,r2:%.2f,q2:%.2f,e2:%.2f,pwm2:%d,fault:%u\n",
                   st[0].pos_ref, st[0].pos, st[0].err, st[0].pwm,
                   st[1].pos_ref, st[1].pos, st[1].err, st[1].pwm, flt);
    }
}
