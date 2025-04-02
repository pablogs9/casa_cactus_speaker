#pragma once

#include <string>

#include <ByteArray.hpp>

namespace cspot_ng
{
    struct TCPClient
    {
        virtual bool connect(const std::string& host, uint16_t port) = 0;
        virtual void send(const ByteArray& data) = 0;
        virtual ByteArray receive(size_t max_size = 4096, size_t timeout_ms = 1000) = 0;
        virtual void close() = 0;
    };

};