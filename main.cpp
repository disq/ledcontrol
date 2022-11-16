#include "pico/stdlib.h"
#ifdef RASPBERRYPI_PICO_W
#include "pico/cyw43_arch.h"
#include "iot.h"
#endif

#include "ledcontrol.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

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
  uint32_t req_ms = leds->loop();
  sleep_ms(req_ms);
}

void on_state_change(ledcontrol::LEDControl::state_t new_state) {
  printf("[on_state_change] hue: %f, angle: %f, speed: %f, brightness: %f, mode:%d, effect:%d\n", new_state.hue,
         new_state.angle, new_state.speed, new_state.brightness, new_state.mode, new_state.effect);

  iot.publish_switch_state(new_state.effect == ledcontrol::LEDControl::HUE_CYCLE);
  iot.publish_brightness(new_state.brightness);
  iot.publish_colour(0.3, 0.5, 0.7);
}

void on_mqtt_connect() {
  printf("mqtt connected\n");
  leds->set_on_state_change_cb(on_state_change);

  auto cur_state = leds->get_state();
  iot.publish_switch_state(true);
  iot.publish_brightness(cur_state.brightness);
  iot.publish_colour(0.3, 0.5, 0.7);
}
#endif

int main() {
  stdio_init_all();
#ifdef RASPBERRYPI_PICO_W
  if (cyw43_arch_init()) {
    printf("WiFi init failed\n");
    return -1;
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
  auto val = iot.init(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, wifi_looper, on_mqtt_connect);
  printf("iot returned: %d\n", val);
//  while(true) sleep_ms(1000);
#endif

  leds = new ledcontrol::LEDControl();
  leds->init(&encoder);

#ifdef RASPBERRYPI_PICO_W
  iot.connect();
#endif

  while(true) {
    sleep_ms(leds->loop());
  }
}
