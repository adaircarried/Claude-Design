# Brazo robótico RR de 2 GDL — Conecta 4

Firmware del controlador para un manipulador planar de dos grados de libertad
que juega Conecta 4 contra un oponente humano. Materia **Control de Robots**.

- **Parcial 1** (entrega actual): control step/dir, tres modos de movimiento,
  homing, consola serial y telemetría.
- **Parcial 2**: protocolo serial binario con CRC. Reemplaza `comms.cpp`.
- **Parcial 3**: visión, pinza y lógica de juego. Reutiliza `kinematics.cpp`.

---

## 1. Puesta en marcha rápida

```bash
pio run                  # compilar
pio run -t upload        # flashear
pio device monitor       # consola a 115200 baudios
pio test -e native       # tests de cinemática en la PC, sin ESP32
```

**Antes del firmware, la prueba de banco** (§6): carga el entorno
`bench_motor`, un programa mínimo que solo comprueba el cableado.

Secuencia de arranque del firmware en la consola (modo sin finales de carrera,
el que se usa ahora):

```
(lleva el brazo a la pose de referencia: brazo horizontal, antebrazo colgando)
ENABLE      energiza los motores (arrancan libres a propósito)
SETZERO     esa pose pasa a ser la referencia
STATUS      comprueba que la posición sea la esperada
MOVJ 30 -60 ya puedes moverte
```

---

## 2. Hardware

| Elemento | Modelo | Notas |
|---|---|---|
| Controlador | ESP32 DevKit V1 (WROOM-32) | doble núcleo a 240 MHz |
| Motores | 2 × NEMA 17 + reductor planetario 5:1 | 1.7 A/fase, 0.42 N·m |
| Drivers | 2 × TB6600 | opto-aislados, 1/16 de micropaso |
| Sensores | 2 × final de carrera mecánico | **normalmente cerrado (NC)** |
| Alimentación | fuente de 24 V | lógica por USB en desarrollo |

**Resolución:** 200 × 5 × 16 = **16 000 micropasos por vuelta de articulación**
= **44.444 micropasos por grado** = 0.0225° por micropaso.

---

## 3. Cableado

### 3.1 Señales del ESP32 a los TB6600

```
          ESP32 DevKit V1                          TB6600 #1 (hombro)
        ┌─────────────────┐                      ┌──────────────────┐
        │                 │                      │                  │
        │  GPIO 25  PUL ──┼──────────────────────┤ PUL+             │
        │  GPIO 26  DIR ──┼──────────────────────┤ DIR+             │
        │  GPIO 27  ENA ──┼───────────┬──────────┤ ENA+             │
        │                 │           │          │                  │
        │           GND ──┼───────┬───┼──────────┤ PUL- DIR- ENA-   │
        │                 │       │   │          └──────────────────┘
        │  GPIO 32  PUL ──┼───────┼───┼───────┐
        │  GPIO 33  DIR ──┼───────┼───┼─────┐ │  TB6600 #2 (codo)
        │                 │       │   │     │ │ ┌──────────────────┐
        │                 │       │   └─────┼─┼─┤ ENA+             │
        │                 │       │         │ └─┤ PUL+             │
        │                 │       │         └───┤ DIR+             │
        │                 │       └─────────────┤ PUL- DIR- ENA-   │
        │                 │                     └──────────────────┘
        │  GPIO 16 ───────┼── final de carrera eje 1 ── GND
        │  GPIO 17 ───────┼── final de carrera eje 2 ── GND
        └─────────────────┘
```

