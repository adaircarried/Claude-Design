#include "motion.h"
#include "config.h"
#include "kinematics.h"
#include "supervisor.h"
#include "homing.h"
#include "persistence.h"
#include "telemetry.h"
#include <math.h>

/* ===========================================================================
 * GENERACION DE PASOS POR HARDWARE
 * ===========================================================================
 * FastAccelStepper usa los perifericos RMT/MCPWM del ESP32 para generar los
 * pulsos. NO se generan desde una tarea de FreeRTOS, y la razon es concreta:
 * el tick del planificador es de 1 ms, asi que lo mas fino que puede hacer
 * una tarea con vTaskDelay son 1000 pulsos por segundo con un jitter del
 * orden del propio tick. A 2100 pasos/s eso produce movimiento vibrante y,
 * bajo carga, perdida de pasos. El hardware genera los pulsos con precision
 * de nanosegundos e independiente de lo que hagan las tareas.
 * -------------------------------------------------------------------------*/
static FastAccelStepperEngine engine = FastAccelStepperEngine();
static FastAccelStepper *g_a1 = NULL;
static FastAccelStepper *g_a2 = NULL;

QueueHandle_t setpointQueue = NULL;

static uint16_t      g_speed_pct = 50;
static ElbowConfig_t g_elbow     = ELBOW_DOWN;

/* --- Maquina de estados de ejecucion ------------------------------------ */
typedef enum {
    EXEC_IDLE = 0,   /* esperando un setpoint                               */
    EXEC_POINT,      /* JOG o MOVJ en curso: solo hay que esperar           */
    EXEC_LINE,       /* MOVL en curso: hay que alimentar el target cada tick*/
    EXEC_STOPPING    /* frenando tras un abort                              */
} ExecState_t;

static ExecState_t g_exec = EXEC_IDLE;

/* Estado del interpolador lineal (MOVL) */
static Point_t  g_line_a, g_line_b;
static float    g_line_len   = 0.0f;   /* mm    */
static float    g_line_s     = 0.0f;   /* 0..1  */
static float    g_line_feed  = 0.0f;   /* mm/s  */
static uint32_t g_line_t_ms  = 0;

/* ------------------------------------------------------------------------ */
FastAccelStepper *motion_stepper(uint8_t axis) { return (axis == 1) ? g_a1 : g_a2; }
uint16_t motion_get_speed_pct(void)            { return g_speed_pct; }
ElbowConfig_t motion_get_elbow(void)           { return g_elbow; }
void motion_set_elbow(ElbowConfig_t e)         { g_elbow = e; }
bool motion_is_busy(void)                      { return g_exec != EXEC_IDLE; }

void motion_set_speed_pct(uint16_t pct)
{
    if (pct < 1)   pct = 1;
    if (pct > 100) pct = 100;
    g_speed_pct = pct;
}

void motion_get_steps(int32_t *s1, int32_t *s2)
{
    if (s1) *s1 = g_a1 ? g_a1->getCurrentPosition() : 0;
    if (s2) *s2 = g_a2 ? g_a2->getCurrentPosition() : 0;
}

void motion_get_joints(float *q1, float *q2)
{
    int32_t a, b;
    motion_get_steps(&a, &b);
    if (q1) *q1 = kin_steps_to_deg(a);
    if (q2) *q2 = kin_steps_to_deg(b);
}

static uint32_t scale_speed(uint32_t base_hz, uint16_t pct)
{
    uint32_t v = (uint32_t)((uint64_t)base_hz * pct / 100U);
    if (v < AXIS_MIN_SPEED_HZ) v = AXIS_MIN_SPEED_HZ;
    return v;
}

