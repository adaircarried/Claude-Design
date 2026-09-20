#include "supervisor.h"
#include "config.h"
#include "kinematics.h"
#include "motion.h"
#include "telemetry.h"

static volatile RobotState_t g_state   = STATE_BOOT;
static volatile FaultCode_t  g_fault   = FAULT_NONE;
static volatile bool         g_homed   = false;
static volatile bool         g_abort   = false;
static volatile bool         g_drv_on  = false;

RobotState_t sv_get_state(void)        { return g_state; }
void         sv_set_state(RobotState_t s) { g_state = s; }
FaultCode_t  sv_get_fault(void)        { return g_fault; }
void         sv_clear_fault(void)      { g_fault = FAULT_NONE; }
bool         sv_is_homed(void)         { return g_homed; }
void         sv_set_homed(bool h)      { g_homed = h; }
bool         sv_abort_requested(void)  { return g_abort; }
void         sv_clear_abort(void)      { g_abort = false; }
bool         sv_drivers_enabled(void)  { return g_drv_on; }

void sv_set_fault(FaultCode_t f)
{
    g_fault = f;
    if (f != FAULT_NONE) g_state = STATE_FAULT;
}

void sv_request_abort(FaultCode_t reason)
{
    g_abort = true;
    /* Vaciar la cola es tan importante como levantar la bandera: si quedaran
     * setpoints pendientes, TaskMotion los ejecutaria en cuanto terminara de
     * abortar el movimiento actual, y el robot volveria a arrancar solo. */
    if (setpointQueue) xQueueReset(setpointQueue);
    if (reason != FAULT_NONE) g_fault = reason;
}

void sv_drivers_enable(bool on)
{
    g_drv_on = on;
    digitalWrite(PIN_ENABLE, on ? ENABLE_ACTIVE_LEVEL : !ENABLE_ACTIVE_LEVEL);
}

/* ---------------------------------------------------------------------------
 * Lectura de finales de carrera.
 * sv_limit_raw()       -> una lectura cruda
 * sv_limit_triggered() -> N lecturas consecutivas iguales (anti-rebote)
 * -------------------------------------------------------------------------*/
bool sv_limit_raw(uint8_t axis)
{
    const int pin = (axis == 1) ? PIN_LIMIT_A1 : PIN_LIMIT_A2;
    return digitalRead(pin) == LIMIT_ACTIVE_LEVEL;
}

bool sv_limit_triggered(uint8_t axis)
{
    for (int i = 0; i < HOMING_DEBOUNCE_READS; ++i) {
        if (!sv_limit_raw(axis)) return false;
        delayMicroseconds(400);
    }
    return true;
}

void supervisor_init(void)
{
    pinMode(PIN_ENABLE, OUTPUT);
    sv_drivers_enable(false);               /* arranca con los motores sueltos */

    pinMode(PIN_LIMIT_A1, INPUT_PULLUP);
    pinMode(PIN_LIMIT_A2, INPUT_PULLUP);

    g_state = STATE_BOOT;
    g_fault = FAULT_NONE;
}

/* ===========================================================================
 * TaskSupervisor  -  nucleo 1, prioridad 6, periodo 50 ms
 * ===========================================================================
 * Usa vTaskDelayUntil y no vTaskDelay: el periodo se mide desde el instante
 * de despertar anterior, no desde el final del trabajo, asi que el periodo
 * real no se va desplazando cuando una iteracion tarda mas de lo normal.
 *
 * Ceder CPU aqui NO es opcional. Esta tarea tiene mayor prioridad que
 * TaskMotion y que la tarea interna de FastAccelStepper; si no cediera, la
 * tarea idle del nucleo 1 nunca correria y el watchdog reiniciaria el ESP32.
 * -------------------------------------------------------------------------*/
void TaskSupervisor(void *pv)
{
    (void)pv;
    TickType_t last = xTaskGetTickCount();

    for (;;) {
        const RobotState_t st = sv_get_state();

        /* Durante el homing los finales de carrera SE PISAN a proposito y la
         * posicion todavia no significa nada, asi que estas comprobaciones se
         * saltan. Es el unico estado en que se saltan. */
        if (st != STATE_HOMING && st != STATE_BOOT && st != STATE_DISABLED) {

            /* 1) Final de carrera pisado fuera del homing = el eje llego a un
             *    tope que no deberia haber alcanzado. Se para de inmediato. */
            if (sv_limit_raw(1) || sv_limit_raw(2)) {
                if (!sv_abort_requested()) {
                    sv_request_abort(FAULT_LIMIT_HIT);
                    report_line("EVT LIMIT_HIT");
                }
            }

            /* 2) Limites articulares y de espacio de trabajo por software.
             *    Solo tienen sentido si el robot esta referenciado. */
            if (g_homed) {
                float q1, q2;
                motion_get_joints(&q1, &q2);

                if (!kin_joints_in_limits(q1, q2)) {
                    if (!sv_abort_requested()) {
                        sv_request_abort(FAULT_SOFT_LIMIT);
                        report_printf("EVT SOFT_LIMIT q1=%.2f q2=%.2f\n", q1, q2);
                    }
                } else {
                    const Point_t p = kin_forward(q1, q2);
                    if (kin_point_in_workspace(p.x, p.y) != IK_OK) {
                        if (!sv_abort_requested()) {
                            sv_request_abort(FAULT_WORKSPACE);
                            report_printf("EVT WORKSPACE x=%.1f y=%.1f\n", p.x, p.y);
                        }
                    }
                }
            }
        }

        vTaskDelayUntil(&last, pdMS_TO_TICKS(SUPERVISOR_PERIOD_MS));
    }
}
