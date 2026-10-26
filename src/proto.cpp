#include "proto.h"
#include "config.h"
#include "types.h"
#include "frame.h"
#include "kinematics.h"
#include "motion.h"
#include "supervisor.h"
#include "telemetry.h"

/* ---------------------------------------------------------------------------
 * Codigos de comando y de respuesta (docs/PROTOCOLO_U2.md, secciones 4 y 5).
 * El pendant de Python define exactamente los mismos valores.
 * -------------------------------------------------------------------------*/
enum : uint8_t {
    CMD_PING        = 0x01,
    CMD_GET_STATUS  = 0x02,
    CMD_GET_DIAG    = 0x03,
    CMD_MODE_TEXT   = 0x04,
    CMD_DEBUG_PAUSE = 0x05,
    CMD_JOG         = 0x10,
    CMD_MOVJ        = 0x11,
    CMD_MOVL        = 0x12,
    CMD_GRIPPER     = 0x13,
    CMD_STOP        = 0x20,
    CMD_ENABLE      = 0x21,
    CMD_DISABLE     = 0x22,
    CMD_SETZERO     = 0x23,
    CMD_CLEAR       = 0x24,

    RSP_PONG        = 0x81,
    RSP_STATUS      = 0x82,
    RSP_DIAG        = 0x83,
    RSP_ACK         = 0x90,
    RSP_NACK        = 0x91,
    RSP_DONE        = 0x92,
    RSP_FAILED      = 0x93
};

#define FLAGS_DIR     0x01
#define FLAGS_NOTIFY  0x02

/* --- Estado del enlace --------------------------------------------------- */
static FrameParser_t g_fp;
static bool          g_binary        = false;
static uint32_t      g_last_valid_ms = 0;
static bool          g_link_tripped  = false;
static uint8_t       g_last_done_seq = 0;

/* --- Contadores de diagnostico (respuesta DIAG) --------------------------- */
static struct {
    uint16_t ok, crc, trama, timeout, desborde, rechazadas;
    uint16_t lat_max_us, lat_last_us;
} g_diag;

/* Los incrementa el callback de errores del UART, que corre en otra tarea */
static volatile uint16_t g_parity_errors   = 0;
static volatile uint16_t g_driver_overflow = 0;

static inline void inc(uint16_t &c) { if (c < 0xFFFF) ++c; }   /* satura, no da la vuelta */

bool proto_binary_mode(void) { return g_binary; }

bool proto_wants_byte(uint8_t b, bool text_line_empty)
{
    return g_binary || fp_busy(&g_fp) || (b == FRAME_SOF && text_line_empty);
}

/* ===========================================================================
 * ENVIO
 * ===========================================================================*/
static void send(uint8_t seq, uint8_t cmd, uint8_t motor, const uint8_t *pl, uint8_t len)
{
    uint8_t buf[FRAME_MAX_SIZE];
    const size_t n = frame_build(buf, sizeof buf, seq, cmd, motor, 0, pl, len);
    if (n) report_write_frame(buf, n);
}

static void send_nack(uint8_t seq, uint8_t cmd, uint8_t motor, ErrCode_t code)
{
    const uint8_t pl[2] = { cmd, (uint8_t)code };
    send(seq, RSP_NACK, motor, pl, 2);
}

/* ===========================================================================
 * RESPUESTA A UNA TRAMA
 * ---------------------------------------------------------------------------
 * Cada manejador rellena la respuesta en vez de enviarla: asi la latencia se
 * mide justo antes de escribir en el puerto, sin incluir el tiempo de
 * transmision, y todas las respuestas salen por un unico sitio.
 * ===========================================================================*/
typedef struct {
    uint8_t   cmd;          /* RSP_*                                   */
    ErrCode_t err;          /* si cmd = RSP_NACK                       */
    uint8_t   pl[FRAME_MAX_PAYLOAD];
    uint8_t   len;
} Reply_t;

