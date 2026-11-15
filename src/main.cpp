// Project 3: an SG90 servo on TIM3_CH1 (PA6, Arduino D12), driven by a 50 Hz control task whose
// timing is measured with the DWT cycle counter and logged over USART2 once a second.
//
// Wiring: servo signal (orange) to PA6, servo power (red) to the separate 5 V supply, and servo
// ground (brown) to both the supply's ground and a Nucleo GND pin. Without the shared ground the
// signal has no reference and the servo jitters or ignores it.
//
// B1 pauses and resumes the sweep. LD2 blinks every 500 ms, so board-check can see the kernel run.

#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "clock.hpp"
#include "cycleCounter.hpp"
#include "custTask.hpp"
#include "gpio.hpp"
#include "logger.hpp"
#include "periodicTask.hpp"
#include "pwm.hpp"
#include "servo.hpp"
#include "uart.hpp"
#include "units.hpp"
#include <atomic>
#include <cstdint>

using namespace units::literals;

// ---- Logging ---------------------------------------------------------------------------------
using Console = hal::UartStatic<hal::Usart2Port, 115200>;
extern "C" void USART2_IRQHandler(void) { Console::onInterrupt(); }
static logging::Logger<Console> logger;

// ---- Servo -----------------------------------------------------------------------------------
using ServoTimer = hal::PwmTimer<hal::Tim3, 20000>;                            // 50 Hz frames, 1 us steps
using ServoPwm   = hal::PwmChannel<ServoTimer, 1, hal::Gpio<GPIOA_BASE, 6>, 2>; // TIM3_CH1 on PA6, AF2

// The SG90 datasheet's nominal 1000-2000 us for -90..+90 degrees, limited to +-60 degrees to keep
// clear of the end stops. Real SG90s vary: experiment 4 in the brief measures yours.
constexpr control::ServoCalibration sg90 = { -60_deg, 60_deg, 1167_us, 1833_us };
static_assert(control::isValid(sg90), "SG90 calibration is out of range");

static control::Servo<ServoPwm> servo(sg90);

// ---- Button ----------------------------------------------------------------------------------
static std::atomic<bool> btnPressed{false};

extern "C" void EXTI15_10_IRQHandler(void)
{
    if (EXTI->PR & EXTI_PR_PR13) {
        btnPressed.store(true, std::memory_order_relaxed);
        EXTI->PR = EXTI_PR_PR13;
    }
}

static void configureButton()
{
    using Btn = hal::Gpio<GPIOC_BASE, 13>;
    Btn::enableClock();
    Btn::setMode(hal::PinMode::Input);

    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    (void)RCC->APB2ENR;
    // EXTICR4 holds EXTI12 to EXTI15. Clear only EXTI13's field, then route it to port C.
    SYSCFG->EXTICR[3] = (SYSCFG->EXTICR[3] & ~SYSCFG_EXTICR4_EXTI13) | SYSCFG_EXTICR4_EXTI13_PC;

    EXTI->IMR  |= EXTI_IMR_MR13;
    EXTI->FTSR |= EXTI_FTSR_TR13;
    EXTI->RTSR &= ~EXTI_RTSR_TR13;

    NVIC_SetPriority(EXTI15_10_IRQn, 5);
    NVIC_EnableIRQ(EXTI15_10_IRQn);
}

// ---- The control step ------------------------------------------------------------------------
// Sweeps the servo back and forth between +-amplitude. It's the stand-in for a flight
// controller's attitude loop: read inputs (the button), compute (the next angle), drive outputs.
class Sweep {
public:
    constexpr Sweep(control::Servo<ServoPwm>& servo, units::Degrees amplitude, units::Degrees step)
        : servo_(servo), amplitude_(amplitude.value()), step_(step.value()), angle_(0.0f),
          periodsSinceToggle_(0), paused_(false) {}

    Sweep(const Sweep&)            = delete;
    Sweep& operator=(const Sweep&) = delete;

    // The trampoline PeriodicTask calls: a plain function, with the object as its context.
    static void entry(void* self) { static_cast<Sweep*>(self)->step(); }

    bool paused() const { return paused_.load(std::memory_order_relaxed); }

private:
    static constexpr std::uint32_t debouncePeriods = 10;   // 200 ms at 50 Hz

