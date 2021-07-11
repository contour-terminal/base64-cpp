// SPDX-License-Identifier: Apache-2.0
#include <base64-cpp/decode.hpp>
#include <catch2/catch_all.hpp>

#include <string>
#include <string_view>

using namespace std::string_literals;
using namespace std::string_view_literals;

TEST_CASE("base64.decode", "[simple]")
{
    auto const de = "abcd"sv;
    auto const di = "YWJjZA=="sv;
    auto const dO = base64::decode(di);
    CHECK(de == dO);

    auto const ae = "a"sv;
    auto const ai = "YQ=="sv;
    auto const ao = base64::decode(ai);
    CHECK(ae == ao);

    auto const be = "ab"sv;
    auto const bi = "YWI="sv;
    auto const bo = base64::decode(bi);
    CHECK(be == bo);

    CHECK("abc" == base64::decode("YWJj"sv));

    CHECK("foo:bar" == base64::decode("Zm9vOmJhcg=="sv));
}

TEST_CASE("accelerated-1")
{
    auto const expected = "1234567890ab"s;
    auto const input    = "MTIzNDU2Nzg5MGFi"sv;
    auto const output   = base64::decode(input);
    CHECK(output == expected);
}

TEST_CASE("accelerated-2")
{
    auto const expected = "123456789012ABCDEF1234PQ"s;
    auto const input    = "MTIzNDU2Nzg5MDEyQUJDREVGMTIzNFBR"sv;
    auto const output   = base64::decode(input);
    CHECK(output == expected);
}

TEST_CASE("accelerated-2-remainder-1")
{
    auto const expected = "123456789012ABCDEF1234PQa"s;
    auto const input    = "MTIzNDU2Nzg5MDEyQUJDREVGMTIzNFBRYQ=="sv;
    auto const output   = base64::decode(input);
    CHECK(output == expected);
}

TEST_CASE("accelerated-2-remainder-2")
{
    auto const expected = "123456789012ABCDEF1234PQab"s;
    auto const input    = "MTIzNDU2Nzg5MDEyQUJDREVGMTIzNFBRYWI="sv;
    auto const output   = base64::decode(input);
    CHECK(output == expected);
}

TEST_CASE("accelerated-2-remainder-3")
{
    auto const expected = "123456789012ABCDEF1234PQabc"s;
    auto const input    = "MTIzNDU2Nzg5MDEyQUJDREVGMTIzNFBRYWJj"sv;
    auto const output   = base64::decode(input);
    CHECK(output == expected);
}
