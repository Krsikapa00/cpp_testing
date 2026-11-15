#include <cstdint>
#include <limits>

#include <unity.h>

#include "servo.hpp"
#include "units.hpp"

// PC tests for control::Servo against a fake PWM channel that records the pulses it's given.

using namespace units::literals;

struct FakePwm {
    static std::uint32_t initPulse;
    static std::uint32_t lastPulse;
    static int           writes;
    static void init(units::Microseconds p)     { initPulse = p.count(); lastPulse = p.count(); }
    static void setPulse(units::Microseconds p) { lastPulse = p.count(); ++writes; }
};
std::uint32_t FakePwm::initPulse = 0;
std::uint32_t FakePwm::lastPulse = 0;
int           FakePwm::writes    = 0;

// 1000 to 2000 us over -50 to +50 degrees: 10 us per degree, centre 1500.
constexpr control::ServoCalibration cal = { -50_deg, 50_deg, 1000_us, 2000_us };

// Checked at compile time, which is the point of isValid() being constexpr.
static_assert(control::isValid(cal), "test calibration should be valid");
static_assert(!control::isValid(control::ServoCalibration{ 50_deg, -50_deg, 1000_us, 2000_us }),
              "min angle above max angle");
static_assert(!control::isValid(control::ServoCalibration{ -50_deg, 50_deg, 100_us, 2000_us }),
              "pulse far below any real servo");
static_assert(!control::isValid(control::ServoCalibration{ -50_deg, 50_deg, 1500_us, 1500_us }),
              "equal pulses would make every angle the same");

void setUp() { FakePwm::initPulse = 0; FakePwm::lastPulse = 0; FakePwm::writes = 0; }
void tearDown() {}

static void test_init_starts_at_centre()
{
    control::Servo<FakePwm> s(cal);
    s.init();
    TEST_ASSERT_EQUAL_UINT32(1500, FakePwm::initPulse);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, s.commanded().value());
}

static void test_maps_linearly()
{
    control::Servo<FakePwm> s(cal);
    s.set(0_deg);    TEST_ASSERT_EQUAL_UINT32(1500, FakePwm::lastPulse);
    s.set(25_deg);   TEST_ASSERT_EQUAL_UINT32(1750, FakePwm::lastPulse);
    s.set(-50_deg);  TEST_ASSERT_EQUAL_UINT32(1000, FakePwm::lastPulse);
    s.set(50_deg);   TEST_ASSERT_EQUAL_UINT32(2000, FakePwm::lastPulse);
}

static void test_rounds_to_nearest_microsecond()
{
    control::Servo<FakePwm> s(cal);
    s.set(units::Degrees(0.04f));    // 1500.4 us
    TEST_ASSERT_EQUAL_UINT32(1500, FakePwm::lastPulse);
    s.set(units::Degrees(0.06f));    // 1500.6 us
    TEST_ASSERT_EQUAL_UINT32(1501, FakePwm::lastPulse);
}

static void test_clamps_to_limits_and_reports_it()
{
    control::Servo<FakePwm> s(cal);
    units::Degrees got = s.set(80_deg);
    TEST_ASSERT_EQUAL_FLOAT(50.0f, got.value());
    TEST_ASSERT_EQUAL_UINT32(2000, FakePwm::lastPulse);

    got = s.set(-1000_deg);
    TEST_ASSERT_EQUAL_FLOAT(-50.0f, got.value());
    TEST_ASSERT_EQUAL_UINT32(1000, FakePwm::lastPulse);
}

static void test_infinity_clamps()
{
    control::Servo<FakePwm> s(cal);
    s.set(units::Degrees(std::numeric_limits<float>::infinity()));
    TEST_ASSERT_EQUAL_UINT32(2000, FakePwm::lastPulse);
}

static void test_nan_is_ignored_and_servo_holds()
{
    control::Servo<FakePwm> s(cal);
    s.set(20_deg);
    const int writesBefore = FakePwm::writes;
    units::Degrees got = s.set(units::Degrees(std::numeric_limits<float>::quiet_NaN()));
    TEST_ASSERT_EQUAL_FLOAT(20.0f, got.value());
    TEST_ASSERT_EQUAL_INT(writesBefore, FakePwm::writes);    // no new pulse sent
    TEST_ASSERT_EQUAL_UINT32(1700, FakePwm::lastPulse);
}

static void test_reversed_servo()
{
    constexpr control::ServoCalibration reversed = { -50_deg, 50_deg, 2000_us, 1000_us };
    static_assert(control::isValid(reversed), "a reversed servo is valid");
    control::Servo<FakePwm> s(reversed);
    s.set(-50_deg);  TEST_ASSERT_EQUAL_UINT32(2000, FakePwm::lastPulse);
    s.set(25_deg);   TEST_ASSERT_EQUAL_UINT32(1250, FakePwm::lastPulse);
}

static void test_literals()
{
    TEST_ASSERT_EQUAL_FLOAT(30.0f, (30_deg).value());
    TEST_ASSERT_EQUAL_FLOAT(-12.5f, (-12.5_deg).value());
    TEST_ASSERT_EQUAL_UINT32(1500, (1500_us).count());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_init_starts_at_centre);
    RUN_TEST(test_maps_linearly);
    RUN_TEST(test_rounds_to_nearest_microsecond);
    RUN_TEST(test_clamps_to_limits_and_reports_it);
    RUN_TEST(test_infinity_clamps);
    RUN_TEST(test_nan_is_ignored_and_servo_holds);
    RUN_TEST(test_reversed_servo);
    RUN_TEST(test_literals);
    return UNITY_END();
}
