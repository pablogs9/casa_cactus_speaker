#pragma once

#include <cstdint>
#include <cmath>

#include "driver/i2s_std.h"

#include <RingBuffer.hpp>
#include <WAVParser.hpp>

extern const uint8_t beep_start[] asm("_binary_beep_wav_start");
extern const uint8_t beep_end[]   asm("_binary_beep_wav_end");

extern const uint8_t start_beep_start[] asm("_binary_start_beep_wav_start");
extern const uint8_t start_beep_end[]   asm("_binary_start_beep_wav_end");

extern const uint8_t volume_beep_start[] asm("_binary_volume_beep_wav_start");
extern const uint8_t volume_beep_end[]   asm("_binary_volume_beep_wav_end");

class I2SSink
{
    static constexpr const char* TAG = "I2SSink";

public:

    static constexpr int8_t MIN_VOLUME = -100;

    I2SSink()
    : beep_(beep_start, beep_end)
    , start_beep_(start_beep_start, start_beep_end)
    , volume_beep_(volume_beep_start, volume_beep_end)
    {
        i2s_chan_config_t chan_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);

        // Add some delay for better stability
        chan_config.dma_desc_num = 8;
        chan_config.dma_frame_num = 1023;
        chan_config.auto_clear = true;

        dma_buffer_size_ = chan_config.dma_frame_num * 2 * 16 / 8;

        ESP_ERROR_CHECK(i2s_new_channel(&chan_config, &handle_, NULL));

        ESP_LOGI("I2S", "I2S channel created, DMA buffer size: %d", dma_buffer_size_);

        i2s_std_config_t config = {};

