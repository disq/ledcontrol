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

#include "util.h"
#include "config.h"

using namespace ledcontrol;

LEDControl::LEDControl():
    state(DEFAULT_STATE),
    encoder_last_blink(0),
    encoder_blink_state(false),
    encoder_last_activity(0),
    global_last_activity(0),
    start_time(0),
    stop_time(0),
    transition_start_time(0),
    transition_duration(0),
    transition_start_brightness(0),
    transition_target_brightness(1.0f),
    led_strip(NUM_LEDS, pio0, 0, LED_DATA_PIN, plasma::WS2812::DEFAULT_SERIAL_FREQ, LED_RGBW, LED_ORDER),
    button_b(pimoroni::Button(BUTTON_B_PIN, pimoroni::Polarity::ACTIVE_LOW, 0)),
    button_c(pimoroni::Button(BUTTON_C_PIN, pimoroni::Polarity::ACTIVE_LOW, 0)),
    cycle_once(false),
    _on_state_change_cb(NULL)
{
  state.on = false; // so that we can turn it on with a transition
}

void LEDControl::set_brightness(float_t brightness) {
  eff_brightness = brightness;
  cycle_loop(0, 0, 0, true);
}

// call led_strip.update() after this?
void LEDControl::cycle_loop(float hue, float t, float angle, bool refresh) {
  static float old_hue = 0.0f, old_t = 0.0f, old_angle = 0.0f;
  if (refresh) {
    hue = old_hue;
    t = old_t;
    angle = old_angle;
  } else {
    old_hue = hue;
    old_t = t;
    old_angle = angle;
  }

  auto hue_deg = hue * 360.0f;
  auto angle_deg = angle * 360.0f;

  t /= 200.0f;

  for(auto i = 0u; i < led_strip.num_leds; ++i) {
    float percent_along = (float)i / (float)led_strip.num_leds;
    float offset = sinf((percent_along + 0.5f + t) * M_PI) * angle_deg;
    float h = wrap((hue_deg + offset) / 360.0f, 0.0f, 1.0f);
    uint8_t white;

    switch(state.effect) {
      case EFFECT_MODE::HUE_CYCLE:
      default:
        led_strip.set_hsv(i, h, 1.0f, eff_brightness);
        break;
      case EFFECT_MODE::WHITE_CHASE:
        white = uint8_t((1.0f - h) * eff_brightness * 255.0f);
        if (LED_RGBW) {
          led_strip.set_rgb(i, 0, 0, 0, white);
        } else {
          led_strip.set_rgb(i, white, white, white);
        }
        break;
    }
  }
}

const char* LEDControl::effect_to_str(EFFECT_MODE effect) {
  return effect < EFFECT_COUNT ? effect_str[effect] : "";
}

const char* LEDControl::speed_to_str(float_t speed) {
  if (speed == 0.0f) return speed_str[0];
  if (speed < 0.02f) return speed_str[1];
  if (speed < 0.06f) return speed_str[2];
  if (speed < 0.11f) return speed_str[3];
  return speed_str[SPEED_COUNT-1];
}

float_t LEDControl::str_to_speed(const char *str) {
  if (strcmp(str, speed_str[0]) == 0) {
    return 0.0f;
  } else if (strcmp(str, speed_str[1]) == 0) {
    return 0.01f;
  } else if (strcmp(str, speed_str[2]) == 0) {
    return 0.05f;
  } else if (strcmp(str, speed_str[3]) == 0) {
    return 0.10f;
  } else if (strcmp(str, speed_str[4]) == 0) {
    return MAX_SPEED;
  } else {
    return DEFAULT_STATE.speed;
  }
}

int LEDControl::parse_effect_str(const char *str, EFFECT_MODE *effect, float_t *speed) {
  bool found = false;
  for(uint8_t i = 0; i < EFFECT_COUNT; i++) {
    if (strstarts(str, effect_str[i])) {
      *effect = (EFFECT_MODE)i;
      found = true;
      break;
    }
  }
  if (!found) {
    return -1;
  }

  size_t remaining_pos = strlen(effect_to_str(*effect));
  if (str[remaining_pos] == ':') {
    *speed = str_to_speed(str + remaining_pos + 1);
    return 1;
  }
  return 0;
}

size_t LEDControl::get_effect_list(LEDControl::EFFECT_MODE *effects, size_t num_effects) {
  size_t limit = 0;
  effects[limit++] = EFFECT_MODE::HUE_CYCLE;
  if (limit >= num_effects) return limit;
  effects[limit++] = EFFECT_MODE::WHITE_CHASE;
  return limit;
}

