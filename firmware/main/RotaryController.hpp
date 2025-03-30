#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_log.h>
#include <driver/gpio.h>
#include <iot_knob.h>

#include <Event.hpp>

class RotaryController
{
    static constexpr const char* TAG = "RotaryController";

    static constexpr gpio_num_t ENCODER_A = GPIO_NUM_41;
    static constexpr gpio_num_t ENCODER_B = GPIO_NUM_38;

public:

    RotaryController()
    {
        knob_config_t config = {};
        config.default_direction = 0;
        config.gpio_encoder_a = ENCODER_A;
        config.gpio_encoder_b = ENCODER_B;

        knob_ = iot_knob_create(&config);

        iot_knob_register_cb(knob_, KNOB_RIGHT, knob_left_cb, this);
        iot_knob_register_cb(knob_, KNOB_LEFT, knob_right_cb, this);
    }

    ~RotaryController()
    {
        iot_knob_delete(knob_);
    }

private:

    static void knob_left_cb(
            void* arg,
            void* data)
    {
        EventQueue::get_instance().push(Event::TURNED_LEFT);
    }

    static void knob_right_cb(
            void* arg,
            void* data)
    {
        EventQueue::get_instance().push(Event::TURNED_RIGHT);
    }

    knob_handle_t knob_;
};
