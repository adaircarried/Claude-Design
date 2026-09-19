#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "claude-design";

void app_main(void)
{
    ESP_LOGI(TAG, "Claude-Design starting on ESP32-S3");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
