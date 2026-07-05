#include "debug_log.h"
#include "FreeRTOS.h"
#include "task.h"

log_level_t g_log_level = LOG_LVL_DEBUG;  // 默认输出 DEBUG 及以上

#define LOG_TYPE  1  /* 0:阻塞发送 1：DMA非阻塞发送 */

void log_output(const char *str) {
#if !LOG_TYPE
    // 重定向到串口发送函数，或者直接 printf（若已重定向）
    extern void log_usart_sendStr_block(const char *str);  
    log_usart_sendStr_block(str);
#else
    /* 静态缓冲区，生命周期贯穿程序全程，保证 DMA 发送时数据有效 */
    static char dma_tx_buf[128];
    
    /* 计算字符串长度，防止溢出 */
    uint16_t len = strlen(str);
    if (len >= sizeof(dma_tx_buf)) 
        len = sizeof(dma_tx_buf) - 1;
    
    // 拷贝到静态缓冲区（因为 str 指向 LOG 宏的局部 buf，函数返回后会被回收）
    memcpy(dma_tx_buf, str, len);
    dma_tx_buf[len] = '\0';   // 确保结尾

    // 调用 DMA 发送（阻塞等待发送完成，但期间不关闭中断）
    extern void log_usart_sendData_dma(uint8_t *data, uint16_t len);
    log_usart_sendData_dma((uint8_t*)dma_tx_buf, len);
#endif 
}

uint32_t log_get_tick(void) {
    // 如果你使用了 FreeRTOS
    return (uint32_t)xTaskGetTickCount();  
}


