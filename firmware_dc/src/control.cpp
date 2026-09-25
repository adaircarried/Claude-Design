// =============================================================================
//  control.cpp — TaskControl
// =============================================================================
//  Ciclo fijo de 5 ms con vTaskDelayUntil (periodo constante aunque el cuerpo
//  tarde distinto en cada iteración; vTaskDelay acumularía deriva).
//
//  Cada ciclo:
//    1. Lee encoders (registro PCNT, no bloquea).
//    2. Intercambia datos con las demás tareas bajo mutex (timeout 1 tick; si
//       no lo obtiene sigue con los últimos valores: jamás se queda esperando).
//    3. Ejecuta el PID de cada eje y la detección de fallas.
//    4. Aplica PWM (LEDC) y publica el estado.
//  Prohibido Serial.print aquí: una escritura a UART puede bloquear varios ms.
// =============================================================================
#include "control.h"
#include "config.h"
#include "shared_state.h"
#include "encoder.h"
#include "motor.h"
#include "pid.h"

const char *fault_name(uint8_t code) {
    switch (code) {
        case FAULT_NONE:   return "OK";
        case FAULT_STALL:  return "BLOQUEO";
        case FAULT_FOLLOW: return "ERROR_SEGUIMIENTO";
        default:           return "?";
    }
}

