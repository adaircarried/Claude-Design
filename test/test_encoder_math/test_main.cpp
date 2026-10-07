// Pruebas de la matemática del encoder en la PC: pio test -e native
#include <config.h>
#include <encoder_math.h>
#include <unity.h>

using namespace encoder_math;

namespace {

constexpr int32_t N = cfg::ENC_CUENTAS_VUELTA;
constexpr int32_t ARRIBA = cfg::ENC_CUENTAS_ARRIBA;
constexpr float CUENTA_RAD = 2.0f * PI_F / N;

float th(int64_t c, int signo = +1) { return theta(c, N, ARRIBA, signo); }

}  // namespace

void setUp() {}
void tearDown() {}

void test_envolver() {
  TEST_ASSERT_EQUAL_INT32(0, envolverCuentas(0, N));
  TEST_ASSERT_EQUAL_INT32(-1200, envolverCuentas(1200, N));
  TEST_ASSERT_EQUAL_INT32(1199, envolverCuentas(1199, N));
  TEST_ASSERT_EQUAL_INT32(-1200, envolverCuentas(-1200, N));
  TEST_ASSERT_EQUAL_INT32(5, envolverCuentas(5 + 10 * N, N));
  TEST_ASSERT_EQUAL_INT32(-5, envolverCuentas(-5 - 10 * N, N));
}

void test_theta_cero_arriba() {
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, th(ARRIBA));
  // Llegar arriba por cualquier lado, o tras varias vueltas, da el mismo theta
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, th(-ARRIBA));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, th(ARRIBA + 3 * N));
}

void test_theta_colgando_es_pi() {
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, PI_F, fabsf(th(0)));
}

void test_theta_cerca_de_arriba() {
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, CUENTA_RAD, th(ARRIBA + 1));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, -CUENTA_RAD, th(ARRIBA - 1));
  // 30 grados = 200 cuentas
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, PI_F / 6.0f, th(ARRIBA + 200));
}

void test_theta_signo() {
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, -CUENTA_RAD * 10, th(ARRIBA + 10, -1));
}

void test_derivada_rampa_constante() {
  // Velocidad constante de 1 cuenta por periodo: converge a CUENTA_RAD / Ts
  DerivadaFiltrada d;
  d.configurar(cfg::TS, cfg::FC_DERIVADA_HZ);
  for (int i = 0; i < 500; i++) d.actualizar(CUENTA_RAD);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, CUENTA_RAD / cfg::TS, d.valor());
}

void test_derivada_constante_de_tiempo() {
  // Tras un escalón, después de tau segundos se alcanza ~63 % del valor final
  DerivadaFiltrada d;
  const float fc = cfg::FC_DERIVADA_HZ;
  d.configurar(cfg::TS, fc);
  const float tau = 1.0f / (2.0f * PI_F * fc);
  const int pasos = static_cast<int>(tau / cfg::TS + 0.5f);
  for (int i = 0; i < pasos; i++) d.actualizar(1.0f * cfg::TS);  // entrada de 1 rad/s
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.632f, d.valor());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_envolver);
  RUN_TEST(test_theta_cero_arriba);
  RUN_TEST(test_theta_colgando_es_pi);
  RUN_TEST(test_theta_cerca_de_arriba);
  RUN_TEST(test_theta_signo);
  RUN_TEST(test_derivada_rampa_constante);
  RUN_TEST(test_derivada_constante_de_tiempo);
  return UNITY_END();
}
