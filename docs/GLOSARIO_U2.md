# Glosario — Unidad 2

Cada término con: **qué es**, una **analogía** cuando ayuda, y **dónde aparece
en tu proyecto**. Está ordenado por temas, de lo más básico a lo más
específico. Compañero de [`DEFENSA_U2.md`](DEFENSA_U2.md).

---

## 1. Lo básico: bits, bytes y hexadecimal

**Bit**
La unidad mínima de información: un 0 o un 1. En el cable, es un voltaje bajo
o alto durante un instante.

**Byte**
8 bits juntos. Puede representar un número del 0 al 255. Todo lo que viaja
por el puerto serie va en bytes.

**Hexadecimal (hex), y el prefijo `0x`**
Una forma corta de escribir bytes: cada byte se escribe con dos símbolos del
`0` al `9` y de la `A` a la `F`. El `0x` delante solo avisa "esto está en
hexadecimal".

| Hex | Decimal | Binario |
|---|---|---|
| `0x00` | 0 | `00000000` |
| `0x0C` | 12 | `00001100` |
| `0x55` | 85 | `01010101` |
| `0xAA` | 170 | `10101010` |
| `0xFF` | 255 | `11111111` |

*En tu proyecto:* el registro del pendant muestra las tramas en hexadecimal,
por ejemplo `aa 04 0c 10 01 ...`.

**ASCII / texto**
Un código que asigna un número a cada letra: `'A'` = 65, `'0'` = 48, el salto
de línea = 10. Todo lo "imprimible" está entre 32 y 126.
*En tu proyecto:* la consola de la U1 (`MOVJ 20 -60`) es texto ASCII. El
protocolo de la U2 es **binario**: los bytes son números directos, no letras.

**Binario (protocolo binario)**
Mandar los datos como números ya listos, en vez de como texto. El número
`-2667` en texto son 5 caracteres que hay que convertir; en binario son
exactamente 4 bytes que se leen directo.

**Little-endian / big-endian**
El orden en que se escriben los bytes de un número que ocupa varios.
**Little-endian = el byte menos significativo primero.**
*Ejemplo:* 222 pasos, en un entero de 4 bytes, es `0x000000DE`; en
little-endian viaja como `DE 00 00 00`.
*Por qué:* es el orden nativo tanto del ESP32 como de una PC, así que no hay
que voltear nada.

**Tipos `u8`, `u16`, `u32`, `i16`, `i32`**
Cómo se guarda cada número: la `u` es "sin signo" (solo positivos), la `i` es
"con signo" (también negativos), y la cifra son los bits.
- `u8`: 0 a 255 (1 byte)
- `i16`: −32 768 a 32 767 (2 bytes)
- `u32`: 0 a 4 294 967 295 (4 bytes)
- `i32`: ±2 147 483 647 (4 bytes)

*En tu proyecto:* los pasos del `MOVJ` van en `i32`, porque pueden ser
negativos; los del `JOG` en `u32`, porque el signo va aparte, en el bit DIR.

---

## 2. Comunicación serie

**Comunicación serie (serial)**
Mandar los bits **uno detrás de otro** por un mismo cable, en vez de varios a
la vez. Es como dictar un número dígito por dígito por teléfono.

**UART** (*Universal Asynchronous Receiver-Transmitter*)
El circuito, dentro del ESP32, que convierte bytes en una secuencia de bits
en el cable, y al revés. "Asíncrono" porque no hay un cable de reloj: los dos
lados se ponen de acuerdo de antemano en la velocidad.

**TX / RX**
Transmitir / recibir. El TX de un lado va al RX del otro. En tu placa, el chip
USB-serie hace esa conexión con la PC.

**Chip USB-serie** (CP2102 o CH340)
El chip junto al conector USB de la NodeMCU. Traduce el USB de la PC al UART
del ESP32. Por eso en Windows aparece como un puerto **COM**.

