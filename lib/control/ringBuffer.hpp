#pragma once

#include <atomic>
#include <cstddef>
#include <array>

namespace control {

template<typename T, std::size_t N>
class RingBuffer {
    static_assert(N >= 2, "RingBuffer must have size greater than 1");
    static_assert((N & (N - 1)) == 0 , "RingBuffer size must be power of 2");

public:

    bool push(const T& item) {
        std::size_t tmpHead = head_.load(std::memory_order_relaxed);
        if (tmpHead - tail_.load(std::memory_order_acquire) == N) {
            return false;
        }
        buf_[tmpHead++ % N] = item;
        head_.store(tmpHead, std::memory_order_release);

        return true;
    }

    bool pop(T& item) {
        std::size_t tmpTail = tail_.load(std::memory_order_relaxed);
        if (tmpTail == head_.load(std::memory_order_acquire)) {
            return false;
        }

        item = buf_[tmpTail++ % N];
        tail_.store(tmpTail, std::memory_order_release);

        return true;
    }

private:
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
    std::array<T, N>    buf_{};
};

}