#pragma once

#include "stm32f4xx.h"
#include <cstdint>

namespace hal {

    enum class PinMode: uint8_t { Input = 0, Output = 1u, Alternate = 2u, Analog = 3u };
    enum class PullDir:  uint8_t { NoPull = 0, PullUp = 1u, PullDown = 2u };

    template<std::uintptr_t PortBase, uint8_t Pin>
    class Gpio {
        static_assert(Pin < 16, "GPIO Pin must be from 0 - 15");
        static_assert(((PortBase - GPIOA_BASE) % 0x400) == 0, "Base port not aligned" );
        static_assert(((PortBase - GPIOA_BASE) / 0x400) < 8, "Base port not valid" );

        static constexpr uint32_t mask = 1ul << Pin;
        static constexpr uint8_t portPin = (PortBase - GPIOA_BASE) / 0x400;

        static GPIO_TypeDef& reg() {
            return *reinterpret_cast<GPIO_TypeDef *>(PortBase);
        }

        public:
            static void enableClock() {
                RCC->AHB1ENR |= (1ul << portPin);
                (void)RCC->AHB1ENR;
            }

            static void setMode(PinMode pinMode) {
                // Clear the bits for this pin then set to the mode
                reg().MODER = (reg().MODER & ~(3u << (Pin * 2))) | (static_cast<uint32_t>(pinMode) << (Pin * 2));
            }

            static void setPullDir(PullDir pull) {
                reg().PUPDR = (reg().PUPDR & ~(3u << (Pin * 2))) | (static_cast<uint32_t>(pull) << (Pin * 2));
            }

            static bool read()   { return (reg().IDR & mask); }
            static void set()    { reg().BSRR = mask; }
            static void clear()  { reg().BSRR = mask << 16; }
            static void toggle() {
                if (reg().ODR & mask) {
                    clear();
                }
                else {
                    set();
                }
            }

            static void setAF(uint8_t afSel) {
                if (afSel > 0xF) {
                    return;
                }

                uint8_t idx = Pin / 8;
                uint8_t shift = (Pin % 8) * 4;

                reg().AFR[idx] = (reg().AFR[idx] & ~(0xFu << shift)) | (afSel << shift);
            }
    };

}