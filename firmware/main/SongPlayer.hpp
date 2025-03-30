#pragma once

#include <string>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <esp_log.h>

#include <HTTPStream.hpp>
#include <MP3Decoder.hpp>
#include <I2SSink.hpp>
#include <RingBuffer.hpp>
#include <Event.hpp>

class SongPlayer
{
    static constexpr const char* TAG = "SongPlayer";

public:

    SongPlayer(
            const std::string& url,
            I2SSink& sink)
        : stream(url)
        , sink(sink)
        , http_to_decoder_ring_(1024 * 16, "HTTP_BUFFER")
        , decoder_to_audio_ring_(1024 * 512, "AUDIO_BUFFER")
    {
        mutex_ = xSemaphoreCreateMutex();

        // Create task for HTTP decoding
        xTaskCreatePinnedToCore(
            SongPlayer::http_decoder_task,
            "HTTP_Decoder",
            8192,
            this,
            5,
            &http_decoder_task_handle_,
            0
            );

        // Create task for audio output
        xTaskCreatePinnedToCore(
            SongPlayer::audio_output_task,
            "Audio_Output",
            8192,
            this,
            5,
            &audio_output_task_handle_,
            1
            );
    }

    ~SongPlayer()
    {
        // Stop stream
        force_stop_ = true;

        vTaskResume(http_decoder_task_handle_);
        vTaskResume(audio_output_task_handle_);

        // Wait while task exit
        while (!is_finished_)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        vSemaphoreDelete(mutex_);
        ESP_LOGI(TAG, "SongPlayer destroyed");
    }

    bool finished() const
    {
        return is_finished_;
    }

private:

    static void http_decoder_task(
            void* arg)
    {

        SongPlayer& player = *static_cast<SongPlayer*>(arg);

        ESP_LOGI(player.TAG, "HTTP Decoder Task Started on Core %d", xPortGetCoreID());

        while ((player.stream.available_data() > 0) && (player.force_stop_ == false))
        {
            // Fetch HTTP data
            player.stream.read_http_stream(player.http_to_decoder_ring_);

            // Decode MP3 data - protect access to decoder_to_audio_ring
            xSemaphoreTake(player.mutex_, portMAX_DELAY);
            player.decoder.process(player.http_to_decoder_ring_, player.decoder_to_audio_ring_);
            player.audio_format_ = player.decoder.get_info();
            player.sink.change_sample_rate(player.audio_format_.sample_rate, player.audio_format_.channel);
            xSemaphoreGive(player.mutex_);

            // Give other tasks a chance to run
            taskYIELD();
        }

        // Direct to task notify end of streaming
        xTaskNotify(player.audio_output_task_handle_, 0, eNoAction);

        ESP_LOGI(TAG, "Streaming and decoding complete");
        vTaskDelete(NULL);
    }

    static void audio_output_task(
            void* arg)
    {

        SongPlayer& player = *static_cast<SongPlayer*>(arg);

        ESP_LOGI(player.TAG, "Audio Output Task Started on Core %d", xPortGetCoreID());

        // End flag
        bool end_of_stream = false;

        while (!end_of_stream)
        {
            // Check if streaming is done
            uint32_t notification_value = 0;

            if (pdTRUE == xTaskNotifyWait(0, 0, &notification_value, 0))
            {
                // This shall be done first to allow taking last data from the decoder
                end_of_stream = true;
            }

            // Feed the audio sink with data from the decoder
            xSemaphoreTake(player.mutex_, portMAX_DELAY);
            player.sink.write(player.decoder_to_audio_ring_);
            xSemaphoreGive(player.mutex_);

            // Give other tasks a chance to run
            taskYIELD();
        }

        player.is_finished_ = true;

        EventQueue::get_instance().push(Event::SONG_END);

        ESP_LOGI(TAG, "Audio playback complete");
        vTaskDelete(NULL);
    }

    HTTPStream stream;
    MP3Decoder decoder;
    I2SSink & sink;

    SemaphoreHandle_t mutex_;
    TaskHandle_t http_decoder_task_handle_;
    TaskHandle_t audio_output_task_handle_;

    RingBuffer http_to_decoder_ring_;
    RingBuffer decoder_to_audio_ring_;

    esp_audio_simple_dec_info_t audio_format_;

    bool is_finished_ = false;
    bool force_stop_ = false;
};
