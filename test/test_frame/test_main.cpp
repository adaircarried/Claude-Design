/* =============================================================================
 *  Tests del protocolo binario  -  corren en la PC, SIN ESP32
 * =============================================================================
 *      pio test -e native
 *
 *  Cubren los criterios de la U2 que se pueden probar sin hardware:
 *    - Integridad (20 pts): rechazo de tramas corruptas e incompletas
 *    - Decodificacion (30 pts): tramas pegadas, ruido, resincronizacion
 *  Los mismos vectores sirven para comprobar que el pendant de Python
 *  construye exactamente las mismas tramas.
 * ---------------------------------------------------------------------------*/
#include <unity.h>
#include <string.h>
#include <stdlib.h>
#include "crc16.h"
#include "frame.h"

void setUp(void) {}
void tearDown(void) {}

/* Alimenta un buffer completo al parser, con el reloj avanzando 1 ms por byte,
 * y devuelve el ultimo resultado distinto de FP_NONE. */
static FpResult_t feed_all(FrameParser_t *p, const uint8_t *b, size_t n,
                           uint32_t *t, Frame_t *out)
{
    FpResult_t last = FP_NONE;
    for (size_t i = 0; i < n; ++i) {
        FpResult_t r = fp_feed(p, b[i], (*t)++, out);
        if (r != FP_NONE) last = r;
    }
    return last;
}

/* --- CRC ---------------------------------------------------------------- */
static void test_crc_vector_estandar(void)
{
    /* Vector oficial de CRC-16/CCITT-FALSE. Si este falla, el pendant y el
     * firmware no se van a entender nunca. */
    TEST_ASSERT_EQUAL_HEX16(0x29B1, crc16((const uint8_t *)"123456789", 9));
}

