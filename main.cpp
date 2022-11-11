#include <stdio.h>
#include <math.h>
#include <cstdint>
#include <common/pimoroni_common.hpp>

#include "pico/stdlib.h"
#ifdef RASPBERRYPI_PICO_W
#include "pico/cyw43_arch.h"
#endif

#include "breakout_encoder.hpp"
#include "button.hpp"
#include "PicoLed.hpp"

// Adjust according to the number of LEDs you have
const uint NUM_LEDS = 151;

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

#ifndef RASPBERRYPI_PICO_W
#define LED_PAUSED_PIN PICO_DEFAULT_LED_PIN
#endif

// --- Code really starts from here ---

float_t brightness = DEFAULT_BRIGHTNESS;
const float_t BRIGHTNESS_SCALE = 255;
bool cycle;
uint32_t encoder_last_blink = 0;
bool encoder_blink_state = false;
uint8_t effect_mode;

using namespace pimoroni;

auto led_strip = PicoLed::addLeds<PicoLed::WS2812B>(pio0, 0, LED_DATA_PIN, NUM_LEDS, PicoLed::FORMAT_WRGB);

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
    SPEED,
    EFFECT,

    MODE_COUNT
};

enum EFFECT_MODE {
    HUE_CYCLE,
    WHITE_CHASE,

    EFFECT_COUNT,
};

const enum EFFECT_MODE DEFAULT_EFFECT = HUE_CYCLE;

float wrap(float v, float min, float max) {
  if(v <= min)
    v += (max - min);

  if(v > max)
    v -= (max - min);

  return v;
}


void effect_cycle(float hue, float t, float angle) {
  auto hue_deg = hue * 360.0f;
  auto angle_deg = angle * 360.0f;

  t /= 200.0f;

  for(auto i = 0u; i < led_strip.getNumLeds(); ++i) {
    float percent_along = (float)i / led_strip.getNumLeds();
    float offset = sinf((percent_along + 0.5f + t) * M_PI) * angle_deg;
    float h = wrap((hue_deg + offset) / 360.0f, 0.0f, 1.0f);

    switch(effect_mode) {
      case EFFECT_MODE::HUE_CYCLE:
      default:
        led_strip.setPixelColor(i, PicoLed::HSV(uint8_t(h*255.0f), 255.0f, 255.0f));
        break;
      case EFFECT_MODE::WHITE_CHASE:
        led_strip.setPixelColor(i, PicoLed::RGBW(0, 0, 0, uint8_t((1.0f - h)*255.0f)));
        break;
    }
  }
  led_strip.show();
}

void set_cycle(bool v) {
  cycle = v;
#ifdef LED_PAUSED_PIN
  gpio_put(LED_PAUSED_PIN, !cycle);
#endif
#ifdef RASPBERRYPI_PICO_W
  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, !cycle);
#endif
}

uint32_t encoder_colour_by_mode(ENCODER_MODE mode) {
  uint32_t col;
  switch(mode) {
    case ENCODER_MODE::OFF:
    default:
      col = 0;
      break;
    case ENCODER_MODE::COLOUR:
      col = 0xFFFF00; // yellow
      break;
    case ENCODER_MODE::ANGLE:
      col = 0xFF8000; // orange
      break;
    case ENCODER_MODE::BRIGHTNESS:
      col = 0xFFFFFF; // white
      break;
    case ENCODER_MODE::SPEED:
      col = 0xFF0000; // red
      break;
    case ENCODER_MODE::EFFECT:
      col = 0xFF00FF; // purple
      break;
  }
  uint8_t col_r = (col >> 16) & 0xFF;
  uint8_t col_g = (col >> 8) & 0xFF;
  uint8_t col_b = col & 0xFF;
  if (encoder_blink_state) {
    col_r = col_r >> 1;
    col_g = col_g >> 1;
    col_b = col_b >> 1;
  }
  enc.set_led(col_r, col_g, col_b);
  return col;
}

