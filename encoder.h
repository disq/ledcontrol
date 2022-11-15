#ifndef ENCODER_H
#define ENCODER_H

#include <cstdio>
#include <cstdint>

class Encoder {
  private:
    uint pin_enc_a, pin_enc_b, pin_enc_sw; // connect encoder pin C to ground
    uint led_pins[3], in_pins[3];
    uint8_t led_vals[3];
    bool leds_active_low;
    float led_brightness;

    int rotary_counter = 0;
    signed int read_rotary();

    uint32_t _last_switch_time = 0;
    bool has_data = false;
    bool is_clicked = false;

public:
    Encoder();
    void init(uint p_pin_led_r, uint p_pin_led_g, uint p_pin_led_b, uint p_pin_enc_a, uint p_pin_enc_b, uint p_pin_enc_sw, bool p_leds_active_low = true);
    void set_leds(uint8_t r, uint8_t g, uint8_t b);
    void set_brightness(float brightness);

    bool get_interrupt_flag() { return has_data; }
    void clear_interrupt_flag() { has_data = false; }
    signed int read() { return rotary_counter; }
    void clear() { rotary_counter = 0; }

    bool get_clicked() { return is_clicked; }
    void clear_clicked() { is_clicked = false; }

    void _gpio_callback(uint gpio, uint32_t events);
};

extern Encoder encoder;
void _encoder_gpio_callback(uint gpio, uint32_t events);

#endif //ENCODER_H
