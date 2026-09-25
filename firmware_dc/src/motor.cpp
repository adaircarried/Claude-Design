// =============================================================================
//  motor.cpp
// =============================================================================
//  PWM por el periférico LEDC a 20 kHz / 10 bits (no analogWrite): la forma de
//  onda la genera el hardware, sin jitter de software y fuera del rango
//  audible. 80 MHz / 2^10 = 78 kHz es el máximo para 10 bits, así que 20 kHz
//  cabe con holgura.
//
//  Dirección: IN_A / IN_B. Velocidad: ciclo de trabajo en EN.
//  Con PWM 0 se deja IN_A = IN_B = 0 y EN = 0 (coast). Se prefirió coast a
//  freno activo porque el reductor 1:34 ya sostiene la posición en un brazo
//  horizontal y el freno de corto en el L298N genera picos de corriente.
// =============================================================================
#include "motor.h"

void motor_init() {
    for (uint8_t i = 0; i < NUM_AXES; i++) {
        pinMode(PIN_INA[i], OUTPUT);
        pinMode(PIN_INB[i], OUTPUT);
        digitalWrite(PIN_INA[i], LOW);
        digitalWrite(PIN_INB[i], LOW);
        ledcSetup(PWM_CHANNEL[i], PWM_FREQ_HZ, PWM_RES_BITS);
        ledcAttachPin(PIN_PWM[i], PWM_CHANNEL[i]);
        ledcWrite(PWM_CHANNEL[i], 0);
    }
}

void motor_set(uint8_t axis, int pwm) {
    if (MOTOR_INVERT[axis]) pwm = -pwm;
    if (pwm > PWM_MAX)  pwm = PWM_MAX;
    if (pwm < -PWM_MAX) pwm = -PWM_MAX;

    if (pwm > 0) {
        digitalWrite(PIN_INA[axis], HIGH);
        digitalWrite(PIN_INB[axis], LOW);
        ledcWrite(PWM_CHANNEL[axis], (uint32_t)pwm);
    } else if (pwm < 0) {
        digitalWrite(PIN_INA[axis], LOW);
        digitalWrite(PIN_INB[axis], HIGH);
        ledcWrite(PWM_CHANNEL[axis], (uint32_t)(-pwm));
    } else {
        motor_coast(axis);
    }
}

void motor_coast(uint8_t axis) {
    ledcWrite(PWM_CHANNEL[axis], 0);
    digitalWrite(PIN_INA[axis], LOW);
    digitalWrite(PIN_INB[axis], LOW);
}
