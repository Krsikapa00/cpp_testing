#include <array>
#include <cstdint>
#include <type_traits>

#include <unity.h>

// Checks that the PC test setup works and is really building as C++11.
// Delete this once real tests exist in Project 2.

void setUp() {}
void tearDown() {}

static void test_compiles_as_cpp11()
{
    TEST_ASSERT_EQUAL_INT32(201103L, __cplusplus);
}

static void test_cpp11_library_features_available()
{
    std::array<std::uint8_t, 4> values = {{1, 2, 3, 4}};
    static_assert(std::is_trivially_copyable<std::array<std::uint8_t, 4>>::value,
                  "std::array of bytes should be trivially copyable");
    TEST_ASSERT_EQUAL_UINT8(4, values[3]);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_compiles_as_cpp11);
    RUN_TEST(test_cpp11_library_features_available);
    return UNITY_END();
}
