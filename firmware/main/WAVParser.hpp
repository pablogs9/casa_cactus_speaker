#pragma once

#include <span>
#include <cstdint>
#include <cstring>
#include "esp_log.h"

class WAVParser
{
public:
    WAVParser(const uint8_t * start, const uint8_t * end)
    : WAVParser(std::span<const uint8_t>(start, end - start))
    {

    }

    WAVParser(const std::span<const uint8_t> data)
        : original_data_(data), data_(data), sample_rate_(0), num_channels_(0), bits_per_sample_(0), data_size_(0)
    {
        parse_header();
        find_data_chunk();
    }

    // Getters for audio properties
    uint32_t get_sample_rate() const { return sample_rate_; }
    uint16_t get_num_channels() const { return num_channels_; }
    uint16_t get_bits_per_sample() const { return bits_per_sample_; }
    uint32_t get_data_size() const { return data_size_; }

    // Methods for consuming the data
    bool has_data() const { return !data_.empty(); }
    std::span<const uint8_t> get_data() const { return data_; }

    // Method to consume N bytes and advance the data pointer
    std::span<const uint8_t> consume_data(size_t bytes_to_consume) {
        if (bytes_to_consume > data_.size()) {
            bytes_to_consume = data_.size();
        }

        auto consumed_data = std::span<const uint8_t>(data_.data(), bytes_to_consume);
        data_ = std::span<const uint8_t>(data_.data() + bytes_to_consume, data_.size() - bytes_to_consume);
        return consumed_data;
    }

    void reset() {
        *this = WAVParser(original_data_);
    }

    void consume_all() {
        data_ = std::span<const uint8_t>();
    }

private:
    // WAV file header
    struct WAVHeader
    {
        char riff[4];               // "RIFF"
        uint32_t file_size;         // File size - 8 bytes
        char wave[4];               // "WAVE"
        char fmt[4];                // "fmt "
        uint32_t fmt_size;          // Size of fmt chunk
        uint16_t audio_format;      // Audio format (1 = PCM)
        uint16_t num_channels;      // Number of channels
        uint32_t sample_rate;       // Sample rate
        uint32_t byte_rate;         // Byte rate
        uint16_t block_align;       // Block align
        uint16_t bits_per_sample;   // Bits per sample
    };

    struct ChunkHeader {
        char id[4];
        uint32_t size;
    };

    WAVHeader header_;

    void parse_header()
    {
        // Check if the data is large enough to contain the header
        if (data_.size() < sizeof(WAVHeader))
        {
            ESP_LOGE("WAVParser", "Data size is too small to contain WAV header");
            return;
        }

        // Copy the header data
        std::memcpy(&header_, data_.data(), sizeof(WAVHeader));

        // Check for "RIFF" and "WAVE" identifiers
        if (std::strncmp(header_.riff, "RIFF", 4) != 0 || std::strncmp(header_.wave, "WAVE", 4) != 0)
        {
            ESP_LOGE("WAVParser", "Invalid WAV file");
            return;
        }

        // Initialize audio properties from header
        sample_rate_ = header_.sample_rate;
        num_channels_ = header_.num_channels;
        bits_per_sample_ = header_.bits_per_sample;

        // Move data pointer past the header
        data_ = std::span<const uint8_t>(data_.data() + sizeof(WAVHeader), data_.size() - sizeof(WAVHeader));

        ESP_LOGI("WAVParser", "Parsed WAV header: Sample Rate: %lu, Channels: %u, Bits per Sample: %u",
                 sample_rate_, num_channels_, bits_per_sample_);
    }

    void find_data_chunk() {
        const uint8_t* current = data_.data();
        const uint8_t* end = data_.data() + data_.size();

        while (current + sizeof(ChunkHeader) <= end) {
            ChunkHeader chunk_header;
            std::memcpy(&chunk_header, current, sizeof(ChunkHeader));
            current += sizeof(ChunkHeader);

            if (std::strncmp(chunk_header.id, "data", 4) == 0) {
                data_size_ = chunk_header.size;
                data_ = std::span<const uint8_t>(current, std::min(static_cast<size_t>(chunk_header.size),
                                                                static_cast<size_t>(end - current)));
                return;
            }

            current += chunk_header.size;
        }

        ESP_LOGE("WAVParser", "Could not find data chunk");
        data_ = std::span<const uint8_t>(); // Empty span
    }

    std::span<const uint8_t> original_data_;
    std::span<const uint8_t> data_;

    uint32_t sample_rate_;
    uint16_t num_channels_;
    uint16_t bits_per_sample_;
    uint32_t data_size_;
};