        config.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate_);
        config.slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);

        // Set the clock to a multiple of the sample rate for stability
        config.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;

        config.gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,    // some codecs may require mclk signal, this example doesn't need it
            .bclk = GPIO_NUM_12,
            .ws   = GPIO_NUM_13,
            .dout = GPIO_NUM_11,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        };

        ESP_ERROR_CHECK(i2s_channel_init_std_mode(handle_, &config));

        ESP_ERROR_CHECK(i2s_channel_enable(handle_));

        // Consume all data from WAVParser
        beep_.consume_all();
        start_beep_.consume_all();
        volume_beep_.consume_all();
    }

    ~I2SSink()
    {
        ESP_LOGI(TAG, "Deleting I2S channel");
        ESP_ERROR_CHECK(i2s_channel_disable(handle_));
        ESP_LOGI(TAG, "I2S channel disabled");
        ESP_ERROR_CHECK(i2s_del_channel(handle_));
        ESP_LOGI(TAG, "I2S channel deleted");
    }

    void change_sample_rate(
            uint32_t sample_rate,
            uint8_t channels)
    {
        if (sample_rate == sample_rate_ && channels == channels_)
        {
            return;
        }

        ESP_LOGI(TAG, "Changing sample rate from %lu to %lu, channels from %d to %d",
                sample_rate_, sample_rate, channels_, channels);

        // Disable channel before reconfiguring
        ESP_ERROR_CHECK(i2s_channel_disable(handle_));

        i2s_std_config_t config = {};
        config.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate);
        config.slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                        channels == 1 ? I2S_SLOT_MODE_MONO : I2S_SLOT_MODE_STEREO);

        ESP_ERROR_CHECK(i2s_channel_reconfig_std_clock(handle_, &config.clk_cfg));
        ESP_ERROR_CHECK(i2s_channel_reconfig_std_slot(handle_, &config.slot_cfg));
        ESP_ERROR_CHECK(i2s_channel_enable(handle_));

        sample_rate_ = sample_rate;
        channels_ = channels;
    }

    void set_volume(
            int8_t volume_db)
    {
        volume_db_ = volume_db;

        ESP_LOGI(TAG, "Setting volume to %d dB", volume_db_);

        if (volume_db <= MIN_VOLUME)
        {
            volume_scale_ = 0;
        }
        else
        {
            // Use float for calculation to avoid overflow
            float scale_factor = std::pow(10, volume_db / 20.0);
            // Use uint32_t to avoid overflow for high volumes
            volume_scale_ = static_cast<uint32_t>(32768 * scale_factor);
        }
    }

    int8_t get_volume()
    {
        return volume_db_;
    }

    void volume_up()
    {
        int8_t new_volume = volume_db_ + 1;

        if (new_volume > 0)
        {
            new_volume = 0;
        }

        set_volume(new_volume);
    }

    void volume_down()
    {
        int8_t new_volume = volume_db_ - 1;

        if (new_volume < MIN_VOLUME)
        {
            new_volume = MIN_VOLUME;
        }

        set_volume(new_volume);
    }

    enum class BeepType
    {
        START,
        VOLUME,
        BEEP
    };

    void beep(
            BeepType type)
    {
        switch (type)
        {
            case BeepType::START:
                start_beep_.reset();
                break;
            case BeepType::VOLUME:
                volume_beep_.reset();
                break;
            case BeepType::BEEP:
                beep_.reset();
                break;
        }
    }

    void mute()
    {
        ESP_LOGI(TAG, "Muting audio");
        muted_ = true;
    }

    void unmute()
    {
        ESP_LOGI(TAG, "Unmuting audio");
        muted_ = false;
    }

    void toggle_mute()
    {
        if (muted_)
        {
            unmute();
        }
        else
        {
            mute();
        }
    }

    void write(
            RingBuffer& data)
    {
        auto read_slot = data.max_read_slot();

        // If not multiple of 4 -> error
        if (read_slot.size() % 4 != 0)
        {
            ESP_LOGE(TAG, "Data size not multiple of 4");

            return;
        }

        while (read_slot.size() > 0)
        {
            size_t wrote = 0;

            read_slot = std::span<uint8_t>(read_slot.data(), std::min(read_slot.size(), dma_buffer_size_));

            // Apply volume on both channels
            if (VOLUME_SCALE_0DB != volume_scale_)
            {
                for (size_t i = 0; i + 3 < read_slot.size(); i += 4)
                {
                    // Safer access with explicit alignment check
                    if (reinterpret_cast<uintptr_t>(&read_slot[i]) % 2 != 0 ||
                            reinterpret_cast<uintptr_t>(&read_slot[i + 2]) % 2 != 0)
                    {
                        ESP_LOGW(TAG, "Unaligned audio data detected");
                        continue;
                    }

                    int16_t& left = *reinterpret_cast<int16_t*>(&read_slot[i]);
                    int16_t& right = *reinterpret_cast<int16_t*>(&read_slot[i + 2]);

                    // Fixed-point multiplication
                    left = (static_cast<int32_t>(left) * volume_scale_) >> 15;
                    right = (static_cast<int32_t>(right) * volume_scale_) >> 15;
                }
            }

            // Handle muting
            if (muted_)
            {
                memset(read_slot.data(), 0, read_slot.size());
            }

            // Mix with beep data
            auto beep_data = beep_.consume_data(read_slot.size());
            if (beep_data.size() > 0)
            {
                ESP_LOGD(TAG, "Mixing beep data");
                for (size_t i = 0; i < beep_data.size(); i += 4)
                {
                    const int16_t& left = *reinterpret_cast<const int16_t*>(&beep_data[i]);
                    const int16_t& right = *reinterpret_cast<const int16_t*>(&beep_data[i + 2]);

                    int16_t& left_out = *reinterpret_cast<int16_t*>(&read_slot[i]);
                    int16_t& right_out = *reinterpret_cast<int16_t*>(&read_slot[i + 2]);

                    left_out += (static_cast<int32_t>(left));
                    right_out += (static_cast<int32_t>(right));

                }
            }

            // Mix with start beep data
            beep_data = start_beep_.consume_data(read_slot.size());
            if (beep_data.size() > 0)
            {
                ESP_LOGD(TAG, "Mixing start beep data");
                for (size_t i = 0; i < beep_data.size(); i += 4)
                {
                    const int16_t& left = *reinterpret_cast<const int16_t*>(&beep_data[i]);
                    const int16_t& right = *reinterpret_cast<const int16_t*>(&beep_data[i + 2]);

                    int16_t& left_out = *reinterpret_cast<int16_t*>(&read_slot[i]);
                    int16_t& right_out = *reinterpret_cast<int16_t*>(&read_slot[i + 2]);

                    left_out += (static_cast<int32_t>(left));
                    right_out += (static_cast<int32_t>(right));

                }
            }

            // Mix with volume beep data
            beep_data = volume_beep_.consume_data(read_slot.size());
            if (beep_data.size() > 0)
            {
                ESP_LOGD(TAG, "Mixing volume beep data");
                for (size_t i = 0; i < beep_data.size(); i += 4)
                {
                    const int16_t& left = *reinterpret_cast<const int16_t*>(&beep_data[i]);
                    const int16_t& right = *reinterpret_cast<const int16_t*>(&beep_data[i + 2]);

                    int16_t& left_out = *reinterpret_cast<int16_t*>(&read_slot[i]);
                    int16_t& right_out = *reinterpret_cast<int16_t*>(&read_slot[i + 2]);

                    left_out += (static_cast<int32_t>(left));
                    right_out += (static_cast<int32_t>(right));

                }
            }

            if (ESP_OK == i2s_channel_write(handle_, read_slot.data(), read_slot.size(), &wrote, portMAX_DELAY))
            {
                ESP_LOGD(TAG, "Wrote %d bytes", wrote);
                ESP_LOGD(TAG, "  - In seconds: %f", (wrote / 2.0) / (sample_rate_ * 1.0));
                data.commit_read(wrote);
                read_slot = data.max_read_slot();
            }
            else
            {
                ESP_LOGE(TAG, "Error writing to I2S");
                break;
            }
        }

    }

private:
    WAVParser beep_;
    WAVParser start_beep_;
    WAVParser volume_beep_;

    i2s_chan_handle_t handle_;
    uint32_t sample_rate_ = 44100;
    uint8_t channels_ = 2;

    size_t dma_buffer_size_;

    int8_t volume_db_ = 0;
    bool muted_ = false;
    static constexpr uint32_t VOLUME_SCALE_0DB = 32768;
    uint32_t volume_scale_ = VOLUME_SCALE_0DB;
};
