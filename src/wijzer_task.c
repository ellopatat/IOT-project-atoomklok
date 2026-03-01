#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include <stdint.h>
#include <math.h>
#include "esp_log.h"

#include "wijzer_task.h"
#include "time_converter_task.h"

#define IN1 GPIO_NUM_12
#define IN2 GPIO_NUM_13
#define IN3 GPIO_NUM_14
#define IN4 GPIO_NUM_15

#define STEPS_PER_REV 4096

static const uint8_t seq_halfstep[8][4] = {
    {1,0,0,0},
    {1,1,0,0},
    {0,1,0,0},
    {0,1,1,0},
    {0,0,1,0},
    {0,0,1,1},
    {0,0,0,1},
    {1,0,0,1}
};

void set_step(uint8_t step) {
    gpio_set_level(IN1, seq_halfstep[step][0]);
    gpio_set_level(IN2, seq_halfstep[step][1]);
    gpio_set_level(IN3, seq_halfstep[step][2]);
    gpio_set_level(IN4, seq_halfstep[step][3]);
}

void stepper_task(void *arg)
{
    gpio_set_direction(IN1, GPIO_MODE_OUTPUT);
    gpio_set_direction(IN2, GPIO_MODE_OUTPUT);
    gpio_set_direction(IN3, GPIO_MODE_OUTPUT);
    gpio_set_direction(IN4, GPIO_MODE_OUTPUT);

    QueueHandle_t q = (QueueHandle_t)arg;
    beat_time bt;

    int current_steps = 0;
    int cb_per_step=0;
    int target_step=0;
    int diff_steps = 0;
    int delay_ms = 21;
    static int phase = 0;//to keep track of the moter phase.

    while (1) {
        xQueuePeek(q, &bt, 0);//wait for new beat_time in the queue, but don't remove it from the queue because other tasks also need it.

        cb_per_step=(bt.centibeats % 100);//keep track of the current centibeat (beat)cycle.
        target_step=cb_per_step*STEPS_PER_REV/100;//calculate the target step based on the current (beat)cycle.
        diff_steps = (target_step - current_steps + STEPS_PER_REV)% STEPS_PER_REV;//calculate the difference between the current step and the target step.

        //this code tells the moter to speed up when it's behind and slow down when it's ahead or on target.
        if(diff_steps>0){
            delay_ms = 10;//move faster
        }else{
            delay_ms = 21;//move slower
        }
        

        //debug
        ESP_LOGI("stepper_task", "cb_per_step: %d, target_step: %d, current_steps: %d, diff_steps: %d, delay_ms: %d", cb_per_step, target_step, current_steps, diff_steps, delay_ms);

        //moves the actual moter every time the task runs, it moves only one step per (while-loop)cycle.
        if(diff_steps>0){ 
            phase = (phase + 1) % 8;//to keep track of the moter phase in the event
            current_steps = (current_steps + 1) % STEPS_PER_REV;
            set_step(phase);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    }
}


