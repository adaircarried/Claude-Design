// =============================================================================
//  comms.cpp — Parser de comandos de texto
// =============================================================================
#include "comms.h"
#include "config.h"
#include "setpoint.h"
#include "shared_state.h"
#include "kinematics.h"
#include "storage.h"
#include "telemetry.h"
#include "control.h"
#include "console.h"
#include <driver/gpio.h>

static uint16_t s_speed    = SPEED_PCT_DEFAULT;
static int8_t   s_cal_axis = -1;
static char     s_buf[128];
static uint8_t  s_len = 0;

// -----------------------------------------------------------------------------
//  Utilidades
// -----------------------------------------------------------------------------
static bool read_line(char *out, size_t n) {
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\r' || c == '\n') {
            if (s_len == 0) continue;
            s_buf[s_len] = 0;
            strncpy(out, s_buf, n - 1);
            out[n - 1] = 0;
            s_len = 0;
            return true;
        }
        if (s_len < sizeof(s_buf) - 1) s_buf[s_len++] = c;
    }
    return false;
}

static void upcase(char *s) { for (; *s; s++) *s = toupper((unsigned char)*s); }

static bool parse_f(const char *tok, float &out) {
    if (!tok) return false;
    char *end;
    out = strtof(tok, &end);
    return end != tok;
}

static int parse_axis(const char *tok) {
    if (!tok) return -1;
    if (tok[0] == 'J') tok++;
    int a = atoi(tok);
    return (a >= 1 && a <= NUM_AXES) ? a - 1 : -1;
}

static bool any_fault() {
    bool f = false;
    state_lock();
    for (uint8_t i = 0; i < NUM_AXES; i++) f |= g_state.fault[i] != FAULT_NONE;
    state_unlock();
    return f;
}

static bool motion_allowed() {
    if (s_cal_axis >= 0) { con_printf("ERR calibracion en curso (CAL END / CAL ABORT)\n"); return false; }
    if (any_fault())     { con_printf("ERR falla activa: revise y envie RESET\n");        return false; }
    return true;
}

static bool submit(const Setpoint_t &sp) {
    if (!motion_submit(sp)) { con_printf("ERR cola llena\n"); return false; }
    return true;
}

static uint32_t moves_done() {
    state_lock();
    uint32_t n = g_state.moves_done;
    state_unlock();
    return n;
}

// Espera a que TaskMotion complete 'target' órdenes. Permite abortar con STOP.
// Devuelve false si hubo falla o el usuario abortó.
static bool wait_done(uint32_t target) {
    char line[64];
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(20));
        if (moves_done() >= target) return !any_fault();
        if (any_fault()) return false;
        if (read_line(line, sizeof(line))) {
            upcase(line);
            if (strncmp(line, "STOP", 4) == 0) {
                Setpoint_t sp = {MOTION_STOP, 0, 0, 0, 0};
                motion_submit(sp);
                con_printf("Abortado por el usuario\n");
                return false;
            }
        }
    }
}

// -----------------------------------------------------------------------------
//  Ciclos ida y vuelta (TEST y SWEEP)
// -----------------------------------------------------------------------------
struct CycleResult {
    float sum_final[NUM_AXES];   // suma de |error final| (acumulado)
    float max_final[NUM_AXES];
    float max_follow[NUM_AXES];  // |error| máximo durante el movimiento
    int   legs;
    bool  ok;
};

