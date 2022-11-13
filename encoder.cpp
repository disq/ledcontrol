#include "encoder.h"
#include "pico/stdlib.h"
#include <cstdio>
#include <common/pimoroni_common.hpp>
#include "hardware/gpio.h"

Encoder::Encoder() {
}

void Encoder::init(int p_pin_led_r, int p_pin_led_g, int p_pin_led_b,
                   int p_pin_enc_a, int p_pin_enc_b, int p_pin_enc_sw, bool p_leds_active_low) {
  pin_led_r = p_pin_led_r;
  pin_led_g = p_pin_led_g;
  pin_led_b = p_pin_led_b;
  pin_enc_a = p_pin_enc_a;
  pin_enc_b = p_pin_enc_b;
  pin_enc_sw = p_pin_enc_sw;
  leds_active_low = p_leds_active_low;

  led_pins[0] = pin_led_r;
  led_pins[1] = pin_led_g;
  led_pins[2] = pin_led_b;
  in_pins[0] = pin_enc_a;
  in_pins[1] = pin_enc_b;
  in_pins[2] = pin_enc_sw;

  for (auto pin : led_pins) {
      gpio_init(pin);
      gpio_set_dir(pin, GPIO_OUT);

      if (leds_active_low) {
        gpio_pull_up(pin);
        gpio_put(pin, true);
      } else {
        gpio_pull_down(pin);
        gpio_put(pin, false);
      }
  }

   for (auto pin : in_pins) {
     gpio_init(pin);
     gpio_set_dir(pin, GPIO_IN);
     if (pin == pin_enc_sw) {
       gpio_pull_down(pin);
     } else {
       gpio_pull_up(pin);
     }
     gpio_set_irq_enabled_with_callback(pin, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &_encoder_gpio_callback);
   }
 }

signed int Encoder::read_rotary() {
  // lifted from https://www.best-microcontroller-projects.com/rotary-encoder.html
  static int8_t rot_enc_table[] = {0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0};
  static uint8_t prevNextCode = 0;
  static uint8_t store = 0;

  prevNextCode <<= 2;
  if (gpio_get(pin_enc_b)) prevNextCode |= 0x02;
  if (gpio_get(pin_enc_a)) prevNextCode |= 0x01;
  prevNextCode &= 0x0f;

  if  (rot_enc_table[prevNextCode] == 0) return 0;

  store <<= 4;
  store |= prevNextCode;
  if ((store&0xff)==0x2b) return -1;
  if ((store&0xff)==0x17) return 1;
  return 0;
}

void Encoder::set_leds(uint8_t r, uint8_t g, uint8_t b) {
  bool r_on = r >= 128;
  bool g_on = g >= 128;
  bool b_on = b >= 128;
  gpio_put(pin_led_r, !r_on ? leds_active_low : !leds_active_low);
  gpio_put(pin_led_g, !g_on ? leds_active_low : !leds_active_low);
  gpio_put(pin_led_b, !b_on ? leds_active_low : !leds_active_low);
}

void Encoder::set_brightness(float brightness) {
  // TODO with PWM
}

void Encoder::_gpio_callback(uint gpio, uint32_t events) {
//  bool val = gpio_get(gpio);
//  printf("gpio:%d, events:%lu fall:%d rise:%d val:%d\n", gpio, events, events&GPIO_IRQ_EDGE_FALL?1:0, events&GPIO_IRQ_EDGE_RISE?1:0, val);
  if (gpio == pin_enc_sw) {
    if ((to_ms_since_boot(get_absolute_time())-_last_switch_time)>50) {
      _last_switch_time = to_ms_since_boot(get_absolute_time());
      if (events == GPIO_IRQ_EDGE_FALL) is_clicked = true;
    }
    return;
  }

  static uint8_t fall_fall = 0;
  if (gpio == pin_enc_a && events == GPIO_IRQ_EDGE_FALL) {
    fall_fall |= 0x01;
  } else if (gpio == pin_enc_b && events == GPIO_IRQ_EDGE_FALL) {
    fall_fall |= 0x02;
  } else {
    fall_fall = 0;
  }

  signed int val_rotary = read_rotary();
  if (val_rotary == 0) {
//    if (fall_fall == 3) {
//      rotary_counter = 0;
//      fall_fall = 0;
//    }
    return;
  }

  rotary_counter += val_rotary;
  has_data = true;
}

Encoder encoder;

void _encoder_gpio_callback(uint gpio, uint32_t events) {
  encoder._gpio_callback(gpio, events);
}