> **Qué GND va con qué.** Las entradas `PUL`, `DIR` y `ENA` del TB6600 son
> **optoacopladas**: cada par `+`/`−` es el LED de un optoacoplador. La
> corriente de la señal sale del GPIO, pasa por el LED y **regresa por el
> terminal `−` al GND del ESP32**. Por eso `PUL−`, `DIR−` y `ENA−` sí tienen
> que ir al GND del ESP32: si falta ese regreso, los pulsos se pierden de forma
> aleatoria.
>
> Lo que **no** hay que hacer es unir el GND del ESP32 con el negativo de la
> fuente de 24 V. El optoacoplador existe precisamente para separar la lógica
> de la potencia; unirlas anula el aislamiento y deja que el ruido de los
> motores llegue al ESP32. *(Una versión anterior de este README decía lo
> contrario: estaba mal.)*
>
> **Sobre `ENA`:** en el TB6600, con el LED de `ENA` encendido el driver se
> **desactiva**. Por eso el firmware lo trata como activo en bajo: GPIO 27 en
> bajo = motores energizados. Si dejas `ENA` sin conectar, el driver queda
> **siempre energizado**; sirve de plan B si sospechas del cableado de `ENA`.

### 3.2 Potencia

```
   Fuente 24 V  V+ ──┬──────────── VCC  TB6600 #1 ─┐ 100 µF entre VCC y GND
                     └──────────── VCC  TB6600 #2 ─┤ de cada driver, pegado
                V− ──┬──────────── GND  TB6600 #1 ─┤ a sus bornes
                     └──────────── GND  TB6600 #2 ─┘

   (el V− de la fuente NO se une al GND del ESP32: ver la nota de §3.1)
```

> **El lado de 220/127 V de la fuente es peligroso.** Si es una fuente
> metálica abierta, los bornes `L`, `N` y tierra están a la tensión de la red.
> Usa un cable con clavija, conecta la tierra física, tapa los bornes y nunca
> los toques con la fuente enchufada.

El **capacitor de 100 µF en la entrada de cada driver** absorbe la energía que
el motor devuelve al frenar. Sin él, la tensión de bus sube en cada
desaceleración y a la larga el driver muere. Respeta la polaridad.

### 3.3 Motores

```
TB6600      NEMA 17 (bobinas)
  A+  ──────  bobina 1, extremo A
  A−  ──────  bobina 1, extremo B
  B+  ──────  bobina 2, extremo A
  B−  ──────  bobina 2, extremo B
```

Identifica los pares midiendo continuidad: los dos cables de una misma bobina
dan unos pocos ohmios entre sí; entre bobinas distintas, circuito abierto.

> **Nunca conectes o desconectes un motor con el driver energizado.** La
> bobina abierta genera un pico inductivo que destruye la etapa de salida del
> TB6600. Apaga la fuente de 24 V, espera a que los LED se apaguen, y entonces
> manipula el conector.

### 3.4 Finales de carrera (NC)

> **Ahora mismo el brazo no lleva finales de carrera** y el firmware está en
> modo `HOMING_TAUGHT_ZERO` (cero enseñado con `SETZERO`), donde los GPIO 16 y
> 17 no se vigilan. Esta sección aplica si se montan más adelante.

```
   GPIO 16 ──┬── contacto NC ── GND
             │
         pull-up interno del ESP32
```

Con contacto **normalmente cerrado**:

| Situación | Contacto | Lectura del GPIO |
|---|---|---|
| Sin accionar | cerrado | `LOW` |
| Accionado | abierto | `HIGH` |
| **Cable roto** | abierto | `HIGH` → se lee como accionado |

Esto es deliberado y se llama cableado a prueba de fallos: una avería en el
cableado detiene el robot en vez de dejar que el eje se estrelle contra el
tope. La constante `LIMIT_ACTIVE_LEVEL` en `config.h` codifica este convenio.

**Verifícalo antes del primer HOME:** energiza solo la lógica (USB, sin los
24 V), escribe `STATUS` y comprueba que ambos finales digan `libre`. Pulsa uno
con la mano y repite `STATUS`: debe decir `PISADO`. Si sale al revés, los
cableaste como NO (normalmente abierto) y hay que cambiar el contacto o
invertir `LIMIT_ACTIVE_LEVEL`.

---

## 4. Configuración de los DIP switches del TB6600

