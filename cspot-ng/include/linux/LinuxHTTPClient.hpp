#pragma once

#include <interfaces/HTTPClient.hpp>
#include <curl/curl.h>
#include <string>

namespace cspot_ng
{
    class LinuxHTTPClient : public HTTPClient
    {
    public:
        LinuxHTTPClient();
        ~LinuxHTTPClient();

        Response get(const std::string& url) override;

    private:
        CURL* m_curl;

        static size_t write_callback(void* data, size_t size, size_t nmemb, void* userdata);
    };
}
