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

Secuencia de arranque en la consola:

```
ENABLE      energiza los motores (arrancan libres a propósito)
HOME        referencia ambos ejes; el codo va primero
STATUS      comprueba que la posición sea la esperada
MOVJ -45 90 ya puedes moverte
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

> **La tierra común no es opcional.** Si el GND del ESP32 y el de la etapa de
> potencia no están unidos, los pulsos se pierden de forma aleatoria e
> intermitente: es el fallo más difícil de diagnosticar de todo el montaje,
> porque el sistema funciona "casi siempre".

### 3.2 Potencia

```
   Fuente 24 V ──┬── (+) ──┬── VCC TB6600 #1 ──┐
                 │         │                   ├── 100 µF cada uno
                 │         └── VCC TB6600 #2 ──┘
                 └── (−) ─────  GND común ───── GND del ESP32
```

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

| S1 | S2 | S3 | Micropaso | Pulsos/vuelta del motor |
|---|---|---|---|---|
| ON | ON | ON | full step | 200 |
| ON | ON | OFF | 1/2 | 400 |
| OFF | ON | OFF | 1/4 | 800 |
| ON | OFF | OFF | 1/8 | 1600 |
| **OFF** | **OFF** | **OFF** | **1/16** | **3200** |
| OFF | ON | ON | 1/32 | 6400 |

**Usa 1/16 → S1=OFF, S2=OFF, S3=OFF.** Si cambias de micropaso, tienes que
cambiar `MICROSTEPS` en `config.h`, o todos los ángulos quedarán escalados por
el factor equivocado y el robot se moverá lo que no es.

Por qué 1/16 y no más: el micropasado fino mejora la suavidad, pero el par
incremental por micropaso cae con el seno del ángulo. Pasado 1/16 estás
añadiendo resolución numérica que el motor no puede mantener contra la carga,
y además ya estás muy por debajo del backlash del reductor (ver §7).

### Corriente — empezar en 1.5 A

| S4 | S5 | S6 | Corriente pico | Corriente RMS |
|---|---|---|---|---|
| ON | ON | ON | 0.5 A | 0.7 A |
| ON | OFF | ON | 1.0 A | 1.2 A |
| **ON** | **ON** | **OFF** | **1.5 A** | **1.7 A** |
| ON | OFF | OFF | 2.0 A | 2.2 A |
| OFF | ON | ON | 2.5 A | 2.7 A |

Los motores son de 1.7 A por fase. **Empieza en 1.5 A**, no en el máximo: es
suficiente para mover el brazo y el driver se calienta mucho menos. Sube solo
si el comando `TEST` revela pérdida de pasos bajo carga.

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
              5V ──┤ VCC     GND ├── GND común
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

1. **Solo lógica.** Conecta el ESP32 por USB, con los 24 V apagados. Flashea.
   Abre el monitor. Debe aparecer el banner con la resolución y el alcance.
2. **Comprueba los finales de carrera.** `STATUS`, pulsa cada switch a mano,
   `STATUS` otra vez. Ver §3.4.
3. **Comprueba el sentido de giro.** Enciende los 24 V. `ENABLE`, después
   `JOG 1 5`. Si el eje gira al revés de lo esperado, cambia
   `A1_DIR_HIGH_COUNTS_UP` en `config.h` y vuelve a flashear.
   **No recablees el motor**: hay que apagar la fuente para hacerlo, y es un
   cambio de una línea en el código.
4. **Comprueba que el homing va hacia el switch.** `JOG 1 -5` debe acercar el
   eje 1 a su final de carrera. Si se aleja, invierte `A1_HOMING_DIR`.
5. **Primer HOME.** Ten la mano sobre el interruptor de la fuente. Si algo va
   mal, `STOP` detiene en milisegundos.
6. **Ajusta los límites articulares.** Con `JOG`, recorre a mano hasta los
   topes mecánicos reales y anota los ángulos con `STATUS`. Escribe esos
   valores (con un par de grados de margen) en `A1_MIN_DEG`, `A1_MAX_DEG`,
   `A2_MIN_DEG`, `A2_MAX_DEG`.
7. **Mide la pérdida de pasos.** `TEST 4444 20`. Ver §7.
8. **Ajusta la corriente.** Sube el DIP de corriente solo si `TEST` acusa
   deriva. Toca el disipador del driver tras unos minutos de trabajo: si
   quema, baja la corriente o añade ventilación.

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
| Pasos perdidos aleatorios, sin patrón | falta tierra común | unir GND del ESP32 y de potencia |
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
