# CLAUDE.md — contexto del proyecto

Firmware de un brazo robótico RR de 2 GDL que juega Conecta 4. Proyecto
universitario de Control de Robots, con tres parciales evaluados.

## Estado

- **Parcial 1** — **entregado y calificado con 10, pero con OTRO firmware.**
  Los NEMA 17 no llegaron a tiempo; se entrego con motorreductores DC
  prestados (GM25-370 ~1:45, encoder Hall, driver TB6612FNG, ESP-32S NodeMCU),
  PID a 200 Hz. Ese firmware vive en la rama `claude/funny-lovelace-jp432u`,
  carpeta `firmware_dc/`, y NO comparte codigo con este. Misma arquitectura
  conceptual: mismo `Setpoint_t`, misma cola, mismas 4 tareas y nucleos.
- **Este firmware (steppers + TB6600)** — es la linea principal desde octubre
  2026. Compila y arranca en hardware real; consola y finales de carrera
  probados en banco. **Todavia no ha movido un motor.**
- **Parcial 2** (ultima semana de oct 2026) — protocolo serial binario con
  CRC. Reescribe `comms.cpp`. El buffer circular ya esta en `comms.cpp`.
  Al pasar a binario: `CONSOLE_ECHO` a 0 en `config.h` (el eco corromperia
  las tramas); el eco del monitor se puede dar con `monitor_echo = yes`.
- **Parcial 3** (30 nov 2026) — vision, pinza SG90, logica de juego. Reutiliza
  `kinematics.cpp`. La API de poses en NVS ya esta en `persistence.h`;
  `MOTION_GRIPPER` ya esta en el enum.

## Convencion del codo (corregida en oct 2026)

`ELBOW_UP` = q2 < 0 = el codo queda fisicamente por encima de la recta
hombro-efector. Es el valor por defecto. Una version anterior tenia las
etiquetas invertidas; la clave NVS paso de `elbow` a `elbow2` para descartar
valores guardados con la convencion vieja. `MOVL` exige que el brazo ya este
en la rama configurada (si no, `ERR MOVL_RAMA`: hacer un `MOVJ` antes).

## Reglas que no se rompen

1. **Nadie mueve motores llamando a funciones.** Todo pasa por
   `xQueueSend(setpointQueue, ...)`. Si para añadir una fuente de comandos
   hace falta tocar `motion.cpp`, el diseño se está rompiendo.
2. **`kinematics.cpp` no incluye `<Arduino.h>`.** Es matemática pura, sin
   efectos secundarios. Eso es lo que permite `pio test -e native`.
3. **Nada de `Serial.print` dentro de `TaskMotion`.** Bloquea cuando el buffer
   de transmisión se llena y desincroniza el lazo. Toda salida va por
   `report_printf`/`report_line`, que serializan con mutex.
4. **Toda tarea cede CPU** con `vTaskDelay` o `vTaskDelayUntil`. Una tarea de
   alta prioridad que no cede dispara el watchdog y reinicia el ESP32.
5. **El paro no se encola.** Va por `sv_request_abort()`. Ver `types.h`.
6. **La posición se guarda en micropasos (`int32_t`)**, nunca en grados.
   44.444 pasos/grado no es entero y el error se acumularía.
7. **Ninguna constante fuera de `config.h`.** Ni un número mágico.
8. **`STOP` mantiene el par de retención.** El brazo es vertical: soltarlo lo
   deja caer. Solo `DISABLE` libera, y avisa.

## Comandos

```bash
pio run                  # compilar
pio run -t upload        # flashear
pio device monitor       # consola, 115200
pio test -e native       # tests de cinemática, sin hardware
```

## Hardware

ESP32 DevKit V1 · 2 × NEMA 17 con reductor 5:1 · 2 × TB6600 a 1/16 ·
2 finales de carrera NC · 44.444 micropasos/grado · l1=200 mm, l2=150 mm.

Pines: PUL 25/32, DIR 26/33, ENA 27 (activo bajo), finales 16/17, servo 13 (P3).

## Cosas que hay que ajustar al montar el mecanismo

Están agrupadas y marcadas en `config.h`:

- `A1_MIN_DEG`, `A1_MAX_DEG`, `A2_MIN_DEG`, `A2_MAX_DEG` — límites reales
- `A1_HOMING_DIR`, `A2_HOMING_DIR` — hacia dónde está cada final de carrera
- `A1_HOME_DEG`, `A2_HOME_DEG` — ángulo asignado al tocar el switch
- `WS_Y_MIN_MM` — altura de la mesa
- `A1_DIR_HIGH_COUNTS_UP`, `A2_DIR_HIGH_COUNTS_UP` — sentido de giro

Al cambiar los límites de `q2`, revisar también el alcance efectivo: el
firmware lo reporta al arrancar (ver README §7).
