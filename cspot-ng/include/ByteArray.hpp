#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cspot_ng
{
    using ByteArray = std::vector<uint8_t>;

    template<typename T>
    void append_to_byte_array(ByteArray& byte_array, const T& value)
    {
        const uint8_t* data = reinterpret_cast<const uint8_t*>(&value);
        byte_array.insert(byte_array.end(), data, data + sizeof(T));
    }

    template<>
    inline void append_to_byte_array<ByteArray>(ByteArray& byte_array, const ByteArray& value)
    {
        byte_array.insert(byte_array.end(), value.begin(), value.end());
    }

    template<typename T>
    T extract_from_byte_array(ByteArray& byte_array)
    {
        T value;
        uint8_t* data = reinterpret_cast<uint8_t*>(&value);
        std::copy(byte_array.begin(), byte_array.begin() + sizeof(T), data);
        byte_array.erase(byte_array.begin(), byte_array.begin() + sizeof(T));
        return value;
    }

    inline void print_byte_array(const ByteArray& byte_array)
    {
        for(size_t i = 0; i < byte_array.size(); ++i)
        {
            printf("%02X ", byte_array[i]);

            if ((i + 1) % 16 == 0)
            {
                printf("\n");
            }
        }
        if (byte_array.size() % 16 != 0)
        {
            printf("\n");
        }
    }
};