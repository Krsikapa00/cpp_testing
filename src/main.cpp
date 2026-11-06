#include "stm32f4xx.h"

// Toolchain check only: blinks LD2 (PA5) using a busy-wait, and uses a float so the FPU has to work.
// Replaced by your Gpio class and SysTick tick in Project 1.
int main()
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    GPIOA->MODER = (GPIOA->MODER & ~GPIO_MODER_MODER5) | GPIO_MODER_MODER5_0;

    volatile float delayCount = 200000.0f;

    while (true)
    {
        GPIOA->ODR ^= GPIO_ODR_OD5;

        for (volatile float i = 0.0f; i < delayCount; i += 1.0f)
        {
        }
    }
}
