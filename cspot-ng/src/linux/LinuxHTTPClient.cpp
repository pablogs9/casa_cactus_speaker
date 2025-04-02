#include <linux/LinuxHTTPClient.hpp>
#include <stdexcept>
#include <iostream>

namespace cspot_ng
{
    LinuxHTTPClient::LinuxHTTPClient()
    {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        m_curl = curl_easy_init();
        if (!m_curl) {
            throw std::runtime_error("Failed to initialize libcurl");
        }
    }

    LinuxHTTPClient::~LinuxHTTPClient()
    {
        if (m_curl) {
            curl_easy_cleanup(m_curl);
        }
        curl_global_cleanup();
    }

    size_t LinuxHTTPClient::write_callback(void* data, size_t size, size_t nmemb, void* userdata)
    {
        size_t real_size = size * nmemb;
        std::string* str = static_cast<std::string*>(userdata);
        str->append(static_cast<char*>(data), real_size);
        return real_size;
    }

    HTTPClient::Response LinuxHTTPClient::get(const std::string& url)
    {
        Response response;
        std::string response_body;

        curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, LinuxHTTPClient::write_callback);
        curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &response_body);
        curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, 30L);

        CURLcode res = curl_easy_perform(m_curl);

        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
            response.status_code = 0;
            return response;
        }

        long http_code = 0;
        curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &http_code);

        response.body = response_body;
        response.status_code = static_cast<int>(http_code);

        return response;
    }
}
