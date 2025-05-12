#pragma once

#include <interfaces/HTTPClient.hpp>
#include <interfaces/TCPClient.hpp>
#include <interfaces/Crypto.hpp>

#include <LoginBlob.hpp>
#include <ByteUtils.hpp>
#include <ByteArray.hpp>
#include <Constants.hpp>
#include <ProtoBuf.hpp>
#include <ShannonTCPClient.hpp>

#include <protobuf/keyexchange.pb.h>
#include <protobuf/authentication.pb.h>
#include <protobuf/mercury.pb.h>
#include <protobuf/spirc.pb.h>

namespace cspot_ng
{
    struct Context
    {
        Context(const LoginBlob & blob, HTTPClient & http_client, TCPClient & tcp_client, Crypto & crypto)
            : blob_(blob)
            , http_client_(http_client)
            , tcp_client_(tcp_client)
            , crypto_(crypto)
            , shannon_tcp_client_(tcp_client)
        {
            crypto_.dh_init();
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

                auto solved_hello = solve_ap_hello(hello_packet, hello_response);
                ContextPacket response_packet({}, solved_hello);
                auto response = response_packet.generate();
                tcp_client_.send(response);

                // Wrap the TCP connection with Shannon encryption
                shannon_tcp_client_.wrap_connection(shan_send_key_, shan_recv_key_);
            }

            return ret;
        }

        ByteArray authenticate()
        {
            if (!shannon_tcp_client_.is_initialized())
            {
                std::cerr << "Shannon connection not established" << std::endl;
                return {};
            }

            constexpr uint8_t LOGIN_REQUEST_COMMAND = 0xAB;
            constexpr uint8_t AUTH_SUCCESSFUL_COMMAND = 0xAC;
            constexpr uint8_t AUTH_DECLINED_COMMAND = 0xAD;

            // Generate the authentication request
            ShannonTCPClient::ShannonPacket packet;
            packet.command = LOGIN_REQUEST_COMMAND;
            packet.data = generate_auth_request();

            shannon_tcp_client_.send(packet);

            // Receive the response
            auto response = shannon_tcp_client_.receive();

            if (response.command == AUTH_SUCCESSFUL_COMMAND)
            {
                APWelcome welcome = APWelcome_init_default;

                // Decode the response
                ProtoBuffer<APWelcome>::decode(response.data, APWelcome_fields, welcome);

                return ByteArray(
                    welcome.reusable_auth_credentials.bytes,
                    welcome.reusable_auth_credentials.bytes +
                        welcome.reusable_auth_credentials.size);
            }
            else if (response.command == AUTH_DECLINED_COMMAND)
            {
                return {};
            }
            else if (response.data.empty())
            {
                std::cerr << "Authentication failed: empty response" << std::endl;
                return {};
            }
            else
            {
                // Handle other response commands if needed
                std::cerr << "Unknown response command: " << static_cast<int>(response.command) << std::endl;
                return {};
            }
        }

        enum class RequestType : uint8_t {
            SUB = 0xB3,
            UNSUB = 0xB4,
            SUBRES = 0xB5,
            SEND = 0xB2,
            GET = 0xFF,  // Shitty workaround, it's value is actually same as SEND
            PING = 0x04,
            PING_RESPONSE = 0x49,
            PONG_ACK = 0x4A,
            AUDIO_CHUNK_REQUEST_COMMAND = 0x08,
            AUDIO_CHUNK_SUCCESS_RESPONSE = 0x09,
            AUDIO_CHUNK_FAILURE_RESPONSE = 0x0A,
            AUDIO_KEY_REQUEST_COMMAND = 0x0C,
            AUDIO_KEY_SUCCESS_RESPONSE = 0x0D,
            AUDIO_KEY_FAILURE_RESPONSE = 0x0E,
            COUNTRY_CODE_RESPONSE = 0x1B,
          };

        enum class MercuryState
        {
            WAITING_FIRST_PING = 0,
            GO_TO_SUBSCRIBE_TO_MERCURY,
            WAITING_FOR_MERCURY_RESPONSE,
            GO_TO_SEND_HELLO,
            WAITING_FOR_HELLO_RESPONSE,
        };