void encoder_loop(ENCODER_MODE mode) {
  const uint32_t encoder_blink_interval = 500;
  uint32_t ts = millis();
  if (encoder_last_blink == 0 || (encoder_last_blink + encoder_blink_interval)<ts) {
    encoder_last_blink = ts;
    encoder_blink_state = !encoder_blink_state;
    encoder_colour_by_mode(mode);
  }
}

ENCODER_MODE encoder_state_by_mode(ENCODER_MODE mode) {
  enc.set_brightness(mode == ENCODER_MODE::BRIGHTNESS ? brightness : ENC_DEFAULT_BRIGHTNESS);
  encoder_colour_by_mode(mode);
  return mode;
}

int main() {
  stdio_init_all();

#ifdef LED_PAUSED_PIN
  gpio_init(LED_PAUSED_PIN);
  gpio_set_dir(LED_PAUSED_PIN, GPIO_OUT);
  gpio_put(LED_PAUSED_PIN, false);
#endif
#ifdef RASPBERRYPI_PICO_W
  if (cyw43_arch_init()) {
      printf("WiFi init failed");
      return -1;
    }
  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
#endif

  //Initialise the default values
  float_t speed = DEFAULT_SPEED;
  float_t hue = DEFAULT_HUE;
  float_t angle = DEFAULT_ANGLE;
  effect_mode = DEFAULT_EFFECT;

  set_cycle(true);

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

        switch(mode) {
          case ENCODER_MODE::OFF:
            break;

          case ENCODER_MODE::COLOUR:
            hue += count;
            hue = wrap(hue, 0.0f, 1.0f);
            printf("new hue start angle: %f\n", hue);
            effect_cycle(hue, 0, angle);
            set_cycle(false);
            break;

          case ENCODER_MODE::ANGLE:
            angle += count;
            angle = std::min(1.0f, std::max(0.0f, angle));
            printf("new hue end angle: %f\n", angle);
            effect_cycle(hue, 0, angle);
            set_cycle(false);
            break;

          case ENCODER_MODE::BRIGHTNESS:
            brightness += count;
            brightness = std::min(1.0f, std::max(MIN_BRIGHTNESS, brightness));
            printf("new brightness: %f\n", brightness);
            led_strip.setBrightness((uint8_t)(brightness*BRIGHTNESS_SCALE));
            enc.set_brightness(brightness);
            set_cycle(true);
            break;

          case ENCODER_MODE::SPEED:
            speed += count;
            speed = std::min(1.0f, std::max(0.01f, speed));
            printf("new speed: %f\n", speed);
            set_cycle(true);
            break;

          case ENCODER_MODE::EFFECT:
            effect_mode = (EFFECT_MODE)(((int)effect_mode + 1) % (int)EFFECT_MODE::EFFECT_COUNT);
            printf("new effect: %d\n", effect_mode);
            effect_cycle(hue, 0, angle);
            set_cycle(false);
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
      effect_mode = DEFAULT_EFFECT;
      led_strip.setBrightness((uint8_t)(brightness*BRIGHTNESS_SCALE));
      mode = encoder_state_by_mode(ENCODER_MODE::OFF);
      set_cycle(true);
    }

    if(b_pressed) {
      if(!cycle)
        start_time = millis();
      set_cycle(true);
    }

    if (a_pressed) {
      mode = (ENCODER_MODE)(((int)mode + 1) % (int)ENCODER_MODE::MODE_COUNT);
      printf("[mode] new mode: %d\n", mode);
      encoder_state_by_mode(mode);
    }

    if(cycle) {
//      printf("[cycle] hue: %f, angle: %f, speed: %f, brightness: %f\n", hue, angle, speed, brightness);
      effect_cycle(hue, (float) t * speed, angle);
    }
    if (!cycle && mode != ENCODER_MODE::OFF) encoder_loop(mode);

    // Sleep time controls the rate at which the LED buffer is updated
    // but *not* the actual framerate at which the buffer is sent to the LEDs
    sleep_ms(1000 / UPDATES);
  }
}