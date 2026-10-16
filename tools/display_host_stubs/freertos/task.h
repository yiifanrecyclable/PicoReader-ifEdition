#pragma once
#define pdMS_TO_TICKS(n) (n)
static inline void vTaskDelay(int ticks){(void)ticks;}