        MercuryState mercury_state_ = MercuryState::WAITING_FIRST_PING;
        uint64_t sequence_id = 1;
        uint32_t spirc_sequence_id = 0;
        int64_t time_offset = 0;

        void subscribe_to_mercury()
        {
            // Create the message
            ByteArray message = {};

            // Serialize sequence ID size
            uint16_t sequence_id_size = sizeof(sequence_id);
            sequence_id_size = htons(sequence_id_size);
            append_to_byte_array(message, sequence_id_size);

            // Serialize sequence ID
            append_to_byte_array(message, hton64(sequence_id));
            sequence_id++;

            // Serialize a 0x01 byte
            uint8_t zero_byte = 0x01;
            append_to_byte_array(message, zero_byte);

            // Serialize the number of payloads (only the header in this case)
            uint16_t payload_count = 1;
            payload_count = htons(payload_count);
            append_to_byte_array(message, payload_count);

            // CREATE THE PAYLOAD
            Header mercury_header = {};

            const std::string uri = "hm://remote/user/" + blob_.username() + "/";
            mercury_header.has_uri = true;
            protobuffer_set_string(uri, mercury_header.uri);

            const std::string method = "SUB";
            mercury_header.has_method = true;
            protobuffer_set_string(method, mercury_header.method);

            auto encoded_header = ProtoBuffer<Header>::encode(mercury_header, Header_fields);

            // Serialize the payload size
            uint16_t payload_size = encoded_header.size();
            payload_size = htons(payload_size);
            append_to_byte_array(message, payload_size);

            // Append the payload
            append_to_byte_array(message, encoded_header);

            // Print the message for debugging
            std::cout << "Sending subscribe message: " << std::endl;
            print_byte_array(message);

            // Send the message
            shannon_tcp_client_.send(ShannonTCPClient::ShannonPacket{
                static_cast<uint8_t>(RequestType::SUB),
                message});
        }

        void parse_spirc_frame(const ByteArray & data)
        {
            Frame frame = Frame_init_default;

            auto track_fn = [](pb_istream_t *stream, const pb_field_t *field, void **arg) -> bool
            {
                bool eof = false;
                pb_wire_type_t wire_type;
                pb_istream_t substream;
                uint32_t tag;

                while (!eof) {
                    if (!pb_decode_tag(stream, &wire_type, &tag, &eof)) {
                    // Decoding failed and not eof
                    if (!eof) {
                        return false;
                    }
                    // EOF
                    } else {
                    switch (tag) {
                        case TrackRef_uri_tag:
                        case TrackRef_context_tag:
                        case TrackRef_gid_tag:
                        {
                            // Make substream
                            if (!pb_make_string_substream(stream, &substream)) {

                                return false;
                            }

                            uint8_t* destBuffer = nullptr;

                            // Handle GID
                            ByteArray output_buffer;
                            if (tag == TrackRef_gid_tag) {
                                output_buffer.resize(substream.bytes_left);
                                destBuffer = &output_buffer[0];
                            } else if (tag == TrackRef_context_tag) {
                                output_buffer.resize(substream.bytes_left);

                                destBuffer = reinterpret_cast<uint8_t*>(&output_buffer[0]);
                            } else if (tag == TrackRef_uri_tag) {
                                output_buffer.resize(substream.bytes_left);

                                destBuffer = reinterpret_cast<uint8_t*>(&output_buffer[0]);
                            }

                            if (!pb_read(&substream, destBuffer, substream.bytes_left)) {
                                return false;
                            }

                            // Close substream
                            if (!pb_close_string_substream(stream, &substream)) {
                                return false;
                            }

                            // Print the decoded value
                            if (tag == TrackRef_gid_tag) {
                                std::cout << "TrackRef GID: ";

                                for (const auto & byte : output_buffer) {
                                    printf("%02X", byte);
                                }
                                std::cout << std::endl;
                            } else if (tag == TrackRef_context_tag) {
                                std::cout << "TrackRef context: " << std::string(reinterpret_cast<char*>(output_buffer.data()), output_buffer.size()) << std::endl;
                            } else if (tag == TrackRef_uri_tag) {
                                std::cout << "TrackRef URI: " << std::string(reinterpret_cast<char*>(output_buffer.data()), output_buffer.size()) << std::endl;
                            }

                            break;
                        }
                        case TrackRef_queued_tag: {
                            uint32_t queuedValue;

                            // Decode boolean
                            if (!pb_decode_varint32(stream, &queuedValue)) {
                                return false;
                            }

                            // Cast down to bool
                            bool queued = (bool)queuedValue;
                            std::cout << "TrackRef queued: " << queued << std::endl;
                            break;
                        }
                        default:
                            // Field not known, skip
                            pb_skip_field(stream, wire_type);

                        break;
                    }
                    }
                }

               return true;
            };

            frame.state.track.funcs.decode = track_fn;

            ProtoBuffer<Frame>::decode(data, Frame_fields, frame);

            // Print the parsed frame for debugging
            std::cout << "Parsed SPIRC frame: " << static_cast<int>(frame.typ) << std::endl;

        }

