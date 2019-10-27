#include "quantum.h"
#include "ws2812.h"
#include "ch.h"
#include "hal.h"

#if !defined(LED_ARRAY) && defined(RGB_MATRIX_ENABLE)
// LED color buffer
LED_TYPE led[DRIVER_LED_TOTAL];
#    define LED_ARRAY led
#endif

#ifdef RGB_MATRIX_ENABLE
// Set an led in the buffer to a color
void inline ws2812_setled(int i, uint8_t r, uint8_t g, uint8_t b) {
    led[i].r = r;
    led[i].g = g;
    led[i].b = b;
}

void ws2812_setled_all(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < sizeof(led) / sizeof(led[0]); i++) {
        led[i].r = r;
        led[i].g = g;
        led[i].b = b;
    }
}
#endif

/* adapted from
https://github.com/bigjosh/SimpleNeoPixelDemo/blob/master/SimpleNeopixelDemo/SimpleNeopixelDemo.ino
*/

#define NS_PER_SEC (1000000000L)  // Note that this has to be SIGNED since we want to be able to check for negative values of derivatives

#define CYCLES_PER_INSTRUCTION 0.35

#define CYCLES_PER_SEC (STM32_SYSCLK * CYCLES_PER_INSTRUCTION)

#define NS_PER_CYCLE (NS_PER_SEC / CYCLES_PER_SEC)

#define NS_TO_CYCLES(n) ((n) / NS_PER_CYCLE)

#define wait_ns(x)                              \
    for (int i = 0; i < NS_TO_CYCLES(x); i++) { \
        __NOP();                                \
    }

// These are the timing constraints taken mostly from the WS2812 datasheets
// These are chosen to be conservative and avoid problems rather than for maximum throughput

#define T1H 900  // Width of a 1 bit in ns
#define T1L 600  // Width of a 1 bit in ns

#define T0H 400  // Width of a 0 bit in ns
#define T0L 900  // Width of a 0 bit in ns

// The reset gap can be 6000 ns, but depending on the LED strip it may have to be increased
// to values like 600000 ns. If it is too small, the pixels will show nothing most of the time.
#define RES 7000  // Width of the low gap between bits to cause a frame to latch

void sendByte(uint8_t byte) {
    for (unsigned char bit = 0; bit < 8; bit++) {
        bool is_one = byte & (1 << (7));
        if (is_one) {
            // 1
            writePinHigh(RGB_DI_PIN);
            wait_ns(T1H);
            writePinLow(RGB_DI_PIN);
            wait_ns(T1L);
        } else {
            // 0
            writePinHigh(RGB_DI_PIN);
            wait_ns(T0H);
            writePinLow(RGB_DI_PIN);
            wait_ns(T0L);
        }
        byte <<= 1;
    }
}

void ws2812_init(void) { setPinOutput(RGB_DI_PIN); }

// Setleds for standard RGB
void ws2812_setleds(LED_TYPE *ledarray, uint16_t leds) {
    static bool s_init = false;
    if (s_init) {
        ws2812_init();
        s_init = true;
    }

    // this code is very time dependent, so we need to disable interrupts
    chSysLock();

    for (uint8_t i = 0; i < leds; i++) {
        sendByte(ledarray[i].g);
        sendByte(ledarray[i].r);
        sendByte(ledarray[i].b);
    }

    wait_ns(RES);

    chSysUnlock();
}

// Setleds for SK6812RGBW
void ws2812_setleds_rgbw(LED_TYPE *ledarray, uint16_t leds) {
// not supported - for now error out if its enabled
#ifdef RGBW
#    error "RGBW not supported"
#endif
}