/* ===========================================================================
 * MOVJ  -  INTERPOLACION ARTICULAR SINCRONIZADA
 * ===========================================================================
 * Los dos ejes deben ARRANCAR Y TERMINAR AL MISMO TIEMPO. El eje que recorre
 * menos se mueve mas lento.
 *
 * La sincronizacion se consigue escalando velocidad Y aceleracion por el
 * mismo factor k = d_eje / d_mayor. Con eso los tiempos coinciden EXACTAMENTE
 * en los dos regimenes posibles de un perfil trapezoidal:
 *
 *   caso trapezoidal (alcanza v_max):
 *       t = d/v + v/a  ->  (k*d)/(k*v) + (k*v)/(k*a) = d/v + v/a    IGUAL
 *
 *   caso triangular (no alcanza v_max, movimiento corto):
 *       t = 2*sqrt(d/a) -> 2*sqrt(k*d / k*a) = 2*sqrt(d/a)          IGUAL
 *
 * Escalar solo la velocidad (el error tipico) sincroniza el tramo de crucero
 * pero no las rampas, y los ejes llegan desfasados en movimientos cortos.
 *
 * La trayectoria del efector NO es una recta, y eso es correcto: es
 * precisamente lo que distingue MOVJ de MOVL.
 * -------------------------------------------------------------------------*/
static void start_movj_steps(int32_t t1, int32_t t2, uint16_t pct)
{
    const int32_t c1 = g_a1->getCurrentPosition();
    const int32_t c2 = g_a2->getCurrentPosition();

    const int32_t d1 = labs(t1 - c1);
    const int32_t d2 = labs(t2 - c2);
    const int32_t dm = (d1 > d2) ? d1 : d2;

    if (dm == 0) { g_exec = EXEC_IDLE; return; }

    const uint32_t vmax = scale_speed(AXIS_MAX_SPEED_HZ, pct);
    const uint32_t amax = AXIS_MAX_ACCEL;

    const float k1 = (float)d1 / (float)dm;
    const float k2 = (float)d2 / (float)dm;

    /* Piso de 1 Hz / 1 paso-s^-2: FastAccelStepper rechaza el valor 0, y un
     * eje con recorrido cero igual necesita parametros validos. */
    g_a1->setSpeedInHz((uint32_t)fmaxf(1.0f, vmax * k1));
    g_a1->setAcceleration((int32_t)fmaxf(1.0f, amax * k1));
    g_a2->setSpeedInHz((uint32_t)fmaxf(1.0f, vmax * k2));
    g_a2->setAcceleration((int32_t)fmaxf(1.0f, amax * k2));

    g_a1->moveTo(t1);
    g_a2->moveTo(t2);

    g_exec = EXEC_POINT;
    sv_set_state(STATE_MOVING);
}

/* ===========================================================================
 * MOVL  -  INTERPOLACION LINEAL EN CARTESIANO
 * ===========================================================================
 * COMO ESTA IMPLEMENTADO Y POR QUE
 * --------------------------------
 * La forma ingenua de hacer MOVL es partir la recta en puntos, resolver la
 * cinematica inversa de cada uno y ejecutar un movimiento completo por punto.
 * El problema es que cada movimiento termina con su propia desaceleracion:
 * el efector SE DETIENE en cada punto intermedio y la recta se recorre a
 * tirones.
 *
 * Aqui se hace distinto. FastAccelStepper permite cambiar el objetivo
 * MIENTRAS el motor se mueve, y recalcula la rampa sin detenerse. Entonces:
 *
 *   cada MOVL_SEGMENT_TICKS_MS (20 ms)
 *      -> se avanza el parametro s sobre la recta segun el avance deseado
 *      -> se resuelve la cinematica inversa de ese punto
 *      -> se llama moveTo() con el nuevo objetivo absoluto
 *
 * El resultado es movimiento continuo. El precio es que la exactitud de la
 * trayectoria depende del periodo de actualizacion: a 80 mm/s y 20 ms, el
 * efector persigue un objetivo que salta 1.6 mm. La desviacion respecto de la
 * recta ideal es menor que eso y esta muy por debajo del backlash del
 * reductor (1.5 mm a alcance maximo), asi que no es el factor limitante.
 *
 * LIMITACION HONESTA: esto es seguimiento de trayectoria, no interpolacion
 * coordinada real con look-ahead como la de un control CNC. Para un pick and
 * place de fichas de Conecta 4 sobra; para cortar un contorno a alta
 * velocidad no.
 * -------------------------------------------------------------------------*/
