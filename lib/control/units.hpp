#pragma once

// Strong types for physical units, so an angle can't be passed where a pulse width is expected.
//
//     servo.set(30);                  // error: no conversion from int to Degrees
//     servo.set(units::Degrees(30));  // fine
//     using namespace units::literals;
//     servo.set(30_deg);              // fine, and reads like the datasheet
//
// Each type is a float or an integer in a class with an explicit constructor. It compiles to the
// bare number: no storage or run-time cost, only a compile-time check.
// Portable: no FreeRTOS and no registers.

#include <cstdint>

namespace units {

class Degrees {
public:
    constexpr explicit Degrees(float value) : value_(value) {}
    constexpr float value() const { return value_; }

private:
    float value_;
};

constexpr Degrees operator-(Degrees a)            { return Degrees(-a.value()); }
constexpr Degrees operator+(Degrees a, Degrees b) { return Degrees(a.value() + b.value()); }
constexpr Degrees operator-(Degrees a, Degrees b) { return Degrees(a.value() - b.value()); }
constexpr bool operator<(Degrees a, Degrees b)    { return a.value() < b.value(); }
constexpr bool operator>(Degrees a, Degrees b)    { return a.value() > b.value(); }

// Pulse widths and other short times. Unsigned, because a negative width means nothing.
class Microseconds {
public:
    constexpr explicit Microseconds(std::uint32_t count) : count_(count) {}
    constexpr std::uint32_t count() const { return count_; }

private:
    std::uint32_t count_;
};

constexpr bool operator==(Microseconds a, Microseconds b) { return a.count() == b.count(); }

// User-defined literals, C++11. The space after "" is needed by C++11 compilers such as the
// host's GCC 5.4. Suffixes without a leading underscore are reserved for the standard library.
namespace literals {

constexpr Degrees operator"" _deg(long double value) {
    return Degrees(static_cast<float>(value));
}
constexpr Degrees operator"" _deg(unsigned long long value) {
    return Degrees(static_cast<float>(value));
}
constexpr Microseconds operator"" _us(unsigned long long value) {
    return Microseconds(static_cast<std::uint32_t>(value));
}

} // namespace literals
} // namespace units
