#pragma once

#include <interfaces/Crypto.hpp>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/dh.h>
#include <openssl/rand.h>
#include <memory>

namespace cspot_ng
{
    class LinuxCrypto : public Crypto
    {
    public:
        LinuxCrypto();
        ~LinuxCrypto();

        ByteArray decode_base64(const std::string& input) override;
        std::string encode_base64(const ByteArray& input) override;
        ByteArray dh_calculate_shared_key(const ByteArray& remote_key) override;

        void sha1_init() override;
        void sha1_update(const ByteArray& data) override;
        ByteArray sha1_final() override;

        ByteArray sha1_hmac(const ByteArray& key, const ByteArray& data) override;

        void aes_ctr_xcrypt(
            const ByteArray& key,
            ByteArray& iv,
            ByteArray& data) override;

        ByteArray pbkdf2_hmac_sha1(
            const ByteArray& password,
            const ByteArray& salt,
            size_t iterations,
            size_t key_length) override;

        void aes_ecb_decrypt(
            const ByteArray& key,
            ByteArray& data) override;

        void dh_init() override;
        ByteArray& public_key() override;
        ByteArray& private_key() override;

        ByteArray generate_random_bytes(size_t length) override;

    private:
        SHA_CTX m_sha1_ctx;
        DH* m_dh;
        ByteArray m_public_key;
        ByteArray m_private_key;
    };
}
