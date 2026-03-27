#pragma once
typedef void* TaskHandle_t;
inline void xTaskCreatePinnedToCore(void(*)(void*), const char*, int, void*, int, TaskHandle_t*, int) {}
inline void vTaskDelete(TaskHandle_t) {}
