#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "gpio.hpp"
#include "clock.hpp"
#include "custTask.hpp"
#include <atomic>
#include "queue.hpp"
#include "ringBuffer.hpp"
#include "uart.hpp"
#include "logger.hpp"

#define NUM_SIZE 8

using Console = hal::UartStatic<hal::Usart2Port, 115200>;
extern "C" void USART2_IRQHandler(void) { Console::onInterrupt(); }
static logging::Logger<Console> logger;

std::atomic<bool> btnPressed{false};
std::atomic<uint32_t> cntr{1000};

using Btn1     = hal::Gpio<GPIOC_BASE, 13>;
using taskInst = rtos::Task<configMINIMAL_STACK_SIZE>;
static taskInst blinkTaskObj;
static taskInst consumerTask;
static taskInst producerTask;
static taskInst fastLogTask;
static taskInst slowLogTask;
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

// Brief step 7: two tasks logging at different rates and priorities. The fast one preempts the
// slow one, so if the mutex didn't work their lines would interleave mid-line.
void fastLogFn(void *arg)
{
    (void) arg;
    TickType_t last = xTaskGetTickCount();
    while (true) {
        xTaskDelayUntil(&last, pdMS_TO_TICKS(10));
        logging::Line<24> line;
        line.append("t=").append(xTaskGetTickCount());
        logger.log(line);
    }
}

void slowLogFn(void *arg)
{
    (void) arg;
    logger.log("boot");
    TickType_t last = xTaskGetTickCount();
    while (true) {
        xTaskDelayUntil(&last, pdMS_TO_TICKS(500));
        // Free stack in words: the lowest it has been. Under about 20 is too close.
        logging::Line<64> line;
        line.append("slow drops=").append(logger.dropped())
            .append(" stack_free fast=").append(uxTaskGetStackHighWaterMark(fastLogTask.handle()))
            .append(" slow=").append(uxTaskGetStackHighWaterMark(nullptr));
        logger.log(line);
    }
}

int main()
{
    hal::clock::init();
    Console::init(6);
    logger.init();
    currQueue.init();

    consumerTask.start(consumerFn, nullptr, "Consumer Task",
                       tskIDLE_PRIORITY + 1);
    producerTask.start(producerFn, nullptr, "Producer Task",
                       tskIDLE_PRIORITY + 1);

    blinkTaskObj.start(blinkTask, nullptr, "Blink Task",
                       tskIDLE_PRIORITY + 1);
    slowLogTask.start(slowLogFn, nullptr, "Slow Log", tskIDLE_PRIORITY + 2);
    fastLogTask.start(fastLogFn, nullptr, "Fast Log", tskIDLE_PRIORITY + 3);

    vTaskStartScheduler();
}

