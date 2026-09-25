// =============================================================================
//  encoder.cpp
// =============================================================================
//  Decisión: se usa el periférico PCNT a través de ESP32Encoder. El conteo de
//  flancos lo hace el hardware; la CPU solo lee un registro cada 5 ms. No se
//  pierden cuentas aunque el motor vaya a máxima velocidad y no hay una
//  interrupción por flanco compitiendo con el lazo de control.
//
//  Modo ENC_QUADRATURE: attachFullQuad -> x4 (4 cuentas por pulso), dirección
//  real decodificada por hardware.
//
//  Modo ENC_SINGLE (cable de fase B roto): se reconfigura el canal 0 del PCNT
//  para contar AMBOS flancos de A siempre hacia arriba (x2) e ignorar la señal
//  de control. El conteo resultante es una MAGNITUD de giro; en cada ciclo el
//  incremento se multiplica por el signo del PWM que se está aplicando.
//  Limitación: si el eje se mueve por inercia en sentido contrario al PWM
//  (rebote, sobrepaso, empujón externo), esas cuentas se suman con el signo
//  equivocado. En un brazo planar horizontal, sin gravedad sobre los ejes y con
//  reductor 1:34, el efecto es pequeño.
// =============================================================================
#include "encoder.h"
#include <ESP32Encoder.h>
#include <driver/pcnt.h>

static ESP32Encoder s_enc[NUM_AXES];
static int64_t      s_last_raw[NUM_AXES] = {0, 0};   // solo ENC_SINGLE
static int64_t      s_accum[NUM_AXES]    = {0, 0};   // solo ENC_SINGLE

void encoder_init() {
    // Las placas del GM25-370 ya traen pull-ups a VCC (3.3 V); el pull-up
    // interno débil no estorba y mantiene definido un canal desconectado.
    ESP32Encoder::useInternalWeakPullResistors = puType::up;

    for (uint8_t i = 0; i < NUM_AXES; i++) {
        if (ENC_MODE[i] == ENC_QUADRATURE) {
            s_enc[i].attachFullQuad(PIN_ENC_A[i], PIN_ENC_B[i]);
        } else {
            s_enc[i].attachSingleEdge(PIN_ENC_A[i], PIN_ENC_B[i]);
            // Ambos flancos de A cuentan +1; la señal de control (B) se ignora.
            pcnt_set_mode(s_enc[i].unit, PCNT_CHANNEL_0,
                          PCNT_COUNT_INC, PCNT_COUNT_INC,
                          PCNT_MODE_KEEP, PCNT_MODE_KEEP);
        }
        s_enc[i].setFilter(ENC_FILTER_APB_CYCLES);
        s_enc[i].clearCount();
        s_last_raw[i] = 0;
        s_accum[i]    = 0;
    }
}

void encoder_update(uint8_t axis, int8_t dir_hint) {
    if (ENC_MODE[axis] != ENC_SINGLE) return;
    int64_t raw   = s_enc[axis].getCount();
    int64_t delta = raw - s_last_raw[axis];
    s_last_raw[axis] = raw;
    if (delta < 0) delta = -delta;          // por construcción es magnitud
    s_accum[axis] += (dir_hint >= 0) ? delta : -delta;
}

int64_t encoder_count(uint8_t axis) {
    if (ENC_MODE[axis] == ENC_SINGLE) return s_accum[axis];
    int64_t c = s_enc[axis].getCount();
    return ENC_INVERT[axis] ? -c : c;
}

void encoder_zero(uint8_t axis) {
    s_enc[axis].clearCount();
    s_last_raw[axis] = 0;
    s_accum[axis]    = 0;
}
