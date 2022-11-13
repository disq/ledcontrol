#ifndef LEDCONTROL_UTIL_H
#define LEDCONTROL_UTIL_H

#include "button.hpp"

float wrap(float v, float min, float max) {
  if(v <= min)
    v += (max - min);

  if(v > max)
    v -= (max - min);

  return v;
}

signed int limiting_wrap(signed int v, int min, int max) {
  if(v < min)
    v += (max - min);

  if(v >= max)
    v -= (max - min);

  return v;
}

inline uint32_t millis() {
  return to_ms_since_boot(get_absolute_time());
}

uint8_t wait_for_long_button(pimoroni::Button b, uint16_t long_duration) {
  uint8_t cur_mode = 0;
  uint32_t start = millis();
  while (b.raw()) {
    uint32_t diff = millis() - start;
    if (cur_mode == 0 && diff < long_duration) {
      cur_mode = 1;
    } else if (cur_mode == 1 && diff >= long_duration) {
      cur_mode = 2;
      break;
    }

    sleep_ms(50);
  }

  return cur_mode;
}

void print_buf(const uint8_t *buf, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    printf("%02x", buf[i]);
    if (i % 16 == 15)
      printf("\n");
    else
      printf(" ");
  }
}

#endif //LEDCONTROL_UTIL_H
