#include <stdio.h>
#include <math.h>
#include <cstdint>
#include <common/pimoroni_common.hpp>

#include "pico/stdlib.h"

#include "breakout_encoder.hpp"
#include "button.hpp"
#include "PicoLed.hpp"

// Adjust according to the number of LEDs you have
const uint NUM_LEDS = 3;

// Defaults... between 0 and 1.0f
// The speed that the LEDs will start cycling at
const float_t DEFAULT_SPEED = 0.04f;

// The hue that the LEDs will start at. 1.0f = 360 degrees
const float_t DEFAULT_HUE = 0.8f;

// The angle (in degrees) from the hue, that the LEDs will end at (1.0f = 360 degrees)
const float_t DEFAULT_ANGLE = 0.1f;

// The brightness to set the LEDs to. 1.0f = 100%
const float_t DEFAULT_BRIGHTNESS = 0.1f;
const float_t MIN_BRIGHTNESS = 0.02f; // below this there's no meaningful output

// How many times the LEDs will be updated per second
const uint UPDATES = 60;

const float_t ENC_DEFAULT_BRIGHTNESS = 1.0f;

// See connections section in README
const uint LED_DATA_PIN = 28;
const uint BUTTON_A_PIN = 11;
const uint BUTTON_B_PIN = 12;
const uint BUTTON_C_PIN = 13;

// --- Code really starts from here ---

float_t brightness = DEFAULT_BRIGHTNESS;
const float_t BRIGHTNESS_SCALE = 255;

using namespace pimoroni;

auto led_strip = PicoLed::addLeds<PicoLed::WS2812B>(pio0, 0, LED_DATA_PIN, NUM_LEDS, PicoLed::FORMAT_GRB);

Button button_a(BUTTON_A_PIN, Polarity::ACTIVE_LOW, 0);
Button button_b(BUTTON_B_PIN, Polarity::ACTIVE_LOW, 0);
Button button_c(BUTTON_C_PIN, Polarity::ACTIVE_LOW, 0);

I2C i2c(BOARD::BREAKOUT_GARDEN);
BreakoutEncoder enc(&i2c);

enum ENCODER_MODE {
    OFF,
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

  for(auto i = 0u; i < led_strip.getNumLeds(); ++i) {
    float percent_along = (float)i / led_strip.getNumLeds();
    float offset = sinf((percent_along + 0.5f + t) * M_PI) * angle_deg;
    float h = wrap((hue_deg + offset) / 360.0f, 0.0f, 1.0f);
    led_strip.setPixelColor(i, PicoLed::HSV(h*255.0f, 255.0f, 255.0f));
  }
  led_strip.show();
}

ENCODER_MODE encoder_state_by_mode(ENCODER_MODE mode) {
  switch(mode) {
    case ENCODER_MODE::OFF:
      enc.set_led(0, 0, 0);
      break;
    case ENCODER_MODE::COLOUR:
      enc.set_brightness(ENC_DEFAULT_BRIGHTNESS);
      enc.set_led(255, 255, 0); // yellow
      break;
    case ENCODER_MODE::ANGLE:
      enc.set_brightness(ENC_DEFAULT_BRIGHTNESS);
      enc.set_led(255, 128, 0); // orange
      break;
    case ENCODER_MODE::BRIGHTNESS:
      enc.set_brightness(brightness);
      enc.set_led(255, 255, 255); // white
      break;
    case ENCODER_MODE::SPEED:
      enc.set_brightness(ENC_DEFAULT_BRIGHTNESS);
      enc.set_led(255, 0, 0); // red
      break;
  }
  return mode;
}

int main() {
  stdio_init_all();

  //Initialise the default values
  float_t speed = DEFAULT_SPEED;
  float_t hue = DEFAULT_HUE;
  float_t angle = DEFAULT_ANGLE;
  bool cycle = true;

  led_strip.setBrightness((uint8_t)(DEFAULT_BRIGHTNESS*BRIGHTNESS_SCALE));
  led_strip.show();

  bool encoder_detected = enc.init();
  enc.clear_interrupt_flag();

  ENCODER_MODE mode = encoder_state_by_mode(ENCODER_MODE::OFF);

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

        if (mode != ENCODER_MODE::OFF) cycle = false;

        switch(mode) {
          case ENCODER_MODE::OFF:
            break;

          case ENCODER_MODE::COLOUR:
            hue += count;
            hue = wrap(hue, 0.0f, 1.0f);
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
            led_strip.setBrightness((uint8_t)(brightness*BRIGHTNESS_SCALE));
            printf("new brightness: %f\n", brightness);
            colour_cycle(hue, 0, angle);
            enc.set_brightness(brightness);
            break;

          case ENCODER_MODE::SPEED:
            speed += count;
            speed = std::min(1.0f, std::max(0.01f, speed));
            printf("new speed: %f\n", speed);
            cycle = true;
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
      mode = encoder_state_by_mode(ENCODER_MODE::OFF);
      cycle = true;
    }

    if(b_pressed) {
      if(!cycle)
        start_time = millis();
      cycle = true;
    }

    if (a_pressed) {
      switch(mode) {
        case ENCODER_MODE::OFF:
          mode = encoder_state_by_mode(ENCODER_MODE::COLOUR);
          printf("[mode] start colour\n");
          break;

        case ENCODER_MODE::COLOUR:
          mode = encoder_state_by_mode(ENCODER_MODE::ANGLE);
          printf("[mode] end colour\n");
          break;

        case ENCODER_MODE::ANGLE:
          mode = encoder_state_by_mode(ENCODER_MODE::BRIGHTNESS);
          printf("[mode] brightness\n");
          break;

        case ENCODER_MODE::BRIGHTNESS:
          mode = encoder_state_by_mode(ENCODER_MODE::SPEED);
          printf("[mode] speed\n");
          break;

        case ENCODER_MODE::SPEED:
          mode = encoder_state_by_mode(ENCODER_MODE::OFF);
          printf("[mode] off\n");
          break;
      }
    }

    if(cycle) {
//      printf("[cycle] hue: %f, angle: %f, speed: %f, brightness: %f\n", hue, angle, speed, brightness);
      colour_cycle(hue, (float)t * speed, angle);
    }

    // Sleep time controls the rate at which the LED buffer is updated
    // but *not* the actual framerate at which the buffer is sent to the LEDs
    sleep_ms(1000 / UPDATES);
  }
}