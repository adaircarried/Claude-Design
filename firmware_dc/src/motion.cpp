// =============================================================================
//  motion.cpp — TaskMotion (núcleo 1, prioridad 5, 10 ms)
// =============================================================================
//  ARQUITECTURA
//  TaskMotion no sabe quién le manda órdenes: solo hace xQueueReceive sobre
//  setpointQueue. Hoy la llena la consola; mañana un parser binario. Ninguno de
//  los dos toca el control: el único punto de contacto es Setpoint_t.
//
//  Las órdenes se ejecutan en secuencia (como un programa de robot): mientras
//  hay un movimiento activo, las siguientes esperan en la cola. STOP es la
//  excepción: motion_submit() lo mete al frente, TaskMotion lo detecta con
//  xQueuePeek en cada ciclo, desacelera y descarta lo que quedaba en cola.
//
//  PERFIL TRAPEZOIDAL
//  El PID nunca recibe el objetivo final: recibe una referencia que avanza con
//  aceleración, crucero y desaceleración limitadas (y su velocidad, para el
//  feed-forward). Si la distancia no alcanza para llegar a vmax, el perfil
//  degenera en triangular.
//
//  SINCRONIZACIÓN MOVJ (criterio de mayor peso)
//  Se planea UN solo perfil normalizado sobre la distancia del eje con mayor
//  recorrido, D = max(|Δq1|, |Δq2|), con la velocidad y aceleración pedidas.
//  Cada eje sigue q_i(t) = q0_i + Δq_i · s(t)/D. Consecuencias:
//    - Ambos ejes arrancan y terminan en el mismo instante, por construcción.
//    - El eje corto escala velocidad Y aceleración por |Δq_i|/D, es decir, se
//      mueve más lento y con la misma forma de perfil.
//    - Ningún eje excede vmax ni amax.
//
//  MOVL
//  Mismo perfil 1D, pero sobre la longitud del segmento cartesiano. Cada 10 ms
//  se interpola (x, y) y se resuelve la cinemática inversa. Antes de arrancar se
//  valida todo el trayecto (muestreado) y se reduce la velocidad si alguna
//  articulación tuviera que exceder su máximo.
// =============================================================================
#include "motion.h"
#include "config.h"
#include "shared_state.h"
#include "kinematics.h"
#include "console.h"

QueueHandle_t setpointQueue = nullptr;

// -----------------------------------------------------------------------------
//  TrapProfile
// -----------------------------------------------------------------------------
void TrapProfile::plan(float D, float vmax, float amax) {
    D_ = fabsf(D);
    a_ = amax > 1e-3f ? amax : 1e-3f;
    if (vmax < 1e-3f) vmax = 1e-3f;
    t_ = 0; s_ = 0; v_ = 0; stopping_ = false;
    if (D_ < 1e-4f) { T_ = 0; done_ = true; return; }

    float ta = vmax / a_;
    float da = 0.5f * a_ * ta * ta;
    if (2.0f * da >= D_) {                 // triangular
        vp_ = sqrtf(D_ * a_);
        ta_ = vp_ / a_;
        tc_ = 0;
    } else {                               // trapezoidal
        vp_ = vmax;
        ta_ = ta;
        tc_ = (D_ - 2.0f * da) / vmax;
    }
    T_ = 2.0f * ta_ + tc_;
    done_ = false;
}

bool TrapProfile::step(float dt) {
    if (done_) return true;
    if (stopping_) {
        tstop_ += dt;
        const float tend = v0_ / a_;
        if (tstop_ >= tend) {
            s_ = s0_ + v0_ * tend - 0.5f * a_ * tend * tend;
            v_ = 0;
            done_ = true;
        } else {
            s_ = s0_ + v0_ * tstop_ - 0.5f * a_ * tstop_ * tstop_;
            v_ = v0_ - a_ * tstop_;
        }
        if (s_ > D_) s_ = D_;
        return done_;
    }

    t_ += dt;
    if (t_ >= T_) {
        s_ = D_; v_ = 0; done_ = true;
    } else if (t_ < ta_) {                                   // acelera
        s_ = 0.5f * a_ * t_ * t_;
        v_ = a_ * t_;
    } else if (t_ < ta_ + tc_) {                             // crucero
        s_ = 0.5f * a_ * ta_ * ta_ + vp_ * (t_ - ta_);
        v_ = vp_;
    } else {                                                 // desacelera
        const float td = T_ - t_;
        s_ = D_ - 0.5f * a_ * td * td;
        v_ = a_ * td;
    }
    return done_;
}

