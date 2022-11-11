#ifndef LEDCONTROL_ENUM_H
#define LEDCONTROL_ENUM_H

enum MENU_MODE: uint8_t {
    MENU_SELECT,
    MENU_ADJUST,

    MENU_COUNT
};

enum ENCODER_MODE: uint8_t {
    OFF,
    COLOUR,
    ANGLE,
    BRIGHTNESS,
    SPEED,
    EFFECT,

    MODE_COUNT
};

enum EFFECT_MODE: uint8_t {
    HUE_CYCLE,
    WHITE_CHASE,

    EFFECT_COUNT,
};

#endif //LEDCONTROL_ENUM_H
