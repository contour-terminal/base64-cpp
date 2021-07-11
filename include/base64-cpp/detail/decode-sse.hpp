#pragma once

#include "decode-common.hpp"

#include <cstdint>
#include <cstdlib>
#include <cassert>
#include <stdexcept>

#include <immintrin.h>

namespace base64::detail::decoder::sse
{

#define packed_byte(b) _mm_set1_epi8(uint8_t(b))
#define packed_dword(x) _mm_set1_epi32(x)
#define masked(x, mask) _mm_and_si128(x, _mm_set1_epi32(mask))

// {{{ pack
inline __m128i pack_naive(__m128i const _values)
{
    // input:  [00dddddd|00cccccc|00bbbbbb|00aaaaaa]

    __m128i const ca = masked(_values, 0x003f003f);
    __m128i const db = masked(_values, 0x3f003f00);

    // t0   =  [0000cccc|ccdddddd|0000aaaa|aabbbbbb]
    __m128i const t0 = _mm_or_si128(
                        _mm_srli_epi32(db, 8),
                        _mm_slli_epi32(ca, 6)
                       );

    // t1   =  [dddd0000|aaaaaabb|bbbbcccc|dddddddd]
    __m128i const t1 = _mm_or_si128(
                        _mm_srli_epi32(t0, 16),
                        _mm_slli_epi32(t0, 12)
                       );

    // result: [00000000|aaaaaabb|bbbbcccc|ccdddddd]
    return masked(t1, 0x00ffffff);
}

inline __m128i pack_madd(__m128i const _values)
{
    // input:  [00dddddd|00cccccc|00bbbbbb|00aaaaaa]

    // merge:  [0000cccc|ccdddddd|0000aaaa|aabbbbbb]
    __m128i const merge_ab_and_bc = _mm_maddubs_epi16(_values, packed_dword(0x01400140));

    // result: [00000000|aaaaaabb|bbbbcccc|ccdddddd]
    return _mm_madd_epi16(merge_ab_and_bc, packed_dword(0x00011000));
}

// }}}
// {{{ lookup

inline __m128i lookup_base(__m128i const _input)
{
    /*
    +--------+-------------------+------------------------+
    | range  | expression        | after constant folding |
    +========+===================+========================+
    | A-Z    | i - ord('A')      | i - 65                 |
    +--------+-------------------+------------------------+
    | a-z    | i - ord('a') + 26 | i - 71                 |
    +--------+-------------------+------------------------+
    | 0-9    | i - ord('0') + 52 | i + 4                  |
    +--------+-------------------+------------------------+
    | +      | i - ord('+') + 62 | i + 19                 |
    +--------+-------------------+------------------------+
    | /      | i - ord('/') + 63 | i + 16                 |
    +--------+-------------------+------------------------+

    number of operations:
    - cmp (le/gt/eq): 9
    - bit-and:        8
    - bit-or:         4
    - add:            1
    - movemask:       1
    - total:        =23
    */

    // shift for range 'A' - 'Z'
    __m128i const ge_A = _mm_cmpgt_epi8(_input, packed_byte('A' - 1));
    __m128i const le_Z = _mm_cmplt_epi8(_input, packed_byte('Z' + 1));
    __m128i const range_AZ = _mm_and_si128(packed_byte(-65), _mm_and_si128(ge_A, le_Z));

    // shift for range 'a' - 'z'
    __m128i const ge_a = _mm_cmpgt_epi8(_input, packed_byte('a' - 1));
    __m128i const le_z = _mm_cmplt_epi8(_input, packed_byte('z' + 1));
    __m128i const range_az = _mm_and_si128(packed_byte(-71), _mm_and_si128(ge_a, le_z));

    // shift for range '0' - '9'
    __m128i const ge_0 = _mm_cmpgt_epi8(_input, packed_byte('0' - 1));
    __m128i const le_9 = _mm_cmplt_epi8(_input, packed_byte('9' + 1));
    __m128i const range_09 = _mm_and_si128(packed_byte(4), _mm_and_si128(ge_0, le_9));

    // shift for character '+'
    __m128i const eq_plus = _mm_cmpeq_epi8(_input, packed_byte('+'));
    __m128i const char_plus = _mm_and_si128(packed_byte(19), eq_plus);

    // shift for character '/'
    __m128i const eq_slash = _mm_cmpeq_epi8(_input, packed_byte('/'));
    __m128i const char_slash = _mm_and_si128(packed_byte(16), eq_slash);

    // merge partial results

    __m128i const shift = _mm_or_si128(range_AZ,
                          _mm_or_si128(range_az,
                          _mm_or_si128(range_09,
                          _mm_or_si128(char_plus, char_slash))));

    // Individual shift values are non-zero, thus if any
    // byte in a shift vector is zero, then the input
    // contains invalid bytes.
    const auto mask = _mm_movemask_epi8(_mm_cmpeq_epi8(shift, packed_byte(0)));
    if (mask) {
        // some characters do not match the valid range
        for (unsigned i=0; i < 16; i++) {
            if (mask & (1 << i)) {
                throw invalid_input{i, 0};
            }
        }
    }

    return _mm_add_epi8(_input, shift);
}

inline __m128i lookup_byte_blend(__m128i const _input)
{
    /*
    improvment of lookup_base

    number of operations:
    - cmp (le/gt/eq): 9
    - bit-and:        4
    - byte-blend:     4
    - add:            1
    - movemask:       1
    - total:        =19
    */

    __m128i shift;

    // shift for range 'A' - 'Z'
    __m128i const ge_A = _mm_cmpgt_epi8(_input, packed_byte('A' - 1));
    __m128i const le_Z = _mm_cmplt_epi8(_input, packed_byte('Z' + 1));
    shift = _mm_and_si128(packed_byte(-65), _mm_and_si128(ge_A, le_Z));

    // shift for range 'a' - 'z'
    __m128i const ge_a = _mm_cmpgt_epi8(_input, packed_byte('a' - 1));
    __m128i const le_z = _mm_cmplt_epi8(_input, packed_byte('z' + 1));
    shift = _mm_blendv_epi8(shift, packed_byte(-71), _mm_and_si128(ge_a, le_z));

    // shift for range '0' - '9'
    __m128i const ge_0 = _mm_cmpgt_epi8(_input, packed_byte('0' - 1));
    __m128i const le_9 = _mm_cmplt_epi8(_input, packed_byte('9' + 1));
    shift = _mm_blendv_epi8(shift, packed_byte(4), _mm_and_si128(ge_0, le_9));

    // shift for character '+'
    __m128i const eq_plus = _mm_cmpeq_epi8(_input, packed_byte('+'));
    shift = _mm_blendv_epi8(shift, packed_byte(19), eq_plus);

    // shift for character '/'
    __m128i const eq_slash = _mm_cmpeq_epi8(_input, packed_byte('/'));
    shift = _mm_blendv_epi8(shift, packed_byte(16), eq_slash);

    // Individual shift values are non-zero, thus if any
    // byte in a shift vector is zero, then the input
    // contains invalid bytes.
    const auto mask = _mm_movemask_epi8(_mm_cmpeq_epi8(shift, packed_byte(0)));
    if (mask) {
        // some characters do not match the valid range
        for (unsigned i=0; i < 16; i++) {
            if (mask & (1 << i)) {
                throw invalid_input{i, 0};
            }
        }
    }

    return _mm_add_epi8(_input, shift);
}

inline __m128i lookup_incremental(__m128i const _input)
{
    /*
    +-------+------------+-----------+--------+
    | index | byte range | comment   | shift  |
    +=======+============+===========+========+
    |  0    |  00 ..  42 | invalid   |      0 |
    +-------+------------+-----------+--------+
    |  1    |         43 | '+'       |  19    |
    +-------+------------+-----------+--------+
    |  2    |  44 ..  46 | invalid   |      0 |
    +-------+------------+-----------+--------+
    |  3    |         47 | '/'       |  16    |
    +-------+------------+-----------+--------+
    |  4    |  48 ..  57 | '0' - '9' |   4    |
    +-------+------------+-----------+--------+
    |  5    |  58 ..  64 | invalid   |      0 |
    +-------+------------+-----------+--------+
    |  6    |  65 ..  90 | 'A' - 'Z' | -65    |
    +-------+------------+-----------+--------+
    |  7    |  91 ..  96 | invalid   |      0 |
    +-------+------------+-----------+--------+
    |  8    |  97 .. 122 | 'a' - 'z' | -71    |
    +-------+------------+-----------+--------+
    |  9    | 122 .. ... | invalid   |      0 |
    +-------+------------+-----------+--------+

    number of operations:
    - cmp (le/gt/eq): 9
    - add:           10
    - movemask:       1
    - pshufb          1
    - total:        =21
    */

    // value from the first column
    __m128i index = packed_byte(0);

    index = _mm_sub_epi8(index, _mm_cmpgt_epi8(_input, packed_byte(42)));
    index = _mm_sub_epi8(index, _mm_cmpgt_epi8(_input, packed_byte(43)));
    index = _mm_sub_epi8(index, _mm_cmpgt_epi8(_input, packed_byte(46)));
    index = _mm_sub_epi8(index, _mm_cmpgt_epi8(_input, packed_byte(47)));
    index = _mm_sub_epi8(index, _mm_cmpgt_epi8(_input, packed_byte(57)));
    index = _mm_sub_epi8(index, _mm_cmpgt_epi8(_input, packed_byte(64)));
    index = _mm_sub_epi8(index, _mm_cmpgt_epi8(_input, packed_byte(90)));
    index = _mm_sub_epi8(index, _mm_cmpgt_epi8(_input, packed_byte(96)));
    index = _mm_sub_epi8(index, _mm_cmpgt_epi8(_input, packed_byte(122)));

    const char invalid = 0;
    __m128i const LUT = _mm_setr_epi8(
     /* 0 */ invalid,
     /* 1 */ char(19),
     /* 2 */ invalid,
     /* 3 */ char(16),
     /* 4 */ char(4),
     /* 5 */ invalid,
     /* 6 */ char(-65),
     /* 7 */ invalid,
     /* 8 */ char(-71),
     /* 9 */ invalid,
     // the rest is also invalid
     invalid, invalid, invalid, invalid, invalid, invalid
    );

    __m128i const shift = _mm_shuffle_epi8(LUT, index);

    const auto mask = _mm_movemask_epi8(_mm_cmpeq_epi8(shift, packed_byte(0)));
    if (mask) {
        // some characters do not match the valid range
        for (unsigned i=0; i < 16; i++) {
            if (mask & (1 << i)) {
                throw invalid_input{i, 0};
            }
        }
    }

    return _mm_add_epi8(_input, shift);
}

inline __m128i lookup_pshufb(__m128i const _input)
{
    /*
    number of operations:
    - cmp (le/gt/eq):  3
    - shift:           1
    - add/sub:         2
    - and/or/andnot:   4
    - movemask:        1
    - pshufb           3
    - total:         =14
    */

    __m128i const higher_nibble = _mm_srli_epi32(_input, 4) & packed_byte(0x0f);
    const char linv = 1;
    const char hinv = 0;

    __m128i const lower_bound_LUT = _mm_setr_epi8(
        /* 0 */ linv, /* 1 */ linv, /* 2 */ 0x2b, /* 3 */ 0x30,
        /* 4 */ 0x41, /* 5 */ 0x50, /* 6 */ 0x61, /* 7 */ 0x70,
        /* 8 */ linv, /* 9 */ linv, /* a */ linv, /* b */ linv,
        /* c */ linv, /* d */ linv, /* e */ linv, /* f */ linv
    );

    __m128i const upper_bound_LUT = _mm_setr_epi8(
        /* 0 */ hinv, /* 1 */ hinv, /* 2 */ 0x2b, /* 3 */ 0x39,
        /* 4 */ 0x4f, /* 5 */ 0x5a, /* 6 */ 0x6f, /* 7 */ 0x7a,
        /* 8 */ hinv, /* 9 */ hinv, /* a */ hinv, /* b */ hinv,
        /* c */ hinv, /* d */ hinv, /* e */ hinv, /* f */ hinv
    );

    __m128i const shift_LUT = _mm_setr_epi8(
        /* 0 */ 0x00,        /* 1 */ 0x00,        /* 2 */ 0x3e - 0x2b, /* 3 */ 0x34 - 0x30,
        /* 4 */ 0x00 - 0x41, /* 5 */ 0x0f - 0x50, /* 6 */ 0x1a - 0x61, /* 7 */ 0x29 - 0x70,
        /* 8 */ 0x00,        /* 9 */ 0x00,        /* a */ 0x00,        /* b */ 0x00,
        /* c */ 0x00,        /* d */ 0x00,        /* e */ 0x00,        /* f */ 0x00
    );

    __m128i const upper_bound = _mm_shuffle_epi8(upper_bound_LUT, higher_nibble);
    __m128i const lower_bound = _mm_shuffle_epi8(lower_bound_LUT, higher_nibble);

    __m128i const below = _mm_cmplt_epi8(_input, lower_bound);
    __m128i const above = _mm_cmpgt_epi8(_input, upper_bound);
    __m128i const eq_2f = _mm_cmpeq_epi8(_input, packed_byte(0x2f));

    // in_range = not (below or above) or eq_2f
    // outside  = not in_range = below or above and not eq_2f (from de Morgan law)
    __m128i const outside = _mm_andnot_si128(eq_2f, above | below);

    const auto mask = _mm_movemask_epi8(outside);
    if (mask)
    {
        // some characters do not match the valid range
        for (unsigned i = 0; i < 16; i++)
        // TODO(christianparpart): Can't this be rewritten into a single bit-test?
        // - such as instruction for counting the 0's until the first 1 is hit
        {
            if (mask & (1 << i))
            {
                throw invalid_input{i, 0};
            }
        }
    }

    __m128i const shift  = _mm_shuffle_epi8(shift_LUT, higher_nibble);
    __m128i const t0     = _mm_add_epi8(_input, shift);
    __m128i const result = _mm_add_epi8(t0, _mm_and_si128(eq_2f, packed_byte(-3)));

    return result;
}

inline __m128i lookup_pshufb_bitmask(__m128i const _input)
{
    /*
    number of operations:
    - cmp (le/gt/eq):  2
    - shift:           1
    - add/sub:         2
    - and/or/andnot:   4
    - movemask:        1
    - pshufb           3
    - total:         =13
    */

    /*
    lower nibble (. is 0)
    ------------------------------------------------------------------------
      0:    .   .   .   4   . -65   . -71   .   .   .   .   .   .   .   .
      1:    .   .   .   4 -65 -65 -71 -71   .   .   .   .   .   .   .   .
      2:    .   .   .   4 -65 -65 -71 -71   .   .   .   .   .   .   .   .
      3:    .   .   .   4 -65 -65 -71 -71   .   .   .   .   .   .   .   .
      4:    .   .   .   4 -65 -65 -71 -71   .   .   .   .   .   .   .   .
      5:    .   .   .   4 -65 -65 -71 -71   .   .   .   .   .   .   .   .
      6:    .   .   .   4 -65 -65 -71 -71   .   .   .   .   .   .   .   .
      7:    .   .   .   4 -65 -65 -71 -71   .   .   .   .   .   .   .   .
      8:    .   .   .   4 -65 -65 -71 -71   .   .   .   .   .   .   .   .
      9:    .   .   .   4 -65 -65 -71 -71   .   .   .   .   .   .   .   .
     10:    .   .   .   . -65 -65 -71 -71   .   .   .   .   .   .   .   .
     11:    .   .  19   . -65   . -71   .   .   .   .   .   .   .   .   .
     12:    .   .   .   . -65   . -71   .   .   .   .   .   .   .   .   .
     13:    .   .   .   . -65   . -71   .   .   .   .   .   .   .   .   .
     14:    .   .   .   . -65   . -71   .   .   .   .   .   .   .   .   .
     15:    .   .  16   . -65   . -71   .   .   .   .   .   .   .   .   .
           LSB                                                         MSB
    */

    __m128i const higher_nibble = _mm_srli_epi32(_input, 4) & packed_byte(0x0f);
    __m128i const lower_nibble  = _input & packed_byte(0x0f);

    __m128i const shiftLUT = _mm_setr_epi8(
        0,   0,  19,   4, -65, -65, -71, -71,
        0,   0,   0,   0,   0,   0,   0,   0);

    __m128i const maskLUT  = _mm_setr_epi8(
        /* 0        : 0b1010_1000*/ char(0xa8),
        /* 1 .. 9   : 0b1111_1000*/ char(0xf8), char(0xf8), char(0xf8), char(0xf8),
                                    char(0xf8), char(0xf8), char(0xf8), char(0xf8),
                                    char(0xf8),
        /* 10       : 0b1111_0000*/ char(0xf0),
        /* 11       : 0b0101_0100*/ 0x54,
        /* 12 .. 14 : 0b0101_0000*/ 0x50, 0x50, 0x50,
        /* 15       : 0b0101_0100*/ 0x54
    );

    __m128i const bitposLUT = _mm_setr_epi8(
        0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, char(0x80),
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    );

    __m128i const sh     = _mm_shuffle_epi8(shiftLUT,  higher_nibble);
    __m128i const eq_2f  = _mm_cmpeq_epi8(_input, packed_byte(0x2f));
    __m128i const shift  = _mm_blendv_epi8(sh, packed_byte(16), eq_2f);

    __m128i const M      = _mm_shuffle_epi8(maskLUT,   lower_nibble);
    __m128i const bit    = _mm_shuffle_epi8(bitposLUT, higher_nibble);

    __m128i const non_match = _mm_cmpeq_epi8(_mm_and_si128(M, bit), _mm_setzero_si128());

    const auto mask = _mm_movemask_epi8(non_match);
    if (mask)
    {
        // some characters do not match the valid range
        for (unsigned i = 0; i < 16; ++i)
        {
            if (mask & (1 << i))
            {
                throw invalid_input{i, 0};
            }
        }
    }

    __m128i const result = _mm_add_epi8(_input, shift);

    return result;
}

// }}}
// {{{ decode

template <typename FN_LOOKUP, typename FN_PACK>
void decode(FN_LOOKUP _lookup, FN_PACK _pack, uint8_t const* _input, size_t _size, uint8_t* _output)
{
    assert(_size % 16 == 0);

    uint8_t* out = _output;

    for (size_t i = 0; i < _size; i += 16)
    {
        __m128i in = _mm_loadu_si128(reinterpret_cast<__m128i const*>(_input + i));
        __m128i values;

        try
        {
            values = _lookup(in);
        }
        catch (invalid_input const& e)
        {
            const auto shift = e.offset;
            throw invalid_input{i + shift, _input[i + shift]};
        }

        // input:  packed_dword([00dddddd|00cccccc|00bbbbbb|00aaaaaa] x 4)
        // merged: packed_dword([00000000|ddddddcc|ccccbbbb|bbaaaaaa] x 4)

        __m128i const merged = _pack(values);

        // merged = packed_byte([0XXX|0YYY|0ZZZ|0WWW])

        __m128i const shuf = _mm_setr_epi8(
                2,  1,  0,
                6,  5,  4,
               10,  9,  8,
               14, 13, 12,
              char(0xff), char(0xff), char(0xff), char(0xff)
        );

        // lower 12 bytes contains the result
        __m128i const shuffled = _mm_shuffle_epi8(merged, shuf);

#if 0
        // Note: on Core i5 maskmove is slower than bare write
        __m128i const mask = _mm_setr_epi8(
              char(0xff), char(0xff), char(0xff), char(0xff),
              char(0xff), char(0xff), char(0xff), char(0xff),
              char(0xff), char(0xff), char(0xff), char(0xff),
              char(0x00), char(0x00), char(0x00), char(0x00)
        );
        _mm_maskmoveu_si128(shuffled, mask, reinterpret_cast<char*>(out));
#else
        _mm_storeu_si128(reinterpret_cast<__m128i*>(out), shuffled);
#endif
        out += 12;
    }
}

#if defined(HAVE_BMI2_INSTRUCTIONS)
__m128i bswap_si128(__m128i const in) {
    return _mm_shuffle_epi8(in, _mm_setr_epi8(
                 3,  2,  1,  0,
                 7,  6,  5,  4,
                11, 10,  9,  8,
                15, 14, 13, 12
           ));
}

template <typename FN>
void decode_bmi2(FN lookup, const uint8_t* input, size_t size, uint8_t* output) {

    assert(size % 16 == 0);

    uint8_t* out = output;

    for (size_t i=0; i < size; i += 16) {

        __m128i in = _mm_loadu_si128(reinterpret_cast<__m128i const*>(input + i));
        __m128i values;

        try
        {
            values = bswap_si128(lookup(in));
        }
        catch (invalid_input& e)
        {
            const auto shift = e.offset;
            throw invalid_input(i + shift, input[i + shift]);
        }

        // input:  packed_dword([00dddddd|00cccccc|00bbbbbb|00aaaaaa] x 4)
        // merged: packed_dword([00000000|ddddddcc|ccccbbbb|bbaaaaaa] x 4)

        const uint64_t lo = _mm_extract_epi64(values, 0);
        const uint64_t hi = _mm_extract_epi64(values, 1);

        uint64_t t0 = _pext_u64(lo, 0x3f3f3f3f3f3f3f3f);
        uint64_t t1 = _pext_u64(hi, 0x3f3f3f3f3f3f3f3f);

        uint64_t b0, b1, b2;

        b0 = t0 & 0x0000ff0000ff;
        b1 = t0 & 0x00ff0000ff00;
        b2 = t0 & 0xff0000ff0000;
        t0 = (b0 << 16) | b1 | (b2 >> 16);

        b0 = t1 & 0x0000ff0000ff;
        b1 = t1 & 0x00ff0000ff00;
        b2 = t1 & 0xff0000ff0000;
        t1 = (b0 << 16) | b1 | (b2 >> 16);

        *reinterpret_cast<uint64_t*>(out + 0)  = t0;
        *reinterpret_cast<uint64_t*>(out + 6)  = t1;
        out += 12;
    }
}
#endif // defined(HAVE_BMI2_INSTRUCTIONS)

// The algorithm by aqrit. It uses a clever hashing of input bytes
inline void decode_aqrit(const uint8_t* input, size_t size, uint8_t* output)
{
    __m128i const delta_asso = _mm_setr_epi8(
            0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x0F
    );
    __m128i const delta_values = _mm_setr_epi8(
            int8_t(0x00), int8_t(0x00), int8_t(0x00), int8_t(0x13),
            int8_t(0x04), int8_t(0xBF), int8_t(0xBF), int8_t(0xB9),
            int8_t(0xB9), int8_t(0x00), int8_t(0x10), int8_t(0xC3),
            int8_t(0xBF), int8_t(0xBF), int8_t(0xB9), int8_t(0xB9)
    );
    __m128i const check_asso = _mm_setr_epi8(
            0x0D, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
            0x01, 0x01, 0x03, 0x07, 0x0B, 0x0B, 0x0B, 0x0F
    );
    __m128i const check_values = _mm_setr_epi8(
            int8_t(0x80), int8_t(0x80), int8_t(0x80), int8_t(0x80),
            int8_t(0xCF), int8_t(0xBF), int8_t(0xD5), int8_t(0xA6),
            int8_t(0xB5), int8_t(0x86), int8_t(0xD1), int8_t(0x80),
            int8_t(0xB1), int8_t(0x80), int8_t(0x91), int8_t(0x80)
    );

    for (size_t i=0; i < size; i += 16) {

        __m128i const src     = _mm_loadu_si128((__m128i*)(input + i));
        __m128i const shifted = _mm_srli_epi32(src, 3);

        __m128i const delta_hash = _mm_avg_epu8(_mm_shuffle_epi8(delta_asso, src), shifted);
        __m128i const check_hash = _mm_avg_epu8(_mm_shuffle_epi8(check_asso, src), shifted);

        __m128i const out = _mm_adds_epi8(_mm_shuffle_epi8(delta_values, delta_hash), src);
        __m128i const chk = _mm_adds_epi8(_mm_shuffle_epi8(check_values, check_hash), src);

        const int mask = _mm_movemask_epi8(chk);
        if (mask != 0) {
            const int pos = __builtin_ctz(mask);
            throw invalid_input{i + pos, input[i + pos]};
        }

        __m128i const pack_shuffle = _mm_setr_epi8(
            2,  1,  0,  6,  5,  4, 10,  9,
            8, 14, 13, 12, -1, -1, -1, -1
        );

        __m128i const t0 = _mm_maddubs_epi16(out, _mm_set1_epi32(0x01400140));
        __m128i const t1 = _mm_madd_epi16(t0, _mm_set1_epi32(0x00011000));
        __m128i const t2 = _mm_shuffle_epi8(t1, pack_shuffle);

        _mm_storeu_si128((__m128i*)output, t2);
        output += 12;
    }
}

// }}}

}