Los seis interruptores se dividen en micropaso (S1–S3) y corriente (S4–S6).

### Micropaso — usar 1/16

Tabla del TB6600 más común (la de DFRobot y la mayoría de sus clones):

| S1 | S2 | S3 | Micropaso | Pulsos por vuelta del motor |
|---|---|---|---|---|
| ON | ON | OFF | 1 (paso completo) | 200 |
| ON | OFF | ON | 1/2 | 400 |
| ON | OFF | OFF | 1/4 | 800 |
| OFF | ON | OFF | 1/8 | 1600 |
| **OFF** | **OFF** | **ON** | **1/16** | **3200** |
| OFF | OFF | OFF | 1/32 | 6400 |

**Usa 1/16 → S1 = OFF, S2 = OFF, S3 = ON**, *siempre que coincida con la
etiqueta de tu driver*. *(Una versión anterior de este README daba
OFF-OFF-OFF, que en esta tabla es 1/32: el robot se habría movido la mitad.)*

**Compruébalo con el programa de banco, no con la tabla.** La tecla `1`
manda 4000 pulsos, que a 1/16 son **1.25 vueltas exactas del eje del motor**.
Si ves 2.5 vueltas estás en 1/8; si ves 0.625, en 1/32. La tecla `5` manda
16 000 pulsos: la **salida del reductor** debe dar una vuelta exacta, lo que
valida a la vez el micropaso y el 5:1.

Si cambias de micropaso, cambia `MICROSTEPS` en `config.h`; si no, todos los
ángulos quedan escalados por el factor equivocado.

Por qué 1/16 y no más: el micropasado fino mejora la suavidad, pero el par
incremental por micropaso cae con el seno del ángulo. Pasado 1/16 estás
añadiendo resolución numérica que el motor no puede mantener contra la carga,
y además ya estás muy por debajo del backlash del reductor (ver §7).

### Corriente — empezar en 1.5 A

| S4 | S5 | S6 | Corriente | Pico |
|---|---|---|---|---|
| ON | ON | ON | 0.5 A | 0.7 A |
| ON | OFF | ON | 1.0 A | 1.2 A |
| **ON** | **ON** | **OFF** | **1.5 A** | **1.7 A** |
| ON | OFF | OFF | 2.0 A | 2.2 A |
| OFF | ON | ON | 2.5 A | 2.7 A |

Los motores son de 1.7 A por fase. **Empieza en 1.5 A**: es suficiente para
mover el brazo y el driver se calienta mucho menos. Sube un escalón solo si el
motor pierde pasos con carga.

> Las tablas serigrafiadas varían entre fabricantes de clones de TB6600.
> **Contrasta siempre con la etiqueta de tu driver**, no con esta tabla.

---

## 5. El asunto de los 3.3 V

Las entradas opto-aisladas del TB6600 están pensadas para señales de 5 V, con
una resistencia serie de unos 330 Ω que fija la corriente del LED del
optoacoplador en ~10 mA. Con los 3.3 V del ESP32 la corriente baja a ~6 mA.

**Suele funcionar, pero es el sospechoso número uno cuando aparecen pasos
perdidos**, sobre todo a frecuencias altas, porque el optoacoplador tarda más
en saturar y el flanco llega degradado.

### Si aparecen pasos perdidos: usa un buffer 74AHCT125

```
   ESP32 GPIO 25 ──┤ 1A      1Y ├──── PUL+ del TB6600
   ESP32 GPIO 26 ──┤ 2A      2Y ├──── DIR+
   ESP32 GPIO 27 ──┤ 3A      3Y ├──── ENA+
                   │  74AHCT125  │
   5V del ESP32 ──┤ VCC     GND ├── GND del ESP32
                   │  (1OE,2OE,3OE a GND) │
```

El 74AHCT125 acepta niveles de entrada TTL (reconoce 3.3 V como alto) y saca
5 V de verdad. Cuesta alrededor de un dólar.

