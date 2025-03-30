#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <esp_log.h>
#include <iot_button.h>
#include <button_gpio.h>

#include <Event.hpp>

class ButtonController
{
    static constexpr const char* TAG = "ButtonController";

    // Pin definitions
    static constexpr gpio_num_t BUTTON  = GPIO_NUM_16;

public:

    ButtonController()
    {
        button_config_t config = {};

        button_gpio_config_t gpio_config = {
            .gpio_num = BUTTON,
            .active_level = 0,
        };

        ESP_ERROR_CHECK(iot_button_new_gpio_device(&config, &gpio_config, &handle_));

        iot_button_register_cb(handle_, BUTTON_SINGLE_CLICK, NULL,
                [](void* button_handle, void* usr_data)
                {
                    EventQueue::get_instance().push(Event::BUTTON_CLICKED);
                }, NULL);

        iot_button_register_cb(handle_, BUTTON_DOUBLE_CLICK, NULL,
                [](void* button_handle, void* usr_data)
                {
                    EventQueue::get_instance().push(Event::BUTTON_DOUBLE_CLICKED);
                }, NULL);

        iot_button_register_cb(handle_, BUTTON_LONG_PRESS_START, NULL,
                [](void* button_handle, void* usr_data)
                {
                    EventQueue::get_instance().push(Event::BUTTON_LONG_CLICKED);
                }, NULL);
    }

    bool pressed()
    {
        return iot_button_get_key_level(handle_);
    }

    ~ButtonController()
    {
        iot_button_delete(handle_);
    }

private:

    button_handle_t handle_;

};
