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
            OFF = 0,
            COLOUR,
            ANGLE,
            BRIGHTNESS,
            SPEED,
            EFFECT,

            MODE_COUNT
        };

        enum EFFECT_MODE : uint8_t {
            HUE_CYCLE = 0,
            WHITE_CHASE,

            EFFECT_COUNT,
        };

        static const uint8_t SPEED_COUNT = 5;

        // state of things
        typedef struct {
            float_t hue;
            float_t angle;
            float_t speed;
            float_t brightness;
            enum EFFECT_MODE effect;
            enum ENCODER_MODE mode;
            bool on;
            bool stopped; // if we're not cycling (effective speed is 0)
        } state_t;

        void init(Encoder *e);
        uint32_t loop(); // returns: required sleep value in ms
        void enable_state(state_t p_state);
        state_t get_state();
        void log_state(const char *prefix, state_t s);

        // iot control helpers
        const char* effect_to_str(EFFECT_MODE effect);
        const char* speed_to_str(float_t speed);
        float_t str_to_speed(const char *str);
        int parse_effect_str(const char *str, EFFECT_MODE *effect, float_t *speed);
        size_t get_effect_list(LEDControl::EFFECT_MODE *effects, size_t num_effects);
        size_t get_speed_list(const char **speeds, size_t num_speeds);

        // flashy things
        int load_state_from_flash();
        int save_state_to_flash();

        void set_on_state_change_cb(void (*cb)(state_t new_state)) { _on_state_change_cb = cb; }

      private:
        state_t state;
        uint32_t encoder_last_blink;
        bool encoder_blink_state;
        uint32_t encoder_last_activity;
        uint32_t start_time, stop_time;
        uint32_t transition_start_time;
        uint32_t transition_duration;
        float_t transition_start_brightness;
        PicoLed::PicoLedController led_strip;
        pimoroni::Button button_b;
        pimoroni::Button button_c;
        Encoder *enc = nullptr;

        enum MENU_MODE menu_mode;
        bool cycle{}, cycle_once{};

        const char flash_save_magic[8] = "LEDCTRL";
        typedef struct {
            char magic[8];
            size_t state_size;
            state_t state;
        } flash_state_t;

        void (*_on_state_change_cb)(state_t new_state);

        // private methods
        void cycle_loop(float hue, float t, float angle);
        uint16_t get_paused_time();
        uint8_t get_effective_brightness();
        bool transition_loop(bool force);
        void set_cycle(bool v);
        uint32_t encoder_colour_by_mode(ENCODER_MODE mode);
        void encoder_loop();
        void set_encoder_state();
        void encoder_blink_off();

        // strings
        const char *effect_str[EFFECT_COUNT] = {
            "hue_cycle",
            "white_chase",
        };
        const char *speed_str[SPEED_COUNT] = {
            "stopped",
            "superslow",
            "slow",
            "medium",
            "fast",
        };
    };
}

#endif //LEDCONTROL_H