**Puerto COM** (COM7)
El nombre que Windows le da al puerto serie. Solo **un programa a la vez**
puede abrirlo: por eso hay que cerrar el monitor de PlatformIO antes de abrir
el pendant.

**Baudios** (*baud rate*)
La velocidad del cable en símbolos por segundo; aquí, **bits por segundo**.
A **115 200 baudios**, un bit dura 8.7 µs.
*Los dos lados tienen que usar la misma velocidad*: si no, uno lee los bits a
destiempo y todo llega como basura.

**Bit de inicio, bits de datos, bit de paridad, bit de paro**
Cada byte viaja "envuelto":

```
 ┌ inicio ┐┌──────── 8 bits de datos ────────┐┌ paridad ┐┌ paro ┐
     0       b0  b1  b2  b3  b4  b5  b6  b7       p          1
```

- **Bit de inicio**: siempre 0. Avisa "ahí viene un byte".
- **Bits de datos**: los 8 bits del byte.
- **Bit de paridad**: opcional, para detectar errores (ver abajo).
- **Bit de paro**: siempre 1. Marca el final del byte.

Con 8E1 son **11 bits por byte**, así que a 115 200 baudios un byte tarda
**95.5 µs**.

**Paridad (par / impar)**
Un bit extra que se elige para que la cantidad total de unos sea **par**
(paridad par, *even*) o **impar** (*odd*). El receptor cuenta los unos: si no
cuadra, sabe que un bit cambió en el camino.
*Ejemplo con paridad par:*
- `0xAA` = `10101010` tiene cuatro unos (ya es par) → bit de paridad **0**
- `0x07` = `00000111` tiene tres unos (impar) → bit de paridad **1**, para
  que el total sea par

*Su límite:* si cambian **dos** bits del mismo byte, la cuenta sigue cuadrando
y no se entera. Por eso además existe el CRC.
*En tu proyecto:* la verifica **el hardware del UART**, gratis, byte por byte.

**8E1 / 8N1**
La notación abreviada de la configuración: **8** bits de datos, paridad
**E** (*even*, par) o **N** (*none*, ninguna), **1** bit de paro.
*Tu bus:* **115200 8E1**.

**Sincronía del bus**
Que los dos extremos tengan **exactamente la misma configuración**. Es
justamente el criterio de 10 pts.
*En tu proyecto:* no se supone, se comprueba: el `PING` trae la configuración
del ESP32 y la PC la compara con la suya.

**DTR / RTS**
Dos líneas de control del puerto serie, además de TX y RX. En las placas
ESP32 están conectadas al botón de reinicio, y por eso abrir el puerto a veces
reinicia la placa.
*En tu proyecto:* el pendant las deja en bajo al abrir, para no reiniciar el
ESP32 y no perder el cero.

---

## 3. La trama y sus campos

**Protocolo**
El conjunto de reglas que los dos lados acuerdan: cómo se arma un mensaje,
qué significa cada parte, qué se contesta y qué se hace ante un error. Como
el formato de una carta: remitente, destinatario, asunto y firma, siempre en
el mismo lugar.
*En tu proyecto:* especificado completo en `docs/PROTOCOLO_U2.md`.

**Trama** (*frame*), también **paquete**
Un mensaje completo, con principio y fin bien marcados.

```
 SOF | LEN | SEQ | CMD | MOTOR | FLAGS | PAYLOAD | CRC_L | CRC_H | EOF
```

**Delimitador**
Un byte fijo que marca dónde empieza o termina la trama.

**SOF** (*Start Of Frame*, inicio de trama) — `0xAA`
El primer byte de toda trama. El receptor ignora todo hasta encontrarlo.
*Por qué `0xAA`:* no es un carácter de texto, así que no se confunde con la
consola de la U1, y su patrón `10101010` es fácil de reconocer en un
osciloscopio.

