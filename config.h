#ifndef CONFIG_H
#define CONFIG_H

#include "ledcontrol.h"
#include <cstdint>
#include <cstdio>
#include <cmath>

using namespace ledcontrol;

// Adjust according to the number of LEDs you have
const uint NUM_LEDS = 151;

// Defaults... floats are between 0 and 1.0f
const LEDControl::state_t DEFAULT_STATE = {
    // Hue the LEDs will start at. 1.0f = 360 degrees
    .hue = 0.56f,

    // Angle (in degrees) from the hue, the LEDs will end at (1.0f = 360 degrees)
    .angle = 0.68f,

    // Speed the LEDs will start cycling at
    .speed = 0.04f,

    // Brightness to set the LEDs to. 1.0f = 100%
    .brightness = 0.5f,

    // Default effect we start at
    .effect_mode = LEDControl::EFFECT_MODE::HUE_CYCLE,

    // Default encoder mode we start at
    .mode = LEDControl::ENCODER_MODE::OFF,
};

// More configuration
const float_t MIN_BRIGHTNESS = 0.02f; // below this there's no meaningful output
// TODO add MAX_BRIGHTNESS and MAX_SPEED

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