        void decode_mercury_response(const ByteArray & data, bool goto_hello)
        {
            ByteArray payload = data;

            // Decode sequence ID size
            uint16_t sequence_id_size = extract_from_byte_array<uint16_t>(payload);
            sequence_id_size = ntohs(sequence_id_size);
            std::cout << "Sequence ID size: " << sequence_id_size << std::endl;

            // Decode sequence ID
            uint64_t sequence_id_local = extract_from_byte_array<uint64_t>(payload);
            sequence_id_local = ntoh64(sequence_id_local);
            std::cout << "Sequence ID: " << sequence_id_local << std::endl;

            // Decode dummy byte
            uint8_t dummy_byte = extract_from_byte_array<uint8_t>(payload);
            std::cout << "Dummy byte: " << static_cast<int>(dummy_byte) << std::endl;

            // Decode the number of payloads
            uint16_t payload_count = extract_from_byte_array<uint16_t>(payload);
            payload_count = ntohs(payload_count);
            std::cout << "Payload count: " << payload_count << std::endl;

            // DECODE THE PAYLOAD
            Header mercury_header = {};

            // Decode the payload size
            uint16_t header_size = extract_from_byte_array<uint16_t>(payload);
            header_size = ntohs(header_size);
            std::cout << "Header size: " << header_size << std::endl;

            // Decode the payload
            Header decoded_header = Header_init_default;
            ProtoBuffer<Header>::decode(payload, Header_fields, decoded_header);
            std::cout << "Decoded header: " << std::endl;
            if (decoded_header.has_uri)
            {
                std::cout << "URI: " << decoded_header.uri << std::endl;
            }
            if (decoded_header.has_method)
            {
                std::cout << "Method: " << decoded_header.method << std::endl;
            }

            // Skip header_size bytes
            payload_count--;
            payload.erase(payload.begin(), payload.begin() + header_size);

            while(payload_count > 0)
            {
                // Decode the payload size
                uint16_t payload_size = extract_from_byte_array<uint16_t>(payload);
                payload_size = ntohs(payload_size);
                std::cout << "Extra payload size: " << payload_size << std::endl;

                ByteArray frame_payload(payload.begin(), payload.begin() + payload_size);
                payload.erase(payload.begin(), payload.begin() + payload_size);

                // Decode the payload
                parse_spirc_frame(frame_payload);

                payload_count--;
            }

            if(goto_hello)
            {
                mercury_state_ = MercuryState::GO_TO_SEND_HELLO;
            }
        }

        void add_capability(Frame & frame, size_t & capability_index, CapabilityType type, int value, std::vector<std::string> strings = std::vector<std::string>())
        {
            frame.device_state.capabilities[capability_index].has_typ = true;
            frame.device_state.capabilities[capability_index].typ = type;

            if (value != -1) {
                frame.device_state.capabilities[capability_index].intValue[0] = value;
                frame.device_state.capabilities[capability_index].intValue_count = 1;
            } else {
                frame.device_state.capabilities[capability_index].intValue_count = 0;
            }

            for (int i = 0; i < strings.size(); i++) {
                protobuffer_set_string(strings[i], frame.device_state.capabilities[capability_index].stringValue[i]);
            }

            frame.device_state.capabilities[capability_index].stringValue_count = strings.size();

            capability_index += 1;
        }