**EOF** (*End Of Frame*, fin de trama) — `0x55`
El último byte. Si en su lugar llega otra cosa, la trama está desalineada o
mal formada y se descarta. `0x55` = `01010101` es el patrón inverso de `0xAA`.

**Cabecera** (*header*)
Los campos fijos del principio: `LEN`, `SEQ`, `CMD`, `MOTOR`, `FLAGS`.

**LEN** (*length*, longitud)
Cuántos bytes de datos (payload) trae la trama, de 0 a 32. El receptor sabe
así cuántos bytes esperar.
*Por qué importa:* si llega un `0xAA` **dentro** de los datos, no se confunde
con un inicio de trama, porque la longitud la manda `LEN`.

**SEQ** (*sequence number*, número de secuencia)
Un número de 1 a 255 que la PC le pone a cada orden, y que **la respuesta
devuelve**.
*Analogía:* el número de turno en la tortillería. Cuando dicen "¡el 12!",
sabes que es el tuyo, aunque haya varios pedidos en curso.
*Para qué:* si mandas tres órdenes seguidas y una falla, el `SEQ` de la
respuesta te dice cuál fue.

**CMD** (*command*, código de comando)
Qué orden es. Por ejemplo: `0x01` = PING, `0x10` = JOG, `0x11` = MOVJ,
`0x20` = STOP. Las respuestas también tienen su código: `0x90` = ACK,
`0x91` = NACK.

**MOTOR** (ID de motor)
A qué motor va la orden: **1 = hombro**, **2 = codo**, **0 = ambos o no
aplica**. Lo pide explícitamente la rúbrica.

**FLAGS** (banderas)
Un byte donde **cada bit es un interruptor** de sí/no:
- **bit 0, `DIR`**: dirección del JOG (0 = positiva, 1 = negativa)
- **bit 1, `NOTIFY`**: "avísame con un DONE cuando termines"

*Analogía:* las casillas de un formulario: cada una se marca o no,
independientemente de las demás.

**Payload** (carga útil)
Los datos de la orden. En un MOVJ: los pasos de cada motor y la velocidad. En
un PING: nada.
*Analogía:* el contenido de la carta, frente al sobre (cabecera) y el sello
(CRC).

**Objetivo** (*target*)
Hacia dónde tiene que ir el motor.
*En tu proyecto:* se manda **en pasos**, no en grados, porque así se guarda
la posición internamente: 44.44 pasos por grado no es entero, y en grados se
acumularía error de redondeo.

---

## 4. Integridad: detectar datos dañados

**Integridad**
Garantizar que lo que llegó es **exactamente** lo que se envió.

**Trama corrupta**
Llegó completa, pero algún bit cambió en el camino: ruido eléctrico de los
motores, un mal contacto.

**Trama incompleta**
Empezó a llegar pero se cortó: la PC se colgó, se desconectó el cable.

**Checksum** (suma de verificación)
Sumar todos los bytes y mandar el resultado. El receptor suma y compara.
*Su debilidad:* no detecta **bytes intercambiados** (A + B = B + A), ni dos
errores que se compensan. Por eso usamos CRC.

**CRC** (*Cyclic Redundancy Check*, verificación por redundancia cíclica)
Un número de control que se calcula con **todos los bytes** de la trama y se
manda al final. El receptor hace el mismo cálculo con lo que recibió: si su
resultado no coincide, la trama está dañada.
*Cómo funciona, en simple:* trata los bytes como un número binario enorme y lo
divide entre un número fijo, el **polinomio**, con una aritmética especial
(XOR en vez de restas). El **residuo** de esa división es el CRC. Cambiar un
solo bit cambia el residuo por completo.
*Analogía:* el **dígito verificador** de una cuenta CLABE o de una tarjeta.
Si tecleas mal un dígito, el banco lo detecta sin consultar nada, porque el
dígito verificador ya no cuadra.
*Qué detecta el CRC-16:* todos los errores de 1 o 2 bits, todos los de un
número impar de bits, y cualquier ráfaga de hasta 16 bits seguidos, que es
justo el patrón típico del ruido.

