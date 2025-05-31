#pragma once

#include <string>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <esp_log.h>

#include <I2SSink.hpp>
#include <RingBuffer.hpp>
#include <Event.hpp>

#include <raop_sink.h>

class RAOPPlayer
{
    static constexpr const char* TAG = "RAOPPlayer";

public:

    RAOPPlayer(
            I2SSink& sink,
            WifiManager& wifi_manager)
        : sink(sink)
        , decoder_to_audio_ring_(1024 * 512, "AUDIO_BUFFER")
    {
        mutex_ = xSemaphoreCreateMutex();

        // Start RAOP
        raop_sink_init(
            RAOPPlayer::raop_sink_cmd_handler,
            RAOPPlayer::raop_sink_data_handler,
            "CactusSpeaker",
            wifi_manager.get_ip_bytes(),
            this
        );

        // // Create task for audio output
        // xTaskCreatePinnedToCore(
        //     RAOPPlayer::audio_output_task,
        //     "Audio_Output",
        //     40 * 1024, // Stack size
        //     this,
        //     10,
        //     &audio_output_task_handle_,
        //     0);
    }

    static void raop_sink_data_handler(void* args, const u8_t *data, size_t len, u32_t playtime)
    {
        RAOPPlayer& player = *static_cast<RAOPPlayer*>(args);

        ESP_LOGI(TAG, "RAOP sink data handler called with %zu bytes", len);

        RingBuffer aux(const_cast<u8_t*>(data), len, "AUX_BUFFER");

        player.sink.write(aux);

        return;


        // u8_t * data_ptr = const_cast<u8_t*>(data);

        // ESP_LOGI(TAG, "Received %zu bytes of audio data", len);

        // // Write data to the audio ring buffer
        // while (len > 0)
        // {
        //     xSemaphoreTake(player.mutex_, portMAX_DELAY);
        //     auto write_slot = player.decoder_to_audio_ring_.max_write_slot();

        //     const size_t write_size = std::min(write_slot.size(), len);

        //     // ESP_LOGI(TAG, "Writing %zu bytes to audio ring buffer", write_size);

        //     if (write_size > 0)
        //     {
        //         // Copy data to the write slot
        //         std::memcpy(write_slot.data(), data_ptr, write_size);
        //         player.decoder_to_audio_ring_.commit_write(write_size);

        //         // Move the pointer and reduce the length
        //         data_ptr += write_size;
        //         len -= write_size;

        //         // Notify the audio output task that new data is available
        //         xTaskNotify(player.audio_output_task_handle_, 0, eNoAction);
        //     }
        //     xSemaphoreGive(player.mutex_);

        //     if (write_size == 0 && len > 0)
        //     {
        //         vTaskDelay(pdMS_TO_TICKS(50)); // Avoid busy waiting if no space available
        //     }
        // }

    }

    static bool raop_sink_cmd_handler(void* cb_args, raop_event_t event, va_list args)
    {
        ESP_LOGI(TAG, "Received event: %d", static_cast<int>(event));
        return true;
    }

    // static void audio_output_task(
    //         void* arg)
    // {
    //     RAOPPlayer& player = *static_cast<RAOPPlayer*>(arg);

    //     ESP_LOGI(player.TAG, "Audio Output Task Started on Core %d", xPortGetCoreID());

    //     player.sink.change_sample_rate(44100, 2); // Set default sample rate and channels

    //     while (true)
    //     {
    //         // Wait for notification from RAOP sink
    //         uint32_t notification_value = 0;
    //         xTaskNotifyWait(0, 0, &notification_value, portMAX_DELAY);

    //         // Feed the audio sink with data from the decoder
    //         xSemaphoreTake(player.mutex_, portMAX_DELAY);
    //         // ESP_LOGI(TAG, "Feeding audio sink with data from decoder ring buffer of size: %zu bytes",
    //         //          player.decoder_to_audio_ring_.used_space());
    //         player.sink.write(player.decoder_to_audio_ring_);

    //         xSemaphoreGive(player.mutex_);
    //     }

    //     ESP_LOGI(TAG, "Audio playback complete");
    //     vTaskDelete(NULL);
    // }

    I2SSink & sink;
    TaskHandle_t audio_output_task_handle_;

    SemaphoreHandle_t mutex_;
    RingBuffer decoder_to_audio_ring_;
};
