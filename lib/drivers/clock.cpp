#include "clock.hpp"
#include "stm32f4xx.h"

namespace hal {

namespace clock {

    void init() {

        RCC->APB1ENR |= RCC_APB1ENR_PWREN;
        (void) RCC->APB1ENR; //Read-back to let value take before moving on
        PWR->CR      |= PWR_CR_VOS_Msk;

        // Turn on HSE EN and Bypass
        RCC->CR |= RCC_CR_HSEBYP | RCC_CR_HSEON;

        while (!(RCC->CR & RCC_CR_HSERDY)) {}

        /* Cfg PLL & Turn on */
        RCC->PLLCFGR  = ((8u   << RCC_PLLCFGR_PLLM_Pos)
                      |  (360u << RCC_PLLCFGR_PLLN_Pos)
                      |  (2u   << RCC_PLLCFGR_PLLR_Pos)
                      |  (0u   << RCC_PLLCFGR_PLLP_Pos)
                      |  (7u   << RCC_PLLCFGR_PLLQ_Pos)
                      |  (RCC_PLLCFGR_PLLSRC_HSE));
        RCC->CR |= RCC_CR_PLLON;

        // Enable Over-drive wait for 2 flags
        PWR->CR |= PWR_CR_ODEN;
        while (!(PWR->CSR & PWR_CSR_ODRDY)) {}
        PWR->CR |= PWR_CR_ODSWEN;
        while (!(PWR->CSR & PWR_CSR_ODSWRDY)) {}

        // Wait for PLL to turn on
        while (!(RCC->CR & RCC_CR_PLLRDY)) {}


        // Set Flash wait state
        FLASH->ACR |= FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;
        FLASH->ACR |= FLASH_ACR_LATENCY_5WS;
        while ((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_5WS) {}

        // Bus Prescalers
        RCC->CFGR |= (RCC_CFGR_PPRE2_DIV2
                    | RCC_CFGR_PPRE1_DIV4
                    | RCC_CFGR_HPRE_DIV1);
        RCC->CFGR |= RCC_CFGR_SW_PLL;

        while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {}

        SystemCoreClockUpdate();
    }
} //clock namespace

} //hal namespace