#include <stdio.h>
#include <math.h>
#include <cstdint>
#include <common/pimoroni_common.hpp>

#include "pico/stdlib.h"

#include "breakout_encoder.hpp"
#include "button.hpp"
#include "ws2812.hpp"

/*
 * Connections
 *
 * WS2812: data to GP28.
 * Encoder: GP4,GP5 for I2C(SDA,SCL) and GP3 for interrupt.
 *
 * Button "A" to GP11. Changes encoder mode.
 * Button "B" to GP12. Enables cycling.
 * Button "C" to GP13. Resets effects.
 *
 * Buttons are active-low.
 */

const uint LED_DATA_PIN = 28;
const uint BUTTON_A_PIN = 11;
const uint BUTTON_B_PIN = 12;
const uint BUTTON_C_PIN = 13;
const uint NUM_LEDS = 3;

using namespace pimoroni;
using namespace plasma;

// Defaults... between 0 and 1.0f
// The speed that the LEDs will start cycling at
const float_t DEFAULT_SPEED = 0.04f;

// The hue that the LEDs will start at. 1.0f = 360 degrees
const float_t DEFAULT_HUE = 0.8f;

// The angle (in degrees) from the hue, that the LEDs will end at (1.0f = 360 degrees)
const float_t DEFAULT_ANGLE = 0.7f;

// The brightness to set the LEDs to. 1.0f = 100%
const float_t DEFAULT_BRIGHTNESS = 0.25f;
const float_t MIN_BRIGHTNESS = 0.15f; // below this there's no meaningful output

// How many times the LEDs will be updated per second
const uint UPDATES = 60;

const float_t ENC_DEFAULT_BRIGHTNESS = 1.0f;

float_t brightness = DEFAULT_BRIGHTNESS;
const bool brightness_as_value = true;

// WS28X-style LEDs with a single signal line. AKA NeoPixel
WS2812 led_strip(NUM_LEDS, pio0, 0, LED_DATA_PIN);


Button button_a(BUTTON_A_PIN, Polarity::ACTIVE_LOW, 0);
Button button_b(BUTTON_B_PIN, Polarity::ACTIVE_LOW, 0);
Button button_c(BUTTON_C_PIN, Polarity::ACTIVE_LOW, 0);

I2C i2c(BOARD::BREAKOUT_GARDEN);
BreakoutEncoder enc(&i2c);

enum ENCODER_MODE {
    COLOUR,
    ANGLE,
    BRIGHTNESS,
    SPEED
};

float wrap(float v, float min, float max) {
  if(v <= min)
    v += (max - min);

  if(v > max)
    v -= (max - min);

  return v;
}


void colour_cycle(float hue, float t, float angle) {
  auto hue_deg = hue * 360.0f;
  auto angle_deg = angle * 360.0f;

  t /= 200.0f;

  for(auto i = 0u; i < led_strip.num_leds; ++i) {
    float percent_along = (float)i / led_strip.num_leds;
    float offset = sinf((percent_along + 0.5f + t) * M_PI) * angle_deg;
    float h = wrap((hue_deg + offset) / 360.0f, 0.0f, 1.0f);
    if (brightness_as_value) {
      led_strip.set_hsv(i, h, 1.0f, brightness);
    } else {
      led_strip.set_hsv(i, h, 1.0f, 1.0f);
    }
  }
}

void speed_gauge(float_t v) {
  uint light_pixels = uint((float_t)led_strip.num_leds * v);

  for(auto i = 0u; i < led_strip.num_leds; ++i) {
    if(i < light_pixels) {
      led_strip.set_rgb(i, 64, 64, 64);
    } else {
      led_strip.set_rgb(i, 0, 0, 0);
    }
  }
}

void brightness_gauge(float_t v) {
  uint light_pixels = uint((float_t)led_strip.num_leds * v);

  for(auto i = 0u; i < led_strip.num_leds; ++i) {
    if(i < light_pixels) {
      led_strip.set_rgb(i, 64, 64, 64);
    }
    else {
      led_strip.set_rgb(i, 0, 0, 0);
    }
  }
}