### Por qué NO recomiendo el truco del ánodo común

La alternativa clásica es llevar `PUL+`, `DIR+` y `ENA+` a 5 V y controlar los
terminales negativos desde los GPIO. Funciona eléctricamente, pero **invierte
la lógica**: el optoacoplador conduce cuando el GPIO está en bajo. El TB6600
cuenta el paso en el flanco de subida de la corriente del LED, así que el
flanco activo pasa a ser el de bajada del GPIO, y hay que invertir la señal de
pulso en el firmware. FastAccelStepper no expone una forma directa de invertir
la polaridad del pin de paso en el ESP32, así que acabarías peleándote con la
librería. El buffer conserva la polaridad y evita todo el problema.

---

## 6. Puesta a punto paso a paso

Se avanza **de lo más simple a lo más complejo**, y cada etapa solo empieza
cuando la anterior funciona. Todo se puede hacer con los motores **sueltos
sobre la mesa**, sin el brazo montado.

| Etapa | Programa | Qué valida |
|---|---|---|
| A | `bench_motor` | Cableado, DIP switches, fuente, corriente, sentido de giro |
| B | `esp32dev`, consola de texto | FastAccelStepper, tareas, sincronía de MOVJ, STOP |
| C | `esp32dev` + pendant en la PC | Protocolo binario de la U2 |

**Etapa A — `bench_motor`.** En VS Code, panel de la hormiga → *Project
Tasks* → `bench_motor` → *Upload and Monitor*. Comandos de una tecla:

1. `e` energiza. El eje debe resistirse si intentas girarlo a mano.
2. `1`: el eje del **motor** da **1.25 vueltas**. `2` lo regresa.
3. `5`: la **salida del reductor** da **1 vuelta exacta** (5 del motor).
4. Lo mismo con `3`, `4` y `6` para el motor 2.
5. `v` sube la velocidad hasta 3200 Hz: debe seguir girando sin perder pasos.

Si el motor vibra sin girar, una bobina está mal emparejada. Si no hace nada,
revisa con la tecla `d` y el multímetro que llegan 3.3 V entre `DIR+` y
`DIR−`.

**Etapa B — firmware con la consola** (entorno `esp32dev`):

1. `ENABLE`, luego `JOG 1 90`: el mismo 1.25 vueltas que en la etapa A, pero
   ahora generado por el hardware del ESP32. Si la etapa A funcionó y esta no,
   el problema es de software, no de cableado.
2. Si un eje gira al revés de lo que esperas, cambia `A1_DIR_HIGH_COUNTS_UP`
   (o `A2_…`) en `config.h`. **No recablees el motor.**
3. `SETZERO` y después `MOVJ 45 -45`: los **dos** ejes deben arrancar y
   **terminar a la vez**, aunque recorran distancias distintas. Pon una tira
   de cinta en cada eje para verlo.
4. Un `MOVJ` largo y `STOP` a mitad: frena con rampa y **sigue sujetando**.
5. Toca el disipador de cada driver tras unos minutos: tibio es normal; si
   quema, baja la corriente.

**Etapa C** — cuando esté el protocolo de la U2: ver `docs/PROTOCOLO_U2.md`.

---

## 7. Diagnóstico y precisión

### El comando TEST mide de verdad

Sin encoders, comparar el contador interno consigo mismo siempre da exacto,
aunque el motor haya perdido 400 pasos: el firmware no tiene forma de saberlo.
La única referencia absoluta disponible es el final de carrera.

`TEST <pasos> [ciclos]` hace esto:

1. parte de una posición referenciada,
2. ejecuta N ciclos de ida y vuelta de la amplitud pedida,
3. vuelve a buscar el final de carrera **en modo medición**, sin corregir,
4. reporta la diferencia entre lo que el contador cree y dónde el switch dice
   que el eje está realmente.