    void step() {
        if (periodsSinceToggle_ < debouncePeriods) {
            ++periodsSinceToggle_;
        }
        // exchange() reads and clears in one step, so a press can't slip in between.
        if (btnPressed.exchange(false, std::memory_order_relaxed) && periodsSinceToggle_ >= debouncePeriods) {
            paused_.store(!paused(), std::memory_order_relaxed);
            periodsSinceToggle_ = 0;
        }
        if (paused()) {
            return;                                        // the PWM keeps sending the last pulse
        }

        angle_ += step_;
        if (angle_ > amplitude_ || angle_ < -amplitude_) {
            step_ = -step_;                                // turn round at each end
            angle_ += 2.0f * step_;
        }
        servo_.set(units::Degrees(angle_));
    }

    control::Servo<ServoPwm>& servo_;
    float                     amplitude_;
    float                     step_;
    float                     angle_;
    std::uint32_t             periodsSinceToggle_;
    std::atomic<bool>         paused_;                     // read by the monitor task
};

// 45 degrees each way at 0.6 degrees per 20 ms: 30 degrees a second, 3 s per side.
static Sweep sweep(servo, 45_deg, 0.6_deg);

// ---- Tasks -----------------------------------------------------------------------------------
static rtos::PeriodicTask<256> controlTask;
static rtos::Task<256>         monitorTask;
static rtos::Task<configMINIMAL_STACK_SIZE> blinkTask;

// Logs the control loop's timing once a second, and stack and drop counts every 5 s. It blocks
// on the timing queue, so it runs exactly when there's something to report.
static void monitorFn(void* arg)
{
    (void)arg;
    logger.log("boot: servo on PA6, control loop at 50 Hz");

    std::uint32_t reports = 0;
    rtos::LoopTiming t;
    while (true) {
        if (!controlTask.receiveTiming(t, portMAX_DELAY)) {
            continue;
        }
        using hal::CycleCounter;
        logging::Line<128> line;
        line.append("ctl n=").append(t.count)
            .append(" period_us min=").append(CycleCounter::toUs(t.periodMinCycles))
            .append(" max=").append(CycleCounter::toUs(t.periodMaxCycles))
            .append(" jitter_ns=").append(CycleCounter::toNs(t.periodMaxCycles - t.periodMinCycles))
            .append(" work_ns max=").append(CycleCounter::toNs(t.workMaxCycles))
            .append(" overruns=").append(t.overruns)
            .append(" pulse_us=").append(ServoPwm::pulse().count())
            .append(sweep.paused() ? " paused" : "");
        logger.log(line);

        if (++reports % 5 == 0) {
            // Free stack in words: the lowest it has ever been. Under about 20 is too close.
            logging::Line<96> status;
            status.append("status drops=").append(logger.dropped())
                  .append(" stack_free ctl=").append(uxTaskGetStackHighWaterMark(controlTask.handle()))
                  .append(" mon=").append(uxTaskGetStackHighWaterMark(nullptr))
                  .append(" blink=").append(uxTaskGetStackHighWaterMark(blinkTask.handle()));
            logger.log(status);
        }
    }
}

static void blinkFn(void* arg)
{
    (void)arg;
    using Led = hal::Gpio<GPIOA_BASE, 5>;
    Led::enableClock();
    Led::setMode(hal::PinMode::Output);

    TickType_t last = xTaskGetTickCount();
    while (true) {
        xTaskDelayUntil(&last, pdMS_TO_TICKS(500));
        Led::toggle();
    }
}

int main()
{
    hal::clock::init();
    hal::CycleCounter::init();
    Console::init(6);
    logger.init();
    configureButton();

    ServoTimer::init();
    servo.init();                     // PWM starts now, at the centre angle

    // The control loop gets the highest priority (configMAX_PRIORITIES - 1), so only interrupts
    // and kernel critical sections can delay it. Experiment 1 in the brief lowers it.
    controlTask.start("Control", tskIDLE_PRIORITY + 4, pdMS_TO_TICKS(20), &Sweep::entry, &sweep);
    monitorTask.start(monitorFn, nullptr, "Monitor", tskIDLE_PRIORITY + 2);
    blinkTask.start(blinkFn, nullptr, "Blink", tskIDLE_PRIORITY + 1);

    vTaskStartScheduler();
}
