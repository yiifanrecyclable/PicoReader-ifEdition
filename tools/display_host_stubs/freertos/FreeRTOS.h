#pragma once
#define portMAX_DELAY 0
#define portMUX_INITIALIZER_UNLOCKED 0
typedef int portMUX_TYPE;
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