static CycleResult run_cycles(int cycles, float amp, bool verbose) {
    CycleResult r = {};
    r.ok = true;
    float q0[NUM_AXES];
    state_lock();
    for (uint8_t i = 0; i < NUM_AXES; i++) q0[i] = g_state.cmd[i].pos_ref;
    state_unlock();
    const float qt[NUM_AXES] = {q0[0] + amp, q0[1] + amp};
    if (!kin_joints_valid(qt[0], qt[1])) {
        con_printf("ERR q0+%.1f sale de los limites articulares\n", amp);
        r.ok = false;
        return r;
    }

    for (int c = 1; c <= cycles && r.ok; c++) {
        float ef[2][NUM_AXES];
        for (int leg = 0; leg < 2; leg++) {
            const float *tgt = leg == 0 ? qt : q0;
            Setpoint_t sp = {MOTION_MOVJ, tgt[0], tgt[1], s_speed, 0};
            uint32_t n0 = moves_done();
            if (!submit(sp) || !wait_done(n0 + 1)) { r.ok = false; break; }
            vTaskDelay(pdMS_TO_TICKS(TEST_SETTLE_MS));
            state_lock();
            for (uint8_t i = 0; i < NUM_AXES; i++) {
                ef[leg][i] = fabsf(g_state.st[i].err);
                r.sum_final[i] += ef[leg][i];
                if (ef[leg][i] > r.max_final[i]) r.max_final[i] = ef[leg][i];
                if (g_state.move_max_err[i] > r.max_follow[i]) r.max_follow[i] = g_state.move_max_err[i];
            }
            state_unlock();
            r.legs++;
        }
        if (verbose && r.ok)
            con_printf("ciclo %3d | ida e1=%.2f e2=%.2f | vuelta e1=%.2f e2=%.2f\n",
                       c, ef[0][0], ef[0][1], ef[1][0], ef[1][1]);
    }
    return r;
}

// -----------------------------------------------------------------------------
//  Comandos
// -----------------------------------------------------------------------------
static void cmd_help() {
    con_lock();
    Serial.println(F(
        "Comandos (eje = 1|2|J1|J2):\n"
        "  CAL <eje> | CAL END | CAL ABORT   calibracion cuentas/vuelta\n"
        "  HOME                    cero en la posicion actual (brazo estirado sobre +X)\n"
        "  JOG <eje> <grados>      jog articular INCREMENTAL, velocidad reducida\n"
        "  JOGC <X|Y> <mm>         jog cartesiano incremental (linea recta)\n"
        "  MOVJ <q1> <q2>          articular coordinado, ambos llegan juntos\n"
        "  MOVL <x> <y>            lineal cartesiano (mm)\n"
        "  PID <eje> <kp> <ki> <kd>   ganancias en caliente\n"
        "  FF <eje> <kff>          feed-forward de velocidad\n"
        "  DB <eje> <grados> <pwm_min>  zona muerta y PWM minimo\n"
        "  LIMIT <pwm>             limite de PWM (100..1023)\n"
        "  FRIC <eje>              mide el PWM minimo que mueve el eje\n"
        "  ENC                     diagnostico: niveles A/B y cuentas en vivo\n"
        "  SPIN <eje> <pwm>        gira en lazo abierto hasta Enter (calibrar con motor)\n"
        "  CPR <eje> <valor>       fija y guarda cuentas por vuelta\n"
        "  SPEED <1-100>  ACCEL <grados/s2>\n"
        "  STOP  STATUS  RESET  SAVE  FACTORY\n"
        "  TELEM ON|OFF            telemetria cada 100 ms\n"
        "  TEST <ciclos> <grados>  ida y vuelta, reporta error\n"
        "  SWEEP <SPEED|ACCEL> <ini> <fin> <inc> <ciclos>"));
    con_unlock();
}

