# Protocolo serial binario — Unidad 2

Especificación del enlace PC ↔ ESP32 del brazo RR. **Borrador para revisión
(26 oct 2026)**; el código se escribe después de aprobarla.

---

## 1. Cómo cumple la rúbrica

| Pts | Criterio | Cómo se cumple | Cómo se demuestra |
|---|---|---|---|
| 10 | Configuración del bus | UART0 a **115200 baudios, 8 bits, paridad PAR, 1 bit de paro (8E1)**, igual en ambos extremos. El comando `PING` devuelve la configuración del firmware y la PC verifica que coincide con la suya | El pendant muestra "Bus 115200 8E1 ✓ sincronizado" al conectar |
| 10 | Formato de trama | Delimitadores `SOF`/`EOF`, campo **MOTOR** (ID de motor), objetivos en **pasos** (`JOG` lleva conteo de pasos + bit de dirección) y campo **FLAGS** | §3 y §4 de este documento |
| 20 | Integridad | **CRC-16/CCITT** sobre toda la trama, **paridad** por byte (hardware), comprobación de `EOF` y de `LEN`, y **timeout entre bytes** para tramas incompletas | Panel de inyección de fallas: CRC corrupto, trama cortada, byte basura → rechazo y contador |
| 30 | Decodificación en tiempo real | Interrupción del UART → buffer interno → **buffer circular** propio (512 B) → **máquina de estados byte a byte**, sin bloqueos → cola de setpoints. Latencia medida y reportada | `DIAG` reporta la latencia trama→setpoint en µs; el pendant muestra el tiempo de ida y vuelta |
| 20 | Manejo de errores | Timeout entre bytes, **desborde** de buffer, **comandos fuera de rango**, cola llena, **pérdida de enlace** (watchdog). Cada uno tiene código de error, respuesta `NACK` y contador; el firmware nunca se reinicia | Ráfaga de 2 KB de basura, valores absurdos, cerrar el pendant a media trayectoria → el robot se detiene |
| 10 | Integración con la U1 | Cada trama válida se convierte en un `Setpoint_t` y entra en la **misma cola** de la U1. `TaskMotion` no cambia de origen | Botón del pendant → el motor gira |

---

## 2. Bus

| Parámetro | Valor | Por qué |
|---|---|---|
| Puerto | UART0 por el USB del ESP32 | El mismo cable que se usa para flashear; no hace falta hardware extra |
| Baudios | 115 200 | Una trama máxima (41 B) tarda 3.9 ms; sobra para jog y subrutinas |
| Datos | 8 bits | Bytes binarios completos |
| Paridad | **Par (E)** | Detección por hardware de un bit cambiado en cada byte, antes incluso del CRC |
| Paro | 1 bit | Estándar |

En el monitor de PlatformIO: `monitor_parity = E`. En Python:
`serial.Serial(port, 115200, bytesize=8, parity='E', stopbits=1)`.

**Sincronía:** al conectar, la PC envía `PING`. El `PONG` trae baudios, bits,
paridad, bits de paro y pasos por vuelta del firmware. Si algo no coincide, el
pendant no deja mandar movimientos. Si la paridad no coincidiera, ni siquiera
llegaría un `PONG` legible: el pendant lo reporta como "sin sincronía".

---

## 3. Trama

Igual en los dos sentidos. Enteros multibyte en **little-endian**.

```
 byte:  0     1     2     3     4      5      6 ... 5+N    6+N  7+N   8+N
       ┌─────┬─────┬─────┬─────┬──────┬──────┬───────────┬─────┬─────┬─────┐
       │ SOF │ LEN │ SEQ │ CMD │MOTOR │FLAGS │  PAYLOAD  │CRC_L│CRC_H│ EOF │
       │0xAA │  N  │     │     │      │      │  N bytes  │     │     │0x55 │
       └─────┴─────┴─────┴─────┴──────┴──────┴───────────┴─────┴─────┴─────┘
                    └──────────── cubierto por el CRC ──────────┘
```

