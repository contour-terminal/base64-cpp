#pragma once

#include <cstdint>
#include <cstdlib>
#include <string_view>

namespace base64::detail::decoder
{

auto static const inline alphabet = std::string_view{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};

struct invalid_input final
{
    size_t const offset;
    uint8_t const byte;
};

}
