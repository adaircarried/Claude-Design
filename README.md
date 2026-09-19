# Claude-Design

Proyecto ESP32-S3 con ESP-IDF, desarrollado con la ayuda del plugin [ESP32-AI-Agent-Skill](https://github.com/ezrover/ESP32-AI-Agent-Skill) (chip selection, validación de pines GPIO, generación de código, referencias de LVGL y Waveshare).

## Requisitos

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/index.html) instalado y con `IDF_PATH` configurado
- Chip objetivo: ESP32-S3

## Compilar y flashear

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p <PUERTO> flash monitor
```
