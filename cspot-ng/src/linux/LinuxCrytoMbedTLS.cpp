#include <linux/LinuxCrytoMbedTLS.hpp>

#include <mbedtls/base64.h>
#include <mbedtls/bignum.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/pkcs5.h>
#include <stdexcept>

extern "C" {
#include "aes.h"  // For AES_ECB_decrypt
}

namespace cspot_ng
{
    // DH constants definition
    const unsigned char LinuxCrytoMbedTLS::DHPrime[] = {
        /* Well-known Group 1, 768-bit prime */
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc9, 0x0f, 0xda, 0xa2,
        0x21, 0x68, 0xc2, 0x34, 0xc4, 0xc6, 0x62, 0x8b, 0x80, 0xdc, 0x1c, 0xd1,
        0x29, 0x02, 0x4e, 0x08, 0x8a, 0x67, 0xcc, 0x74, 0x02, 0x0b, 0xbe, 0xa6,
        0x3b, 0x13, 0x9b, 0x22, 0x51, 0x4a, 0x08, 0x79, 0x8e, 0x34, 0x04, 0xdd,
        0xef, 0x95, 0x19, 0xb3, 0xcd, 0x3a, 0x43, 0x1b, 0x30, 0x2b, 0x0a, 0x6d,
        0xf2, 0x5f, 0x14, 0x37, 0x4f, 0xe1, 0x35, 0x6d, 0x6d, 0x51, 0xc2, 0x45,
        0xe4, 0x85, 0xb5, 0x76, 0x62, 0x5e, 0x7e, 0xc6, 0xf4, 0x4c, 0x42, 0xe9,
        0xa6, 0x3a, 0x36, 0x20, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    };

    const unsigned char LinuxCrytoMbedTLS::DHGenerator[] = {2};

    ByteArray LinuxCrytoMbedTLS::decode_base64(const std::string& input)
    {
        // Calculate max decode length
        size_t requiredSize;
        mbedtls_base64_encode(nullptr, 0, &requiredSize,
                             reinterpret_cast<const unsigned char*>(input.c_str()), input.size());

        ByteArray output(requiredSize);
        size_t outputLen = 0;
        mbedtls_base64_decode(output.data(), requiredSize, &outputLen,
                             reinterpret_cast<const unsigned char*>(input.c_str()), input.size());

        return ByteArray(output.begin(), output.begin() + outputLen);
    }

    std::string LinuxCrytoMbedTLS::encode_base64(const ByteArray& input)
    {
        // Calculate max output length
        size_t requiredSize;
        mbedtls_base64_encode(nullptr, 0, &requiredSize, input.data(), input.size());

        ByteArray output(requiredSize);
        size_t outputLen = 0;

        mbedtls_base64_encode(output.data(), requiredSize, &outputLen, input.data(), input.size());

        return std::string(output.begin(), output.begin() + outputLen);
    }

    void LinuxCrytoMbedTLS::sha1_init()
    {
        // Init mbedtls md context, pick sha1
        mbedtls_md_init(&sha1Context);
        mbedtls_md_setup(&sha1Context, mbedtls_md_info_from_type(MBEDTLS_MD_SHA1), 1);
        mbedtls_md_starts(&sha1Context);
    }

    void LinuxCrytoMbedTLS::sha1_update(const ByteArray& data)
    {
        mbedtls_md_update(&sha1Context, data.data(), data.size());
    }

    ByteArray LinuxCrytoMbedTLS::sha1_final()
    {
        ByteArray digest(20);  // SHA1 digest size

        mbedtls_md_finish(&sha1Context, digest.data());
        mbedtls_md_free(&sha1Context);

        return digest;
    }

    ByteArray LinuxCrytoMbedTLS::sha1_hmac(const ByteArray& key, const ByteArray& data)
    {
        ByteArray digest(20);  // SHA1 digest size

        sha1_init();
        mbedtls_md_hmac_starts(&sha1Context, key.data(), key.size());
        mbedtls_md_hmac_update(&sha1Context, data.data(), data.size());
        mbedtls_md_hmac_finish(&sha1Context, digest.data());
        mbedtls_md_free(&sha1Context);

        return digest;
    }

    void LinuxCrytoMbedTLS::aes_ctr_xcrypt(const ByteArray& key, ByteArray& iv, ByteArray& data)
    {
        if (!aesCtxInitialized) {
            mbedtls_aes_init(&aesCtx);
            aesCtxInitialized = true;
        }

        // needed for internal cache
        size_t off = 0;
        unsigned char streamBlock[16] = {0};

        // set IV
        if (mbedtls_aes_setkey_enc(&aesCtx, key.data(), key.size() * 8) != 0) {
            throw std::runtime_error("Failed to set AES key");
        }

        // Perform decrypt
        if (mbedtls_aes_crypt_ctr(&aesCtx, data.size(), &off, iv.data(), streamBlock,
                                data.data(), data.data()) != 0) {
            throw std::runtime_error("Failed to decrypt");
        }
    }

