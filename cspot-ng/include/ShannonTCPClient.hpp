#pragma once

#include <interfaces/TCPClient.hpp>

#include <ByteArray.hpp>
#include <Shannon.h>

namespace cspot_ng
{
    class ShannonTCPClient
    {
    public:
        ShannonTCPClient(TCPClient& tcp_client)
            : tcp_client_(tcp_client)
            , send_nonce_(0)
            , recv_nonce_(0)
        {
        }

        struct ShannonPacket
        {
            uint8_t command;
            ByteArray data;

            ByteArray get_raw() const
            {
                uint16_t total_size = sizeof(uint8_t) + sizeof(uint16_t) + data.size();

                ByteArray packet(total_size);
                packet[0] = command;

                packet[1] = (data.size() >> 8) & 0xFF;
                packet[2] = data.size() & 0xFF;
                std::memcpy(packet.data() + 3, data.data(), data.size());

                return packet;
            }
        };

        void send(const ShannonPacket& packet)
        {
            static int i = 0;

            // Create a copy of the data that we can encrypt
            ByteArray encrypted_data = packet.get_raw();

            std::cout << "!!!!! CYPHER RAW: " << i << std::endl;
            std::cout << "-------------------------" << std::endl;
            for(size_t i = 0; i < encrypted_data.size(); i++) {
              printf("%02X ", encrypted_data[i]);

              if ((i + 1) % 16 == 0) {
                std::cout << std::endl;
              }
            }
            std::cout << std::endl;

            // Shannon encrypt the packet
            send_cipher_.encrypt(encrypted_data);

            std::cout << "!!!!! CYPHER ENCRYPT: " << i << std::endl;
            std::cout << "-------------------------" << std::endl;
            for(size_t i = 0; i < encrypted_data.size(); i++) {
              printf("%02X ", encrypted_data[i]);

              if ((i + 1) % 16 == 0) {
                std::cout << std::endl;
              }
            }
            std::cout << std::endl;

            // tcp_client_.send(encrypted_data);

            // Generate MAC
            ByteArray mac(4); // MAC_SIZE is 4 bytes
            send_cipher_.finish(mac);

            std::cout << "!!!!! CYPHER MAC: " << i << std::endl;
            std::cout << "-------------------------" << std::endl;
            for(size_t i = 0; i < mac.size(); i++) {
              printf("%02X ", mac[i]);

              if ((i + 1) % 16 == 0) {
                std::cout << std::endl;
              }
            }
            std::cout << std::endl;

            i++;

            // Update the nonce
            send_nonce_ += 1;
            auto nonce_vec = uint32_to_vector(htonl(send_nonce_));
            send_cipher_.nonce(nonce_vec);

            // Send the MAC
            // tcp_client_.send(mac);

            auto complete_msg = ByteArray();
            complete_msg.reserve(encrypted_data.size() + mac.size());
            complete_msg.insert(complete_msg.end(), encrypted_data.begin(), encrypted_data.end());
            complete_msg.insert(complete_msg.end(), mac.begin(), mac.end());
            tcp_client_.send(complete_msg);
        }

        ShannonPacket receive(size_t timeout_ms = 1000)
        {
            // Receive the initial 3 bytes (command + size)
            ByteArray header = tcp_client_.receive(3, timeout_ms);

            if (header.size() < 3)
            {
                // Handle error (could throw exception or return an empty packet)
                std::cerr << "Failed to receive header" << std::endl;
                return ShannonPacket();
            }

            // Decrypt the header
            recv_cipher_.decrypt(header);

            // Extract command and size
            uint16_t size = (header[1] << 8) | header[2];
            uint8_t command = header[0];

            ShannonPacket packet;
            packet.command = command;

            packet.data = tcp_client_.receive(size, timeout_ms);

            // Decrypt the data
            recv_cipher_.decrypt(packet.data);

            // Receive the MAC
            ByteArray mac = tcp_client_.receive(4, timeout_ms);

            // Generate MAC for verification
            ByteArray calculated_mac(4); // MAC_SIZE is 4 bytes
            recv_cipher_.finish(calculated_mac);

            // Verify MAC (in production code, should handle mismatch)
            if (mac != calculated_mac)
            {
                std::cerr << "MAC mismatch" << std::endl;
                // Handle MAC mismatch (could throw exception or log)
                return ShannonPacket();
            }

            // Update the nonce
            recv_nonce_ += 1;
            auto nonce_vec = uint32_to_vector(htonl(recv_nonce_));
            recv_cipher_.nonce(nonce_vec);

            // Return the decrypted data
            return packet;
        }

        void wrap_connection(const ByteArray& send_key, const ByteArray& recv_key)
        {
            // Set keys
            send_cipher_.key(send_key);
            recv_cipher_.key(recv_key);

            // Set initial nonces
            auto initial_nonce = uint32_to_vector(htonl(0));
            send_cipher_.nonce(initial_nonce);
            recv_cipher_.nonce(initial_nonce);

            // Reset nonce counters
            send_nonce_ = 0;
            recv_nonce_ = 0;

            // Mark as initialized
            initialized_ = true;
        }

        bool is_initialized() const
        {
            return initialized_;
        }

    private:
        TCPClient& tcp_client_;
        Shannon send_cipher_;
        Shannon recv_cipher_;

        uint32_t send_nonce_;
        uint32_t recv_nonce_;

        bool initialized_ = false;

        uint32_t htonl(uint32_t value)
        {
            uint16_t test = 0x0102;
            if (*(uint8_t*)&test == 0x01) {
                // Big-endian system
                return value;
            } else {
                // Little-endian system
                return ((value & 0xFF) << 24) |
                       ((value & 0xFF00) << 8) |
                       ((value & 0xFF0000) >> 8) |
                       ((value & 0xFF000000) >> 24);
            }
        }

        ByteArray uint32_to_vector(uint32_t value)
        {
            ByteArray result(4);
            result[0] = (value >> 24) & 0xFF;
            result[1] = (value >> 16) & 0xFF;
            result[2] = (value >> 8) & 0xFF;
            result[3] = value & 0xFF;
            return result;
        }
    };
};