static void cmd_status() {
    AxisStatus st[NUM_AXES];
    FaultCode  flt[NUM_AXES];
    PIDGains   g[NUM_AXES];
    float      cpr[NUM_AXES];
    bool       busy;
    float      accel;
    int        lim;
    uint32_t   cyc, ovr;
    state_lock();
    memcpy(st, g_state.st, sizeof(st));
    memcpy(flt, g_state.fault, sizeof(flt));
    memcpy(g, g_state.gains, sizeof(g));
    memcpy(cpr, g_state.cpr, sizeof(cpr));
    busy = g_state.motion_busy; accel = g_state.accel; lim = g_state.pwm_limit;
    cyc = g_state.control_cycles; ovr = g_state.control_overruns;
    state_unlock();

    float x, y;
    kin_forward(st[0].pos, st[1].pos, x, y);
    con_lock();
    con_printf("--- STATUS ---\n");
    con_printf("Eje  Objetivo   Real      Error    Vel(°/s)  PWM(%%)       Cuentas   CPR     Enc   Estado\n");
    for (uint8_t i = 0; i < NUM_AXES; i++) {
        con_printf("J%u  %8.2f %8.2f %8.2f %9.1f  %5d(%3d%%) %9lld %7.1f  %s  %s\n",
                   i + 1, st[i].pos_ref, st[i].pos, st[i].err, st[i].vel,
                   st[i].pwm, st[i].pwm * 100 / PWM_MAX, (long long)st[i].counts, cpr[i],
                   ENC_MODE[i] == ENC_QUADRATURE ? "x4 " : "1ch", fault_name(flt[i]));
    }
    con_printf("Efector: x=%.1f y=%.1f mm   Movimiento: %s   Cola: %u\n",
               x, y, busy ? "ACTIVO" : "detenido", (unsigned)uxQueueMessagesWaiting(setpointQueue));
    con_printf("SPEED=%u%%  ACCEL=%.0f °/s2  LIMIT=%d  Telemetria=%s\n",
               s_speed, accel, lim, telemetry_enabled() ? "ON" : "OFF");
    for (uint8_t i = 0; i < NUM_AXES; i++)
        con_printf("PID J%u: kp=%.3f ki=%.3f kd=%.3f kff=%.3f db=%.2f pwm_min=%d\n",
                   i + 1, g[i].kp, g[i].ki, g[i].kd, g[i].kff, g[i].deadband, g[i].pwm_min);
    con_printf("Control: %lu ciclos, %lu retrasos de periodo\n", (unsigned long)cyc, (unsigned long)ovr);
    con_unlock();
}

static void cmd_cal(char *arg) {
    if (!arg) { con_printf("Uso: CAL <eje> | CAL END | CAL ABORT\n"); return; }
    if (strcmp(arg, "END") == 0) {
        if (s_cal_axis < 0) { con_printf("ERR no hay calibracion en curso\n"); return; }
        int64_t c;
        state_lock(); c = g_state.st[s_cal_axis].counts; state_unlock();
        if (c < 0) c = -c;
        if (c < 20) { con_printf("ERR solo %lld cuentas: ¿giro el eje? ¿encoder conectado?\n", (long long)c); return; }
        const float cpr = (float)c;
        storage_save_cpr(s_cal_axis, cpr);
        state_lock();
        g_state.cpr[s_cal_axis] = cpr;
        g_state.cal_axis = -1;
        g_state.req_zero_mask |= (1u << s_cal_axis);
        state_unlock();
        con_printf("CAL J%d OK: %.0f cuentas/vuelta (%.3f °/cuenta), guardado en NVS.\n",
                   s_cal_axis + 1, cpr, 360.0f / cpr);
        con_printf("El eje quedo en cero. Coloque el brazo en la pose HOME y envie HOME.\n");
        s_cal_axis = -1;
        return;
    }
    if (strcmp(arg, "ABORT") == 0) {
        state_lock(); g_state.cal_axis = -1; g_state.req_fault_reset = true; state_unlock();
        s_cal_axis = -1;
        con_printf("Calibracion cancelada\n");
        return;
    }
    int ax = parse_axis(arg);
    if (ax < 0) { con_printf("ERR eje invalido\n"); return; }
    bool busy;
    state_lock(); busy = g_state.motion_busy; state_unlock();
    if (busy) { con_printf("ERR hay un movimiento en curso\n"); return; }
    state_lock(); g_state.cal_axis = ax; g_state.req_cal_zero = true; state_unlock();
    s_cal_axis = ax;
    con_lock();
    con_printf("CAL J%d: motor SIN PAR, conteo en cero.\n", ax + 1);
    con_printf("1) Marque una referencia en el eje de SALIDA.\n");
    con_printf("2) Gire a mano exactamente UNA vuelta%s.\n",
               ENC_MODE[ax] == ENC_SINGLE ? " (SIEMPRE en el mismo sentido: encoder de 1 canal)" : "");
    con_printf("3) Envie CAL END para guardar (CAL ABORT para cancelar).\n");
    con_unlock();
}

