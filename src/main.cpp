#include "stm32f4xx.h"
#include "gpio.hpp"
#include "clock.hpp"

int main()
{
    hal::clock::init();

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