**CRC-16/CCITT-FALSE**
La variante exacta que usamos: 16 bits, polinomio `0x1021`, valor inicial
`0xFFFF`. Es un estándar; hay varias variantes y los dos lados tienen que usar
la misma.

**Polinomio**
El "divisor" fijo del CRC. `0x1021` representa x¹⁶ + x¹² + x⁵ + 1. No hace
falta saber más que eso: es el estándar CCITT, el que usan muchos protocolos
industriales.

**Vector de prueba**
Una entrada de ejemplo con su resultado conocido, para comprobar que una
implementación está bien hecha. El del CRC-16/CCITT-FALSE es:
**`"123456789"` → `0x29B1`**.
*En tu proyecto:* el C++ y el Python se prueban contra el mismo vector, y el
`PING` que arma cada uno es idéntico byte a byte. Por eso se entienden.

**Resincronización**
Volver a encontrar el inicio de una trama después de un error. El parser
regresa a buscar un `0xAA` y descarta lo demás.

---

## 5. Respuestas y flujo de la conversación

**PING / PONG**
Una orden de "¿estás ahí?" y su respuesta "sí, aquí estoy". Viene del juego
de ping-pong.
*En tu proyecto:* el PONG además trae la configuración del bus del ESP32,
para verificar la sincronía.

**ACK** (*acknowledge*, acuse de recibo)
"Recibido y aceptado." El robot lo manda cuando una orden es válida y **entró
a la cola**.
**No significa "terminado"**: un movimiento tarda segundos, y el ACK sale en
menos de un milisegundo.

**NACK** (*negative acknowledge*, acuse negativo)
"Recibido pero rechazado", con un **código de error** que dice por qué: CRC
inválido, fuera de rango, motor inválido...
*Analogía:* el cajero que te devuelve el billete diciéndote "este está roto".

**DONE**
"Terminé ese movimiento." Llega cuando el robot llega a su destino, si la
orden se mandó con el bit `NOTIFY`.
*En la U3:* así sabrá el programa maestro que el robot ya soltó la ficha.

**FAILED**
"Aceptado, pero no pude terminarlo", por ejemplo porque un STOP lo
interrumpió. Lleva un código de error. Junto con NACK, garantiza que
**ningún error sea silencioso**.

**STATUS**
Respuesta con el estado completo del robot: posición, estado, fallos y
órdenes en cola. El pendant la pide 5 veces por segundo.

**DIAG** (diagnóstico)
Respuesta con los contadores del enlace: tramas buenas, errores de CRC, de
trama, de timeout, desbordes, órdenes rechazadas, errores de paridad y
latencia.

**Código de error**
Un número que identifica el tipo de falla (`0x01` = CRC, `0x07` =
FUERA_DE_RANGO...). Son 16, y están definidos en un solo lugar, `src/types.h`.

**Timeout** (tiempo de espera agotado)
Esperar algo con un límite de tiempo. Se usa en dos lugares:
- **En el ESP32:** si pasan **20 ms** sin bytes a media trama, se descarta por
  incompleta.
- **En la PC:** si el robot no contesta en **500 ms**, la orden se da por
  perdida.

**Reintento** (*retry*) **e idempotente**
Volver a mandar algo que falló. Solo es seguro con operaciones
**idempotentes**: las que, hechas dos veces, dan lo mismo que una. Pedir el
estado es idempotente; un JOG de +5° no (dos veces serían +10°).
*En tu proyecto:* la PC **solo reintenta consultas**, nunca movimientos.

**Latencia**
El tiempo que tarda algo en ocurrir desde que se pide.
*En tu proyecto:* el ESP32 procesa una trama en **~283 µs**, y del último byte
a que el motor arranca pasan **menos de 8 ms**.

