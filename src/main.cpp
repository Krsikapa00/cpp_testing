#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "gpio.hpp"
#include "clock.hpp"
#include "custTask.hpp"
#include <atomic>
#include "queue.hpp"
#include "ringBuffer.hpp"

#define NUM_SIZE 8

std::atomic<bool> btnPressed{false};
std::atomic<uint32_t> cntr{1000};

using Btn1     = hal::Gpio<GPIOC_BASE, 13>;
using taskInst = rtos::Task<configMINIMAL_STACK_SIZE>;
static taskInst blinkTaskObj;
static taskInst consumerTask;
static taskInst producerTask;
static control::RingBuffer<uint16_t, NUM_SIZE> ringBuf;
static rtos::Queue<uint16_t, NUM_SIZE> currQueue;

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

    NVIC_SetPriority(EXTI15_10_IRQn, 5);
    NVIC_EnableIRQ(EXTI15_10_IRQn);
}

void blinkTask(void *arg)
{
    (void) arg;
    using Led = hal::Gpio<GPIOA_BASE, 5>;
    Led::enableClock();
    Led::setMode(hal::PinMode::Output);

    Btn1::enableClock();
    Btn1::setMode(hal::PinMode::Input);

    configureBtn();

    TickType_t last = xTaskGetTickCount();

    while (true)
    {
        // LED updates
        xTaskDelayUntil(&last, cntr.load(std::memory_order_acquire));
        Led::toggle();
    }
}


void producerFn(void *arg)
{
    (void) arg;
    uint16_t currData[NUM_SIZE] = {200, 400, 500, 750, 1000, 1500, 100, 150};
    uint8_t idx = 0;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (ringBuf.push(currData[idx])) {
            idx = (idx + 1) % NUM_SIZE;
        }
        // currQueue.send(currData[idx++ % 6], portMAX_DELAY);
    }
}

void consumerFn(void *arg)
{
    (void) arg;
    uint16_t currData = 0;

    while (true) {
        if (ringBuf.pop(currData)) {
            cntr.store(currData, std::memory_order_release);
        }
        // if (currQueue.receive(currData, portMAX_DELAY)) {
        //     cntr.store(currData, std::memory_order_release);
        // };
    }
}

int main()
{
    hal::clock::init();
    currQueue.init();

    consumerTask.start(consumerFn, nullptr, "Consumer Task",
                       tskIDLE_PRIORITY + 1);
    producerTask.start(producerFn, nullptr, "Producer Task",
                       tskIDLE_PRIORITY + 1);

    blinkTaskObj.start(blinkTask, nullptr, "Blink Task",
                       tskIDLE_PRIORITY + 1);

    vTaskStartScheduler();
}