static void cmd_test(char *a1, char *a2) {
    float fc, amp;
    if (!parse_f(a1, fc) || !parse_f(a2, amp) || fc < 1) { con_printf("Uso: TEST <ciclos> <grados>\n"); return; }
    if (!motion_allowed()) return;
    const int cycles = (int)fc;
    con_printf("TEST %d ciclos de ±%.1f° en ambos ejes (MOVJ, SPEED %u%%). STOP para abortar.\n",
               cycles, amp, s_speed);
    CycleResult r = run_cycles(cycles, amp, true);
    if (r.legs == 0) return;
    con_lock();
    con_printf("--- RESULTADO TEST (%d movimientos%s) ---\n", r.legs, r.ok ? "" : ", INCOMPLETO");
    for (uint8_t i = 0; i < NUM_AXES; i++)
        con_printf("J%u: error final medio=%.3f°  max=%.3f°  acumulado=%.3f°  max seguimiento=%.2f°\n",
                   i + 1, r.sum_final[i] / r.legs, r.max_final[i], r.sum_final[i], r.max_follow[i]);
    con_unlock();
}

static void cmd_sweep(char *param, char *a1, char *a2, char *a3, char *a4) {
    float ini, fin, inc, fc;
    if (!param || !parse_f(a1, ini) || !parse_f(a2, fin) || !parse_f(a3, inc) || !parse_f(a4, fc) ||
        inc <= 0 || fin < ini || fc < 1) {
        con_printf("Uso: SWEEP <SPEED|ACCEL> <ini> <fin> <inc> <ciclos>\n");
        return;
    }
    const bool is_speed = strcmp(param, "SPEED") == 0;
    if (!is_speed && strcmp(param, "ACCEL") != 0) { con_printf("ERR parametro: SPEED o ACCEL\n"); return; }
    if (!motion_allowed()) return;

    const uint16_t speed0 = s_speed;
    float accel0;
    state_lock(); accel0 = g_state.accel; state_unlock();

    con_printf("SWEEP %s %.0f..%.0f paso %.0f, %d ciclos de ±%.0f° por nivel. STOP para abortar.\n",
               param, ini, fin, inc, (int)fc, SWEEP_AMPLITUDE_DEG);
    con_printf("nivel ; J1_err_final_medio ; J1_max_seguimiento ; J2_err_final_medio ; J2_max_seguimiento\n");
    for (float lv = ini; lv <= fin + 1e-3f; lv += inc) {
        if (is_speed) {
            s_speed = (uint16_t)constrain((int)lv, 1, 100);
        } else {
            state_lock(); g_state.accel = constrain(lv, ACCEL_MIN_DEG_S2, ACCEL_MAX_DEG_S2); state_unlock();
        }
        CycleResult r = run_cycles((int)fc, SWEEP_AMPLITUDE_DEG, false);
        if (r.legs > 0)
            con_printf("%.0f ; %.3f ; %.2f ; %.3f ; %.2f\n", lv,
                       r.sum_final[0] / r.legs, r.max_follow[0],
                       r.sum_final[1] / r.legs, r.max_follow[1]);
        if (!r.ok) { con_printf("SWEEP interrumpido\n"); break; }
    }
    s_speed = speed0;
    state_lock(); g_state.accel = accel0; state_unlock();
    con_printf("SWEEP terminado; SPEED y ACCEL restaurados.\n");
}

