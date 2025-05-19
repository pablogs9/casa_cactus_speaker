#pragma once

#include <ByteArray.hpp>
#include <ByteUtils.hpp>
#include <ShannonTCPClient.hpp>
#include <LoginBlob.hpp>

#include <protobuf/mercury.pb.h>
#include <protobuf/spirc.pb.h>

#include <cstring>
#include <ctime>

#include <iostream>

namespace cspot_ng
{
    struct MercurySession
    {
        MercurySession(const LoginBlob & blob, ShannonTCPClient & shannon_tcp_client)
            : blob_(blob)
            , shannon_tcp_client_(shannon_tcp_client)
        {
        }

        void spin(const uint8_t & command, const ByteArray & data)
        {
            if (!data.empty())
            {
                std::cout << "Received data: " << (uint32_t)command << " - " << data.size() << " bytes" << std::endl;

                switch (static_cast<RequestType>(command))
                {
                case RequestType::COUNTRY_CODE_RESPONSE:
                {
                    // printf("Received country code response: ");
                    // for (const auto& byte : data)
                    // {
                    //     printf("0x%02X ", byte);
                    // }
                    // printf("\n");

                    // const std::string country_code(data.begin(), data.end());
                    // std::cout << "Country code: " << country_code << std::endl;
                    break;
                }
                case RequestType::PING:
                {
                    // Ping sends server timepoint POSIX in seconds in the payload
                    uint32_t server_time = 0;
                    std::memcpy(&server_time, data.data(), sizeof(uint32_t));
                    // Convert to host byte order
                    server_time = ntohl(server_time);


                    // Calculate time offset
                    std::time_t now = std::time(nullptr);
                    time_offset_ = static_cast<int64_t>(now) - static_cast<int64_t>(server_time);

                    std::cout << "Received ping" << std::endl;
                    std::cout << "Server time: " << server_time << std::endl;
                    std::cout << "Time offset: " << time_offset_ << std::endl;

                    // Send a PONG_ACK response
                    ShannonTCPClient::ShannonPacket pong_packet;
                    pong_packet.command = static_cast<uint8_t>(RequestType::PING_RESPONSE);
                    pong_packet.data = data; // Echo the data back
                    shannon_tcp_client_.send(pong_packet);

                    if(mercury_state_ == MercuryState::WAITING_FIRST_PING)
                    {
                        subscribe_to_mercury();
                        mercury_state_ = MercuryState::SUBSCRIBING_TO_MERCURY_WAITING;
                    }

                    break;
                }
                case RequestType::SEND:
                case RequestType::SUB:
                case RequestType::UNSUB:
                {
                    // std::cout << "Received Mercury SEND/SUB/UNSUB packet" << std::endl;
                    // print_byte_array(data);

                    if(mercury_state_ == MercuryState::SUBSCRIBING_TO_MERCURY_WAITING)
                    {
                        decode_mercury_response(data, true);
                    }
                    else
                    {
                        std::cout << "Received data: " << (uint32_t)command << " - " << data.size() << " bytes" << std::endl;
                    }
                    break;
                    }
                case RequestType::SUBRES: {
                    std::cout << "----------- Received SUBRES packet" << std::endl;
                    std::cout << "Received data: " << (uint32_t)command << " - " << data.size() << " bytes" << std::endl;

                    decode_mercury_response(data, false);
                    break;
                }
                default:
                    break;
                }
            }
        }

    private:

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
            SUBSCRIBING_TO_MERCURY,
            SUBSCRIBING_TO_MERCURY_WAITING,
            SENDING_HELLO,
            SENDING_HELLO_WAITING
        };

        const LoginBlob & blob_;
        ShannonTCPClient & shannon_tcp_client_;

        MercuryState mercury_state_ = MercuryState::WAITING_FIRST_PING;
        uint64_t sequence_id_ = 1;
        uint32_t spirc_sequence_id_ = 0;
        int64_t time_offset_ = 0;

        void subscribe_to_mercury()
        {
            // Create the message
            ByteArray message = {};

            // Serialize sequence ID size
            uint16_t sequence_id_size = sizeof(sequence_id_);
            sequence_id_size = htons(sequence_id_size);
            append_to_byte_array(message, sequence_id_size);

            // Serialize sequence ID
            append_to_byte_array(message, hton64(sequence_id_));
            sequence_id_++;

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
                mercury_state_ = MercuryState::SENDING_HELLO;
                send_hello();
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
            uint16_t sequence_id_size = sizeof(sequence_id_);
            sequence_id_size = htons(sequence_id_size);
            append_to_byte_array(message, sequence_id_size);

            // Serialize sequence ID
            append_to_byte_array(message, hton64(sequence_id_));
            sequence_id_++;

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

            frame.state.context_uri = const_cast<char*>("hm://remote/user/");
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
            frame.seq_nr = spirc_sequence_id_++;
            frame.typ = MessageType_kMessageTypeHello;
            frame.state_update_id = std::time(nullptr) + time_offset_;
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

    };
};