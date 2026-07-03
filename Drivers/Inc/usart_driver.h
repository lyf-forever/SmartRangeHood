#ifndef __USART_H
#define __USART_H

#include "sys.h" 

#if HARDWARE_UPDATE_OPEN /* 固件升级 */
#if !HW_UPDATE_METHOD /* 有线IAP */

#define IAP_USART              USART1
#define IAP_USART_IRQHandler   USART1_IRQHandler

#define IAP_USART_DMA_RXCH     DMA1_Channel5

#define IAP_REC_LEN            512     /* 定义HWUP_USART最大接收字节数 */

/* API declare */
void iap_uart_cfg(void);

#else /* 无线OTA */

#endif /* #if !HW_UPDATE_METHOD */

#endif 

#if UART_LOG_OUT /* 使用串口打印日志 */
#define LOG_USART_PERIPH       UART4

/* API declare */
void debug_uart_cfg(void);
void log_usart_send_string(const char *str);

#endif /* #if UART_LOG_OUT */

#if PID_DEBUG
#define PID_USART_PERIPH       LOG_USART_PERIPH
#define PID_USART_IRQ          UART4_IRQn
#define PID_USART_IT           USART_IT_RXNE
#define PID_USART_IRQHandler   UART4_IRQHandler

#define PID_RXPacket_LEN       8  // 接受数据包调试长度
#endif /* #if PID_DEBUG */

#if PRINT_USE /* 使用串口打印日志 */

#define PRINT_USART_PERIPH     UART5
/* no use, just show msg */
#define PUP_TX_PORT            GPIOC
#define PUP_TX_PIN             GPIO_Pin_12
#define PUP_RX_PORT            GPIOD
#define PUP_RX_PIN             GPIO_Pin_2
/* no use, just show msg */

/* API declare */
void print_usart_onChipCfg(void);
#endif /* #if PRINT_USE */

#endif


