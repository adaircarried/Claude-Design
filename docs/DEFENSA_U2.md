# Guía de defensa — Unidad 2

Para preparar la presentación y responder preguntas del código.

**Cómo usarla mañana:** ábrela en VS Code y pulsa `Ctrl+Shift+V` para verla
formateada. Las referencias tipo `src/frame.cpp:51` se abren con `Ctrl+P`,
escribiendo el nombre del archivo, y después `Ctrl+G` y el número de línea.

---

## 1. El proyecto en 30 segundos

> "Diseñamos un protocolo serial binario entre la PC y el ESP32 que controla
> el brazo. Cada orden viaja en una trama con delimitadores de inicio y fin,
> ID de motor, banderas y un CRC-16. El ESP32 decodifica byte a byte en tiempo
> real con un buffer circular, rechaza cualquier trama corrupta o incompleta
> con un código de error, y nunca se cae. Las tramas válidas se convierten en
> la misma orden interna que usaba la Unidad 1, así que el control de
> movimiento no cambió. En la PC, un teach pendant en Python manda las órdenes
> y permite inyectar fallas para demostrar cada criterio."

---

## 2. El recorrido de una orden

Si entiendes este diagrama, puedes contestar casi cualquier pregunta:

```
  PC (Python)                                   ESP32
  ───────────                                   ─────
  pendant.py   botón "J1 +"
     │
  robot.py     asigna SEQ, espera ACK
     │
  protocolo.py arma la trama + CRC-16
     │
     └──── USB ── 115200 8E1 ───► UART (interrupción del hardware)
                                     │  buffer del driver, 1024 B
                                  TaskComms (núcleo 0, cada 1 ms)
                                     │  buffer circular propio, 512 B
                                  frame.cpp  máquina de estados, byte a byte
                                     │  trama válida
                                  proto.cpp  valida la orden → ACK / NACK
                                     │  Setpoint_t (el MISMO de la U1)
                                  setpointQueue (cola de FreeRTOS, 16)
                                     │
                                  TaskMotion (núcleo 1, cada 5 ms)
                                     │
                                  FastAccelStepper → pulsos por hardware
                                     │
                                  TB6600 → motor
```

**Latencia de punta a punta**, del último byte de la trama al motor
arrancando:

| Tramo | Tiempo |
|---|---|
| Transmitir una trama de 13 bytes a 115200 8E1 | 1.24 ms |
| Espera hasta el siguiente tick de TaskComms | ≤ 1 ms |
| Decodificar, validar y encolar (medido en tu robot) | 0.28 ms |
| Espera hasta el siguiente tick de TaskMotion | ≤ 5 ms |
| **Total** | **< 8 ms** |

El umbral de percepción humana es de unos 100 ms. La "ida y vuelta" que
muestra el pendant (~10 ms) es mayor porque incluye la latencia del USB de
Windows en los dos sentidos.

---

## 3. Criterio por criterio

### 3.1 · Bus and Peripheral Setup — 10 pts

**Qué pide:** que configures explícitamente los parámetros del puerto
(baudios, bits de paro, paridad) y que **los dos extremos coincidan**. Si no
coinciden, los bytes llegan como basura.

**Cómo lo cumplimos:**
- 115200 baudios, 8 bits de datos, **paridad par**, 1 bit de paro: **8E1**.
- Elegimos paridad par a propósito: el hardware del UART comprueba cada byte
  por su cuenta, como una primera capa de integridad antes del CRC.
- **Sincronía verificada, no supuesta:** al conectar, la PC manda `PING`, y el
  `PONG` trae la configuración del firmware: baudios, bits, paridad, paro y
  pasos por vuelta. La PC la compara con la suya.

**Qué decir:** *"La configuración está en un solo lugar en cada extremo, y al
conectar no la suponemos: el PING trae la del ESP32 y la PC la compara. Por
eso el pendant dice 'sincronizado'."*

**Qué mostrar:** al conectar, `Bus 115200 8E1 ✓ sincronizado`.

| Código | Dónde |
|---|---|
| Constantes del bus | `src/config.h:292` |
| `Serial.begin(..., SERIAL_8E1)` | `src/main.cpp:88` |
| Respuesta `PONG` | `src/proto.cpp:218` |
| La PC abre el puerto en 8E1 | `pc/robot.py:69` (y las líneas de arriba) |

