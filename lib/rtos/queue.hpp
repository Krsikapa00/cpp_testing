#pragma once

#include "FreeRTOS.h"
#include "queue.h"
#include <type_traits>

namespace rtos {
template<typename T, size_t N>
class Queue {
    static_assert(N >= 1, "Queue must have non-zero size");
    static_assert(std::is_trivially_copyable<T>::value, "Type given cannot be copied trivially");

    static constexpr size_t itemSize = sizeof(T);

private:
    StaticQueue_t qBuffer;
    QueueHandle_t xQueue;
    uint8_t data[N * itemSize];

public:
    Queue()                             = default;
    Queue(const Queue &q)               = delete;
    Queue& operator=(const Queue &q)    = delete;
    Queue(Queue &&q)                    = delete;
    Queue& operator=(Queue &&q)         = delete;

    void init()
    {
        xQueue = xQueueCreateStatic(N, itemSize, data, &qBuffer);
        configASSERT(xQueue != nullptr);
    }

    bool send(const T& item, TickType_t timeout)
    {
        return xQueueSend(xQueue, &item, timeout) == pdPASS;
    }

    bool receive(T &retItem, TickType_t timeout)
    {
        return xQueueReceive(xQueue, &retItem, timeout) == pdPASS;
    }

    bool sendFromISR(const T& item, BaseType_t *priority)
    {
        return xQueueSendFromISR(xQueue, &item, priority) == pdPASS;
    }

    bool receiveFromISR(T &retItem, BaseType_t *highPriority)
    {
        return xQueueReceiveFromISR(xQueue, &retItem, highPriority) == pdPASS;
    }
};

}