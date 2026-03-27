#pragma once
#include "FreeRTOS.h"
#include <queue>
#include <vector>
#include <cstring>

typedef void* QueueHandle_t;

// Simple queue implementation for testing
struct FakeQueue {
    std::queue<std::vector<uint8_t>> items;
    size_t itemSize;
};

inline QueueHandle_t xQueueCreate(int depth, size_t itemSize) {
    auto* q = new FakeQueue();
    q->itemSize = itemSize;
    return q;
}

inline BaseType_t xQueueSend(QueueHandle_t handle, const void* item, TickType_t) {
    auto* q = static_cast<FakeQueue*>(handle);
    std::vector<uint8_t> data(q->itemSize);
    memcpy(data.data(), item, q->itemSize);
    q->items.push(data);
    return pdPASS;
}

inline BaseType_t xQueueReceive(QueueHandle_t handle, void* item, TickType_t) {
    auto* q = static_cast<FakeQueue*>(handle);
    if (q->items.empty()) return pdFAIL;
    memcpy(item, q->items.front().data(), q->itemSize);
    q->items.pop();
    return pdPASS;
}

inline void vQueueDelete(QueueHandle_t handle) {
    delete static_cast<FakeQueue*>(handle);
}
