#pragma once

#include <stdio.h>

typedef struct {
    int64_t beats;
    int64_t centibeats;
}beat_time;


void timer_calc();
void time_converter_task(void *arg);
