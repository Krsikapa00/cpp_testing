#include <climits>
#include <cstdint>

#include <unity.h>

#include "line.hpp"

// PC tests for logging::Line, the printf-free formatter the Logger uses.

void setUp() {}
void tearDown() {}

static void test_empty_line()
{
    logging::Line<8> line;
    TEST_ASSERT_EQUAL_STRING("", line.c_str());
    TEST_ASSERT_EQUAL_UINT32(0, line.size());
    TEST_ASSERT_FALSE(line.truncated());
}

static void test_text_and_chars()
{
    logging::Line<16> line;
    line.append("ab").append('c').append("");
    TEST_ASSERT_EQUAL_STRING("abc", line.c_str());
    TEST_ASSERT_EQUAL_UINT32(3, line.size());
}

static void test_unsigned_values()
{
    logging::Line<64> line;
    line.append(0u).append(' ').append(7u).append(' ').append(UINT32_MAX).append(' ').append(UINT64_MAX);
    TEST_ASSERT_EQUAL_STRING("0 7 4294967295 18446744073709551615", line.c_str());
}

static void test_signed_values_including_most_negative()
{
    logging::Line<64> line;
    line.append(-1).append(' ').append(42).append(' ').append(INT32_MIN).append(' ').append(INT64_MIN);
    TEST_ASSERT_EQUAL_STRING("-1 42 -2147483648 -9223372036854775808", line.c_str());
}

static void test_small_integer_types_print_as_numbers()
{
    logging::Line<32> line;
    line.append(static_cast<std::uint8_t>(200)).append(' ').append(static_cast<std::int8_t>(-128))
        .append(' ').append(static_cast<std::int16_t>(-300)).append(' ').append(true);
    TEST_ASSERT_EQUAL_STRING("200 -128 -300 1", line.c_str());
}

static void test_hex()
{
    logging::Line<32> line;
    line.appendHex(0x2008, 4).append(' ').appendHex(0xA80004A0u).append(' ').appendHex(0x7, 0);
    TEST_ASSERT_EQUAL_STRING("0x2008 0xA80004A0 0x00000007", line.c_str());
}

static void test_exact_fit_is_not_truncated()
{
    logging::Line<5> line;
    line.append("t=").append(123u);
    TEST_ASSERT_EQUAL_STRING("t=123", line.c_str());
    TEST_ASSERT_FALSE(line.truncated());
}

static void test_overflow_keeps_what_fits_and_flags_it()
{
    logging::Line<5> line;
    line.append("t=").append(123456u);
    TEST_ASSERT_EQUAL_STRING("t=123", line.c_str());
    TEST_ASSERT_EQUAL_UINT32(5, line.size());
    TEST_ASSERT_TRUE(line.truncated());
}

static void test_clear_resets_everything()
{
    logging::Line<3> line;
    line.append("abcdef");
    line.clear();
    TEST_ASSERT_FALSE(line.truncated());
    line.append("xy");
    TEST_ASSERT_EQUAL_STRING("xy", line.c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_empty_line);
    RUN_TEST(test_text_and_chars);
    RUN_TEST(test_unsigned_values);
    RUN_TEST(test_signed_values_including_most_negative);
    RUN_TEST(test_small_integer_types_print_as_numbers);
    RUN_TEST(test_hex);
    RUN_TEST(test_exact_fit_is_not_truncated);
    RUN_TEST(test_overflow_keeps_what_fits_and_flags_it);
    RUN_TEST(test_clear_resets_everything);
    return UNITY_END();
}
