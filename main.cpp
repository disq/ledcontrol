#include "pico/stdlib.h"
#ifdef RASPBERRYPI_PICO_W
#include "iot.h"
#include "cjson/cJSON.h"
#endif

#include "hardware/flash.h"
#include "hardware/sync.h"
#include <math.h>
#include <cstring>
#include "ledcontrol.h"
#include "presence.h"
#include "config.h"

ledcontrol::LEDControl *leds = NULL;

void board_led(bool val) {
#ifdef BOARD_LED_PIN
  gpio_put(BOARD_LED_PIN, val);
#endif
#ifdef RASPBERRYPI_PICO_W
  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, val);
#endif
}

#ifdef RASPBERRYPI_PICO_W
void wifi_looper() {
  static bool looper_beeper = false;
  static uint32_t last_looper_beeper_change = 0;

  uint32_t req_ms = leds->loop();
  sleep_ms(req_ms);

  uint32_t ts = to_ms_since_boot(get_absolute_time());
  if (ts - last_looper_beeper_change > 500) {
    last_looper_beeper_change = ts;
    looper_beeper = !looper_beeper;
    board_led(looper_beeper);
  }
}

void publish_state(ledcontrol::LEDControl::state_t state) {
  char buffer[2048] = {0};
  snprintf(buffer, sizeof(buffer), "{\"brightness\": %d, \"color_mode\":\"hs\", \"color\": {\"h\": %d, \"s\": %d}, \"effect\":\"%s:%s\", \"state\":\"%s\"}",
          (int)(state.brightness * 100.0f), (int)(state.hue * 360.0f), (int)(state.angle * 100),
          leds->effect_to_str(state.effect),
          leds->speed_to_str(state.stopped ? 0.0f : state.speed),
          state.on ? "ON" : "OFF"
          );
  iot.publish_state(buffer);
}

void on_state_change(ledcontrol::LEDControl::state_t new_state) {
  leds->log_state("on_state_change", new_state);
  publish_state(new_state);
}

void on_save_command(const char *data, size_t len) {
  printf("[on_save_command] %.*s\n", len, data);

  cJSON *json = cJSON_ParseWithLength(data, len);
  if (json == NULL) {
    cJSON_Delete(json);
    printf("[on_save_command] json parse failed\n");
    return;
  }
//  char *s = cJSON_Print(json);
//  printf("here: %s\n", s);
//  free(s);

  auto on_off = cJSON_GetObjectItem(json, "state");
  if (cJSON_IsString(on_off) && (on_off->valuestring != NULL) && strcmp(on_off->valuestring, "OFF") == 0) {
    // ignore off state. any other json will pass
    cJSON_Delete(json);
    return;
  }

  cJSON_Delete(json);
  leds->save_state_to_flash();
}

void on_command(const char *data, size_t len) {
  printf("[on_command] %.*s\n", len, data);

  cJSON *json = cJSON_ParseWithLength(data, len);
  if (json == NULL) {
    cJSON_Delete(json);
    printf("[on_command] json parse failed\n");
    return;
  }
//  char *s = cJSON_Print(json);
//  printf("here: %s\n", s);
//  free(s);

  auto state = leds->get_state();
  bool changed = false;
  {
    auto on_off = cJSON_GetObjectItem(json, "state");
    if (cJSON_IsString(on_off) && (on_off->valuestring != NULL)) {
      if (strcmp(on_off->valuestring, "ON") == 0) {
        if (state.on != true) {
          state.on = true;
          changed = true;
        }
      } else if (strcmp(on_off->valuestring, "OFF") == 0) {
        if (state.on != false) {
          state.on = false;
          changed = true;
        }
      } else {
        printf("[on_command] received unknown state: %s\n", on_off->valuestring);
      }
    }
  }

  {
    auto effect = cJSON_GetObjectItem(json, "effect");
    if (cJSON_IsString(effect) && (effect->valuestring != NULL)) {
      ledcontrol::LEDControl::EFFECT_MODE eff;
      float_t speed;
      int res = leds->parse_effect_str((const char *)effect->valuestring, &eff, &speed);
      if (res < 0) {
        printf("[on_command] received unknown effect: %s\n", effect->valuestring);
      } else {
        if (state.effect != eff) {
          state.effect = eff;
          changed = true;
        }
        if (res == 1 && (state.speed != speed || state.stopped)) {
          state.speed = speed;
//          state.stopped = speed == 0.0f; // not needed as we calculate it in enable_state anyway
          changed = true;
        }
      }
    }
  }

  {
    auto color = cJSON_GetObjectItem(json, "color");
    if (cJSON_IsObject(color)) {
      auto hVal = cJSON_GetObjectItem(color, "h");
      auto hSal = cJSON_GetObjectItem(color, "s");
      if (cJSON_IsNumber(hVal) && cJSON_IsNumber(hSal)) {
        float h = hVal->valuedouble / 360.0f;
        float s = hSal->valuedouble / 100.0f;
        if (state.hue != h || state.angle != s) {
          state.hue = h;
          state.angle = s;
          changed = true;
        }
      } else {
        printf("[on_command] received unknown color\n");
      }
    }
  }

  {
    auto brightness = cJSON_GetObjectItem(json, "brightness");
    if (cJSON_IsNumber(brightness)) {
      float b = brightness->valuedouble / 100.0f;
      if (!std::isnan(b) && !std::isinf(b) && state.brightness != b) {
        state.brightness = b;
        changed = true;
      }
    }
  }

  cJSON_Delete(json);

  if (changed) {
    printf("[on_command] applying new state\n");
    leds->enable_state(state);
  }
}