static void line_set_axis_speed(FastAccelStepper *st, int32_t target, float dt_s)
{
    const int32_t d = labs(target - st->getCurrentPosition());

    /* Velocidad necesaria para cubrir el salto en un tick, con 60 % de margen
     * para que el eje alcance el objetivo antes del siguiente refresco y el
     * movimiento no se quede rezagado. Sin margen, el eje persigue siempre
     * por detras y la trayectoria se recorta en las curvas. */
    uint32_t hz = (uint32_t)((float)d / fmaxf(dt_s, 0.001f) * 1.6f);

    if (hz < AXIS_MIN_SPEED_HZ)  hz = AXIS_MIN_SPEED_HZ;
    if (hz > AXIS_MAX_SPEED_HZ)  hz = AXIS_MAX_SPEED_HZ;

    st->setSpeedInHz(hz);
    st->setAcceleration(AXIS_MAX_ACCEL);
    st->moveTo(target);
}

static void line_tick(void)
{
    const uint32_t now  = millis();
    const float    dt_s = (float)(now - g_line_t_ms) / 1000.0f;
    g_line_t_ms = now;

    if (g_line_s < 1.0f) {
        g_line_s += (g_line_feed * dt_s) / g_line_len;
        if (g_line_s > 1.0f) g_line_s = 1.0f;

        Point_t p;
        p.x = g_line_a.x + (g_line_b.x - g_line_a.x) * g_line_s;
        p.y = g_line_a.y + (g_line_b.y - g_line_a.y) * g_line_s;

        Joints_t j;
        if (kin_inverse(p.x, p.y, g_elbow, &j) != IK_OK) {
            /* No deberia ocurrir: la recta completa se valido antes de
             * arrancar. Si ocurre, se para en vez de seguir a ciegas. */
            sv_request_abort(FAULT_WORKSPACE);
            return;
        }

        line_set_axis_speed(g_a1, kin_deg_to_steps(j.q1), dt_s);
        line_set_axis_speed(g_a2, kin_deg_to_steps(j.q2), dt_s);
    }
    else if (!g_a1->isRunning() && !g_a2->isRunning()) {
        g_exec = EXEC_IDLE;
        sv_set_state(STATE_IDLE);
    }
}

static bool start_movl(float x, float y, uint16_t pct)
{
    float q1, q2;
    motion_get_joints(&q1, &q2);

    g_line_a = kin_forward(q1, q2);
    g_line_b.x = x;
    g_line_b.y = y;

    /* VALIDACION DE LA TRAYECTORIA COMPLETA ANTES DE MOVERSE.
     * El espacio de trabajo es un ANILLO, no un disco: una recta entre dos
     * puntos alcanzables puede atravesar el agujero central. Sin esto, el
     * brazo arrancaria y se detendria a media trayectoria. */
    Point_t fail;
    const IkResult_t v = kin_validate_line(g_line_a, g_line_b, g_elbow,
                                           MOVL_VALIDATE_SAMPLES, &fail);
    if (v != IK_OK) {
        report_printf("ERR MOVL_PATH codigo=%d punto=(%.1f,%.1f)\n",
                      (int)v, fail.x, fail.y);
        return false;
    }

    const float dx = g_line_b.x - g_line_a.x;
    const float dy = g_line_b.y - g_line_a.y;
    g_line_len = sqrtf(dx * dx + dy * dy);

    if (g_line_len < 0.5f) {       /* ya estamos ahi */
        report_line("OK MOVL (sin desplazamiento)");
        return true;
    }

    g_line_feed = MOVL_MAX_FEED_MMS * (float)pct / 100.0f;
    g_line_s    = 0.0f;
    g_line_t_ms = millis();
    g_exec      = EXEC_LINE;
    sv_set_state(STATE_MOVING);
    return true;
}

/* ===========================================================================
 * PARO
 * ===========================================================================*/
void motion_decelerate_stop(void)
{
    /* Se refuerza la desaceleracion (3x) para frenar rapido, pero se sigue
     * usando una RAMPA, no un corte seco: un forceStop() a velocidad alta
     * hace que la inercia del brazo arrastre el rotor y se pierdan pasos,
     * dejando la posicion referenciada invalida. */
    if (g_a1) {
        g_a1->setAcceleration((int32_t)AXIS_MAX_ACCEL * STOP_DECEL_MULTIPLIER);
        g_a1->applySpeedAcceleration();
        g_a1->stopMove();
    }
    if (g_a2) {
        g_a2->setAcceleration((int32_t)AXIS_MAX_ACCEL * STOP_DECEL_MULTIPLIER);
        g_a2->applySpeedAcceleration();
        g_a2->stopMove();
    }
    /* Los drivers SIGUEN ENERGIZADOS: el brazo es vertical y soltarlos lo
     * dejaria caer. Para liberar par hay un comando DISABLE aparte. */
}

