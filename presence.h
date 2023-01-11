#ifndef PRESENCE_H
#define PRESENCE_H

#include <cstdio>
#include <cstdint>

class Presence {
private:
    uint pin;
    bool pin_active_low;

public:
    Presence();
    void init(uint p_pin, bool p_active_low = true);
    bool is_present();
};

extern Presence presence;

#endif //PRESENCE_H
