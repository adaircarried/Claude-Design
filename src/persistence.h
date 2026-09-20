/* =============================================================================
 * persistence.h  -  Memoria no volatil (NVS via Preferences)
 * =============================================================================
 *
 * Aisla el acceso a NVS en un solo modulo. Razon de diseño: en el Parcial 3
 * hay que guardar una tabla de poses enseñadas (las 7 columnas del tablero de
 * Conecta 4). Teniendo el acceso aislado, eso es agregar claves aqui y nada
 * mas; la API de poses ya esta declarada abajo.
 *
 * DESGASTE DE FLASH: la NVS del ESP32 aguanta del orden de 10^5 borrados por
 * sector. Escribir la posicion en cada movimiento la destruiria en semanas.
 * Por eso solo se escribe al terminar el homing y ante un SAVE explicito.
 * ---------------------------------------------------------------------------*/

#pragma once
#include <Arduino.h>
#include "types.h"

void persist_init(void);

/* Configuracion del usuario: velocidad y configuracion del codo. */
void persist_save_config(uint16_t speed_pct, ElbowConfig_t elbow);
void persist_load_config(uint16_t *speed_pct, ElbowConfig_t *elbow);

/* Ultima posicion referenciada conocida.
 *
 * ADVERTENCIA: restaurar la posicion tras un corte de energia solo es valido
 * si el brazo NO se movio mientras estuvo apagado, y en un brazo vertical sin
 * freno SIEMPRE se mueve (cae). Por eso el firmware nunca restaura la posicion
 * automaticamente: se guarda para diagnostico y para el comando STATUS. Al
 * arrancar, el robot exige HOME. */
void persist_save_home(int32_t s1, int32_t s2);
bool persist_load_home(int32_t *s1, int32_t *s2);

/* --- PARCIAL 3: tabla de poses enseñadas (7 columnas del tablero) -------- */
#define POSE_TABLE_SIZE 8
void persist_save_pose(uint8_t idx, float q1, float q2);
bool persist_load_pose(uint8_t idx, float *q1, float *q2);

void persist_erase_all(void);