bool mqtt_connected = false;

void on_mqtt_connect() {
  mqtt_connected = true;
  printf("mqtt connected\n");
  leds->set_on_state_change_cb(on_state_change);

  auto cur_state = leds->get_state();
  publish_state(cur_state);

  // calculate effect list
  char buffer[2048] = {0};

  LEDControl::EFFECT_MODE effs[16];
  size_t num_effs = leds->get_effect_list(effs, sizeof(effs));
  const char *speeds[16];
  size_t num_speeds = leds->get_speed_list(speeds, sizeof(speeds));
  for(size_t i = 0; i < num_effs; i++) {
    for(size_t j = 0; j < num_speeds; j++) {
      // give up and use strcat
      if (i != 0 || j != 0) {
        strcat(buffer, ",");
      }
      strcat(buffer, "\"");
      strcat(buffer, leds->effect_to_str(effs[i]));
      strcat(buffer, ":");
      strcat(buffer, speeds[j]);
      strcat(buffer, "\"");
    }
  }
  iot.publish_config(buffer);
//  iot.publish_config("", true); // save button. fails with -1 so we do it in main() below
}
#endif

void handle_presence() {
  static uint32_t last_check = 0;
  uint32_t ts = to_ms_since_boot(get_absolute_time());
  if (ts - last_check < PRESENCE_CHECK_INTERVAL) return;
  last_check = ts;

  auto val = !presence.is_present();
  auto s = leds->get_state();
  if (s.absent == val) return;

  s.absent = val;
  leds->enable_state(s);
}

void error_loop(uint32_t delay_ms) {
  while(true) {
    board_led(true);
    sleep_ms(delay_ms);
    board_led(false);
    sleep_ms(delay_ms);
  }
}

int main() {
  stdio_init_all();
#ifdef RASPBERRYPI_PICO_W
#ifdef WIFI_COUNTRY_CODE
  if (cyw43_arch_init_with_country(WIFI_COUNTRY_CODE)) {
#else
  if (cyw43_arch_init()) {
#endif
    printf("[error] WiFi init failed\n");
    error_loop(200);
  }
#endif

  if (false)
  {
    board_led(true);
    while (!stdio_usb_connected()) sleep_ms(100);
    printf("hello\n");
    board_led(false);
  }

  leds = new ledcontrol::LEDControl();
  leds->init(&encoder);

#ifdef RASPBERRYPI_PICO_W
  auto init_val = iot.init(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, wifi_looper, on_mqtt_connect, on_command, on_save_command);
  if (init_val != 0) {
    printf("[error] iot.init returned: %d\n", init_val);
    error_loop(1000);
  }
  board_led(false); // turn off 'looper beeper'

  printf("Initiating MQTT connection\n");
  auto conn_val = iot.connect();
  if (conn_val != 0) {
    printf("[error] iot.connect returned: %d\n", conn_val);
    error_loop(500);
  }
#endif

  if (PRESENCE_ENABLED) presence.init(PRESENCE_PIN, PRESENCE_PIN_ACTIVE_LOW, PRESENCE_UART_TX_PIN, PRESENCE_UART_RX_PIN);

  bool save_published = false;
  while(true) {
#if PICO_CYW43_ARCH_POLL
    cyw43_arch_poll();
    sleep_ms(1);
#endif
    sleep_ms(leds->loop());
    if (PRESENCE_ENABLED) handle_presence();

#ifdef RASPBERRYPI_PICO_W
    // consecutive publish calls fail, so we need to wait a bit
    if (mqtt_connected && !save_published) {
        sleep_ms(100);
#if PICO_CYW43_ARCH_POLL
        cyw43_arch_poll();
        sleep_ms(1);
#endif
        save_published = true;
        iot.publish_config("", true); // save button
    }
#endif
  }
}