// Mide el PWM mínimo que mueve el eje (fricción estática del motor + reductor)
// con una rampa en lazo abierto en ambos sentidos. El eje se mueve poco:
// la rampa se detiene en cuanto se detecta movimiento.
static void cmd_fric(char *arg) {
    int ax = parse_axis(arg);
    if (ax < 0) { con_printf("Uso: FRIC <eje>\n"); return; }
    if (!motion_allowed()) return;
    bool busy;
    state_lock(); busy = g_state.motion_busy; state_unlock();
    if (busy) { con_printf("ERR hay un movimiento en curso\n"); return; }

    con_printf("FRIC J%d: rampa de PWM en lazo abierto (el eje se movera unos grados)...\n", ax + 1);
    int found[2] = {-1, -1};
    for (int d = 0; d < 2 && !any_fault(); d++) {
        const int sgn = d == 0 ? 1 : -1;
        for (int p = 0; p <= 600; p += 5) {
            state_lock(); g_state.ol_axis = ax; g_state.ol_pwm = sgn * p; state_unlock();
            vTaskDelay(pdMS_TO_TICKS(40));
            float v;
            state_lock(); v = g_state.st[ax].vel; state_unlock();
            if (fabsf(v) > 5.0f) { found[d] = p; break; }
        }
        state_lock(); g_state.ol_pwm = 0; state_unlock();
        vTaskDelay(pdMS_TO_TICKS(400));
    }
    state_lock(); g_state.ol_axis = -1; g_state.ol_pwm = 0; g_state.req_fault_reset = true; state_unlock();

    if (found[0] < 0 || found[1] < 0) {
        con_printf("No se detecto movimiento hasta PWM 600 (+:%d -:%d). Revise alimentacion 12 V, "
                   "jumper ENA/ENB, cableado del motor y encoder.\n", found[0], found[1]);
        return;
    }
    const int m = found[0] < found[1] ? found[0] : found[1];
    con_printf("FRIC J%d: arranca con PWM +%d (%d%%) / -%d (%d%%)\n", ax + 1,
               found[0], found[0] * 100 / PWM_MAX, found[1], found[1] * 100 / PWM_MAX);
    con_printf("Sugerido: DB %d 0.5 %d   (pwm_min = 80%% del menor)\n", ax + 1, m * 8 / 10);
}