---

### 3.2 · Data Packet Framing — 10 pts

**Qué pide:** un formato de paquete claro, con **delimitadores de inicio y
fin**, **ID de motor**, **objetivos** (para stepper: **conteo de pasos y
dirección**) y **banderas de comando**.

**Cómo lo cumplimos:**

```
 SOF | LEN | SEQ | CMD | MOTOR | FLAGS | PAYLOAD | CRC_L | CRC_H | EOF
0xAA                                                             0x55
```

| Campo | Para qué |
|---|---|
| `SOF` / `EOF` | Delimitadores de inicio y fin. `0xAA` no es ASCII: no se confunde con la consola de texto |
| `LEN` | Cuántos bytes de datos vienen |
| `SEQ` | Número de orden; la respuesta lo devuelve para saber a qué contesta |
| `CMD` | Qué orden es (JOG, MOVJ, STOP...) |
| `MOTOR` | **ID de motor**: 1 = hombro, 2 = codo, 0 = ambos |
| `FLAGS` | **Banderas**: bit 0 = dirección, bit 1 = avisar al terminar |
| `PAYLOAD` | **Objetivos en pasos** (enteros de 32 bits, little-endian) |
| `CRC` | Integridad (ver 3.3) |

**El caso literal de la rúbrica es el `JOG`:** lleva el **conteo de pasos** en
el payload y la **dirección en un bit de FLAGS**.

**Qué decir:** *"Los objetivos viajan en pasos, no en grados, porque así se
guarda internamente la posición: 44.44 pasos por grado no es entero, y en
grados se acumularía error de redondeo."*

**Qué mostrar:** cualquier botón de inyección imprime los bytes en el registro.
La anatomía completa de una trama está en la sección 4.

| Código | Dónde |
|---|---|
| Constantes de la trama | `src/config.h:302` |
| Construir una trama | `src/frame.cpp:119` |
| Manejador del `JOG` (pasos + bit DIR) | `src/proto.cpp:246` |
| Especificación completa | `docs/PROTOCOLO_U2.md` |

---

### 3.3 · Data Integrity and Validation — 20 pts

**Qué pide:** un mecanismo para **rechazar tramas corruptas o incompletas**.

**Cómo lo cumplimos: cuatro capas, cada una atrapa algo distinto.**

| Capa | Qué atrapa |
|---|---|
| **Paridad** (hardware del UART) | Un bit cambiado dentro de un byte |
| **CRC-16/CCITT** sobre toda la trama | Cualquier corrupción de 1 o 2 bits, de un número impar de bits, ráfagas de hasta 16 bits, **y bytes intercambiados** |
| **`LEN` ≤ 32 y `EOF` = 0x55** | Tramas mal formadas o desalineadas |
| **Timeout de 20 ms entre bytes** | **Tramas incompletas**: si la PC se calla a media trama, se descarta |

**Por qué CRC y no una suma (checksum):** una suma no detecta dos bytes
intercambiados, porque `A + B = B + A`, ni dos errores que se compensan. Hay
un test exactamente para eso.

**Qué decir:** *"Ninguna trama inválida llega a mover el robot: el parser solo
entrega la trama cuando el CRC y el EOF son correctos. Si no, se responde NACK
con el código del error y se cuenta."*

**Qué mostrar:** **CRC corrupto**, **Trama cortada** y **Sin EOF**. Cada una
devuelve su código y sube su contador.

| Código | Dónde |
|---|---|
| CRC-16, bit a bit | `src/crc16.cpp:3` |
| `LEN` validado en cuanto llega | `src/frame.cpp:69` |
| Comprobación de `EOF` y CRC | `src/frame.cpp:103` y `:109` |
| Timeout entre bytes | `src/frame.cpp:43` |
| Test: bytes intercambiados (un checksum no lo vería) | `test/test_frame/test_main.cpp:115` |

---

### 3.4 · Decoding and Parsing — 30 pts (el que más vale)

**Qué pide:** decodificar el flujo de bytes **en tiempo real**, usando
**buffers circulares o interrupciones**, y que los setpoints se actualicen
**sin latencia perceptible**.

**Cómo lo cumplimos, con las dos cosas que pide:**

