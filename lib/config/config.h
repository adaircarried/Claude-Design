// Configuración del hardware y constantes del péndulo.
// Única fuente de verdad para pines y parámetros: todos los entornos la incluyen.
// Unidades SI (m, s, rad).
#pragma once

#include <stdint.h>

namespace cfg {

// ---------------------------------------------------------------- Pines
constexpr int PIN_ENC_A = 32;   // verde, pull-up externa 4.7k a 3.3 V
constexpr int PIN_ENC_B = 33;   // blanco, pull-up externa 4.7k a 3.3 V
constexpr int PIN_STEP = 25;
constexpr int PIN_DIR = 26;
constexpr int PIN_ENABLE = 27;  // activo bajo
constexpr int PIN_FC_IZQ = -1;  // final de carrera izquierdo: por definir
constexpr int PIN_FC_DER = -1;  // final de carrera derecho: por definir

// -------------------------------------------------------------- Encoder
// LPD3806-600BM: 600 PPR en cuadratura completa (x4)
constexpr int32_t ENC_CUENTAS_VUELTA = 2400;
// Con el péndulo colgando como referencia (cuenta 0), la vertical arriba está a media vuelta
constexpr int32_t ENC_CUENTAS_ARRIBA = ENC_CUENTAS_VUELTA / 2;
// +1 o -1: ajustar para que theta > 0 hacia donde avanza el carro con a > 0
constexpr int ENC_SIGNO = +1;
// Filtro de glitches del PCNT en ciclos de APB (80 MHz), máximo 1023 = 12.8 us.
// El péndulo cayendo gira < 2 rev/s (~250 us entre flancos), así que el máximo es seguro.
constexpr uint16_t ENC_FILTRO_PCNT = 1023;

// -------------------------------------------------------------- Control
constexpr float TS = 0.002f;                 // periodo de control [s] (500 Hz)
constexpr uint32_t TS_US = 2000;             // mismo periodo en microsegundos
constexpr float FC_DERIVADA_HZ = 30.0f;      // corte del filtro paso bajo de theta_dot [Hz]

constexpr int NUCLEO_CONTROL = 0;
constexpr int PRIORIDAD_CONTROL = 20;        // configMAX_PRIORITIES = 25 en Arduino-ESP32

// ------------------------------------------------------------ Transmisión
constexpr float PASOS_POR_METRO = 40000.0f;  // 200 x 8 micropasos / 0.040 m

// ---------------------------------------------------------------- Modelo
constexpr float G = 9.81f;
constexpr float L = 0.342f;                  // actualizar con medición real [m]

}  // namespace cfg
