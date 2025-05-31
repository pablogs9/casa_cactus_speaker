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
    {
        uint8_t *buffer = (uint8_t*)heap_caps_malloc(AUDIO_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        stream_buffer_ = xStreamBufferCreateStatic(AUDIO_BUFFER_SIZE, 1, buffer, &stream_buffer_static_);

        raop_buffer_ = (uint8_t*)heap_caps_malloc(RAOP_OUTPUT_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

        if (stream_buffer_ == nullptr)
        {
            ESP_LOGE(TAG, "Failed to create stream buffer");
            // Wait forever if we can't create the buffer
            vTaskDelay(portMAX_DELAY);
        }

        // Start RAOP
        raop_sink_init(
            RAOPPlayer::raop_sink_cmd_handler,
            RAOPPlayer::raop_sink_data_handler,
            "CactusSpeaker",
            wifi_manager.get_ip_bytes(),
            this
        );

        // Create task for audio output
        xTaskCreatePinnedToCore(
            RAOPPlayer::audio_output_task,
            "Audio_Output",
            10 * 1024, // Stack size
            this,
            5,
            &audio_output_task_handle_,
            0);
    }

    static void raop_sink_data_handler(void* args, const u8_t *data, size_t len, u32_t playtime)
    {
        RAOPPlayer& player = *static_cast<RAOPPlayer*>(args);

        while (len > 0)
        {
            size_t pushed = xStreamBufferSend(player.stream_buffer_, data, len, pdMS_TO_TICKS(10));

            if (pushed == 0)
            {
                /* Buffer is full – throw away the oldest half-second */
                constexpr size_t DROP_CHUNK = 44100 * 2 /*ch*/ * sizeof(int16_t) / 2;
                static uint8_t dummy[DROP_CHUNK];
                xStreamBufferReceive(player.stream_buffer_, dummy, std::min(DROP_CHUNK, len), 0);
                ESP_LOGW(TAG, "Stream buffer full, dropping old samples");
                continue;
            }

            data += pushed;
            len  -= pushed;
        }
    }

    static bool raop_sink_cmd_handler(void* cb_args, raop_event_t event, va_list args)
    {
        ESP_LOGI(TAG, "Received event: %s", raop_event_to_string(event));
        RAOPPlayer& player = *static_cast<RAOPPlayer*>(cb_args);

        switch (event)
        {
        case RAOP_SETUP:
        {
			uint8_t **buffer = va_arg(args, uint8_t**);
			size_t *size = va_arg(args, size_t*);

			*size = RAOP_OUTPUT_SIZE;
            *buffer = player.raop_buffer_;

            break;
        }
        default:
            break;
        }
        return true;
    }

    static void audio_output_task(
            void* arg)
    {
        RAOPPlayer& player = *static_cast<RAOPPlayer*>(arg);

        ESP_LOGI(player.TAG, "Audio Output Task Started on Core %d", xPortGetCoreID());

        player.sink.change_sample_rate(44100, 2); // Set default sample rate and channels

        constexpr size_t CHUNK = 1024 * 2 * sizeof(int16_t); // 2kB chunk size (stereo, 16-bit samples)
        uint8_t * local_buf = (uint8_t*)heap_caps_malloc(CHUNK, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

        bool primed = false;

        while (true)
        {
            /* Prime: wait until at least a half of the buffer is filled */
            if (!primed)
            {
                if (xStreamBufferBytesAvailable(player.stream_buffer_) < (AUDIO_BUFFER_SIZE / 2))
                {
                    // ESP_LOGI(TAG, "Waiting for buffer to be primed");
                    vTaskDelay(pdMS_TO_TICKS(10));
                    continue;
                }
                primed = true;
                ESP_LOGI(TAG, "Buffer primed – starting playback");
            }

            /* Pull a chunk – block a little while to keep CPU use low */
            size_t received = xStreamBufferReceive(player.stream_buffer_,
                                                   local_buf,
                                                   CHUNK,
                                                   pdMS_TO_TICKS(0));

            if (received == 0)
            {
                /* Under-run – restart priming */
                primed = false;
                ESP_LOGW(TAG, "Buffer under-run, re-priming");
                continue;
            }

            player.sink.direct_write(local_buf, received);
        }
        /* never returns */

        ESP_LOGI(TAG, "Audio playback complete");
        vTaskDelete(NULL);
    }

    I2SSink & sink;
    TaskHandle_t audio_output_task_handle_;

    // We need 5 seconds of audio buffer for smooth playback
    static constexpr size_t AUDIO_BUFFER_SIZE = 5 * 44100 * 2 * sizeof(int16_t); // 3 seconds, stereo, 16-bit samples
    StreamBufferHandle_t stream_buffer_ = nullptr;
    StaticStreamBuffer_t stream_buffer_static_;

    static constexpr size_t RAOP_OUTPUT_SIZE = 1024 * 10; // 10kB for RAOP output buffer
    uint8_t * raop_buffer_ = nullptr; // Buffer for RAOP data
};
