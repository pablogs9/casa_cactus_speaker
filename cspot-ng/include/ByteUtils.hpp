#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cspot_ng
{
    static inline bool is_big_endian()
    {
        uint16_t test = 0x0102;
        return *(uint8_t*)&test == 0x01;
    }

    uint32_t htonl(uint32_t value)
    {
        if (is_big_endian()) {
            // Big-endian system
            return value;
        } else {
            // Little-endian system
            return ((value & 0xFF) << 24) |
                   ((value & 0xFF00) << 8) |
                   ((value & 0xFF0000) >> 8) |
                   ((value & 0xFF000000) >> 24);
        }
    }

    uint16_t ntohs(uint16_t value)
    {
        if (is_big_endian()) {
            // Big-endian system
            return value;
        } else {
            // Little-endian system
            return ((value & 0xFF) << 8) | ((value & 0xFF00) >> 8);
        }
    }

    uint16_t htons(uint16_t value)
    {
        if (is_big_endian()) {
            // Big-endian system
            return value;
        } else {
            // Little-endian system
            return ((value & 0xFF) << 8) | ((value & 0xFF00) >> 8);
        }
    }

    uint32_t ntohl(uint32_t value)
    {
        if (is_big_endian()) {
            // Big-endian system
            return value;
        } else {
            // Little-endian system
            return ((value & 0xFF) << 24) |
                   ((value & 0xFF00) << 8) |
                   ((value & 0xFF0000) >> 8) |
                   ((value & 0xFF000000) >> 24);
        }
    }

    uint64_t hton64(uint64_t value) {
        if (is_big_endian()) {
          return value;
        } else {
          uint32_t high_part = htonl((uint32_t)(value >> 32));
          uint32_t low_part = htonl((uint32_t)(value & 0xFFFFFFFFLL));
          return (((uint64_t)low_part) << 32) | high_part;
        }
      }

    uint64_t ntoh64(uint64_t value) {
        if (is_big_endian()) {
          return value;
        } else {
          uint32_t high_part = ntohl((uint32_t)(value >> 32));
          uint32_t low_part = ntohl((uint32_t)(value & 0xFFFFFFFFLL));
          return (((uint64_t)low_part) << 32) | high_part;
        }
      }
};