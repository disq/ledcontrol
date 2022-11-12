#ifndef LEDCONTROL_UTIL_H
#define LEDCONTROL_UTIL_H

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

#endif //LEDCONTROL_UTIL_H
