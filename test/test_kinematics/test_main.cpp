/* =============================================================================
 *  Tests unitarios de cinematica  -  corren en la PC, SIN ESP32
 * =============================================================================
 *      pio test -e native
 *
 *  Por que esto vale la pena: la cinematica inversa que se prueba aqui es
 *  exactamente la que usara el Parcial 3 para convertir coordenadas de
 *  tablero en angulos. Un error de signo en la seleccion de rama del codo se
 *  manifiesta como "el brazo se voltea solo", y depurar eso con el hardware
 *  montado cuesta horas. Aqui se detecta en un segundo.
 * ---------------------------------------------------------------------------*/

#include <unity.h>
#include <math.h>
#include "kinematics.h"
#include "config.h"

#define TOL 0.01f

void setUp(void) {}
void tearDown(void) {}

/* --- Cinematica directa contra poses calculadas a mano ------------------ */
static void test_fk_brazo_estirado(void)
{
    /* q1=0, q2=0 -> totalmente extendido sobre +X: x = l1+l2 = 350 */
    Point_t p = kin_forward(0.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 350.0f, p.x);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.0f,   p.y);
}

static void test_fk_hombro_vertical(void)
{
    /* q1=90, q2=0 -> recto hacia arriba */
    Point_t p = kin_forward(90.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.0f,   p.x);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 350.0f, p.y);
}

static void test_fk_codo_plegado(void)
{
    /* q1=0, q2=180 -> el antebrazo vuelve sobre el brazo: x = l1-l2 = 50 */
    Point_t p = kin_forward(0.0f, 180.0f);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 50.0f, p.x);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.0f,  p.y);
}

static void test_fk_codo_90(void)
{
    /* q1=0, q2=90 -> (l1, l2) = (200, 150) */
    Point_t p = kin_forward(0.0f, 90.0f);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 200.0f, p.x);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 150.0f, p.y);
}

/* --- LA PRUEBA QUE MAS IMPORTA: ida y vuelta FK -> IK -> FK -------------
 * Barre el espacio de trabajo util y comprueba que resolver la inversa y
 * volver con la directa devuelve el mismo punto. Cubre a la vez los signos,
 * la seleccion de rama y los casos limite del atan2. */
static void test_ik_roundtrip_codo_abajo(void)
{
    int probados = 0;
    for (float x = -300.0f; x <= 300.0f; x += 20.0f) {
        for (float y = -200.0f; y <= 300.0f; y += 20.0f) {
            Joints_t j;
            if (kin_inverse(x, y, ELBOW_DOWN, &j) != IK_OK) continue;
            Point_t p = kin_forward(j.q1, j.q2);
            TEST_ASSERT_FLOAT_WITHIN(0.1f, x, p.x);
            TEST_ASSERT_FLOAT_WITHIN(0.1f, y, p.y);
            ++probados;
        }
    }
    TEST_ASSERT_GREATER_THAN(50, probados);   /* que realmente haya probado algo */
}

static void test_ik_roundtrip_codo_arriba(void)
{
    int probados = 0;
    for (float x = -300.0f; x <= 300.0f; x += 25.0f) {
        for (float y = -200.0f; y <= 300.0f; y += 25.0f) {
            Joints_t j;
            if (kin_inverse(x, y, ELBOW_UP, &j) != IK_OK) continue;
            Point_t p = kin_forward(j.q1, j.q2);
            TEST_ASSERT_FLOAT_WITHIN(0.1f, x, p.x);
            TEST_ASSERT_FLOAT_WITHIN(0.1f, y, p.y);
            ++probados;
        }
    }
    TEST_ASSERT_GREATER_THAN(20, probados);
}

