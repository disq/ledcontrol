/*!
   @file  DFRobot_mmWave_Radar.cpp
   @brief  Implement the basic structure of class mmwave rader sensor-human presence detection 
   @copyright  Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
   @licence  The MIT License (MIT)
   @author  huyujie(yujie.hu@dfrobot.com)
   @version  V1.0
   @date  2020-3-25
   @get  from https://www.dfrobot.com
   @url  https://github.com/DFRobot
*/

// Modified to suit Raspberry Pi Pico stdlib

#include "DFRobot_mmWave_Radar.h"
#include "pico/stdlib.h"
#include <cstdio>
#include <cstring>


DFRobot_mmWave_Radar::DFRobot_mmWave_Radar(uart_inst_t *s)
{
  _s = s;
}

size_t DFRobot_mmWave_Radar::readN(uint8_t *buf, size_t len)
{
  size_t offset = 0, left = len;
  const int32_t timeout = 1500;
  uint8_t  *buffer = buf;
  uint32_t curr = to_ms_since_boot(get_absolute_time());
  while (left) {
    if (uart_is_readable(_s)) {
      buffer[offset] = uart_getc(_s);
      offset++;
      left--;
    }
    if (to_ms_since_boot(get_absolute_time()) - curr > timeout) {
      break;
    }
  }
  return offset;
}

bool DFRobot_mmWave_Radar::recdData(uint8_t *buf)
{

  uint32_t timeout = 50000;
  long curr = to_ms_since_boot(get_absolute_time());
  uint8_t ch;
  bool ret = false;
  while (!ret) {
    if (to_ms_since_boot(get_absolute_time()) - curr > timeout) {
      break;
    }

    if (readN(&ch, 1) == 1) {
      if (ch == '$') {
        buf[0] = ch;
        if (readN(&ch, 1) == 1) {
          if (ch == 'J') {
            buf[1] = ch;
            if (readN(&ch, 1) == 1) {
              if (ch == 'Y') {
                buf[2] = ch;
                if (readN(&ch, 1) == 1) {
                  if (ch == 'B') {
                    buf[3] = ch;
                    if (readN(&ch, 1) == 1) {
                      if (ch == 'S') {
                        buf[4] = ch;
                        if (readN(&ch, 1) == 1) {
                          if (ch == 'S') {
                            buf[5] = ch;
                            if (readN(&buf[6], 9) == 9) {
                              ret = true;
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  return ret;
}


bool DFRobot_mmWave_Radar::readPresenceDetection(bool *result)
{
  uint8_t dat[15] = {0};

  if (!recdData(dat)) {
    return false;
  }

  switch (dat[7]) {
    case '1':
      *result = true;
      return true;
    case '0':
      *result = false;
      return true;
    default:
      return false;
  }
}


void DFRobot_mmWave_Radar::DetRangeCfg(float parA_s, float parA_e)
{
  char comDetRangeCfg[22] = {0};
  int16_t parA_S = parA_s / 0.15;
  int16_t parA_E = parA_e / 0.15;
  sprintf(comDetRangeCfg, "detRangeCfg -1 %d %d", parA_S, parA_E);

  uart_write_blocking(_s, (uint8_t*)comStop, strlen(comStop));
  sleep_ms(comDelay);

  uart_write_blocking(_s, (uint8_t*)comDetRangeCfg, strlen(comDetRangeCfg));
  sleep_ms(comDelay);

  uart_write_blocking(_s, (uint8_t*)comSaveCfg, strlen(comSaveCfg));
  sleep_ms(comDelay);

  uart_write_blocking(_s, (uint8_t*)comStart, strlen(comStart));
  sleep_ms(comDelay);
}

void DFRobot_mmWave_Radar::OutputLatency(float par1, float par2)
{

  char comOutputLatency[28] = {0};
  int16_t Par1 = par1 * 1000 / 25;
  int16_t Par2 = par2 * 1000 / 25;
  sprintf(comOutputLatency, "outputLatency -1 %d %d", Par1 , Par2);

  uart_write_blocking(_s, (uint8_t*)comStop, strlen(comStop));
  sleep_ms(comDelay);

  uart_write_blocking(_s, (uint8_t*)comOutputLatency, strlen(comOutputLatency));
  sleep_ms(comDelay);

  uart_write_blocking(_s, (uint8_t*)comSaveCfg, strlen(comSaveCfg));
  sleep_ms(comDelay);

  uart_write_blocking(_s, (uint8_t*)comStart, strlen(comStart));
  sleep_ms(comDelay);
}

void DFRobot_mmWave_Radar::factoryReset(void)
{
  uart_write_blocking(_s, (uint8_t*)comStop, strlen(comStop));
  sleep_ms(comDelay);

  uart_write_blocking(_s, (uint8_t*)comFactoryReset, strlen(comFactoryReset));
  sleep_ms(comDelay);

  uart_write_blocking(_s, (uint8_t*)comSaveCfg, strlen(comSaveCfg));
  sleep_ms(comDelay);

  uart_write_blocking(_s, (uint8_t*)comStart, strlen(comStart));
  sleep_ms(comDelay);
}