        void send_hello()
        {
            // Create the message
            ByteArray message = {};

            // Serialize sequence ID size
            uint16_t sequence_id_size = sizeof(sequence_id);
            sequence_id_size = htons(sequence_id_size);
            append_to_byte_array(message, sequence_id_size);

            // Serialize sequence ID
            append_to_byte_array(message, hton64(sequence_id));
            sequence_id++;

            // Serialize a 0x01 byte
            uint8_t zero_byte = 0x01;
            append_to_byte_array(message, zero_byte);

            // Serialize the number of payloads (header + Frame in this case)
            uint16_t payload_count = 2;
            payload_count = htons(payload_count);
            append_to_byte_array(message, payload_count);

            // CREATE THE PAYLOAD
            Header mercury_header = {};

            const std::string uri = "hm://remote/user/" + blob_.username() + "/";
            mercury_header.has_uri = true;
            protobuffer_set_string(uri, mercury_header.uri);

            const std::string method = "SEND";
            mercury_header.has_method = true;
            protobuffer_set_string(method, mercury_header.method);

            auto encoded_header = ProtoBuffer<Header>::encode(mercury_header, Header_fields);

            // Serialize the payload size
            uint16_t payload_size = encoded_header.size();
            payload_size = htons(payload_size);
            append_to_byte_array(message, payload_size);

            // Append the payload
            append_to_byte_array(message, encoded_header);

            // ADD EXTRA PAYLOADS
            Frame frame = {};

            // This part is the same
            frame.ident = strdup(blob_.device_id().c_str());
            frame.protocol_version = const_cast<char*>(Constants::PROTOCOL_VERSION);

            frame.state.has_position_ms = true;
            frame.state.position_ms = 0;

            frame.state.status = PlayStatus_kPlayStatusStop;
            frame.state.has_status = true;

            frame.state.position_measured_at = 0;
            frame.state.has_position_measured_at = true;

            frame.state.context_uri = "hm://remote/user/";
            frame.state.shuffle = false;
            frame.state.has_shuffle = true;

            frame.state.repeat = false;
            frame.state.has_repeat = true;

            frame.device_state.sw_version = const_cast<char*>(Constants::SW_VERSION);

            frame.device_state.is_active = false;
            frame.device_state.has_is_active = true;

            frame.device_state.can_play = true;
            frame.device_state.has_can_play = true;

            frame.device_state.volume = 10;
            frame.device_state.has_volume = true;

            frame.device_state.name = const_cast<char*>(blob_.device_name().c_str());

            // Prepare player's capabilities
            size_t capability_index = 0;
            add_capability(frame, capability_index, CapabilityType_kCanBePlayer, 1);
            add_capability(frame, capability_index, CapabilityType_kDeviceType, 4);
            add_capability(frame, capability_index, CapabilityType_kGaiaEqConnectId, 1);
            add_capability(frame, capability_index, CapabilityType_kSupportsLogout, 0);
            add_capability(frame, capability_index, CapabilityType_kSupportsPlaylistV2, 1);
            add_capability(frame, capability_index, CapabilityType_kIsObservable, 1);
            add_capability(frame, capability_index, CapabilityType_kVolumeSteps, 64);
            add_capability(frame, capability_index, CapabilityType_kSupportedContexts, -1,
                            std::vector<std::string>({"album", "playlist", "search",
                                                    "inbox", "toplist", "starred",
                                                    "publishedstarred", "track"}));
            add_capability(frame, capability_index, CapabilityType_kSupportedTypes, -1,
                            std::vector<std::string>(
                                {"audio/track", "audio/episode", "audio/episode+track"}));
            frame.device_state.capabilities_count = 8;

            // This part changes
            frame.version = 1;
            frame.seq_nr = spirc_sequence_id++;
            frame.typ = MessageType_kMessageTypeHello;
            frame.state_update_id = std::time(nullptr) + time_offset;
            frame.has_version = true;
            frame.has_seq_nr = true;
            frame.recipient_count = 0;
            frame.has_state = true;
            frame.has_device_state = true;
            frame.has_typ = true;
            frame.has_state_update_id = true;

            auto encoded_frame = ProtoBuffer<Frame>::encode(frame, Frame_fields);

            std::cout << "Encoded frame size: " << encoded_frame.size() << std::endl;

            // Serialize the payload size
            uint16_t frame_payload_size = encoded_frame.size();
            frame_payload_size = htons(frame_payload_size);
            append_to_byte_array(message, frame_payload_size);

            // Append the payload
            append_to_byte_array(message, encoded_frame);

            // Print the message for debugging
            std::cout << "Sending hello message: " << std::endl;
            print_byte_array(message);

            // Send the message
            shannon_tcp_client_.send(ShannonTCPClient::ShannonPacket{
                static_cast<uint8_t>(RequestType::SEND),
                message});
        }