int main() {
  stdio_init_all();

  led_strip.start(UPDATES);

  bool encoder_detected = enc.init();
  enc.clear_interrupt_flag();

  //Initialise the default values
  float_t speed = DEFAULT_SPEED;
  float_t hue = DEFAULT_HUE;
  float_t angle = DEFAULT_ANGLE;

  bool cycle = true;
  ENCODER_MODE mode = ENCODER_MODE::COLOUR;
  enc.set_led(0, 0, 255);

  uint32_t start_time = millis();
  while(true) {
    uint32_t t = millis() - start_time;
    if(encoder_detected) {
      if(enc.get_interrupt_flag()) {
        int16_t count_raw = enc.read(); // Looks like -64 to +64, but we assume -10 to +10
        float_t count = std::min(10.0f, std::max(-10.0f, (float_t)count_raw))/50.0f; // Max increase can be 20% per update
        printf("[encoder] count: %d (%f)\n", count_raw, count);
        enc.clear_interrupt_flag();
        enc.clear();

        cycle = false;
        float hue_angle;
        switch(mode) {
          case ENCODER_MODE::COLOUR:
            hue += count;
            hue = std::min(1.0f, std::max(0.0f, hue));
            colour_cycle(hue, 0, angle);
            printf("new hue start angle: %f\n", hue);
            break;

          case ENCODER_MODE::ANGLE:
            angle += count;
            angle = std::min(1.0f, std::max(0.0f, angle));
            printf("new hue end angle: %f\n", angle);
            colour_cycle(hue, 0, angle);
            break;

          case ENCODER_MODE::BRIGHTNESS:
            brightness += count;
            brightness = std::min(1.0f, std::max(MIN_BRIGHTNESS, brightness));
//            led_strip.set_brightness((uint8_t)(brightness*31.0f));
            printf("new brightness: %f\n", brightness);
            colour_cycle(hue, 0, angle);
//            brightness_gauge(brightness);
            enc.set_brightness(brightness);
            break;

          case ENCODER_MODE::SPEED:
            speed += count;
            speed = std::min(1.0f, std::max(0.01f, speed));
            printf("new speed: %f\n", speed);
            speed_gauge(speed);
            break;
        }
      }
    }
    bool a_pressed = button_a.read();
    bool b_pressed = button_b.read();
    bool c_pressed = button_c.read();

    if(c_pressed) {
      printf("C pressed! defaults\n");
      speed = DEFAULT_SPEED;
      hue = DEFAULT_HUE;
      angle = DEFAULT_ANGLE;
      brightness = DEFAULT_BRIGHTNESS;
      cycle = true;
    }

    if(b_pressed) {
      if(!cycle)
        start_time = millis();
      cycle = true;
    }

    if (a_pressed) {
      enc.set_brightness(ENC_DEFAULT_BRIGHTNESS);

      switch(mode) {
        case ENCODER_MODE::COLOUR:
          enc.set_led(255, 0, 0);
          mode = ENCODER_MODE::ANGLE;
          printf("[mode] colour end\n");
          break;

        case ENCODER_MODE::ANGLE:
          enc.set_led(255, 255, 0);
          mode = ENCODER_MODE::BRIGHTNESS;
          printf("[mode] brightness\n");
          break;

        case ENCODER_MODE::BRIGHTNESS:
          enc.set_led(0, 255, 0);
          mode = ENCODER_MODE::SPEED;
          printf("[mode] speed\n");
          break;

        case ENCODER_MODE::SPEED:
          enc.set_led(0, 0, 255);
          mode = ENCODER_MODE::COLOUR;
          printf("[mode] colour start\n");
          break;
      }
    }

    if(cycle) {
//      printf("[cycle] hue: %f, angle: %f, speed: %f, brightness: %f\n", hue, angle, speed, brightness);
      colour_cycle(hue, (float)t * speed, angle);
    }

//    auto mid_led = led_strip.get(led_strip.num_leds / 2);
//    enc.set_led(mid_led.r, mid_led.g, mid_led.b);

    // Sleep time controls the rate at which the LED buffer is updated
    // but *not* the actual framerate at which the buffer is sent to the LEDs
    sleep_ms(1000 / UPDATES);
  }
}