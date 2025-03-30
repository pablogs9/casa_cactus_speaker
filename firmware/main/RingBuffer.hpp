#pragma once

#include <cstdint>
#include <span>
#include <esp_log.h>
#include <algorithm>
#include <cstring>

class RingBuffer
{
public:

    RingBuffer(
            size_t size,
            std::string name = "")
        : buffer_(new uint8_t[size])
        , size_(size)
        , read_pos_(0)
        , write_pos_(0)
        , available_(0)
        , name_("RingBuffer " + name)
    {
    }

    ~RingBuffer()
    {
        delete[] buffer_;
    }

    std::span<uint8_t> max_write_slot()
    {
        size_t size = max_contiguous_size_to_write();

        // Avoid writing the last byte to prevent read and write pointers from overlapping.
        return std::span<uint8_t>(write_position(), size);
    }

    void commit_write(
            size_t size)
    {
        // Check that the requested size does not exceed the contiguous free space.
        if (size > max_contiguous_size_to_write())
        {
            ESP_LOGE(name_.c_str(), "Trying to commit more data than available contiguous space");

            return;
        }

        // Also check that we are not trying to write more than the overall available space.
        if (size > free_space())
        {
            ESP_LOGE(name_.c_str(), "Not enough space in buffer to commit write");

            return;
        }

        // Update the write pointer and available data count.
        write_pos_ = (write_pos_ + size) % size_;
        available_ += size;
    }

    std::span<uint8_t> max_read_slot()
    {
        size_t size = max_contiguous_size_to_read();

        return std::span<uint8_t>(read_position(), size);
    }

    void commit_read(
            size_t size)
    {
        if (size > available_)
        {
            ESP_LOGE(name_.c_str(), "Trying to commit more data than available");

            return;
        }

        if (size > max_contiguous_size_to_read())
        {
            ESP_LOGE(name_.c_str(), "Trying to commit more contiguous data than available");

            return;
        }

        read_pos_ = (read_pos_ + size) % size_;
        available_ -= size;

        // If we have read all data, reset pointers
        if (available_ == 0)
        {
            read_pos_ = write_pos_ = 0;
        }
    }

    size_t size() const
    {
        return size_;
    }

    size_t free_space() const
    {
        return (size_ - 1) - available_;
    }

    size_t used_space() const
    {
        return available_;
    }

private:

    // Return the maximum contiguous space available to write.
    size_t max_contiguous_size_to_write() const
    {
        if (available_ >= size_ - 1)
        {
            return 0;
        }

        if (write_pos_ >= read_pos_)
        {
            // The free space from write_pos_ to the end of the buffer.
            // If we reach the end, consider the beginning of the buffer.
            if (write_pos_ == size_ - 1)
            {
                return (read_pos_ == 0) ? 0 : read_pos_ - 1;
            }
            else
            {
                return std::min(size_ - write_pos_ - 1, (size_ - 1) - available_);
            }
        }
        else
        {
            // When write pointer is behind read pointer,
            // the free space is the gap between them.
            return read_pos_ - write_pos_ - 1;
        }
    }

    uint8_t* write_position() const
    {
        return buffer_ + write_pos_;
    }

    // Return the maximum contiguous data available to read.
    size_t max_contiguous_size_to_read() const
    {
        if (available_ == 0)
        {
            return 0;
        }

        if (write_pos_ >= read_pos_)
        {
            return write_pos_ - read_pos_;
        }
        else
        {
            // Data is split at the end of the buffer.
            return size_ - read_pos_;
        }
    }

    uint8_t* read_position() const
    {
        return buffer_ + read_pos_;
    }

private:

    uint8_t* buffer_;           // The actual buffer
    size_t size_;               // Total size of the buffer
    size_t read_pos_;           // Position where to read next
    size_t write_pos_;          // Position where to write next
    size_t available_;          // Number of bytes available to read

    std::string name_;
};
