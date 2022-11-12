#include "ledcontrol.h"
#include <cstdint>
#include <common/pimoroni_common.hpp>
#include <cstring>

#include "pico/stdlib.h"
#ifdef RASPBERRYPI_PICO_W
#include "pico/cyw43_arch.h"
#endif
#include "hardware/flash.h"
#include "hardware/sync.h"

#include "ledcontrol.h"
#include "util.h"
#include "config.h"

using namespace ledcontrol;

LEDControl::LEDControl():
    state(DEFAULT_STATE),
    encoder_last_blink(0),
    encoder_blink_state(false),
    stop_time(0),
    led_strip(PicoLed::addLeds<PicoLed::WS2812B>(pio0, 0, LED_DATA_PIN, NUM_LEDS, PicoLed::FORMAT_WRGB)),
    button_a(pimoroni::Button(BUTTON_A_PIN, pimoroni::Polarity::ACTIVE_LOW, 0)),
    button_b(pimoroni::Button(BUTTON_B_PIN, pimoroni::Polarity::ACTIVE_LOW, 0)),
    i2c(pimoroni::I2C(pimoroni::BOARD::BREAKOUT_GARDEN)),
    enc(pimoroni::BreakoutEncoder(&i2c))
{
}

void LEDControl::cycle_loop(float hue, float t, float angle) {
  auto hue_deg = hue * 360.0f;
  auto angle_deg = angle * 360.0f;

  t /= 200.0f;

  for(auto i = 0u; i < led_strip.getNumLeds(); ++i) {
    float percent_along = (float)i / led_strip.getNumLeds();
    float offset = sinf((percent_along + 0.5f + t) * M_PI) * angle_deg;
    float h = wrap((hue_deg + offset) / 360.0f, 0.0f, 1.0f);

    switch(state.effect) {
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

uint16_t LEDControl::get_paused_time() {
  return cycle ? 0 : pimoroni::millis() - stop_time;
}

void LEDControl::set_cycle(bool v) {
  if (cycle == v) return;

  // don't set `cycle` before calling get_paused_time below, or we'll get a false reading
  if (!v) {
    stop_time = pimoroni::millis();
  } else {
    // adjust start time to account for time spent paused
    start_time += get_paused_time();
  }

  cycle = v;

#ifdef LED_PAUSED_PIN
  gpio_put(LED_PAUSED_PIN, !cycle);
#endif
#ifdef RASPBERRYPI_PICO_W
  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, !cycle);
#endif
}

uint32_t LEDControl::encoder_colour_by_mode(LEDControl::ENCODER_MODE mode) {
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

void LEDControl::encoder_loop(LEDControl::ENCODER_MODE mode) {
  const uint32_t encoder_blink_interval = 500;
  uint32_t ts = pimoroni::millis();
  if (encoder_last_blink == 0 || (encoder_last_blink + encoder_blink_interval)<ts) {
    encoder_last_blink = ts;
    encoder_blink_state = !encoder_blink_state;
    encoder_colour_by_mode(mode);
  }
}

LEDControl::ENCODER_MODE LEDControl::encoder_state_by_mode(ENCODER_MODE mode) {
  enc.set_brightness(mode == ENCODER_MODE::BRIGHTNESS ? state.brightness : ENC_DEFAULT_BRIGHTNESS);
  encoder_colour_by_mode(mode);
  return mode;
}

void LEDControl::init() {
#ifdef LED_PAUSED_PIN
  gpio_init(LED_PAUSED_PIN);
  gpio_set_dir(LED_PAUSED_PIN, GPIO_OUT);
  gpio_put(LED_PAUSED_PIN, false);
#endif
#ifdef RASPBERRYPI_PICO_W
  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
#endif

  encoder_detected = enc.init();
  enc.clear_interrupt_flag();

  if (load_state_from_flash() != 0) {
    printf("failed to load state from flash, using defaults\n");
    enable_state(DEFAULT_STATE);
  }

  menu_mode = MENU_SELECT;

  start_time = pimoroni::millis();
  set_cycle(true);
}

void LEDControl::enable_state(state_t p_state) {
  printf("[enable_state] hue: %f, angle: %f, speed: %f, brightness: %f, mode:%d, effect:%d\n", state.hue, state.angle, state.speed, state.brightness, state.mode, state.effect);

  state = p_state;
  led_strip.setBrightness((uint8_t)(state.brightness*BRIGHTNESS_SCALE));
  led_strip.show();
  encoder_state_by_mode(state.mode);
}

LEDControl::state_t LEDControl::get_state() {
  return state;
}

int LEDControl::load_state_from_flash() {
  auto *flash_target_contents = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET);

//  print_buf(flash_target_contents, 256);

  uint8_t buffer[256];
  memcpy(buffer, flash_target_contents, sizeof(buffer));

  auto *flash_state = (flash_state_t *)buffer;
  if (memcmp(flash_state->magic, flash_save_magic, strlen(flash_save_magic)) != 0) {
    printf("load_state_from_flash: invalid state magic\n");
    return -1;
  }
  if (flash_state->state_size != sizeof(state_t)) {
    printf("load_state_from_flash: invalid state_t size\n");
    return -2;
  }
  enable_state(flash_state->state);

  return 0;
}

int LEDControl::save_state_to_flash() {
  printf("save_state_to_flash: start\n");

  // prepare the buffer
  uint8_t buffer[256];
  memset(buffer, 0xcc, sizeof(buffer));

  flash_state_t fs = {
    .state_size = sizeof(state_t),
    .state = state,
  };
  memcpy(fs.magic, flash_save_magic, strlen(flash_save_magic));
  memcpy(buffer, &fs, sizeof(flash_state_t));

//  print_buf(buffer, sizeof(buffer));

  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(FLASH_TARGET_OFFSET, sizeof(buffer));
  flash_range_program(FLASH_TARGET_OFFSET, buffer, sizeof(buffer));
  restore_interrupts(ints);

  printf("save_state_to_flash: success\n");

//  LEDControl::load_state_from_flash();
  return 0;
}

uint32_t LEDControl::loop() {
  uint32_t t = pimoroni::millis() - start_time;
  if(encoder_detected && enc.get_interrupt_flag()) {
    int16_t count_raw = enc.read(); // Looks like -64 to +64, but we assume -10 to +10
    float_t count = std::min(10.0f, std::max(-10.0f, (float_t)count_raw))/50.0f; // Max increase can be 20% per update
    printf("[encoder] count: %d (%f)\n", count_raw, count);
    enc.clear_interrupt_flag();
    enc.clear();

    switch(menu_mode) {
      default:
      case MENU_MODE::MENU_SELECT:
        state.mode = (ENCODER_MODE)limiting_wrap(state.mode + (count < 0.0f ? -1 : 1), 0, ENCODER_MODE::MODE_COUNT);
        printf("[mode] new mode: %d\n", state.mode);
        encoder_state_by_mode(state.mode);
        break;
      case MENU_MODE::MENU_ADJUST:
        if (state.mode == ENCODER_MODE::OFF) break;

        set_cycle(state.mode == ENCODER_MODE::SPEED);

        switch (state.mode) {
          default:
          case ENCODER_MODE::OFF:
            break;

          case ENCODER_MODE::COLOUR:
            state.hue = wrap(state.hue + count, 0.0f, 1.0f);
            printf("new hue start angle: %f\n", state.hue);
            break;

          case ENCODER_MODE::ANGLE:
            state.angle = std::min(1.0f, std::max(0.0f, state.angle + count));
            printf("new hue end angle: %f\n", state.angle);
            break;

          case ENCODER_MODE::BRIGHTNESS:
            state.brightness = std::min(1.0f, std::max(MIN_BRIGHTNESS, state.brightness + count));
            printf("new brightness: %f\n", state.brightness);
            led_strip.setBrightness((uint8_t) (state.brightness * BRIGHTNESS_SCALE));
            enc.set_brightness(state.brightness);
            led_strip.show();
            break;

          case ENCODER_MODE::SPEED:
            state.speed = std::min(1.0f, std::max(0.01f, state.speed + count));
            printf("new speed: %f\n", state.speed);
            break;

          case ENCODER_MODE::EFFECT:
            state.effect = (EFFECT_MODE)limiting_wrap(state.effect + (count < 0.0 ? -1 : 1), 0, EFFECT_MODE::EFFECT_COUNT);

            printf("new effect: %d\n", state.effect);
            break;
        }

        switch (state.mode) {
          default:
            break;
          case ENCODER_MODE::COLOUR:
          case ENCODER_MODE::ANGLE:
          case ENCODER_MODE::EFFECT:
            cycle_loop(state.hue, (float) (t - get_paused_time()) * state.speed, state.angle);
            break;
        }
    }
  }
  auto a_val = wait_for_long_button(button_a, 1500);
  bool a_pressed = a_val == 1;
  bool a_held = a_val == 2;

  auto b_val = wait_for_long_button(button_b, 1500);
  bool b_pressed = b_val == 1;
  bool b_held = b_val == 2;

  if (a_pressed || a_held) {
    printf("[button] A pressed:%d held:%d\n", a_pressed, a_held);
  }
  if (b_pressed || b_held) {
    printf("[button] B pressed:%d held:%d\n", b_pressed, b_held);
  }

  if(b_held) {
    led_strip.setBrightness(0);
    led_strip.show();
    printf("B held\n");
    sleep_ms(1500);
    save_state_to_flash();

    led_strip.setBrightness((uint8_t)(state.brightness*BRIGHTNESS_SCALE));
    led_strip.show();
  }

  if(b_pressed) {
    printf("B pressed! saved state or defaults\n");
    enable_state(DEFAULT_STATE);
    menu_mode = MENU_MODE::MENU_SELECT;
    set_cycle(true);
  }

  if (a_pressed) {
    if (state.mode == ENCODER_MODE::OFF) { // If we're off, switch to first mode
      state.mode = ENCODER_MODE::COLOUR;
      encoder_state_by_mode(state.mode);
    } else {
      menu_mode = (MENU_MODE)(((int) menu_mode + 1) % (int) MENU_MODE::MENU_COUNT);
      printf("[menu] new menu selection: %d\n", menu_mode);
      encoder_state_by_mode(state.mode);
      if (!cycle) {
        set_cycle(true);
        t = pimoroni::millis() - start_time;
        printf("[cycle] hue: %f, angle: %f, speed: %f, brightness: %f, mode:%d, effect:%d\n", state.hue, state.angle, state.speed, state.brightness, state.mode, state.effect);
      }
    }
  }

  if (cycle) cycle_loop(state.hue, (float) (t - get_paused_time()) * state.speed, state.angle);
  if (menu_mode == MENU_MODE::MENU_ADJUST) encoder_loop(state.mode);

  // Sleep time controls the rate at which the LED buffer is updated
  // but *not* the actual framerate at which the buffer is sent to the LEDs
  return 1000 / UPDATES;
}
