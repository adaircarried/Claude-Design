// Encoder del péndulo (LPD3806-600BM) leído por PCNT con ESP32Encoder.
//
// Uso típico en la tarea de control (núcleo 0, cada Ts):
//   EstadoEncoder e = encoder.actualizar();   // theta [rad], theta_dot [rad/s]
//
// actualizar() mantiene el estado del filtro de la derivada, así que debe llamarse
// desde una sola tarea y exactamente una vez por periodo. Las demás tareas deben
// recibir copias de EstadoEncoder (cola o variable protegida), nunca llamar aquí.
#pragma once

#include <stdint.h>

#include "encoder_math.h"

struct EstadoEncoder {
  int64_t cuentas;   // desde la referencia colgando
  float theta;       // [rad], 0 = vertical arriba, en (-pi, pi]
  float theta_dot;   // [rad/s], derivada filtrada
};

#ifdef ARDUINO

#include <ESP32Encoder.h>

class EncoderPendulo {
 public:
  // Configura el PCNT en cuadratura completa con filtro de hardware, según config.h.
  // Las pull-ups son externas (4.7k a 3.3 V); se desactivan las internas.
  void begin();

  // Toma la posición actual como referencia (péndulo colgando y quieto).
  // Reinicia el filtro: llamar desde la misma tarea que actualizar().
  void fijarReferenciaColgando();

  // Lee el contador, calcula theta y actualiza la derivada filtrada. Llamar una vez por Ts.
  EstadoEncoder actualizar();

  // Lectura cruda del contador (válida desde cualquier tarea).
  int64_t cuentas();

 private:
  ESP32Encoder enc_;
  encoder_math::DerivadaFiltrada derivada_;
  int64_t cuentas_previas_ = 0;
};

#endif  // ARDUINO
