
#include <string>
#include <map>
#include <functional>

#include "json.hpp"

#include "protobuf/keyexchange.pb.h"
#include "pb_encode.h"
#include "pb_decode.h"


namespace cspot_ng
{
    struct Crypto
    {
        using ByteArray = std::vector<uint8_t>;

        virtual ByteArray decode_base64(const std::string& input) = 0;
        virtual ByteArray dh_calculate_shared_key(const ByteArray& remote_key) = 0;

        virtual void sha1_init() = 0;
        virtual void sha1_update(const ByteArray& data) = 0;
        virtual ByteArray sha1_final() = 0;

        virtual ByteArray sha1_hmac(const ByteArray& key, const ByteArray& data) = 0;

        virtual void aes_ctr_xcrypt(
            const ByteArray& key,
            ByteArray& iv,
            ByteArray& data) = 0;

        virtual ByteArray pbkdf2_hmac_sha1(
            const ByteArray& password,
            const ByteArray& salt,
            size_t iterations,
            size_t key_length) = 0;

        virtual void aes_ecb_decrypt(
            const ByteArray& key,
            ByteArray& data) = 0;

        virtual void dh_init() = 0;
        virtual ByteArray & public_key() = 0;
        virtual ByteArray & private_key() = 0;

        virtual ByteArray generate_random_bytes(size_t length) = 0;
    };

    struct MDNSProvided
    {
        virtual void initialize() = 0;

        virtual void set_hostname(const std::string& hostname) = 0;

        virtual void register_service(
            const std::string& name,
            const std::string& type,
            const std::string& proto,
            const std::string& host,
            u_int16_t port,
            const std::map<std::string, std::string>& properties) = 0;
    };

    struct HTTPServer
    {
        struct HTTPResponse
        {
            std::string body;
            std::map<std::string, std::string> headers;
            int status;
        };

        using RequestHandler = std::function<HTTPResponse(const std::string&)>;

        virtual void initialize(uint16_t port) = 0;

        virtual void register_get_handler(
            const std::string& path,
            RequestHandler handler) = 0;

        virtual void register_post_handler(
            const std::string& path,
            RequestHandler handler) = 0;

        using QueryMap = std::map<std::string, std::string>;

        static QueryMap decode_query(const std::string& query)
        {
            return QueryMap(); // Placeholder for actual query decoding
        }
    };

    struct HTTPClient
    {
        struct Response
        {
            std::string body;
            int status_code;
        };

        virtual Response get(const std::string& url) = 0;
    };

    struct TCPClient
    {
        using ByteArray = std::vector<uint8_t>;

        virtual bool connect(const std::string& host, uint16_t port) = 0;
        virtual void send(const ByteArray& data) = 0;
        virtual ByteArray receive(size_t timeout_ms = 0) = 0;
        virtual void close() = 0;
    };

    constexpr const char * PROTOCOL_VERSION = "2.7.1";
    constexpr const char * SW_VERSION = "1.0.0";
    constexpr const char * BRAND_NAME = "cspot";
    constexpr const char * DEVICE_NAME = "CSpot";
    constexpr long long SPOTIFY_VERSION = 0x10800000000;


    struct LoginBlob
    {
        LoginBlob(const std::string& name, Crypto & crypto)
            : name_(name)
            , crypto_(crypto)
        {
            char hash[32];
            sprintf(hash, "%016zu", std::hash<std::string>{}(name));
            // base is 142137fd329622137a14901634264e6f332e2411
            device_id_ = std::string("142137fd329622137a149016") + std::string(hash);
        }

        const std::string get_info() const
        {
            nlohmann::json obj;
            obj["status"] = 101;
            obj["statusString"] = "OK";
            obj["version"] = cspot_ng::PROTOCOL_VERSION;
            obj["spotifyError"] = 0;
            obj["libraryVersion"] = cspot_ng::SW_VERSION;
            obj["accountReq"] = "PREMIUM";
            obj["brandDisplayName"] = cspot_ng::BRAND_NAME;
            obj["modelDisplayName"] = name_;
            obj["voiceSupport"] = "NO";
            obj["availability"] = username_;
            obj["productID"] = 0;
            obj["tokenType"] = "default";
            obj["groupStatus"] = "NONE";
            obj["resolverVersion"] = "0";
            obj["scope"] = "streaming,client-authorization-universal";
            obj["activeUser"] = "";
            obj["deviceID"] = device_id_;
            obj["remoteName"] = name_;
            obj["publicKey"] = encoded_key_;
            obj["deviceType"] = "SPEAKER";

            return obj.dump();
        }

