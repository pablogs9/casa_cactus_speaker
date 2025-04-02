#pragma once

#include <ByteArray.hpp>

#include <pb_encode.h>
#include <pb_decode.h>

namespace cspot_ng
{
    template<typename T>
    struct ProtoBuffer
    {
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

        static T decode(const ByteArray& data, const pb_msgdesc_t * fields, T& message)
        {
            pb_istream_t stream = pb_istream_from_buffer(data.data(), data.size());

            if (!pb_decode(&stream, fields, &message))
            {
                // Handle decoding error
                return {};
            }

            return message;
        }
    };

};