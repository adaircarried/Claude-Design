/* =============================================================================
 * bench_motor.cpp  -  PRUEBA DE BANCO DEL CABLEADO (programa independiente)
 * =============================================================================
 *
 * Este NO es el firmware del robot. Es un programa minimo para comprobar la
 * parte electrica antes de cargar el firmware completo:
 *
 *   - sin FreeRTOS, sin FastAccelStepper, sin supervisor, sin cola
 *   - los pulsos se generan a mano, con un ancho de 10 us (el TB6600 pide un
 *     minimo de unos 2.5 us), a velocidad baja y constante
 *   - usa exactamente los mismos pines que el firmware, tomados de config.h
 *
 * Por que separarlo: si el motor gira con este programa, el cableado, los DIP
 * switches, la fuente y la corriente estan bien. Si despues el firmware
 * completo no lo mueve, el problema ya no es electrico, y se acota la busqueda
 * a la mitad.
 *
 * Se carga con el entorno "bench_motor" de PlatformIO (ver platformio.ini).
 * Monitor a 115200, 8N1. Los comandos son de UNA tecla, sin Enter.
 * ---------------------------------------------------------------------------*/

#include <Arduino.h>
#include "../config.h"

/* Angulo de prueba de las teclas 1-4, elegible con 'g'. Arranca en 10 grados
 * porque el brazo puede estar YA MONTADO: un giro grande en el sentido
 * equivocado lo estrellaria. Con 90 grados (4000 micropasos) se comprueba el
 * micropaso a ojo: un cuarto de vuelta exacto de la salida del reductor (el
 * eje del motor queda oculto dentro del planetario). */
static const float    ANGULOS_PRUEBA[]  = { 10.0f, 45.0f, 90.0f };
static uint8_t        g_ang_idx         = 0;
static const uint32_t PASOS_VUELTA_ART  = 16000;   /* 1 vuelta de articulacion */

/* Arranca en 400 Hz: sin rampa, levantando el brazo contra la gravedad, una
 * velocidad de arranque alta puede ahogar el motor (vibra y no sube). */
static const uint32_t VELOCIDADES_HZ[]  = { 400, 800, 1600, 3200 };
static uint8_t        g_vel_idx         = 0;

static uint32_t pasos_prueba(void)
{
    return (uint32_t)lroundf(ANGULOS_PRUEBA[g_ang_idx] * PASOS_VUELTA_ART / 360.0f);
}
static bool           g_energizado      = false;
static bool           g_dir_nivel       = false;

static void energizar(bool on)
{
    g_energizado = on;
    digitalWrite(PIN_ENABLE, on ? ENABLE_ACTIVE_LEVEL : !ENABLE_ACTIVE_LEVEL);
    Serial.printf("Drivers %s\r\n", on ? "ENERGIZADOS (el eje debe resistirse al girarlo a mano)"
                                       : "LIBRES (el eje gira a mano)");
}

static void menu(void)
{
    Serial.print(
        "\r\n=========== PRUEBA DE BANCO - cableado de los TB6600 ===========\r\n"
        "  1 / 2   motor 1 (hombro, PUL 25 / DIR 26)  +angulo / -angulo\r\n"
        "  3 / 4   motor 2 (codo,   PUL 32 / DIR 33)  +angulo / -angulo\r\n"
        "  g       cambiar el angulo de prueba (10, 45, 90 grados)\r\n"
        "  5 / 6   motor 1 / motor 2: UNA VUELTA COMPLETA de la salida\r\n"
        "          ** SOLO CON EL MOTOR DESMONTADO: con el brazo puesto, choca **\r\n"
        "  e       energizar / liberar los drivers (ENA, GPIO 27)\r\n"
        "  h       sostener con pulsos: prueba de la caida del hombro\r\n"
        "  v       cambiar velocidad\r\n"
        "  d       alternar DIR en ambos drivers (para medir con multimetro)\r\n"
        "  ?       este menu\r\n"
        "----------------------------------------------------------------\r\n"
        "  Con el angulo en 90: un CUARTO de vuelta exacto de la salida.\r\n"
        "  Si ves media vuelta, estas en 1/8; si ves un octavo, en 1/32.\r\n"
        "  Cualquier tecla detiene un movimiento en curso.\r\n"
        "================================================================\r\n");
    Serial.printf("Angulo de prueba: %.0f grados | velocidad: %lu Hz | drivers %s\r\n",
                  (double)ANGULOS_PRUEBA[g_ang_idx], (unsigned long)VELOCIDADES_HZ[g_vel_idx],
                  g_energizado ? "ENERGIZADOS" : "LIBRES");
}

