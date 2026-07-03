#include "debug_log.h"
#include "FreeRTOS.h"
#include "task.h"

log_level_t g_log_level = LOG_LVL_DEBUG;  // 默认输出 DEBUG 及以上

void log_output(const char *str) {
    // 重定向到串口发送函数，或者直接 printf（若已重定向）
    extern void log_usart_send_string(const char *str);  
    log_usart_send_string(str);
}

uint32_t log_get_tick(void) {
    // 如果你使用了 FreeRTOS
    return (uint32_t)xTaskGetTickCount();  
}