/* --- Las dos ramas dan el mismo punto pero angulos distintos ------------ */
static void test_ik_dos_ramas_mismo_punto(void)
{
    Joints_t up, down;
    TEST_ASSERT_EQUAL(IK_OK, kin_inverse(150.0f, 100.0f, ELBOW_UP,   &up));
    TEST_ASSERT_EQUAL(IK_OK, kin_inverse(150.0f, 100.0f, ELBOW_DOWN, &down));

    Point_t pu = kin_forward(up.q1,   up.q2);
    Point_t pd = kin_forward(down.q1, down.q2);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, pu.x, pd.x);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, pu.y, pd.y);

    TEST_ASSERT_TRUE(up.q2 > 0.0f);     /* rama positiva */
    TEST_ASSERT_TRUE(down.q2 < 0.0f);   /* rama negativa */
}

/* --- Validacion del espacio de trabajo ---------------------------------- */
static void test_fuera_de_alcance(void)
{
    Joints_t j;
    TEST_ASSERT_EQUAL(IK_OUT_OF_REACH, kin_inverse(400.0f, 0.0f, ELBOW_DOWN, &j));
}

static void test_demasiado_cerca(void)
{
    Joints_t j;
    /* r = 10 mm, muy dentro del agujero central del anillo */
    TEST_ASSERT_EQUAL(IK_TOO_CLOSE, kin_inverse(10.0f, 0.0f, ELBOW_DOWN, &j));
}

static void test_por_debajo_de_la_mesa(void)
{
    Joints_t j;
    TEST_ASSERT_EQUAL(IK_BELOW_TABLE, kin_inverse(0.0f, -300.0f, ELBOW_DOWN, &j));
}

/* --- EL CASO QUE JUSTIFICA VALIDAR LA RECTA COMPLETA --------------------
 * Los dos extremos son individualmente alcanzables, pero la recta que los une
 * se mete hacia el interior del anillo y a mitad de camino deja de serlo. Sin
 * esta validacion previa, MOVL arrancaria y el brazo se quedaria tirado a
 * media trayectoria en una pose arbitraria.
 *
 * (Este es el motivo por el que la recta bulge HACIA DENTRO: la cuerda entre
 * dos puntos de una corona circular siempre se acerca mas al centro que sus
 * extremos. La violacion por exceso de alcance no puede darse a mitad de
 * camino, solo en los extremos.) */
static void test_recta_valida_en_extremos_falla_en_medio(void)
{
    Point_t a = {  80.0f, 130.0f };
    Point_t b = { 140.0f,  45.0f };

    /* Los dos extremos, por separado, son perfectamente alcanzables */
    TEST_ASSERT_EQUAL(IK_OK, kin_inverse(a.x, a.y, ELBOW_UP, NULL));
    TEST_ASSERT_EQUAL(IK_OK, kin_inverse(b.x, b.y, ELBOW_UP, NULL));

    /* ...y sin embargo la recta entre ellos no lo es */
    Point_t fallo;
    IkResult_t r = kin_validate_line(a, b, ELBOW_UP, 40, &fallo);
    TEST_ASSERT_TRUE(r != IK_OK);

    /* el punto que falla esta por dentro del alcance efectivo */
    TEST_ASSERT_TRUE(hypotf(fallo.x, fallo.y) < kin_effective_r_min() + 1.0f);
}

/* --- El alcance efectivo lo impone el limite del codo, no la geometria ---
 * Con q2 acotado a +/-135 grados el codo no se cierra del todo, asi que el
 * radio minimo real (141.7 mm) es muy superior al geometrico (50 mm). Si este
 * test empieza a fallar es porque cambiaste A2_MIN_DEG/A2_MAX_DEG, y entonces
 * hay que revisar tambien WS_R_MIN_MM. */
