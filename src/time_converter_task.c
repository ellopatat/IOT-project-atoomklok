#include "time_converter_task.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "wifi_task.h"

#define MICROSEC_IN_SEC 1000000

static const char *TAG = "time_converter_task";
int64_t last_time = 0;
int64_t unix_base = 0;
int64_t us_base=0;

void resync_time(uint32_t *unix_time){
     unix_base= *unix_time;
     us_base = esp_timer_get_time();
}

void timer_calc(uint32_t *unix_time, beat_time *bt)
{
    int64_t us_now = esp_timer_get_time();
    int64_t unix_time_now = unix_base + (us_now - us_base)/MICROSEC_IN_SEC;// seconds now

    bt->centibeats = (((unix_time_now + 3600) % 86400) * 100000) / 86400; // 100000 centibeats/day
    bt->beats      = bt->centibeats / 100;
    
    last_time = esp_timer_get_time();
}

void time_converter_task(void *arg)
{
    unix_args *arg_ptrs = arg;
    xQueueReceive(arg_ptrs->unix_queue, arg_ptrs->unix_time, portMAX_DELAY);
    resync_time(arg_ptrs->unix_time);
    while (1) {
        if(xQueueReceive(arg_ptrs->unix_queue, arg_ptrs->unix_time, 0) == pdTRUE){//if there is new unix time in the queue, resync the time.
            resync_time(arg_ptrs->unix_time);
        }
        beat_time bt= {0};

        timer_calc(arg_ptrs->unix_time, &bt);
        xQueueSend(arg_ptrs->beat_queue, &bt, 0);
        ESP_LOGI(TAG, "Beats: %" PRId64 ", Centibeats: %" PRId64, bt.beats, bt.centibeats);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}