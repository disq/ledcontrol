#ifndef PRESENCE_H
#define PRESENCE_H

#include <cstdio>
#include <cstdint>
#include "DFRobot_mmWave_Radar.h"

class Presence {
private:
    uint pin;
    bool pin_active_low;
    bool uart_enabled;
    DFRobot_mmWave_Radar *sns;

public:
    Presence();
    void init(uint p_pin, bool p_active_low, uint p_uart_id, uint p_tx_pin, uint p_rx_pin);
    bool is_present();
};

extern Presence presence;

#endif //PRESENCE_H
