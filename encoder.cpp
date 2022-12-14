#include "encoder.h"
#include "pico/stdlib.h"
#include <cstdio>
#include <common/pimoroni_common.hpp>
#include "hardware/gpio.h"
#include "hardware/pwm.h"

uint32_t pwm_set_freq_duty(uint slice_num, uint chan, uint32_t f, int d) {
  // from this article: https://www.i-programmer.info/programming/hardware/14849-the-pico-in-c-basic-pwm.html
  uint32_t clock = 125000000;
  uint32_t divider16 = clock / f / 4096 + (clock % (f * 4096) != 0);
  if (divider16 / 16 == 0) divider16 = 16;
  uint32_t wrap = clock * 16 / divider16 / f - 1;
  pwm_set_clkdiv_int_frac(slice_num, divider16/16, divider16 & 0xF);
  pwm_set_wrap(slice_num, wrap);
  pwm_set_chan_level(slice_num, chan, wrap * d / 100);
  return wrap;
}

Encoder::Encoder() {
}

void Encoder::init(uint p_pin_led_r, uint p_pin_led_g, uint p_pin_led_b,
                   uint p_pin_enc_a, uint p_pin_enc_b, uint p_pin_enc_sw, bool p_leds_active_low) {
  pin_enc_a = p_pin_enc_a;
  pin_enc_b = p_pin_enc_b;
  pin_enc_sw = p_pin_enc_sw;
  leds_active_low = p_leds_active_low;

  led_pins[0] = p_pin_led_r;
  led_pins[1] = p_pin_led_g;
  led_pins[2] = p_pin_led_b;
  in_pins[0] = pin_enc_a;
  in_pins[1] = pin_enc_b;
  in_pins[2] = pin_enc_sw;

  led_brightness = 1.0f;
  led_vals[0] = 0;
  led_vals[1] = 0;
  led_vals[2] = 0;

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

      gpio_set_function(pin, GPIO_FUNC_PWM);
      uint slice_num = pwm_gpio_to_slice_num(pin);
      uint chan = pwm_gpio_to_channel(pin);
      pwm_set_freq_duty(slice_num,chan, 50, 0);
      pwm_set_enabled(slice_num, true);
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
  led_vals[0] = r;
  led_vals[1] = g;
  led_vals[2] = b;

  for(auto i = 0; i < 3; i++) {
    uint slice_num = pwm_gpio_to_slice_num(led_pins[i]);
    uint chan = pwm_gpio_to_channel(led_pins[i]);
    float duty_pct = ((float)led_vals[i])/255.0f*100.0f*led_brightness;
    if (leds_active_low) duty_pct = 100.0f - duty_pct;
    pwm_set_freq_duty(slice_num,chan, 60, int(duty_pct));
  }
}

void Encoder::set_brightness(float brightness) {
  if (brightness < 0.0f) brightness = 0.0f;
  else if (brightness > 1.0f) brightness = 1.0f;
  led_brightness = brightness;
  set_leds(led_vals[0], led_vals[1], led_vals[2]);
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
