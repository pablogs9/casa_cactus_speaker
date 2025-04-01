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
        }

        const std::string get_info() const
        {
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

};