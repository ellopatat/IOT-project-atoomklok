#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/queue.h"

#include "wifi_task.h"
#include "sntp_task.h"
#include "time_converter_task.h"
#include "oled_task.h"
#include "wijzer_task.h"

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
    wifi_init();//init wifi after nvs flash is initialized

    static unix_args time_args;//struct to hold all time_related args 
    static oled_args oled_args;//struct to hold all oled_related args

    uint32_t unix_time = 0;

    QueueHandle_t unix_queue = xQueueCreate(1, sizeof(uint32_t));
    QueueHandle_t beat_queue = xQueueCreate(1, sizeof(beat_time));
    QueueHandle_t oled_queue = xQueueCreate(1, sizeof(beat_time));
    QueueHandle_t net_queue  = xQueueCreate(1, sizeof(network_status)); //for the lcd and led


    time_args.unix_time = &unix_time;
    time_args.unix_queue = unix_queue;
    time_args.beat_queue = beat_queue;
    time_args.oled_queue = oled_queue;
    time_args.net_queue = net_queue;//for the lcd and led

    oled_args.oled_queue = oled_queue;
    oled_args.net_queue  = net_queue;
    xTaskCreate(wifi_task, "wifi_task", 4096, &time_args, 5, NULL);
    xTaskCreate(time_converter_task, "time_converter_task", 4096, &time_args, 10, NULL);

    xTaskCreate(stepper_task, "stepper_task", 4096, beat_queue, 5, NULL);
    xTaskCreate(oled_task, "oled_task", 4096, &oled_args, 10, NULL);


}
