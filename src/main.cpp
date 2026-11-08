#include "stm32f4xx.h"
#include "gpio.hpp"

// Toolchain check only: blinks LD2 (PA5) using a busy-wait, and uses a float so the FPU has to work.
// Replaced by your Gpio class and SysTick tick in Project 1.
int main()
{
    volatile float delayCount = 2000000.0f;
    using Led = hal::Gpio<GPIOA_BASE, 5>;
    Led::enableClock();
    Led::setMode(hal::PinMode::Output);

    while (true)
    {
        Led::toggle();

        for (volatile float i = 0.0f; i < delayCount; i += 1.0f) {
        }
    }
}
