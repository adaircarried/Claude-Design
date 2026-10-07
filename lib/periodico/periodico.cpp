#ifdef ARDUINO

#include "periodico.h"

namespace {

struct Contexto {
  FuncionPeriodica fn;
  void* arg;
  uint32_t periodo_us;
  uint8_t num_timer;
};

Contexto ctx;
TaskHandle_t tarea = nullptr;
hw_timer_t* timer = nullptr;

portMUX_TYPE mux_stats = portMUX_INITIALIZER_UNLOCKED;
EstadisticasPeriodo stats;
volatile bool pedir_reinicio = true;

void IRAM_ATTR isrTimer() {
  BaseType_t despertar = pdFALSE;
  vTaskNotifyGiveFromISR(tarea, &despertar);
  portYIELD_FROM_ISR(despertar);
}

void cuerpoTarea(void*) {
  // El timer se configura desde la tarea para que su interrupción quede en este mismo núcleo.
  timer = timerBegin(ctx.num_timer, 80, true);  // APB 80 MHz / 80 = 1 tick por us
  timerAttachInterrupt(timer, &isrTimer, true);
  timerAlarmWrite(timer, ctx.periodo_us, true);
  timerAlarmEnable(timer);

  uint32_t t_previo = 0;
  bool primero = true;

  for (;;) {
    // Devuelve cuántas notificaciones había pendientes: > 1 significa ciclos perdidos.
    const uint32_t pendientes = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    const uint32_t t0 = micros();

    ctx.fn(ctx.arg);

    const uint32_t ejecucion = micros() - t0;
    const uint32_t periodo = t0 - t_previo;
    t_previo = t0;

    portENTER_CRITICAL(&mux_stats);
    if (pedir_reinicio) {
      stats = EstadisticasPeriodo{0, UINT32_MAX, 0, 0, 0};
      pedir_reinicio = false;
      primero = true;
    }
    stats.ciclos++;
    if (!primero) {
      if (periodo < stats.periodo_min_us) stats.periodo_min_us = periodo;
      if (periodo > stats.periodo_max_us) stats.periodo_max_us = periodo;
    }
    if (ejecucion > stats.ejecucion_max_us) stats.ejecucion_max_us = ejecucion;
    if (pendientes > 1) stats.atrasos += pendientes - 1;
    portEXIT_CRITICAL(&mux_stats);
    primero = false;
  }
}

}  // namespace

bool iniciarTareaPeriodica(FuncionPeriodica fn, void* arg, uint32_t periodo_us, int nucleo,
                           int prioridad, uint8_t num_timer) {
  ctx = Contexto{fn, arg, periodo_us, num_timer};
  return xTaskCreatePinnedToCore(cuerpoTarea, "periodica", 4096, nullptr, prioridad, &tarea,
                                 nucleo) == pdPASS;
}

EstadisticasPeriodo leerEstadisticasPeriodo() {
  portENTER_CRITICAL(&mux_stats);
  const EstadisticasPeriodo copia = stats;
  portEXIT_CRITICAL(&mux_stats);
  return copia;
}

void reiniciarEstadisticasPeriodo() { pedir_reinicio = true; }

#endif  // ARDUINO
