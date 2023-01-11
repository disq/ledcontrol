#include "presence.h"
#include <cstdio>
#include "hardware/gpio.h"

Presence::Presence() {
}

void Presence::init(uint p_pin, bool p_active_low) {
  pin = p_pin;
  pin_active_low = p_active_low;

  gpio_init(pin);
  gpio_set_dir(pin, GPIO_IN);
  if (pin_active_low) {
    gpio_pull_up(pin);
  } else {
    gpio_pull_down(pin);
  }
}

bool Presence::is_present() {
  static bool last_val = false;

  bool val = gpio_get(pin);
  if (pin_active_low) val = !val;

  if (val != last_val) {
        printf("[presence] %s\n", val ? "present" : "absent");
        last_val = val;
  }

  return val;
}

Presence presence;
