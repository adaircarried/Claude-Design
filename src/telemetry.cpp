#include "telemetry.h"
#include "config.h"
#include "motion.h"
#include "kinematics.h"
#include "supervisor.h"
#include <stdarg.h>

static SemaphoreHandle_t g_serial_mtx = NULL;
static volatile bool     g_stream     = false;

void telemetry_init(void)
{
    g_serial_mtx = xSemaphoreCreateMutex();
}

/* ---------------------------------------------------------------------------
 * Salida serial serializada.
 *
 * El mutex protege contra el entrelazado de lineas entre TaskComms y
 * TaskTelemetry, que corren las dos en el nucleo 0. El timeout de 100 ms
 * evita que una tarea se quede colgada para siempre si la otra muriera: se
 * prefiere perder una linea de telemetria antes que bloquear el sistema.
 *
 * Nota de escala: Serial.printf bloquea cuando el buffer de transmision se
 * llena. A 115200 baudios eso son ~11.5 caracteres por milisegundo. Por eso
 * nada de esto se llama desde TaskMotion.
 * -------------------------------------------------------------------------*/
/* ---------------------------------------------------------------------------
 * Traduce LF suelto a CR+LF al escribir.
 *
 * Un terminal serial es un teletipo, no una consola del sistema operativo:
 *      '\n' (LF) baja una linea, pero deja el cursor en la MISMA columna
 *      '\r' (CR) es lo que lo devuelve a la columna 0
 * Sin la traduccion, cada linea empieza donde termino la anterior y el texto
 * se pisa unos renglones con otros.
 *
 * Serial.println() ya inserta CR+LF, por eso report_line() siempre se vio
 * bien; printf no lo hace, y por eso report_printf() no lo hacia. Se traduce
 * aqui, en un unico punto, para que quien llame siga escribiendo "\n" a secas
 * y no haya que acordarse en cada una de las cuarenta cadenas del proyecto.
 *
 * La comprobacion de p[-1] evita duplicar el CR si alguien ya escribio CR+LF.
 * -------------------------------------------------------------------------*/
static void serial_write_crlf(const char *s)
{
    for (const char *p = s; *p; ++p) {
        if (*p == '\n' && (p == s || p[-1] != '\r')) Serial.write('\r');
        Serial.write(*p);
    }
}

void report_printf(const char *fmt, ...)
{
    char buf[192];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (g_serial_mtx && xSemaphoreTake(g_serial_mtx, pdMS_TO_TICKS(100)) == pdTRUE) {
        serial_write_crlf(buf);
        xSemaphoreGive(g_serial_mtx);
    }
}

void report_line(const char *s)
{
    if (g_serial_mtx && xSemaphoreTake(g_serial_mtx, pdMS_TO_TICKS(100)) == pdTRUE) {
        Serial.println(s);
        xSemaphoreGive(g_serial_mtx);
    }
}

void telemetry_set_stream(bool on) { g_stream = on; }
bool telemetry_get_stream(void)    { return g_stream; }

/* ---------------------------------------------------------------------------
 * STATUS: reporte completo. Formato de una clave por linea, pensado para ser
 * legible a ojo en el Parcial 1 y trivial de parsear en el Parcial 2.
 * -------------------------------------------------------------------------*/