void TrapProfile::stop() {
    if (done_ || stopping_) return;
    stopping_ = true;
    s0_ = s_; v0_ = v_; tstop_ = 0;
    if (v0_ < 1e-4f) { done_ = true; v_ = 0; }
}

// -----------------------------------------------------------------------------
//  Cola
// -----------------------------------------------------------------------------
void motion_init() {
    setpointQueue = xQueueCreate(SETPOINT_QUEUE_LEN, sizeof(Setpoint_t));
}

bool motion_submit(const Setpoint_t &sp) {
    if (!setpointQueue) return false;
    if (sp.type == MOTION_STOP) return xQueueSendToFront(setpointQueue, &sp, 0) == pdTRUE;
    return xQueueSendToBack(setpointQueue, &sp, 0) == pdTRUE;
}

// -----------------------------------------------------------------------------
//  TaskMotion
// -----------------------------------------------------------------------------
enum MoveKind { MK_NONE, MK_JOINT, MK_LINEAR };

static TrapProfile s_prof;
static MoveKind    s_kind = MK_NONE;
static float       s_q0[NUM_AXES], s_dq[NUM_AXES], s_D = 0;   // articular
static float       s_p0[2], s_p1[2], s_L = 0;                 // cartesiano
static float       s_qprev[NUM_AXES];

static const float DT = MOTION_PERIOD_MS / 1000.0f;

static void read_ref(float q[NUM_AXES]) {
    state_lock();
    for (uint8_t i = 0; i < NUM_AXES; i++) q[i] = g_state.cmd[i].pos_ref;
    state_unlock();
}

static void write_ref(const float q[NUM_AXES], const float v[NUM_AXES]) {
    state_lock();
    for (uint8_t i = 0; i < NUM_AXES; i++) {
        g_state.cmd[i].pos_ref = q[i];
        g_state.cmd[i].vel_ref = v[i];
    }
    g_state.cmd_seq++;
    state_unlock();
}

static void finish_move() {
    s_kind = MK_NONE;
    state_lock();
    g_state.motion_busy = false;
    g_state.moves_done++;
    for (uint8_t i = 0; i < NUM_AXES; i++) g_state.cmd[i].vel_ref = 0;
    g_state.cmd_seq++;
    state_unlock();
}

static void reject(const char *why) {
    con_printf("[MOTION] rechazado: %s\n", why);
    state_lock();
    g_state.moves_done++;       // cuenta como terminado para quien espere
    state_unlock();
}

static void begin_busy() {
    state_lock();
    g_state.motion_busy = true;
    for (uint8_t i = 0; i < NUM_AXES; i++) g_state.move_max_err[i] = 0;
    state_unlock();
}

static float speed_frac(uint16_t pct) {
    if (pct < 1) pct = 1;
    if (pct > 100) pct = 100;
    return pct / 100.0f;
}

static float get_accel() {
    state_lock();
    float a = g_state.accel;
    state_unlock();
    return a;
}

// Movimiento articular coordinado (MOVJ y JOG)
static void start_joint(const float target[NUM_AXES], float vmax, float amax) {
    if (!kin_joints_valid(target[0], target[1])) {
        con_printf("[MOTION] objetivo q1=%.1f q2=%.1f; limites q1 [%.0f, %.0f], q2 [%.0f, %.0f]\n",
                   target[0], target[1], JOINT_MIN_DEG[0], JOINT_MAX_DEG[0],
                   JOINT_MIN_DEG[1], JOINT_MAX_DEG[1]);
        reject("objetivo fuera de los limites articulares (¿falta HOME? revise STATUS)");
        return;
    }
    read_ref(s_q0);
    s_D = 0;
    for (uint8_t i = 0; i < NUM_AXES; i++) {
        s_dq[i] = target[i] - s_q0[i];
        if (fabsf(s_dq[i]) > s_D) s_D = fabsf(s_dq[i]);
    }
    s_prof.plan(s_D, vmax, amax);
    s_kind = MK_JOINT;
    begin_busy();
    if (s_prof.done()) finish_move();
    else con_printf("[MOTION] MOVJ dq1=%.2f dq2=%.2f  T=%.2f s\n", s_dq[0], s_dq[1], s_prof.duration());
}

