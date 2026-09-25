// =============================================================================
//  pid.h — PID de posición con anti-windup, D sobre la medición, zona muerta,
//          compensación de fricción y feed-forward de velocidad.
// =============================================================================
#pragma once
#include <Arduino.h>

struct PIDGains {
    float kp;        // PWM por grado
    float ki;        // PWM por (grado * s)
    float kd;        // PWM por (grado / s)
    float kff;       // PWM por (grado / s) de velocidad de referencia
    float deadband;  // grados: |error| menor a esto -> salida 0
    int   pwm_min;   // PWM mínimo que vence la fricción estática
};

class PID {
public:
    void  configure(const PIDGains &g, int out_limit);
    void  setGains(const PIDGains &g);
    const PIDGains &gains() const { return g_; }
    void  setOutputLimit(int lim) { out_lim_ = lim; }
    // Reinicia estados internos; meas = medición actual (evita salto en D).
    void  reset(float meas);
    // sp: posición de referencia (°), vel_ff: velocidad de referencia (°/s),
    // meas: posición medida (°), dt: periodo (s). Devuelve PWM con signo.
    int   update(float sp, float vel_ff, float meas, float dt);

    float lastP = 0, lastI = 0, lastD = 0;   // para diagnóstico

private:
    PIDGains g_{};
    int   out_lim_   = 1023;
    float integ_     = 0;     // ya multiplicado por ki (en unidades de PWM)
    float prev_meas_ = 0;
    float d_filt_    = 0;
};
