// =============================================================================
//  pid.cpp
// =============================================================================
//  u = Kp*e + I - Kd*d(meas)/dt + Kff*v_ref
//
//  - Derivativo sobre la MEDICIÓN (no sobre el error): un cambio brusco del
//    setpoint no produce un pico ("derivative kick"). Además se filtra con un
//    pasa-bajas de primer orden porque derivar cuentas de encoder a 200 Hz
//    amplifica el ruido de cuantización.
//  - Anti-windup por doble vía: (1) integración condicional: si la salida está
//    saturada y el error empuja hacia la misma saturación, no se integra;
//    (2) el integrador se limita a PID_ILIMIT_FRAC del PWM máximo.
//  - El integrador acumula ki*e*dt (no e*dt): así cambiar ki en caliente no
//    provoca un salto en la salida.
//  - Zona muerta: dentro de ±deadband la salida es 0 y el integrador se
//    congela. Evita que el motor "cace" alrededor del objetivo por la
//    resolución del encoder y el juego del reductor.
//  - pwm_min: el L298N + reductor necesitan un ciclo mínimo para moverse. Se
//    suma fuera de la zona muerta para que el lazo no dependa solo del
//    integrador para vencer la fricción estática.
// =============================================================================
#include "pid.h"
#include "config.h"

void PID::configure(const PIDGains &g, int out_limit) {
    g_ = g;
    out_lim_ = out_limit;
    integ_ = 0;
    d_filt_ = 0;
}

void PID::setGains(const PIDGains &g) { g_ = g; }

void PID::reset(float meas) {
    integ_ = 0;
    d_filt_ = 0;
    prev_meas_ = meas;
}

int PID::update(float sp, float vel_ff, float meas, float dt) {
    const float err = sp - meas;

    // Derivativo sobre la medición, filtrado
    const float dmeas = (meas - prev_meas_) / dt;
    prev_meas_ = meas;
    d_filt_ += PID_D_FILTER_ALPHA * (dmeas - d_filt_);

    const float ff = g_.kff * vel_ff;

    if (fabsf(err) < g_.deadband && fabsf(vel_ff) < 1e-3f) {
        // Detenido dentro de la tolerancia: salida 0, integrador congelado.
        lastP = lastD = 0;
        lastI = integ_;
        return 0;
    }

    const float p = g_.kp * err;
    const float d = -g_.kd * d_filt_;
    float u_unsat = p + integ_ + d + ff;

    // Integración condicional (anti-windup)
    const float lim = (float)out_lim_;
    const bool sat_hi = u_unsat >  lim && err > 0;
    const bool sat_lo = u_unsat < -lim && err < 0;
    if (!sat_hi && !sat_lo) {
        integ_ += g_.ki * err * dt;
        const float ilim = PID_ILIMIT_FRAC * lim;
        if (integ_ >  ilim) integ_ =  ilim;
        if (integ_ < -ilim) integ_ = -ilim;
    }

    float u = p + integ_ + d + ff;

    // Compensación de fricción estática
    if (g_.pwm_min > 0) {
        if (u > 0)      u += g_.pwm_min;
        else if (u < 0) u -= g_.pwm_min;
    }

    // Saturación al rango de PWM permitido
    if (u >  lim) u =  lim;
    if (u < -lim) u = -lim;

    lastP = p; lastI = integ_; lastD = d;
    return (int)lroundf(u);
}
