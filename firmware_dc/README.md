# Firmware: brazo planar 2 GDL con motorreductores DC y encoder (ESP32)

Control de Robots: control de posición en lazo cerrado de dos motorreductores
**GM25-370 (12 V, 140 rpm, reducción ~1:34)** con encoder Hall, puente H **L298N**
y **ESP32 DevKit V1**. Es un PID a 200 Hz con perfil trapezoidal, movimiento
coordinado de dos ejes y tareas FreeRTOS ancladas por núcleo.

---

## Índice

1. [Cómo cumple la rúbrica](#1-cómo-cumple-la-rúbrica)
2. [Arquitectura](#2-arquitectura)
3. [Compilar y cargar](#3-compilar-y-cargar)
4. [Cableado](#4-cableado)
5. [Primera puesta en marcha](#5-primera-puesta-en-marcha-en-este-orden)
6. [Calibración del encoder](#6-calibración-del-encoder)
7. [Sintonía del PID sin conocer los motores](#7-sintonía-del-pid-sin-conocer-los-motores)
8. [Comandos](#8-comandos)
9. [Encoder de un solo canal (J2)](#9-encoder-de-un-solo-canal-j2)
10. [Dónde puede fallar en hardware real](#10-dónde-puede-fallar-en-hardware-real)
11. [Simplificaciones hechas y por qué](#11-simplificaciones-hechas-y-por-qué)
12. [Siguiente parcial: protocolo binario](#12-siguiente-parcial-protocolo-binario)
13. [Guion de demostración](#13-guion-de-demostración-para-la-evaluación)

---

## 1. Cómo cumple la rúbrica

| Pts | Criterio | Implementación | Dónde |
|---|---|---|---|
| 10 | Electrónica de potencia | L298N con los 12 V de motores separados de la lógica (ESP32 por USB), tierra común, jumpers ENA/ENB retirados, capacitores de desacoplo | §4 |
| 10 | Dirección y velocidad/posición | IN1/IN2 dan la dirección y el PWM por **LEDC** (20 kHz, 10 bits) da la magnitud; posicionamiento con `JOG`, `MOVJ` y `MOVL` | `motor.cpp` |
| 20 | Realimentación con encoder | Encoders leídos por el periférico **PCNT** (hardware, x4 en J1) y PID de posición a **200 Hz** exactos con `vTaskDelayUntil`; también se estima la velocidad | `encoder.cpp`, `control.cpp` |
| 30 | Varios motores sin bloquear | 4 tareas FreeRTOS ancladas por núcleo, una cola de setpoints y un mutex. PWM y conteo son de hardware. **MOVJ sincronizado**: los dos ejes arrancan y llegan juntos | `main.cpp`, `motion.cpp` |
| 20 | Perfiles de movimiento | Perfil trapezoidal (triangular si no alcanza vmax) con `SPEED` y `ACCEL`, `STOP` con desaceleración y *feed-forward* de velocidad | `motion.cpp` |
| 10 | Diagnóstico | Telemetría a 10 Hz. Corte de salida por **motor bloqueado** y por **error de seguimiento**, límite de PWM, medición de fricción (`FRIC`), caracterización (`TEST`, `SWEEP`) y conteo de retrasos del lazo | `control.cpp`, `comms.cpp` |

---

## 2. Arquitectura

```
                 NÚCLEO 0 (PRO_CPU)                           NÚCLEO 1 (APP_CPU)
 ┌──────────────────────────────────────┐      ┌───────────────────────────────────────────┐
 │ TaskComms      prio 2  por evento    │      │ TaskMotion   prio 5  10 ms                 │
 │  texto -> Setpoint_t -> motion_submit├─────►│  xQueueReceive(setpointQueue)              │
 │                                      │ cola │  perfil trapezoidal / sincronía / IK       │
 │ (siguiente parcial: parser binario)  │      │  escribe cmd.pos_ref + vel_ref ──┐         │
 │                                      │      │                                  │ mutex   │
 │ TaskTelemetry  prio 1  100 ms        │      │ TaskControl  prio 6  5 ms (200 Hz)▼        │
 │  lee estado (mutex) -> Serial        │◄─────┤  PCNT -> grados -> PID -> LEDC PWM         │
 └──────────────────────────────────────┘estado│  detección de bloqueo / seguimiento        │
                                               └───────────────────────────────────────────┘
```

| Tarea | Núcleo | Prioridad | Periodo | Stack |
|---|---|---|---|---|
| TaskControl (PID de ambos ejes) | 1 | 6 | 5 ms (200 Hz) | 4 KB |
| TaskMotion (cola y perfiles) | 1 | 5 | 10 ms | 4 KB |
| TaskComms (consola serial) | 0 | 2 | por evento (sondeo cada 10 ms) | 8 KB |
| TaskTelemetry (reporte) | 0 | 1 | 100 ms | 4 KB |

### Decisiones clave

- **La cola es la única frontera.** TaskMotion solo sabe hacer `xQueueReceive` de
  `Setpoint_t` (`setpoint.h`) y no sabe quién la llenó. STOP entra al frente de la
  cola (`xQueueSendToFront`) para no esperar su turno.
- **Un solo dueño por dato.** TaskMotion escribe la referencia y TaskControl el
  estado medido. Las operaciones que tocan el encoder o el PID (HOME, cambiar
  ganancias, reset de falla, calibración) son peticiones que TaskControl ejecuta
  entre dos muestras. Así nunca hay una carrera entre el cero del encoder y el
  cálculo del PID.
- **TaskControl nunca espera.** Toma el mutex con un timeout de 1 tick; si no lo
  obtiene, usa los últimos valores y conserva su periodo. No imprime nada.
- **Referencia suave.** TaskMotion publica cada 10 ms y el PID corre cada 5 ms.
  Entre dos publicaciones, TaskControl extrapola la referencia con la velocidad,
  así que no avanza en escalones.
- **Sincronía MOVJ.** Se planea un solo perfil sobre `D = max(|Δq1|, |Δq2|)` y
  cada eje sigue `q_i = q0_i + Δq_i · s(t)/D`. Los dos arrancan y terminan en el
  mismo instante por construcción. El eje corto escala su velocidad y su
  aceleración por `|Δq_i|/D`.
- **Salida serial protegida.** Un mutex (`console.cpp`) evita que se mezclen las
  líneas de telemetría y de la consola.

### Archivos

```
firmware_dc/
  platformio.ini
  src/
    main.cpp            arranque: periféricos, cola, mutex, creación de tareas
    config.h            pines, geometría, límites, ganancias por defecto, modo de encoder
    setpoint.h          CONTRATO Setpoint_t + motion_submit() (lo único que ve un productor)
    shared_state.*      estado compartido entre tareas + mutex
    encoder.*           PCNT (ESP32Encoder), modo cuadratura y modo 1 canal
    motor.*             L298N: LEDC 20 kHz / 10 bits + pines de dirección
    pid.*               PID: anti-windup, D sobre medición, zona muerta, pwm_min, feed-forward
    control.*           TaskControl: lazo de 200 Hz, fallas
    motion.*            TaskMotion: perfil trapezoidal, MOVJ/JOG/MOVL/JOGC/HOME/STOP
    kinematics.*        cinemática directa e inversa + validación
    comms.*             TaskComms: parser de comandos, CAL, FRIC, TEST, SWEEP
    telemetry.*         TaskTelemetry
    console.*           impresión serial segura entre tareas
    storage.*           NVS (Preferences): cuentas por vuelta y ganancias
```

---

## 3. Compilar y cargar

**Requisitos:** VS Code con la extensión PlatformIO.

1. Abre la carpeta `firmware_dc/` en VS Code. PlatformIO detecta `platformio.ini`.
2. Conecta el ESP32 por USB y presiona **Upload** (flecha →).
3. Abre el **Serial Monitor** (enchufe), que ya está configurado a 115200 baudios.
4. Escribe `HELP`.

Por línea de comandos:

```bash
cd firmware_dc
pio run -t upload
pio device monitor
```

El proyecto fija `platform = espressif32@6.9.0` (Arduino-ESP32 **2.0.17**) y
`ESP32Encoder@0.11.7`. **No lo actualices a Arduino 3.x antes de la entrega**: la
API de LEDC cambió. El código se compiló sin errores ni advertencias propias
(`-Wall`) contra esas mismas versiones del core y de la librería: usa 24 % de la
flash y 6 % de la RAM.

> Si la placa no entra en modo de carga, mantén presionado **BOOT** mientras dice
> `Connecting....`

---

## 4. Cableado

### 4.1 Conector del GM25-370 (6 hilos)

| Color | Función | Conexión |
|---|---|---|
| Rojo | Motor − | Salida del L298N |
| Negro | GND del encoder | GND del ESP32 |
| Amarillo | Fase A | GPIO del ESP32 |
| Verde | Fase B | GPIO del ESP32 (**roto en el motor de J2**: queda sin conectar) |
| Azul | VCC del encoder | **3V3 del ESP32 (nunca 5 V)** |
| Blanco | Motor + | Salida del L298N |

### 4.2 Tabla completa

| Desde | Hacia | Nota |
|---|---|---|
| **Motor J1 (hombro)**, blanco | L298N OUT1 | |
| Motor J1, rojo | L298N OUT2 | |
| Motor J1, amarillo (A) | ESP32 GPIO 16 | |
| Motor J1, verde (B) | ESP32 GPIO 17 | |
| Motor J1, azul | ESP32 3V3 | |
| Motor J1, negro | ESP32 GND | |
| **Motor J2 (codo)**, blanco | L298N OUT3 | **este es el motor con el cable verde roto** |
| Motor J2, rojo | L298N OUT4 | |
| Motor J2, amarillo (A) | ESP32 GPIO 18 | |
| Motor J2, verde (B) | sin conectar | GPIO 19 queda libre, reservado |
| Motor J2, azul | ESP32 3V3 | |
| Motor J2, negro | ESP32 GND | |
| L298N ENA | ESP32 GPIO 25 | **quitar el jumper de ENA**; usar el pin de señal (el del borde) |
| L298N IN1 | ESP32 GPIO 26 | |
| L298N IN2 | ESP32 GPIO 27 | |
| L298N ENB | ESP32 GPIO 14 | **quitar el jumper de ENB** |
| L298N IN3 | ESP32 GPIO 32 | |
| L298N IN4 | ESP32 GPIO 33 | |
| Fuente +12 V | L298N +12V (VS) | fuente de 12 V y al menos 3 A |
| Fuente GND | L298N GND | |
| L298N GND | ESP32 GND | **tierra común obligatoria** |
| L298N +5V | **nada** | no conectar al ESP32 mientras esté por USB |
| Jumper del regulador 5 V del L298N | **puesto** | con 12 V es seguro; alimenta la lógica del L298N |

**¿Por qué el encoder roto va en J2?** Un error angular en J1 se amplifica por
todo el brazo (hasta 350 mm); en J2 solo por el antebrazo (150 mm). Si
necesitas cambiarlo de eje, edita `ENC_MODE` en `config.h`.

### 4.3 Diagrama

```
   FUENTE 12 V ≥3 A                    L298N                              ESP32 DevKit V1
  ┌──────────────┐           ┌─────────────────────────┐            ┌─────────────────────┐
  │         +12V ├──────────►│ +12V                    │            │                     │
  │          GND ├──────────►│ GND ────────────────────┼───────────►│ GND  (tierra común) │
  └──────────────┘    ┌─────►│ +5V (NO conectar)       │            │                     │
     470–1000 µF ─────┘      │ [jumper 5V puesto]      │            │ USB ◄── laptop      │
     entre +12V y GND        │                         │            │                     │
                             │ ENA ◄───────────────────┼────────────┤ GPIO 25             │
                             │ IN1 ◄───────────────────┼────────────┤ GPIO 26             │
                             │ IN2 ◄───────────────────┼────────────┤ GPIO 27             │
                             │ IN3 ◄───────────────────┼────────────┤ GPIO 32             │
                             │ IN4 ◄───────────────────┼────────────┤ GPIO 33             │
                             │ ENB ◄───────────────────┼────────────┤ GPIO 14             │
                             │                         │            │                     │
   Motor J1  blanco ◄────────┤ OUT1                    │            │                     │
             rojo   ◄────────┤ OUT2                    │            │                     │
   Motor J2  blanco ◄────────┤ OUT3                    │            │                     │
             rojo   ◄────────┤ OUT4                    │            │                     │
                             └─────────────────────────┘            │                     │
   Encoder J1: amarillo ────────────────────────────────────────────► GPIO 16             │
               verde    ────────────────────────────────────────────► GPIO 17             │
               azul     ◄──────────────────────────────────────────── 3V3                 │
               negro    ─────────────────────────────────────────────► GND                │
   Encoder J2: amarillo ────────────────────────────────────────────► GPIO 18             │
               verde    ✗ (roto, sin conectar)                        │ GPIO 19 (libre)     │
               azul     ◄──────────────────────────────────────────── 3V3                 │
               negro    ─────────────────────────────────────────────► GND                │
                                                                      └─────────────────────┘
```

### 4.4 Ruido y protección

- **Capacitor cerámico de 100 nF** soldado entre las terminales de cada motor:
  absorbe el ruido de las escobillas.
- **Electrolítico de 470–1000 µF / 25 V** entre +12 V y GND en la bornera del
  L298N: absorbe los picos de corriente al arrancar o invertir.
- **Separa** los hilos del encoder de los de potencia y trenza el par de cada
  motor.
- El PCNT tiene un filtro de glitches de 12.5 µs (`ENC_FILTER_APB_CYCLES`).
- **Nunca** alimentes el encoder con 5 V: sus salidas A/B llevarían 5 V a
  pines que solo toleran 3.3 V.
- Los GPIO elegidos no están en la flash (6–11), no son solo de entrada (34–39)
  y no son pines de arranque, salvo GPIO 14, que emite PWM un instante al
  arrancar. Como IN3 e IN4 inician en bajo, el motor queda libre y no pasa nada.
- En un módulo **WROVER** (con PSRAM), GPIO 16 y 17 están ocupados; en el DevKit
  V1 (WROOM) están libres.

---

## 5. Primera puesta en marcha (en este orden)

**Convención:** mirando el brazo desde arriba, los ángulos positivos van en
sentido **antihorario**. HOME es el brazo **estirado sobre +X** (q1 = q2 = 0).

1. **Solo USB, sin 12 V.** Carga el firmware, abre el monitor y envía `TELEM ON`.
2. **Sentido del encoder de J1.** Gira J1 a mano en sentido antihorario: `q1`
   debe **subir**. Si baja, pon `ENC_INVERT[0] = true` en `config.h` y vuelve a
   cargar.
   - J2 es de un canal y aún no se mueve. En este paso, con PWM 0, cuenta en el
     último sentido comandado.
3. **Conecta los 12 V** con el brazo sin carga, o sin el antebrazo si puedes.
   Envía `LIMIT 400`.
4. `FRIC J1` y `FRIC J2`: el eje se mueve unos grados y reporta el PWM con el
   que arranca. Aplica la línea sugerida, por ejemplo `DB 1 0.5 140`.
5. **Sentido del motor.** Envía `JOG J1 10`: J1 debe girar 10° en sentido
   antihorario.
   - Si el eje se desboca y aparece `ERROR_SEGUIMIENTO`, el motor está invertido
     respecto al encoder. Intercambia blanco y rojo en OUT1/OUT2, o pon
     `MOTOR_INVERT[0] = true`. Después envía `RESET`.
   - `JOG J2 10` debe girar el antebrazo en sentido antihorario. Si gira al
     revés, intercambia OUT3/OUT4 o pon `MOTOR_INVERT[1] = true`. En J2 el signo
     sale del PWM, así que no se desboca, pero la convención de ángulos quedaría
     al revés.
6. **Calibración** de cada eje (§6).
7. **HOME.** Coloca el brazo estirado sobre +X y envía `HOME`.
8. Envía `LIMIT 920` y prueba `MOVJ 45 -60`, `MOVJ 0 0`, `MOVJ 30 -90`,
   `MOVL 250 100`, `TEST 5 30`.

---

## 6. Calibración del encoder

Mide cuántas cuentas da el encoder por **una vuelta del eje de salida**. Con eso
el firmware convierte cuentas a grados.

1. Envía `CAL J1`. El motor queda **sin par** y el conteo se pone en cero.
2. Marca una referencia en el eje de salida: cinta con una raya, frente a otra
   raya en el cuerpo del motor.
3. Gira a mano **exactamente una vuelta**. La consola imprime las cuentas en
   vivo; en J2, gira **siempre en el mismo sentido**.
4. Envía `CAL END`. Se guarda en NVS (sobrevive a reinicios) y el eje queda en
   cero.
5. Repite con `CAL J2`. `CAL ABORT` cancela sin guardar.

**Valores esperados** para el GM25-370 140 rpm (1:34, encoder de 11 PPR):

- J1 en cuadratura x4: ≈ 11 × 4 × 34 = **1496** cuentas.
- J2 en un canal x2: ≈ **748** cuentas.

Si te sale un valor muy distinto (por ejemplo, la mitad), revisa el encoder. Si
tu encoder es de 12 PPR, esperarías unas 1632 y 816.

**Consejo:** para mayor precisión, gira 5 vueltas y divide entre 5. Para eso
cambia `CAL END` y escribe el valor a mano en `DEFAULT_CPR` de `config.h`.

---

## 7. Sintonía del PID sin conocer los motores

El firmware ya trae ganancias que funcionaron en una simulación del GM25-370
(`kp=60, ki=20, kd=1.0, kff=1.0`). Úsalas como punto de partida y ajústalas en
el hardware real. **Todo se hace en caliente**, sin recompilar, y `SAVE` lo
guarda.

Para ver el efecto de cada ajuste: `TELEM ON`, y en el Serial Plotter de Arduino
IDE 2 (o en el monitor) compara `r1` (referencia) con `q1` (real).

| Paso | Qué hacer | Qué buscar |
|---|---|---|
| 0 | `FRIC <eje>` y aplica el `DB` sugerido | Sin esto el eje se queda corto del objetivo por la fricción |
| 1 | `PID 1 20 0 0` y `FF 1 0`: solo P | `JOG J1 20`. Sube kp (20 → 40 → 60 → 90) hasta que llegue rápido con una oscilación leve al final |
| 2 | Deja kp en **~70 %** de ese valor | Sin oscilación |
| 3 | Agrega kd: `PID 1 <kp> 0 0.5` → 1 → 2 | Amortigua el sobrepaso. Si el motor "zumba" o vibra, kd es demasiado |
| 4 | Agrega kff: `FF 1 0.5` → 1.0 → 1.4 | Durante el movimiento `e1` debe acercarse a 0. Si `q1` se adelanta a `r1`, kff es demasiado |
| 5 | Agrega ki: `PID 1 <kp> 5 <kd>` → 10 → 20 | Elimina el error final que queda. Si hay sobrepaso lento o ciclos, baja ki |
| 6 | `TEST 5 30` y `SWEEP SPEED 20 100 20 2` | Anota el error final y el de seguimiento por nivel |
| 7 | `SAVE` | Guarda las ganancias en NVS |

**Signos de que algo va mal:**

- **Oscilación sostenida alrededor del objetivo:** kp o ki altos, o `pwm_min`
  demasiado grande. Súbele la zona muerta con `DB 1 1.0 ...`.
- **Se queda a 1–2° del objetivo:** falta ki o `pwm_min`.
- **`BLOQUEO` al arrancar:** carga excesiva, 12 V sin conectar o jumper de EN
  puesto.
- **`retrasos de periodo` > 0 en `STATUS`:** algo bloquea el núcleo 1. No
  debería pasar.

---

## 8. Comandos

A 115200 baudios. No distingue mayúsculas. `eje` puede ser `1`, `2`, `J1` o `J2`.

| Comando | Qué hace |
|---|---|
| `CAL <eje>` / `CAL END` / `CAL ABORT` | Calibra las cuentas por vuelta del eje de salida y las guarda en NVS |
| `HOME` | Fija el cero en la posición actual (brazo estirado sobre +X) |
| `JOG <eje> <grados>` | Jog articular **incremental** al estilo FANUC, a 30 % de SPEED |
| `JOGC <X\|Y> <mm>` | Jog cartesiano incremental en línea recta, como WORLD/USER en FANUC o Yaskawa |
| `MOVJ <q1> <q2>` | Movimiento articular absoluto; **los dos ejes llegan juntos** |
| `MOVL <x> <y>` | Movimiento lineal cartesiano absoluto en mm, con validación del trayecto |
| `PID <eje> <kp> <ki> <kd>` | Ganancias en caliente |
| `FF <eje> <kff>` | Feed-forward de velocidad |
| `DB <eje> <grados> <pwm_min>` | Zona muerta y PWM mínimo (fricción) |
| `LIMIT <100..1023>` | Límite de PWM para ambos ejes |
| `SPEED <1-100>` | Velocidad: 100 % = 180 °/s articular, 200 mm/s lineal |
| `ACCEL <°/s²>` | Aceleración del perfil (10..3000; por defecto 360) |
| `STOP` | Desacelera y vacía la cola; también aborta `TEST` y `SWEEP` |
| `STATUS` | Objetivo, real, error, velocidad, PWM y cuentas por eje; posición del efector; ganancias; salud del lazo |
| `TELEM ON\|OFF` | Telemetría cada 100 ms: `r1,q1,e1,pwm1,r2,q2,e2,pwm2,fault` |
| `FRIC <eje>` | Mide el PWM mínimo que mueve el eje (lazo abierto) |
| `TEST <ciclos> <grados>` | Ida y vuelta con MOVJ en ambos ejes; reporta el error final medio, máximo y acumulado, y el de seguimiento |
| `SWEEP <SPEED\|ACCEL> <ini> <fin> <inc> <ciclos>` | Repite TEST (±45°) en cada nivel e imprime una tabla separada por `;` para Excel |
| `RESET` | Borra las fallas; la referencia pasa a ser la posición actual |
| `SAVE` / `FACTORY` | Guarda las ganancias en NVS / borra la NVS |

**Órdenes encoladas:** las órdenes de movimiento se ejecutan en secuencia. Puedes
mandar varias seguidas, como un programa de robot. `STOP` se salta la cola.

**Sobre MOVL:** requiere empezar en la rama "codo arriba" (q2 < 0) y fuera de la
singularidad. Justo después de `HOME` el brazo está totalmente estirado (r = 350
mm, singular), así que primero da un `MOVJ` (por ejemplo `MOVJ 30 -60`) y luego
`MOVL`.

---

## 9. Encoder de un solo canal (J2)

El cable verde (fase B) del motor de J2 no funciona. Sin B, el hardware cuenta
pulsos pero **no sabe la dirección**. El firmware:

- Cuenta **ambos flancos de A** (x2) con el PCNT.
- Asigna a cada incremento el **signo del PWM** que se está aplicando. Si el PWM
  es 0, usa el último sentido.

**Limitaciones reales:**

- **Sobrepaso o rebote:** si el eje se mueve por inercia en contra del PWM, esas
  cuentas se suman con el signo equivocado y el error se acumula. En este brazo
  horizontal con reductor 1:34, el efecto es pequeño.
- **Empujar el brazo a mano** con los motores activos o sin par descuadra J2.
  Después haz `HOME`.
- **Resolución:** 0.48° por cuenta en J2 contra 0.24° en J1.
- **La deriva no la ve el propio encoder.** Para medirla, pon una marca física,
  ejecuta `TEST 20 45` y observa si J2 regresa a la marca. Es un buen dato para
  el reporte.
- Si reparas el cable, conecta el verde a GPIO 19, cambia `ENC_MODE[1]` a
  `ENC_QUADRATURE`, cambia `DEFAULT_CPR[1]` a 1496 y recalibra.

---

## 10. Dónde puede fallar en hardware real

| Riesgo | Síntoma | Mitigación |
|---|---|---|
| **Sin tierra común** | Motores muertos o erráticos | Unir GND del L298N con GND del ESP32 |
| **Jumpers ENA/ENB puestos** | El motor va siempre a tope; el PID no controla | Quitarlos |
| **Encoder a 5 V** | Daño a los GPIO 16–19 | Alimentarlo solo con 3V3 |
| **Motor y encoder con sentidos opuestos** | Se desboca y aparece `ERROR_SEGUIMIENTO` | §5, paso 5; primeras pruebas con `LIMIT 400` |
| **Caída del L298N (~2 V)** | Menos velocidad y par; zona muerta de PWM grande | `FRIC` + `DB`; `SPEED 100` es solo 180 °/s (el motor da ~700) |
| **Fuente débil** | Reinicios del ESP32 o `BLOQUEO` al arrancar los dos motores | Fuente ≥ 3 A, electrolítico en la bornera y `ACCEL` más bajo |
| **Juego del reductor** | Error en la salida que el encoder no ve (está antes del reductor) | Es inherente; aproximarse siempre desde el mismo lado reduce la dispersión |
| **Ruido de escobillas** | Saltos de cuentas; oscilación rara | Capacitores de 100 nF, hilos separados, filtro del PCNT |
| **Calentamiento del L298N** | Se apaga térmicamente | Disipador; `LIMIT`; el corte por bloqueo evita estar parado con PWM alto |
| **Motor bloqueado** | Calentamiento rápido | Corte automático: PWM > 60 % y < 3 °/s durante 0.8 s. Se rearma con `RESET` |
| **Encoder de un canal (J2)** | Deriva por inercia o empujones | §9; `HOME` periódico |
| **HOME en otra pose** | MOVL va a otro lado | HOME siempre con el brazo estirado sobre +X |
| **Módulo WROVER** | Encoder J1 sin cuentas | Mover GPIO 16/17 a otros pines libres |
| **GPIO 14 al arrancar** | Posible tirón mínimo de J2 | Inofensivo (IN3/IN4 en bajo) |

---

## 11. Simplificaciones hechas y por qué

- **Consola por sondeo cada 10 ms** en lugar de una interrupción de UART. Da
  10 ms de latencia, irrelevante para una persona, y cumple el "por evento" en
  la práctica sin agregar complejidad.
- **Un solo mutex** para todo el estado compartido. Las secciones críticas son
  copias de unos cuantos bytes, así que un mutex por variable no daría nada.
- **TEST y SWEEP corren en TaskComms**, que se comporta como un productor más de
  la cola: encola MOVJ y espera. Así no se toca TaskMotion ni el control.
- **Límites articulares fijos** (q1 ±170°, q2 ±150°) en `config.h`.
- **Coast (sin freno) con PWM 0:** el reductor 1:34 sostiene la posición en un
  brazo horizontal, y así se evitan picos de corriente del freno de corto.

Ninguna de estas simplificaciones quita puntos de la rúbrica.

---

## 12. Siguiente parcial: protocolo binario

El control quedó desacoplado del origen de las órdenes. Para agregar un
protocolo binario:

1. Crea `binproto.cpp` con una tarea en el **núcleo 0** que lea tramas (del
   UART, WiFi o BLE), verifique el CRC y arme un `Setpoint_t`.
2. Llama a `motion_submit(sp)`. Es todo.
3. Si necesitas responder con el estado, copia `g_state.st[]` bajo
   `state_lock()`, igual que hace `telemetry.cpp`.

No hay que modificar `control.cpp`, `motion.cpp`, `pid.cpp` ni
`kinematics.cpp`. Si el protocolo binario usa el mismo UART que la consola de
texto, desactiva `TaskComms` en `main.cpp` o úsala como respaldo con un byte de
arranque distinto.

---

## 13. Guion de demostración para la evaluación

1. `STATUS` y `TELEM ON`: mostrar el lazo cerrado vivo.
2. `JOG J1 30` y `JOG J2 -30`: arranque suave (perfil) con dirección y
   velocidad controladas.
3. `SPEED 60`, `MOVJ 60 -90`: **los dos ejes llegan juntos** aunque J1 recorre
   60° y J2 90°. La consola imprime la duración `T` del perfil común.
4. `MOVJ 30 -60` y luego `MOVL 250 50`: línea recta cartesiana.
5. `STOP` a mitad de un `MOVJ -60 60`: frena con desaceleración, no en seco.
6. Detén el brazo con la mano (sin forzar) durante un movimiento: aparece
   `ERROR_SEGUIMIENTO` (o `BLOQUEO` si lo retienes estando quieto) y la salida
   de **ambos** motores se corta. Luego `RESET`.
7. `TEST 5 45`: error final y de seguimiento por ciclo.
8. `SWEEP SPEED 20 100 20 2`: tabla de caracterización, lista para graficar.
