/* =============================================================================
 *  BRAZO ROBOTICO RR DE 2 GDL  -  Conecta 4
 *  Control de Robots  -  Parcial 1
 * =============================================================================
 *
 *  ARQUITECTURA EN UNA PANTALLA
 *  ----------------------------
 *
 *      NUCLEO 0 (donde vive el stack de WiFi)      NUCLEO 1 (movimiento puro)
 *      ---------------------------------------     --------------------------
 *      TaskComms    prio 2  <-- consola serial     TaskSupervisor  prio 6
 *      TaskTelemetry prio 1 --> reporte                |  vigila limites y paro
 *          |                                       TaskMotion      prio 5
 *          |                                           |  consume la cola
 *          |          setpointQueue (8)                |
 *          +------------------------------------------>+
 *                                                      |
 *                                          FastAccelStepper (RMT/MCPWM)
 *                                                      |
 *                                              TB6600 x2 -> NEMA17 x2
 *
 *  EL CONTRATO: nadie mueve motores llamando a funciones. Todo el mundo
 *  ENCOLA Setpoint_t. TaskMotion no sabe ni le importa quien encolo.
 *
 *      Parcial 1 -> la cola la llena la consola de texto (comms.cpp)
 *      Parcial 2 -> la llena el parser binario con CRC   (comms.cpp reescrito)
 *      Parcial 3 -> la llena la logica de juego          (modulo nuevo)
 *
 *  Si en el Parcial 2 o 3 hace falta modificar motion.cpp para agregar una
 *  fuente de comandos, es señal de que algo se esta haciendo mal.
 * ---------------------------------------------------------------------------*/

#include <Arduino.h>
#include "config.h"
#include "types.h"
#include "motion.h"
#include "comms.h"
#include "supervisor.h"
#include "telemetry.h"
#include "persistence.h"
#include "kinematics.h"
#include "proto.h"

static void print_banner(void)
{
    report_line("");
    report_line("=====================================================");
    report_line(" Brazo RR 2 GDL - Conecta 4 - Parcial 1");
    report_line("=====================================================");
    report_printf(" l1=%.1f mm  l2=%.1f mm\n", (double)LINK_L1_MM, (double)LINK_L2_MM);
    report_printf(" resolucion: %.4f micropasos/grado (%.0f por vuelta)\n",
                  (double)STEPS_PER_DEG, (double)STEPS_PER_JOINT_REV);
    report_printf(" anillo configurado: R=[%.0f, %.0f] mm, Y>=%.0f mm\n",
                  (double)WS_R_MIN_MM, (double)WS_R_MAX_MM, (double)WS_Y_MIN_MM);
    /* ATENCION: el alcance EFECTIVO puede ser mas estrecho que el configurado.
     * Con q2 acotado a +/-135 grados el codo no se cierra del todo y el radio
     * minimo real sube de 60 a 141.7 mm. Se reporta al arrancar para que no
     * sea una sorpresa al enseñar las poses del tablero. */
    report_printf(" ALCANCE EFECTIVO: R=[%.1f, %.1f] mm  <- lo que manda\n",
                  (double)kin_effective_r_min(), (double)kin_effective_r_max());
    report_printf(" limites: q1=[%.0f,%.0f] q2=[%.0f,%.0f] grados\n",
                  (double)A1_MIN_DEG, (double)A1_MAX_DEG,
                  (double)A2_MIN_DEG, (double)A2_MAX_DEG);
    report_line("-----------------------------------------------------");
    report_line(" Los drivers arrancan LIBRES. Secuencia de arranque:");
    if (HOMING_MODE == HOMING_SWITCHES) {
        report_line("   1) ENABLE     energiza los motores");
        report_line("   2) HOME       referencia (el codo va primero)");
    } else {
        report_line("   1) lleva el brazo a la pose de referencia");
        report_printf("      (q1=%.0f, q2=%.0f: brazo horizontal, antebrazo colgando)\n",
                      (double)ZERO_POSE_Q1_DEG, (double)ZERO_POSE_Q2_DEG);
        report_line("   2) ENABLE     energiza: los motores sostienen la pose");
        report_line("   3) SETZERO    esa pose pasa a ser la referencia");
    }
    report_line("   despues ya puedes usar MOVJ / MOVL. JOG funciona sin referencia.");
    report_line(" Protocolo binario U2: 115200 8E1. La primera trama valida");
    report_line(" apaga este texto; MODE_TEXT o un reinicio lo regresan.");
    report_line(" HELP para la lista completa de comandos.");
    report_line("=====================================================");
}

