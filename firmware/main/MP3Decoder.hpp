#pragma once

#include <esp_log.h>

#include <decoder/impl/esp_mp3_dec.h>
#include <simple_dec/esp_audio_simple_dec.h>
#include <simple_dec/impl/esp_m4a_dec.h>

#include <RingBuffer.hpp>

class MP3Decoder
{
    static constexpr const char* TAG = "MP3Decoder";

public:

    MP3Decoder()
    {
        esp_audio_err_t ret = ESP_AUDIO_ERR_OK;

        static bool initialized_register = false;

        if (!initialized_register)
        {
            ret = esp_mp3_dec_register();

            if (ret != ESP_AUDIO_ERR_OK)
            {
                ESP_LOGE(TAG, "Failed to register decoder with error %d", ret);

                return;
            }

            initialized_register = true;
        }

        esp_audio_simple_dec_cfg_t config_ = {};
        config_.dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_MP3;

        ret = esp_audio_simple_dec_open(&config_, &handle_);

        if (ret != ESP_AUDIO_ERR_OK)
        {
            return;
        }

        ESP_LOGI(TAG, "MP3 decoder initialized successfully");
    }

    ~MP3Decoder()
    {
        ESP_LOGI(TAG, "Closing MP3 decoder");
        esp_audio_simple_dec_close(handle_);
        ESP_LOGI(TAG, "MP3 decoder closed");
    }

    void process(
            RingBuffer& input,
            RingBuffer& output)
    {
        ESP_LOGD(TAG, "Starting process with input: %zu bytes, output space: %zu bytes",
                input.used_space(), output.free_space());

        esp_audio_simple_dec_raw_t input_frame = {};
        esp_audio_simple_dec_out_t output_frame = {};

        auto read_slot = input.max_read_slot();
        auto write_slot = output.max_write_slot();

        ESP_LOGD(TAG, "Read slot size: %zu, Write slot size: %zu", read_slot.size(), write_slot.size());

        if (read_slot.size() == 0)
        {
            ESP_LOGW(TAG, "No data available to decode");

            return;
        }

        // Process until no more input data or output space
        while (read_slot.size() > 0 && write_slot.size() > 0)
        {
            input_frame.buffer = read_slot.data();
            input_frame.len = read_slot.size();
            input_frame.eos = false;
            input_frame.consumed = 0;

            output_frame.buffer = write_slot.data();
            output_frame.len = write_slot.size();
            output_frame.needed_size = 0;
            output_frame.decoded_size = 0;

            esp_audio_err_t ret = esp_audio_simple_dec_process(handle_, &input_frame, &output_frame);

            input.commit_read(input_frame.consumed);
            output.commit_write(output_frame.decoded_size);

            if (ret != ESP_AUDIO_ERR_OK)
            {
                ESP_LOGE(TAG, "Failed to decode MP3 frame with error %d", ret);
                break;
            }

            read_slot = input.max_read_slot();
            write_slot = output.max_write_slot();
        }
    }

    esp_audio_simple_dec_info_t get_info()
    {
        esp_audio_simple_dec_info_t info = {};
        esp_audio_simple_dec_get_info(handle_, &info);

        return info;
    }

private:

    esp_audio_simple_dec_handle_t handle_ = {};
};
