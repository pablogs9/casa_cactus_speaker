#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>

#include <ByteArray.hpp>
#include <interfaces/Crypto.hpp>
#include <Constants.hpp>

#include <json.hpp>

namespace cspot_ng
{
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

            crypto_.dh_init();
        }

        const std::string get_info() const
        {
            auto encoded_key = crypto_.encode_base64(crypto_.public_key());

            nlohmann::json obj;
            obj["status"] = 101;
            obj["statusString"] = "OK";
            obj["version"] = cspot_ng::Constants::PROTOCOL_VERSION;
            obj["spotifyError"] = 0;
            obj["libraryVersion"] = cspot_ng::Constants::SW_VERSION;
            obj["accountReq"] = "PREMIUM";
            obj["brandDisplayName"] = cspot_ng::Constants::BRAND_NAME;
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
            obj["publicKey"] = encoded_key;
            obj["deviceType"] = "SPEAKER";

            return obj.dump();
        }

        bool set_info(const HTTPServer::QueryMap & data)
        {
            bool ret = false;

            // Ensure the data contains the required fields
            if (data.find("userName") != data.end() &&
                data.find("blob") != data.end() &&
                data.find("clientKey") != data.end() &&
                data.find("deviceName") != data.end())
            {
                ret = true;

                const auto username = data.find("userName")->second;
                const auto blob_string  = data.find("blob")->second;
                const auto client_key_string = data.find("clientKey")->second;
                const auto device_name = data.find("deviceName")->second;

                const auto client_key = crypto_.decode_base64(client_key_string);
                std::cout << "!!!!! CLIENT_KEY_BYTES: " << std::endl;
                std::cout << "-------------------------" << std::endl;
                for(size_t i = 0; i < client_key.size(); i++) {
                  printf("%02X ", client_key[i]);

                  if ((i + 1) % 16 == 0) {
                    std::cout << std::endl;
                  }
                }
                std::cout << std::endl;
                const auto blob = crypto_.decode_base64(blob_string);

                std::cout << "!!!!! BLOB_BYTES: " << std::endl;
                std::cout << "-------------------------" << std::endl;
                for(size_t i = 0; i < blob.size(); i++) {
                    printf("%02X ", blob[i]);

                    if ((i + 1) % 16 == 0) {
                    std::cout << std::endl;
                    }
                }
                std::cout << std::endl;

                const auto shared_key = crypto_.dh_calculate_shared_key(client_key);

                std::cout << "!!!!! SECRET_KEY: " << std::endl;
                std::cout << "-------------------------" << std::endl;
                for(size_t i = 0; i < shared_key.size(); i++) {
                    printf("%02X ", shared_key[i]);

                    if ((i + 1) % 16 == 0) {
                    std::cout << std::endl;
                    }
                }
                std::cout << std::endl;

                process_blob(blob, shared_key, device_id_, username);
            }

            return ret;
        }

        const std::string & username() const
        {
            return username_;
        }

        const ByteArray & auth_data() const
        {
            return auth_data_;
        }

        int auth_type() const
        {
            return auth_type_;
        }

        const std::string & device_id() const
        {
            return device_id_;
        }

    private:

        void process_blob(
            const ByteArray& blob,
            const ByteArray& shared_key,
            const std::string& device_id,
            const std::string& username)
        {
            const auto part_decoded = decode_blob(blob, shared_key);
            const auto login_data = decode_blob_secondary(part_decoded, username, device_id);

            std::cout << "!!!!! LOGIN_DATA: " << std::endl;
            std::cout << "-------------------------" << std::endl;
            for(size_t i = 0; i < login_data.size(); i++) {
              printf("%02X ", login_data[i]);

              if ((i + 1) % 16 == 0) {
                std::cout << std::endl;
              }
            }
            std::cout << std::endl;

            auto read_blob_int = [](const ByteArray& data, size_t& pos) -> uint32_t {
                auto lo = data[pos];
                if ((int)(lo & 0x80) == 0) {
                    pos += 1;
                    return lo;
                }

                auto hi = data[pos + 1];
                pos += 2;

                uint32_t ret = (uint32_t)((lo & 0x7f) | (hi << 7));

                return ret;
            };

            // Parse blob - using the same approach as the original implementation
            size_t blob_position = 1;
            blob_position += read_blob_int(login_data, blob_position);
            blob_position += 1;
            auth_type_ = read_blob_int(login_data, blob_position);
            blob_position += 1;
            auto auth_size = read_blob_int(login_data, blob_position);

            username_ = username;
            auth_data_ = ByteArray(
                login_data.begin() + blob_position,
                login_data.begin() + blob_position + auth_size);
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
                // Log error but continue - don't halt with infinite loop
                std::cerr << "Mac doesn't match!" << std::endl;
                return ByteArray();
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

        int auth_type_ = 0;
        ByteArray auth_data_;

        Crypto & crypto_;
    };

};