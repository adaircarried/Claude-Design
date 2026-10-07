// main: control completo del péndulo (PD en núcleo 0, telemetría en núcleo 1).
//
// Pendiente de implementar. Por ahora solo deja el driver deshabilitado.

#include <Arduino.h>
#include <config.h>

void setup() {
  // Seguridad: driver deshabilitado (ENABLE activo bajo) y STEP en bajo.
  pinMode(cfg::PIN_ENABLE, OUTPUT);
  digitalWrite(cfg::PIN_ENABLE, HIGH);
  pinMode(cfg::PIN_STEP, OUTPUT);
  digitalWrite(cfg::PIN_STEP, LOW);

  Serial.begin(115200);
  delay(200);
  Serial.println("main: pendiente de implementar, driver deshabilitado.");
}

void loop() { delay(1000); }
