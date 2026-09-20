#include "persistence.h"
#include "config.h"
#include <Preferences.h>

static Preferences prefs;
static const char *NS = "arm";

void persist_init(void)
{
    /* false = lectura y escritura. El espacio de nombres "arm" queda
     * reservado; el Parcial 3 puede añadir claves sin migrar nada. */
    prefs.begin(NS, false);
}

void persist_save_config(uint16_t speed_pct, ElbowConfig_t elbow)
{
    prefs.putUShort("speed", speed_pct);
    prefs.putUChar("elbow", (uint8_t)elbow);
}

void persist_load_config(uint16_t *speed_pct, ElbowConfig_t *elbow)
{
    if (speed_pct) *speed_pct = prefs.getUShort("speed", 50);
    if (elbow)     *elbow     = (ElbowConfig_t)prefs.getUChar("elbow", (uint8_t)ELBOW_DOWN);
}

void persist_save_home(int32_t s1, int32_t s2)
{
    prefs.putInt("h1", s1);
    prefs.putInt("h2", s2);
    prefs.putBool("hok", true);
}

bool persist_load_home(int32_t *s1, int32_t *s2)
{
    if (!prefs.getBool("hok", false)) return false;
    if (s1) *s1 = prefs.getInt("h1", 0);
    if (s2) *s2 = prefs.getInt("h2", 0);
    return true;
}

/* --- PARCIAL 3: poses enseñadas ----------------------------------------- */
void persist_save_pose(uint8_t idx, float q1, float q2)
{
    if (idx >= POSE_TABLE_SIZE) return;
    char k[8];
    snprintf(k, sizeof(k), "p%u1", idx); prefs.putFloat(k, q1);
    snprintf(k, sizeof(k), "p%u2", idx); prefs.putFloat(k, q2);
    snprintf(k, sizeof(k), "p%uv", idx); prefs.putBool(k, true);
}

bool persist_load_pose(uint8_t idx, float *q1, float *q2)
{
    if (idx >= POSE_TABLE_SIZE) return false;
    char k[8];
    snprintf(k, sizeof(k), "p%uv", idx);
    if (!prefs.getBool(k, false)) return false;
    snprintf(k, sizeof(k), "p%u1", idx); if (q1) *q1 = prefs.getFloat(k, 0.0f);
    snprintf(k, sizeof(k), "p%u2", idx); if (q2) *q2 = prefs.getFloat(k, 0.0f);
    return true;
}

void persist_erase_all(void) { prefs.clear(); }