static void test_alcance_efectivo(void)
{
    const float rmin = kin_effective_r_min();
    const float rmax = kin_effective_r_max();

    TEST_ASSERT_FLOAT_WITHIN(0.5f, 141.7f, rmin);
    TEST_ASSERT_TRUE(rmin > fabsf(LINK_L1_MM - LINK_L2_MM));   /* > 50 mm */
    TEST_ASSERT_TRUE(rmax <= LINK_L1_MM + LINK_L2_MM);
    TEST_ASSERT_TRUE(rmax > rmin);

    /* Un punto entre el radio geometrico y el efectivo NO debe aceptarse */
    Joints_t j;
    TEST_ASSERT_TRUE(kin_inverse(100.0f, 0.0f, ELBOW_DOWN, &j) != IK_OK);
}

static void test_recta_valida(void)
{
    Point_t a = { 150.0f, 100.0f };
    Point_t b = { 250.0f, 100.0f };
    TEST_ASSERT_EQUAL(IK_OK, kin_validate_line(a, b, ELBOW_DOWN, 40, NULL));
}

/* --- Conversion grados <-> micropasos ----------------------------------- */
static void test_conversion_pasos(void)
{
    /* Una vuelta completa de articulacion = 16000 micropasos */
    TEST_ASSERT_EQUAL_INT32(16000, kin_deg_to_steps(360.0f));
    TEST_ASSERT_EQUAL_INT32(4000,  kin_deg_to_steps(90.0f));
    TEST_ASSERT_EQUAL_INT32(-4000, kin_deg_to_steps(-90.0f));

    /* 1 grado = 44.444 micropasos -> redondea a 44, no a 45.
     * Y el redondeo debe ser SIMETRICO: un cast a int truncaria hacia cero y
     * sesgaria sistematicamente los movimientos negativos medio paso. */
    TEST_ASSERT_EQUAL_INT32( 44, kin_deg_to_steps( 1.0f));
    TEST_ASSERT_EQUAL_INT32(-44, kin_deg_to_steps(-1.0f));
    for (float d = 0.5f; d < 120.0f; d += 7.3f)
        TEST_ASSERT_EQUAL_INT32(-kin_deg_to_steps(d), kin_deg_to_steps(-d));

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 90.0f, kin_steps_to_deg(4000));
}

/* --- Limites articulares ------------------------------------------------ */
static void test_limites_articulares(void)
{
    TEST_ASSERT_TRUE (kin_joints_in_limits(0.0f, 0.0f));
    TEST_ASSERT_TRUE (kin_joints_in_limits(A1_MIN_DEG, A2_MAX_DEG));
    TEST_ASSERT_FALSE(kin_joints_in_limits(A1_MIN_DEG - 1.0f, 0.0f));
    TEST_ASSERT_FALSE(kin_joints_in_limits(0.0f, A2_MAX_DEG + 1.0f));
}

/* --- La pose de home debe ser alcanzable y estar dentro del espacio ----- */
static void test_pose_de_home_es_valida(void)
{
    TEST_ASSERT_TRUE(kin_joints_in_limits(A1_HOME_DEG, A2_HOME_DEG));
    Point_t p = kin_forward(A1_HOME_DEG, A2_HOME_DEG);
    TEST_ASSERT_EQUAL(IK_OK, kin_point_in_workspace(p.x, p.y));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_fk_brazo_estirado);
    RUN_TEST(test_fk_hombro_vertical);
    RUN_TEST(test_fk_codo_plegado);
    RUN_TEST(test_fk_codo_90);
    RUN_TEST(test_ik_roundtrip_codo_abajo);
    RUN_TEST(test_ik_roundtrip_codo_arriba);
    RUN_TEST(test_ik_dos_ramas_mismo_punto);
    RUN_TEST(test_fuera_de_alcance);
    RUN_TEST(test_demasiado_cerca);
    RUN_TEST(test_por_debajo_de_la_mesa);
    RUN_TEST(test_recta_valida_en_extremos_falla_en_medio);
    RUN_TEST(test_alcance_efectivo);
    RUN_TEST(test_recta_valida);
    RUN_TEST(test_conversion_pasos);
    RUN_TEST(test_limites_articulares);
    RUN_TEST(test_pose_de_home_es_valida);
    return UNITY_END();
}
