# Programa de la PC — Unidad 2

Teach pendant y enlace con el brazo por el protocolo binario de la U2
(especificación en [`../docs/PROTOCOLO_U2.md`](../docs/PROTOCOLO_U2.md)).

| Archivo | Qué es |
|---|---|
| `pendant.py` | **El programa:** teach pendant con jog, puntos enseñados, diagnóstico e inyección de fallas |
| `robot.py` | Enlace: órdenes, respuestas, avisos `DONE`/`FAILED`. Lo reutilizará la U3 |
| `protocolo.py` | Formato de trama y CRC. Puro, sin puerto |
| `test_protocolo.py` | Tests sin hardware, con los mismos vectores que el firmware |
| `puntos.json` | Los puntos que enseñes (se crea al guardar el primero) |

## Instalación en Windows (una sola vez)

1. Instala **Python 3.10 o más nuevo** desde python.org. En el instalador,
   marca **"Add python.exe to PATH"**. Tkinter, la librería de la ventana, ya
   viene incluida.
2. En una terminal, dentro de esta carpeta `pc`:
   ```
   pip install -r requirements.txt
   ```

## Uso

**Antes:** el ESP32 debe tener cargado el firmware `esp32dev` (no
`bench_motor`), y **el monitor serial de PlatformIO debe estar cerrado**:
solo un programa a la vez puede abrir el puerto COM.

```
python robot.py COM7       comprobación rápida, sin ventana
python pendant.py          el teach pendant
python -m unittest test_protocolo -v      tests, sin hardware
```

En el pendant:

1. Elige el puerto y pulsa **Conectar**. Debe decir `Bus 115200 8E1 ✓ sincronizado`.
2. **ENABLE**. Lleva el brazo (o los ejes sueltos) a la pose de referencia y
   pulsa **SETZERO**.
3. Muévete con el jog articular o el cartesiano. **Esc = STOP** desde
   cualquier parte de la ventana. Después de un STOP hay que pulsar
   **CLEAR** antes de mandar un `MOVJ` o un `MOVL`.
4. **Guardar pose actual…** graba el punto con un nombre en `puntos.json`.
   **Ir (MOVJ)** e **Ir (MOVL)** vuelven a él.
5. **Ir a coordenada:** escribe los ángulos (q1, q2) o la posición (X, Y) en
   mm. **Copiar la posición actual** rellena los campos para editar solo lo
   que quieres cambiar. Para X, Y hay dos botones: **MOVJ** (la PC calcula los
   ángulos con la cinemática inversa y llega por cualquier camino) y **MOVL**
   (en línea recta; puede rechazarse si la recta sale del alcance). Usa punto
   decimal, no coma.

Al conectar, el pendant **no reinicia el ESP32**: abre el puerto con DTR y
RTS en bajo. Si aun así tu placa se reinicia al conectar, se pierde el cero:
vuelve a hacer `SETZERO`.

## Guion de la demo de la U2

| Pts | Criterio | Qué mostrar en el pendant |
|---|---|---|
| 10 | Bus | Al conectar: `Bus 115200 8E1 ✓ sincronizado`. Explica que el `PING` trae la configuración del firmware y la PC la compara con la suya |
| 10 | Formato de trama | Cualquier botón de inyección muestra en el registro los bytes de la trama: `aa` inicio, `LEN`, `SEQ`, `CMD`, `MOTOR`, `FLAGS`, payload, CRC, `55` fin. Un jog lleva el **conteo de pasos** y la dirección en `FLAGS` |
| 20 | Integridad | **CRC corrupto**, **Trama cortada** y **Sin EOF**: el robot rechaza cada una con su código y los contadores de `err CRC`, `err timeout` y `err trama` suben |
| 30 | Parseo en tiempo real | La línea `latencia en el ESP32` (decenas de µs) y la `ida y vuelta` arriba a la derecha (~1 ms). El jog responde al instante. **Ráfaga 2 KB**: el robot sigue respondiendo |
| 20 | Manejo de errores | **Fuera de rango**, **Motor inválido** y **Cmd desconocido** se rechazan sin moverse. **Desborde**: el contador sube y el robot sigue vivo. **Watchdog**: manda un `Ir (MOVJ)` lento y desmarca **Latido activo**: al segundo, el robot frena solo con `ENLACE_PERDIDO` |
| 10 | Integración | Cada botón de jog mueve el motor físico; **Ir (MOVJ)** a un punto guardado |
