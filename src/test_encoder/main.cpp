// test_encoder: lectura del encoder y validación de cuentas.
//
// Usa exactamente el mismo módulo de encoder y la misma tarea periódica (núcleo 0, Ts = 2 ms)
// que el programa de control. El motor nunca da pasos; solo se puede energizar el driver
// (comando 'e') para comprobar que el ruido del chopper no altera la cuenta.
//
// Comandos por serial (115200):
//   z  referencia: péndulo colgando y quieto -> cuenta 0
//   m  marca la cuenta actual (para contar vueltas y verificar que regresa al mismo valor)
//   r  reinicia mínimo/máximo de cuentas y estadísticas del periodo
//   e  energiza / desenergiza el driver (sin pasos)
//   p  pausa / reanuda la impresión
//   h  ayuda

#include <Arduino.h>
#include <config.h>
#include <encoder_pendulo.h>
#include <periodico.h>

namespace {

constexpr uint32_t PERIODO_IMPRESION_MS = 100;
constexpr float RAD_A_GRADOS = 180.0f / encoder_math::PI_F;

struct Muestra {
  EstadoEncoder e;
  int64_t cuenta_min;  // extremos desde el último 'r': con el péndulo quieto deben coincidir
  int64_t cuenta_max;
};

EncoderPendulo encoder;
QueueHandle_t buzon;  // cola de longitud 1 (xQueueOverwrite): núcleo 0 -> núcleo 1

volatile bool pedir_cero = false;
volatile bool pedir_reinicio_extremos = true;

int64_t marca = 0;
bool driver_habilitado = false;
bool imprimir = true;

// ---------------------------------------------------- Núcleo 0 (tiempo real)
// Nada de Serial ni delay aquí.
void tareaControl(void*) {
  static int64_t cmin = 0, cmax = 0;

  if (pedir_cero) {
    encoder.fijarReferenciaColgando();
    pedir_cero = false;
    pedir_reinicio_extremos = true;
  }

  Muestra m;
  m.e = encoder.actualizar();

  if (pedir_reinicio_extremos) {
    cmin = cmax = m.e.cuentas;
    pedir_reinicio_extremos = false;
  }
  if (m.e.cuentas < cmin) cmin = m.e.cuentas;
  if (m.e.cuentas > cmax) cmax = m.e.cuentas;
  m.cuenta_min = cmin;
  m.cuenta_max = cmax;

  xQueueOverwrite(buzon, &m);
}

// ---------------------------------------------------- Núcleo 1 (usuario)
void habilitarDriver(bool on) {
  driver_habilitado = on;
  digitalWrite(cfg::PIN_ENABLE, on ? LOW : HIGH);  // activo bajo
}

void ayuda() {
  Serial.println();
  Serial.println("=== test_encoder ===");
  Serial.printf("Encoder: %d cuentas/vuelta (%.3f grados/cuenta), Ts = %lu us\n",
                cfg::ENC_CUENTAS_VUELTA, 360.0f / cfg::ENC_CUENTAS_VUELTA,
                (unsigned long)cfg::TS_US);
  Serial.println("z: cero (colgando)  m: marca  r: reinicia extremos/periodo");
  Serial.println("e: driver on/off (sin pasos)  p: pausa  h: ayuda");
  Serial.println("Columnas: cuentas | grados desde colgando | theta [grados] | theta_dot [rad/s] |");
  Serial.println("          desde marca [cuentas, vueltas] | rango min..max | periodo min/max [us] |");
  Serial.println("          ejecucion max [us] | atrasos | driver");
  Serial.println();
}

void atenderComandos() {
  while (Serial.available()) {
    switch (Serial.read()) {
      case 'z':
        pedir_cero = true;
        marca = 0;
        Serial.println("> referencia colgando = 0");
        break;
      case 'm': {
        Muestra m;
        if (xQueuePeek(buzon, &m, 0) == pdTRUE) marca = m.e.cuentas;
        Serial.printf("> marca = %lld\n", (long long)marca);
        break;
      }
      case 'r':
        pedir_reinicio_extremos = true;
        reiniciarEstadisticasPeriodo();
        Serial.println("> extremos y periodo reiniciados");
        break;
      case 'e':
        habilitarDriver(!driver_habilitado);
        Serial.printf("> driver %s\n", driver_habilitado ? "ENERGIZADO" : "apagado");
        break;
      case 'p':
        imprimir = !imprimir;
        break;
      case 'h':
        ayuda();
        break;
      default:
        break;
    }
  }
}

void imprimirEstado() {
  Muestra m;
  if (xQueuePeek(buzon, &m, 0) != pdTRUE) return;
  const EstadisticasPeriodo s = leerEstadisticasPeriodo();

  const float grados_colgando =
      encoder_math::cuentasARad(m.e.cuentas, cfg::ENC_CUENTAS_VUELTA) * RAD_A_GRADOS;
  const int64_t desde_marca = m.e.cuentas - marca;

  Serial.printf(
      "%8lld | %9.2f | %8.2f | %7.2f | %6lld %7.3f | %lld..%lld | %lu/%lu | %lu | %lu | %s\n",
      (long long)m.e.cuentas, grados_colgando, m.e.theta * RAD_A_GRADOS, m.e.theta_dot,
      (long long)desde_marca, (float)desde_marca / cfg::ENC_CUENTAS_VUELTA,
      (long long)m.cuenta_min, (long long)m.cuenta_max, (unsigned long)s.periodo_min_us,
      (unsigned long)s.periodo_max_us, (unsigned long)s.ejecucion_max_us,
      (unsigned long)s.atrasos, driver_habilitado ? "ON" : "off");
}

}  // namespace

void setup() {
  // Seguridad primero: driver deshabilitado y STEP en bajo antes de cualquier otra cosa.
  pinMode(cfg::PIN_ENABLE, OUTPUT);
  habilitarDriver(false);
  pinMode(cfg::PIN_STEP, OUTPUT);
  digitalWrite(cfg::PIN_STEP, LOW);
  pinMode(cfg::PIN_DIR, OUTPUT);
  digitalWrite(cfg::PIN_DIR, LOW);

  Serial.begin(115200);
  delay(200);

  buzon = xQueueCreate(1, sizeof(Muestra));
  encoder.begin();

  if (!iniciarTareaPeriodica(tareaControl, nullptr, cfg::TS_US, cfg::NUCLEO_CONTROL,
                             cfg::PRIORIDAD_CONTROL)) {
    Serial.println("ERROR: no se pudo crear la tarea de control");
  }

  ayuda();
  Serial.println("Deja el péndulo colgando y quieto, y presiona 'z'.");
}

void loop() {
  atenderComandos();
  if (imprimir) imprimirEstado();
  delay(PERIODO_IMPRESION_MS);
}
