#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"


void oled_task(void *arg);

typedef struct {
    QueueHandle_t oled_queue;
    QueueHandle_t net_queue;
} oled_args;