static void reply_nack(Reply_t *r, ErrCode_t e) { r->cmd = RSP_NACK; r->err = e; }

static bool enqueue(const Setpoint_t *sp, Reply_t *r)
{
    /* Sin espera: si la cola esta llena se rechaza en el acto. Bloquear aqui
     * congelaria TaskComms y con ella el STOP. */
    if (xQueueSend(setpointQueue, sp, 0) != pdTRUE) { reply_nack(r, ERR_COLA_LLENA); return false; }
    r->cmd = RSP_ACK;
    return true;
}

static Setpoint_t make_sp(MotionType_t type, const Frame_t *f)
{
    Setpoint_t sp = {};
    sp.type      = type;
    sp.speed_pct = 100;
    sp.seq       = f->seq;
    sp.flags     = (f->flags & FLAGS_NOTIFY) ? SP_FLAG_NOTIFY : 0;
    return sp;
}

/* Pasos (lo que viaja en la trama) -> grados (lo que lleva el Setpoint_t). La
 * conversion inversa en TaskMotion recupera exactamente los mismos pasos. */
static inline float steps_to_deg(int32_t s) { return kin_steps_to_deg(s); }

/* Validacion comun a todo movimiento */
static bool check_motion_ready(Reply_t *r, bool need_zero, bool allow_in_fault)
{
    if (!sv_drivers_enabled())                         { reply_nack(r, ERR_DRIVERS_APAGADOS); return false; }
    if (need_zero && !sv_is_homed())                   { reply_nack(r, ERR_SIN_CERO);         return false; }
    if (!allow_in_fault && sv_get_fault() != FAULT_NONE) { reply_nack(r, ERR_FALLO_ACTIVO);   return false; }
    return true;
}

static bool check_speed(uint8_t v, Reply_t *r)
{
    if (v < 1 || v > 100) { reply_nack(r, ERR_FUERA_DE_RANGO); return false; }
    return true;
}

static void build_status(Reply_t *r)
{
    int32_t s1, s2;
    motion_get_steps(&s1, &s2);
    const Point_t p = kin_forward(kin_steps_to_deg(s1), kin_steps_to_deg(s2));

    uint8_t flags = 0;
    if (sv_is_homed())        flags |= 0x01;
    if (sv_drivers_enabled()) flags |= 0x02;
    if (motion_is_busy())     flags |= 0x04;

    uint8_t *b = r->pl;
    b[0] = (uint8_t)sv_get_state();
    b[1] = (uint8_t)sv_get_fault();
    b[2] = flags;
    b[3] = (uint8_t)uxQueueMessagesWaiting(setpointQueue);
    b[4] = g_last_done_seq;
    wr_i32(&b[5],  s1);
    wr_i32(&b[9],  s2);
    wr_i16(&b[13], (int16_t)lroundf(p.x * 10.0f));
    wr_i16(&b[15], (int16_t)lroundf(p.y * 10.0f));
    r->cmd = RSP_STATUS;
    r->len = 17;
}

static void build_diag(Reply_t *r)
{
    const uint16_t v[9] = {
        g_diag.ok, g_diag.crc, g_diag.trama, g_diag.timeout,
        (uint16_t)(g_diag.desborde + g_driver_overflow),
        g_diag.rechazadas, g_parity_errors,
        g_diag.lat_max_us, g_diag.lat_last_us
    };
    for (int i = 0; i < 9; ++i) wr_u16(&r->pl[2 * i], v[i]);
    r->cmd = RSP_DIAG;
    r->len = 18;
}

/* Longitud de payload que exige cada comando; -1 = comando desconocido */
static int expected_len(uint8_t cmd)
{
    switch (cmd) {
    case CMD_PING: case CMD_GET_STATUS: case CMD_GET_DIAG: case CMD_MODE_TEXT:
    case CMD_STOP: case CMD_ENABLE: case CMD_DISABLE: case CMD_CLEAR:  return 0;
    case CMD_DEBUG_PAUSE: return 2;
    case CMD_JOG:         return 4;
    case CMD_MOVJ:        return 9;
    case CMD_MOVL:        return 5;
    case CMD_GRIPPER:     return 3;
    case CMD_SETZERO:     return 8;
    default:              return -1;
    }
}

