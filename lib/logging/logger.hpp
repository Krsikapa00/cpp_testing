#pragma once

// Line logger shared by several tasks, writing to a static byte sink such as hal::UartStatic.
//
//     using Console = hal::UartStatic<hal::Usart2Port, 115200>;
//     static logging::Logger<Console> logger;      // global or static: it holds a kernel object
//     logger.init();                               // in main(), before the tasks start
//     logger.log("hello");                         // from any task; "\r\n" is added
//
// - Each line goes out whole, with "\r\n" added, or not at all. A mutex keeps lines from
//   different tasks apart.
// - It never waits for the UART. If a line doesn't fit in the sink's buffer, or was truncated
//   while being built, it's dropped and counted in dropped().
// - It can wait for the mutex, but only while another task copies a line into the buffer, which
//   takes microseconds. Priority inheritance stops a low-priority holder being preempted meanwhile.
// - Tasks only, after the scheduler has started. A mutex can't be taken from an interrupt
//   (Mutex::lock asserts that).
//
// Sink must provide:
//     static std::size_t freeSpace();
//     static bool write(const char* data, std::size_t len);

#include "line.hpp"
#include "mutex.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace logging {

template<typename Sink>
class Logger {
public:
    // No member initializers on purpose. Mutex's members have none, so if dropped_ had `{0}` this
    // constructor couldn't be constexpr, and every Logger would get a constructor that runs before
    // main (the same trap as Task's handle). Static storage zero-initializes dropped_ instead.
    Logger() = default;
    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;

    void init() { mutex_.init(); }

    bool log(const char* text) { return writeLine(text, std::strlen(text)); }

    template<std::size_t N>
    bool log(const Line<N>& line) {
        if (line.truncated()) {
            countDrop();
            return false;
        }
        return writeLine(line.c_str(), line.size());
    }

    std::uint32_t dropped() const { return dropped_.load(std::memory_order_relaxed); }

private:
    bool writeLine(const char* text, std::size_t len) {
        static const char crlf[] = "\r\n";

        rtos::LockGuard lock(mutex_);
        if (!lock) {                     // can't happen with portMAX_DELAY, but don't write unlocked
            countDrop();
            return false;
        }
        // While we hold the mutex we're the sink's only producer, and its free space can only grow
        // (the interrupt drains it). So after this one check, both writes are certain to succeed.
        if (len + 2 > Sink::freeSpace()) {
            countDrop();
            return false;
        }
        Sink::write(text, len);
        Sink::write(crlf, 2);
        return true;
    }

    // Atomic because truncated lines are counted outside the lock.
    void countDrop() { dropped_.fetch_add(1, std::memory_order_relaxed); }

    rtos::Mutex                mutex_;
    std::atomic<std::uint32_t> dropped_;
};

} // namespace logging
