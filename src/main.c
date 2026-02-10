#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "wifi_task.h"
#include "sntp_task.h"

static const char *TAG = "main";

EventGroupHandle_t s_wifi_event_group = NULL;


void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(10000)); //wait for the serial monitor to be ready before printing anything
    // NVS nodig voor WiFi
    nvs_flash_init();
    s_wifi_event_group = xEventGroupCreate();
    
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init failed (%d)", (int)err);
        return;
    }
    
    xTaskCreate(wifi_task, "wifi_task", 4096, NULL, 5, NULL);
    uint32_t unix_time = 0;
    xTaskCreate(sntp_task, "sntp_task", 4096, &unix_time, 5, NULL);
}