/* --- Ida y vuelta -------------------------------------------------------- */
static void test_construir_y_decodificar(void)
{
    const uint8_t pl[9] = { 0x10, 0x27, 0, 0,  0xF0, 0xD8, 0xFF, 0xFF,  50 };
    uint8_t buf[FRAME_MAX_SIZE];
    size_t n = frame_build(buf, sizeof buf, 7, 0x11, 0, 0x02, pl, sizeof pl);
    TEST_ASSERT_EQUAL(FRAME_OVERHEAD + 9, n);
    TEST_ASSERT_EQUAL_UINT8(FRAME_SOF, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(FRAME_EOF, buf[n - 1]);

    FrameParser_t p; fp_init(&p); Frame_t f; uint32_t t = 0;
    TEST_ASSERT_EQUAL(FP_FRAME, feed_all(&p, buf, n, &t, &f));
    TEST_ASSERT_EQUAL_UINT8(7, f.seq);
    TEST_ASSERT_EQUAL_UINT8(0x11, f.cmd);
    TEST_ASSERT_EQUAL_UINT8(0, f.motor);
    TEST_ASSERT_EQUAL_UINT8(0x02, f.flags);
    TEST_ASSERT_EQUAL_UINT8(9, f.len);
    TEST_ASSERT_EQUAL_INT32(10000, rd_i32(&f.payload[0]));
    TEST_ASSERT_EQUAL_INT32(-10000, rd_i32(&f.payload[4]));
    TEST_ASSERT_FALSE(fp_busy(&p));
}

static void test_trama_sin_payload(void)
{
    uint8_t buf[FRAME_MAX_SIZE];
    size_t n = frame_build(buf, sizeof buf, 1, 0x01, 0, 0, NULL, 0);
    TEST_ASSERT_EQUAL(FRAME_OVERHEAD, n);
    FrameParser_t p; fp_init(&p); Frame_t f; uint32_t t = 0;
    TEST_ASSERT_EQUAL(FP_FRAME, feed_all(&p, buf, n, &t, &f));
    TEST_ASSERT_EQUAL_UINT8(0x01, f.cmd);
}

static void test_payload_maximo(void)
{
    uint8_t pl[FRAME_MAX_PAYLOAD];
    for (int i = 0; i < FRAME_MAX_PAYLOAD; ++i) pl[i] = (uint8_t)(i * 7);
    uint8_t buf[FRAME_MAX_SIZE];
    size_t n = frame_build(buf, sizeof buf, 9, 0x83, 0, 0, pl, FRAME_MAX_PAYLOAD);
    TEST_ASSERT_EQUAL(FRAME_MAX_SIZE, n);
    FrameParser_t p; fp_init(&p); Frame_t f; uint32_t t = 0;
    TEST_ASSERT_EQUAL(FP_FRAME, feed_all(&p, buf, n, &t, &f));
    TEST_ASSERT_EQUAL_MEMORY(pl, f.payload, FRAME_MAX_PAYLOAD);
}

static void test_construir_rechaza_payload_excesivo(void)
{
    uint8_t pl[FRAME_MAX_PAYLOAD + 1] = {0};
    uint8_t buf[FRAME_MAX_SIZE + 8];
    TEST_ASSERT_EQUAL(0, frame_build(buf, sizeof buf, 0, 1, 0, 0, pl, FRAME_MAX_PAYLOAD + 1));
    TEST_ASSERT_EQUAL(0, frame_build(buf, 5, 0, 1, 0, 0, NULL, 0));   /* buffer chico */
}

/* --- Integridad (20 pts) ------------------------------------------------- */
static void test_rechaza_payload_alterado(void)
{
    const uint8_t pl[4] = { 1, 2, 3, 4 };
    uint8_t buf[FRAME_MAX_SIZE];
    size_t n = frame_build(buf, sizeof buf, 3, 0x10, 1, 0, pl, 4);
    buf[7] ^= 0x01;                                  /* un solo bit cambiado */
    FrameParser_t p; fp_init(&p); Frame_t f; uint32_t t = 0;
    TEST_ASSERT_EQUAL(FP_ERR_CRC, feed_all(&p, buf, n, &t, &f));
    TEST_ASSERT_EQUAL_UINT8(3, p.err_seq);           /* el NACK puede citarla */
}

static void test_rechaza_crc_alterado(void)
{
    uint8_t buf[FRAME_MAX_SIZE];
    size_t n = frame_build(buf, sizeof buf, 3, 0x02, 0, 0, NULL, 0);
    buf[n - 3] ^= 0x80;
    FrameParser_t p; fp_init(&p); Frame_t f; uint32_t t = 0;
    TEST_ASSERT_EQUAL(FP_ERR_CRC, feed_all(&p, buf, n, &t, &f));
}

static void test_rechaza_bytes_intercambiados(void)
{
    /* Una suma simple (checksum) NO veria esto; el CRC si. */
    const uint8_t pl[2] = { 0x12, 0x34 };
    uint8_t buf[FRAME_MAX_SIZE];
    size_t n = frame_build(buf, sizeof buf, 0, 0x10, 1, 0, pl, 2);
    uint8_t tmp = buf[6]; buf[6] = buf[7]; buf[7] = tmp;
    FrameParser_t p; fp_init(&p); Frame_t f; uint32_t t = 0;
    TEST_ASSERT_EQUAL(FP_ERR_CRC, feed_all(&p, buf, n, &t, &f));
}

static void test_rechaza_eof_ausente(void)
{
    uint8_t buf[FRAME_MAX_SIZE];
    size_t n = frame_build(buf, sizeof buf, 0, 0x01, 0, 0, NULL, 0);
    buf[n - 1] = 0x00;
    FrameParser_t p; fp_init(&p); Frame_t f; uint32_t t = 0;
    TEST_ASSERT_EQUAL(FP_ERR_FRAMING, feed_all(&p, buf, n, &t, &f));
}

static void test_rechaza_len_excesivo_al_instante(void)
{
    /* El LEN se valida en cuanto llega: no se espera a tragarse 200 bytes. */
    FrameParser_t p; fp_init(&p); Frame_t f;
    TEST_ASSERT_EQUAL(FP_NONE,        fp_feed(&p, FRAME_SOF, 0, &f));
    TEST_ASSERT_EQUAL(FP_ERR_FRAMING, fp_feed(&p, 200, 1, &f));
    TEST_ASSERT_FALSE(fp_busy(&p));
}

static void test_trama_incompleta_por_timeout(void)
{
    uint8_t buf[FRAME_MAX_SIZE];
    const uint8_t pl[4] = { 9, 9, 9, 9 };
    size_t n = frame_build(buf, sizeof buf, 5, 0x10, 1, 0, pl, 4);
    FrameParser_t p; fp_init(&p); Frame_t f; uint32_t t = 0;

    /* Llega solo la mitad y la PC se calla */
    TEST_ASSERT_EQUAL(FP_NONE, feed_all(&p, buf, n / 2, &t, &f));
    TEST_ASSERT_TRUE(fp_busy(&p));
    TEST_ASSERT_EQUAL(FP_NONE,        fp_poll_timeout(&p, t + FRAME_INTERBYTE_MS - 1));
    TEST_ASSERT_EQUAL(FP_ERR_TIMEOUT, fp_poll_timeout(&p, t + FRAME_INTERBYTE_MS + 5));
    TEST_ASSERT_FALSE(fp_busy(&p));
    TEST_ASSERT_EQUAL(FP_NONE, fp_poll_timeout(&p, t + 100));   /* se reporta una vez */

    /* Y la siguiente trama completa entra sin problema */
    t += 100;
    TEST_ASSERT_EQUAL(FP_FRAME, feed_all(&p, buf, n, &t, &f));
}

static void test_bytes_lentos_pero_a_tiempo_no_son_timeout(void)
{
    uint8_t buf[FRAME_MAX_SIZE];
    size_t n = frame_build(buf, sizeof buf, 1, 0x01, 0, 0, NULL, 0);
    FrameParser_t p; fp_init(&p); Frame_t f; FpResult_t r = FP_NONE;
    for (size_t i = 0; i < n; ++i) r = fp_feed(&p, buf[i], (uint32_t)(i * (FRAME_INTERBYTE_MS - 1)), &f);
    TEST_ASSERT_EQUAL(FP_FRAME, r);
}

/* --- Decodificacion (30 pts) -------------------------------------------- */
static void test_sof_dentro_del_payload(void)
{
    /* 0xAA y 0x55 como datos: la longitud la manda LEN, no el delimitador */
    const uint8_t pl[6] = { FRAME_SOF, FRAME_EOF, FRAME_SOF, 0x00, FRAME_SOF, FRAME_EOF };
    uint8_t buf[FRAME_MAX_SIZE];
    size_t n = frame_build(buf, sizeof buf, 0xAA, 0x12, 0, 0, pl, 6);
    FrameParser_t p; fp_init(&p); Frame_t f; uint32_t t = 0;
    TEST_ASSERT_EQUAL(FP_FRAME, feed_all(&p, buf, n, &t, &f));
    TEST_ASSERT_EQUAL_MEMORY(pl, f.payload, 6);
    TEST_ASSERT_EQUAL_UINT8(0xAA, f.seq);
}

static void test_tramas_pegadas(void)
{
    uint8_t stream[3 * FRAME_MAX_SIZE]; size_t n = 0;
    for (uint8_t s = 0; s < 3; ++s) {
        const uint8_t pl[1] = { (uint8_t)(s * 10) };
        n += frame_build(stream + n, sizeof stream - n, s, 0x02, 0, 0, pl, 1);
    }
    FrameParser_t p; fp_init(&p); Frame_t f; int frames = 0;
    for (size_t i = 0; i < n; ++i)
        if (fp_feed(&p, stream[i], (uint32_t)i, &f) == FP_FRAME) {
            TEST_ASSERT_EQUAL_UINT8(frames, f.seq);
            TEST_ASSERT_EQUAL_UINT8(frames * 10, f.payload[0]);
            ++frames;
        }
    TEST_ASSERT_EQUAL(3, frames);
}

static void test_basura_antes_de_la_trama(void)
{
    uint8_t stream[64] = { 'h', 'o', 'l', 'a', 0x00, 0xFF, 0x55, 0x13 };
    size_t n = 8 + frame_build(stream + 8, sizeof stream - 8, 4, 0x01, 0, 0, NULL, 0);
    FrameParser_t p; fp_init(&p); Frame_t f; uint32_t t = 0;
    TEST_ASSERT_EQUAL(FP_FRAME, feed_all(&p, stream, n, &t, &f));
    TEST_ASSERT_EQUAL_UINT8(4, f.seq);
}

static void test_resincroniza_con_sof_que_rompe_la_trama(void)
{
    /* Una trama queda cortada y, justo donde esperaba su EOF, empieza otra.
     * El 0xAA que "rompe" la primera se toma como inicio de la segunda. */
    uint8_t a[FRAME_MAX_SIZE], b[FRAME_MAX_SIZE];
    size_t na = frame_build(a, sizeof a, 1, 0x01, 0, 0, NULL, 0);
    size_t nb = frame_build(b, sizeof b, 2, 0x02, 0, 0, NULL, 0);
    uint8_t stream[2 * FRAME_MAX_SIZE];
    memcpy(stream, a, na - 1);                 /* la primera sin su EOF */
    memcpy(stream + na - 1, b, nb);
    FrameParser_t p; fp_init(&p); Frame_t f; int frames = 0, errores = 0;
    for (size_t i = 0; i < na - 1 + nb; ++i) {
        FpResult_t r = fp_feed(&p, stream[i], (uint32_t)i, &f);
        if (r == FP_FRAME) { ++frames; TEST_ASSERT_EQUAL_UINT8(2, f.seq); }
        if (r == FP_ERR_FRAMING) ++errores;
    }
    TEST_ASSERT_EQUAL(1, errores);
    TEST_ASSERT_EQUAL(1, frames);
}

static void test_ruido_aleatorio_no_rompe_nada(void)
{
    /* 200 000 bytes de ruido con semilla fija: el parser nunca se cuelga y,
     * tras una pausa, la siguiente trama valida entra. Una trama aleatoria
     * solo pasaria con un SOF, un LEN valido, un EOF y un CRC correcto a la
     * vez: en la practica no ocurre (aqui se exige que no ocurra). */
    srand(1234);
    FrameParser_t p; fp_init(&p); Frame_t f; uint32_t t = 0;
    int falsos = 0;
    for (int i = 0; i < 200000; ++i)
        if (fp_feed(&p, (uint8_t)(rand() & 0xFF), t++, &f) == FP_FRAME) ++falsos;
    TEST_ASSERT_EQUAL(0, falsos);

    uint8_t buf[FRAME_MAX_SIZE];
    size_t n = frame_build(buf, sizeof buf, 77, 0x01, 0, 0, NULL, 0);
    t += FRAME_INTERBYTE_MS + 1;               /* la PC deja un hueco entre tramas */
    TEST_ASSERT_EQUAL(FP_FRAME, feed_all(&p, buf, n, &t, &f));
    TEST_ASSERT_EQUAL_UINT8(77, f.seq);
}

static void test_enteros_little_endian(void)
{
    uint8_t b[4];
    wr_i32(b, -2); TEST_ASSERT_EQUAL_UINT8(0xFE, b[0]); TEST_ASSERT_EQUAL_UINT8(0xFF, b[3]);
    TEST_ASSERT_EQUAL_INT32(-2, rd_i32(b));
    wr_i16(b, -1500); TEST_ASSERT_EQUAL(-1500, rd_i16(b));
    wr_u16(b, 0xBEEF); TEST_ASSERT_EQUAL_UINT8(0xEF, b[0]); TEST_ASSERT_EQUAL_UINT16(0xBEEF, rd_u16(b));
}

/* Vector de referencia para el pendant: PING con SEQ 1.
 * Python debe producir exactamente estos 9 bytes. */
static void test_vector_ping_para_python(void)
{
    uint8_t buf[FRAME_MAX_SIZE];
    size_t n = frame_build(buf, sizeof buf, 1, 0x01, 0, 0, NULL, 0);
    const uint16_t crc = crc16(&buf[1], 5);
    printf("    vector PING seq=1: ");
    for (size_t i = 0; i < n; ++i) printf("%02X ", buf[i]);
    printf("(CRC 0x%04X)\n", crc);
    TEST_ASSERT_EQUAL(9, n);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_crc_vector_estandar);
    RUN_TEST(test_construir_y_decodificar);
    RUN_TEST(test_trama_sin_payload);
    RUN_TEST(test_payload_maximo);
    RUN_TEST(test_construir_rechaza_payload_excesivo);
    RUN_TEST(test_rechaza_payload_alterado);
    RUN_TEST(test_rechaza_crc_alterado);
    RUN_TEST(test_rechaza_bytes_intercambiados);
    RUN_TEST(test_rechaza_eof_ausente);
    RUN_TEST(test_rechaza_len_excesivo_al_instante);
    RUN_TEST(test_trama_incompleta_por_timeout);
    RUN_TEST(test_bytes_lentos_pero_a_tiempo_no_son_timeout);
    RUN_TEST(test_sof_dentro_del_payload);
    RUN_TEST(test_tramas_pegadas);
    RUN_TEST(test_basura_antes_de_la_trama);
    RUN_TEST(test_resincroniza_con_sof_que_rompe_la_trama);
    RUN_TEST(test_ruido_aleatorio_no_rompe_nada);
    RUN_TEST(test_enteros_little_endian);
    RUN_TEST(test_vector_ping_para_python);
    return UNITY_END();
}