void telemetry_status_report(void)
{
    float q1, q2; int32_t s1, s2;
    motion_get_joints(&q1, &q2);
    motion_get_steps(&s1, &s2);
    const Point_t p = kin_forward(q1, q2);

    report_printf("--- STATUS ---\n");
    report_printf("estado    : %s\n", sv_state_name(sv_get_state()));
    report_printf("fallo     : %s\n", sv_fault_name(sv_get_fault()));
    report_printf("referencia: %s\n", sv_is_homed() ? "SI" : "NO");
    report_printf("drivers   : %s\n", sv_drivers_enabled() ? "ENERGIZADOS" : "LIBRES (el brazo cae)");
    report_printf("q1        : %+8.3f deg  (%ld pasos)\n", q1, (long)s1);
    report_printf("q2        : %+8.3f deg  (%ld pasos)\n", q2, (long)s2);
    report_printf("efector   : X=%+8.2f mm  Y=%+8.2f mm  R=%.2f mm\n",
                  p.x, p.y, sqrtf(p.x * p.x + p.y * p.y));
    report_printf("codo      : %s\n", motion_get_elbow() == ELBOW_UP ? "ARRIBA" : "ABAJO");
    report_printf("velocidad : %u %%\n", (unsigned)motion_get_speed_pct());
    report_printf("finales   : A1=%s A2=%s\n",
                  sv_limit_raw(1) ? "PISADO" : "libre",
                  sv_limit_raw(2) ? "PISADO" : "libre");

    /* --- DIAGNOSTICO (criterio de 10 puntos del Parcial 1) --------------- */
    report_printf("alcance   : R efectivo=[%.1f, %.1f] mm (configurado=[%.0f, %.0f])\n",
                  (double)kin_effective_r_min(), (double)kin_effective_r_max(),
                  (double)WS_R_MIN_MM, (double)WS_R_MAX_MM);
    report_printf("resolucion: %.4f pasos/deg  (%.4f deg/paso)\n",
                  (double)STEPS_PER_DEG, (double)DEG_PER_STEP);
    report_printf("backlash  : %.2f deg = %.1f pasos = %.2f mm a R=%.0f\n",
                  (double)BACKLASH_DEG, (double)(BACKLASH_DEG * STEPS_PER_DEG),
                  (double)(BACKLASH_DEG * 3.14159265f / 180.0f * WS_R_MAX_MM),
                  (double)WS_R_MAX_MM);
    report_printf("heap libre: %u bytes\n", (unsigned)ESP.getFreeHeap());
    report_printf("uptime    : %lu s\n", (unsigned long)(millis() / 1000));
    /* La temperatura del TB6600 no es medible por software: los drivers no
     * tienen telemetria. Se vigila con el dorso de la mano o un termopar,
     * y se ajusta con el potenciometro de corriente (ver README). */
    report_printf("--- FIN ---\n");
}

/* ===========================================================================
 * TaskTelemetry  -  nucleo 0, prioridad 1, periodo 100 ms
 * ===========================================================================
 * La prioridad mas baja del sistema, y en el nucleo 0: la telemetria nunca
 * debe competir con el movimiento. Si el sistema esta cargado, esta tarea se
 * retrasa y no pasa nada.
 * -------------------------------------------------------------------------*/
void TaskTelemetry(void *pv)
{
    (void)pv;
    TickType_t last = xTaskGetTickCount();

    for (;;) {
        if (g_stream) {
            float q1, q2;
            motion_get_joints(&q1, &q2);
            const Point_t p = kin_forward(q1, q2);
            report_printf("TLM %s %.3f %.3f %.2f %.2f %d\n",
                          sv_state_name(sv_get_state()), q1, q2, p.x, p.y,
                          (int)sv_get_fault());
        }
        vTaskDelayUntil(&last, pdMS_TO_TICKS(TELEMETRY_PERIOD_MS));
    }
}

/* --- Nombres legibles (viven aqui porque solo los usa el reporte) -------- */
const char *sv_state_name(RobotState_t s)
{
    switch (s) {
    case STATE_BOOT:     return "BOOT";
    case STATE_UNHOMED:  return "SIN_REFERENCIA";
    case STATE_HOMING:   return "REFERENCIANDO";
    case STATE_IDLE:     return "LISTO";
    case STATE_MOVING:   return "EN_MOVIMIENTO";
    case STATE_STOPPED:  return "DETENIDO";
    case STATE_DISABLED: return "DRIVERS_LIBRES";
    case STATE_FAULT:    return "FALLO";
    }
    return "?";
}

const char *sv_fault_name(FaultCode_t f)
{
    switch (f) {
    case FAULT_NONE:           return "ninguno";
    case FAULT_LIMIT_HIT:      return "FINAL_DE_CARRERA";
    case FAULT_HOMING_TIMEOUT: return "HOMING_SIN_EXITO";
    case FAULT_SOFT_LIMIT:     return "LIMITE_ARTICULAR";
    case FAULT_WORKSPACE:      return "FUERA_DE_ESPACIO";
    case FAULT_ESTOP:          return "PARO_USUARIO";
    case FAULT_QUEUE_FULL:     return "COLA_LLENA";
    case FAULT_RX_OVERFLOW:    return "DESBORDE_RX";
    }
    return "?";
}