        void spin()
        {

            // Subscribe to the mercury stream after first ping
            if (mercury_state_ == MercuryState::GO_TO_SUBSCRIBE_TO_MERCURY)
            {
                subscribe_to_mercury();
                mercury_state_ = MercuryState::WAITING_FOR_MERCURY_RESPONSE;
            }
            else if (mercury_state_ == MercuryState::GO_TO_SEND_HELLO)
            {
                send_hello();
                mercury_state_ = MercuryState::WAITING_FOR_HELLO_RESPONSE;
            }


            if (shannon_tcp_client_.is_initialized())
            {
                auto received_data = shannon_tcp_client_.receive();

                if (!received_data.data.empty())
                {
                    std::cout << "Received data: " << (uint32_t)received_data.command << " - " << received_data.data.size() << " bytes" << std::endl;

                    switch (static_cast<RequestType>(received_data.command))
                    {
                    case RequestType::COUNTRY_CODE_RESPONSE:
                    {    printf("Received country code response: ");
                        for (const auto& byte : received_data.data)
                        {
                            printf("0x%02X ", byte);
                        }
                        printf("\n");

                        // This is ascii print as ascii
                        std::string country_code(received_data.data.begin(), received_data.data.end());
                        std::cout << "Country code: " << country_code << std::endl;
                        break;
                    }
                    case RequestType::PING:
                    {
                        if(mercury_state_ == MercuryState::WAITING_FIRST_PING)
                        {
                            mercury_state_ = MercuryState::GO_TO_SUBSCRIBE_TO_MERCURY;
                        }

                        std::cout << "Received ping" << std::endl;
                        // Ping sends server timepoint POSIX in seconds in the payload
                        uint32_t server_time = 0;
                        std::memcpy(&server_time, received_data.data.data(), sizeof(uint32_t));
                        // Convert to host byte order
                        server_time = ntohl(server_time);
                        std::cout << "Server time: " << server_time << std::endl;

                        // Calculate time offset
                        std::time_t now = std::time(nullptr);   // same as std::time(0)
                        time_offset = static_cast<int64_t>(now) - static_cast<int64_t>(server_time);
                        std::cout << "Time offset: " << time_offset << std::endl;

                        // Send a PONG_ACK response
                        ShannonTCPClient::ShannonPacket pong_packet;
                        pong_packet.command = static_cast<uint8_t>(RequestType::PING_RESPONSE);
                        pong_packet.data = received_data.data; // Echo the data back
                        shannon_tcp_client_.send(pong_packet);
                        break;
                    }
                    case RequestType::SEND:
                    case RequestType::SUB:
                    case RequestType::UNSUB:
                    {
                        std::cout << "Received Mercury SEND/SUB/UNSUB packet" << std::endl;

                        print_byte_array(received_data.data);

                        if(mercury_state_ == MercuryState::WAITING_FOR_MERCURY_RESPONSE)
                        {
                            decode_mercury_response(received_data.data, true);
                        }
                        else
                        {
                            std::cout << "Received data: " << (uint32_t)received_data.command << " - " << received_data.data.size() << " bytes" << std::endl;
                        }
                        break;
                        }
                    case RequestType::SUBRES: {
                        std::cout << "----------- Received SUBRES packet" << std::endl;
                        std::cout << "Received data: " << (uint32_t)received_data.command << " - " << received_data.data.size() << " bytes" << std::endl;

                        decode_mercury_response(received_data.data, false);
                        break;
                        }
                    default:
                        break;
                    }
                }

            }
        }

