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

    //calculate everything in microseconds to keep the precision.
    bt->centibeats = ((((unix_base + 3600) % 86400) * MICROSEC_IN_SEC + (us_now - us_base)) % ((int64_t)86400 * MICROSEC_IN_SEC) * 100000) / ((int64_t)86400 * MICROSEC_IN_SEC);
    bt->beats      = bt->centibeats / 100;
    
    last_time = esp_timer_get_time();
}

void time_converter_task(void *arg)
{
    unix_args *arg_ptrs = arg;
    xQueueReceive(arg_ptrs->unix_queue, arg_ptrs->unix_time, portMAX_DELAY);
    resync_time(arg_ptrs->unix_time);
    beat_time bt= {0};
    while (1) {
        if(xQueueReceive(arg_ptrs->unix_queue, arg_ptrs->unix_time, 0) == pdTRUE){//if there is new unix time in the queue, resync the time.
            resync_time(arg_ptrs->unix_time);
        }

        uint64_t temp_centibeats = bt.centibeats;
        timer_calc(arg_ptrs->unix_time, &bt);


        if(bt.centibeats != temp_centibeats){
            xQueueOverwrite(arg_ptrs->beat_queue, &bt);
            xQueueOverwrite(arg_ptrs->oled_queue, &bt);
            ESP_LOGI(TAG, "Beats: %" PRId64 ", Centibeats: %" PRId64, bt.beats, bt.centibeats);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}