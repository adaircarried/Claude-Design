#include "comms.h"
#include "config.h"
#include "types.h"
#include "motion.h"
#include "kinematics.h"
#include "supervisor.h"
#include "telemetry.h"
#include "persistence.h"
#include <string.h>
#include <stdlib.h>

/* ===========================================================================
 * BUFFER CIRCULAR DE RECEPCION
 * ===========================================================================
 * Desacopla la llegada de bytes de su interpretacion. TaskComms vacia el FIFO
 * del UART hacia este anillo en cada tick y luego extrae lineas completas
 * cuando las hay. El FIFO por hardware del ESP32 tiene solo 128 bytes: si no
 * se vaciara con regularidad, se perderian bytes en silencio.
 *
 * El desbordamiento se DETECTA y se REPORTA en vez de sobrescribir datos en
 * silencio. En el Parcial 2 esto es un criterio evaluado ("manejo de errores
 * como desbordamiento de buffer sin que el firmware se caiga").
 * -------------------------------------------------------------------------*/
typedef struct {
    uint8_t  buf[RX_RING_SIZE];
    uint16_t head;
    uint16_t tail;
    bool     overflow;
} RingBuffer_t;

static RingBuffer_t g_rx;

static inline uint16_t ring_count(const RingBuffer_t *r)
{
    return (uint16_t)((r->head - r->tail) & (uint16_t)(RX_RING_SIZE - 1));
}

static bool ring_put(RingBuffer_t *r, uint8_t b)
{
    const uint16_t next = (uint16_t)((r->head + 1) & (RX_RING_SIZE - 1));
    if (next == r->tail) { r->overflow = true; return false; }   /* lleno */
    r->buf[r->head] = b;
    r->head = next;
    return true;
}

static bool ring_get(RingBuffer_t *r, uint8_t *out)
{
    if (r->head == r->tail) return false;
    *out = r->buf[r->tail];
    r->tail = (uint16_t)((r->tail + 1) & (RX_RING_SIZE - 1));
    return true;
}

/* RX_RING_SIZE debe ser potencia de dos: el enmascarado de arriba depende de
 * ello y es lo que hace que el anillo no necesite divisiones. */
static_assert((RX_RING_SIZE & (RX_RING_SIZE - 1)) == 0,
              "RX_RING_SIZE debe ser potencia de dos");

/* ===========================================================================
 * ENCOLADO
 * ===========================================================================
 * El unico punto del modulo que toca la cola. Todo comando de movimiento
 * termina aqui. Si algun dia hay que cambiar la politica (por ejemplo,
 * descartar el mas viejo en vez de rechazar el nuevo), se cambia en un sitio.
 * -------------------------------------------------------------------------*/
static bool enqueue(MotionType_t t, float a, float b, uint8_t axis, int32_t aux)
{
    Setpoint_t sp;
    sp.type      = t;
    sp.target_a  = a;
    sp.target_b  = b;
    sp.speed_pct = motion_get_speed_pct();
    sp.axis_id   = axis;
    sp.aux       = aux;

    /* Timeout 0: no bloquear TaskComms esperando sitio. Si la cola esta
     * llena, el sistema ya tiene 8 movimientos pendientes y lo correcto es
     * rechazar y avisar, no acumular retraso. */
    if (xQueueSend(setpointQueue, &sp, 0) != pdTRUE) {
        sv_set_fault(FAULT_QUEUE_FULL);
        report_line("ERR QUEUE_FULL");
        return false;
    }
    return true;
}

/* ===========================================================================
 * AYUDA
 * ===========================================================================*/
