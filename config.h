#ifndef CONFIG_H
#define CONFIG_H

#include "ledcontrol.h"
#include <cstdint>
#include <cstdio>
#include <cmath>

using namespace ledcontrol;

// Adjust according to the number of LEDs you have
const uint NUM_LEDS = 151;

// This should be PicoLed::FORMAT_RGB, PicoLed::FORMAT_GRB or PicoLed::FORMAT_WRGB depending on the LED strip you use
#define LED_FORMAT PicoLed::FORMAT_WRGB
//#define LED_FORMAT PicoLed::FORMAT_GRB

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
    .effect = LEDControl::EFFECT_MODE::HUE_CYCLE,

    // Default encoder mode we start at
    .mode = LEDControl::ENCODER_MODE::OFF,

    // Leave these as-is unless you want funky starting values
    .on = true,
    .stopped = false,
};

// More configuration
const float_t MIN_BRIGHTNESS = 0.02f; // below this there's no meaningful output
const float_t MAX_BRIGHTNESS = 1.0f;
const float_t MIN_SPEED = 0.0f;
const float_t MAX_SPEED = 0.18f; // let's be safe and not trigger anyone

// How many times the LEDs will be updated per second
const uint UPDATES = 60;

// Default brightness for the encoder LED
const float_t ENC_DEFAULT_BRIGHTNESS = 0.5f;

// See connections section in README
const uint LED_DATA_PIN = 28;
const uint BUTTON_B_PIN = 27;
const uint BUTTON_C_PIN = 26;

const uint16_t FADE_IN_DURATION = 1000; // ms
const uint16_t FADE_OUT_DURATION = 2000; // ms
const uint16_t ENCODER_INACTIVITY_TIMEOUT = 10000; // ms. after 10 seconds, encoder will switch to off mode and encoder LED will turn off
const uint16_t GLOBAL_INACTIVITY_TIMEOUT_SECS = 0; // ms. after 1 hour (3600) seconds of inactivity, LEDs will turn off. Set to 0 to disable.
//const uint16_t GLOBAL_INACTIVITY_TIMEOUT_SECS = 3600; // ms. after 1 hour (3600) seconds of inactivity, LEDs will turn off. Set to 0 to disable.

#define ROT_A 16 // rotary A (leftmost pin in rotary)
#define ROT_B 17 // rotary B (rightmost pin in rotary)
#define ROT_LEDR 18 // red LED
#define ROT_LEDG 19 // green LED
#define ROT_SW 20 // rotary pushbutton
#define ROT_LEDB 21 // blue LED

#ifndef RASPBERRYPI_PICO_W
#define LED_PAUSED_PIN PICO_DEFAULT_LED_PIN
#define BOARD_LED_PIN PICO_DEFAULT_LED_PIN
#endif

// A region 1.5MB from the start of flash.
#define FLASH_TARGET_OFFSET (1536 * 1024)

#endif //CONFIG_H
