/*
 * Onboard addressable RGB LED (WS2812) — one glance tells you the mode:
 *   idle             dim white, steady
 *   scanning         blue, steady
 *   counting devices cyan, steady
 *   attacking        red, blinking
 *   god mode         magenta, fast strobe
 *
 * If your board's RGB LED is on a different pin, change LED_GPIO below.
 * Common ESP32-C5 DevKit values: 27 (DevKitC-1), else try 8 / 35 / 48.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_strip.h"
#include "led.h"
#include "control.h"

#define LED_GPIO 27

static const char *TAG = "LED";
static led_strip_handle_t s_strip;

static void put(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_strip) return;
    led_strip_set_pixel(s_strip, 0, r, g, b);
    led_strip_refresh(s_strip);
}

// God-mode palette: gold + full rainbow, cycled fast.
static const uint8_t GOD[][3] = {
    {150,110,0}, {150,0,0}, {150,70,0}, {130,130,0},
    {0,150,0}, {0,130,130}, {0,0,150}, {90,0,150}, {150,0,90},
};
#define GOD_N (sizeof(GOD)/sizeof(GOD[0]))

static void led_task(void *arg)
{
    (void)arg;
    int step = 0;
    for (;;) {
        int code = control_led_code();
        int phase = step & 1;
        int period = 400;
        switch (code) {
            case 1: put(0, 0, 80);              break;   // scanning  - blue
            case 2: put(0, 60, 60);             break;   // devices   - cyan
            case 3: put(phase ? 110 : 2, 0, 0); period = 280; break; // attack - red blink
            case 4: {                                     // god - fast rainbow+gold
                const uint8_t *c = GOD[step % GOD_N];
                put(c[0], c[1], c[2]);
                period = 55;
                break;
            }
            default: put(6, 6, 6);              break;   // idle - dim white
        }
        step++;
        vTaskDelay(pdMS_TO_TICKS(period));
    }
}

void led_init(void)
{
    led_strip_config_t sc = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,   // default color order GRB
        .flags = { .invert_out = false },
    };
    led_strip_rmt_config_t rc = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .flags = { .with_dma = false },
    };
    if (led_strip_new_rmt_device(&sc, &rc, &s_strip) != ESP_OK) {
        ESP_LOGW(TAG, "LED init failed (wrong GPIO?) — status LED disabled");
        s_strip = NULL;
        return;
    }
    put(6, 6, 6);
    xTaskCreate(led_task, "led", 2560, NULL, 3, NULL);
    ESP_LOGI(TAG, "status LED on GPIO%d", LED_GPIO);
}