/* ===========================================================================
 * TEST  -  MEDICION REAL DE PERDIDA DE PASOS
 * ===========================================================================
 * Un contador interno comparado consigo mismo siempre da exacto, aunque el
 * motor haya perdido 400 pasos: el firmware no tiene forma de enterarse. Sin
 * encoder, la UNICA referencia absoluta disponible es el final de carrera.
 *
 * Por eso TEST hace esto:
 *      1. parte de una posicion referenciada
 *      2. ejecuta N ciclos de ida y vuelta de la amplitud pedida
 *      3. vuelve a buscar el final de carrera EN MODO MEDICION, sin corregir
 *      4. reporta la diferencia entre lo que el contador cree y donde el
 *         switch dice que esta realmente
 *
 * Esa diferencia es perdida de pasos + repetibilidad del switch + backlash.
 * Para separarlas: corre el test primero a aceleracion baja (la lectura de
 * ahi es tu piso de ruido, tipicamente unos pocos pasos) y repitelo subiendo
 * AXIS_MAX_ACCEL hasta que la cifra se dispare. Ese es tu limite real.
 * -------------------------------------------------------------------------*/
static bool wait_motion_done(uint32_t timeout_ms)
{
    const uint32_t t0 = millis();
    while (g_a1->isRunning() || g_a2->isRunning()) {
        if (sv_abort_requested())            return false;
        if (millis() - t0 > timeout_ms)      return false;
        vTaskDelay(pdMS_TO_TICKS(MOTION_TICK_MS));
    }
    return true;
}

static void run_test(int32_t amplitude_steps, int32_t cycles)
{
    if (!sv_is_homed()) { report_line("ERR NOT_HOMED (TEST requiere HOME previo)"); return; }
    if (amplitude_steps <= 0) amplitude_steps = (int32_t)(10.0f * STEPS_PER_DEG);
    if (cycles <= 0) cycles = 10;

    /* Cada eje se aleja de su propio final de carrera, nunca hacia el. */
    const int32_t dir1 = -A1_HOMING_DIR;
    const int32_t dir2 = -A2_HOMING_DIR;

    const int32_t base1 = g_a1->getCurrentPosition();
    const int32_t base2 = g_a2->getCurrentPosition();

    report_printf("OK TEST amplitud=%ld pasos (%.2f deg) ciclos=%ld\n",
                  (long)amplitude_steps, amplitude_steps * DEG_PER_STEP, (long)cycles);

    sv_set_state(STATE_MOVING);
    for (int32_t c = 0; c < cycles; ++c) {
        start_movj_steps(base1 + dir1 * amplitude_steps,
                         base2 + dir2 * amplitude_steps, 100);
        if (!wait_motion_done(30000)) { report_line("ERR TEST_ABORTED"); return; }

        start_movj_steps(base1, base2, 100);
        if (!wait_motion_done(30000)) { report_line("ERR TEST_ABORTED"); return; }
    }

    /* Re-referenciado EN MODO MEDICION: no corrige nada, solo mide. */
    int32_t m1 = 0, m2 = 0;
    const HomingResult_t r1 = homing_axis(1, false, &m1);
    const HomingResult_t r2 = homing_axis(2, false, &m2);

    if (r1 != HOMING_OK || r2 != HOMING_OK) {
        report_printf("ERR TEST_REHOME a1=%s a2=%s\n",
                      homing_result_name(r1), homing_result_name(r2));
        g_exec = EXEC_IDLE;
        return;
    }

    const int32_t exp1 = kin_deg_to_steps(A1_HOME_DEG);
    const int32_t exp2 = kin_deg_to_steps(A2_HOME_DEG);
    const int32_t d1 = m1 - exp1;
    const int32_t d2 = m2 - exp2;

    report_printf("TEST A1 esperado=%ld medido=%ld deriva=%+ld pasos (%+.3f deg)\n",
                  (long)exp1, (long)m1, (long)d1, d1 * DEG_PER_STEP);
    report_printf("TEST A2 esperado=%ld medido=%ld deriva=%+ld pasos (%+.3f deg)\n",
                  (long)exp2, (long)m2, (long)d2, d2 * DEG_PER_STEP);
    report_line("TEST FIN (deriva incluye perdida de pasos + repetibilidad del switch + backlash)");

    g_exec = EXEC_IDLE;
    sv_set_state(STATE_IDLE);
}

