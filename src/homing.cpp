#include "homing.h"
#include "config.h"
#include "motion.h"
#include "kinematics.h"
#include "supervisor.h"
#include "telemetry.h"

const char *homing_result_name(HomingResult_t r)
{
    switch (r) {
    case HOMING_OK:      return "OK";
    case HOMING_TIMEOUT: return "TIMEOUT";
    case HOMING_ABORTED: return "ABORTED";
    case HOMING_STUCK:   return "STUCK";
    }
    return "?";
}

/* ---------------------------------------------------------------------------
 * Una pasada de aproximacion al final de carrera.
 *
 * Se lanza un movimiento relativo de longitud HOMING_MAX_TRAVEL_DEG en vez de
 * un runForward() indefinido. Ese limite de recorrido es la red de seguridad:
 * si el switch esta mal cableado o el cable se solto, el eje recorre su
 * maximo, el movimiento termina solo y se reporta TIMEOUT. Sin el, el eje
 * empujaria contra el tope mecanico indefinidamente hasta romper la correa,
 * el reductor o el propio switch.
 *
 * Latencia del muestreo: se consulta el switch cada HOMING_POLL_MS (2 ms). A
 * la velocidad lenta (150 Hz) eso son 0.3 micropasos de incertidumbre, o sea
 * 0.007 grados: dos ordenes de magnitud por debajo del backlash. Por eso la
 * pasada que fija el cero es la lenta y no la rapida.
 * -------------------------------------------------------------------------*/
static bool approach(FastAccelStepper *st, uint8_t axis, int dir,
                     uint32_t hz, int32_t *trigger_pos)
{
    const int32_t max_steps = kin_deg_to_steps(HOMING_MAX_TRAVEL_DEG);

    st->setSpeedInHz(hz);
    st->setAcceleration(HOMING_ACCEL);
    st->move(dir * max_steps);

    for (;;) {
        if (sv_limit_triggered(axis)) {
            if (trigger_pos) *trigger_pos = st->getCurrentPosition();
            /* forceStop() es un corte seco, sin rampa. Aqui es correcto
             * precisamente porque la velocidad es baja: la inercia no alcanza
             * para arrastrar el rotor, y parar en el instante del disparo es
             * lo que hace repetible el cero. A velocidad alta esto SI
             * perderia pasos, y por eso la pasada rapida nunca fija el cero. */
            st->forceStop();
            return true;
        }
        if (sv_abort_requested()) { st->forceStop(); return false; }
        if (!st->isRunning())     { return false; }   /* recorrio todo: timeout */
        vTaskDelay(pdMS_TO_TICKS(HOMING_POLL_MS));
    }
}

static bool move_blocking(FastAccelStepper *st, int32_t delta, uint32_t hz)
{
    st->setSpeedInHz(hz);
    st->setAcceleration(HOMING_ACCEL);
    st->move(delta);
    while (st->isRunning()) {
        if (sv_abort_requested()) { st->forceStop(); return false; }
        vTaskDelay(pdMS_TO_TICKS(HOMING_POLL_MS));
    }
    return true;
}

HomingResult_t homing_axis(uint8_t axis, bool set_zero, int32_t *measured_steps)
{
    FastAccelStepper *st = motion_stepper(axis);
    if (!st) return HOMING_TIMEOUT;

    const int   dir      = (axis == 1) ? A1_HOMING_DIR : A2_HOMING_DIR;
    const float home_deg = (axis == 1) ? A1_HOME_DEG   : A2_HOME_DEG;
    const int32_t backoff = kin_deg_to_steps(HOMING_BACKOFF_DEG);

    sv_set_state(STATE_HOMING);

    /* Paso 0: si el eje ya arranca con el switch pisado, primero hay que
     * salirse. Si no, la primera aproximacion lo daria por encontrado en el
     * mismo sitio donde ya estaba y el cero saldria desplazado. */
    if (sv_limit_raw(axis)) {
        if (!move_blocking(st, -dir * backoff * 2, HOMING_SLOW_HZ))
            return HOMING_ABORTED;
        if (sv_limit_raw(axis))
            return HOMING_STUCK;     /* no se libera: switch pegado o mal cableado */
    }

    /* Paso 1: aproximacion RAPIDA, solo para localizar el switch. */
    if (!approach(st, axis, dir, HOMING_FAST_HZ, NULL)) {
        if (sv_abort_requested()) return HOMING_ABORTED;
        return HOMING_TIMEOUT;
    }

    /* Paso 2: retroceso para liberar el contacto. */
    if (!move_blocking(st, -dir * backoff, HOMING_SLOW_HZ)) return HOMING_ABORTED;
    if (sv_limit_raw(axis)) return HOMING_STUCK;

    /* Paso 3: aproximacion LENTA. Esta es la que vale. */
    int32_t trig = 0;
    if (!approach(st, axis, dir, HOMING_SLOW_HZ, &trig)) {
        if (sv_abort_requested()) return HOMING_ABORTED;
        return HOMING_TIMEOUT;
    }

    if (set_zero) {
        /* Aqui nace el sistema de coordenadas del robot. */
        st->setCurrentPosition(kin_deg_to_steps(home_deg));
    } else {
        /* MODO MEDICION (comando TEST): no se corrige nada. Lo que interesa
         * es cuanto se ha desviado el contador respecto de la realidad. */
        if (measured_steps) *measured_steps = trig;
    }

    /* Paso 4: salir del switch. Dejarlo pisado haria que TaskSupervisor lo
     * interpretara como un choque en cuanto termine el homing. */
    if (!move_blocking(st, -dir * backoff, HOMING_SLOW_HZ)) return HOMING_ABORTED;

    return HOMING_OK;
}

HomingResult_t homing_all(void)
{
    report_line("EVT HOMING_START");

    /* ORDEN: el codo (eje 2) PRIMERO. Al plegarse, el radio del efector se
     * reduce antes de que el hombro empiece a barrer el espacio de trabajo,
     * asi que el brazo no se lleva por delante el tablero ni la mesa. */
    HomingResult_t r = homing_axis(2, true, NULL);
    if (r != HOMING_OK) return r;

    r = homing_axis(1, true, NULL);
    if (r != HOMING_OK) return r;

    sv_clear_fault();
    return HOMING_OK;
}