void task_control(void *arg) {
    PID       pid[NUM_AXES];
    float     cpr[NUM_AXES];
    float     pos[NUM_AXES], prev_pos[NUM_AXES], vel[NUM_AXES] = {0, 0};
    float     ref[NUM_AXES], vel_ref[NUM_AXES] = {0, 0};
    int       pwm[NUM_AXES] = {0, 0};
    int8_t    dir[NUM_AXES] = {1, 1};
    uint32_t  stall_ms[NUM_AXES] = {0, 0}, follow_ms[NUM_AXES] = {0, 0};
    FaultCode fault[NUM_AXES] = {FAULT_NONE, FAULT_NONE};
    int       pwm_limit = PWM_LIMIT_DEFAULT;
    int8_t    cal_axis = -1;
    int8_t    ol_axis = -1;
    int       ol_pwm = 0;
    uint32_t  last_seq = 0;
    uint8_t   cycles_since_cmd = 0;
    uint32_t  cycles = 0, overruns = 0;

    // Estado inicial: la referencia es la posición actual (no hay salto).
    state_lock();
    for (uint8_t i = 0; i < NUM_AXES; i++) {
        cpr[i] = g_state.cpr[i];
        pos[i] = prev_pos[i] = ref[i] = encoder_count(i) * 360.0f / cpr[i];
        pid[i].configure(g_state.gains[i], pwm_limit);
        pid[i].reset(pos[i]);
        g_state.cmd[i].pos_ref = ref[i];
        g_state.cmd[i].vel_ref = 0;
    }
    last_seq = g_state.cmd_seq;
    state_unlock();

    const TickType_t period = pdMS_TO_TICKS(CONTROL_PERIOD_MS);
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&last_wake, period);
        if ((xTaskGetTickCount() - last_wake) >= period) overruns++;
        cycles++;

        // ---- 1. Lectura de encoders ---------------------------------------
        for (uint8_t i = 0; i < NUM_AXES; i++) {
            encoder_update(i, dir[i]);
            pos[i] = encoder_count(i) * 360.0f / cpr[i];
        }

        // ---- 2. Intercambio con las demás tareas --------------------------
        bool new_cmd = false;
        if (state_lock(1)) {
            // Parámetros
            for (uint8_t i = 0; i < NUM_AXES; i++) {
                if (g_state.cpr[i] != cpr[i]) {
                    cpr[i] = g_state.cpr[i];
                    pos[i] = encoder_count(i) * 360.0f / cpr[i];
                }
                if (g_state.req_gains[i]) {
                    pid[i].setGains(g_state.gains[i]);
                    g_state.req_gains[i] = false;
                }
            }
            if (g_state.pwm_limit != pwm_limit) {
                pwm_limit = g_state.pwm_limit;
                for (uint8_t i = 0; i < NUM_AXES; i++) pid[i].setOutputLimit(pwm_limit);
            }

            // Calibración
            cal_axis = g_state.cal_axis;
            ol_axis  = g_state.ol_axis;
            ol_pwm   = g_state.ol_pwm;
            if (g_state.req_cal_zero && cal_axis >= 0) {
                encoder_zero(cal_axis);
                pos[cal_axis] = 0;
                g_state.req_cal_zero = false;
            }

            // HOME: cero en la posición actual
            if (g_state.req_zero_mask) {
                for (uint8_t i = 0; i < NUM_AXES; i++) {
                    if (!(g_state.req_zero_mask & (1u << i))) continue;
                    encoder_zero(i);
                    pos[i] = prev_pos[i] = ref[i] = 0;
                    vel_ref[i] = 0;
                    pid[i].reset(0);
                    g_state.cmd[i].pos_ref = 0;
                    g_state.cmd[i].vel_ref = 0;
                }
                g_state.req_zero_mask = 0;
                g_state.cmd_seq++;
            }

            // Reset de falla / re-sincronización: la referencia pasa a ser la
            // posición real para que el eje no salte al rehabilitarse.
            if (g_state.req_fault_reset) {
                for (uint8_t i = 0; i < NUM_AXES; i++) {
                    fault[i] = FAULT_NONE;
                    stall_ms[i] = follow_ms[i] = 0;
                    ref[i] = pos[i];
                    vel_ref[i] = 0;
                    pid[i].reset(pos[i]);
                    g_state.cmd[i].pos_ref = pos[i];
                    g_state.cmd[i].vel_ref = 0;
                }
                g_state.cmd_seq++;
                g_state.req_fault_reset = false;
            }

            // Referencia de TaskMotion
            if (g_state.cmd_seq != last_seq) {
                last_seq = g_state.cmd_seq;
                for (uint8_t i = 0; i < NUM_AXES; i++) {
                    ref[i]     = g_state.cmd[i].pos_ref;
                    vel_ref[i] = g_state.cmd[i].vel_ref;
                }
                new_cmd = true;
            }
            state_unlock();
        }

        // TaskMotion actualiza cada 10 ms y el PID corre cada 5 ms: entre dos
        // referencias se extrapola con la velocidad para que el setpoint no
        // avance en escalones. Si Motion deja de publicar, se congela.
        if (new_cmd) {
            cycles_since_cmd = 0;
        } else if (cycles_since_cmd < 2) {
            cycles_since_cmd++;
            for (uint8_t i = 0; i < NUM_AXES; i++) ref[i] += vel_ref[i] * CONTROL_DT;
        } else {
            for (uint8_t i = 0; i < NUM_AXES; i++) vel_ref[i] = 0;
        }

        // ---- 3. PID + diagnóstico -----------------------------------------
        bool any_fault = false;
        for (uint8_t i = 0; i < NUM_AXES; i++) {
            float v = (pos[i] - prev_pos[i]) / CONTROL_DT;
            vel[i] += 0.1f * (v - vel[i]);            // velocidad filtrada
            prev_pos[i] = pos[i];
            if (fault[i] != FAULT_NONE) any_fault = true;
        }

        for (uint8_t i = 0; i < NUM_AXES; i++) {
            if (any_fault || cal_axis == (int8_t)i) {
                // Sin par: el eje queda libre (falla o calibración a mano)
                pwm[i] = 0;
                motor_coast(i);
                pid[i].reset(pos[i]);
                if (cal_axis == (int8_t)i) ref[i] = pos[i];
                continue;
            }

            if (ol_axis == (int8_t)i) {
                // Lazo abierto (FRIC): PWM fijo, referencia sigue a la medición
                pwm[i] = constrain(ol_pwm, -pwm_limit, pwm_limit);
                ref[i] = pos[i];
                pid[i].reset(pos[i]);
            } else {
                pwm[i] = pid[i].update(ref[i], vel_ref[i], pos[i], CONTROL_DT);
            }

            // Motor bloqueado: mucho PWM y el eje no se mueve
            if (abs(pwm[i]) > (int)(STALL_PWM_FRAC * pwm_limit) &&
                fabsf(vel[i]) < STALL_VEL_DEG_S) {
                stall_ms[i] += CONTROL_PERIOD_MS;
                if (stall_ms[i] >= STALL_TIME_MS) fault[i] = FAULT_STALL;
            } else {
                stall_ms[i] = 0;
            }
            // Error de seguimiento excesivo
            if (fabsf(ref[i] - pos[i]) > FOLLOW_ERR_DEG) {
                follow_ms[i] += CONTROL_PERIOD_MS;
                if (follow_ms[i] >= FOLLOW_TIME_MS) fault[i] = FAULT_FOLLOW;
            } else {
                follow_ms[i] = 0;
            }

            if (fault[i] != FAULT_NONE) {
                // Corta ambos ejes en el mismo ciclo
                for (uint8_t k = 0; k < NUM_AXES; k++) { pwm[k] = 0; motor_coast(k); }
                any_fault = true;
                break;
            }
            motor_set(i, pwm[i]);
            if (pwm[i] > 0) dir[i] = 1;
            else if (pwm[i] < 0) dir[i] = -1;
        }

        // ---- 4. Publicar estado -------------------------------------------
        if (state_lock(1)) {
            for (uint8_t i = 0; i < NUM_AXES; i++) {
                AxisStatus &s = g_state.st[i];
                s.pos_ref = ref[i];
                s.pos     = pos[i];
                s.err     = ref[i] - pos[i];
                s.vel     = vel[i];
                s.pwm     = (int16_t)pwm[i];
                s.counts  = encoder_count(i);
                g_state.fault[i] = fault[i];
            }
            g_state.control_cycles   = cycles;
            g_state.control_overruns = overruns;
            state_unlock();
        }
    }
}
