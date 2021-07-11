// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <array>
#include <cassert>
#include <limits>
#include <string_view>
#include <tuple>
#include <utility>

#include <immintrin.h>
#include <cpuid.h>

namespace base64::detail::cpu
{

enum class feature
{
    SSE2 = 0,
    SSE3,
    SSE4_1,
    SSE4_2,
    AVX,
    AVX2
};

std::string_view to_string(feature _f)
{
    auto constexpr names = std::array<std::string_view, 6>{
        "SSE2",
        "SSE3",
        "SSE4.1",
        "SSE4.2",
        "AVX",
        "AVX2"
    };
    return names.at(static_cast<size_t>(_f));
}

inline bool is_available(feature _feature) noexcept
{
    using namespace std;

    enum class reg : uint8_t { eax, ebx, ecx, edx };
    constexpr auto mappings = array<std::tuple<feature, reg, unsigned>, 6> {
        tuple{feature::SSE2,   reg::edx, bit_SSE2},
        tuple{feature::SSE3,   reg::ecx, bit_SSE3},
        tuple{feature::SSE4_1, reg::ecx, bit_SSE4_1},
        tuple{feature::SSE4_2, reg::ecx, bit_SSE4_2},
        tuple{feature::AVX,    reg::ecx, bit_AVX},
        tuple{feature::AVX2,   reg::ebx, bit_AVX2},
    };

    auto regs = array<unsigned, 4>{0, 0, 0, 0};
    __get_cpuid(1, &regs[0], &regs[1], &regs[2], &regs[3]);

    assert(          get<0>(mappings[static_cast<uint8_t>(_feature)]) == _feature);
    auto const reg = get<1>(mappings[static_cast<uint8_t>(_feature)]);
    auto const bit = get<2>(mappings[static_cast<uint8_t>(_feature)]);
    return (regs[static_cast<uint8_t>(reg)] & bit) ? true : false;
}

}

namespace std
{
    template <>
    struct numeric_limits<base64::detail::cpu::feature> {
        static size_t min() { return 0; }
        static size_t max() { return 5; }
    };
}

// int main()
// {
//     using namespace std;
//
//     for (auto i = numeric_limits<cpu::feature>::min();
//               i <= numeric_limits<cpu::feature>::max();
//               ++i)
//     {
//         auto const feature = static_cast<cpu::feature>(i);
//         cout << setw(20) << cpu::to_string(feature) << ": "
//              << (cpu::is_available(feature) ? "supported" : "not supported")
//              << endl;
//     }
// }