**RTT** (*round-trip time*, tiempo de ida y vuelta)
Lo que tarda una orden en ir y su respuesta en volver. El pendant lo muestra
arriba a la derecha (~10 ms). Es mayor que la latencia porque incluye el USB
de Windows en los dos sentidos.

**µs (microsegundo) / ms (milisegundo)**
1 ms = 1/1000 de segundo. 1 µs = 1/1 000 000 de segundo, es decir, 1000 µs =
1 ms.

**Watchdog** (perro guardián)
Un vigilante que espera señales periódicas de que "todo sigue bien", y actúa
si dejan de llegar.
*Analogía:* el **pedal de hombre muerto** de un tren: si el maquinista se
desmaya y lo suelta, el tren frena solo.
*En tu proyecto:* si pasa **1 segundo sin tramas** con el robot en movimiento,
el ESP32 asume que la PC se cayó y **frena con rampa**.

**Latido** (*heartbeat*)
Las señales periódicas que mantienen contento al watchdog.
*En tu proyecto:* las consultas de STATUS del pendant, 5 por segundo. La
casilla **"Latido activo"** sirve para cortarlas y demostrar el watchdog.

---

## 6. Decodificación en tiempo real

**Parser / parsear / decodificar**
El código que lee los bytes que llegan y reconoce en ellos las tramas: dónde
empieza cada una, qué dice cada campo y si es válida.
*En tu proyecto:* `src/frame.cpp` en el ESP32 y la clase `Parser` en
`pc/protocolo.py`.

**Máquina de estados**
Una forma de programar donde el código siempre está en un **estado**
("esperando inicio", "leyendo longitud", "leyendo datos"...) y cada byte que
llega lo hace avanzar al siguiente.

```
 ESPERA_SOF → LEN → SEQ → CMD → MOTOR → FLAGS → PAYLOAD → CRC → EOF → (trama lista)
```

*Por qué así:* procesa **un byte a la vez y nunca se queda esperando** a que
llegue la trama completa. Cada byte cuesta lo mismo, y eso es lo que permite
decir "tiempo real".

**Tiempo real**
Que el sistema responda dentro de un tiempo **garantizado y acotado**, no
solo "rápido en promedio".
*En tu proyecto:* la máquina de estados tiene costo constante por byte, las
tareas tienen periodos fijos (1 ms y 5 ms) y la latencia se mide.

**Buffer**
Un espacio de memoria temporal donde se guardan los datos que llegaron hasta
que alguien los procese.
*Analogía:* la bandeja de entrada de un escritorio.

**Buffer circular** (*ring buffer*)
Un buffer de tamaño fijo cuyo final se "conecta" con su principio. Usa dos
índices: uno para **escribir** (`head`) y otro para **leer** (`tail`). Cuando
llegan al final, dan la vuelta al principio.
*Analogía:* la banda giratoria de un restaurante de sushi. Uno pone platos y
otro los toma, sin mover nunca los platos de lugar.
*Ventaja:* no hay que mover datos ni pedir memoria nueva: solo avanzan dos
números.
*En tu proyecto:* 512 bytes, en `src/comms.cpp`. Como 512 es potencia de dos,
la vuelta se hace con una máscara (`& 511`), sin divisiones.

**Desbordamiento** (*overflow*)
Cuando llegan más datos de los que caben en el buffer. Los que no caben se
pierden.
*En tu proyecto:* se detecta, se descarta todo lo del buffer (ya no es
fiable), se reinicia el parser y se responde `NACK DESBORDE_RX`, sin caerse.

**Interrupción / ISR** (*Interrupt Service Routine*)
Un aviso de hardware que **interrumpe** lo que la CPU estaba haciendo para
atender algo urgente. La ISR es la pequeña función que lo atiende.
*Analogía:* el timbre de la casa: no tienes que asomarte a la puerta a cada
rato, el timbre te avisa.
*En tu proyecto:* el UART lanza una interrupción cuando llegan bytes, y el
driver los guarda en su buffer (1024 B). Después, TaskComms los pasa al
buffer circular.

