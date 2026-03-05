#include "oled_task.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include <ssd1306.h>
#include <inttypes.h>
#include "esp_log.h"

#include "time_converter_task.h"
#include "wifi_task.h"

//pins on esp32
#define OLED_SDA GPIO_NUM_8
#define OLED_SCL GPIO_NUM_9

#define LED_MODE GPIO_NUM_36//mode LED

void oled_status(ssd1306_handle_t dev, network_status net_status)
{
    static uint8_t led_status = 2;
    //ESP_LOGI("oled_task", "Updating OLED status: %d", net_status);
    //ESP_LOGI("oled_task", "Updating OLED led_status: %d", led_status);
    char line1[64];
    switch (net_status) {
        case NET_VERBONDEN:
            snprintf(line1, sizeof(line1), "Verbonden");
            led_status=1;
            gpio_set_level(LED_MODE, led_status);
            break;
        case NET_NIET_VERBONDEN:
            snprintf(line1, sizeof(line1), "Niet verbonden");
            led_status=0;
            gpio_set_level(LED_MODE, led_status);
            break;
        case NET_NIET_BESCHIKBAAR:
            led_status=!led_status;
            gpio_set_level(LED_MODE, led_status);
            snprintf(line1, sizeof(line1), "Niet beschikbaar");

            break;
        case NET_ERROR:
        default:
            led_status=!led_status;
            gpio_set_level(LED_MODE, led_status);
            snprintf(line1, sizeof(line1), "error");
            break;
    }

    ssd1306_display_text(dev, 2, "                ", false);
    ssd1306_set_contrast(dev, 0xff);//maximum brightness
    ssd1306_display_text(dev, 2, line1, false);
}

static void oled_show(ssd1306_handle_t dev, const beat_time *bt, uint8_t net_status)
{
    char line0[64];

    oled_status(dev, net_status);

    snprintf(line0, sizeof(line0),"BS:%03" PRId64 " CB:%05" PRId64,bt->beats,bt->centibeats);
    //snprintf(line1, sizeof(line1), "Centibeats:%05" PRId64, bt->centibeats);


    ssd1306_display_text(dev, 0, "                ", false);//to clear the line
    ssd1306_set_contrast(dev, 0xff);//maximum brightness
    ssd1306_display_text(dev, 0, line0, false);


}



void oled_task(void *arg)
{
    gpio_set_direction(LED_MODE, GPIO_MODE_OUTPUT);

    oled_args *a = arg;
    network_status net_status;
    network_status net_status_val = NET_NIET_VERBONDEN;
    if(xQueueReceive(a->net_queue, &net_status, 0)){
        net_status_val = net_status;
    }

    i2c_master_bus_handle_t i2c0_bus_hdl;
    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .sda_io_num = OLED_SDA,
        .scl_io_num = OLED_SCL,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_new_master_bus(&bus_cfg, &i2c0_bus_hdl);


    ssd1306_config_t dev_cfg = I2C_SSD1306_128x64_CONFIG_DEFAULT;
    ssd1306_handle_t dev = NULL;
    ssd1306_init(i2c0_bus_hdl, &dev_cfg, &dev);

    // Startup screen
    ssd1306_clear_display(dev, false);
    ssd1306_display_text(dev, 0, "Wacht op data", false);
    oled_status(dev, net_status_val);

    //wait for data to arrive in the queue and show it on the OLED

    while (1) {
        beat_time bt = {0};
        if(xQueueReceive(a->net_queue, &net_status, 0)){
        net_status_val = net_status;
        }
        if (xQueueReceive(a->oled_queue, &bt, pdMS_TO_TICKS(864)) == pdTRUE) {//pdTRUE means we got data, portMAX_DELAY means wait 1 centibeat (864ms)
            oled_show(dev, &bt, net_status_val);
        }else{
            //no new beat_time, but we still want to update the network status in case it has changed.
            oled_status(dev, net_status_val);
        }
    }
}