        bool set_info(const HTTPServer::QueryMap & data)
        {
            bool ret = false;

            // Ensure the data contains the required fields
            if (data.find("username") != data.end() &&
                data.find("blob") != data.end() &&
                data.find("clientKey") != data.end() &&
                data.find("deviceName") != data.end())
            {
                ret = true;

                const auto username = data.find("username")->second;
                const auto blob_string  = data.find("blob")->second;
                const auto client_key_string = data.find("clientKey")->second;
                const auto device_name = data.find("deviceName")->second;

                const auto client_key = crypto_.decode_base64(client_key_string);
                const auto blob = crypto_.decode_base64(blob_string);

                const auto shared_key = crypto_.dh_calculate_shared_key(client_key);

                process_blob(blob, shared_key, device_name, username);
            }

            return ret;
        }

    private:

        using ByteArray = Crypto::ByteArray;

        void process_blob(
            const ByteArray& blob,
            const ByteArray& shared_key,
            const std::string& device_id,
            const std::string& username)
        {
            const auto part_decoded = decode_blob(blob, shared_key);
            const auto login_data = decode_blob_secondary(part_decoded, username, device_id);

            struct BlobReader
            {
                BlobReader(const ByteArray& data)
                    : data_(data)
                    , position_(0)
                {}

                uint32_t read_int()
                {
                    auto low = data_[position_];
                    if ((int)(low & 0x80) == 0)
                    {
                        position_ += 1;
                        return low;
                    }

                    auto high = data_[position_ + 1];
                    position_ += 2;

                    return (uint32_t)((low & 0x7f) | (high << 7));
                }

                void skip(size_t bytes)
                {
                    position_ += bytes;
                }

                size_t position() const
                {
                    return position_;
                }

            private:
                const ByteArray& data_;
                size_t position_;
            };

            BlobReader reader(login_data);
            reader.skip(1);
            reader.skip(reader.read_int());
            reader.skip(1);
            auth_type_ = reader.read_int();
            reader.skip(1);
            auto auth_size = reader.read_int();

            username_ = username;
            auth_data_ = ByteArray(
                login_data.begin() + reader.position(),
                login_data.begin() + reader.position() + auth_size);
        }

        ByteArray decode_blob(
            const ByteArray& blob,
            const ByteArray& shared_key)
        {
            constexpr size_t IV_SIZE = 16;
            constexpr size_t CHECKSUM_SIZE = 20;

            ByteArray iv(blob.begin(), blob.begin() + IV_SIZE);
            ByteArray encrypted(blob.begin() + IV_SIZE, blob.end() - CHECKSUM_SIZE);
            ByteArray checksum(blob.end() - CHECKSUM_SIZE, blob.end());

            crypto_.sha1_init();
            crypto_.sha1_update(shared_key);
            auto base_key = crypto_.sha1_final();
            base_key.resize(IV_SIZE);

            const std::string checksum_message = "checksum";
            const auto checksum_key = crypto_.sha1_hmac(base_key, ByteArray(checksum_message.begin(), checksum_message.end()));

            const std::string encryption_message = "encryption";
            auto encryption_key = crypto_.sha1_hmac(base_key, ByteArray(encryption_message.begin(), encryption_message.end()));

            const auto mac = crypto_.sha1_hmac(checksum_key, encrypted);

            if (mac != checksum)
            {
                // Error: MAC doesn't match
                while(1){}
            }

            encryption_key = ByteArray(encryption_key.begin(), encryption_key.begin() + IV_SIZE);
            crypto_.aes_ctr_xcrypt(encryption_key, iv, encrypted);

            return encrypted;
        }

        ByteArray decode_blob_secondary(
            const ByteArray& blob,
            const std::string& username,
            const std::string& device_id)
        {
            const auto encrypted_string = std::string(blob.begin(), blob.end());
            auto blob_data = crypto_.decode_base64(encrypted_string);

            crypto_.sha1_init();
            crypto_.sha1_update(ByteArray(device_id.begin(), device_id.end()));
            const auto secret = crypto_.sha1_final();

            const auto pk_base_key = crypto_.pbkdf2_hmac_sha1(
                secret, ByteArray(username.begin(), username.end()),
                256, 20);

            crypto_.sha1_init();
            crypto_.sha1_update(pk_base_key);
            auto key = ByteArray({0x00, 0x00, 0x00, 0x14});
            const auto base_key_hashed = crypto_.sha1_final();
            key.insert(key.begin(), base_key_hashed.begin(), base_key_hashed.end());

            crypto_.aes_ecb_decrypt(key, blob_data);

            const auto len = blob_data.size();

            for (size_t i = 0; i < len - 16; i++)
            {
                blob_data[len - i - 1] ^= blob_data[len - i - 17];
            }

            return blob_data;
        }

        std::string name_;
        std::string username_;
        std::string device_id_;
        std::string encoded_key_;

        int auth_type_ = 0;
        ByteArray auth_data_;

