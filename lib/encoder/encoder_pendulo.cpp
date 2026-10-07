#ifdef ARDUINO

#include "encoder_pendulo.h"

#include <config.h>

void EncoderPendulo::begin() {
  ESP32Encoder::useInternalWeakPullResistors = puType::none;
  enc_.attachFullQuad(cfg::PIN_ENC_A, cfg::PIN_ENC_B);
  enc_.setFilter(cfg::ENC_FILTRO_PCNT);
  derivada_.configurar(cfg::TS, cfg::FC_DERIVADA_HZ);
  fijarReferenciaColgando();
}

void EncoderPendulo::fijarReferenciaColgando() {
  enc_.clearCount();
  cuentas_previas_ = 0;
  derivada_.reiniciar();
}

EstadoEncoder EncoderPendulo::actualizar() {
  const int64_t c = enc_.getCount();
  const int64_t delta = c - cuentas_previas_;
  cuentas_previas_ = c;

  EstadoEncoder e;
  e.cuentas = c;
  e.theta = encoder_math::theta(c, cfg::ENC_CUENTAS_VUELTA, cfg::ENC_CUENTAS_ARRIBA, cfg::ENC_SIGNO);
  e.theta_dot = derivada_.actualizar(
      cfg::ENC_SIGNO * encoder_math::cuentasARad(delta, cfg::ENC_CUENTAS_VUELTA));
  return e;
}

int64_t EncoderPendulo::cuentas() { return enc_.getCount(); }

#endif  // ARDUINO
