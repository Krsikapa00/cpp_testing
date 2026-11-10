/*
 * FreeRTOS kernel configuration for the NUCLEO-F446RE (Cortex-M4F, 180 MHz).
 *
 * Ground rules this file enforces:
 *   - No heap: static allocation only, and no heap_x.c is built (see lib/freertos/library.json).
 *   - Safety nets: configASSERT, stack overflow checking (method 2), and the kernel's check that its
 *     handlers are really in the vector table.
 *
 * The kernel includes this file from C and from C++ (through FreeRTOS.h), outside its extern "C"
 * block, so only include headers that are safe in both. stm32f4xx.h is.
 *
 * Every setting is described in https://www.freertos.org/a00110.html
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include "stm32f4xx.h" /* SystemCoreClock and __NVIC_PRIO_BITS */

/* ---- Scheduler ------------------------------------------------------------------------------ */
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1 /* Uses CLZ to pick the next task, needs priorities <= 32 */
#define configUSE_TICKLESS_IDLE                 0
#define configCPU_CLOCK_HZ                      (SystemCoreClock) /* Read when the scheduler starts, so call hal::clock::init() first */
#define configTICK_RATE_HZ                      1000u            /* 1 ms tick, so 1 tick == 1 ms */
#define configMAX_PRIORITIES                    5u
#define configMINIMAL_STACK_SIZE                128u             /* In words (4 bytes each), used by the idle task */
#define configMAX_TASK_NAME_LEN                 12u
#define configTICK_TYPE_WIDTH_IN_BITS           TICK_TYPE_WIDTH_32_BITS
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TIME_SLICING                  1

/* ---- Memory: static only -------------------------------------------------------------------- */
#define configSUPPORT_STATIC_ALLOCATION         1
#define configSUPPORT_DYNAMIC_ALLOCATION        0 /* Any xTaskCreate/xQueueCreate call fails to link */
#define configKERNEL_PROVIDED_STATIC_MEMORY     0 /* You provide vApplicationGetIdleTaskMemory() */
#define configAPPLICATION_ALLOCATED_HEAP        0

/* ---- Features ------------------------------------------------------------------------------- */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_TASK_NOTIFICATIONS            1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   1
#define configUSE_QUEUE_SETS                    0
#define configQUEUE_REGISTRY_SIZE               0
#define configUSE_TIMERS                        0 /* Turn on later if needed, then also provide vApplicationGetTimerTaskMemory() */
#define configUSE_CO_ROUTINES                   0
#define configUSE_NEWLIB_REENTRANT              0 /* No newlib printf in tasks, as it can use the heap */
#define configENABLE_BACKWARD_COMPATIBILITY     0

/* ---- Hooks (each one you turn on is a function you must write, with extern "C") ------------- */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            0 /* No heap, so nothing to fail */
#define configCHECK_FOR_STACK_OVERFLOW          2 /* You provide vApplicationStackOverflowHook() */
#define configUSE_TRACE_FACILITY                0
#define configGENERATE_RUN_TIME_STATS           0
#define configRECORD_STACK_HIGH_ADDRESS         1 /* Lets the debugger show each task's whole stack */

/* ---- Interrupt priorities --------------------------------------------------------------------
 * The F446 has 4 priority bits (0 = most urgent, 15 = least). The kernel runs at the lowest
 * priority. Any ISR that calls a ...FromISR() function must have a numeric priority of 5 or more
 * (5 to 15). ISRs at 0 to 4 are never masked by the kernel, but must not call any FreeRTOS API.
 * NVIC_SetPriority() takes the unshifted number (0-15). These macros are the shifted forms the
 * kernel writes to the hardware. */
#define configPRIO_BITS                         __NVIC_PRIO_BITS /* 4 */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY         (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* ---- Asserts ---------------------------------------------------------------------------------
 * A failed assert stops everything here. In the debugger, halt and look at the call stack. */
#define configASSERT(x)                                                                            \
    do {                                                                                           \
        if ((x) == 0) {                                                                            \
            taskDISABLE_INTERRUPTS();                                                              \
            for (;;) {                                                                             \
            }                                                                                      \
        }                                                                                          \
    } while (0)
#define configCHECK_HANDLER_INSTALLATION        1 /* Asserts at start-up if the mapping below didn't take */

/* ---- Handler names ---------------------------------------------------------------------------
 * Gives the port's handlers the CMSIS names the startup file's vector table uses, so the kernel
 * owns SVCall, PendSV and SysTick. Your own SysTick_Handler must go, or the link fails. */
#define vPortSVCHandler                         SVC_Handler
#define xPortPendSVHandler                      PendSV_Handler
#define xPortSysTickHandler                     SysTick_Handler

/* ---- Optional API functions ----------------------------------------------------------------- */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     0 /* Tasks run forever. A deleted static task's memory can't be reused safely without care. */
#define INCLUDE_vTaskSuspend                    1 /* Also makes portMAX_DELAY wait forever */
#define INCLUDE_xTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetIdleTaskHandle          0
#define INCLUDE_eTaskGetState                   0
#define INCLUDE_xTaskResumeFromISR              0
#define INCLUDE_xTimerPendFunctionCall          0

#endif /* FREERTOS_CONFIG_H */
