#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "led_strip.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lightshow.h"

#define BUTTON_GPIO     41      /* ATOM S3R built-in button */
#define TICK_MS         20      /* ~50 fps */

static const char *TAG = "lightshow";

static led_strip_handle_t strip;
static volatile effect_t  current_effect = EFFECT_RAINBOW_RIPPLE;

static void IRAM_ATTR button_isr(void *arg)
{
    static uint32_t last_press = 0;
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    if (now - last_press < 200) return;   /* debounce 200ms */
    last_press = now;
    current_effect = (current_effect + 1) % EFFECT_COUNT;
}

static void setup_button(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&cfg);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, button_isr, NULL);
}

static led_strip_handle_t setup_led_strip(void)
{
    led_strip_config_t strip_cfg = {
        .strip_gpio_num        = LED_GPIO,
        .max_leds              = NUM_LEDS,
        .led_model             = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out      = false,
    };
    led_strip_rmt_config_t rmt_cfg = {
        .clk_src       = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_RMT_RES_HZ,
        .flags.with_dma = false,
    };
    led_strip_handle_t h;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &h));
    return h;
}

void app_main(void)
{
    ESP_LOGI(TAG, "ATOM S3R + NeoHEX light show starting");

    setup_button();
    strip = setup_led_strip();
    lightshow_init(strip);

    uint32_t t = 0;
    while (1) {
        lightshow_tick(strip, current_effect, t);
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
        t += TICK_MS;
    }
}
