#pragma once

// A hobby position servo such as the SG90: an angle command becomes a pulse width, sent every
// 20 ms by a PWM channel. The servo is where the limits live, so nothing above it can drive the
// horn past its end stops.
//
//     constexpr control::ServoCalibration sg90 = { -60_deg, 60_deg, 1167_us, 1833_us };
//     static_assert(control::isValid(sg90), "SG90 calibration");
//     static control::Servo<ServoPwm> servo(sg90);
//     servo.init();                    // starts the PWM at the centre angle
//     servo.set(25_deg);               // returns the angle actually commanded, after clamping
//
// Composition by template parameter: Pwm is any type with
//     static void init(units::Microseconds initialPulse);
//     static void setPulse(units::Microseconds width);
// On the board that's hal::PwmChannel. In the PC tests it's a fake that records the pulse, so
// this header includes no registers and builds on the PC.

#include "units.hpp"
#include <cstdint>

namespace control {

// The two ends of the allowed travel, and the pulse that reaches each. Angles in between map
// linearly. The ends double as the limits: set() never goes past them.
// A reversed servo is just maxPulse < minPulse.
struct ServoCalibration {
    units::Degrees      minAngle;
    units::Degrees      maxAngle;
    units::Microseconds minPulse;   // pulse width that gives minAngle
    units::Microseconds maxPulse;   // pulse width that gives maxAngle
};

// For a static_assert on a calibration. Hobby servos accept roughly 500 to 2500 us, and anything
// outside that is a typo, not a servo.
constexpr bool isValid(const ServoCalibration& c) {
    return c.minAngle < c.maxAngle
        && c.minPulse.count() >= 500u && c.minPulse.count() <= 2500u
        && c.maxPulse.count() >= 500u && c.maxPulse.count() <= 2500u
        && !(c.minPulse == c.maxPulse);
}

template<typename Pwm>
class Servo {
public:
    // constexpr, so a static Servo is built at compile time: no constructor runs before main.
    constexpr explicit Servo(const ServoCalibration& cal)
        : cal_(cal), commanded_(units::Degrees((cal.minAngle.value() + cal.maxAngle.value()) * 0.5f)) {}

    Servo(const Servo&)            = delete;   // two objects driving one PWM channel would fight
    Servo& operator=(const Servo&) = delete;

    // Starts the PWM at the centre of the travel.
    void init() { Pwm::init(pulseFor(commanded_)); }

    // Clamps to the calibrated travel, sends the pulse and returns the angle actually commanded.
    // A NaN target is ignored and the servo holds, rather than slamming to one end: in a control
    // loop a NaN usually means a bad sensor read upstream.
    units::Degrees set(units::Degrees target) {
        const float v = target.value();
        if (v != v) {                                  // NaN is the only value not equal to itself
            return commanded_;
        }
        units::Degrees a = target;
        if (a < cal_.minAngle) { a = cal_.minAngle; }
        if (a > cal_.maxAngle) { a = cal_.maxAngle; }
        commanded_ = a;
        Pwm::setPulse(pulseFor(a));
        return a;
    }

    units::Degrees commanded() const { return commanded_; }
    units::Degrees minAngle() const { return cal_.minAngle; }
    units::Degrees maxAngle() const { return cal_.maxAngle; }

    // The pulse for an angle already inside the travel, rounded to the nearest microsecond.
    units::Microseconds pulseFor(units::Degrees a) const {
        const float lo   = static_cast<float>(cal_.minPulse.count());
        const float hi   = static_cast<float>(cal_.maxPulse.count());
        const float frac = (a.value() - cal_.minAngle.value())
                         / (cal_.maxAngle.value() - cal_.minAngle.value());
        return units::Microseconds(static_cast<std::uint32_t>(lo + frac * (hi - lo) + 0.5f));
    }

private:
    ServoCalibration cal_;
    units::Degrees   commanded_;
};

} // namespace control