```
> TEST 4444 20
OK TEST amplitud=4444 pasos (100.00 deg) ciclos=20
TEST A1 esperado=-4000 medido=-4003 deriva=-3 pasos (-0.068 deg)
TEST A2 esperado=6000 medido=5996 deriva=-4 pasos (-0.090 deg)
TEST FIN (deriva incluye perdida de pasos + repetibilidad del switch + backlash)
```

**Cómo interpretarlo.** La deriva mezcla tres cosas. Para separarlas, corre el
test primero con una aceleración baja (`AXIS_MAX_ACCEL` = 2000): la cifra que
salga ahí es tu piso de ruido, típicamente unos pocos pasos, y es repetibilidad
del switch más backlash. Después repite subiendo `AXIS_MAX_ACCEL` hasta que la
cifra se dispare: ese salto es pérdida real de pasos y marca tu límite. Deja la
aceleración de trabajo un 30 % por debajo de ese punto.

### El backlash domina, no el micropaso

| Fuente | Error angular | Error a R = 340 mm |
|---|---|---|
| Un micropaso | 0.0225° | 0.13 mm |
| Backlash del reductor (15′) | 0.25° | 1.48 mm |

El backlash es **once veces** el micropaso. Añadir resolución de micropasado no
mejora nada mientras el backlash mande.

**Lo que sí ayuda:** aproximar siempre las poses desde la misma dirección. El
juego del reductor solo se manifiesta al invertir el sentido; si todas las
aproximaciones vienen del mismo lado, el error se vuelve un sesgo constante
que el homing absorbe. Para Conecta 4 esto importa: enseña las siete columnas
aproximándolas siempre desde arriba.

### El alcance efectivo no es el geométrico

El anillo geométrico del brazo es [50, 350] mm, pero solo si el codo puede
plegarse del todo. Con `q2` limitado a ±135° el codo no se cierra por
completo, y el radio mínimo real sube a:

```
r_min = √(200² + 150² + 2·200·150·cos 135°) = 141.7 mm
```

O sea que el espacio útil con los límites por defecto es **[142, 340] mm**, no
[60, 340]. El firmware calcula esto al arrancar y lo reporta en el banner y en
`STATUS`, en la línea `ALCANCE EFECTIVO`. **Tenlo en cuenta al decidir dónde
colocar el tablero**, o descubrirás al enseñar las poses que la columna más
cercana no es alcanzable.

### Otros síntomas

| Síntoma | Causa probable | Qué hacer |
|---|---|---|
| Pasos perdidos aleatorios, sin patrón | falta el regreso de la señal | `PUL−`, `DIR−` y `ENA−` bien apretados al GND del ESP32 (§3.1) |
| Pasos perdidos solo a alta velocidad | 3.3 V insuficientes para el opto | buffer 74AHCT125 (§5) |
| Pierde un paso al invertir el sentido | tiempo de establecimiento de DIR | subir `DIR_CHANGE_DELAY_US` |
| Zumbido con el eje parado | corriente demasiado alta | bajar el DIP de corriente |
| Driver quema al tacto | corriente alta o sin ventilación | bajar corriente, añadir disipación |
| El ESP32 se reinicia al mover | caída de tensión o watchdog | fuente aparte para la lógica |
| Movimiento a tirones | se están generando pulsos por software | debe usar FastAccelStepper |
| El brazo cae al parar | se liberó ENA | usar `STOP`, nunca `DISABLE` |

> La temperatura del TB6600 **no es medible por software**: los drivers no
> tienen telemetría. Se vigila con el dorso de la mano o con un termopar.

---

## 8. Comandos de la consola

115200 baudios, 8N1, un comando por línea.