/* ===========================================================================
 * DESPACHO DE SETPOINTS
 * ===========================================================================*/
static void dispatch(const Setpoint_t *sp)
{
    switch (sp->type) {

    case MOTION_HOME: {
        const HomingResult_t r = homing_all();
        if (r == HOMING_OK) {
            sv_set_homed(true);
            sv_set_state(STATE_IDLE);
            int32_t s1, s2; motion_get_steps(&s1, &s2);
            persist_save_home(s1, s2);
            report_printf("OK HOME q1=%.2f q2=%.2f\n",
                          kin_steps_to_deg(s1), kin_steps_to_deg(s2));
        } else {
            sv_set_homed(false);
            sv_set_fault(FAULT_HOMING_TIMEOUT);
            report_printf("ERR HOME %s\n", homing_result_name(r));
        }
        g_exec = EXEC_IDLE;
        break;
    }

    case MOTION_JOG: {
        /* JOG es relativo y siempre lento: es el modo de enseñar posiciones
         * a mano, donde importa el control fino, no la velocidad. */
        FastAccelStepper *st = motion_stepper(sp->axis_id);
        if (!st) { report_line("ERR AXIS"); break; }

        const float cur    = kin_steps_to_deg(st->getCurrentPosition());
        const float target = cur + sp->target_a;
        const float lo = (sp->axis_id == 1) ? A1_MIN_DEG : A2_MIN_DEG;
        const float hi = (sp->axis_id == 1) ? A1_MAX_DEG : A2_MAX_DEG;

        if (sv_is_homed() && (target < lo || target > hi)) {
            report_printf("ERR JOG_LIMIT destino=%.2f rango=[%.1f,%.1f]\n", target, lo, hi);
            break;
        }
        st->setSpeedInHz(JOG_SPEED_HZ);
        st->setAcceleration(JOG_ACCEL);
        st->moveTo(kin_deg_to_steps(target));
        g_exec = EXEC_POINT;
        sv_set_state(STATE_MOVING);
        break;
    }

    case MOTION_MOVJ: {
        if (!sv_is_homed()) { report_line("ERR NOT_HOMED"); break; }
        if (!kin_joints_in_limits(sp->target_a, sp->target_b)) {
            report_printf("ERR JOINT_LIMIT q1=%.2f q2=%.2f\n", sp->target_a, sp->target_b);
            break;
        }
        const Point_t p = kin_forward(sp->target_a, sp->target_b);
        if (kin_point_in_workspace(p.x, p.y) != IK_OK) {
            report_printf("ERR WORKSPACE destino=(%.1f,%.1f)\n", p.x, p.y);
            break;
        }
        start_movj_steps(kin_deg_to_steps(sp->target_a),
                         kin_deg_to_steps(sp->target_b), sp->speed_pct);
        break;
    }

    case MOTION_MOVL:
        if (!sv_is_homed()) { report_line("ERR NOT_HOMED"); break; }
        start_movl(sp->target_a, sp->target_b, sp->speed_pct);
        break;

    case MOTION_TEST:
        run_test((int32_t)sp->target_a, sp->aux);
        break;

    case MOTION_STOP:
        /* Ruta secundaria: lo normal es que un STOP llegue por
         * sv_request_abort() y no por la cola (ver types.h). Se atiende aqui
         * tambien por si algun productor futuro lo encola. */
        sv_request_abort(FAULT_NONE);
        break;

    case MOTION_GRIPPER:
        report_line("ERR NOT_IMPLEMENTED (pinza: Parcial 3)");
        break;

    default:
        report_line("ERR UNKNOWN_MOTION_TYPE");
        break;
    }
}