**Polling** (sondeo)
Lo contrario de una interrupción: **preguntar periódicamente** si hay algo
nuevo. TaskComms revisa el puerto cada 1 ms. Combinado con la interrupción,
ningún byte se pierde y la espera máxima es de 1 ms.

**Tick**
Cada "vuelta" periódica de una tarea. El tick de TaskComms es de 1 ms y el de
TaskMotion de 5 ms.

---

## 7. Firmware y arquitectura

**Firmware**
El programa que corre dentro de un microcontrolador, como el ESP32.

**FreeRTOS**
Un **sistema operativo de tiempo real** para microcontroladores. Permite
tener varias **tareas** corriendo "a la vez", cada una con su prioridad.

**Tarea** (*task*)
Un programa independiente con su propio bucle infinito. Tu firmware tiene
cuatro:
| Tarea | Núcleo | Qué hace |
|---|---|---|
| TaskSupervisor | 1 | Vigila límites y fallos |
| TaskMotion | 1 | Mueve los motores |
| TaskComms | 0 | Recibe órdenes (texto y binario) |
| TaskTelemetry | 0 | Reporta el estado |

**Núcleo** (*core*)
El ESP32 tiene **dos CPUs**. El núcleo 0 aloja el WiFi y el Bluetooth;
el movimiento va en el 1, para que la comunicación nunca lo retrase.

**Prioridad**
Qué tarea gana cuando dos quieren la CPU al mismo tiempo. El supervisor tiene
la más alta.

**Cola** (*queue*)
Una fila donde una tarea deja mensajes y otra los recoge **en orden**.
*Analogía:* la fila de la tortillería: el primero en llegar es el primero en
salir (FIFO, *First In, First Out*).
*En tu proyecto:* `setpointQueue`, de 16 órdenes. Es la frontera entre
"quién da órdenes" y "quién mueve".

**Productor / consumidor**
El que pone mensajes en la cola (productor: la consola o el protocolo) y el
que los saca (consumidor: TaskMotion). Ninguno necesita conocer al otro.

**Setpoint** (`Setpoint_t`)
Una **orden interna de movimiento**: tipo (JOG, MOVJ...), objetivo,
velocidad. Es la **misma** estructura en la U1 y en la U2: por eso el
protocolo se integró sin tocar el control.

**Mutex** (*mutual exclusion*, exclusión mutua)
Un "candado" que garantiza que **solo una tarea a la vez** use algo
compartido.
*Analogía:* la llave del baño de una oficina.
*En tu proyecto:* el puerto serie tiene un mutex, para que dos tareas no
escriban a la vez y se mezclen sus bytes.

**Fuera de banda** (*out of band*)
Un mensaje que **no hace fila**, que va por un camino aparte.
*En tu proyecto:* el **STOP** no entra a la cola: si tuviera que esperar
detrás de un MOVJ de 3 segundos, llegaría tarde.

**Modo dual (texto / binario)**
El firmware entiende las dos cosas: arranca con la consola de texto de la U1,
y en cuanto recibe la primera trama binaria válida, apaga el texto para que
por el puerto solo viajen tramas.

---

## 8. Robot y movimiento

**Stepper (motor a pasos)**
Un motor que gira en **pasos fijos**: 200 por vuelta (1.8° cada uno). Cada
pulso eléctrico = un paso. No necesita sensor para saber cuánto giró, siempre
que no se salte pasos.

**Micropaso** (*microstep*)
El driver divide cada paso en partes más pequeñas. A **1/16**, cada paso son
16 micropasos: 3200 por vuelta del motor.

**Reductor** (5:1)
Engranes que hacen que la salida gire 5 veces más lento, pero con 5 veces más
fuerza. Resultado: **16 000 micropasos por vuelta de la articulación = 44.44
por grado**.