    void LinuxCrytoMbedTLS::aes_ecb_decrypt(const ByteArray& key, ByteArray& data)
    {
        struct AES_ctx aesCtr;
        AES_init_ctx(&aesCtr, key.data());

        for (unsigned int x = 0; x < data.size() / 16; x++) {
            AES_ECB_decrypt(&aesCtr, data.data() + (x * 16));
        }
    }

    ByteArray LinuxCrytoMbedTLS::pbkdf2_hmac_sha1(const ByteArray& password,
                                                 const ByteArray& salt,
                                                 size_t iterations,
                                                 size_t key_length)
    {
        auto digest = ByteArray(key_length);

#if MBEDTLS_VERSION_NUMBER < 0x03030000
        // Init sha context
        sha1_init();
        mbedtls_pkcs5_pbkdf2_hmac(&sha1Context, password.data(), password.size(),
                                salt.data(), salt.size(), iterations, key_length,
                                digest.data());

        // Free sha context
        mbedtls_md_free(&sha1Context);
#else
        mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA1, password.data(),
                                    password.size(), salt.data(), salt.size(),
                                    iterations, key_length, digest.data());
#endif

        return digest;
    }

    void LinuxCrytoMbedTLS::dh_init()
    {
        private_key_data = generate_random_bytes(DH_KEY_SIZE);

        // initialize big num
        mbedtls_mpi prime, generator, res, privKey;
        mbedtls_mpi_init(&prime);
        mbedtls_mpi_init(&generator);
        mbedtls_mpi_init(&privKey);
        mbedtls_mpi_init(&res);

        // Read bin into big num mpi
        mbedtls_mpi_read_binary(&prime, DHPrime, sizeof(DHPrime));
        mbedtls_mpi_read_binary(&generator, DHGenerator, sizeof(DHGenerator));
        mbedtls_mpi_read_binary(&privKey, private_key_data.data(), DH_KEY_SIZE);

        // perform diffie hellman G^X mod P
        mbedtls_mpi_exp_mod(&res, &generator, &privKey, &prime, nullptr);

        // Write generated public key to vector
        public_key_data = ByteArray(DH_KEY_SIZE);
        mbedtls_mpi_write_binary(&res, public_key_data.data(), DH_KEY_SIZE);

        // Release memory
        mbedtls_mpi_free(&prime);
        mbedtls_mpi_free(&generator);
        mbedtls_mpi_free(&privKey);
        mbedtls_mpi_free(&res);
    }

    ByteArray& LinuxCrytoMbedTLS::public_key()
    {
        return public_key_data;
    }

    ByteArray& LinuxCrytoMbedTLS::private_key()
    {
        return private_key_data;
    }

    ByteArray LinuxCrytoMbedTLS::dh_calculate_shared_key(const ByteArray& remote_key)
    {
        // initialize big num
        mbedtls_mpi prime, remKey, res, privKey;
        mbedtls_mpi_init(&prime);
        mbedtls_mpi_init(&remKey);
        mbedtls_mpi_init(&privKey);
        mbedtls_mpi_init(&res);

        // Read bin into big num mpi
        mbedtls_mpi_read_binary(&prime, DHPrime, sizeof(DHPrime));
        mbedtls_mpi_read_binary(&remKey, remote_key.data(), remote_key.size());
        mbedtls_mpi_read_binary(&privKey, private_key_data.data(), DH_KEY_SIZE);

        // perform diffie hellman (G^Y)^X mod P (for shared secret)
        mbedtls_mpi_exp_mod(&res, &remKey, &privKey, &prime, nullptr);

        auto sharedKey = ByteArray(DH_KEY_SIZE);
        mbedtls_mpi_write_binary(&res, sharedKey.data(), DH_KEY_SIZE);

        // Release memory
        mbedtls_mpi_free(&prime);
        mbedtls_mpi_free(&remKey);
        mbedtls_mpi_free(&privKey);
        mbedtls_mpi_free(&res);

        return sharedKey;
    }

    ByteArray LinuxCrytoMbedTLS::generate_random_bytes(size_t length)
    {
        static int i = 0;
        ByteArray randomVector(length);
        for (size_t x = 0; x < length; x++) {
            randomVector[x] = i++;
        }
        return randomVector;

        // ByteArray randomVector(length);
        // mbedtls_entropy_context entropy;
        // mbedtls_ctr_drbg_context ctrDrbg;
        // // Personification string
        // const char* pers = "cspotGen";

        // // init entropy and random num generator
        // mbedtls_entropy_init(&entropy);
        // mbedtls_ctr_drbg_init(&ctrDrbg);

        // // Seed the generator
        // mbedtls_ctr_drbg_seed(&ctrDrbg, mbedtls_entropy_func, &entropy,
        //                     reinterpret_cast<const unsigned char*>(pers), 7);

        // // Generate random bytes
        // mbedtls_ctr_drbg_random(&ctrDrbg, randomVector.data(), length);

        // // Release memory
        // mbedtls_entropy_free(&entropy);
        // mbedtls_ctr_drbg_free(&ctrDrbg);

        // return randomVector;
    }
}
