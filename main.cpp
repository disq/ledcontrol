#include "pico/stdlib.h"
#ifdef RASPBERRYPI_PICO_W
#include "pico/cyw43_arch.h"
#endif

#include "ledcontrol.h"

int main() {
  stdio_init_all();

  auto leds = ledcontrol::LEDControl();
  if (leds.init() != 0) {
    while(true);
  }

  while(true) {
    sleep_ms(leds.loop());
  }
}
