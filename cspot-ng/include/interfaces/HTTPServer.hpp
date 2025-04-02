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
            QueryMap result;
            size_t pos = 0;

            while (pos < query.size())
            {
                size_t eq_pos = query.find('=', pos);
                if (eq_pos == std::string::npos)
                    break;

                size_t amp_pos = query.find('&', eq_pos);
                if (amp_pos == std::string::npos)
                    amp_pos = query.size();

                std::string key = query.substr(pos, eq_pos - pos);
                std::string value = query.substr(eq_pos + 1, amp_pos - eq_pos - 1);

                result[key] = value;

                pos = amp_pos + 1;
            }

            // Decode URL-encoded keys and values
            for (auto& [key, value] : result)
            {
                // Decode key
                std::string decoded_key;
                for (size_t i = 0; i < key.size(); ++i)
                {
                    if (key[i] == '%')
                    {
                        int hex_value;
                        sscanf(key.substr(i + 1, 2).c_str(), "%x", &hex_value);
                        decoded_key += static_cast<char>(hex_value);
                        i += 2;
                    }
                    else if (key[i] == '+')
                    {
                        decoded_key += ' ';
                    }
                    else
                    {
                        decoded_key += key[i];
                    }
                }

                // Decode value
                std::string decoded_value;
                for (size_t i = 0; i < value.size(); ++i)
                {
                    if (value[i] == '%')
                    {
                        int hex_value;
                        sscanf(value.substr(i + 1, 2).c_str(), "%x", &hex_value);
                        decoded_value += static_cast<char>(hex_value);
                        i += 2;
                    }
                    else if (value[i] == '+')
                    {
                        decoded_value += ' ';
                    }
                    else
                    {
                        decoded_value += value[i];
                    }
                }

                result[decoded_key] = decoded_value;

                // Remove the old key
                if (decoded_key != key)
                {
                    result.erase(key);
                }
            }

            return result;
        }
    };
};