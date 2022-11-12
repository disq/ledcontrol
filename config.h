#ifndef CONFIG_H
#define CONFIG_H

#include "ledcontrol.h"
#include <cstdint>
#include <cstdio>
#include <cmath>

using namespace ledcontrol;

// Adjust according to the number of LEDs you have
const uint NUM_LEDS = 151;

// Defaults... between 0 and 1.0f
// The speed that the LEDs will start cycling at
const float_t DEFAULT_SPEED = 0.04f;

// The hue that the LEDs will start at. 1.0f = 360 degrees
const float_t DEFAULT_HUE = 0.56f;

// The angle (in degrees) from the hue, that the LEDs will end at (1.0f = 360 degrees)
const float_t DEFAULT_ANGLE = 0.68f;

// The brightness to set the LEDs to. 1.0f = 100%
const float_t DEFAULT_BRIGHTNESS = 0.50f;
const float_t MIN_BRIGHTNESS = 0.02f; // below this there's no meaningful output

// Default effect we start at
const enum LEDControl::EFFECT_MODE DEFAULT_EFFECT = LEDControl::HUE_CYCLE;

// How many times the LEDs will be updated per second
const uint UPDATES = 60;

// Default brightness for the encoder LED
const float_t ENC_DEFAULT_BRIGHTNESS = 1.0f;

// See connections section in README
const uint LED_DATA_PIN = 28;
const uint BUTTON_A_PIN = 11;
const uint BUTTON_B_PIN = 12;

#ifndef RASPBERRYPI_PICO_W
#define LED_PAUSED_PIN PICO_DEFAULT_LED_PIN
#endif

#endif //CONFIG_H
