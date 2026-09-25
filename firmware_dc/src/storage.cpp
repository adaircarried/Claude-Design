// =============================================================================
//  storage.cpp
// =============================================================================
//  Nota: escribir en flash suspende brevemente la caché de ambos núcleos
//  (pocos ms). Por eso solo se guarda por orden explícita (CAL END, SAVE) y
//  nunca dentro de TaskControl.
// =============================================================================
#include "storage.h"
#include <Preferences.h>

static const char *NS = "robot2dof";

void storage_load(float cpr[NUM_AXES], PIDGains gains[NUM_AXES]) {
    Preferences p;
    p.begin(NS, true);
    char key[12];
    for (uint8_t i = 0; i < NUM_AXES; i++) {
        snprintf(key, sizeof(key), "cpr%u", i);
        float c = p.getFloat(key, 0.0f);
        if (c > 10.0f) cpr[i] = c;
        snprintf(key, sizeof(key), "pid%u", i);
        if (p.getBytesLength(key) == sizeof(PIDGains)) {
            p.getBytes(key, &gains[i], sizeof(PIDGains));
        }
    }
    p.end();
}

void storage_save_cpr(uint8_t axis, float cpr) {
    Preferences p;
    p.begin(NS, false);
    char key[12];
    snprintf(key, sizeof(key), "cpr%u", axis);
    p.putFloat(key, cpr);
    p.end();
}

void storage_save_gains(const PIDGains gains[NUM_AXES]) {
    Preferences p;
    p.begin(NS, false);
    char key[12];
    for (uint8_t i = 0; i < NUM_AXES; i++) {
        snprintf(key, sizeof(key), "pid%u", i);
        p.putBytes(key, &gains[i], sizeof(PIDGains));
    }
    p.end();
}

void storage_load_motion(int &pwm_limit, uint16_t &speed_pct, float &accel) {
    Preferences p;
    p.begin(NS, true);
    pwm_limit = p.getInt("lim", pwm_limit);
    speed_pct = p.getUShort("spd", speed_pct);
    accel     = p.getFloat("acc", accel);
    p.end();
}

void storage_save_motion(int pwm_limit, uint16_t speed_pct, float accel) {
    Preferences p;
    p.begin(NS, false);
    p.putInt("lim", pwm_limit);
    p.putUShort("spd", speed_pct);
    p.putFloat("acc", accel);
    p.end();
}

void storage_clear() {
    Preferences p;
    p.begin(NS, false);
    p.clear();
    p.end();
}
