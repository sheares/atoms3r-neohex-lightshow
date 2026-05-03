#include "lightshow.h"
#include <math.h>
#include "esp_random.h"

/* True hex ring distances from center (U19 = index 18).
   Layout rows 4-5-6-7-6-5-4, read left-to-right top-to-bottom. */
static const uint8_t RING_OF[NUM_LEDS] = {
    3,3,3,3,          /* row 0: U1-U4  */
    3,2,2,2,3,        /* row 1: U5-U9  */
    3,2,1,1,2,3,      /* row 2: U10-U15 */
    3,2,1,0,1,2,3,    /* row 3: U16-U22 (U19=center) */
    3,2,1,1,2,3,      /* row 4: U23-U28 */
    3,2,2,2,3,        /* row 5: U29-U33 */
    3,3,3,3           /* row 6: U34-U37 */
};

void lightshow_init(led_strip_handle_t strip)
{
    led_strip_clear(strip);
    led_strip_refresh(strip);
}

static void effect_rainbow_ripple(led_strip_handle_t strip, uint32_t t)
{
    for (int i = 0; i < NUM_LEDS; i++) {
        uint16_t hue = (uint16_t)((t / 10 + RING_OF[i] * 40) % 360);
        led_strip_set_pixel_hsv(strip, i, hue, 220, 13);
    }
    led_strip_refresh(strip);
}

static void effect_breathe(led_strip_handle_t strip, uint32_t t)
{
    float phase = (float)(t % 2000) / 2000.0f * 2.0f * (float)M_PI;
    uint8_t brightness = (uint8_t)((sinf(phase) * 0.5f + 0.5f) * 13.0f);
    uint16_t hue = (t / 20) % 360;
    for (int i = 0; i < NUM_LEDS; i++) {
        led_strip_set_pixel_hsv(strip, i, hue, 240, brightness);
    }
    led_strip_refresh(strip);
}

static void effect_sparkle(led_strip_handle_t strip, uint32_t t)
{
    static uint8_t r[NUM_LEDS], g[NUM_LEDS], b[NUM_LEDS];
    static uint32_t last_t = 0;

    if (t - last_t < 30) {
        for (int i = 0; i < NUM_LEDS; i++) led_strip_set_pixel(strip, i, r[i], g[i], b[i]);
        led_strip_refresh(strip);
        return;
    }
    last_t = t;

    for (int i = 0; i < NUM_LEDS; i++) {
        r[i] = r[i] > 1 ? r[i] - 1 : 0;
        g[i] = g[i] > 1 ? g[i] - 1 : 0;
        b[i] = b[i] > 1 ? b[i] - 1 : 0;
    }
    for (int n = 0; n < 3; n++) {
        int idx = esp_random() % NUM_LEDS;
        uint16_t hue = (uint16_t)(esp_random() % 360);
        /* HSV->RGB inline for sparkle state cache */
        uint8_t rr, gg, bb;
        uint16_t region = hue / 60;
        uint16_t rem    = (hue - region * 60) * 135 / 60;
        uint8_t  q = (13 * (135 - rem)) >> 8;
        uint8_t  t2 = (13 * rem) >> 8;
        switch (region) {
            case 0: rr=13;  gg=t2;  bb=0;   break;
            case 1: rr=q;   gg=13;  bb=0;   break;
            case 2: rr=0;   gg=13;  bb=t2;  break;
            case 3: rr=0;   gg=q;   bb=13;  break;
            case 4: rr=t2;  gg=0;   bb=13;  break;
            default:rr=13;  gg=0;   bb=q;   break;
        }
        r[idx]=rr; g[idx]=gg; b[idx]=bb;
    }
    for (int i = 0; i < NUM_LEDS; i++) led_strip_set_pixel(strip, i, r[i], g[i], b[i]);
    led_strip_refresh(strip);
}

static void effect_ring_chase(led_strip_handle_t strip, uint32_t t)
{
    uint8_t active = (t / 300) % 4;
    uint16_t hue   = (t / 15) % 360;
    for (int i = 0; i < NUM_LEDS; i++) {
        if (RING_OF[i] == active) led_strip_set_pixel_hsv(strip, i, hue, 230, 13);
        else                      led_strip_set_pixel(strip, i, 0, 0, 0);
    }
    led_strip_refresh(strip);
}

void lightshow_tick(led_strip_handle_t strip, effect_t effect, uint32_t tick_ms)
{
    switch (effect) {
        case EFFECT_RAINBOW_RIPPLE: effect_rainbow_ripple(strip, tick_ms); break;
        case EFFECT_BREATHE:        effect_breathe(strip, tick_ms);        break;
        case EFFECT_SPARKLE:        effect_sparkle(strip, tick_ms);        break;
        case EFFECT_RING_CHASE:     effect_ring_chase(strip, tick_ms);     break;
        default: break;
    }
}
