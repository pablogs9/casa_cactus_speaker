#pragma once

#include <string>

namespace cspot_ng
{
    struct HTTPClient
    {
        struct Response
        {
            std::string body;
            int status_code;
        };

        virtual Response get(const std::string& url) = 0;
    };
};