| Comando | Qué hace |
|---|---|
| `HOME` | referencia ambos ejes; el codo primero |
| `JOG <eje> <grados>` | mueve un eje, relativo, lento. eje = 1 o 2 |
| `MOVJ <q1> <q2>` | interpolación articular, ambos ejes llegan juntos |
| `MOVL <x> <y>` | interpolación lineal, el efector recorre una recta |
| `SPEED <1-100>` | velocidad para MOVJ y MOVL |
| `STOP` | paro inmediato con rampa, **mantiene el par** |
| `STATUS` | posición, estado y diagnóstico |
| `TEST <pasos> [ciclos]` | mide la pérdida real de pasos |
| `ELBOW UP\|DOWN` | configuración del codo para la cinemática inversa |
| `TELEM ON\|OFF` | flujo periódico de telemetría cada 100 ms |
| `ENABLE` / `DISABLE` | energiza / libera los drivers |
| `CLEAR` | borra el fallo actual |
| `SAVE` | guarda velocidad y codo en memoria no volátil |
| `HELP` | lista de comandos |

Todas las respuestas llevan prefijo: `OK`, `ERR`, `EVT` (evento asíncrono) o
`TLM` (telemetría). El prefijo permite que el programa del Parcial 2 distinga
respuestas de eventos sin ambigüedad.

> **`DISABLE` deja caer el brazo.** Opera en plano vertical y no tiene freno.
> Por eso `STOP` desacelera pero mantiene el par de retención, y liberar los
> motores exige un comando distinto y explícito.

### Los tres modos de movimiento

**JOG** mueve un solo eje, en relativo y despacio. Es para enseñar posiciones
a mano durante la puesta a punto.

**MOVJ** recibe `q1` y `q2` en grados y los dos ejes **arrancan y terminan al
mismo tiempo**. El eje que recorre menos se mueve más lento. La trayectoria
del efector no es una recta, y eso es correcto: es lo que distingue MOVJ de
MOVL. La sincronización se consigue escalando velocidad *y* aceleración por
el mismo factor, lo que hace coincidir los tiempos exactamente tanto en el
perfil trapezoidal como en el triangular (la demostración está en
`motion.cpp`). Escalar solo la velocidad —el error habitual— sincroniza el
tramo de crucero pero no las rampas.

**MOVL** recibe `X` e `Y` en milímetros y el efector recorre una recta.
Valida la trayectoria completa antes de arrancar, porque el espacio de
trabajo es un anillo: una recta entre dos puntos alcanzables puede meterse
por dentro del alcance efectivo. La nomenclatura sigue la convención de los
robots industriales Fanuc y Motoman, donde JOINT mueve ejes de forma
independiente y LINEAR interpola en cartesiano.

---

## 9. Arquitectura del firmware

```
        NÚCLEO 0 (stack de WiFi)          NÚCLEO 1 (movimiento puro)
        ────────────────────────          ──────────────────────────
        TaskComms     prio 2              TaskSupervisor  prio 6
        TaskTelemetry prio 1              TaskMotion      prio 5
             │                                  │
             │        setpointQueue (8)         │
             └─────────────────────────────────>│
                                                │
                                    FastAccelStepper (RMT/MCPWM)
                                                │
                                        TB6600 ×2 → NEMA 17 ×2
```

| Archivo | Responsabilidad |
|---|---|
| `main.cpp` | arranque, creación de colas y tareas |
| `config.h` | pines, constantes, límites |
| `types.h` | tipos compartidos entre módulos |
| `kinematics.cpp` | cinemática directa e inversa, validación |
| `motion.cpp` | TaskMotion, perfiles, sincronización de ejes |
| `homing.cpp` | rutina de referenciado de dos pasadas |
| `supervisor.cpp` | límites, paro de emergencia, estado global |
| `comms.cpp` | TaskComms — **se reescribe en el Parcial 2** |
| `telemetry.cpp` | reporte de estado, salida serial con mutex |
| `persistence.cpp` | memoria no volátil (NVS) |

### Las tres decisiones que sostienen todo

**1. Los pulsos los genera el hardware.** FastAccelStepper usa los periféricos
RMT/MCPWM del ESP32. Generarlos desde una tarea de FreeRTOS no funciona: el
tick del planificador es de 1 ms, así que lo más fino que puede hacer una
tarea son ~1000 pulsos por segundo con un jitter del orden del propio tick. A
2100 pasos/s eso produce movimiento vibrante y, bajo carga, pérdida de pasos.

