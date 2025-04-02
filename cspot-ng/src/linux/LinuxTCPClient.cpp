#include <linux/LinuxTCPClient.hpp>
#include <iostream>
#include <cstring>

#include <netinet/tcp.h>
#include <errno.h>

namespace cspot_ng
{
    LinuxTCPClient::LinuxTCPClient()
        : m_socket(-1)
        , m_connected(false)
    {
    }

    LinuxTCPClient::~LinuxTCPClient()
    {
        if (m_connected) {
            close();
        }
    }

    bool LinuxTCPClient::connect(const std::string& host, uint16_t port)
    {
        struct addrinfo h, *airoot, *ai;
        std::memset(&h, 0, sizeof(h));
        h.ai_family = AF_UNSPEC; // Allow both IPv4 and IPv6
        h.ai_socktype = SOCK_STREAM;
        h.ai_protocol = IPPROTO_TCP;

        // Convert port to string
        std::string portStr = std::to_string(port);

        // Lookup host using the more modern getaddrinfo
        if (getaddrinfo(host.c_str(), portStr.c_str(), &h, &airoot) != 0) {
            std::cerr << "Error in getaddrinfo: " << strerror(errno) << std::endl;
            return false;
        }

        // Try each address until we successfully connect
        for (ai = airoot; ai; ai = ai->ai_next) {
            if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
                continue;

            m_socket = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
            if (m_socket < 0)
                continue;

            if (::connect(m_socket, (struct sockaddr*)ai->ai_addr, ai->ai_addrlen) != -1) {
                // Set socket options
                struct timeval tv;
                tv.tv_sec = 3;
                tv.tv_usec = 0;
                setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
                setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

                int flag = 1;
                setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));

                m_connected = true;
                freeaddrinfo(airoot);
                return true;
            }

            ::close(m_socket);
            m_socket = -1;
        }

        freeaddrinfo(airoot);
        std::cerr << "Failed to connect to " << host << ":" << port << std::endl;
        return false;
    }

    void LinuxTCPClient::send(const ByteArray& data)
    {
        if (!m_connected) {
            throw std::runtime_error("Not connected");
        }

        std::cout << "Sending " << data.size() << " bytes" << std::endl;

        size_t total_sent = 0;
        int retries = 0;

        while (total_sent < data.size()) {
            const size_t remaining = data.size() - total_sent;
            size_t max_to_send = remaining < 64 ? remaining : 64; // Same 64-byte chunking as cspot

            ssize_t sent = ::send(m_socket, data.data() + total_sent, max_to_send, 0);
            if (sent <= 0) {
                if (errno == EAGAIN || errno == ETIMEDOUT) {
                    // Handle timeout like cspot does
                    if (retries++ > 4) {
                        throw std::runtime_error("Send timeout");
                    }
                    continue; // Retry
                } else if (errno == EINTR) {
                    continue; // Interrupted, try again
                } else {
                    // Other errors
                    if (retries++ > 4) {
                        throw std::runtime_error(std::string("Send error: ") + strerror(errno));
                    }
                    continue; // Retry a few times
                }
            }

            total_sent += sent;
        }
    }

    ByteArray LinuxTCPClient::receive(size_t max_size, size_t timeout_ms)
    {
        if (!m_connected) {
            throw std::runtime_error("Not connected");
        }

        std::cout << "Receiving up to " << max_size << " bytes" << std::endl;
        ByteArray buffer(max_size);
        ssize_t bytes_read = 0;
        unsigned int idx = 0;
        int retries = 0;

        while (idx < max_size) {
            bytes_read = recv(m_socket, buffer.data() + idx, max_size - idx, 0);

            std::cout << "Received " << bytes_read << " bytes" << std::endl;
            if (bytes_read <= 0) {
                if (errno == EAGAIN || errno == ETIMEDOUT) {
                    // Mimicking cspot's timeout handling
                    if (retries++ > 4) {
                        throw std::runtime_error("Receive timeout");
                    }
                } else if (errno == EINTR) {
                } else if (bytes_read == 0) {
                    // Connection closed
                    m_connected = false;
                    if (idx == 0) {
                        return ByteArray(); // Nothing received
                    }
                    break; // Return what we have so far
                } else {
                    // Other errors
                    if (retries++ > 4) {
                        throw std::runtime_error(std::string("Receive error: ") + strerror(errno));
                    }
                }
            }

            idx += bytes_read;

            break;
            if (timeout_ms == 0) break; // If no timeout specified, just return what we got
        }

        buffer.resize(idx);
        return buffer;
    }

    void LinuxTCPClient::close()
    {
        if (m_connected && m_socket != -1) {
            shutdown(m_socket, SHUT_RDWR);
            ::close(m_socket);
            m_socket = -1;
            m_connected = false;
        }
    }
}
