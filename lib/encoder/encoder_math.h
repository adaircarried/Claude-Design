// Matemática del encoder sin dependencias de hardware (se prueba en la PC con `pio test -e native`).
#pragma once

#include <math.h>
#include <stdint.h>

namespace encoder_math {

constexpr float PI_F = 3.14159265358979f;

// Envuelve una cuenta al intervalo [-media_vuelta, media_vuelta).
inline int32_t envolverCuentas(int64_t cuentas, int32_t cuentas_vuelta) {
  const int32_t media = cuentas_vuelta / 2;
  int64_t r = (cuentas + media) % cuentas_vuelta;
  if (r < 0) r += cuentas_vuelta;
  return static_cast<int32_t>(r - media);
}

inline float cuentasARad(int64_t cuentas, int32_t cuentas_vuelta) {
  return static_cast<float>(cuentas) * (2.0f * PI_F / static_cast<float>(cuentas_vuelta));
}

// Ángulo del péndulo respecto a la vertical arriba, en (-pi, pi].
// `cuentas` se mide desde la posición colgando; arriba = colgando + cuentas_arriba.
// La resta y el envolvimiento se hacen en enteros para no acumular error de punto flotante.
inline float theta(int64_t cuentas, int32_t cuentas_vuelta, int32_t cuentas_arriba, int signo) {
  const int32_t rel = envolverCuentas(cuentas - cuentas_arriba, cuentas_vuelta);
  return static_cast<float>(signo) * cuentasARad(rel, cuentas_vuelta);
}

// Derivada filtrada de primer orden: w_k = a*w_{k-1} + (1-a)*(dtheta/Ts),
// con a = tau / (tau + Ts) y tau = 1 / (2*pi*fc).
// Se alimenta con la diferencia de cuentas (sin envolver) para no ver saltos de +-2*pi.
class DerivadaFiltrada {
 public:
  void configurar(float ts, float fc_hz) {
    ts_ = ts;
    const float tau = 1.0f / (2.0f * PI_F * fc_hz);
    alfa_ = tau / (tau + ts);
  }

  void reiniciar(float valor = 0.0f) { y_ = valor; }

  float actualizar(float delta) {
    y_ = alfa_ * y_ + (1.0f - alfa_) * (delta / ts_);
    return y_;
  }

  float valor() const { return y_; }
  float alfa() const { return alfa_; }

 private:
  float ts_ = 0.002f;
  float alfa_ = 0.0f;
  float y_ = 0.0f;
};

}  // namespace encoder_math
