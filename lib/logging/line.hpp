#pragma once

// A fixed-size line of text built on the stack, with integer formatting and no printf.
// One snprintf("%u") links newlib's malloc and _sbrk, so this does the formatting itself.
// Portable: no FreeRTOS and no registers, so it builds and is tested on the PC (test/test_line/).
//
//     logging::Line<48> line;
//     line.append("tick=").append(xTaskGetTickCount()).append(" cr1=").appendHex(USART2->CR1, 4);
//     logger.log(line);
//
// Appending past the end keeps what fitted and sets truncated(). Logger drops truncated lines
// rather than send half of one.

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace logging {

template<std::size_t Capacity>
class Line {
    static_assert(Capacity > 0, "A Line needs room for at least one character");

public:
    Line() : len_(0), truncated_(false) { buf_[0] = '\0'; }

    Line& append(const char* text) {
        while (*text != '\0') {
            put(*text++);
        }
        return *this;
    }

    // A non-template overload wins over the templates below, so a char is appended as a
    // character. Every other integer type, including uint8_t, is printed as a number.
    Line& append(char c) {
        put(c);
        return *this;
    }

    // Integers of any width and signedness, in decimal. A template rather than overloads for
    // int, long and so on: on Arm, uint32_t is `unsigned long` and size_t is `unsigned int`, so
    // a fixed set of overloads makes calls like append(5) ambiguous on one compiler or the other.
    template<typename T>
    typename std::enable_if<std::is_integral<T>::value && std::is_unsigned<T>::value, Line&>::type
    append(T value) {
        return appendUnsigned(value);
    }

    template<typename T>
    typename std::enable_if<std::is_integral<T>::value && std::is_signed<T>::value, Line&>::type
    append(T value) {
        typedef typename std::make_unsigned<T>::type U;
        U magnitude = static_cast<U>(value);
        if (value < 0) {
            put('-');
            // Negate in unsigned arithmetic, so the most negative value doesn't overflow.
            magnitude = static_cast<U>(U(0) - magnitude);
        }
        return appendUnsigned(magnitude);
    }

    // Fixed-width hex with a 0x prefix, for registers: appendHex(0x2008, 4) gives "0x2008".
    Line& appendHex(std::uint32_t value, unsigned width = 8) {
        static const char digits[] = "0123456789ABCDEF";
        if (width == 0 || width > 8) {
            width = 8;
        }
        put('0');
        put('x');
        for (unsigned i = width; i-- > 0;) {
            put(digits[(value >> (i * 4u)) & 0xFu]);
        }
        return *this;
    }

    void clear() {
        len_ = 0;
        truncated_ = false;
        buf_[0] = '\0';
    }

    const char* c_str() const { return buf_; }
    std::size_t size() const { return len_; }
    bool truncated() const { return truncated_; }
    static constexpr std::size_t capacity() { return Capacity; }

private:
    void put(char c) {
        if (len_ < Capacity) {
            buf_[len_++] = c;
            buf_[len_] = '\0';
        } else {
            truncated_ = true;
        }
    }

    template<typename U>
    Line& appendUnsigned(U value) {
        // digits10 is one short of the longest value: 9 for uint32_t, whose maximum has 10 digits.
        char reversed[std::numeric_limits<U>::digits10 + 1];
        std::size_t n = 0;
        do {
            reversed[n++] = static_cast<char>('0' + value % 10u);
            value = static_cast<U>(value / 10u);
        } while (value != 0);
        while (n > 0) {
            put(reversed[--n]);
        }
        return *this;
    }

    char        buf_[Capacity + 1];   // + 1 keeps it null-terminated for c_str()
    std::size_t len_;
    bool        truncated_;
};

} // namespace logging
