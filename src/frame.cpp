#include "frame.h"
#include "crc16.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * MAQUINA DE ESTADOS DEL PARSER
 *
 *   S_SOF ─0xAA─► S_LEN ─≤32─► S_SEQ ► S_CMD ► S_MOTOR ► S_FLAGS ─┬─► S_PAYLOAD ─┐
 *     ▲            │>32                                          │ (LEN = 0)    │
 *     │            ▼                                             ▼              ▼
 *     └──────── error ◄──── EOF? ◄──── S_CRC_H ◄──── S_CRC_L ◄───┴──────────────┘
 *
 * Cada llamada a fp_feed procesa un byte y regresa: el parser nunca espera a
 * que llegue la trama completa. Eso es lo que permite decodificar "en tiempo
 * real": el coste por byte es constante y TaskComms nunca se bloquea.
 *
 * El LEN se valida en cuanto llega: un LEN corrupto de 200 haria que el parser
 * se tragara 200 bytes de las tramas siguientes antes de darse cuenta.
 * -------------------------------------------------------------------------*/
enum {
    S_SOF = 0, S_LEN, S_SEQ, S_CMD, S_MOTOR, S_FLAGS,
    S_PAYLOAD, S_CRC_L, S_CRC_H, S_EOF
};

void fp_init(FrameParser_t *p)
{
    memset(p, 0, sizeof(*p));
    p->state   = S_SOF;
    p->err_seq = 0xFF;
}

bool fp_busy(const FrameParser_t *p) { return p->state != S_SOF; }

/* Error a media trama: se recuerda el SEQ (si ya habia llegado) para el NACK
 * y se vuelve a buscar el inicio. */
static FpResult_t fail(FrameParser_t *p, FpResult_t why)
{
    p->err_seq = (p->state > S_SEQ) ? p->f.seq : 0xFF;
    p->state   = S_SOF;
    return why;
}

FpResult_t fp_poll_timeout(FrameParser_t *p, uint32_t now_ms)
{
    /* Resta sin signo: funciona aunque millis() se desborde (cada 49 dias). */
    if (p->state != S_SOF && (uint32_t)(now_ms - p->last_ms) > FRAME_INTERBYTE_MS)
        return fail(p, FP_ERR_TIMEOUT);
    return FP_NONE;
}

FpResult_t fp_feed(FrameParser_t *p, uint8_t b, uint32_t now_ms, Frame_t *out)
{
    /* 1) ¿La trama anterior se quedo a medias? Se descarta, y este byte se
     *    procesa desde cero: puede ser el SOF de una trama nueva. */
    FpResult_t timed_out = fp_poll_timeout(p, now_ms);
    p->last_ms = now_ms;

    switch (p->state) {

    case S_SOF:
        if (b == FRAME_SOF) {
            p->state    = S_LEN;
            p->crc_calc = CRC16_INIT;
            p->idx      = 0;
        }
        /* cualquier otro byte es basura entre tramas: se ignora sin contar */
        return timed_out;

    case S_LEN:
        if (b > FRAME_MAX_PAYLOAD) {
            /* RESINCRONIZACION: si el byte que rompe la trama es un SOF, se
             * toma como el inicio de otra en vez de tirarlo. */
            FpResult_t r = fail(p, FP_ERR_FRAMING);
            if (b == FRAME_SOF) { p->state = S_LEN; p->crc_calc = CRC16_INIT; p->idx = 0; }
            return r;
        }
        p->f.len    = b;
        p->crc_calc = crc16_update(p->crc_calc, b);
        p->state    = S_SEQ;
        return FP_NONE;

    case S_SEQ:   p->f.seq   = b; p->crc_calc = crc16_update(p->crc_calc, b); p->state = S_CMD;   return FP_NONE;
    case S_CMD:   p->f.cmd   = b; p->crc_calc = crc16_update(p->crc_calc, b); p->state = S_MOTOR; return FP_NONE;
    case S_MOTOR: p->f.motor = b; p->crc_calc = crc16_update(p->crc_calc, b); p->state = S_FLAGS; return FP_NONE;

    case S_FLAGS:
        p->f.flags  = b;
        p->crc_calc = crc16_update(p->crc_calc, b);
        p->state    = (p->f.len > 0) ? S_PAYLOAD : S_CRC_L;
        return FP_NONE;

    case S_PAYLOAD:
        /* Un 0xAA dentro del payload es un dato mas: la longitud la manda LEN,
         * no la busqueda del delimitador. */
        p->f.payload[p->idx++] = b;
        p->crc_calc = crc16_update(p->crc_calc, b);
        if (p->idx >= p->f.len) p->state = S_CRC_L;
        return FP_NONE;

    case S_CRC_L: p->crc_rx = b;                         p->state = S_CRC_H; return FP_NONE;
    case S_CRC_H: p->crc_rx |= (uint16_t)((uint16_t)b << 8); p->state = S_EOF;   return FP_NONE;

    case S_EOF:
        if (b != FRAME_EOF) {
            FpResult_t r = fail(p, FP_ERR_FRAMING);
            if (b == FRAME_SOF) { p->state = S_LEN; p->crc_calc = CRC16_INIT; p->idx = 0; }
            return r;
        }
        if (p->crc_rx != p->crc_calc) return fail(p, FP_ERR_CRC);
        if (out) *out = p->f;
        p->state = S_SOF;
        return FP_FRAME;
    }

    /* estado imposible: se reinicia en vez de quedarse atascado */
    return fail(p, FP_ERR_FRAMING);
}

size_t frame_build(uint8_t *buf, size_t cap, uint8_t seq, uint8_t cmd,
                   uint8_t motor, uint8_t flags,
                   const uint8_t *payload, uint8_t len)
{
    if (len > FRAME_MAX_PAYLOAD) return 0;
    const size_t total = (size_t)FRAME_OVERHEAD + len;
    if (!buf || cap < total) return 0;

    size_t i = 0;
    buf[i++] = FRAME_SOF;
    buf[i++] = len;
    buf[i++] = seq;
    buf[i++] = cmd;
    buf[i++] = motor;
    buf[i++] = flags;
    for (uint8_t k = 0; k < len; ++k) buf[i++] = payload[k];

    const uint16_t crc = crc16(&buf[1], (size_t)5 + len);   /* LEN .. fin del payload */
    buf[i++] = (uint8_t)(crc & 0xFF);
    buf[i++] = (uint8_t)(crc >> 8);
    buf[i++] = FRAME_EOF;
    return i;
}
