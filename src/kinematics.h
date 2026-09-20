/* =============================================================================
 * kinematics.h  -  Cinematica directa e inversa del manipulador planar RR
 * =============================================================================
 *
 * ESTE MODULO ES MATEMATICA PURA. No incluye <Arduino.h>, no toca hardware,
 * no imprime, no bloquea. Esa disciplina es deliberada y tiene dos razones:
 *
 *   1) Se compila y se prueba en la PC con "pio test -e native", sin ESP32
 *      conectado. Ver test/test_kinematics/.
 *   2) Es el modulo que se reutiliza sin cambios en el Parcial 3: la logica
 *      de juego necesitara convertir coordenadas de tablero a angulos con
 *      exactamente estas funciones. Si tuviera efectos secundarios, no seria
 *      reutilizable desde otro contexto.
 * ---------------------------------------------------------------------------*/

#pragma once
#include <stdint.h>
#include "types.h"

typedef struct { float x, y; } Point_t;
typedef struct { float q1, q2; } Joints_t;

typedef enum {
    IK_OK            = 0,
    IK_OUT_OF_REACH  = 1,   /* r > WS_R_MAX_MM  */
    IK_TOO_CLOSE     = 2,   /* r < WS_R_MIN_MM  */
    IK_BELOW_TABLE   = 3,   /* y < WS_Y_MIN_MM  */
    IK_JOINT_LIMIT   = 4    /* solucion valida geometricamente, pero fuera
                               de los limites articulares mecanicos          */
} IkResult_t;

/* Cinematica directa: angulos (grados) -> posicion del efector (mm). */
Point_t kin_forward(float q1_deg, float q2_deg);

/* Cinematica inversa: posicion (mm) -> angulos (grados).
 * Devuelve IK_OK y escribe *out solo si la solucion es alcanzable Y respeta
 * los limites articulares. En cualquier otro caso *out queda sin tocar.  */
IkResult_t kin_inverse(float x, float y, ElbowConfig_t elbow, Joints_t *out);

/* Comprobaciones individuales, utiles para el supervisor. */
bool kin_joints_in_limits(float q1_deg, float q2_deg);
IkResult_t kin_point_in_workspace(float x, float y);

/* Valida una recta completa ANTES de empezar a recorrerla.
 * Muestrea `samples` puntos entre a y b (incluidos los extremos) y devuelve
 * el primer error encontrado, o IK_OK si toda la trayectoria es viable.
 *
 * Por que importa: el espacio de trabajo es un ANILLO, no un disco. Una recta
 * entre dos puntos perfectamente alcanzables puede atravesar el agujero
 * central. Sin esta validacion, el brazo arrancaria el movimiento y se
 * detendria a media trayectoria en una pose arbitraria.
 * `out_fail` recibe el punto que fallo (puede ser NULL). */
IkResult_t kin_validate_line(Point_t a, Point_t b, ElbowConfig_t elbow,
                             int samples, Point_t *out_fail);

/* ---------------------------------------------------------------------------
 * ALCANCE EFECTIVO  -  lee esto antes de fijar los limites articulares
 * ---------------------------------------------------------------------------
 * El anillo geometrico del brazo es [|l1-l2|, l1+l2] = [50, 350] mm, pero ese
 * rango solo se alcanza si el codo puede plegarse por completo. Con q2
 * acotado a +/-135 grados, el codo NO llega a cerrarse del todo y el radio
 * minimo REAL sube a:
 *
 *      r_min = sqrt(l1^2 + l2^2 + 2*l1*l2*cos(|q2|_max))
 *            = sqrt(200^2 + 150^2 + 2*200*150*cos(135)) = 141.7 mm
 *
 * O sea que con los limites articulares por defecto el espacio util no es
 * [60, 340] sino [142, 340]: WS_R_MIN_MM ni siquiera llega a aplicarse porque
 * el limite articular actua antes. Estas funciones lo calculan para que el
 * arranque lo reporte y no haya sorpresas al enseñar las poses del tablero.
 * -------------------------------------------------------------------------*/
float kin_effective_r_min(void);
float kin_effective_r_max(void);

/* Conversiones articulacion <-> micropasos. Centralizadas aqui para que el
 * factor 44.444 aparezca en un solo lugar del proyecto. */
int32_t kin_deg_to_steps(float deg);
float   kin_steps_to_deg(int32_t steps);
