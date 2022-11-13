#ifndef LEDCONTROL_H
#define LEDCONTROL_H

#include <cstdint>
#include <cmath>
#include <common/pimoroni_common.hpp>
#include "button.hpp"
#include "PicoLed.hpp"
#include "encoder.h"

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

        // state of things
        typedef struct {
            float_t hue;
            float_t angle;
            float_t speed;
            float_t brightness;
            enum EFFECT_MODE effect;
            enum ENCODER_MODE mode;
        } state_t;

        void init(Encoder *e);
        uint32_t loop(); // returns: required sleep value in ms
        void enable_state(state_t p_state);
        state_t get_state();

        // flashy things
        int load_state_from_flash();
        int save_state_to_flash();

        void _enc_switch_cb(bool val);
        void _enc_rotate_cb(signed int counter);

      private:
        PicoLed::PicoLedController led_strip;
        pimoroni::Button button_b;
        Encoder *enc = nullptr;

        state_t state;
        enum MENU_MODE menu_mode;
        bool cycle{};
        uint32_t encoder_last_blink;
        bool encoder_blink_state;
        uint32_t start_time{}, stop_time;

        const char flash_save_magic[8] = "LEDCTRL";
        typedef struct {
            char magic[8];
            size_t state_size;
            state_t state;
        } flash_state_t;

        // private methods
        void cycle_loop(float hue, float t, float angle);
        uint16_t get_paused_time();
        void set_cycle(bool v);
        uint32_t encoder_colour_by_mode(ENCODER_MODE mode);
        void encoder_loop();
        void set_encoder_state();
        void encoder_blink_off();
    };
}

#endif //LEDCONTROL_H
