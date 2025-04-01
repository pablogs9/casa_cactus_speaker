#pragma once

#include <interfaces/LoginBlob.hpp>
#include <interfaces/HTTPClient.hpp>
#include <interfaces/TCPClient.hpp>
#include <interfaces/Crypto.hpp>

#include <ByteArray.hpp>

namespace cspot_ng
{
    struct Context
    {
        Context(const LoginBlob & blob, HTTPClient & http_client, TCPClient & tcp_client, Crypto & crypto)
            : blob_(blob)
            , http_client_(http_client)
            , tcp_client_(tcp_client)
            , crypto_(crypto)
        {

        }

        bool connect()
        {
            bool ret = connect_to_ap();

            if (ret)
            {
                auto hello_payload = generate_client_hello();
                ContextPacket packet({0x00, 0x04}, hello_payload);
                auto hello_packet = packet.generate();
                tcp_client_.send(hello_packet);

                auto hello_response = tcp_client_.receive();

                auto solved_hello = solve_server_hello(hello_packet, hello_response);
                ContextPacket response_packet({}, solved_hello);
                auto response = response_packet.generate();
                tcp_client_.send(response);



            }

            return ret;
        }

        private:

            ByteArray generate_client_hello()
            {
                crypto_.dh_init();

                auto & public_key = crypto_.public_key();

                ClientHello client_hello_packet;

                std::copy(
                    public_key.begin(),
                    public_key.end(),
                    client_hello_packet.login_crypto_hello.diffie_hellman.gc);

                client_hello_packet.login_crypto_hello.diffie_hellman.server_keys_known = 1;
                client_hello_packet.build_info.product = Product_PRODUCT_CLIENT;
                client_hello_packet.build_info.platform = Platform2_PLATFORM_LINUX_X86;
                client_hello_packet.build_info.version = SPOTIFY_VERSION;
                client_hello_packet.feature_set.autoupdate2 = true;
                client_hello_packet.cryptosuites_supported[0] = Cryptosuite_CRYPTO_SUITE_SHANNON;
                client_hello_packet.padding[0] = 0x1E;

                client_hello_packet.has_feature_set = true;
                client_hello_packet.login_crypto_hello.has_diffie_hellman = true;
                client_hello_packet.has_padding = true;
                client_hello_packet.has_feature_set = true;

                auto nonce = crypto_.generate_random_bytes(16);
                std::copy(nonce.begin(), nonce.end(), client_hello_packet.client_nonce);

                // Encode the ClientHello message
                auto hello_packet = ProtoBuffer<ClientHello>::encode(client_hello_packet, ClientHello_fields);

                return hello_packet;
            }

            ByteArray solve_server_hello(const ByteArray & hello_packet, ByteArray & hello_response)
            {
                auto skip_size = ByteArray(hello_response.begin() + 4, hello_response.end());

                auto response = ProtoBuffer<APResponseMessage>::decode(skip_size, APResponseMessage_fields);

                const auto diffie_key = ByteArray(
                    response.challenge.login_crypto_challenge.diffie_hellman.gs,
                    response.challenge.login_crypto_challenge.diffie_hellman.gs + 96);

                const auto server_key = crypto_.dh_calculate_shared_key(diffie_key);

                hello_response.insert(
                    hello_response.begin(),
                    hello_packet.begin(),
                    hello_packet.end());

                auto result_data = ByteArray(0);

                for (int x = 1; x < 6; x++)
                {
                    auto challenge_vector = ByteArray(1);
                    challenge_vector[0] = x;

                    challenge_vector.insert(challenge_vector.begin(), hello_response.begin(), hello_response.end());
                    auto digest = crypto_.sha1_hmac(server_key, challenge_vector);
                    result_data.insert(result_data.end(), digest.begin(), digest.end());
                }

                auto last_vec = ByteArray(
                    result_data.begin(),
                    result_data.begin() + 0x14);

                auto digest = crypto_.sha1_hmac(last_vec, hello_response);

                ClientResponsePlaintext client_response;
                client_response.login_crypto_response.has_diffie_hellman = true;
                std::copy(
                    digest.begin(),
                    digest.end(),
                    client_response.login_crypto_response.diffie_hellman.hmac);


                // Get send and receive keys
                shan_send_key_ = ByteArray(
                    result_data.begin() + 0x14,
                    result_data.begin() + 0x34);

                shan_recv_key_ = ByteArray(
                    result_data.begin() + 0x34,
                    result_data.begin() + 0x54);

                return ProtoBuffer<ClientResponsePlaintext>::encode(client_response, ClientResponsePlaintext_fields);
            }

            struct ContextPacket
            {
                using Prefix = std::array<uint8_t, 2>;

                ContextPacket(const Prefix& prefix, const ByteArray & data)
                    : prefix_(prefix)
                    , data_(data)
                {}

                ByteArray generate()
                {
                    uint32_t size = prefix_.size() + data_.size() + sizeof(uint32_t);

                    ByteArray packet(size);

                    // Structure: Prefix + Size + Data
                    auto it = packet.begin();
                    std::memcpy(&(*it), prefix_.data(), prefix_.size());
                    it += prefix_.size();
                    std::memcpy(&(*it), &size, sizeof(uint32_t));
                    it += sizeof(uint32_t);
                    std::memcpy(&(*it), data_.data(), data_.size());

                    return packet;
                }



            private:
                const Prefix & prefix_;
                const ByteArray & data_;
            };

            bool connect_to_ap()
            {
                bool ret = false;

                // Load the AP lists
                auto response = http_client_.get("https://apresolve.spotify.com/");

                if (response.status_code == 200)
                {
                    // Parse the response
                    auto json = nlohmann::json::parse(response.body);
                    auto ap_list = json["ap_list"];

                    if (!ap_list.empty())
                    {
                        // Connect to the first AP address
                        ap_address_ = ap_list[0];
                        // Split the address into host and port
                        auto pos = ap_address_.find(':');

                        if (pos != std::string::npos)
                        {
                            ap_port_ = std::stoi(ap_address_.substr(pos + 1));
                            ap_address_ = ap_address_.substr(0, pos);
                        }

                        // Connect to the AP
                        ret = tcp_client_.connect(ap_address_, ap_port_);
                    }
                }

                return ret;
            }

            const LoginBlob & blob_;
            HTTPClient & http_client_;
            TCPClient & tcp_client_;
            Crypto & crypto_;

            std::string ap_address_ = "";
            uint16_t ap_port_ = 0;

            ByteArray shan_send_key_;
            ByteArray shan_recv_key_;
    };
};