static void handle_frame(const Frame_t *f, uint32_t now_ms)
{
    const uint32_t t0 = micros();
    inc(g_diag.ok);

    /* Una trama valida es tambien un latido del enlace */
    g_last_valid_ms = now_ms;
    g_link_tripped  = false;

    /* Primera trama valida: de aqui en adelante solo tramas por el puerto */
    if (!g_binary) { g_binary = true; telemetry_set_text_muted(true); }

    Reply_t r = {};
    r.cmd = RSP_ACK;
    bool     to_text  = false;
    uint16_t pause_ms = 0;

    const int need = expected_len(f->cmd);
    const uint8_t mot = f->motor;

    if (need < 0)                    reply_nack(&r, ERR_CMD_DESCONOCIDO);
    else if (f->len != need)         reply_nack(&r, ERR_LONGITUD);
    else if (f->cmd == CMD_JOG ? (mot != 1 && mot != 2) : (mot != 0))
                                     reply_nack(&r, ERR_MOTOR_INVALIDO);
    else switch (f->cmd) {

    case CMD_PING: {
        uint8_t *b = r.pl;
        b[0] = PROTO_VERSION;
        wr_u32(&b[1], SERIAL_BAUD);
        b[5] = SERIAL_DATA_BITS;
        b[6] = SERIAL_PARITY_EVEN ? 1 : 0;
        b[7] = SERIAL_STOP_BITS;
        wr_u16(&b[8], (uint16_t)STEPS_PER_JOINT_REV);
        r.cmd = RSP_PONG;
        r.len = 10;
        break;
    }

    case CMD_GET_STATUS: build_status(&r); break;
    case CMD_GET_DIAG:   build_diag(&r);   break;
    case CMD_MODE_TEXT:  to_text = true;   break;

    case CMD_DEBUG_PAUSE: {
        /* SOLO PARA LA DEMO del manejo de desbordes: TaskComms deja de leer el
         * puerto durante N ms mientras el pendant lo inunda. Se rechaza con
         * el robot en movimiento, porque durante la pausa no llegaria un STOP
         * por el enlace. */
        const uint16_t ms = rd_u16(f->payload);
        if (ms < 1 || ms > 2000 || motion_is_busy()) reply_nack(&r, ERR_FUERA_DE_RANGO);
        else pause_ms = ms;
        break;
    }

    case CMD_JOG: {
        /* El caso literal de la rubrica para steppers: conteo de pasos en el
         * payload y direccion en FLAGS.DIR. Se acepta con un fallo pendiente:
         * es la forma de salir de un limite. */
        if (!check_motion_ready(&r, false, true)) break;
        const uint32_t steps = rd_u32(f->payload);
        if (steps < 1 || steps > JOG_MAX_STEPS) { reply_nack(&r, ERR_FUERA_DE_RANGO); break; }
        Setpoint_t sp = make_sp(MOTION_JOG, f);
        sp.axis_id  = mot;
        sp.target_a = steps_to_deg((int32_t)steps) * ((f->flags & FLAGS_DIR) ? -1.0f : 1.0f);
        enqueue(&sp, &r);
        break;
    }

    case CMD_MOVJ: {
        if (!check_motion_ready(&r, true, false)) break;
        const float q1 = steps_to_deg(rd_i32(&f->payload[0]));
        const float q2 = steps_to_deg(rd_i32(&f->payload[4]));
        if (!check_speed(f->payload[8], &r)) break;
        /* Se valida AQUI para responder NACK en el acto. TaskMotion vuelve a
         * validar al ejecutar, por si algo cambio mientras estaba en cola. */
        if (!kin_joints_in_limits(q1, q2)) { reply_nack(&r, ERR_FUERA_DE_RANGO); break; }
        const Point_t p = kin_forward(q1, q2);
        if (kin_point_in_workspace(p.x, p.y) != IK_OK) { reply_nack(&r, ERR_FUERA_DE_RANGO); break; }
        Setpoint_t sp = make_sp(MOTION_MOVJ, f);
        sp.target_a = q1; sp.target_b = q2; sp.speed_pct = f->payload[8];
        enqueue(&sp, &r);
        break;
    }

    case CMD_MOVL: {
        if (!check_motion_ready(&r, true, false)) break;
        const float x = rd_i16(&f->payload[0]) / 10.0f;
        const float y = rd_i16(&f->payload[2]) / 10.0f;
        if (!check_speed(f->payload[4], &r)) break;
        /* Solo se puede validar el DESTINO: el inicio de la recta es donde
         * termine el movimiento anterior de la cola, que aun no se conoce. La
         * recta completa la valida TaskMotion al ejecutar, y si falla avisa
         * con FAILED (TRAYECTORIA o RAMA_CODO). */
        if (kin_inverse(x, y, motion_get_elbow(), NULL) != IK_OK) { reply_nack(&r, ERR_FUERA_DE_RANGO); break; }
        Setpoint_t sp = make_sp(MOTION_MOVL, f);
        sp.target_a = x; sp.target_b = y; sp.speed_pct = f->payload[4];
        enqueue(&sp, &r);
        break;
    }

    case CMD_GRIPPER:
        reply_nack(&r, ERR_NO_IMPLEMENTADO);        /* llega en la U3 */
        break;

    case CMD_STOP:
        /* Fuera de banda, como en la U1: no se encola */
        sv_request_abort(FAULT_ESTOP);
        break;

    case CMD_ENABLE:  sv_cmd_enable();  break;
    case CMD_DISABLE: sv_cmd_disable(); break;
    case CMD_CLEAR:   sv_cmd_clear();   break;

    case CMD_SETZERO: {
        if (!check_motion_ready(&r, false, true)) break;
        const float q1 = steps_to_deg(rd_i32(&f->payload[0]));
        const float q2 = steps_to_deg(rd_i32(&f->payload[4]));
        if (!kin_joints_in_limits(q1, q2)) { reply_nack(&r, ERR_FUERA_DE_RANGO); break; }
        const Point_t p = kin_forward(q1, q2);
        if (kin_point_in_workspace(p.x, p.y) != IK_OK) { reply_nack(&r, ERR_FUERA_DE_RANGO); break; }
        Setpoint_t sp = make_sp(MOTION_SETZERO, f);
        sp.target_a = q1; sp.target_b = q2;
        enqueue(&sp, &r);
        break;
    }
    }

    /* Latencia: desde que se completo la trama hasta tener la respuesta lista
     * (para un movimiento, el setpoint ya esta en la cola). */
    const uint32_t lat = micros() - t0;
    g_diag.lat_last_us = (uint16_t)(lat > 0xFFFF ? 0xFFFF : lat);
    if (g_diag.lat_last_us > g_diag.lat_max_us) g_diag.lat_max_us = g_diag.lat_last_us;

    if (r.cmd == RSP_NACK) {
        inc(g_diag.rechazadas);
        send_nack(f->seq, f->cmd, mot, r.err);
    } else if (r.cmd == RSP_ACK) {
        const uint8_t pl[1] = { f->cmd };
        send(f->seq, RSP_ACK, mot, pl, 1);
    } else {
        send(f->seq, r.cmd, mot, r.pl, r.len);
    }

    if (to_text) { g_binary = false; telemetry_set_text_muted(false); report_line("OK MODE_TEXT"); }
    if (pause_ms) vTaskDelay(pdMS_TO_TICKS(pause_ms));
}

