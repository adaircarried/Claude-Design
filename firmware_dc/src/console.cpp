#include "console.h"
#include <stdarg.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static SemaphoreHandle_t s_mutex = nullptr;

void console_init() { s_mutex = xSemaphoreCreateRecursiveMutex(); }

void con_lock()   { xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY); }
void con_unlock() { xSemaphoreGiveRecursive(s_mutex); }

void con_printf(const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    con_lock();
    Serial.print(buf);
    con_unlock();
}