static void mover(const char *nombre, uint8_t pin_pul, uint8_t pin_dir,
                  bool dir_high_up, bool positivo, uint32_t pasos)
{
    if (!g_energizado) {
        Serial.print("Los drivers estan LIBRES: pulsa 'e' para energizar primero\r\n");
        return;
    }
    const uint32_t hz      = VELOCIDADES_HZ[g_vel_idx];
    const uint32_t periodo = 1000000UL / hz;

    Serial.printf("%s: %s%lu pasos a %lu Hz (%.1f s)...\r\n", nombre,
                  positivo ? "+" : "-", (unsigned long)pasos,
                  (unsigned long)hz, (double)pasos / hz);

    /* Mismo convenio que el firmware (A?_DIR_HIGH_COUNTS_UP de config.h):
     * asi la tecla "+" gira hacia el mismo lado aqui y en el robot. */
    digitalWrite(pin_dir, (positivo == dir_high_up) ? HIGH : LOW);
    delayMicroseconds(50);          /* establecimiento de DIR: el TB6600 pide >= 5 us */

    const uint32_t t0 = millis();
    for (uint32_t i = 0; i < pasos; ++i) {
        digitalWrite(pin_pul, HIGH);
        delayMicroseconds(10);      /* ancho de pulso: unas 4 veces el minimo */
        digitalWrite(pin_pul, LOW);
        delayMicroseconds(periodo - 10);

        /* Cada 512 pasos se cede 1 ms a FreeRTOS para que no salte el
         * watchdog en los movimientos largos. A estas velocidades una pausa
         * no pierde pasos: solo alarga ese paso. */
        if ((i & 0x1FF) == 0x1FF) vTaskDelay(1);

        /* Cualquier tecla detiene el movimiento */
        if (Serial.available()) {
            while (Serial.available()) Serial.read();
            Serial.printf("  detenido por teclado tras %lu pasos\r\n", (unsigned long)i + 1);
            return;
        }
    }
    Serial.printf("  listo en %.2f s\r\n", (millis() - t0) / 1000.0);
}

/* ---------------------------------------------------------------------------
 * PRUEBA DE SOSTENIMIENTO CON PULSOS (tecla h)
 * Muchas tarjetas TB6600 bajan la corriente a la mitad cuando dejan de recibir
 * pulsos. Sintoma: el hombro SUBE el brazo a horizontal, pero ya detenido no
 * lo sostiene y lo deja caer despacio, saltando pasos.
 * Aqui se manda, cada 50 ms, un micropaso adelante y uno atras a los DOS
 * motores. La posicion neta no cambia (0.0045 grados en la salida, invisible)
 * y el driver nunca ve el reposo. Si con esto el brazo deja de caer, la causa
 * era esa y el firmware puede hacer lo mismo.
 * -------------------------------------------------------------------------*/
static void pulso(uint8_t pin_pul)
{
    digitalWrite(pin_pul, HIGH); delayMicroseconds(10); digitalWrite(pin_pul, LOW);
}

