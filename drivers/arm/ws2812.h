#pragma once

#include "quantum/color.h"

/* User Interface
 *
 * Input:
 *         ledarray:           An array of GRB data describing the LED colors
 *         number_of_leds:     The number of LEDs to write
 *         pinmask (optional): Bitmask describing the output bin. e.g. _BV(PB0)
 *
 * The functions will perform the following actions:
 *         - Set the data-out pin as output
 *         - Send out the LED data
 *         - Wait 50�s to reset the LEDs
 */
#ifdef RGB_MATRIX_ENABLE
void ws2812_setled(int index, uint8_t r, uint8_t g, uint8_t b);
void ws2812_setled_all(uint8_t r, uint8_t g, uint8_t b);
#endif

void ws2812_setleds(LED_TYPE *ledarray, uint16_t number_of_leds);
void ws2812_setleds_rgbw(LED_TYPE *ledarray, uint16_t number_of_leds);
