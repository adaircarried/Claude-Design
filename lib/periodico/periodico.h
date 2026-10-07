// Tarea periódica de tiempo real disparada por timer de hardware.
//
// El ISR del timer solo notifica a la tarea; el trabajo se hace en la tarea, fijada a un
// núcleo y con prioridad alta. Así el periodo lo marca el hardware y no vTaskDelay.
#pragma once

#ifdef ARDUINO

#include <Arduino.h>

struct EstadisticasPeriodo {
  uint32_t ciclos;         // iteraciones ejecutadas
  uint32_t periodo_min_us; // periodo medido entre activaciones
  uint32_t periodo_max_us;
  uint32_t ejecucion_max_us;  // duración máxima del cuerpo de la tarea
  uint32_t atrasos;        // activaciones en que el cuerpo no terminó antes del siguiente disparo
};

using FuncionPeriodica = void (*)(void* arg);

// Crea la tarea y arranca el timer. `fn(arg)` se ejecuta una vez por periodo.
// Devuelve false si no se pudo crear la tarea.
bool iniciarTareaPeriodica(FuncionPeriodica fn, void* arg, uint32_t periodo_us, int nucleo,
                           int prioridad, uint8_t num_timer = 0);

// Copia de las estadísticas de temporización (segura desde cualquier tarea).
EstadisticasPeriodo leerEstadisticasPeriodo();

// Reinicia mínimos y máximos.
void reiniciarEstadisticasPeriodo();

#endif  // ARDUINO
