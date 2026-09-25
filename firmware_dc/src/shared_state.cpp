#include "shared_state.h"

SharedState       g_state;
SemaphoreHandle_t g_stateMutex = nullptr;

void shared_state_init() {
    // Mutex (no semáforo binario): tiene herencia de prioridad, así una tarea
    // de baja prioridad que lo tenga no puede retrasar indefinidamente a
    // TaskControl.
    g_stateMutex = xSemaphoreCreateMutex();
    memset(&g_state, 0, sizeof(g_state));
    g_state.cal_axis  = -1;
    g_state.ol_axis   = -1;
    g_state.pwm_limit = PWM_LIMIT_DEFAULT;
    g_state.accel     = ACCEL_DEFAULT_DEG_S2;
    g_state.speed_pct = SPEED_PCT_DEFAULT;
    for (uint8_t i = 0; i < NUM_AXES; i++) {
        g_state.cpr[i] = DEFAULT_CPR[i];
        g_state.gains[i] = {PID_KP_DEFAULT, PID_KI_DEFAULT, PID_KD_DEFAULT,
                            PID_KFF_DEFAULT, PID_DEADBAND_DEFAULT, PID_PWM_MIN_DEFAULT};
    }
}