        Crypto & crypto_;
    };

    struct ZeroConf
    {
        ZeroConf(MDNSProvided & mdns, HTTPServer & server, Crypto & crypto)
            : mdns_(mdns)
            , server_(server)
            , device_name_("TESTNAME")
            , login_blob_(device_name_, crypto)
        {
            // ###############################
            // ##### MDNS Initialization #####
            // ###############################

            // Initialize the mDNS service
            mdns_.initialize();

            // Set the hostname
            mdns_.set_hostname(device_name_);

            // Register the mDNS service
            mdns_.register_service(
                device_name_,
                "_spotify-connect",
                "_tcp",
                "",
                0,
                {{"VERSION", "1.0"}, {"CPath", "/spotify_info"}, {"Stack", "SP"}});

            // #############################
            // ##### HTTP Server Setup #####
            // #############################

            // Initialize the HTTP server
            const uint16_t server_port = 7864;
            server_.initialize(server_port);

            // Register the HTTP GET handler for /spotify_info
            server_.register_get_handler(
                "/spotify_info",
                [&](const std::string& request) {
                    // Handle the GET request

                    HTTPServer::HTTPResponse response;
                    response.status = 200;
                    response.headers["Content-Type"] = "application/json";
                    response.body = login_blob_.get_info();
                    return response;
                });

            // Register the HTTP POST handler for /spotify_info
            server_.register_post_handler(
                "/spotify_info",
                [&](const std::string& request) {
                    // Feed the blob with the request data
                    auto info = HTTPServer::decode_query(request);

                    if(login_blob_.set_info(info))
                    {
                        // Notify auth success
                        auth_success_ = true;
                    }


                    // Prepare the response
                    nlohmann::json obj;
                    obj["status"] = 101;
                    obj["spotifyError"] = 0;
                    obj["statusString"] = "ERROR-OK";

                    // Send the response
                    HTTPServer::HTTPResponse response;
                    response.status = 200;
                    response.headers["Content-Type"] = "application/json";
                    response.body = obj.dump();
                    return response;
                });

            // Register the HTTP GET handler for /close
            server_.register_get_handler(
                "/close",
                [&](const std::string& request) {
                    // Handle the close request
                    closed_ = true;

                    // Send empty response
                    HTTPServer::HTTPResponse response;
                    response.status = 200;
                    response.body = "";
                    return response;
                });
        }

        bool is_auth_success() const
        {
            return auth_success_;
        }

        bool is_closed() const
        {
            return closed_;
        }

    private:
        MDNSProvided & mdns_;
        HTTPServer & server_;

        const std::string device_name_;
        LoginBlob login_blob_;

        bool auth_success_ = false;
        bool closed_ = false;
    };

    template<typename T>
    struct ProtoBuffer
    {
        using ByteArray = std::vector<uint8_t>;

        static ByteArray encode(const T& message, const pb_msgdesc_t * fields)
        {
            ByteArray buffer;

            pb_ostream_t stream;

            stream.callback = [](pb_ostream_t* stream, const pb_byte_t* buf, size_t count) {
                auto* dest = reinterpret_cast<ByteArray*>(stream->state);
                dest->insert(dest->end(), buf, buf + count);
                return true;
            };

            stream.state = &buffer;
            stream.max_size = 100000;
            stream.bytes_written = 0;
            stream.errmsg = nullptr;


            if (!pb_encode(&stream, fields, &message))
            {
                // Handle encoding error
                return {};
            }

            return buffer;
        }

        static T decode(const ByteArray& data, const pb_msgdesc_t * fields)
        {
            T message;
            pb_istream_t stream = pb_istream_from_buffer(data.data(), data.size());

            if (!pb_decode(&stream, fields, &message))
            {
                // Handle decoding error
                return {};
            }

            return message;
        }
    };

    struct Context
    {
        Context(const LoginBlob & blob, HTTPClient & http_client, TCPClient & tcp_client, Crypto & crypto)
            : blob_(blob)
            , http_client_(http_client)
            , tcp_client_(tcp_client)
            , crypto_(crypto)
        {

        }

        using ByteArray = std::vector<uint8_t>;

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
}

using namespace cspot_ng;

int main()
{
    MDNSProvided & mdns = *static_cast<MDNSProvided*>(nullptr); // Replace with actual implementation
    HTTPServer & server = *static_cast<HTTPServer*>(nullptr); // Replace with actual implementation
    Crypto & crypto = *static_cast<Crypto*>(nullptr); // Replace with actual implementation

    ZeroConf zeroconf(mdns, server, crypto);

    // Wait for authentication success
    while (!zeroconf.is_auth_success())
    {
        // Handle other tasks or sleep
    }

    while (!zeroconf.is_closed())
    {
        // Handle other tasks or sleep
    }

    return 0;
}