// Movimiento lineal cartesiano (MOVL y JOGC)
static void start_linear(float x1, float y1, uint16_t pct, float speed_factor) {
    float q0[NUM_AXES];
    read_ref(q0);
    float x0, y0;
    kin_forward(q0[0], q0[1], x0, y0);

    // El punto de inicio debe ser alcanzable con la misma rama de codo;
    // si no, la primera muestra de IK saltaría a la otra configuración.
    float qa, qb;
    if (!kin_inverse(x0, y0, ELBOW_UP, qa, qb) ||
        fabsf(qa - q0[0]) > 2.0f || fabsf(qb - q0[1]) > 2.0f) {
        reject("pose inicial singular o con otra rama de codo; use MOVJ primero");
        return;
    }
    if (!kin_inverse(x1, y1, ELBOW_UP, qa, qb)) {
        reject(kin_last_error());
        return;
    }

    const float dx = x1 - x0, dy = y1 - y0;
    const float L = sqrtf(dx * dx + dy * dy);
    if (L < 0.05f) { begin_busy(); finish_move(); return; }

    // Validación del trayecto completo y factor de amplificación k = |dq|/ds
    const int N = 60;
    float kmax = 0, qp0 = q0[0], qp1 = q0[1];
    for (int n = 1; n <= N; n++) {
        const float f = (float)n / N;
        float q1, q2;
        if (!kin_inverse(x0 + dx * f, y0 + dy * f, ELBOW_UP, q1, q2)) {
            reject("la trayectoria recta sale del espacio de trabajo");
            return;
        }
        const float dq = fmaxf(fabsf(q1 - qp0), fabsf(q2 - qp1));
        const float k = dq / (L / N);
        if (k > kmax) kmax = k;
        qp0 = q1; qp1 = q2;
    }

    const float frac   = speed_frac(pct) * speed_factor;
    const float accel  = get_accel();
    float vlin = VMAX_LINEAR_MM_S * frac;
    float alin = accel * (VMAX_LINEAR_MM_S / VMAX_JOINT_DEG_S);
    if (kmax > 1e-6f) {
        vlin = fminf(vlin, VMAX_JOINT_DEG_S * frac / kmax);
        alin = fminf(alin, accel / kmax);
    }

    s_p0[0] = x0; s_p0[1] = y0;
    s_p1[0] = x1; s_p1[1] = y1;
    s_L = L;
    for (uint8_t i = 0; i < NUM_AXES; i++) s_qprev[i] = q0[i];
    s_prof.plan(L, vlin, alin);
    s_kind = MK_LINEAR;
    begin_busy();
    con_printf("[MOTION] MOVL (%.1f,%.1f)->(%.1f,%.1f) L=%.1f mm v=%.1f mm/s T=%.2f s\n",
               x0, y0, x1, y1, L, vlin, s_prof.duration());
}