| Campo | Tamaño | Significado |
|---|---|---|
| `SOF` | 1 | Inicio de trama, siempre `0xAA`. No es ASCII, así que no se confunde con la consola de texto |
| `LEN` | 1 | Bytes de `PAYLOAD`, de 0 a 32. Mayor que 32 = trama inválida |
| `SEQ` | 1 | Número de secuencia 0–255 que pone la PC. Las respuestas lo devuelven, así la PC sabe a qué orden responden |
| `CMD` | 1 | Código de comando (§4) o de respuesta (§5) |
| `MOTOR` | 1 | **ID de motor**: `0` = ambos / no aplica, `1` = hombro (J1), `2` = codo (J2) |
| `FLAGS` | 1 | **Banderas de comando** (abajo) |
| `PAYLOAD` | N | Datos del comando |
| `CRC` | 2 | **CRC-16/CCITT-FALSE** (polinomio `0x1021`, inicial `0xFFFF`) de `LEN` hasta el último byte de `PAYLOAD`. Vector de prueba: `"123456789"` → `0x29B1` |
| `EOF` | 1 | Fin de trama, siempre `0x55` |

Tamaño total: `9 + N` bytes. Sin payload, 9 bytes.

**FLAGS:**

| Bit | Nombre | Uso |
|---|---|---|
| 0 | `DIR` | Dirección en `JOG`: 0 = positiva, 1 = negativa |
| 1 | `NOTIFY` | Avisar con `DONE` cuando el movimiento **termine**. Es lo que usa la subrutina para saber que ya soltó la ficha |
| 2–7 | — | Reservados, deben ir en 0 |

---

## 4. Comandos (PC → robot)

| CMD | Nombre | MOTOR | Payload | Va a la cola |
|---|---|---|---|---|
| `0x01` | `PING` | 0 | — | no |
| `0x02` | `GET_STATUS` | 0 | — | no |
| `0x03` | `GET_DIAG` | 0 | — | no |
| `0x04` | `MODE_TEXT` | 0 | — | no |
| `0x10` | `JOG` | 1 o 2 | `u32 pasos`, `u8 vel%` | sí |
| `0x11` | `MOVJ` | 0 | `i32 pasos_J1`, `i32 pasos_J2`, `u8 vel%` (absolutos) | sí |
| `0x12` | `MOVL` | 0 | `i16 x`, `i16 y` en décimas de mm, `u8 vel%` | sí |
| `0x13` | `GRIPPER` | 0 | `u8 ángulo`, `u16 espera_ms` | sí — **reservado U3** |
| `0x20` | `STOP` | 0 | — | **no: fuera de banda** |
| `0x21` | `ENABLE` | 0 | — | no |
| `0x22` | `DISABLE` | 0 | — | no |
| `0x23` | `SETZERO` | 0 | `i32 pasos_J1`, `i32 pasos_J2` | sí |
| `0x24` | `CLEAR` | 0 | — | no |

- **Objetivos en pasos, no en grados.** Es lo que pide la rúbrica para
  steppers y coincide con la regla del firmware de guardar la posición en
  micropasos. La PC convierte con los pasos por vuelta que le dio `PING`
  (16 000 → 44.444 pasos/°).
- **`JOG`** es el caso literal "conteo de pasos + dirección": magnitud en el
  payload, signo en `FLAGS.DIR`.
- **`STOP` no se encola**, igual que en la U1: llama a `sv_request_abort()`,
  frena con rampa, mantiene el par y vacía la cola.
- **`SETZERO`** sustituye al homing con finales de carrera: "la pose en la que
  estás ahora vale estos pasos". Ver §9.
- **`GRIPPER`** responde `NACK NO_IMPLEMENTADO` hasta la U3. Se define ya para
  que la numeración no cambie. Lleva una espera porque un SG90 tarda unos
  300 ms en cerrar: sin ella el robot subiría antes de sujetar la ficha.

---

## 5. Respuestas (robot → PC)

Toda respuesta devuelve el `SEQ` de la orden que contesta.

