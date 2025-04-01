#pragma once

#include <map>
#include <string>
#include <functional>
#include <cstdint>

namespace cspot_ng
{
    struct HTTPServer
    {
        struct HTTPResponse
        {
            std::string body;
            std::map<std::string, std::string> headers;
            int status;
        };

        using RequestHandler = std::function<HTTPResponse(const std::string&)>;

        virtual void initialize(uint16_t port) = 0;

        virtual void register_get_handler(
            const std::string& path,
            RequestHandler handler) = 0;

        virtual void register_post_handler(
            const std::string& path,
            RequestHandler handler) = 0;

        using QueryMap = std::map<std::string, std::string>;

        static QueryMap decode_query(const std::string& query)
        {
            return QueryMap(); // Placeholder for actual query decoding
        }
    };
};