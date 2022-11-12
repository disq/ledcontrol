#ifndef LEDCONTROL_H
#define LEDCONTROL_H

#include <cstdint>
#include <cmath>
#include <common/pimoroni_common.hpp>
#include "breakout_encoder.hpp"
#include "button.hpp"
#include "PicoLed.hpp"

namespace ledcontrol {

    class LEDControl {
    private:
        enum MENU_MODE : uint8_t {
            MENU_SELECT,
            MENU_ADJUST,

            MENU_COUNT
        };
        const float_t BRIGHTNESS_SCALE = 255;

    public:
        LEDControl();

        enum ENCODER_MODE : uint8_t {
            OFF,
            COLOUR,
            ANGLE,
            BRIGHTNESS,
            SPEED,
            EFFECT,

            MODE_COUNT
        };

        enum EFFECT_MODE : uint8_t {
            HUE_CYCLE,
            WHITE_CHASE,

            EFFECT_COUNT,
        };

        int init(); // returns: 0 for success
        uint32_t loop(); // returns: required sleep value in ms

    private:
        // hardware things
        PicoLed::PicoLedController led_strip;
        pimoroni::Button button_a, button_b;
        pimoroni::I2C i2c;
        pimoroni::BreakoutEncoder enc;

        // state things
        enum EFFECT_MODE effect_mode;
        enum MENU_MODE menu_mode;
        enum ENCODER_MODE mode;
        float_t brightness;
        bool cycle{};
        uint32_t encoder_last_blink;
        bool encoder_blink_state;
        uint32_t start_time{}, stop_time;
        float_t speed{}, hue{}, angle{};
        bool encoder_detected{};


        // private methods
        void cycle_loop(float hue, float t, float angle);
        uint16_t get_paused_time();
        void set_cycle(bool v);
        uint32_t encoder_colour_by_mode(ENCODER_MODE mode);
        void encoder_loop(ENCODER_MODE mode);
        ENCODER_MODE encoder_state_by_mode(ENCODER_MODE mode);
    };
}

#endif //LEDCONTROL_H