| CMD | Nombre | Cuándo | Payload |
|---|---|---|---|
| `0x81` | `PONG` | a `PING` | `u8 versión`, `u32 baudios`, `u8 bits`, `u8 paridad` (0 N, 1 E, 2 O), `u8 paro`, `u16 pasos_por_vuelta` |
| `0x82` | `STATUS` | a `GET_STATUS` | `u8 estado`, `u8 fallo`, `u8 banderas`, `u8 en_cola`, `u8 último_seq_terminado`, `i32 pasos_J1`, `i32 pasos_J2`, `i16 x`, `i16 y` (0.1 mm) |
| `0x83` | `DIAG` | a `GET_DIAG` | contadores `u16`: tramas_ok, err_crc, err_trama, err_timeout, err_desborde, err_rango, err_paridad, latencia_µs_máx, latencia_µs_última |
| `0x90` | `ACK` | orden aceptada | `u8 cmd` |
| `0x91` | `NACK` | orden rechazada | `u8 cmd`, `u8 código` (§6) |
| `0x92` | `DONE` | terminó un movimiento con `NOTIFY` | — |
| `0x93` | `FAILED` | un movimiento aceptado no pudo ejecutarse | `u8 código` |

`banderas` del `STATUS`: bit 0 = cero fijado, bit 1 = drivers energizados,
bit 2 = en movimiento.

**`ACK` no significa "terminado"**, significa "entró a la cola". Que terminó lo
dice `DONE`. Y si al ejecutarse resulta imposible (por ejemplo, un `MOVL` cuya
recta sale del espacio de trabajo desde donde acabó el movimiento anterior), lo
dice `FAILED`. **Ningún error es silencioso.**

---

## 6. Códigos de error

| Código | Nombre | Causa |
|---|---|---|
| `0x01` | `CRC` | El CRC calculado no coincide |
| `0x02` | `TRAMA` | Falta `EOF` o `LEN` > 32 |
| `0x03` | `TIMEOUT` | Trama incompleta: más de 20 ms entre dos bytes |
| `0x04` | `CMD_DESCONOCIDO` | Código de comando no definido |
| `0x05` | `MOTOR_INVALIDO` | `MOTOR` no vale 0, 1 o 2, o no corresponde al comando |
| `0x06` | `LONGITUD` | `LEN` no corresponde al comando |
| `0x07` | `FUERA_DE_RANGO` | Objetivo fuera de los límites articulares, del espacio de trabajo, o velocidad fuera de 1–100 |
| `0x08` | `COLA_LLENA` | Ya hay 16 movimientos pendientes |
| `0x09` | `SIN_CERO` | Movimiento absoluto sin `SETZERO` previo |
| `0x0A` | `DRIVERS_APAGADOS` | Movimiento con los drivers liberados |
| `0x0B` | `FALLO_ACTIVO` | Hay un fallo pendiente: primero `CLEAR` |
| `0x0C` | `DESBORDE_RX` | Se llenó el buffer de recepción |
| `0x0D` | `NO_IMPLEMENTADO` | Comando reservado (`GRIPPER` hasta la U3) |
| `0x0E` | `RAMA_CODO` | `MOVL` desde la rama del codo contraria |
| `0x0F` | `TRAYECTORIA` | La recta del `MOVL` sale del espacio de trabajo |

Para `CRC`, `TRAMA` y `TIMEOUT` el `SEQ` del `NACK` puede venir corrupto (es
el que se leyó); la PC lo toma como orientativo.

---

## 7. Decodificación

```
  UART (interrupción) → buffer del driver → buffer circular 512 B → parser → cola de setpoints
                                              (TaskComms, núcleo 0, cada 1 ms)
```

Máquina de estados, **un byte por paso**, sin esperar nunca a que llegue la
trama completa:

```
  ESPERA_SOF ──0xAA──► LEN ──≤32──► CABECERA (SEQ CMD MOTOR FLAGS) ──► PAYLOAD (N) ──► CRC (2) ──► EOF
      ▲                 │>32                                                              │0x55     │otro
      │                 ▼                                                                 ▼         ▼
      └────────────── error: contar, NACK, volver a buscar SOF ◄────────────────── validar CRC   error
```