static void print_help(void)
{
    report_line("--- COMANDOS ---");
    report_line("HOME                 referenciado de ambos ejes (codo primero)");
    report_line("JOG <eje> <grados>   mueve un eje, relativo, lento. eje = 1 o 2");
    report_line("MOVJ <q1> <q2>       interpolacion articular, ejes sincronizados");
    report_line("MOVL <x> <y>         interpolacion lineal en mm, efector en recta");
    report_line("SPEED <1-100>        velocidad para MOVJ y MOVL");
    report_line("STOP                 paro inmediato con rampa, mantiene par");
    report_line("STATUS               posicion, estado y diagnostico");
    report_line("TEST <pasos> [ciclos] mide perdida real de pasos (requiere HOME)");
    report_line("ELBOW UP|DOWN        configuracion del codo para la cinematica inversa");
    report_line("TELEM ON|OFF         flujo periodico de telemetria cada 100 ms");
    report_line("ENABLE / DISABLE     energiza / libera los drivers");
    report_line("                     CUIDADO: DISABLE deja caer el brazo");
    report_line("CLEAR                borra el fallo actual");
    report_line("SAVE                 guarda velocidad y codo en memoria no volatil");
    report_line("HELP                 esta ayuda");
    report_line("--- FIN ---");
}

/* ===========================================================================
 * PARSER DE LINEA
 * ===========================================================================
 * Texto plano, un comando por linea, terminada en \n o \r. Deliberadamente
 * simple: en el Parcial 1 lo importante es poder teclear comandos a mano
 * mientras se pone a punto el hardware.
 *
 * Respuestas siempre con prefijo: "OK ...", "ERR <codigo> ...", "EVT ...",
 * "TLM ...". Ese prefijo hace que el Parcial 2 pueda distinguir respuestas de
 * eventos asincronos sin ambiguedad.
 * -------------------------------------------------------------------------*/
