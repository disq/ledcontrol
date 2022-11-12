#include "pico/stdlib.h"
#ifdef RASPBERRYPI_PICO_W
#include "pico/cyw43_arch.h"
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

int main() {
  stdio_init_all();
#ifdef RASPBERRYPI_PICO_W
  if (cyw43_arch_init()) {
    printf("WiFi init failed");
    return -1;
  }
#endif

  if (false)
  {
    board_led(true);
    while (!stdio_usb_connected()) sleep_ms(100);
    printf("hello\n");
    board_led(false);
  }

  auto leds = ledcontrol::LEDControl();
  leds.init();

  while(true) {
    sleep_ms(leds.loop());
  }
}