/* ===========================================================================
 * TaskMotion  -  nucleo 1, prioridad 5
 * ===========================================================================
 * POR QUE HACE POLLING CADA 5 ms Y NO SE BLOQUEA EN LA COLA
 * --------------------------------------------------------
 * Un xQueueReceive con espera infinita seria mas elegante, pero la tarea
 * tiene tres trabajos ademas de recibir ordenes:
 *      - refrescar el objetivo del interpolador lineal (MOVL)
 *      - detectar que un movimiento termino
 *      - atender el paro fuera de banda en milisegundos
 * Ninguno de los tres genera un evento de cola. El tick de 5 ms cubre los
 * tres con un coste de CPU despreciable.
 *
 * El vTaskDelay del final NO ES OPCIONAL. Esta tarea tiene prioridad 5 en el
 * nucleo 1, por encima de la tarea interna de FastAccelStepper. Si no cediera
 * CPU, ahogaria al generador de rampas y la tarea idle del nucleo 1 nunca
 * correria: el watchdog reiniciaria el ESP32 a los pocos segundos.
 * -------------------------------------------------------------------------*/
void TaskMotion(void *pv)
{
    (void)pv;
    Setpoint_t sp;

    for (;;) {
        if (sv_abort_requested()) {
            motion_decelerate_stop();
            sv_clear_abort();
            g_exec = EXEC_STOPPING;
        }

        switch (g_exec) {

        case EXEC_IDLE:
            if (xQueueReceive(setpointQueue, &sp, 0) == pdTRUE) {
                dispatch(&sp);
            }
            break;

        case EXEC_POINT:
            if (!g_a1->isRunning() && !g_a2->isRunning()) {
                g_exec = EXEC_IDLE;
                if (sv_get_state() == STATE_MOVING) sv_set_state(STATE_IDLE);
            }
            break;

        case EXEC_LINE:
            line_tick();
            break;

        case EXEC_STOPPING:
            if (!g_a1->isRunning() && !g_a2->isRunning()) {
                g_exec = EXEC_IDLE;
                if (sv_get_fault() == FAULT_NONE) sv_set_state(STATE_STOPPED);
                report_line("EVT STOPPED");
            }
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(MOTION_TICK_MS));
    }
}

/* ===========================================================================
 * INICIALIZACION
 * ===========================================================================*/
void motion_init(void)
{
    /* engine.init(1) ancla la tarea interna de FastAccelStepper al nucleo 1,
     * junto con el resto del movimiento. Si se dejara en el nucleo 0, el
     * stack de WiFi (que vive ahi) introduciria jitter en el refresco de las
     * rampas. Requiere FastAccelStepper >= 0.30. */
    engine.init(CORE_MOTION);

    g_a1 = engine.stepperConnectToPin(PIN_A1_PUL);
    g_a2 = engine.stepperConnectToPin(PIN_A2_PUL);

    if (!g_a1 || !g_a2) {
        /* Ocurre si los pines no admiten salida o si se superaron los
         * canales RMT disponibles. Es un fallo fatal de configuracion. */
        report_line("ERR FATAL: no se pudo conectar los steppers");
        return;
    }

    /* dir_change_delay_us: el TB6600 exige ~5 us de establecimiento de DIR
     * antes del flanco de PUL. Sin este retardo, el primer paso tras un
     * cambio de sentido se ejecuta en la direccion anterior: el error clasico
     * de "pierde un paso cada vez que invierte". */
    g_a1->setDirectionPin(PIN_A1_DIR, A1_DIR_HIGH_COUNTS_UP, DIR_CHANGE_DELAY_US);
    g_a2->setDirectionPin(PIN_A2_DIR, A2_DIR_HIGH_COUNTS_UP, DIR_CHANGE_DELAY_US);

    /* ENA NO se registra en FastAccelStepper a proposito.
     * La libreria ofrece setAutoEnable(), que libera los drivers cuando el
     * eje lleva un rato quieto. En un brazo horizontal es una funcion util
     * que ahorra calor; en ESTE brazo, que trabaja en plano vertical, soltar
     * los drivers significa que el brazo SE CAE. El pin se controla a mano
     * desde supervisor.cpp y solo el comando DISABLE lo libera. */

    g_a1->setSpeedInHz(JOG_SPEED_HZ);
    g_a1->setAcceleration(JOG_ACCEL);
    g_a2->setSpeedInHz(JOG_SPEED_HZ);
    g_a2->setAcceleration(JOG_ACCEL);

    g_a1->setCurrentPosition(0);
    g_a2->setCurrentPosition(0);
}
