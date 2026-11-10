#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "gpio.hpp"
#include "clock.hpp"
#include <atomic>
#include <cstddef>

// std::atomic<std::uint32_t> msCounter{0};
std::atomic<bool> btnPressed{false};

using Btn1 = hal::Gpio<GPIOC_BASE, 13>;
uint32_t getTimeMs();

extern "C" {
    void EXTI15_10_IRQHandler(void)
    {
        if (EXTI->PR & EXTI_PR_PR13) {
            btnPressed = true;
            EXTI->PR = EXTI_PR_PR13;
        }
    }
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
    NVIC_SetPriority(EXTI15_10_IRQn, 5);
}

static StackType_t blinkTaskStack[configMINIMAL_STACK_SIZE];
static StaticTask_t blinkTaskTCB;

void blinkTask(void *arg)
{
    (void) arg;
    uint32_t delayCount = 1000; // ms Delay
    using Led = hal::Gpio<GPIOA_BASE, 5>;
    Led::enableClock();
    Led::setMode(hal::PinMode::Output);

    Btn1::enableClock();
    Btn1::setMode(hal::PinMode::Input);

    configureBtn();

    TickType_t last = xTaskGetTickCount();
    std::uint32_t lastPressed{0};

    while (true)
    {
        // Btn pressed check
        bool pressedCheck = btnPressed.exchange(false);
        if (pressedCheck && (xTaskGetTickCount() - lastPressed > 50)) {
            lastPressed = xTaskGetTickCount();
            delayCount  = (delayCount == 1000)? 500: 1000;
        }

        // LED updates
        xTaskDelayUntil(&last, delayCount);
        Led::toggle();
    }
}

int main()
{
    hal::clock::init();

    xTaskCreateStatic(  blinkTask,
                        "Blink Task",
                        configMINIMAL_STACK_SIZE,
                        0,
                        tskIDLE_PRIORITY + 1,
                        blinkTaskStack,
                        &blinkTaskTCB);

    vTaskStartScheduler();
}

