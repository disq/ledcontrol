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
    uart_inst_t *u;
    DFRobot_mmWave_Radar *sns;

    void flush_uart();

public:
    Presence();
    void init(uint p_pin, bool p_active_low, uint p_tx_pin, uint p_rx_pin);
    bool is_present();
};

extern Presence presence;

#endif //PRESENCE_H
