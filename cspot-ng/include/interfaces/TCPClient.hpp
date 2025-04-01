#pragma once

#include <string>

#include <ByteArray.hpp>

namespace cspot_ng
{
    struct TCPClient
    {
        virtual bool connect(const std::string& host, uint16_t port) = 0;
        virtual void send(const ByteArray& data) = 0;
        virtual ByteArray receive(size_t timeout_ms = 0) = 0;
        virtual void close() = 0;
    };

};