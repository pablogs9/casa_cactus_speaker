#pragma once

#include <interfaces/TCPClient.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>
#include <string>
#include <stdexcept>

namespace cspot_ng
{
    class LinuxTCPClient : public TCPClient
    {
    public:
        LinuxTCPClient();
        ~LinuxTCPClient();

        bool connect(const std::string& host, uint16_t port) override;
        void send(const ByteArray& data) override;
        ByteArray receive(size_t max_size, size_t timeout_ms = 0) override;
        void close() override;

    private:
        int m_socket;
        bool m_connected;
    };
}