/* ===========================================================================
 * ERRORES DEL PARSER
 * ===========================================================================*/
static void handle_parser_error(FpResult_t r)
{
    ErrCode_t code;
    switch (r) {
    case FP_ERR_CRC:     inc(g_diag.crc);     code = ERR_CRC;     break;
    case FP_ERR_FRAMING: inc(g_diag.trama);   code = ERR_TRAMA;   break;
    case FP_ERR_TIMEOUT: inc(g_diag.timeout); code = ERR_TIMEOUT; break;
    default: return;
    }
    /* El SEQ y el CMD son los que se alcanzaron a leer: pueden venir
     * corruptos, la PC los toma como orientativos. */
    send_nack(g_fp.err_seq, g_fp.f.cmd, g_fp.f.motor, code);
}

void proto_feed(uint8_t b, uint32_t now_ms)
{
    Frame_t f;
    const FpResult_t r = fp_feed(&g_fp, b, now_ms, &f);
    if (r == FP_FRAME) handle_frame(&f, now_ms);
    else if (r != FP_NONE) handle_parser_error(r);
}

void proto_note_overflow(void)
{
    inc(g_diag.desborde);
    /* Se perdieron bytes a media corriente: lo que hubiera a medias ya no
     * sirve. Se reinicia el parser y se avisa. */
    fp_init(&g_fp);
    send_nack(0xFF, 0, 0, ERR_DESBORDE_RX);
}