1. **Interrupción:** el UART del ESP32 recibe por interrupción de hardware y
   guarda en el buffer del driver (agrandado a 1024 B).
2. **Buffer circular propio** de 512 B. Su tamaño es potencia de dos, así que
   el índice da la vuelta con una máscara (`& 511`), sin divisiones.
3. **Máquina de estados byte a byte:** cada byte se procesa en tiempo
   constante y **nunca se espera** a que llegue la trama completa. TaskComms
   jamás se bloquea.
4. **Tiempo real medido, no supuesto:** el firmware mide con `micros()` cuánto
   tarda en procesar cada trama y lo reporta (en tu robot: **283 µs**).
5. **Nada se rompe con datos raros:** tramas pegadas en un solo envío, un
   `0xAA` dentro de los datos, basura antes de la trama. Hay tests de todo
   eso, incluido uno con **200 000 bytes de ruido aleatorio sin una sola
   trama falsa**.

**Qué decir:** *"El parser es una máquina de estados que avanza un byte a la
vez y nunca espera. Por eso el costo es constante y la latencia es de
microsegundos. Y no depende de Arduino: lo probamos en la PC con 19 tests,
contra los mismos vectores que la implementación de Python."*

**Qué mostrar:** la línea `latencia en el ESP32` del panel de diagnóstico, y
el jog respondiendo al instante. Después **Ráfaga 2 KB**: el robot sigue
respondiendo.

| Código | Dónde |
|---|---|
| Buffer circular | `src/comms.cpp:39` |
| TaskComms: UART → buffer → parser | `src/comms.cpp:347` |
| Máquina de estados | `src/frame.cpp:51` |
| Medición de latencia | `src/proto.cpp:321` |
| Test de ruido | `test/test_frame/test_main.cpp:232` |
| El mismo parser en Python | `pc/protocolo.py:237` |

---

### 3.5 · Error Handling and Exception Control — 20 pts

**Qué pide:** manejar fallas de comunicación (**timeouts**, **desbordamiento
de buffer**, **comandos fuera de rango**) **sin que el firmware se caiga**.

**Cómo lo cumplimos: cada falla tiene detección, reacción y contador.**

| Falla | Detección | Reacción |
|---|---|---|
| **Timeout** (trama a medias) | 20 ms sin bytes | `NACK TIMEOUT`, el parser se resincroniza |
| **Desbordamiento** | El buffer circular se llena | Se descarta, se reinicia el parser, `NACK DESBORDE_RX`. **No** se bloquea el robot |
| **Fuera de rango** | Límites articulares, espacio de trabajo, velocidad 1–100 | `NACK FUERA_DE_RANGO`, **no se encola** |
| Motor, longitud o comando inválidos | Validación por comando | `NACK` con su código |
| Cola llena | `xQueueSend` falla | `NACK COLA_LLENA` |
| Falla al ejecutar | TaskMotion | Trama `FAILED` con el código |
| **PC caída** | **Watchdog:** 1 s sin tramas con el robot en movimiento | **STOP automático** con rampa |

Todos los códigos de error (16) están en un solo `enum`, compartido por el
protocolo y el control de movimiento.

**Qué decir:** *"Ningún error es silencioso: o se responde NACK en el acto, o
se avisa con FAILED si falla al ejecutarse. Y el caso más peligroso, que la PC
se caiga a media trayectoria, lo cubre un watchdog: un robot que sigue
ejecutando órdenes de un programa que ya no existe es lo que hay que evitar."*

**Qué mostrar:** **Fuera de rango**, **Motor inválido**, **Cmd
desconocido**; después **Desborde**; y al final el **watchdog**: un "Ir
(MOVJ)" lento y desmarcar "Latido activo" a media trayectoria. Frena en un
segundo.

| Código | Dónde |
|---|---|
| Todos los códigos de error | `src/types.h:89` |
| Validación común de movimientos | `src/proto.cpp:124` |
| Longitud por comando | `src/proto.cpp:177` y `:213` |
| Desborde | `src/proto.cpp:364` |
| Watchdog del enlace | `src/proto.cpp:382` |
| Errores del parser → NACK | `src/proto.cpp:342` |
| FAILED desde TaskMotion | `src/motion.cpp:108` |

---

### 3.6 · Integration with Term 1 — 10 pts

**Qué pide:** que una trama válida **se convierta en movimiento físico** en
tus motores.

