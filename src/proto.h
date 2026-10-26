/* =============================================================================
 * proto.h  -  Protocolo binario de la U2 (lado del ESP32)
 * =============================================================================
 *
 * Recibe las tramas que valida frame.cpp, comprueba cada orden y la convierte
 * en el MISMO Setpoint_t que usa la consola de texto de la U1. TaskMotion no
 * distingue de donde vino: ese es el criterio de integracion de la rubrica.
 *
 * Todo corre dentro de TaskComms (nucleo 0). Especificacion completa en
 * docs/PROTOCOLO_U2.md.
 *
 * MODOS: el firmware arranca en modo texto (la consola de la U1). La primera
 * trama binaria valida lo pasa a modo binario y silencia el texto. El comando
 * MODE_TEXT, o un reinicio, lo regresan.
 * ---------------------------------------------------------------------------*/
#pragma once
#include <Arduino.h>

void proto_init(void);          /* despues de Serial.begin() y de crear colas */
bool proto_binary_mode(void);

/* ¿Este byte le toca al protocolo y no a la consola de texto?
 * Si: en modo binario, si hay una trama a medias, o si es un SOF al inicio
 * de una linea de texto vacia. */
bool proto_wants_byte(uint8_t b, bool text_line_empty);

void proto_feed(uint8_t b, uint32_t now_ms);   /* un byte por llamada        */
void proto_tick(uint32_t now_ms);              /* timeouts, watchdog, avisos */
void proto_note_overflow(void);                /* el buffer circular se lleno */
