#include "presence.h"
#include <cstdio>
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "config.h"

Presence::Presence() {
}

void Presence::init(uint p_pin, bool p_active_low, uint p_uart_id, uint p_tx_pin, uint p_rx_pin) {
  if (p_tx_pin == 0 || p_rx_pin == 0) {
    pin = p_pin;
    pin_active_low = p_active_low;

    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    if (pin_active_low) {
      gpio_pull_up(pin);
    } else {
      gpio_pull_down(pin);
    }
    return;
  }

  auto u = uart_get_instance(p_uart_id);
  uart_init(u, 115200);
  gpio_set_function(p_tx_pin, GPIO_FUNC_UART);
  gpio_set_function(p_rx_pin, GPIO_FUNC_UART);
  uart_enabled = true;

  sns = new DFRobot_mmWave_Radar(u);
  sns->factoryReset();
  sns->DetRangeCfg(0, PRESENCE_RANGE_METERS);
  sns->OutputLatency(0, 0);
}

bool Presence::is_present() {
  static bool last_val = false;

  bool val;

  if (uart_enabled) {
    val = sns->readPresenceDetection();
  } else {
    val = gpio_get(pin);
    if (pin_active_low) val = !val;
  }

  if (val != last_val) {
        printf("[presence] %s\n", val ? "present" : "absent");
        last_val = val;
  }

  return val;
}

Presence presence;
