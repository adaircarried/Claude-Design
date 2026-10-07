# Péndulo invertido lineal (ESP32 + stepper)

Péndulo invertido sobre carro en riel V-Slot, estabilizado con un PD discreto en un ESP32.
Detalles de hardware, modelo y reglas del proyecto en [CLAUDE.md](CLAUDE.md).

## Estructura

```
platformio.ini          entornos test_encoder, test_stepper, main (+ native para pruebas en PC)
lib/
  config/config.h       pines y constantes (única fuente de verdad)
  encoder/              EncoderPendulo (PCNT) + encoder_math.h (theta, derivada filtrada)
  periodico/            tarea de tiempo real disparada por timer de hardware (núcleo 0)
src/
  test_encoder/         validación del encoder
  test_stepper/         aceleración máxima sin pérdida de pasos (pendiente)
  main/                 control completo (pendiente)
test/
  test_encoder_math/    pruebas unitarias de encoder_math.h
```

Cada entorno compila solo su carpeta de `src/`; los módulos de `lib/` son los mismos para
las pruebas y para el programa final.

## Uso

Requiere [PlatformIO](https://platformio.org/) (`pip install platformio`).

```bash
pio run -e test_encoder -t upload -t monitor
pio test -e native            # matemática del encoder, sin hardware
```

## Validación del encoder (`test_encoder`)

El programa lee el encoder en la misma tarea periódica (núcleo 0, Ts = 2 ms) que usará el
control e imprime a 10 Hz. El motor nunca da pasos.

| Comando | Acción |
|---|---|
| `z` | referencia: péndulo colgando y quieto → cuenta 0 |
| `m` | marca la cuenta actual |
| `r` | reinicia el rango min..max de cuentas y las estadísticas del periodo |
| `e` | energiza / desenergiza el driver (sin pasos) |
| `p` | pausa / reanuda la impresión |
| `h` | ayuda |

Procedimiento:

1. **Referencia:** péndulo colgando y quieto, `z`. La cuenta debe quedar en 0 y theta en ±180°.
2. **Cuentas por vuelta:** marca un punto, `m`, gira exactamente N vueltas en un sentido.
   "desde marca" debe dar N × 2400 (N vueltas). Repite en el otro sentido.
3. **Sin pérdidas:** gira de ida y vuelta muchas veces, rápido, y regresa a la marca. Debe volver a 0 desde la marca (±1 por posicionamiento a mano).
4. **Signo:** sube el péndulo a la vertical: theta ≈ 0°. Inclínalo hacia donde avanza el carro con
   `a > 0`: theta debe ser positivo; si no, cambia `ENC_SIGNO` en `config.h`.
5. **Ruido con motor encendido:** péndulo quieto, `e` para energizar el driver, `r`.
   Tras unos minutos el rango min..max debe seguir siendo un solo valor (o ±1 si el péndulo vibra).
6. **Temporización:** periodo min/max cerca de 2000 µs y `atrasos` en 0.
