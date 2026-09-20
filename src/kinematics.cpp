#include "kinematics.h"
#include "config.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline float deg2rad(float d) { return d * (float)M_PI / 180.0f; }
static inline float rad2deg(float r) { return r * 180.0f / (float)M_PI; }

/* ---------------------------------------------------------------------------
 * CINEMATICA DIRECTA
 *   x = l1*cos(q1) + l2*cos(q1 + q2)
 *   y = l1*sin(q1) + l2*sin(q1 + q2)
 * q2 es el angulo RELATIVO del antebrazo respecto del brazo (convencion
 * Denavit-Hartenberg estandar), no el angulo absoluto respecto de +X.
 * -------------------------------------------------------------------------*/
Point_t kin_forward(float q1_deg, float q2_deg)
{
    const float q1 = deg2rad(q1_deg);
    const float q2 = deg2rad(q2_deg);
    Point_t p;
    p.x = LINK_L1_MM * cosf(q1) + LINK_L2_MM * cosf(q1 + q2);
    p.y = LINK_L1_MM * sinf(q1) + LINK_L2_MM * sinf(q1 + q2);
    return p;
}

IkResult_t kin_point_in_workspace(float x, float y)
{
    const float r = sqrtf(x * x + y * y);
    if (r > WS_R_MAX_MM) return IK_OUT_OF_REACH;
    if (r < WS_R_MIN_MM) return IK_TOO_CLOSE;
    if (y < WS_Y_MIN_MM) return IK_BELOW_TABLE;
    return IK_OK;
}

bool kin_joints_in_limits(float q1_deg, float q2_deg)
{
    return (q1_deg >= A1_MIN_DEG && q1_deg <= A1_MAX_DEG &&
            q2_deg >= A2_MIN_DEG && q2_deg <= A2_MAX_DEG);
}

/* ---------------------------------------------------------------------------
 * CINEMATICA INVERSA
 *   cos(q2) = (x^2 + y^2 - l1^2 - l2^2) / (2*l1*l2)
 *   q2      = +/- acos(cos_q2)        <- el signo elige codo arriba / abajo
 *   q1      = atan2(y,x) - atan2(l2*sin(q2), l1 + l2*cos(q2))
 *
 * Dos cuidados numericos que importan en la practica:
 *
 *   a) cos_q2 puede salir 1.0000001 por error de punto flotante justo en el
 *      borde del alcance, y acosf() de eso devuelve NaN. Se acota al rango
 *      [-1, 1] DESPUES de haber validado el espacio de trabajo, para que el
 *      acotamiento solo absorba ruido numerico y nunca enmascare un punto
 *      realmente inalcanzable.
 *
 *   b) El orden importa: primero se valida el punto, luego se resuelve. Asi
 *      un punto fuera del anillo devuelve un codigo de error util (esta
 *      lejos / esta cerca / esta bajo la mesa) en vez de un NaN silencioso.
 * -------------------------------------------------------------------------*/
IkResult_t kin_inverse(float x, float y, ElbowConfig_t elbow, Joints_t *out)
{
    const IkResult_t ws = kin_point_in_workspace(x, y);
    if (ws != IK_OK) return ws;

    const float l1 = LINK_L1_MM;
    const float l2 = LINK_L2_MM;

    float cos_q2 = (x * x + y * y - l1 * l1 - l2 * l2) / (2.0f * l1 * l2);
    if (cos_q2 >  1.0f) cos_q2 =  1.0f;      /* ver nota (a) */
    if (cos_q2 < -1.0f) cos_q2 = -1.0f;

    float q2 = acosf(cos_q2);                 /* rama positiva = ELBOW_UP */
    if (elbow == ELBOW_DOWN) q2 = -q2;

    const float q1 = atan2f(y, x) - atan2f(l2 * sinf(q2), l1 + l2 * cosf(q2));

    const float q1_deg = rad2deg(q1);
    const float q2_deg = rad2deg(q2);

    if (!kin_joints_in_limits(q1_deg, q2_deg)) return IK_JOINT_LIMIT;

    if (out) { out->q1 = q1_deg; out->q2 = q2_deg; }
    return IK_OK;
}

IkResult_t kin_validate_line(Point_t a, Point_t b, ElbowConfig_t elbow,
                             int samples, Point_t *out_fail)
{
    if (samples < 2) samples = 2;

    for (int i = 0; i <= samples; ++i) {
        const float s = (float)i / (float)samples;
        Point_t p;
        p.x = a.x + (b.x - a.x) * s;
        p.y = a.y + (b.y - a.y) * s;

        const IkResult_t r = kin_inverse(p.x, p.y, elbow, NULL);
        if (r != IK_OK) {
            if (out_fail) *out_fail = p;
            return r;
        }
    }
    return IK_OK;
}

/* ---------------------------------------------------------------------------
 * Alcance efectivo, acotado por los limites articulares (ver kinematics.h).
 * Para minimizar el radio interesa el |q2| mas grande permitido; para
 * maximizarlo, el |q2| mas pequeño permitido (0 si el rango lo incluye).
 * -------------------------------------------------------------------------*/
static float radius_at_q2(float q2_deg)
{
    const float c = cosf(deg2rad(q2_deg));
    return sqrtf(LINK_L1_MM * LINK_L1_MM + LINK_L2_MM * LINK_L2_MM
                 + 2.0f * LINK_L1_MM * LINK_L2_MM * c);
}

float kin_effective_r_min(void)
{
    float q2_abs_max = fmaxf(fabsf(A2_MIN_DEG), fabsf(A2_MAX_DEG));
    if (q2_abs_max > 180.0f) q2_abs_max = 180.0f;
    const float r = radius_at_q2(q2_abs_max);
    return (r > WS_R_MIN_MM) ? r : WS_R_MIN_MM;
}

float kin_effective_r_max(void)
{
    /* Si el rango de q2 contiene el 0, el brazo se estira del todo. */
    const float q2_abs_min = (A2_MIN_DEG <= 0.0f && A2_MAX_DEG >= 0.0f)
                             ? 0.0f
                             : fminf(fabsf(A2_MIN_DEG), fabsf(A2_MAX_DEG));
    const float r = radius_at_q2(q2_abs_min);
    return (r < WS_R_MAX_MM) ? r : WS_R_MAX_MM;
}

/* ---------------------------------------------------------------------------
 * Conversion grados <-> micropasos.
 * lroundf y no un cast: un cast truncaria hacia cero y sesgaria todos los
 * movimientos negativos medio paso hacia el origen.
 * -------------------------------------------------------------------------*/
int32_t kin_deg_to_steps(float deg)     { return (int32_t)lroundf(deg * STEPS_PER_DEG); }
float   kin_steps_to_deg(int32_t steps) { return (float)steps * DEG_PER_STEP; }
