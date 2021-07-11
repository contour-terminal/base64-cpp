// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "decode-common.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace base64::detail::decoder::simple
{

constexpr inline uint8_t alphabetIndexMap[256] = {
    /* ASCII table */
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, //   0..15
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, //  16..31
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63, //  32..47
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64, //  48..63
    64, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, //  64..95
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64, //  80..95
    64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, //  96..111
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64, // 112..127
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, // 128..143
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, // 144..150
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, // 160..175
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, // 176..191
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, // 192..207
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, // 208..223
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, // 224..239
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64  // 240..255
};

template <typename Iterator, typename Output>
size_t decode(Iterator _begin, Iterator _end, Output _output)
{
    auto const index = [](uint8_t i) -> uint8_t {
        return alphabetIndexMap[i];
    };

    if (_begin == _end)
        return 0;

    Iterator input = _begin;
    while (input != _end && index(*input) <= 63)
        input++;
    auto inputLength = static_cast<size_t>(std::distance(_begin, input));
    size_t decodedCount = ((inputLength + 3) / 4) * 3;
    auto out = _output;
    input = _begin;

    while (inputLength > 4)
    {
        *out++ = index(input[0]) << 2 | index(input[1]) >> 4;
        *out++ = index(input[1]) << 4 | index(input[2]) >> 2;
        *out++ = index(input[2]) << 6 | index(input[3]);

        input += 4;
        inputLength -= 4;
    }

    if (inputLength > 1)
    {
        *(out++) = index(input[0]) << 2 | index(input[1]) >> 4;

        if (inputLength > 2)
        {
            *(out++) = index(input[1]) << 4 | index(input[2]) >> 2;

            if (inputLength > 3)
            {
                *(out++) = index(input[2]) << 6 | index(input[3]);
            }
        }
    }

    decodedCount -= (4 - inputLength) & 0x03;

    return decodedCount;
}

}
