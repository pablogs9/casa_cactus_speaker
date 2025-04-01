#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cspot_ng
{
    struct Constants
    {
        static constexpr const char * PROTOCOL_VERSION = "2.7.1";
        static constexpr const char * SW_VERSION = "1.0.0";
        static constexpr const char * BRAND_NAME = "cspot";
        static constexpr const char * DEVICE_NAME = "CSpot";
        static constexpr long long SPOTIFY_VERSION = 0x10800000000;
    };
};