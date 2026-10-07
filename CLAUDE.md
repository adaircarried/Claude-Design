# Péndulo invertido lineal (ESP32 + stepper)

Proyecto de Control Avanzado (control digital). Péndulo invertido sobre carro en riel V-Slot.
Objetivo: estabilizar el ángulo del péndulo en la vertical con un PID/PD discreto en el ESP32.

Comentarios y mensajes en español. Unidades SI en todo el código (m, s, rad).

## Hardware

| Elemento | Detalle |
|---|---|
| Microcontrolador | ESP32 (doble núcleo, 240 MHz) |
| Actuador | NEMA 17 sin reducción + driver DRV8825 o A4988, microstepping 1/8 |
| Transmisión | Banda GT2, polea 20 dientes = 40 mm por vuelta |
| Conversión | 200 x 8 = 1600 pasos/vuelta = **40,000 pasos/m** (0.025 mm/paso) |
| Sensor de ángulo | Encoder incremental LPD3806-600BM (NPN colector abierto) |
| Resolución encoder | 600 PPR x 4 = **2400 cuentas/vuelta** (0.15° por cuenta) |
| Montaje | Péndulo directo al eje del encoder (sin rodamientos externos) |
| Seguridad | 2 finales de carrera en los extremos del riel |
| Alimentación motor | 24 V al driver, con capacitor de 100 µF en VMOT |

## Pines

| Señal | GPIO |
|---|---|
| Encoder A (verde) | 32 (pull-up externa 4.7k a 3.3 V) |
| Encoder B (blanco) | 33 (pull-up externa 4.7k a 3.3 V) |
| STEP | 25 |
| DIR | 26 |
| ENABLE (activo bajo) | 27 |
| Final de carrera izq. | por definir |
| Final de carrera der. | por definir |

Encoder alimentado a 5 V; sus señales NUNCA deben subir a 5 V en el ESP32.

## Modelo y control

Entrada de la planta = **aceleración del carro** `a` [m/s²] (el stepper la impone; la masa del carro no aparece).

- No lineal: `theta_dd = (g*sin(theta) - a*cos(theta)) / l`
- Lineal: `Theta(s)/A(s) = -1 / (l s^2 - g)`
- Parámetros: `g = 9.81`, `l = 0.342 m` (actualizar con medición real)

Ley de control (PD, Ki = 0):
`a = Kp*theta + Kd*theta_dot`, con `Kp = 44.0`, `Kd = 4.79`
(asignación de polos, wn = 10 rad/s, zeta = 0.7).

`theta` = 0 en la vertical arriba, positivo hacia el mismo sentido en que avanza el carro con `a > 0`.
Si el carro corre al lado contrario de la caída, invertir el signo de la salida.

Referencia del encoder: se toma con el péndulo **colgando**; vertical arriba = colgando + 1200 cuentas.

## Arquitectura de software

- **Núcleo 0 (control, tiempo real):** tarea de alta prioridad disparada por timer de hardware a periodo fijo
  `Ts = 2 ms` (500 Hz). Lee encoder, calcula theta y theta_dot, PD, saturación, envía aceleración al stepper.
  Nada de `Serial`, WiFi ni `delay` aquí. El periodo debe ser determinista.
- **Núcleo 1:** telemetría (serial o WiFi), comandos de usuario, cambio de ganancias en vivo.
- Comunicación entre núcleos con colas o variables protegidas (no compartir sin protección).
- Encoder por PCNT (librería ESP32Encoder) con filtro de hardware activado.
- Pasos por hardware con FastAccelStepper (`moveByAcceleration` para comandar aceleración).
- Derivada con filtro paso bajo (no derivar la cuenta cruda).

## Seguridad (obligatorio en todo programa que mueva el motor)

- Saturar aceleración y velocidad a los máximos medidos en la prueba de banco.
- Final de carrera activado → detener y deshabilitar el driver de inmediato.
- Si |theta| > 30° → apagar el control (el péndulo ya cayó).
- Al arrancar: homing con los finales de carrera y centrar el carro antes de habilitar el control.

## Estructura del proyecto (PlatformIO)

Entornos en `platformio.ini`:
- `test_encoder`: lectura del encoder y validación de cuentas.
- `test_stepper`: aceleración máxima sin pérdida de pasos.
- `main`: control completo.

Módulos compartidos en `lib/` o `src/` (encoder, motor, control, telemetría) para que las pruebas
reutilicen exactamente el mismo código que el programa final.

## Pendientes conocidos

1. Validar encoder (2400 cuentas/vuelta, sin pérdidas, sin ruido con el motor encendido).
2. Medir aceleración máxima del carro sin perder pasos (meta >= 15 m/s²).
3. El control solo de ángulo deja deriva del carro (~0.7 m en 3 s en simulación):
   agregar lazo externo lento de posición/velocidad del carro usando la cuenta de pasos.
