// =============================================================================
//  motion.h — TaskMotion: consume la cola de setpoints y genera perfiles
// =============================================================================
#pragma once
#include "setpoint.h"

void motion_init();              // crea la cola
void task_motion(void *arg);

// Perfil trapezoidal 1D sobre una distancia D >= 0 (unidades arbitrarias:
// grados para MOVJ/JOG, mm para MOVL). Expuesto para poder probarlo aislado.
class TrapProfile {
public:
    void  plan(float D, float vmax, float amax);
    // Avanza dt segundos. Devuelve true cuando el perfil terminó.
    bool  step(float dt);
    // Parada controlada: desacelera con amax desde la velocidad actual.
    void  stop();
    float s() const { return s_; }        // posición a lo largo del perfil
    float v() const { return v_; }        // velocidad a lo largo del perfil
    float duration() const { return T_; }
    bool  done() const { return done_; }
private:
    float D_ = 0, vp_ = 0, a_ = 1, ta_ = 0, tc_ = 0, T_ = 0, t_ = 0;
    float s_ = 0, v_ = 0;
    bool  done_ = true;
    bool  stopping_ = false;
    float s0_ = 0, v0_ = 0, tstop_ = 0;
};