void setup(void)
{
    /* Bus de la U2: 115200 8E1. El buffer del driver se agranda ANTES de
     * begin(), que es cuando se reserva. */
    Serial.setRxBufferSize(SERIAL_RX_DRIVER_BUF);
    Serial.begin(SERIAL_BAUD, SERIAL_PARITY_EVEN ? SERIAL_8E1 : SERIAL_8N1);
    delay(300);                 /* deja que el monitor serial se enganche */

    telemetry_init();           /* crea el mutex del serial: PRIMERO, porque
                                   todo lo demas reporta a traves de el     */
    persist_init();
    supervisor_init();          /* deja ENA en estado LIBRE                 */
    comms_init();

    /* La cola se crea ANTES que las tareas: TaskComms podria intentar
     * encolar en su primer tick. */
    setpointQueue    = xQueueCreate(SETPOINT_QUEUE_LEN, sizeof(Setpoint_t));
    motionEventQueue = xQueueCreate(MOTION_EVENT_QUEUE_LEN, sizeof(MotionEvent_t));
    if (!setpointQueue || !motionEventQueue) {
        report_line("ERR FATAL: no se pudieron crear las colas");
        for (;;) delay(1000);
    }
    proto_init();

    motion_init();

    uint16_t      saved_speed;
    ElbowConfig_t saved_elbow;
    persist_load_config(&saved_speed, &saved_elbow);
    motion_set_speed_pct(saved_speed);
    motion_set_elbow(saved_elbow);

    /* -------------------------------------------------------------------
     * Creacion de tareas ANCLADAS POR NUCLEO.
     *
     * xTaskCreatePinnedToCore y no xTaskCreate: sin anclar, el planificador
     * puede migrar una tarea entre nucleos y el aislamiento entre red y
     * movimiento deja de existir. El anclaje es el que hace que una rafaga
     * de comunicacion no produzca jitter en las rampas.
     * ------------------------------------------------------------------ */
    xTaskCreatePinnedToCore(TaskSupervisor, "Supervisor", TASK_SUPERVISOR_STACK,
                            NULL, TASK_SUPERVISOR_PRIO, NULL, CORE_MOTION);
    xTaskCreatePinnedToCore(TaskMotion,     "Motion",     TASK_MOTION_STACK,
                            NULL, TASK_MOTION_PRIO,     NULL, CORE_MOTION);
    xTaskCreatePinnedToCore(TaskComms,      "Comms",      TASK_COMMS_STACK,
                            NULL, TASK_COMMS_PRIO,      NULL, CORE_COMMS);
    xTaskCreatePinnedToCore(TaskTelemetry,  "Telemetry",  TASK_TELEMETRY_STACK,
                            NULL, TASK_TELEMETRY_PRIO,  NULL, CORE_COMMS);

    sv_set_state(STATE_UNHOMED);
    print_banner();

    /* El robot NO hace home solo al arrancar, a proposito. Si el brazo quedo
     * en una pose comprometida (apoyado en el tablero, contra un tope), un
     * homing automatico al energizar empujaria contra el obstaculo antes de
     * que nadie pueda reaccionar. El operador energiza y referencia cuando
     * ha comprobado que el camino esta libre. */
}

/* ---------------------------------------------------------------------------
 * loop() corre en loopTask: nucleo 1, prioridad 1. No hace nada porque todo
 * el trabajo vive en las cuatro tareas de arriba.
 *
 * El vTaskDelay NO es decorativo: loopTask tiene mayor prioridad que la tarea
 * idle del nucleo 1, y esa tarea idle es la que alimenta el watchdog. Un
 * loop() vacio sin delay dispara "Task watchdog got triggered (IDLE1)" y
 * reinicia el ESP32 en unos cinco segundos.
 * -------------------------------------------------------------------------*/
void loop(void)
{
    vTaskDelay(pdMS_TO_TICKS(1000));
}
