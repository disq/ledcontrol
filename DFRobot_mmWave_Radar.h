/*!
   @file  DFRobot_mmWave_Radar.h
   @brief  Define the basic structure of class mmWave radar sensor-human presence detection 
   @copyright  Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
   @licence  The MIT License (MIT)
   @author  huyujie(yujie.hu@dfrobot.com)
   @version  V1.0
   @date  2020-2-25
   @get  from https://www.dfrobot.com
   @url  https://github.com/DFRobot
*/

// Modified to suit Raspberry Pi Pico stdlib

#ifndef __DFRobot_mmWave_Radar_H__
#define __DFRobot_mmWave_Radar_H__

#include "hardware/uart.h"

class DFRobot_mmWave_Radar
{
  public:
    DFRobot_mmWave_Radar(uart_inst_t *s);
    void DetRangeCfg(float parA_s, float parA_e);
    bool readPresenceDetection(void);
    void OutputLatency(float par1, float par2);
    void factoryReset();

  private:

    size_t readN(uint8_t *buf, size_t len);
    bool recdData(uint8_t *buf);

    const uint comDelay = 1000; //Command sending interval (The test states that the interval between two commands must be larger than 1000ms)

    uart_inst_t *_s;
    const char *comStop = "sensorStop";     //Sensor stop command. Stop the sensor when it is still running
    const char *comStart = "sensorStart";     //Sensor start command. When the sensor is not started and there are no set parameters to save, start the sensor to run
    const char *comSaveCfg = "saveCfg 0x45670123 0xCDEF89AB 0x956128C6 0xDF54AC89";     //Parameter save command. When the sensor parameter is reconfigured via serialport but no tsaved, use this command to save the new configuration into sensor Flash
    const char *comFactoryReset = "factoryReset 0x45670123 0xCDEF89AB 0x956128C6 0xDF54AC89";     //Factory settings restore command. Restore the sensor to the factory default settings
};

#endif