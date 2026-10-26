/* =============================================================================
 * frame.h  -  Trama del protocolo binario: construccion y decodificacion
 * =============================================================================
 *
 *   SOF | LEN | SEQ | CMD | MOTOR | FLAGS | PAYLOAD (LEN) | CRC_L CRC_H | EOF
 *  0xAA                                                                  0x55
 *        └──────────────── cubierto por el CRC-16 ───────┘
 *
 * Especificacion completa en docs/PROTOCOLO_U2.md.
 *
 * MATEMATICA PURA, sin Arduino, igual que kinematics.cpp: no lee el puerto,
 * no imprime, no consulta el reloj por su cuenta (el tiempo se le pasa como
 * argumento). Por eso se prueba en la PC con "pio test -e native", inyectando
 * tramas corruptas, cortadas o ruido sin necesidad de hardware. Y por eso mismo
 * el pendant de Python puede implementar exactamente el mismo formato y
 * comprobarse contra los mismos vectores de prueba.
 * ---------------------------------------------------------------------------*/
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "config.h"

typedef struct {
    uint8_t len;
    uint8_t seq;
    uint8_t cmd;
    uint8_t motor;
    uint8_t flags;
    uint8_t payload[FRAME_MAX_PAYLOAD];
} Frame_t;

/* Resultado de alimentar un byte al parser */
typedef enum {
    FP_NONE = 0,       /* el byte se acepto; la trama aun no termina        */
    FP_FRAME,          /* trama completa y valida en *out                   */
    FP_ERR_CRC,        /* trama completa pero el CRC no coincide            */
    FP_ERR_FRAMING,    /* LEN fuera de rango o falta EOF                    */
    FP_ERR_TIMEOUT     /* la trama anterior quedo a medias (ver abajo)      */
} FpResult_t;

typedef struct {
    uint8_t  state;
    uint8_t  idx;          /* bytes de payload leidos                       */
    uint16_t crc_calc;     /* CRC acumulado mientras llega la trama         */
    uint16_t crc_rx;       /* CRC que trae la trama                         */
    uint32_t last_ms;      /* instante del ultimo byte aceptado             */
    uint8_t  err_seq;      /* SEQ de la ultima trama rechazada, o 0xFF si
                              no llego a leerse: va en el NACK              */
    Frame_t  f;            /* trama en construccion                         */
} FrameParser_t;

void fp_init(FrameParser_t *p);

/* Alimenta UN byte. Nunca espera ni bloquea: coste constante por byte.
 *   now_ms : reloj en milisegundos (millis() en el ESP32, simulado en tests)
 *   out    : recibe la trama cuando el resultado es FP_FRAME
 * Si el byte llega mas de FRAME_INTERBYTE_MS despues del anterior a media
 * trama, la trama vieja se descarta (FP_ERR_TIMEOUT) y el byte se procesa
 * como posible inicio de una nueva. */
FpResult_t fp_feed(FrameParser_t *p, uint8_t byte, uint32_t now_ms, Frame_t *out);

/* Comprueba el timeout sin que llegue ningun byte. Se llama en cada tick de
 * TaskComms: si la PC se calla a media trama, el parser no se queda colgado
 * esperando para siempre. Devuelve FP_ERR_TIMEOUT una sola vez, o FP_NONE. */
FpResult_t fp_poll_timeout(FrameParser_t *p, uint32_t now_ms);

/* true si hay una trama a medio recibir */
bool fp_busy(const FrameParser_t *p);

/* Construye una trama completa en buf. Devuelve los bytes escritos, o 0 si
 * el payload excede FRAME_MAX_PAYLOAD o el buffer no alcanza. */
size_t frame_build(uint8_t *buf, size_t cap, uint8_t seq, uint8_t cmd,
                   uint8_t motor, uint8_t flags,
                   const uint8_t *payload, uint8_t len);

/* --- Enteros little-endian en el payload --------------------------------
 * Byte a byte y no con un cast a puntero: el payload no esta alineado y un
 * acceso desalineado de 32 bits es comportamiento indefinido en C++. */
static inline void     wr_u16(uint8_t *b, uint16_t v) { b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); }
static inline void     wr_i16(uint8_t *b, int16_t v)  { wr_u16(b, (uint16_t)v); }
static inline void     wr_u32(uint8_t *b, uint32_t v) { for (int i = 0; i < 4; ++i) b[i] = (uint8_t)(v >> (8 * i)); }
static inline void     wr_i32(uint8_t *b, int32_t v)  { wr_u32(b, (uint32_t)v); }
static inline uint16_t rd_u16(const uint8_t *b) { return (uint16_t)(b[0] | (b[1] << 8)); }
static inline int16_t  rd_i16(const uint8_t *b) { return (int16_t)rd_u16(b); }
static inline uint32_t rd_u32(const uint8_t *b) { return (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
                                                         ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24); }
static inline int32_t  rd_i32(const uint8_t *b) { return (int32_t)rd_u32(b); }
