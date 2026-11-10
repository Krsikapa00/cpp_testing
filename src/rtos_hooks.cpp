#include "FreeRTOS.h"
#include "stm32f4xx.h"
#include "task.h"
#include <cstddef>

static StaticTask_t idleTaskTCB;
static StackType_t idleTaskStack[configMINIMAL_STACK_SIZE];

extern "C" {

    void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer,
                                        StackType_t ** ppxIdleTaskStackBuffer,
                                        configSTACK_DEPTH_TYPE * puxIdleTaskStackSize )
    {
        *ppxIdleTaskTCBBuffer   = &idleTaskTCB;
        *ppxIdleTaskStackBuffer = idleTaskStack;
        *puxIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
    }

    void vApplicationStackOverflowHook( TaskHandle_t xTask,
                                        char * pcTaskName )
    {
        (void) xTask;
        (void) pcTaskName;
        configASSERT(false);
    }

    void *_sbrk(std::ptrdiff_t increment)
    {
        (void) increment;
        configASSERT(false);
    }


}