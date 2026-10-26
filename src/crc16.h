/* =============================================================================
 * crc16.h  -  CRC-16/CCITT-FALSE
 * =============================================================================
 * Polinomio 0x1021, valor inicial 0xFFFF, sin reflexion, sin XOR final.
 * Vector de prueba estandar: "123456789" -> 0x29B1.
 *
 * Por que CRC y no una suma (checksum): una suma no ve dos bytes
 * intercambiados ni dos errores que se compensan. El CRC-16 detecta todos los
 * errores de 1 o 2 bits, todos los de un numero impar de bits y toda rafaga
 * de hasta 16 bits, que es el patron tipico del ruido en una linea serie.
 *
 * Calculo bit a bit, sin tabla: 41 bytes por trama como maximo son unos pocos
 * microsegundos en el ESP32, y se ahorran 512 bytes de tabla.
 *
 * Matematica pura, sin Arduino: se prueba en la PC con pio test -e native.
 * ---------------------------------------------------------------------------*/
#pragma once
#include <stdint.h>
#include <stddef.h>

#define CRC16_INIT 0xFFFFu

uint16_t crc16_update(uint16_t crc, uint8_t byte);
uint16_t crc16(const uint8_t *data, size_t len);
