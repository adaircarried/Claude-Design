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

    /* Memoria de flanco del final de carrera. Sin esto, una condicion que
     * persiste (por ejemplo un switch sin conectar) se re-dispara en cada
     * periodo y llena la consola de eventos identicos. */
    bool limit_prev = false;

    /* -------------------------------------------------------------------
     * Diagnostico de arranque.
     * Que los DOS finales lean "pisado" nada mas encender casi nunca
     * significa que ambos ejes esten contra su tope a la vez: significa que
     * los switches no estan conectados. Con el pull-up interno y nada
     * enchufado, el pin flota a alto, y alto = pisado en el convenio NC.
     * Avisar aqui ahorra un buen rato de desconcierto.
     * ------------------------------------------------------------------ */
    vTaskDelay(pdMS_TO_TICKS(250));          /* deja terminar el banner */
    if (sv_limit_raw(1) && sv_limit_raw(2)) {
        report_line("AVISO: los dos finales de carrera leen PISADO.");
        report_line("       Si aun no los has conectado, es lo esperado:");
        report_line("       contacto NC + pull-up interno = alto = pisado.");
        report_line("       Para probar en banco, puentea GPIO 16 y GPIO 17 a GND.");
    }

    for (;;) {
        const RobotState_t st = sv_get_state();

        /* -----------------------------------------------------------------
         * CUANDO VIGILAR
         * -----------------------------------------------------------------
         * El supervisor solo actua si el robot PUEDE moverse y no hay ya un
         * fallo pendiente de atender:
         *
         *   drivers energizados : si estan libres no hay par y nada se mueve,
         *                         asi que no hay nada que detener. Ademas es
         *                         el estado de arranque, cuando los finales
         *                         pueden no estar ni conectados todavia.
         *   sin fallo activo    : el fallo queda LATCHEADO hasta que el
         *                         usuario escriba CLEAR. Se comprueba el
         *                         codigo de fallo y no el estado porque
         *                         sv_request_abort() lo fija al instante,
         *                         mientras que la transicion a STATE_FAULT
         *                         tarda unos ticks en completarse.
         *   fuera del homing    : durante el referenciado los finales SE
         *                         PISAN a proposito.
         * --------------------------------------------------------------- */
        const bool watch = sv_drivers_enabled()
                        && sv_get_fault() == FAULT_NONE
                        && st != STATE_HOMING
                        && st != STATE_BOOT;

        const bool limit_now = sv_limit_raw(1) || sv_limit_raw(2);

        if (watch) {

            /* 1) Final de carrera pisado fuera del homing = el eje llego a un
             *    tope que no deberia haber alcanzado. Solo en el FLANCO: nos
             *    interesa el instante en que aparece la condicion, no que
             *    siga presente. */
            if (limit_now && !limit_prev) {
                sv_request_abort(FAULT_LIMIT_HIT);
                report_printf("EVT LIMIT_HIT a1=%s a2=%s\n",
                              sv_limit_raw(1) ? "PISADO" : "libre",
                              sv_limit_raw(2) ? "PISADO" : "libre");
            }
            limit_prev = limit_now;

            /* 2) Limites articulares y de espacio de trabajo por software.
             *    Solo tienen sentido si el robot esta referenciado. No hace
             *    falta proteger el disparo repetido: al fijar el fallo, la
             *    condicion "watch" se cierra en el siguiente periodo. */
            if (g_homed) {
                float q1, q2;
                motion_get_joints(&q1, &q2);

                if (!kin_joints_in_limits(q1, q2)) {
                    sv_request_abort(FAULT_SOFT_LIMIT);
                    report_printf("EVT SOFT_LIMIT q1=%.2f q2=%.2f\n", q1, q2);
                } else {
                    const Point_t p = kin_forward(q1, q2);
                    if (kin_point_in_workspace(p.x, p.y) != IK_OK) {
                        sv_request_abort(FAULT_WORKSPACE);
                        report_printf("EVT WORKSPACE x=%.1f y=%.1f\n", p.x, p.y);
                    }
                }
            }
        } else {
            /* Mientras no se vigila, se olvida el flanco anterior. Asi, al
             * volver a vigilar (un ENABLE, o un CLEAR tras un fallo), la
             * condicion se re-evalua desde cero: si el final sigue pisado,
             * vuelve a disparar de inmediato en vez de quedarse callado. */
            limit_prev = false;
        }

        vTaskDelayUntil(&last, pdMS_TO_TICKS(SUPERVISOR_PERIOD_MS));
    }
}
