// =============================================================================
//  kinematics.cpp
// =============================================================================
//  Directa:  x = l1 cos q1 + l2 cos(q1+q2)
//            y = l1 sin q1 + l2 sin(q1+q2)
//  Inversa:  c2 = (x² + y² - l1² - l2²) / (2 l1 l2)
//            q2 = ± acos(c2)          (+ codo abajo, - codo arriba)
//            q1 = atan2(y, x) - atan2(l2 sin q2, l1 + l2 cos q2)
//  Toda solución se valida (anillo 50..350 mm con margen y límites de junta)
//  ANTES de ejecutarse.
// =============================================================================
#include "kinematics.h"
#include "config.h"

static const char *s_err = "";

const char *kin_last_error() { return s_err; }

void kin_forward(float q1_deg, float q2_deg, float &x, float &y) {
    const float q1 = radians(q1_deg);
    const float q12 = radians(q1_deg + q2_deg);
    x = L1_MM * cosf(q1) + L2_MM * cosf(q12);
    y = L1_MM * sinf(q1) + L2_MM * sinf(q12);
}

bool kin_in_workspace(float x, float y) {
    const float r = sqrtf(x * x + y * y);
    return r >= WS_RMIN_MM + WS_MARGIN_MM && r <= WS_RMAX_MM - WS_MARGIN_MM;
}

bool kin_joints_valid(float q1_deg, float q2_deg) {
    return q1_deg >= JOINT_MIN_DEG[0] && q1_deg <= JOINT_MAX_DEG[0] &&
           q2_deg >= JOINT_MIN_DEG[1] && q2_deg <= JOINT_MAX_DEG[1];
}

bool kin_inverse(float x, float y, bool elbow_up, float &q1_deg, float &q2_deg) {
    if (!kin_in_workspace(x, y)) {
        s_err = "punto fuera del espacio de trabajo (50..350 mm)";
        return false;
    }
    float c2 = (x * x + y * y - L1_MM * L1_MM - L2_MM * L2_MM) / (2.0f * L1_MM * L2_MM);
    if (c2 > 1.0f) c2 = 1.0f;        // protección numérica
    if (c2 < -1.0f) c2 = -1.0f;
    float q2 = acosf(c2);
    if (elbow_up) q2 = -q2;
    const float q1 = atan2f(y, x) - atan2f(L2_MM * sinf(q2), L1_MM + L2_MM * cosf(q2));

    q1_deg = degrees(q1);
    q2_deg = degrees(q2);
    // Normaliza q1 a (-180, 180]
    while (q1_deg > 180.0f)   q1_deg -= 360.0f;
    while (q1_deg <= -180.0f) q1_deg += 360.0f;

    if (!kin_joints_valid(q1_deg, q2_deg)) {
        s_err = "solucion fuera de los limites articulares";
        return false;
    }
    return true;
}
