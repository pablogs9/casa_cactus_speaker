#pragma once

#include <ByteArray.hpp>
#include <limits.h>  // for CHAR_BIT
#include <cstdint>   // for uint32_t, uint8_t

namespace cspot_ng
{
    static inline uint32_t rotl(uint32_t n, unsigned int c) {
        const unsigned int mask = (CHAR_BIT * sizeof(n) - 1);
        c &= mask;
        return (n << c) | (n >> ((-c) & mask));
    }

    static inline uint32_t rotr(uint32_t n, unsigned int c) {
        const unsigned int mask = (CHAR_BIT * sizeof(n) - 1);
        c &= mask;
        return (n >> c) | (n << ((-c) & mask));
    }

    #define Byte(x, i) ((uint32_t)(((x) >> (8 * (i))) & 0xFF))
    #define BYTE2WORD(b) ((((uint32_t)(b)[3] & 0xFF) << 24) | (((uint32_t)(b)[2] & 0xFF) << 16) | \
                         (((uint32_t)(b)[1] & 0xFF) << 8) | (((uint32_t)(b)[0] & 0xFF)))
    #define WORD2BYTE(w, b) { \
        (b)[3] = Byte(w, 3); \
        (b)[2] = Byte(w, 2); \
        (b)[1] = Byte(w, 1); \
        (b)[0] = Byte(w, 0); \
    }
    #define XORWORD(w, b) { \
        (b)[3] ^= Byte(w, 3); \
        (b)[2] ^= Byte(w, 2); \
        (b)[1] ^= Byte(w, 1); \
        (b)[0] ^= Byte(w, 0); \
    }
    #define ADDKEY(k) this->R[KEYP] ^= (k)

    struct Shannon
    {
        static constexpr unsigned int N = 16;

        void key(const ByteArray& key) {
            this->initState();
            this->loadKey(key);
            this->genkonst();
            this->saveState();
            this->nbuf = 0;
        }

        void nonce(const ByteArray& nonce) {
            this->reloadState();
            this->konst = Shannon::INITKONST;
            this->loadKey(nonce);
            this->genkonst();
            this->nbuf = 0;
        }

        void stream(ByteArray& buf) {
            uint8_t* endbuf;
            size_t nbytes = buf.size();
            uint8_t* bufData = buf.data();

            /* handle any previously buffered bytes */
            while (this->nbuf != 0 && nbytes != 0) {
                *bufData++ ^= this->sbuf & 0xFF;
                this->sbuf >>= 8;
                this->nbuf -= 8;
                --nbytes;
            }

            /* handle whole words */
            endbuf = &bufData[nbytes & ~((uint32_t)0x03)];
            while (bufData < endbuf) {
                this->cycle();
                XORWORD(this->sbuf, bufData);
                bufData += 4;
            }

            /* handle any trailing bytes */
            nbytes &= 0x03;
            if (nbytes != 0) {
                this->cycle();
                this->nbuf = 32;
                while (this->nbuf != 0 && nbytes != 0) {
                    *bufData++ ^= this->sbuf & 0xFF;
                    this->sbuf >>= 8;
                    this->nbuf -= 8;
                    --nbytes;
                }
            }
        }

        void maconly(ByteArray& buf) {
            size_t nbytes = buf.size();
            uint8_t* bufData = buf.data();
            uint8_t* endbuf;

            /* handle any previously buffered bytes */
            if (this->nbuf != 0) {
                while (this->nbuf != 0 && nbytes != 0) {
                    this->mbuf ^= (*bufData++) << (32 - this->nbuf);
                    this->nbuf -= 8;
                    --nbytes;
                }
                if (this->nbuf != 0) /* not a whole word yet */
                    return;
                /* LFSR already cycled */
                this->macfunc(this->mbuf);
            }

            /* handle whole words */
            endbuf = &bufData[nbytes & ~((uint32_t)0x03)];
            while (bufData < endbuf) {
                this->cycle();
                this->macfunc(BYTE2WORD(bufData));
                bufData += 4;
            }

            /* handle any trailing bytes */
            nbytes &= 0x03;
            if (nbytes != 0) {
                this->cycle();
                this->mbuf = 0;
                this->nbuf = 32;
                while (this->nbuf != 0 && nbytes != 0) {
                    this->mbuf ^= (*bufData++) << (32 - this->nbuf);
                    this->nbuf -= 8;
                    --nbytes;
                }
            }
        }

        void encrypt(ByteArray& buf) {
            size_t nbytes = buf.size();
            uint8_t* bufData = buf.data();
            uint8_t* endbuf;
            uint32_t t = 0;

            /* handle any previously buffered bytes */
            if (this->nbuf != 0) {
                while (this->nbuf != 0 && nbytes != 0) {
                    this->mbuf ^= *bufData << (32 - this->nbuf);
                    *bufData ^= (this->sbuf >> (32 - this->nbuf)) & 0xFF;
                    ++bufData;
                    this->nbuf -= 8;
                    --nbytes;
                }
                if (this->nbuf != 0) /* not a whole word yet */
                    return;
                /* LFSR already cycled */
                this->macfunc(this->mbuf);
            }

            /* handle whole words */
            endbuf = &bufData[nbytes & ~((uint32_t)0x03)];
            while (bufData < endbuf) {
                this->cycle();
                t = BYTE2WORD(bufData);
                this->macfunc(t);
                t ^= this->sbuf;
                WORD2BYTE(t, bufData);
                bufData += 4;
            }

            /* handle any trailing bytes */
            nbytes &= 0x03;
            if (nbytes != 0) {
                this->cycle();
                this->mbuf = 0;
                this->nbuf = 32;
                while (this->nbuf != 0 && nbytes != 0) {
                    this->mbuf ^= *bufData << (32 - this->nbuf);
                    *bufData ^= (this->sbuf >> (32 - this->nbuf)) & 0xFF;
                    ++bufData;
                    this->nbuf -= 8;
                    --nbytes;
                }
            }
        }

        void decrypt(ByteArray& buf) {
            size_t nbytes = buf.size();
            uint8_t* bufData = buf.data();
            uint8_t* endbuf;
            uint32_t t = 0;

            /* handle any previously buffered bytes */
            if (this->nbuf != 0) {
                while (this->nbuf != 0 && nbytes != 0) {
                    *bufData ^= (this->sbuf >> (32 - this->nbuf)) & 0xFF;
                    this->mbuf ^= *bufData << (32 - this->nbuf);
                    ++bufData;
                    this->nbuf -= 8;
                    --nbytes;
                }
                if (this->nbuf != 0) /* not a whole word yet */
                    return;
                /* LFSR already cycled */
                this->macfunc(this->mbuf);
            }

            /* handle whole words */
            endbuf = &bufData[nbytes & ~((uint32_t)0x03)];
            while (bufData < endbuf) {
                this->cycle();
                t = BYTE2WORD(bufData) ^ this->sbuf;
                this->macfunc(t);
                WORD2BYTE(t, bufData);
                bufData += 4;
            }

            /* handle any trailing bytes */
            nbytes &= 0x03;
            if (nbytes != 0) {
                this->cycle();
                this->mbuf = 0;
                this->nbuf = 32;
                while (this->nbuf != 0 && nbytes != 0) {
                    *bufData ^= (this->sbuf >> (32 - this->nbuf)) & 0xFF;
                    this->mbuf ^= *bufData << (32 - this->nbuf);
                    ++bufData;
                    this->nbuf -= 8;
                    --nbytes;
                }
            }
        }

        void finish(ByteArray& buf) {
            size_t nbytes = buf.size();
            uint8_t* bufData = buf.data();
            int i;

            /* handle any previously buffered bytes */
            if (this->nbuf != 0) {
                /* LFSR already cycled */
                this->macfunc(this->mbuf);
            }

            /* perturb the MAC to mark end of input */
            this->cycle();
            ADDKEY(INITKONST ^ (this->nbuf << 3));
            this->nbuf = 0;

            /* now add the CRC to the stream register and diffuse it */
            for (i = 0; i < N; ++i)
                this->R[i] ^= this->CRC[i];
            this->diffuse();

            /* produce output from the stream buffer */
            while (nbytes > 0) {
                this->cycle();
                if (nbytes >= 4) {
                    WORD2BYTE(this->sbuf, bufData);
                    nbytes -= 4;
                    bufData += 4;
                } else {
                    for (i = 0; i < nbytes; ++i)
                        bufData[i] = Byte(this->sbuf, i);
                    break;
                }
            }
        }

       private:

        static constexpr unsigned int FOLD = Shannon::N;
        static constexpr unsigned int INITKONST = 0x6996c53a;
        static constexpr unsigned int KEYP = 13;
        uint32_t R[Shannon::N];
        uint32_t CRC[Shannon::N];
        uint32_t initR[Shannon::N];
        uint32_t konst;
        uint32_t sbuf;
        uint32_t mbuf;
        int nbuf;

        static uint32_t sbox1(uint32_t w) {
            w ^= rotl(w, 5) | rotl(w, 7);
            w ^= rotl(w, 19) | rotl(w, 22);
            return w;
        }

        static uint32_t sbox2(uint32_t w) {
            w ^= rotl(w, 7) | rotl(w, 22);
            w ^= rotl(w, 5) | rotl(w, 19);
            return w;
        }

        void cycle() {
            uint32_t t;
            int i;

            /* nonlinear feedback function */
            t = this->R[12] ^ this->R[13] ^ this->konst;
            t = Shannon::sbox1(t) ^ rotl(this->R[0], 1);
            /* shift register */
            for (i = 1; i < N; ++i)
                this->R[i - 1] = this->R[i];
            this->R[N - 1] = t;
            t = Shannon::sbox2(this->R[2] ^ this->R[15]);
            this->R[0] ^= t;
            this->sbuf = t ^ this->R[8] ^ this->R[12];
        }

        void crcfunc(uint32_t i) {
            uint32_t t;
            int j;

            /* Accumulate CRC of input */
            t = this->CRC[0] ^ this->CRC[2] ^ this->CRC[15] ^ i;
            for (j = 1; j < N; ++j)
                this->CRC[j - 1] = this->CRC[j];
            this->CRC[N - 1] = t;
        }

        void macfunc(uint32_t i) {
            this->crcfunc(i);
            this->R[KEYP] ^= i;
        }

        void initState() {
            int i;

            /* Register initialised to Fibonacci numbers; Counter zeroed. */
            this->R[0] = 1;
            this->R[1] = 1;
            for (i = 2; i < N; ++i)
                this->R[i] = this->R[i - 1] + this->R[i - 2];
            this->konst = Shannon::INITKONST;
        }

        void saveState() {
            int i;
            for (i = 0; i < Shannon::N; ++i)
                this->initR[i] = this->R[i];
        }

        void reloadState() {
            int i;
            for (i = 0; i < Shannon::N; ++i)
                this->R[i] = this->initR[i];
        }

        void genkonst() {
            this->konst = this->R[0];
        }

        void diffuse() {
            int i;
            for (i = 0; i < Shannon::FOLD; ++i)
                this->cycle();
        }

        void loadKey(const ByteArray& key) {
            int i, j;
            uint32_t k;
            uint8_t xtra[4];
            size_t keylen = key.size();
            const uint8_t* keyData = key.data();

            /* start folding in key */
            for (i = 0; i < (keylen & ~0x3); i += 4) {
                k = BYTE2WORD(&keyData[i]);
                ADDKEY(k);
                this->cycle();
            }

            /* if there were any extra key bytes, zero pad to a word */
            if (i < keylen) {
                for (j = 0 /* i unchanged */; i < keylen; ++i)
                    xtra[j++] = keyData[i];
                for (/* j unchanged */; j < 4; ++j)
                    xtra[j] = 0;
                k = BYTE2WORD(xtra);
                ADDKEY(k);
                this->cycle();
            }

            /* also fold in the length of the key */
            ADDKEY(keylen);
            this->cycle();

            /* save a copy of the register */
            for (i = 0; i < N; ++i)
                this->CRC[i] = this->R[i];

            /* now diffuse */
            this->diffuse();

            /* now xor the copy back -- makes key loading irreversible */
            for (i = 0; i < N; ++i)
                this->R[i] ^= this->CRC[i];
        }
    };
}