**Cómo lo cumplimos:** cada trama válida se convierte en un `Setpoint_t`, **la
misma estructura de la Unidad 1**, y entra en **la misma cola**. TaskMotion no
sabe ni le importa si la orden vino de la consola de texto o del protocolo
binario. La consola de texto sigue funcionando: el firmware es dual.

**Qué decir:** *"La arquitectura de la Unidad 1 se pensó para esto: el control
solo consume de una cola. Para agregar el protocolo no hubo que tocar el
control de movimiento. Solo se agregó un productor más de órdenes."*

**Qué mostrar:** **Ir (MOVJ)** entre dos puntos guardados. Si preguntan por la
sincronía, los dos ejes arrancan y terminan juntos.

| Código | Dónde |
|---|---|
| Se encola el mismo `Setpoint_t` | `src/proto.cpp:100` |
| TaskMotion lo consume | `src/motion.cpp:466` (`dispatch`) |
| `MOVJ` sincronizado | `src/motion.cpp:178` |

---

## 4. Anatomía de una trama (para explicar en el pizarrón)

**JOG del hombro, +5°** (5 × 44.44 = **222 pasos**):

```
AA  04  0C  10  01  00  DE 00 00 00  A8 ED  55
│   │   │   │   │   │   └─────────┘  └───┘  │
│   │   │   │   │   │    222 pasos    CRC   EOF
│   │   │   │   │   FLAGS: dirección + (bit 0 = 0)
│   │   │   │   MOTOR: 1 = hombro
│   │   │   CMD: 0x10 = JOG
│   │   SEQ: orden número 12
│   LEN: 4 bytes de datos
SOF
```

**El mismo JOG, pero −5°:**

```
AA  04  0D  10  01  01  DE 00 00 00  2A 00  55
                    ▲                ▲
                    │                └── el CRC cambia por completo
                    └── bit DIR = 1: dirección negativa
```

Un solo bit distinto en `FLAGS` cambia el CRC por completo: así detecta el
CRC cualquier alteración.

**Respuesta del robot** (ACK a esa orden):

```
AA  01  0C  90  01  00  10  05 23  55
            │           │
            ACK         contesta al comando 0x10 (JOG), con el mismo SEQ 0C
```

**`222` en little-endian es `DE 00 00 00`:** el byte menos significativo va
primero, que es el orden nativo del ESP32 y de la PC.

---

## 5. Preguntas probables y cómo contestarlas

**¿Por qué binario y no texto?**
Una orden en texto (`MOVJ 20.00 -60.00`) ocupa unos 18 caracteres que hay que
convertir con `atof`; la trama binaria tiene los enteros listos, un tamaño
acotado y un CRC. Es más rápida de decodificar y su integridad se puede
verificar. La consola de texto se conservó para depurar.

**¿Por qué paridad si ya tienes CRC?**
Son capas distintas: la paridad la verifica el hardware byte por byte, gratis;
el CRC protege la trama completa. La paridad sola no ve dos bits cambiados en
el mismo byte; el CRC sí.

**¿Qué pasa si se pierde un byte a media trama?**
La trama falla por CRC o por EOF y se responde NACK. Si la PC deja de mandar,
el timeout de 20 ms resincroniza. Lo peor que puede pasar es que haya que
repetir una orden; **nunca se ejecuta una orden corrupta**.

**¿Y si aparece `0xAA` dentro de los datos?**
No pasa nada: la longitud la manda `LEN`, no la búsqueda del delimitador. Hay
un test para eso. Y si una trama se rompe justo donde empieza otra, el `0xAA`
que la rompe se toma como inicio de la siguiente, en vez de perderlo.

**¿Por qué el ACK no significa que el robot terminó?**
El ACK dice "la orden es válida y entró a la cola", en menos de un
milisegundo. El movimiento tarda segundos. El aviso de que terminó es otra
trama, `DONE`, que se pide con el bit `NOTIFY` de `FLAGS`. Una subrutina de la
Unidad 3 lo usa para saber que el robot ya soltó la ficha.

**¿Por qué la PC no reintenta las órdenes que fallan?**
Si se perdiera el ACK pero la orden sí se hubiera ejecutado, un reintento
movería el robot dos veces. Solo se reintentan las consultas (PING, STATUS),
que no tienen efectos.

