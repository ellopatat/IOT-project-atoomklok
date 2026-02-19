#include "oled_task.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/i2c_master.h"
#include <ssd1306.h>
#include <inttypes.h>

#include "time_converter_task.h"

//pins on esp32
#define OLED_SDA GPIO_NUM_8
#define OLED_SCL GPIO_NUM_9

static void oled_show(ssd1306_handle_t dev, const beat_time *bt)
{
    char line0[32];
    char line1[32];


    snprintf(line0, sizeof(line0), "Beats: %" PRId64, bt->beats);
    snprintf(line1, sizeof(line1), "Centibeats: %" PRId64, bt->centibeats);


    ssd1306_clear_display(dev, false);
    ssd1306_set_contrast(dev, 0xff);//maximum brightness
    ssd1306_display_text(dev, 0, line0, false);
    ssd1306_display_text(dev, 2, line1, false);

}

void oled_task(void *arg)
{



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
    ssd1306_display_text(dev, 0, "WiFi...", false);
    ssd1306_display_text(dev, 2, "Wacht op data", false);

    //wait for data to arrive in the queue and show it on the OLED

    while (1) {
        beat_time bt = {0};
        if (xQueueReceive(arg, &bt, portMAX_DELAY) == pdTRUE) {//pdTRUE means we got data, portMAX_DELAY means wait indefinitely
            oled_show(dev, &bt);
        }
    }
}