static void start_setpoint(const Setpoint_t &sp) {
    const float frac  = speed_frac(sp.speed_pct);
    const float accel = get_accel();

    switch (sp.type) {
        case MOTION_MOVJ: {
            float t[NUM_AXES] = {sp.target_a, sp.target_b};
            start_joint(t, VMAX_JOINT_DEG_S * frac, accel);
            break;
        }
        case MOTION_JOG: {
            if (sp.axis_id >= NUM_AXES) { reject("eje invalido"); break; }
            float t[NUM_AXES];
            read_ref(t);
            t[sp.axis_id] += sp.target_a;               // incremental
            start_joint(t, VMAX_JOINT_DEG_S * frac * JOG_SPEED_FACTOR, accel);
            break;
        }
        case MOTION_MOVL:
            start_linear(sp.target_a, sp.target_b, sp.speed_pct, 1.0f);
            break;
        case MOTION_JOGC: {
            float q[NUM_AXES], x, y;
            read_ref(q);
            kin_forward(q[0], q[1], x, y);
            start_linear(x + sp.target_a, y + sp.target_b, sp.speed_pct, JOG_SPEED_FACTOR);
            break;
        }
        case MOTION_HOME:
            // El cero lo ejecuta TaskControl entre dos muestras (sin carreras)
            state_lock();
            g_state.req_zero_mask = (1u << NUM_AXES) - 1;
            state_unlock();
            vTaskDelay(pdMS_TO_TICKS(3 * CONTROL_PERIOD_MS));
            begin_busy();
            finish_move();
            con_printf("[MOTION] HOME: cero fijado en la posicion actual\n");
            break;
        case MOTION_STOP:
            xQueueReset(setpointQueue);
            begin_busy();
            finish_move();
            break;
    }
}

void task_motion(void *arg) {
    const TickType_t period = pdMS_TO_TICKS(MOTION_PERIOD_MS);
    TickType_t last_wake = xTaskGetTickCount();
    Setpoint_t sp;

    for (;;) {
        vTaskDelayUntil(&last_wake, period);

        // Falla en TaskControl: se aborta todo y se vacía la cola
        bool fault = false;
        float err[NUM_AXES];
        state_lock();
        for (uint8_t i = 0; i < NUM_AXES; i++) {
            if (g_state.fault[i] != FAULT_NONE) fault = true;
            err[i] = fabsf(g_state.st[i].err);
        }
        state_unlock();
        if (fault) {
            if (s_kind != MK_NONE) {
                finish_move();
                con_printf("[MOTION] movimiento abortado por falla\n");
            }
            if (uxQueueMessagesWaiting(setpointQueue)) {
                xQueueReset(setpointQueue);
                state_lock(); g_state.moves_done++; state_unlock();
            }
            continue;
        }

        if (s_kind == MK_NONE) {
            if (xQueueReceive(setpointQueue, &sp, 0) == pdTRUE) start_setpoint(sp);
            continue;
        }

        // Movimiento activo: ¿llegó un STOP?
        if (xQueuePeek(setpointQueue, &sp, 0) == pdTRUE && sp.type == MOTION_STOP) {
            xQueueReset(setpointQueue);
            s_prof.stop();
            con_printf("[MOTION] STOP: desacelerando\n");
        }

        const bool done = s_prof.step(DT);
        float q[NUM_AXES], v[NUM_AXES];

        if (s_kind == MK_JOINT) {
            const float f  = s_D > 0 ? s_prof.s() / s_D : 1.0f;
            const float fv = s_D > 0 ? s_prof.v() / s_D : 0.0f;
            for (uint8_t i = 0; i < NUM_AXES; i++) {
                q[i] = s_q0[i] + s_dq[i] * f;
                v[i] = s_dq[i] * fv;
            }
        } else {
            const float f = s_prof.s() / s_L;
            const float x = s_p0[0] + (s_p1[0] - s_p0[0]) * f;
            const float y = s_p0[1] + (s_p1[1] - s_p0[1]) * f;
            if (!kin_inverse(x, y, ELBOW_UP, q[0], q[1])) {
                // No debería ocurrir (se validó antes); por seguridad se detiene
                const float zero[NUM_AXES] = {0, 0};
                write_ref(s_qprev, zero);
                finish_move();
                con_printf("[MOTION] MOVL abortado: IK fallo a mitad de trayecto\n");
                continue;
            }
            for (uint8_t i = 0; i < NUM_AXES; i++) {
                v[i] = (q[i] - s_qprev[i]) / DT;
                s_qprev[i] = q[i];
            }
        }

        write_ref(q, v);

        state_lock();
        for (uint8_t i = 0; i < NUM_AXES; i++)
            if (err[i] > g_state.move_max_err[i]) g_state.move_max_err[i] = err[i];
        state_unlock();

        if (done) finish_move();
    }
}
