#pragma once
#include "FreeRTOS.h"
#include "task.h"
#include <cstdint>

namespace rtos {

template<uint32_t StackWords>
class Task
{
    static_assert(StackWords >= configMINIMAL_STACK_SIZE, "Task needs a larger stack size");

private:
    StackType_t  taskStack[StackWords];
    StaticTask_t taskTCB;
    TaskHandle_t taskHandle;

public:
    Task() = default;

    Task(const Task& obj) = delete;
    Task& operator=(const Task& obj) = delete;
    Task(Task&& obj) = delete;
    Task& operator=(Task&& obj) = delete;

    TaskHandle_t handle() const { return taskHandle; }

    void start(TaskFunction_t entryFn, void * ctx, const char *name,
               UBaseType_t priority)
    {
        configASSERT(taskHandle == nullptr);
        taskHandle = xTaskCreateStatic(entryFn, name, StackWords, ctx, priority,
                                       taskStack, &taskTCB);
        configASSERT(taskHandle != nullptr);
    }

};

}