#include "pico/stdlib.h"
#ifdef RASPBERRYPI_PICO_W
#include "iot.h"
#include "cjson/cJSON.h"
#endif

#include "ledcontrol.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <math.h>

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
//  uint32_t req_ms = leds->loop();
//  sleep_ms(req_ms);
sleep_ms(10);
}

void publish_state(ledcontrol::LEDControl::state_t state) {
  char buffer[2048] = {0};
  snprintf(buffer, sizeof(buffer), "{\"brightness\": %d, \"color_mode\":\"hs\", \"color\": {\"h\": %d, \"s\": %d}, \"effect\":\"%s\", \"transition\":%d, \"state\":\"%s\"}",
          (int)(state.brightness * 100.0f), (int)(state.hue * 360.0f), (int)(state.angle * 100),
          state.effect == ledcontrol::LEDControl::EFFECT_MODE::HUE_CYCLE ? "hue_cycle" : "white_chase",
          (int)(state.speed * 100.0f),
          state.on ? "ON" : "OFF"
          );
  iot.topic_publish("", buffer);
}

void on_state_change(ledcontrol::LEDControl::state_t new_state) {
  printf("[on_state_change] hue: %f, angle: %f, speed: %f, brightness: %f, mode:%d, effect:%d\n", new_state.hue,
         new_state.angle, new_state.speed, new_state.brightness, new_state.mode, new_state.effect);

  publish_state(new_state);
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
      if (strcmp(effect->valuestring, "hue_cycle") == 0) {
        if (state.effect != ledcontrol::LEDControl::EFFECT_MODE::HUE_CYCLE) {
          state.effect = ledcontrol::LEDControl::EFFECT_MODE::HUE_CYCLE;
          changed = true;
        }
      } else if (strcmp(effect->valuestring, "white_chase") == 0) {
        if (state.effect != ledcontrol::LEDControl::EFFECT_MODE::WHITE_CHASE) {
          state.effect = ledcontrol::LEDControl::EFFECT_MODE::WHITE_CHASE;
          changed = true;
        }
      } else {
        printf("[on_command] received unknown effect: %s\n", effect->valuestring);
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

void on_mqtt_connect() {
  printf("mqtt connected\n");
  leds->set_on_state_change_cb(on_state_change);

  auto cur_state = leds->get_state();
  publish_state(cur_state);
//  iot.publish_switch_state(true);
//  iot.publish_brightness(cur_state.brightness);
//  iot.publish_colour(0.3, 0.5, 0.7);
}
#endif

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
  if (cyw43_arch_init_with_country(WIFI_COUNTRY_CODE)) {
    printf("[error] WiFi init failed\n");
    error_loop(200);
  }
#endif

//  if (false)
  {
    board_led(true);
    while (!stdio_usb_connected()) sleep_ms(100);
    printf("hello\n");
    board_led(false);
  }

#ifdef RASPBERRYPI_PICO_W
  auto init_val = iot.init(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, wifi_looper, on_mqtt_connect, on_command);
  if (init_val != 0) {
    printf("[error] iot.init returned: %d\n", init_val);
    error_loop(1000);
  }
#endif

  leds = new ledcontrol::LEDControl();
  leds->init(&encoder);

#ifdef RASPBERRYPI_PICO_W
  printf("Initiating MQTT connection\n");
  auto conn_val = iot.connect();
  if (conn_val != 0) {
    printf("[error] iot.connect returned: %d\n", conn_val);
    error_loop(500);
  }
#endif

  while(true) {
    sleep_ms(leds->loop());
  }
}
