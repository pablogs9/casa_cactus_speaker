#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

enum class Event
{
    BUTTON_CLICKED,
    BUTTON_DOUBLE_CLICKED,
    BUTTON_LONG_CLICKED,
    TURNED_RIGHT,
    TURNED_LEFT,
    SONG_END
};

class EventQueue
{
public:

    static EventQueue& get_instance()
    {
        static EventQueue queue;

        return queue;
    }

    void push(
            Event event)
    {
        xQueueSend(queue_, &event, 0);
    }

    void push_from_isr(
            Event event)
    {
        xQueueSendFromISR(queue_, &event, 0);
    }

    Event pop()
    {
        Event event;
        xQueueReceive(queue_, &event, portMAX_DELAY);

        return event;
    }

private:

    EventQueue()
    {
        queue_ = xQueueCreate(10, sizeof(Event));
    }

    QueueHandle_t queue_;
};