static void handle_line(char *line)
{
    while (*line == ' ' || *line == '\t') ++line;
    if (*line == '\0') return;

    char *cmd = strtok(line, " \t");
    if (!cmd) return;
    for (char *p = cmd; *p; ++p) *p = (char)toupper((unsigned char)*p);

    char *a1 = strtok(NULL, " \t");
    char *a2 = strtok(NULL, " \t");

    /* ---- STOP: RUTA FUERA DE BANDA ------------------------------------
     * No se encola. Si se encolara y TaskMotion estuviera a mitad de un MOVJ
     * de 3 segundos, el paro no se atenderia hasta que ese movimiento
     * terminara. sv_request_abort() levanta la bandera que TaskMotion
     * consulta cada 5 ms y ademas vacia la cola de pendientes. */
    if (!strcmp(cmd, "STOP")) {
        sv_request_abort(FAULT_ESTOP);
        report_line("OK STOP");
        return;
    }

    if (!strcmp(cmd, "STATUS")) { telemetry_status_report(); return; }
    if (!strcmp(cmd, "HELP") || !strcmp(cmd, "?")) { print_help(); return; }

    if (!strcmp(cmd, "HOME")) {
        if (!sv_drivers_enabled()) { report_line("ERR DISABLED (usa ENABLE primero)"); return; }
        sv_clear_fault();
        if (enqueue(MOTION_HOME, 0, 0, 0, 0)) report_line("OK HOME encolado");
        return;
    }

    if (!strcmp(cmd, "JOG")) {
        if (!a1 || !a2) { report_line("ERR SINTAXIS: JOG <eje> <grados>"); return; }
        const int   axis = atoi(a1);
        const float deg  = (float)atof(a2);
        if (axis != 1 && axis != 2) { report_line("ERR AXIS (usa 1 o 2)"); return; }
        if (!sv_drivers_enabled())  { report_line("ERR DISABLED"); return; }
        if (enqueue(MOTION_JOG, deg, 0, (uint8_t)axis, 0))
            report_printf("OK JOG eje=%d %.2f deg\n", axis, deg);
        return;
    }

    if (!strcmp(cmd, "MOVJ")) {
        if (!a1 || !a2) { report_line("ERR SINTAXIS: MOVJ <q1> <q2>"); return; }
        const float q1 = (float)atof(a1);
        const float q2 = (float)atof(a2);
        /* Se valida AQUI, en el productor, y no en TaskMotion: asi el usuario
         * recibe el error en el mismo instante en que escribe el comando, en
         * vez de descubrir medio segundo despues que su orden se descarto. */
        if (!kin_joints_in_limits(q1, q2)) {
            report_printf("ERR JOINT_LIMIT q1=[%.0f,%.0f] q2=[%.0f,%.0f]\n",
                          (double)A1_MIN_DEG, (double)A1_MAX_DEG,
                          (double)A2_MIN_DEG, (double)A2_MAX_DEG);
            return;
        }
        if (enqueue(MOTION_MOVJ, q1, q2, 0, 0))
            report_printf("OK MOVJ %.2f %.2f\n", q1, q2);
        return;
    }

    if (!strcmp(cmd, "MOVL")) {
        if (!a1 || !a2) { report_line("ERR SINTAXIS: MOVL <x_mm> <y_mm>"); return; }
        const float x = (float)atof(a1);
        const float y = (float)atof(a2);
        Joints_t j;
        const IkResult_t r = kin_inverse(x, y, motion_get_elbow(), &j);
        if (r != IK_OK) {
            const char *m = "?";
            switch (r) {
            case IK_OUT_OF_REACH: m = "fuera de alcance";         break;
            case IK_TOO_CLOSE:    m = "demasiado cerca del eje";  break;
            case IK_BELOW_TABLE:  m = "por debajo del limite Y";  break;
            case IK_JOINT_LIMIT:  m = "excede limites articulares"; break;
            default: break;
            }
            report_printf("ERR IK %s (codigo %d)\n", m, (int)r);
            return;
        }
        if (enqueue(MOTION_MOVL, x, y, 0, 0))
            report_printf("OK MOVL %.1f %.1f -> q1=%.2f q2=%.2f\n", x, y, j.q1, j.q2);
        return;
    }

    if (!strcmp(cmd, "SPEED")) {
        if (!a1) { report_printf("OK SPEED %u\n", (unsigned)motion_get_speed_pct()); return; }
        const int v = atoi(a1);
        if (v < 1 || v > 100) { report_line("ERR RANGO (1-100)"); return; }
        motion_set_speed_pct((uint16_t)v);
        report_printf("OK SPEED %d\n", v);
        return;
    }

    if (!strcmp(cmd, "TEST")) {
        const int32_t pasos  = a1 ? atol(a1) : (int32_t)(10.0f * STEPS_PER_DEG);
        const int32_t ciclos = a2 ? atol(a2) : 10;
        if (!sv_is_homed()) { report_line("ERR NOT_HOMED (TEST necesita HOME previo)"); return; }
        if (enqueue(MOTION_TEST, (float)pasos, 0, 0, ciclos))
            report_line("OK TEST encolado");
        return;
    }

    if (!strcmp(cmd, "ELBOW")) {
        if (!a1) { report_printf("OK ELBOW %s\n",
                     motion_get_elbow() == ELBOW_UP ? "UP" : "DOWN"); return; }
        for (char *p = a1; *p; ++p) *p = (char)toupper((unsigned char)*p);
        if      (!strcmp(a1, "UP"))   motion_set_elbow(ELBOW_UP);
        else if (!strcmp(a1, "DOWN")) motion_set_elbow(ELBOW_DOWN);
        else { report_line("ERR SINTAXIS: ELBOW UP|DOWN"); return; }
        report_printf("OK ELBOW %s\n", a1);
        return;
    }

    if (!strcmp(cmd, "TELEM")) {
        if (!a1) { report_printf("OK TELEM %s\n", telemetry_get_stream() ? "ON" : "OFF"); return; }
        for (char *p = a1; *p; ++p) *p = (char)toupper((unsigned char)*p);
        telemetry_set_stream(strcmp(a1, "ON") == 0);
        report_printf("OK TELEM %s\n", a1);
        return;
    }

    if (!strcmp(cmd, "ENABLE")) {
        sv_drivers_enable(true);
        if (sv_get_state() == STATE_DISABLED)
            sv_set_state(sv_is_homed() ? STATE_IDLE : STATE_UNHOMED);
        report_line("OK ENABLE");
        return;
    }

    if (!strcmp(cmd, "DISABLE")) {
        /* Liberar los drivers en un brazo vertical significa que el brazo
         * cae. Se para primero y se avisa sin ambiguedad. Ademas se marca
         * como no referenciado: tras caer, la posicion guardada es mentira. */
        sv_request_abort(FAULT_NONE);
        vTaskDelay(pdMS_TO_TICKS(300));
        sv_drivers_enable(false);
        sv_set_homed(false);
        sv_set_state(STATE_DISABLED);
        report_line("OK DISABLE - AVISO: el brazo cae por gravedad, se pierde la referencia");
        return;
    }

    if (!strcmp(cmd, "CLEAR")) {
        sv_clear_fault();
        sv_clear_abort();
        sv_set_state(sv_is_homed() ? STATE_IDLE : STATE_UNHOMED);
        report_line("OK CLEAR");
        return;
    }

    if (!strcmp(cmd, "SAVE")) {
        persist_save_config(motion_get_speed_pct(), motion_get_elbow());
        report_line("OK SAVE");
        return;
    }

    report_printf("ERR UNKNOWN '%s' (escribe HELP)\n", cmd);
}