/* ===========================================================================
 * TICK: timeout de trama, watchdog del enlace y avisos de TaskMotion
 * ===========================================================================*/
void proto_tick(uint32_t now_ms)
{
    /* 1) Trama a medias y la PC callada: no esperar para siempre */
    const FpResult_t r = fp_poll_timeout(&g_fp, now_ms);
    if (r != FP_NONE) handle_parser_error(r);

    /* 2) WATCHDOG DEL ENLACE. En modo binario, con el robot moviendose, si no
     *    llega ninguna trama valida en LINK_TIMEOUT_MS se asume que la PC se
     *    cayo (el pendant se cerro, se colgo, se desconecto el cable) y se
     *    para con rampa. Un robot que sigue ejecutando ordenes de un programa
     *    que ya no existe es justo lo que hay que evitar. */
    if (g_binary && !g_link_tripped && motion_is_busy() &&
        (uint32_t)(now_ms - g_last_valid_ms) > LINK_TIMEOUT_MS) {
        sv_request_abort(FAULT_LINK_LOST);
        g_link_tripped = true;
    }

    /* 3) Avisos DONE / FAILED de TaskMotion -> tramas */
    MotionEvent_t ev;
    while (motionEventQueue && xQueueReceive(motionEventQueue, &ev, 0) == pdTRUE) {
        if (ev.kind == MEV_DONE) g_last_done_seq = ev.seq;
        if (!g_binary) continue;                  /* en modo texto ya se imprimio */
        if (ev.kind == MEV_DONE) {
            send(ev.seq, RSP_DONE, 0, NULL, 0);
        } else {
            const uint8_t pl[1] = { ev.code };
            send(ev.seq, RSP_FAILED, 0, pl, 1);
        }
    }
}

/* ===========================================================================
 * INICIALIZACION
 * ===========================================================================*/
#if PROTO_COUNT_PARITY_ERRORS && defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 2)
#define PROTO_HAS_RX_ERROR_CB 1
/* Lo invoca el driver del UART desde su propia tarea. Solo cuenta. */
static void on_uart_error(hardwareSerial_error_t e)
{
    if (e == UART_PARITY_ERROR) {
        if (g_parity_errors < 0xFFFF) g_parity_errors = g_parity_errors + 1;
    } else if (e == UART_BUFFER_FULL_ERROR || e == UART_FIFO_OVF_ERROR) {
        if (g_driver_overflow < 0xFFFF) g_driver_overflow = g_driver_overflow + 1;
    }
}
#endif

void proto_init(void)
{
    fp_init(&g_fp);
    g_diag = {};
#ifdef PROTO_HAS_RX_ERROR_CB
    Serial.onReceiveError(on_uart_error);
#endif
}