        private:

            static void protobuffer_set_string(const std::string& string, char* dest) {
                string.copy(dest, string.size());
                dest[string.size()] = '\0';
            }

            ByteArray generate_auth_request()
            {
                ClientResponseEncrypted auth_request = ClientResponseEncrypted_init_default;

                // Set the username
                protobuffer_set_string(blob_.username(), auth_request.login_credentials.username);

                std::copy(
                    blob_.auth_data().begin(),
                    blob_.auth_data().end(),
                    auth_request.login_credentials.auth_data.bytes);

                auth_request.login_credentials.auth_data.size = blob_.auth_data().size();

                auth_request.login_credentials.typ = static_cast<AuthenticationType>(blob_.auth_type());

                auth_request.system_info.cpu_family = CpuFamily_CPU_UNKNOWN;
                auth_request.system_info.os = Os_OS_UNKNOWN;

                protobuffer_set_string("cspot-player", auth_request.system_info.system_information_string);

                // Fake device ID
                protobuffer_set_string("142137fd329622137a14901634264e6f332e2411", auth_request.system_info.device_id);
                protobuffer_set_string("cspot-1.1", auth_request.version_string);
                auth_request.has_version_string = true;

                auto encoded_request = ProtoBuffer<ClientResponseEncrypted>::encode(
                    auth_request, ClientResponseEncrypted_fields);

                return encoded_request;
            }

            ByteArray generate_client_hello()
            {
                auto & public_key = crypto_.public_key();

                ClientHello client_hello_packet = ClientHello_init_default;

                std::copy(
                    public_key.begin(),
                    public_key.end(),
                    client_hello_packet.login_crypto_hello.diffie_hellman.gc);

                client_hello_packet.login_crypto_hello.diffie_hellman.server_keys_known = 1;
                client_hello_packet.build_info.product = Product_PRODUCT_CLIENT;
                client_hello_packet.build_info.platform = Platform2_PLATFORM_LINUX_X86;
                client_hello_packet.build_info.version = Constants::SPOTIFY_VERSION;
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

            ByteArray solve_ap_hello(const ByteArray & hello_packet, ByteArray & hello_response)
            {
                auto skip_size = ByteArray(hello_response.begin() + 4, hello_response.end());

                APResponseMessage response = APResponseMessage_init_default;
                ProtoBuffer<APResponseMessage>::decode(skip_size, APResponseMessage_fields, response);

                if (!response.has_challenge)
                {
                    std::cerr << "No challenge in response" << std::endl;
                    return {};
                }

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

                ClientResponsePlaintext client_response = ClientResponsePlaintext_init_default;
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
                using Prefix = std::vector<uint8_t>;

                ContextPacket(const ByteArray& prefix, const ByteArray & data)
                    : prefix_(prefix)
                    , data_(data)
                {}

                ByteArray generate()
                {
                    uint32_t size = prefix_.size() + data_.size() + sizeof(uint32_t);

                    ByteArray packet(size);

                    // Structure: Prefix + Size + Data
                    auto it = packet.begin();
                    if (prefix_.size() > 0) {
                        std::memcpy(&(*it), prefix_.data(), prefix_.size());
                        it += prefix_.size();
                    }

                    // Check endianness at runtime
                    const uint16_t test_endianness = 1;
                    if (*(uint8_t*)&test_endianness == 0) {
                        // Big-endian system, no conversion needed
                        std::memcpy(&(*it), &size, sizeof(uint32_t));
                    } else {
                        // Little-endian system, use converted value
                        // Serialize size in big-endian format regardless of machine endianness
                        uint32_t size_be = ((size & 0xFF) << 24) |
                            ((size & 0xFF00) << 8) |
                            ((size & 0xFF0000) >> 8) |
                            ((size & 0xFF000000) >> 24);
                        std::memcpy(&(*it), &size_be, sizeof(uint32_t));
                    }
                    it += sizeof(uint32_t);

                    std::memcpy(&(*it), data_.data(), data_.size());

                    return packet;
                }



            private:
                const Prefix prefix_;
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

            ShannonTCPClient shannon_tcp_client_;
    };
};