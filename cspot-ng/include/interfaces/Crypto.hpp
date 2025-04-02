#pragma once

#include <ByteArray.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace cspot_ng
{
    struct Crypto
    {
        virtual ByteArray decode_base64(const std::string& input) = 0;
        virtual std::string encode_base64(const ByteArray& input) = 0;
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
};