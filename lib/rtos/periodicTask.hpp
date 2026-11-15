#pragma once

// A task that calls a function at a fixed rate and measures how well it keeps that rate.
//
//     static rtos::PeriodicTask<256> control;
//     control.start("Control", tskIDLE_PRIORITY + 4, pdMS_TO_TICKS(20), &Sweep::entry, &sweep);
//     ...
//     rtos::LoopTiming t;
//     if (control.receiveTiming(t, portMAX_DELAY)) { /* log it */ }
//
// The step is a plain function pointer plus a context pointer, not std::function (which can
// allocate) and not a base class with a virtual step() (inheritance). To run a member function,
// pass a static member that casts the context back:
//
//     static void entry(void* self) { static_cast<Sweep*>(self)->step(); }
//
// Timing comes from the DWT cycle counter, so call hal::CycleCounter::init() before the scheduler
// starts. Each wake-up is timed against the last one (period) and against the end of the step
// (work). Once per reportEvery periods the totals go into a one-item queue and start again.
// Sending never waits: if the reader hasn't taken the last report, the new one is dropped. The
// control loop must never block on its observer.
//
// Like Task, it must have static storage: the kernel keeps pointers into it.

#include "FreeRTOS.h"
#include "task.h"
#include "custTask.hpp"
#include "queue.hpp"
#include "cycleCounter.hpp"
#include <cstdint>

namespace rtos {

// Plain data, so the kernel can copy it through the queue with memcpy.
struct LoopTiming {
    std::uint32_t count;             // periods measured
    std::uint32_t periodMinCycles;   // shortest time between wake-ups
    std::uint32_t periodMaxCycles;   // longest
    std::uint32_t workMaxCycles;     // longest step
    std::uint32_t overruns;          // periods where the step ran past the next deadline

    void reset() {
        count = 0;
        periodMinCycles = UINT32_MAX;
        periodMaxCycles = 0;
        workMaxCycles = 0;
        overruns = 0;
    }
};

template<std::uint32_t StackWords>
class PeriodicTask {
public:
    using StepFn = void (*)(void* ctx);

    static constexpr std::uint32_t reportEvery = 50;   // once a second at 50 Hz

    PeriodicTask() = default;                          // no initializers: stays in .bss, like Task
    PeriodicTask(const PeriodicTask&)            = delete;
    PeriodicTask& operator=(const PeriodicTask&) = delete;

    void start(const char* name, UBaseType_t priority, TickType_t period, StepFn step, void* ctx) {
        configASSERT(step != nullptr && period > 0);
        period_ = period;
        step_   = step;
        ctx_    = ctx;
        reports_.init();
        task_.start(&PeriodicTask::entry, this, name, priority);
    }

    // For a lower-priority task to read the timing. Returns false on timeout.
    bool receiveTiming(LoopTiming& out, TickType_t timeout) { return reports_.receive(out, timeout); }

    TaskHandle_t handle() const { return task_.handle(); }

private:
    // The same trampoline the header comment shows, used here to run run() as the task.
    static void entry(void* self) { static_cast<PeriodicTask*>(self)->run(); }

    void run() {
        LoopTiming t;
        t.reset();
        TickType_t    lastWake  = xTaskGetTickCount();
        std::uint32_t lastCycle = 0;
        bool          first     = true;

        while (true) {
            // pdFALSE means the deadline had already passed, so it didn't wait at all.
            if (xTaskDelayUntil(&lastWake, period_) == pdFALSE) {
                ++t.overruns;
            }
            const std::uint32_t woke = hal::CycleCounter::now();
            step_(ctx_);
            const std::uint32_t done = hal::CycleCounter::now();

            if (!first) {                                  // the first period started from start()
                const std::uint32_t period = woke - lastCycle;
                if (period < t.periodMinCycles) { t.periodMinCycles = period; }
                if (period > t.periodMaxCycles) { t.periodMaxCycles = period; }
                if (done - woke > t.workMaxCycles) { t.workMaxCycles = done - woke; }
                if (++t.count == reportEvery) {
                    (void)reports_.send(t, 0);             // never wait on the reader
                    t.reset();
                }
            }
            first = false;
            lastCycle = woke;
        }
    }

    Task<StackWords>    task_;
    Queue<LoopTiming, 1> reports_;
    TickType_t          period_;
    StepFn              step_;
    void*               ctx_;
};

} // namespace rtos