**Driver (TB6600)**
El circuito que recibe pulsos del ESP32 y entrega a las bobinas del motor la
corriente fuerte que necesita. Tiene tres entradas:
- **PUL** (pulso): cada pulso = un micropaso
- **DIR** (dirección): alto o bajo = sentido de giro
- **ENA** (habilitar): energiza o libera el motor

**Optoacoplador**
Un LED y un fototransistor dentro de un mismo chip: la señal pasa como **luz**,
sin conexión eléctrica.
*Para qué:* separar la lógica del ESP32 (3.3 V) de la potencia de los
motores (24 V). Por eso el GND de la fuente **no** se une al del ESP32.

**Par de retención y reducción de corriente en reposo**
**Par** = fuerza de giro. El TB6600 de este robot **baja la corriente a la
mitad cuando no recibe pulsos**, y a media fuerza el hombro no sostenía el
brazo horizontal.

**Sostenimiento con pulsos** (*dither*)
La solución: cada 50 ms, un micropaso de ida y vuelta a cada eje quieto. El
driver nunca "ve" el reposo. Es el **tic-tic** que se oye. Desviación
máxima: 0.0225°.

**Rampa / aceleración**
Subir y bajar la velocidad poco a poco, en vez de arrancar de golpe. Evita que
el motor se salte pasos.

**JOG**
Mover **un eje** un incremento pequeño, despacio. Para acercarse con cuidado
a una posición.

**MOVJ** (movimiento articular)
Mover las dos articulaciones a ángulos objetivo, **arrancando y terminando a
la vez**. El efector no va en línea recta.

**MOVL** (movimiento lineal)
Mover el efector **en línea recta** hasta un punto (X, Y). Las dos
articulaciones se coordinan continuamente.

**Cinemática directa / inversa**
- **Directa:** de los ángulos (q1, q2) a la posición del efector (X, Y).
- **Inversa:** de la posición deseada (X, Y) a los ángulos necesarios.

**Teach pendant**
El control con el que se mueve un robot industrial a mano para **enseñarle**
posiciones. El tuyo es `pc/pendant.py`.

**Punto enseñado**
Una posición que llevaste a mano con el jog y guardaste con un nombre. Se
guarda en `pc/puntos.json`.

**SETZERO (cero enseñado)**
Decirle al robot "la pose en la que estás ahora es la referencia". Reemplaza
al homing, porque el robot no tiene finales de carrera.

**Homing** (referenciado)
Buscar automáticamente una posición de referencia con sensores. Está en el
código, pero no se usa porque no hay finales de carrera.

---

## 9. Pruebas

**Test unitario**
Un pequeño programa que prueba **una sola pieza** del código, de forma
automática: "si le doy esta trama, ¿la acepta?".

**Test nativo**
Un test que corre **en la PC**, sin ESP32. Es posible porque el parser, el CRC
y la cinemática no dependen de Arduino.
*Comando:* `pio test -e native`.

**Prueba de punta a punta** (*end-to-end*)
Probar el sistema **completo**, de un extremo al otro: el programa de Python
real hablando con el firmware real.

**Simulador**
Un programa que imita el hardware en la PC (las colas, las tareas, el puerto
y unos motores virtuales), para correr el firmware real sin el ESP32.
*En tu proyecto:* así se verificaron los 35 casos de punta a punta antes de
tener el robot.

**Inyección de fallas**
Provocar errores **a propósito** (tramas dañadas, cortadas, valores
absurdos...) para comprobar que el sistema los maneja bien.
*En tu proyecto:* el panel "Inyección de fallas" del pendant.

**Fuzzing / ruido aleatorio**
Bombardear el parser con bytes al azar para comprobar que nunca se cuelga ni
acepta basura. Hay un test con **200 000 bytes aleatorios** y ninguna trama
falsa.