- **Resincronización:** tras cualquier error se vuelve a `ESPERA_SOF`. Si el
  byte que causó el error es un `0xAA`, se toma como inicio de una trama nueva
  en vez de perderlo.
- **Timeout entre bytes de 20 ms:** a 115 200 baudios un byte tarda 95 µs, así
  que 20 ms sin bytes a media trama significa que la trama no va a terminar.
- **El parser es C++ puro**, sin Arduino, como `kinematics.cpp`. Se prueba en
  la PC con `pio test -e native`: tramas válidas, CRC corrupto, tramas
  cortadas, ruido aleatorio, tramas pegadas, `0xAA` dentro del payload.
- **Latencia:** se mide con `micros()` desde que se procesa el `EOF` hasta que
  el setpoint está en la cola, y se reporta en `DIAG`.

---

## 8. Manejo de errores

| Falla | Cómo se detecta | Reacción | Contador |
|---|---|---|---|
| Byte alterado | Paridad (hardware) y CRC | `NACK CRC`, trama descartada | `err_paridad`, `err_crc` |
| Trama incompleta | Timeout de 20 ms entre bytes | `NACK TIMEOUT`, resincroniza | `err_timeout` |
| Trama mal formada | `LEN` > 32 o falta `EOF` | `NACK TRAMA`, resincroniza | `err_trama` |
| Ráfaga de datos | Buffer circular lleno | Se descarta el buffer, `NACK DESBORDE_RX` | `err_desborde` |
| Orden absurda | Validación de motor, longitud, límites, IK, velocidad | `NACK` con su código; **no se encola** | `err_rango` |
| Cola llena | `xQueueSend` falla | `NACK COLA_LLENA` | — |
| Falla al ejecutar | `TaskMotion` | `FAILED` con código | — |
| **Pérdida de enlace** | Sin trama válida durante 1 s con el robot en movimiento | **`STOP`** automático con rampa | — |

El pendant pide `GET_STATUS` 5 veces por segundo para refrescar la pantalla, y
eso mismo sirve de latido para el watchdog. Si cierras el pendant a media
trayectoria, el robot frena solo.

---

## 9. Cero sin finales de carrera

Sin switches, el cero se enseña:

1. Al encender, los drivers están libres y el brazo cuelga.
2. Se lleva el brazo a mano a la **pieza de referencia** (o se alinean las
   marcas).
3. `ENABLE`: los motores sostienen la pose.
4. `SETZERO`: el firmware asigna a esa pose los pasos conocidos de la
   referencia.

El cero no necesita ser **exacto**, solo **repetible**: los puntos enseñados se
graban con el mismo desvío y vuelven a caer donde deben. Hay que repetirlo en
cada encendido.

Con `HOMING_MODE` = sin finales, el supervisor deja de vigilar los GPIO 16 y
17. El homing con finales de carrera sigue en el código por si se montan.

---

## 10. Modos texto y binario

- El firmware arranca en **modo texto** (la consola de la U1, útil en banco).
- La **primera trama binaria válida** lo pasa a **modo binario**: se apagan el
  eco y todo el texto, y solo se responde con tramas.
- `MODE_TEXT` o un reinicio lo regresan a texto.

---

## 11. Ejemplo: tomar una ficha

Subrutina en la PC. Cada línea es una trama; el robot las ejecuta en orden
desde su cola.

| # | Trama | Qué hace |
|---|---|---|
| 1 | `MOVJ` → `aprox_disp` | Viaje rápido a la aproximación del dispensador |
| 2 | `MOVL` → `disp` | Entra lento y en recta: robot listo para tomar la ficha |
| 3 | `GRIPPER` cerrar, espera 400 ms | Cierra la pinza y espera a que sujete *(U3)* |
| 4 | `MOVL` → `disp + 10 mm en Y` | Sube en vertical para librar el tope del dispensador |
| 5 | `MOVL` → `zona_segura` | Se aleja por un camino conocido |
| 6 | `MOVJ` → `home`, `NOTIFY` | Regresa; al llegar el robot envía `DONE` |

La PC manda las 6 tramas seguidas (caben en la cola de 16) y espera el `DONE`.