size_t LEDControl::get_speed_list(const char **speeds, size_t num_speeds) {
  size_t limit = 0;
  for(int i = 0; i < SPEED_COUNT; i++) {
    speeds[limit++] = speed_str[i];
    if (limit >= num_speeds) return limit;
  }
  return limit;
}

uint16_t LEDControl::get_paused_time() {
  return cycle ? 0 : millis() - stop_time;
}

void LEDControl::set_cycle(bool v) {
  if (cycle == v) return;

  // don't set `cycle` before calling get_paused_time below, or we'll get a false reading
  if (!v) {
    stop_time = millis();
  } else {
    // adjust start time to account for time spent paused
    start_time += get_paused_time();
  }

  cycle = v;
  if (state.stopped) return; // don't update LEDs if stopped on command

#ifdef LED_PAUSED_PIN
  gpio_put(LED_PAUSED_PIN, !cycle);
#endif
#ifdef RASPBERRYPI_PICO_W
  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, !cycle);
#endif
}

void LEDControl::encoder_blink_off() {
  if (!encoder_blink_state) return;
  encoder_blink_state = false;
  set_encoder_state();
}

uint32_t LEDControl::encoder_colour_by_mode(LEDControl::ENCODER_MODE mode) {
  uint32_t col;
  switch(mode) {
    case ENCODER_MODE::OFF:
    default:
      col = 0;
      break;
    case ENCODER_MODE::COLOUR:
      col = 0x808000; // dim yellow
      break;
    case ENCODER_MODE::ANGLE:
      col = 0x804000; // dim orange
      break;
    case ENCODER_MODE::BRIGHTNESS:
      col = 0x606060; // dim white
      break;
    case ENCODER_MODE::SPEED:
      col = 0x400000; // dim red
      break;
    case ENCODER_MODE::EFFECT:
      col = 0x400040; // dim purple
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
  enc->set_leds(col_r, col_g, col_b);
  return col;
}

void LEDControl::encoder_loop() {
  const uint32_t encoder_blink_interval = 500;
  uint32_t ts = millis();
  if (encoder_last_blink == 0 || (encoder_last_blink + encoder_blink_interval)<ts) {
    encoder_last_blink = ts;
    encoder_blink_state = !encoder_blink_state;
    encoder_colour_by_mode(state.mode);
  }
}

void LEDControl::set_encoder_state() {
  encoder_colour_by_mode(state.mode);
  enc->set_brightness(state.mode == ENCODER_MODE::BRIGHTNESS ? state.brightness : ENC_DEFAULT_BRIGHTNESS);
}

void LEDControl::init(Encoder *e) {
  enc = e;
  enc->init(ROT_LEDR, ROT_LEDG, ROT_LEDB, ROT_A, ROT_B, ROT_SW, true);

#ifdef LED_PAUSED_PIN
  gpio_init(LED_PAUSED_PIN);
  gpio_set_dir(LED_PAUSED_PIN, GPIO_OUT);
  gpio_put(LED_PAUSED_PIN, false);
#endif
#ifdef RASPBERRYPI_PICO_W
  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
#endif

  // since we call led_strip.update every time we need an update, we don't need to call led_strip.start to start the update timer
  // led_strip.start(UPDATES);

  if (load_state_from_flash() != 0) {
    printf("failed to load state from flash, using defaults\n");
    enable_state(DEFAULT_STATE);
  }

  menu_mode = MENU_SELECT;

  start_time = millis();
  set_cycle(true);
}

float_t LEDControl::get_effective_brightness() {
  if (transition_start_time == 0) {
    transition_start_brightness = state.on ? state.brightness : 0;
    return transition_start_brightness;
  }

  auto ts = millis();
  if (ts < transition_start_time || ts > transition_start_time+transition_duration) {
    transition_start_time = 0;
    return get_effective_brightness();
  }

  // sinusoidal transition
  float_t t = (float_t)(ts-transition_start_time) / (float_t)transition_duration;
  float_t b = (1.0f-cosf(t*M_PI))/2.0f;
  return (transition_start_brightness + b*(transition_target_brightness-transition_start_brightness));
}

void LEDControl::enable_state(state_t p_state) {
  // clamp in case we loaded from flash or iot
  p_state.hue = std::min(1.0f, std::max(0.0f, p_state.hue));
  p_state.angle = std::min(1.0f, std::max(0.0f, p_state.angle));
  p_state.speed = std::min(MAX_SPEED, std::max(MIN_SPEED, p_state.speed));
  p_state.brightness = std::min(MAX_BRIGHTNESS, std::max(MIN_BRIGHTNESS, p_state.brightness));
  if (p_state.effect < 0 || p_state.effect >= EFFECT_COUNT) p_state.effect = DEFAULT_STATE.effect;

  bool change_cycle = false;
  if (p_state.stopped) p_state.speed = 0.0f;
  if (p_state.speed == 0.0f) {
    if (state.speed != 0.0f) {
      printf("[enable_state] enabling stopped mode\n");
      p_state.speed = state.speed; // keep it the same
      change_cycle = true;
    } else {
      printf("[enable_state] already in stopped mode\n");
    }
    p_state.stopped = true;
  } else {
    printf("[enable_state] disabling stopped mode: new speed will be %f\n", p_state.speed);
    p_state.stopped = false;
    change_cycle = true;
  }

  if (p_state.hue != state.hue || p_state.angle != state.angle || p_state.effect != state.effect) cycle_once = true;

  if (p_state.on != state.on) {
    // fade in-out
    transition_start_brightness = eff_brightness;
    transition_start_time = millis();
    transition_duration = p_state.on ? FADE_IN_DURATION : FADE_OUT_DURATION;
    transition_target_brightness = p_state.on ? state.brightness : MIN_BRIGHTNESS;
  }

  state = p_state;

  if (change_cycle) set_cycle(!state.stopped);

  log_state("enable_state", state);

  if (transition_loop(true)) led_strip.update();

  set_encoder_state();
  global_last_activity = millis();
  if (_on_state_change_cb) _on_state_change_cb(state);
}

bool LEDControl::transition_loop(bool force) {
  if (!force && transition_start_time == 0) return false;

  // we do this to prevent flickering
  static float_t old_eff_brightness = -1.0f;
  float_t tmp = get_effective_brightness();
  if (old_eff_brightness == -1.0f || old_eff_brightness != tmp) {
    set_brightness(tmp);
    old_eff_brightness = tmp;
    return true; // changed, need to call led_strip.update()
  }

  return false;
}

LEDControl::state_t LEDControl::get_state() {
  return state;
}

void LEDControl::log_state(const char *prefix, state_t s) {
  printf("[%s] hue: %f, angle: %f, speed: %f, brightness: %f, mode:%d, effect:%d%s%s\n",
         prefix, s.hue, s.angle, s.speed, s.brightness, s.mode, s.effect, s.stopped? " (stopped)":"", s.on? "":" (off)");
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
  uint32_t t = millis() - start_time;
  if(enc->get_interrupt_flag()) {
    signed int count_raw = enc->read(); // Looks like -64 to +64, but we assume -10 to +10
    float_t count = std::min(10.0f, std::max(-10.0f, (float_t)count_raw))/50.0f; // Max increase can be 20% per update
    printf("[encoder] count: %d (%f)\n", count_raw, count);
    enc->clear_interrupt_flag();
    enc->clear();
    encoder_last_activity = millis();
    global_last_activity = encoder_last_activity;

    switch(menu_mode) {
      default:
      case MENU_MODE::MENU_SELECT:
        state.mode = (ENCODER_MODE)limiting_wrap(state.mode + (count < 0.0f ? -1 : 1), 0, ENCODER_MODE::MODE_COUNT);
        printf("[mode] new mode: %d\n", state.mode);
        set_encoder_state();
        break;
      case MENU_MODE::MENU_ADJUST:
        if (state.mode == ENCODER_MODE::OFF) break;

        set_cycle(state.mode == ENCODER_MODE::SPEED);

        auto new_state = state;
        switch (state.mode) {
          default:
          case ENCODER_MODE::OFF: // not possible due to if above
            break;

          case ENCODER_MODE::COLOUR:
            new_state.hue = wrap(state.hue + count, 0.0f, 1.0f);
            printf("new hue start angle: %f\n", state.hue);
            break;

          case ENCODER_MODE::ANGLE:
            new_state.angle = std::min(1.0f, std::max(0.0f, state.angle + count));
            printf("new hue end angle: %f\n", state.angle);
            break;

          case ENCODER_MODE::BRIGHTNESS:
            new_state.brightness = std::min(MAX_BRIGHTNESS, std::max(MIN_BRIGHTNESS, state.brightness + count));
            printf("new brightness: %f\n", new_state.brightness);
            set_brightness(get_effective_brightness());
            enc->set_brightness(new_state.brightness);
            led_strip.update();
            break;

          case ENCODER_MODE::SPEED:
            new_state.speed = std::min(MAX_SPEED, std::max(MIN_SPEED, state.speed + count));
            if (new_state.speed == 0.0f && state.speed != 0.0f) {
              new_state.speed = state.speed;
              new_state.stopped = true;
            } else if (new_state.speed != 0.0f) {
              new_state.stopped = false;
            }
            printf("new speed: %f%s\n", new_state.speed, new_state.stopped?" (stopped)":"");
            break;

          case ENCODER_MODE::EFFECT:
            new_state.effect = (EFFECT_MODE)limiting_wrap(state.effect + (count < 0.0 ? -1 : 1), 0, EFFECT_MODE::EFFECT_COUNT);

            printf("new effect: %d\n", state.effect);
            break;
        }

        new_state.on = true; // always set to on if there is a change
        enable_state(new_state);
    }
  } // get_interrupt_flag

  auto b_val = wait_for_long_button(button_b, 1500);
  bool b_pressed = b_val == 1;
  bool b_held = b_val == 2;

  bool a_pressed = enc->get_clicked();
  if (a_pressed) {
    enc->clear_clicked();
    encoder_last_activity = millis();
    global_last_activity = encoder_last_activity;
  }

  if (b_pressed || b_held) {
    printf("[button] B pressed:%d held:%d\n", b_pressed, b_held);
  }

  if(b_held) {
    set_brightness(0);
    led_strip.update();
    printf("B held\n");
    sleep_ms(1500);
    save_state_to_flash();

    set_brightness(get_effective_brightness());
    led_strip.update();
    global_last_activity = millis();
  }

  if(b_pressed) {
    printf("B pressed! saved state or defaults\n");
    enable_state(DEFAULT_STATE);
    menu_mode = MENU_MODE::MENU_SELECT;
    set_cycle(true);
  }

  if (button_c.read()) {
    printf("C pressed! toggling on/off to %d\n", (int)(!state.on));

    menu_mode = MENU_MODE::MENU_SELECT;
    state.mode = ENCODER_MODE::OFF;

    state_t new_state = state;
    new_state.on = !state.on;
    enable_state(new_state);
  }

  bool resume_cycle = false;
  if (a_pressed) {
    if (state.mode == ENCODER_MODE::OFF) { // If we're off, switch to first mode
      state.mode = ENCODER_MODE::COLOUR;
    } else {
      menu_mode = (MENU_MODE)(((int) menu_mode + 1) % (int) MENU_MODE::MENU_COUNT);
      printf("[menu] new menu selection: %d\n", menu_mode);
      if (!cycle) resume_cycle = true;
    }
    set_encoder_state();
  }

  if (ENCODER_INACTIVITY_TIMEOUT>0 && state.mode != ENCODER_MODE::OFF && encoder_last_activity > 0 && millis() - encoder_last_activity > ENCODER_INACTIVITY_TIMEOUT) {
    printf("[menu] encoder inactivity, switching to off mode\n");
    encoder_last_activity = 0;
    menu_mode = MENU_MODE::MENU_SELECT;
    state.mode = ENCODER_MODE::OFF;
    set_encoder_state();
    if (!cycle) resume_cycle = true;
  }

  if (resume_cycle) {
    set_cycle(true);
    t = millis() - start_time;
    log_state("cycle", state);
  }

  bool need_refresh = false;
  if (cycle || cycle_once) {
    cycle_loop(state.hue, (float) (t - get_paused_time()) * state.speed, state.angle);
    cycle_once = false;
    need_refresh = true;
  }

  need_refresh |= transition_loop(false);

  if (global_last_activity > 0 && GLOBAL_INACTIVITY_TIMEOUT_SECS > 0 && millis() - global_last_activity > GLOBAL_INACTIVITY_TIMEOUT_SECS * 1000 && state.on) {
    printf("[menu] global inactivity, turning off\n");
    global_last_activity = 0;
    auto p_state = state;
    p_state.on = false;
    enable_state(p_state);
  }

  if (need_refresh) {
    led_strip.update();
  }

  if (menu_mode == MENU_MODE::MENU_ADJUST) encoder_loop();
  else encoder_blink_off();

  // Sleep time controls the rate at which the LED buffer is updated
  // but *not* the actual framerate at which the buffer is sent to the LEDs
  return 1000 / UPDATES;
}
