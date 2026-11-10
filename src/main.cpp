#include "stm32f4xx.h"
#include "gpio.hpp"
#include "clock.hpp"
#include <atomic>

std::atomic<std::uint32_t> msCounter{0};
std::atomic<bool> btnPressed{false};

using Btn1 = hal::Gpio<GPIOC_BASE, 13>;
uint32_t getTimeMs();

extern "C" {
    void SysTick_Handler(void)
    {
        msCounter++;
    }

    void EXTI15_10_IRQHandler(void)
    {
        if (EXTI->PR & EXTI_PR_PR13) {
            btnPressed = true;
            EXTI->PR = EXTI_PR_PR13;
        }
    }
}

uint32_t getTimeMs() {
    return msCounter;
}

void configureBtn() {
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    (void)RCC->APB2ENR;
    // Clear out the other pins for EXTI15 but keep EXTI11 - 14
    SYSCFG->EXTICR[3] &= ~(SYSCFG_EXTICR4_EXTI13);
    SYSCFG->EXTICR[3] |= SYSCFG_EXTICR4_EXTI13_PC;

    EXTI->IMR  |= EXTI_IMR_MR13;
    EXTI->FTSR |= EXTI_FTSR_TR13;
    EXTI->RTSR &= ~EXTI_RTSR_TR13;

    NVIC_EnableIRQ(EXTI15_10_IRQn);
}

int main()
{
    hal::clock::init();

    uint32_t delayCount = 1000; // ms Delay
    using Led = hal::Gpio<GPIOA_BASE, 5>;
    Led::enableClock();
    Led::setMode(hal::PinMode::Output);
    const uint32_t tickCfg = SystemCoreClock / 1000;
    SysTick_Config(tickCfg);

    Btn1::enableClock();
    Btn1::setMode(hal::PinMode::Input);

    configureBtn();

    std::uint32_t last = getTimeMs();
    std::uint32_t now  = getTimeMs();
    std::uint32_t lastPressed{0};

    while (true)
    {
        // Btn pressed check
        bool pressedCheck = btnPressed.exchange(false);
        if (pressedCheck && (getTimeMs() - lastPressed > 50)) {
            lastPressed = getTimeMs();
            delayCount  = (delayCount == 1000)? 500: 1000;
        }

        // LED updates
        now = getTimeMs();
        if (now - last >= delayCount) {
            last = now;
            Led::toggle();
        }
    }
}
