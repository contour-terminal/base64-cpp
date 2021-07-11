// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <base64-cpp/detail/decode-common.hpp>
#include <base64-cpp/detail/decode-simple.hpp>
#include <base64-cpp/detail/decode-sse.hpp>
//#include <base64-cpp/detail/decode-avx.hpp>

namespace base64
{

inline void decode(uint8_t const* _input, size_t _size, uint8_t* _output)
{
    detail::decoder::sse::decode(detail::decoder::sse::lookup_pshufb,
                                 detail::decoder::sse::pack_madd,
                                 _input,
                                 _size,
                                 _output);
}

inline std::string decode(std::string_view _input)
{
    while (!_input.empty() && _input.back() == '=')
        _input.remove_suffix(1);

    std::string output;
    output.resize((3 * _input.size()) / 4);

    auto const remainderLength = _input.size() & 15;
    auto const mainInputLength = _input.size() & ~15;
    size_t outputLength = (mainInputLength / 4) * 3;
    size_t decodedCount = ((outputLength + 3) / 4) * 3;
    if (_input.size() >= 16)
    {
        decode(reinterpret_cast<uint8_t const*>(_input.data()),
               mainInputLength,
               reinterpret_cast<uint8_t*>(output.data()));
        _input.remove_prefix(mainInputLength);
    }

    // abcd|e
    // 4    1
    //
    // abcd|efgh|ijk
    //  4   4     3

    if (remainderLength)
    {
        auto const outputBegin = output.data() + outputLength;
        auto const finalFragmentLength = detail::decoder::simple::decode(
            _input.data(),
            _input.data() + _input.size(),
            outputBegin
        );
        outputLength += finalFragmentLength;
        output.resize(outputLength);
    }

    return output;
}

} // namespace base64
