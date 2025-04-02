#include <linux/LinuxCrypto.hpp>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/err.h>
#include <openssl/bn.h>

#include <stdexcept>
#include <vector>
#include <cstring>

// Define DH key size to match reference
#define DH_KEY_SIZE 96

// Copy the DHPrime from reference implementation
const static unsigned char DHPrime[] = {
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

// Define DHGenerator to match reference
static unsigned char DHGenerator[1] = {2};

namespace cspot_ng
{
    LinuxCrypto::LinuxCrypto()
        : m_dh(nullptr)
    {
        // Initialize OpenSSL
        OpenSSL_add_all_algorithms();

        dh_init();
    }

    LinuxCrypto::~LinuxCrypto()
    {
        if (m_dh) {
            DH_free(m_dh);
        }

        // Cleanup OpenSSL
        EVP_cleanup();
    }

    ByteArray LinuxCrypto::decode_base64(const std::string& input)
    {
        BIO* b64 = BIO_new(BIO_f_base64());
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

        BIO* bmem = BIO_new_mem_buf(input.c_str(), input.length());
        bmem = BIO_push(b64, bmem);

        ByteArray buffer(input.length());
        int decoded_size = BIO_read(bmem, buffer.data(), buffer.size());

        BIO_free_all(bmem);

        if (decoded_size <= 0) {
            return ByteArray();
        }

        buffer.resize(decoded_size);
        return buffer;
    }

    std::string LinuxCrypto::encode_base64(const ByteArray& input)
    {
        BIO* b64 = BIO_new(BIO_f_base64());
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

        BIO* bmem = BIO_new(BIO_s_mem());
        bmem = BIO_push(b64, bmem);

        BIO_write(bmem, input.data(), input.size());
        BIO_flush(bmem);

        BUF_MEM* buffer_ptr;
        BIO_get_mem_ptr(bmem, &buffer_ptr);

        std::string result(buffer_ptr->data, buffer_ptr->length);
        BIO_free_all(bmem);

        return result;
    }

    ByteArray LinuxCrypto::dh_calculate_shared_key(const ByteArray& remote_key)
    {
        if (!m_dh) {
            throw std::runtime_error("DH context not initialized");
        }

        // Convert remote key to BIGNUM
        BIGNUM* bn_remote_key = BN_bin2bn(remote_key.data(), remote_key.size(), nullptr);
        if (!bn_remote_key) {
            throw std::runtime_error("Failed to convert remote key to BIGNUM");
        }

        // Calculate shared key - ensure output is DH_KEY_SIZE
        ByteArray shared_key(DH_KEY_SIZE);
        int key_size = DH_compute_key(shared_key.data(), bn_remote_key, m_dh);

        if (key_size <= 0) {
            BN_free(bn_remote_key);
            throw std::runtime_error("DH shared key computation failed");
        }

        BN_free(bn_remote_key);
        return shared_key; // Return the full DH_KEY_SIZE bytes
    }

    void LinuxCrypto::sha1_init()
    {
        if (SHA1_Init(&m_sha1_ctx) != 1) {
            throw std::runtime_error("Failed to initialize SHA1 context");
        }
    }

    void LinuxCrypto::sha1_update(const ByteArray& data)
    {
        if (SHA1_Update(&m_sha1_ctx, data.data(), data.size()) != 1) {
            throw std::runtime_error("Failed to update SHA1 context");
        }
    }

    ByteArray LinuxCrypto::sha1_final()
    {
        ByteArray digest(SHA_DIGEST_LENGTH);
        if (SHA1_Final(digest.data(), &m_sha1_ctx) != 1) {
            throw std::runtime_error("Failed to finalize SHA1 context");
        }
        return digest;
    }

    ByteArray LinuxCrypto::sha1_hmac(const ByteArray& key, const ByteArray& data)
    {
        unsigned char digest[EVP_MAX_MD_SIZE];
        unsigned int digest_len = 0;

        HMAC_CTX* ctx = HMAC_CTX_new();
        HMAC_Init_ex(ctx, key.data(), key.size(), EVP_sha1(), nullptr);
        HMAC_Update(ctx, data.data(), data.size());
        HMAC_Final(ctx, digest, &digest_len);
        HMAC_CTX_free(ctx);

        return ByteArray(digest, digest + digest_len);
    }

    void LinuxCrypto::aes_ctr_xcrypt(
        const ByteArray& key,
        ByteArray& iv,
        ByteArray& data)
    {
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            throw std::runtime_error("Failed to create AES CTR context");
        }

        // Make a copy of the IV as OpenSSL will modify it internally
        unsigned char iv_copy[16];
        memcpy(iv_copy, iv.data(), 16);

        if (EVP_EncryptInit_ex(ctx, EVP_aes_128_ctr(), nullptr, key.data(), iv_copy) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Failed to initialize AES CTR context");
        }

        // In-place encryption/decryption
        int out_len;
        if (EVP_EncryptUpdate(ctx, data.data(), &out_len, data.data(), data.size()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Failed to perform AES CTR encryption/decryption");
        }

        EVP_CIPHER_CTX_free(ctx);

        // Manually update IV - increment the last block counter
        // This mimics the behavior in the reference implementation
        for (int i = 15; i >= 0; i--) {
            iv[i] += 1;
            if (iv[i] != 0) break;
        }
    }

    ByteArray LinuxCrypto::pbkdf2_hmac_sha1(
        const ByteArray& password,
        const ByteArray& salt,
        size_t iterations,
        size_t key_length)
    {
        ByteArray derived_key(key_length);

        if (PKCS5_PBKDF2_HMAC_SHA1(
                reinterpret_cast<const char*>(password.data()),
                password.size(),
                salt.data(),
                salt.size(),
                iterations,
                key_length,
                derived_key.data()) != 1) {
            throw std::runtime_error("PBKDF2 operation failed");
        }

        return derived_key;
    }

    void LinuxCrypto::aes_ecb_decrypt(
        const ByteArray& key,
        ByteArray& data)
    {
        if (data.size() % AES_BLOCK_SIZE != 0) {
            throw std::runtime_error("Data size must be a multiple of AES block size");
        }

        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            throw std::runtime_error("Failed to create AES ECB context");
        }

        if (EVP_DecryptInit_ex(ctx, EVP_aes_128_ecb(), nullptr, key.data(), nullptr) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Failed to initialize AES ECB context");
        }

        EVP_CIPHER_CTX_set_padding(ctx, 0);  // Disable padding

        ByteArray output(data.size() + AES_BLOCK_SIZE);  // Allow extra block for padding
        int out_len1 = 0;

        if (EVP_DecryptUpdate(ctx, output.data(), &out_len1, data.data(), data.size()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Failed to decrypt with AES ECB");
        }

        int out_len2 = 0;
        if (EVP_DecryptFinal_ex(ctx, output.data() + out_len1, &out_len2) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Failed to finalize AES ECB decryption");
        }

        output.resize(out_len1 + out_len2);
        data = output;

        EVP_CIPHER_CTX_free(ctx);
    }

    void LinuxCrypto::dh_init()
    {
        if (m_dh) {
            DH_free(m_dh);
        }

        // Create DH instance
        m_dh = DH_new();
        if (!m_dh) {
            throw std::runtime_error("Failed to create DH context");
        }

        // Use the same parameters as the reference implementation
        BIGNUM* p = BN_bin2bn(DHPrime, sizeof(DHPrime), nullptr);
        BIGNUM* g = BN_bin2bn(DHGenerator, sizeof(DHGenerator), nullptr);

        if (!p || !g || !DH_set0_pqg(m_dh, p, nullptr, g)) {
            if (p) BN_free(p);
            if (g) BN_free(g);
            DH_free(m_dh);
            m_dh = nullptr;
            throw std::runtime_error("Failed to set DH parameters");
        }

        // Generate private key as random bytes like in reference
        m_private_key = generate_random_bytes(DH_KEY_SIZE);
        BIGNUM* priv_key = BN_bin2bn(m_private_key.data(), m_private_key.size(), nullptr);

        // Set private key
        if (!priv_key || !DH_set0_key(m_dh, nullptr, priv_key)) {
            if (priv_key) BN_free(priv_key);
            DH_free(m_dh);
            m_dh = nullptr;
            throw std::runtime_error("Failed to set DH private key");
        }

        // Compute public key
        BIGNUM* pub_key = BN_new();
        if (!pub_key) {
            DH_free(m_dh);
            m_dh = nullptr;
            throw std::runtime_error("Failed to allocate BIGNUM for public key");
        }

        // Compute g^x mod p manually as in reference
        // g is the generator, priv_key is x, p is the prime
        const BIGNUM* dh_p = DH_get0_p(m_dh);
        const BIGNUM* dh_g = DH_get0_g(m_dh);

        BN_CTX* ctx = BN_CTX_new();
        if (!ctx || !BN_mod_exp(pub_key, dh_g, priv_key, dh_p, ctx)) {
            if (ctx) BN_CTX_free(ctx);
            BN_free(pub_key);
            DH_free(m_dh);
            m_dh = nullptr;
            throw std::runtime_error("Failed to compute DH public key");
        }
        BN_CTX_free(ctx);

        // Set the computed public key in DH object
        if (!DH_set0_key(m_dh, pub_key, nullptr)) {
            BN_free(pub_key);
            DH_free(m_dh);
            m_dh = nullptr;
            throw std::runtime_error("Failed to set DH public key");
        }

        // Convert public key to ByteArray
        m_public_key.resize(DH_KEY_SIZE);
        BN_bn2bin(pub_key, m_public_key.data());
    }

    ByteArray& LinuxCrypto::public_key()
    {
        if (!m_dh) {
            throw std::runtime_error("DH not initialized");
        }
        return m_public_key;
    }

    ByteArray& LinuxCrypto::private_key()
    {
        if (!m_dh) {
            throw std::runtime_error("DH not initialized");
        }
        return m_private_key;
    }

    ByteArray LinuxCrypto::generate_random_bytes(size_t length)
    {
        ByteArray random_bytes(length);
        if (RAND_bytes(random_bytes.data(), length) != 1) {
            throw std::runtime_error("Failed to generate random bytes");
        }
        return random_bytes;
    }
}