**¿Qué hace el SEQ?**
Empareja cada respuesta con su orden. Sin él, si mandas tres órdenes seguidas
y una falla, no sabrías cuál.

**¿Cómo pruebas el desbordamiento si el ESP32 vacía el buffer cada 1 ms?**
*(Conviene decirlo antes de que lo pregunten.)* Con un comando de diagnóstico,
`DEBUG_PAUSE`, que hace que TaskComms deje de leer unos instantes mientras la
PC inunda el puerto. En operación normal el buffer no se desborda; el comando
existe para poder demostrar que, si pasa, el firmware lo detecta y se
recupera. Está bloqueado con el robot en movimiento.

**¿Por qué el contador de paridad está en cero?**
Porque los dos extremos están bien configurados y el cable es corto. Para
provocar errores de paridad habría que cambiar la paridad de un solo extremo.

**¿Qué pasa si desconectas el cable o cierras el programa a media
trayectoria?**
El watchdog: sin tramas válidas durante 1 s con el robot en movimiento, el
firmware frena con rampa y reporta `ENLACE_PERDIDO`. Se demuestra con la
casilla "Latido activo".

**¿Por qué dos núcleos?**
El núcleo 0 es donde vive el stack de WiFi/Bluetooth del ESP32. Todo el
movimiento va en el núcleo 1 y la comunicación en el 0, así que una ráfaga de
datos nunca mete retrasos en el control.

**¿Cómo lo probaste?**
En tres niveles:
1. **Tests unitarios en la PC, sin hardware:** 19 del protocolo en C++, 12 en
   Python con los mismos vectores (el PING es idéntico byte a byte en los dos
   lenguajes) y 18 de cinemática.
2. **De punta a punta:** el firmware real compilado contra un simulador del
   hardware, con el programa de Python real: 35 casos que cubren cada
   criterio.
3. **En el robot real**, con el pendant.

**¿Por qué Python en la PC?**
Porque en la Unidad 3 la visión se hará con OpenCV en Python, y el programa
maestro del juego va a usar exactamente estas mismas funciones del protocolo.

**En la Unidad 1 usaste motores DC. ¿Por qué ahora steppers?**
Los steppers eran el diseño original; los DC fueron una contingencia porque
los NEMA 17 no llegaron a tiempo. La arquitectura es la misma (la misma
estructura de orden, la misma cola, las mismas tareas), y eso permitió
migrar sin rehacer el control.

**¿Qué es el tic-tic que se oye con el robot quieto?**
El driver TB6600 baja la corriente a la mitad cuando no recibe pulsos, y a
media corriente el hombro no sostenía el brazo horizontal. El firmware manda
un micropaso de ida y vuelta cada 50 ms a cada eje detenido: el driver nunca
entra en reposo. Desviación máxima: un micropaso, 0.0225°. Lo diagnosticamos
en banco: el brazo subía pero no se sostenía, y con pulsos sí.

**¿Qué limitaciones tiene tu protocolo?**
Responder esto bien suma puntos:
- El formato es por longitud, sin relleno de bytes: tras perder un byte se
  puede descartar también la trama siguiente. Se recupera solo y nunca se
  ejecuta nada corrupto, pero cuesta una repetición.
- El enlace es punto a punto; no hay direcciones de dispositivo porque solo
  hay un robot.
- Si la placa se reinicia al abrir el puerto, se pierde el cero: no hay
  finales de carrera, así que hay que repetir `SETZERO`.

---

## 6. Números para tener a mano

| Dato | Valor |
|---|---|
| Bus | 115200 baudios, 8E1 |
| Trama | 9 bytes + datos (máximo 41) |
| CRC | CRC-16/CCITT, polinomio 0x1021; `"123456789"` → `0x29B1` |
| Latencia de proceso en el ESP32 | 283 µs (medida en tu robot) |
| Timeout entre bytes | 20 ms |
| Watchdog del enlace | 1 s |
| Buffer circular / del driver | 512 B / 1024 B |
| Cola de órdenes | 16 |
| Resolución | 44.44 pasos/grado, 0.0225°/paso |
| Velocidad máxima | 1600 pasos/s = 36°/s |
| Códigos de error | 16 |
| Tests | 49 unitarios + 35 de punta a punta |
