#pragma once

#include <string>

#include <esp_log.h>

#include <esp_http_client.h>
#include <esp_crt_bundle.h>

#include <RingBuffer.hpp>

class HTTPStream
{
    static constexpr const char* TAG = "HTTPStream";

public:

    HTTPStream(
            const std::string& url)
    {
        esp_http_client_config_t config = {};

        config.url = url.c_str();
        config.method = HTTP_METHOD_GET;
        config.crt_bundle_attach = esp_crt_bundle_attach;

        handle_ = esp_http_client_init(&config);

        if (handle_ == NULL)
        {
            ESP_LOGE(TAG, "Failed to initialize HTTP client");

            return;
        }

        esp_err_t err = esp_http_client_open(handle_, 0);

        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));

            return;
        }

        content_length_ = esp_http_client_fetch_headers(handle_);

        int status_code = esp_http_client_get_status_code(handle_);
        ESP_LOGI(TAG, "HTTP status code: %d", status_code);

        if (status_code != 200)
        {
            ESP_LOGE(TAG, "HTTP error: status code %d", status_code);

            return;
        }

        if (content_length_ < 0)
        {
            ESP_LOGW(TAG, "Content length not provided by server, streaming mode enabled");
            // In streaming mode, we'll keep reading until the server closes the connection
            unread_length_ = INT64_MAX; // Set to a large value to indicate streaming mode
        }
        else
        {
            unread_length_ = content_length_;
            ESP_LOGI(TAG, "HTTP content length: %lld", content_length_);
        }
    }

    ~HTTPStream()
    {
        ESP_LOGI(TAG, "Cleaning up HTTP client");
        esp_http_client_close(handle_);
        esp_http_client_cleanup(handle_);
        ESP_LOGI(TAG, "HTTP client cleaned up");
    }

    int64_t available_data()
    {
        // If we're in streaming mode (content_length < 0) or still have data to read
        if (unread_length_ > 0 && unread_length_ != INT64_MAX)
        {
            return unread_length_;
        }
        else if (unread_length_ == INT64_MAX)
        {
            // In streaming mode, return a positive value to indicate data might be available
            return 1;
        }
        else
        {
            return 0;
        }
    }

    void read_http_stream(
            RingBuffer& buffer)
    {
        uint32_t chunk_read = 0;
        uint32_t total_read = 0;

        do
        {
            auto write_slot = buffer.max_write_slot();

            if (write_slot.size() == 0)
            {
                ESP_LOGW(TAG, "Buffer full, can't read more data");
                break;
            }

            chunk_read = esp_http_client_read(handle_, (char*)write_slot.data(), write_slot.size());

            if (chunk_read > 0)
            {
                buffer.commit_write(chunk_read);
                total_read += chunk_read;

                // Only decrement if we're not in streaming mode
                if (unread_length_ != INT64_MAX)
                {
                    unread_length_ -= chunk_read;
                }
            }
            else if (chunk_read == 0)
            {
                // End of data
                if (unread_length_ == INT64_MAX)
                {
                    // In streaming mode, this means the connection is closed
                    unread_length_ = 0;
                    ESP_LOGI(TAG, "End of HTTP stream reached");
                }

                break;
            }
            else
            {
                // Error occurred
                ESP_LOGE(TAG, "Error reading from HTTP stream: %lu", chunk_read);
                break;
            }
        } while (chunk_read > 0 && buffer.max_write_slot().size() > 0);

        ESP_LOGD(TAG, "Total read: %lu bytes, remaining: %lld", total_read, unread_length_);
    }

private:

    esp_http_client_handle_t handle_ = {};
    int64_t content_length_ = 0;
    int64_t unread_length_ = 0;
};
