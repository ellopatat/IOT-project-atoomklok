#pragma once
#include "time_converter_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"


#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1


/* FreeRTOS event group to signal when we are connected*/
extern EventGroupHandle_t s_wifi_event_group;

void wifi_task(void* arg);
void wifi_init();

typedef struct {
    uint32_t *unix_time;
    QueueHandle_t unix_queue;
    QueueHandle_t beat_queue;
    QueueHandle_t oled_queue;

    QueueHandle_t net_queue;
} unix_args;

typedef enum {
    NET_VERBONDEN,
    NET_NIET_VERBONDEN,
    NET_NIET_BESCHIKBAAR,
    NET_ERROR
} network_status;