static void sostener_con_pulsos(void)
{
    if (!g_energizado) {
        Serial.print("Los drivers estan LIBRES: pulsa 'e' para energizar primero\r\n");
        return;
    }
    Serial.print("Sosteniendo con un micropaso de ida y vuelta cada 50 ms.\r\n"
                 "Observa si el brazo sigue cayendo. Cualquier tecla para salir.\r\n");
    const uint32_t t0 = millis();
    while (!Serial.available()) {
        digitalWrite(PIN_A1_DIR, HIGH); digitalWrite(PIN_A2_DIR, HIGH);
        delayMicroseconds(50);
        pulso(PIN_A1_PUL); pulso(PIN_A2_PUL);
        delayMicroseconds(500);
        digitalWrite(PIN_A1_DIR, LOW);  digitalWrite(PIN_A2_DIR, LOW);
        delayMicroseconds(50);
        pulso(PIN_A1_PUL); pulso(PIN_A2_PUL);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    while (Serial.available()) Serial.read();
    Serial.printf("  fin de la prueba tras %.1f s\r\n", (millis() - t0) / 1000.0);
}

void setup(void)
{
    Serial.begin(SERIAL_BAUD);

    /* Primero los pines de potencia en estado seguro: drivers LIBRES y sin
     * pulsos, antes de cualquier otra cosa. */
    pinMode(PIN_ENABLE, OUTPUT);
    digitalWrite(PIN_ENABLE, !ENABLE_ACTIVE_LEVEL);
    const uint8_t salidas[] = { PIN_A1_PUL, PIN_A1_DIR, PIN_A2_PUL, PIN_A2_DIR };
    for (uint8_t p : salidas) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }

    delay(300);
    menu();
}

void loop(void)
{
    if (!Serial.available()) { delay(10); return; }
    const char c = (char)Serial.read();

    switch (c) {
    case '1': mover("Motor 1", PIN_A1_PUL, PIN_A1_DIR, A1_DIR_HIGH_COUNTS_UP, true,  pasos_prueba()); break;
    case '2': mover("Motor 1", PIN_A1_PUL, PIN_A1_DIR, A1_DIR_HIGH_COUNTS_UP, false, pasos_prueba()); break;
    case '3': mover("Motor 2", PIN_A2_PUL, PIN_A2_DIR, A2_DIR_HIGH_COUNTS_UP, true,  pasos_prueba()); break;
    case '4': mover("Motor 2", PIN_A2_PUL, PIN_A2_DIR, A2_DIR_HIGH_COUNTS_UP, false, pasos_prueba()); break;
    case 'g': case 'G':
        g_ang_idx = (g_ang_idx + 1) % (sizeof(ANGULOS_PRUEBA) / sizeof(ANGULOS_PRUEBA[0]));
        Serial.printf("Angulo de prueba: %.0f grados (%lu pasos)\r\n",
                      (double)ANGULOS_PRUEBA[g_ang_idx], (unsigned long)pasos_prueba());
        break;
    case '5': mover("Motor 1", PIN_A1_PUL, PIN_A1_DIR, A1_DIR_HIGH_COUNTS_UP, true,  PASOS_VUELTA_ART); break;
    case '6': mover("Motor 2", PIN_A2_PUL, PIN_A2_DIR, A2_DIR_HIGH_COUNTS_UP, true,  PASOS_VUELTA_ART); break;
    case 'e': case 'E': energizar(!g_energizado); break;
    case 'h': case 'H': sostener_con_pulsos(); break;
    case 'v': case 'V':
        g_vel_idx = (g_vel_idx + 1) % (sizeof(VELOCIDADES_HZ) / sizeof(VELOCIDADES_HZ[0]));
        Serial.printf("Velocidad: %lu Hz\r\n", (unsigned long)VELOCIDADES_HZ[g_vel_idx]);
        break;
    case 'd': case 'D':
        g_dir_nivel = !g_dir_nivel;
        digitalWrite(PIN_A1_DIR, g_dir_nivel);
        digitalWrite(PIN_A2_DIR, g_dir_nivel);
        Serial.printf("DIR = %s en ambos drivers. Mide entre DIR+ y DIR-: ~%s\r\n",
                      g_dir_nivel ? "ALTO" : "BAJO", g_dir_nivel ? "3.3 V" : "0 V");
        break;
    case '?': menu(); break;
    case '\r': case '\n': break;
    default: Serial.printf("Tecla '%c' sin uso. Pulsa ? para el menu\r\n", c); break;
    }
}
