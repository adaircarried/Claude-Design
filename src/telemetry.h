/* =============================================================================
 * telemetry.h  -  Salida serial y reporte de estado
 * =============================================================================
 *
 * POR QUE TODA LA SALIDA PASA POR AQUI
 * ------------------------------------
 * TaskComms y TaskTelemetry corren ambas en el nucleo 0 y ambas escriben al
 * puerto serial. Si cada una llamara a Serial.print() por su cuenta, sus
 * lineas se entrelazarian a mitad de caracter y el parser del Parcial 2 (que
 * va a leer estas mismas lineas desde la PC) recibiria basura.
 *
 * Todo pasa por report_printf()/report_line(), que serializan con un mutex.
 *
 * REGLA QUE NO SE ROMPE: nada de esto se llama desde el lazo de control de
 * TaskMotion. Serial.print() bloquea hasta que hay espacio en el buffer de
 * transmision; una llamada dentro del lazo de movimiento desincroniza el
 * periodo y produce movimiento a tirones.
 * ---------------------------------------------------------------------------*/

#pragma once
#include <Arduino.h>
#include "types.h"

void telemetry_init(void);
void TaskTelemetry(void *pv);

/* Salida serial protegida por mutex. Seguras desde cualquier tarea.
 * En modo binario no escriben nada (ver telemetry_set_text_muted). */
void report_printf(const char *fmt, ...);
void report_line(const char *s);

/* Escribe una trama binaria ya construida, bajo el mismo mutex que el texto.
 * Devuelve false si no consiguio el puerto en 100 ms. */
bool report_write_frame(const uint8_t *data, size_t len);

/* Modo binario: silencia TODA la salida de texto (eventos, errores, eco,
 * telemetria TLM) para que por el puerto solo viajen tramas. */
void telemetry_set_text_muted(bool muted);
bool telemetry_text_muted(void);

/* Reporte completo de una sola vez (comando STATUS). */
void telemetry_status_report(void);

/* Flujo periodico de telemetria (lineas "TLM ..." cada 100 ms).
 * Apagado por defecto: si estuviera encendido, la consola se llenaria de
 * lineas y seria imposible escribir comandos a mano. En el Parcial 2, con un
 * programa leyendo del otro lado, se enciende y se deja. */
void telemetry_set_stream(bool on);
bool telemetry_get_stream(void);
