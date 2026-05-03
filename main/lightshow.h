#pragma once
#include "led_strip.h"

#define NUM_LEDS       37
#define LED_GPIO       2    // ATOM S3R Grove data pin
#define LED_RMT_RES_HZ (10 * 1000 * 1000)

/* NeoHEX ring layout: center(1) + ring1(6) + ring2(12) + ring3(18) = 37 */
#define RING0_START  0
#define RING0_COUNT  1
#define RING1_START  1
#define RING1_COUNT  6
#define RING2_START  7
#define RING2_COUNT  12
#define RING3_START  19
#define RING3_COUNT  18

typedef enum {
    EFFECT_RAINBOW_RIPPLE,
    EFFECT_BREATHE,
    EFFECT_SPARKLE,
    EFFECT_RING_CHASE,
    EFFECT_COUNT
} effect_t;

void lightshow_init(led_strip_handle_t strip);
void lightshow_tick(led_strip_handle_t strip, effect_t effect, uint32_t tick_ms);