/* ---------------------------------------------------------------------------
 * Eco local. Va por report_printf() y no por Serial.print() directo para
 * respetar el mutex del puerto: si TaskTelemetry estuviera emitiendo una
 * linea a la vez, los caracteres se entrelazarian a mitad de texto.
 *
 * El coste de tomar el mutex por caracter es despreciable porque la fuente
 * es un humano tecleando, no un flujo de datos.
 * -------------------------------------------------------------------------*/
#if CONSOLE_ECHO
static inline void echo_char(char c)  { report_printf("%c", c); }
static inline void echo_backspace(void) { report_printf("\b \b"); }
static inline void echo_newline(void) { report_printf("\r\n"); }
#else
static inline void echo_char(char)    { }
static inline void echo_backspace(void) { }
static inline void echo_newline(void) { }
#endif

void comms_init(void)
{
    g_rx.head = g_rx.tail = 0;
    g_rx.overflow = false;
}

/* ===========================================================================
 * TaskComms  -  nucleo 0, prioridad 2
 * ===========================================================================
 * En el nucleo 0 (junto al stack de WiFi) y con prioridad baja, porque una
 * rafaga de comunicacion NUNCA debe introducir jitter en el movimiento, que
 * vive entero en el nucleo 1.
 *
 * El tick de 5 ms mantiene la latencia percibida muy por debajo del umbral
 * humano y, a 115200 baudios, garantiza que el FIFO de 128 bytes del UART no
 * llegue a llenarse (5 ms a esa velocidad son 57 bytes).
 * -------------------------------------------------------------------------*/
void TaskComms(void *pv)
{
    (void)pv;
    static char    line[CMD_LINE_MAX];
    static uint8_t idx = 0;

    for (;;) {
        /* 1) UART -> anillo */
        while (Serial.available() > 0) {
            ring_put(&g_rx, (uint8_t)Serial.read());
        }

        if (g_rx.overflow) {
            g_rx.overflow = false;
            g_rx.head = g_rx.tail = 0;        /* descartar todo: hay datos rotos */
            idx = 0;
            sv_set_fault(FAULT_RX_OVERFLOW);
            report_line("ERR RX_OVERFLOW");
        }

        /* 2) anillo -> lineas */
        uint8_t b;
        while (ring_get(&g_rx, &b)) {
            if (b == '\n' || b == '\r') {
                /* Con terminales que envian CR+LF, el segundo caracter llega
                 * con idx ya a cero y se ignora solo: no hay doble proceso. */
                if (idx > 0) {
                    line[idx] = '\0';
                    echo_newline();
                    handle_line(line);
                    idx = 0;
                }
            } else if (b == 0x08 || b == 0x7F) {
                /* Retroceso y suprimir. Poder corregir una errata sin
                 * reescribir el comando entero importa cuando se teclea
                 * "MOVJ -45 90" treinta veces seguidas en el banco. */
                if (idx > 0) { idx--; echo_backspace(); }
            } else if (b >= 0x20 && b < 0x7F && idx < CMD_LINE_MAX - 1) {
                /* Solo imprimibles: un caracter de control colado en la
                 * linea no seria visible pero si romperia el parser. */
                line[idx++] = (char)b;
                echo_char((char)b);
            } else if (idx >= CMD_LINE_MAX - 1) {
                /* Linea mas larga que el maximo: se descarta entera en vez de
                 * procesar un comando truncado, que podria interpretarse como
                 * otro comando distinto. */
                idx = 0;
                report_line("ERR LINE_TOO_LONG");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
