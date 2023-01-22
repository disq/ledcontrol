#include "presence.h"
#include <cstdio>
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "config.h"
#include "pico/cyw43_arch.h"


Presence::Presence(): u(PRESENCE_UART) {
}

void Presence::init(uint p_pin, bool p_active_low, uint p_tx_pin, uint p_rx_pin) {
  if (PRESENCE_PIN_ENABLED) {
    pin = p_pin;
    pin_active_low = p_active_low;

    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    (pin_active_low ? gpio_pull_up : gpio_pull_down)(pin);
  }

  uart_enabled = p_tx_pin > 0 && p_rx_pin > 0;
  if (!uart_enabled) return;

  uart_init(u, 115200);
  gpio_set_function(p_tx_pin, GPIO_FUNC_UART);
  gpio_set_function(p_rx_pin, GPIO_FUNC_UART);

  sns = new DFRobot_mmWave_Radar(u);
  sns->stop();
  flush_uart();
  sns->factoryReset();
  flush_uart();
  sns->DetRangeCfg(0.0f, PRESENCE_RANGE_METERS);
  flush_uart();
  sns->OutputLatency(0.0f, 0.0f);
  flush_uart();
  sns->save();
  flush_uart();
  sns->start();
  flush_uart();

  if (PRESENCE_PIN_ENABLED) uart_deinit(u);
}

void Presence::flush_uart() {
  if (!uart_enabled) return;

  uint32_t rx = 0;
  while (uart_is_readable(u)) {
    auto uu = uart_getc(u);
    if (rx == 0) printf("[presence UART] ");
    if (uu >= 32 && uu <= 126) printf("%c", uu);
    else printf("%02X", uu);
    rx++;
  }
  if (rx > 0) {
    printf("\n[presence] flushed %d bytes\n", rx);
  }
}

bool Presence::is_present() {
  static bool last_val = false;

  bool val;
  if (PRESENCE_PIN_ENABLED) {
    val = gpio_get(pin);
    if (pin_active_low) val = !val;
  } else if (uart_enabled) {
    if (!sns->readPresenceDetection(&val)) {
      printf("[presence] readPresenceDetection failed, returning previous value (%d)\n", last_val);
      return last_val;
    }
  } else {
    printf("[presence] unhandled condition\n");
    return true;
  }

  if (val != last_val) {
        printf("[presence] %s\n", val ? "present" : "absent");
        last_val = val;
  }

//  flush_uart();
  return val;
}

Presence presence;
