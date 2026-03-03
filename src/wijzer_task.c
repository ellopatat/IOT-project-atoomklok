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

#define S1    GPIO_NUM_4//S1
#define S2    GPIO_NUM_5// S2
#define ROT_KEY  GPIO_NUM_6//drukknop
#define LED_MODE GPIO_NUM_7//mode LED

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

    //mode LED
    gpio_set_direction(LED_MODE, GPIO_MODE_OUTPUT);

    // rotary inputs + pullups
    gpio_set_direction(S1, GPIO_MODE_INPUT);
    gpio_set_direction(S2, GPIO_MODE_INPUT);
    gpio_set_direction(ROT_KEY, GPIO_MODE_INPUT);
    gpio_pullup_en(S1);
    gpio_pullup_en(S2);
    gpio_pullup_en(ROT_KEY);

    QueueHandle_t q = (QueueHandle_t)arg;
    beat_time bt;

    int current_steps = 0;
    int cb_per_step=0;
    int target_step=0;
    int diff_steps = 0;
    int delay_ms = 21;
    static int phase = 0;//to keep track of the moter phase.

    //mode: manual=1 ,auto=0
    int manual_mode = 1;
    gpio_set_level(LED_MODE, 1); //manual mode at startup

    // simpele rotary state
    int last_a = gpio_get_level(S1);
    int last_key = gpio_get_level(ROT_KEY);

    while (1) {
        // ====rotory encoder code====
        int key = gpio_get_level(ROT_KEY); //internal pullup so 0 when pressed, 1 when not pressed
        if (last_key == 1 && key == 0) {
            //debounce basic
            vTaskDelay(pdMS_TO_TICKS(30));
            if (gpio_get_level(ROT_KEY) == 0) {
                manual_mode = !manual_mode;
                gpio_set_level(LED_MODE, manual_mode);
            }
        }
        last_key = key;

        
        if (manual_mode) {
            int a = gpio_get_level(S1);
            if (a != last_a) {//see if S1 has changed
                int b = gpio_get_level(S2);//read S2 to determine direction

                //if s2 is different from s1, we are moving in one direction, if it's the same, we are moving in the other direction.
                if (b != a) {
                    //one step forward
                    phase = (phase + 1) % 8;
                } else {
                    //one step backward
                    phase = (phase + -1 + 8) % 8;
                }

                set_step(phase);
            }
            last_a = a;

            vTaskDelay(pdMS_TO_TICKS(5));//delay to not block
            continue; 
        }


        //========auto mode========
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
        }else{
            vTaskDelay(pdMS_TO_TICKS(2));//prevent blocking even when we are on target or ahead
        }
    }
}


