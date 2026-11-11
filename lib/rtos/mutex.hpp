#pragma once

#include "FreeRTOS.h"
#include "semphr.h"

namespace rtos {

class Mutex {
public:
    Mutex() = default;
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

    void init() {
        handle_ = xSemaphoreCreateMutexStatic(&storage_);   // no heap
        configASSERT(handle_ != nullptr);
    }
    // We want the user to always check the result since this could involve critical sections
    __attribute__((warn_unused_result))
    bool lock(TickType_t timeout = portMAX_DELAY) {
        configASSERT(!xPortIsInsideInterrupt());

        return xSemaphoreTake(handle_, timeout) == pdTRUE;
    }
    void unlock() {
        configASSERT(!xPortIsInsideInterrupt());

        BaseType_t ok = xSemaphoreGive(handle_);
        configASSERT(ok != pdPASS);

        (void) ok;
    }

private:
    StaticSemaphore_t storage_;
    SemaphoreHandle_t handle_;
};

class LockGuard {
public:
    explicit LockGuard(Mutex &m): mutex_(m), locked_(mutex_.lock(portMAX_DELAY)) {}

    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;
    LockGuard(LockGuard&&) = delete;
    LockGuard& operator=(const LockGuard&&) = delete;

    ~LockGuard() { if (locked_) { mutex_.unlock(); } }

private:
    Mutex &mutex_;
    bool locked_;
};

} //namespace rtos