// Diagnóstico de encoders: nivel eléctrico crudo de A/B y cuentas, cada
// 200 ms, hasta que llegue cualquier línea. Separa fallas de cableado /
// alimentación (los niveles no cambian) de fallas de conteo (cambian pero
// las cuentas no).
static void cmd_enc() {
    con_printf("ENC: gire los ejes DESPACIO. Cualquier tecla + Enter para salir.\n");
    con_printf("A/B = nivel del pin (0/1). cnt = cuentas del PCNT.\n");
    char line[32];
    for (int n = 0; n < 300; n++) {            // máx. 60 s
        if (read_line(line, sizeof(line))) break;
        int64_t c[NUM_AXES];
        state_lock();
        for (uint8_t i = 0; i < NUM_AXES; i++) c[i] = g_state.st[i].counts;
        state_unlock();
        con_printf("J1: A=%d B=%d cnt=%6lld   |   J2: A=%d B=%d cnt=%6lld\n",
                   gpio_get_level((gpio_num_t)PIN_ENC_A[0]), gpio_get_level((gpio_num_t)PIN_ENC_B[0]),
                   (long long)c[0],
                   gpio_get_level((gpio_num_t)PIN_ENC_A[1]), gpio_get_level((gpio_num_t)PIN_ENC_B[1]),
                   (long long)c[1]);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    con_printf("ENC terminado\n");
}

// Calibración con el motor: gira en lazo abierto con PWM fijo hasta que el
// usuario presiona Enter. El usuario cuenta las vueltas del eje de salida y
// luego fija CPR = cuentas / vueltas con el comando CPR.
static void cmd_spin(char *a1, char *a2) {
    int ax = parse_axis(a1);
    float p;
    if (ax < 0 || !parse_f(a2, p) || fabsf(p) < 50 || fabsf(p) > 700) {
        con_printf("Uso: SPIN <eje> <pwm 50..700, signo = sentido>\n"); return;
    }
    if (!motion_allowed()) return;
    bool busy;
    state_lock(); busy = g_state.motion_busy; state_unlock();
    if (busy) { con_printf("ERR hay un movimiento en curso\n"); return; }

    int64_t c0;
    state_lock(); c0 = g_state.st[ax].counts; g_state.ol_axis = ax; g_state.ol_pwm = (int)p; state_unlock();
    con_printf("SPIN J%d a PWM %d. Cuente las vueltas del eje de SALIDA y presione Enter para parar.\n",
               ax + 1, (int)p);
    char line[32];
    int64_t c = c0;
    for (int n = 0; n < 1200; n++) {           // máx. 60 s
        if (read_line(line, sizeof(line)) || any_fault()) break;
        vTaskDelay(pdMS_TO_TICKS(50));
        if (n % 6 == 0) {
            state_lock(); c = g_state.st[ax].counts; state_unlock();
            con_printf("  cuentas: %lld\n", (long long)(c - c0));
        }
    }
    state_lock(); g_state.ol_pwm = 0; state_unlock();
    vTaskDelay(pdMS_TO_TICKS(500));             // deja que se detenga
    state_lock();
    c = g_state.st[ax].counts;
    g_state.ol_axis = -1;
    g_state.req_fault_reset = true;             // referencia = posición actual
    state_unlock();
    long long d = (long long)(c - c0);
    if (d < 0) d = -d;
    con_printf("SPIN J%d: %lld cuentas en total.\n", ax + 1, d);
    con_printf("Si dio N vueltas: CPR = %lld / N. Guardelo con:  CPR %d <valor>\n", d, ax + 1);
    con_printf("  (1 vuelta = %lld, 3 vueltas = %.0f, 5 vueltas = %.0f)\n", d, d / 3.0, d / 5.0);
}

static void cmd_cpr(char *a1, char *a2) {
    int ax = parse_axis(a1);
    float v;
    if (ax < 0 || !parse_f(a2, v) || v < 20 || v > 100000) { con_printf("Uso: CPR <eje> <cuentas_por_vuelta>\n"); return; }
    storage_save_cpr(ax, v);
    state_lock();
    g_state.cpr[ax] = v;
    g_state.req_zero_mask |= (1u << ax);
    state_unlock();
    con_printf("OK CPR J%d = %.1f (%.3f °/cuenta), guardado en NVS. Eje en cero: haga HOME en la pose correcta.\n",
               ax + 1, v, 360.0f / v);
}

static void handle(char *line) {
    upcase(line);
    char *cmd = strtok(line, " \t,");
    if (!cmd) return;
    char *a1 = strtok(nullptr, " \t,");
    char *a2 = strtok(nullptr, " \t,");
    char *a3 = strtok(nullptr, " \t,");
    char *a4 = strtok(nullptr, " \t,");
    char *a5 = strtok(nullptr, " \t,");
    float f1, f2, f3;

    if (!strcmp(cmd, "HELP") || !strcmp(cmd, "?")) { cmd_help(); return; }
    if (!strcmp(cmd, "STATUS")) { cmd_status(); return; }
    if (!strcmp(cmd, "CAL"))    { cmd_cal(a1); return; }

    if (!strcmp(cmd, "STOP")) {
        Setpoint_t sp = {MOTION_STOP, 0, 0, 0, 0};
        submit(sp);
        con_printf("OK STOP\n");
        return;
    }
    if (!strcmp(cmd, "HOME")) {
        if (!motion_allowed()) return;
        Setpoint_t sp = {MOTION_HOME, 0, 0, 0, 0};
        if (submit(sp)) con_printf("OK HOME encolado\n");
        return;
    }
    if (!strcmp(cmd, "JOG")) {
        int ax = parse_axis(a1);
        if (ax < 0 || !parse_f(a2, f1)) { con_printf("Uso: JOG <eje> <grados>\n"); return; }
        if (!motion_allowed()) return;
        Setpoint_t sp = {MOTION_JOG, f1, 0, s_speed, (uint8_t)ax};
        if (submit(sp)) con_printf("OK JOG J%d %+.2f°\n", ax + 1, f1);
        return;
    }
    if (!strcmp(cmd, "JOGC")) {
        if (!a1 || !parse_f(a2, f1) || (strcmp(a1, "X") && strcmp(a1, "Y"))) {
            con_printf("Uso: JOGC <X|Y> <mm>\n"); return;
        }
        if (!motion_allowed()) return;
        const bool isx = !strcmp(a1, "X");
        Setpoint_t sp = {MOTION_JOGC, isx ? f1 : 0.0f, isx ? 0.0f : f1, s_speed, 0};
        if (submit(sp)) con_printf("OK JOGC %s %+.1f mm\n", a1, f1);
        return;
    }
    if (!strcmp(cmd, "MOVJ")) {
        if (!parse_f(a1, f1) || !parse_f(a2, f2)) { con_printf("Uso: MOVJ <q1> <q2>\n"); return; }
        if (!kin_joints_valid(f1, f2)) { con_printf("ERR fuera de limites articulares\n"); return; }
        if (!motion_allowed()) return;
        Setpoint_t sp = {MOTION_MOVJ, f1, f2, s_speed, 0};
        if (submit(sp)) con_printf("OK MOVJ q1=%.2f q2=%.2f\n", f1, f2);
        return;
    }
    if (!strcmp(cmd, "MOVL")) {
        float q1, q2;
        if (!parse_f(a1, f1) || !parse_f(a2, f2)) { con_printf("Uso: MOVL <x> <y>\n"); return; }
        if (!kin_inverse(f1, f2, ELBOW_UP, q1, q2)) { con_printf("ERR %s\n", kin_last_error()); return; }
        if (!motion_allowed()) return;
        Setpoint_t sp = {MOTION_MOVL, f1, f2, s_speed, 0};
        if (submit(sp)) con_printf("OK MOVL x=%.1f y=%.1f (q1=%.2f q2=%.2f)\n", f1, f2, q1, q2);
        return;
    }
    if (!strcmp(cmd, "PID")) {
        int ax = parse_axis(a1);
        if (ax < 0 || !parse_f(a2, f1) || !parse_f(a3, f2) || !parse_f(a4, f3) || f1 < 0 || f2 < 0 || f3 < 0) {
            con_printf("Uso: PID <eje> <kp> <ki> <kd>\n"); return;
        }
        state_lock();
        g_state.gains[ax].kp = f1; g_state.gains[ax].ki = f2; g_state.gains[ax].kd = f3;
        g_state.req_gains[ax] = true;
        state_unlock();
        con_printf("OK PID J%d kp=%.3f ki=%.3f kd=%.3f (SAVE para guardar)\n", ax + 1, f1, f2, f3);
        return;
    }
    if (!strcmp(cmd, "FF")) {
        int ax = parse_axis(a1);
        if (ax < 0 || !parse_f(a2, f1) || f1 < 0) { con_printf("Uso: FF <eje> <kff>\n"); return; }
        state_lock(); g_state.gains[ax].kff = f1; g_state.req_gains[ax] = true; state_unlock();
        con_printf("OK FF J%d kff=%.3f\n", ax + 1, f1);
        return;
    }
    if (!strcmp(cmd, "DB")) {
        int ax = parse_axis(a1);
        if (ax < 0 || !parse_f(a2, f1) || !parse_f(a3, f2) || f1 < 0 || f2 < 0 || f2 > 500) {
            con_printf("Uso: DB <eje> <zona_muerta_grados> <pwm_min 0..500>\n"); return;
        }
        state_lock();
        g_state.gains[ax].deadband = f1; g_state.gains[ax].pwm_min = (int)f2;
        g_state.req_gains[ax] = true;
        state_unlock();
        con_printf("OK DB J%d zona muerta=%.2f° pwm_min=%d\n", ax + 1, f1, (int)f2);
        return;
    }
    if (!strcmp(cmd, "LIMIT")) {
        if (!parse_f(a1, f1) || f1 < 100 || f1 > PWM_MAX) { con_printf("Uso: LIMIT <100..1023>\n"); return; }
        state_lock(); g_state.pwm_limit = (int)f1; state_unlock();
        con_printf("OK LIMIT %d (%d%%)\n", (int)f1, (int)f1 * 100 / PWM_MAX);
        return;
    }
    if (!strcmp(cmd, "SPEED")) {
        if (!parse_f(a1, f1) || f1 < 1 || f1 > 100) { con_printf("Uso: SPEED <1-100>\n"); return; }
        s_speed = (uint16_t)f1;
        con_printf("OK SPEED %u%% (%.0f °/s articular, %.0f mm/s lineal)\n",
                   s_speed, VMAX_JOINT_DEG_S * s_speed / 100.0f, VMAX_LINEAR_MM_S * s_speed / 100.0f);
        return;
    }
    if (!strcmp(cmd, "ACCEL")) {
        if (!parse_f(a1, f1) || f1 < ACCEL_MIN_DEG_S2 || f1 > ACCEL_MAX_DEG_S2) {
            con_printf("Uso: ACCEL <%.0f..%.0f> (°/s2)\n", ACCEL_MIN_DEG_S2, ACCEL_MAX_DEG_S2); return;
        }
        state_lock(); g_state.accel = f1; state_unlock();
        con_printf("OK ACCEL %.0f °/s2\n", f1);
        return;
    }
    if (!strcmp(cmd, "TELEM")) {
        if (a1 && !strcmp(a1, "ON"))       telemetry_enable(true);
        else if (a1 && !strcmp(a1, "OFF")) telemetry_enable(false);
        else { con_printf("Uso: TELEM ON|OFF\n"); return; }
        con_printf("OK TELEM %s\n", a1);
        return;
    }
    if (!strcmp(cmd, "RESET")) {
        state_lock(); g_state.req_fault_reset = true; state_unlock();
        con_printf("OK fallas borradas; referencia = posicion actual\n");
        return;
    }
    if (!strcmp(cmd, "SAVE")) {
        PIDGains g[NUM_AXES];
        state_lock(); memcpy(g, g_state.gains, sizeof(g)); state_unlock();
        storage_save_gains(g);
        con_printf("OK ganancias guardadas en NVS\n");
        return;
    }
    if (!strcmp(cmd, "FACTORY")) {
        storage_clear();
        con_printf("OK NVS borrada; reinicie para cargar valores por defecto\n");
        return;
    }
    if (!strcmp(cmd, "FRIC"))  { cmd_fric(a1); return; }
    if (!strcmp(cmd, "ENC"))   { cmd_enc(); return; }
    if (!strcmp(cmd, "SPIN"))  { cmd_spin(a1, a2); return; }
    if (!strcmp(cmd, "CPR"))   { cmd_cpr(a1, a2); return; }
    if (!strcmp(cmd, "TEST"))  { cmd_test(a1, a2); return; }
    if (!strcmp(cmd, "SWEEP")) { cmd_sweep(a1, a2, a3, a4, a5); return; }

    con_printf("ERR comando desconocido '%s' (HELP)\n", cmd);
}

// -----------------------------------------------------------------------------
//  TaskComms
// -----------------------------------------------------------------------------
void task_comms(void *arg) {
    char line[128];
    con_printf("\n=== Brazo 2GDL - control DC con encoder (ESP32) ===\n");
    con_printf("CPR J1=%.0f J2=%.0f | encoder J1=%s J2=%s\n", g_state.cpr[0], g_state.cpr[1],
               ENC_MODE[0] == ENC_QUADRATURE ? "cuadratura x4" : "1 canal x2",
               ENC_MODE[1] == ENC_QUADRATURE ? "cuadratura x4" : "1 canal x2");
    cmd_help();

    TickType_t last_cal_print = 0;
    for (;;) {
        if (read_line(line, sizeof(line))) handle(line);

        // Durante la calibración se muestran las cuentas en vivo
        if (s_cal_axis >= 0 && xTaskGetTickCount() - last_cal_print > pdMS_TO_TICKS(300)) {
            last_cal_print = xTaskGetTickCount();
            int64_t c;
            state_lock(); c = g_state.st[s_cal_axis].counts; state_unlock();
            con_printf("  CAL J%d cuentas: %lld\n", s_cal_axis + 1, (long long)c);
        }
        vTaskDelay(pdMS_TO_TICKS(10));   // cede CPU; 10 ms de latencia es irrelevante
    }
}