**2. Las tareas están ancladas por núcleo.** El núcleo 0 aloja el stack de
WiFi; todo el movimiento vive en el núcleo 1. Sin anclar (`xTaskCreate` en vez
de `xTaskCreatePinnedToCore`), el planificador puede migrar una tarea y el
aislamiento entre red y movimiento deja de existir.

**3. La cola de setpoints desacopla el origen de las órdenes.** `TaskMotion`
no sabe de dónde vienen: solo consume `Setpoint_t`. Quien llena la cola cambia
en cada parcial, `TaskMotion` no cambia nunca.

| Parcial | Productor de la cola |
|---|---|
| 1 | consola serial de texto |
| 2 | parser de protocolo binario con CRC |
| 3 | lógica del juego |

Si en el Parcial 2 o 3 hace falta modificar `motion.cpp` para añadir una
fuente de comandos, es señal de que algo se está haciendo mal: la fuente nueva
debe encolar, no llamar.

### El paro de emergencia no pasa por la cola

`MOTION_STOP` existe en el enum, pero un `STOP` **no se encola**. Si
`TaskMotion` está ejecutando un MOVJ de tres segundos y solo lee la cola entre
movimientos, un STOP encolado no haría nada hasta que ese movimiento
terminara. Un paro que llega tarde no es un paro.

El paro viaja fuera de banda: `sv_request_abort()` levanta una bandera que
`TaskMotion` consulta cada 5 ms dentro de cualquier movimiento en curso —
incluido el homing y cada segmento de un MOVL— y además vacía la cola de
pendientes, para que el robot no vuelva a arrancar solo al terminar de frenar.

---

## 10. Tests

```bash
pio test -e native
```

Corren en la PC, sin ESP32 conectado, porque `kinematics.cpp` no depende de
Arduino. Cubren cinemática directa contra poses calculadas a mano, ida y
vuelta directa→inversa→directa barriendo el espacio de trabajo en ambas
configuraciones de codo, las dos ramas del codo dando el mismo punto,
validación del espacio de trabajo, el caso de la recta con extremos válidos
que falla a mitad de camino, el alcance efectivo, y la conversión grados ↔
micropasos incluida la simetría del redondeo.

La cinemática inversa que se prueba aquí es exactamente la que usará el
Parcial 3 para convertir coordenadas de tablero en ángulos. Un error de signo
en la selección de rama se manifiesta como "el brazo se voltea solo", y
depurar eso con el hardware montado cuesta horas.

---

## 11. Riesgos conocidos

1. **3.3 V a las entradas opto-aisladas.** Ver §5. Es el sospechoso número uno
   de la pérdida de pasos.
2. **Ancho de pulso.** El TB6600 pide ~2.2 µs mínimo de `PUL` y sus
   optoacopladores son lentos. A las velocidades de este proyecto
   (≤ 3000 Hz) hay margen de sobra, pero el límite práctico del TB6600 ronda
   los 20 kHz y no se puede subir indefinidamente.
3. **El brazo cae si se liberan los drivers.** Plano vertical, sin freno.
   `STOP` mantiene el par; solo `DISABLE` lo libera, y avisa.
4. **`engine.init(1)` requiere FastAccelStepper ≥ 0.30.** Está fijado en
   `platformio.ini`. Con una versión anterior no compila.
5. **La posición no sobrevive a un corte de energía.** Se guarda en NVS para
   diagnóstico, pero el firmware nunca la restaura automáticamente: tras un
   corte el brazo ha caído y la posición guardada es mentira. Siempre `HOME`.
6. **MOVL es seguimiento de trayectoria, no interpolación con look-ahead.** A
   80 mm/s y refresco de 20 ms el objetivo salta 1.6 mm. Para un pick and
   place de fichas sobra; para cortar un contorno a alta velocidad, no.
