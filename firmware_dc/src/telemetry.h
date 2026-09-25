// =============================================================================
//  telemetry.h — TaskTelemetry: reporte periódico (núcleo 0, prioridad 1)
// =============================================================================
#pragma once
void telemetry_enable(bool on);
bool telemetry_enabled();
void task_telemetry(void *arg);
