#include "pico/stdlib.h"
#ifdef RASPBERRYPI_PICO_W
#include "pico/cyw43_arch.h"
#include "iot.h"
#endif

#include "ledcontrol.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

void board_led(bool val) {
#ifdef BOARD_LED_PIN
  gpio_put(BOARD_LED_PIN, val);
#endif
#ifdef RASPBERRYPI_PICO_W
  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, val);
#endif
}

void wifi_looper() {
  printf(".");
  sleep_ms(500);
}

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
  auto val = iot.init(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, wifi_looper);
  printf("iot returned: %d\n", val);
  while(true) sleep_ms(1000);
#endif

  auto leds = ledcontrol::LEDControl();
  leds.init(&encoder);

  while(true) {
    sleep_ms(leds.loop());
  }
}
