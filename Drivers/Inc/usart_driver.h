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
#define LOG_USARTx       UART4

/* API declare */
void log_uartOnChipCfg(void);
void log_usart_sendStr_block(const char *str);
void log_usart_sendData_dma(uint8_t *data, uint16_t len);

#endif /* #if UART_LOG_OUT */

#if PID_DEBUG
#define LOG_USART_IRQ          UART4_IRQn
#define LOG_USART_IT           USART_IT_RXNE
#define LOG_USART_IRQHandler   UART4_IRQHandler

#define LOG_TX_DMA_CH          DMA2_Channel5
#define LOG_TX_DMA_TC_FLAG     DMA2_FLAG_TC5
#define LOG_RXPACKET_LEN       8  // 接受数据包调试长度

#if 1
/* -------- UART4 引脚定义 -------- */
#define LOG_UART_TX_PIN       GPIO_Pin_10
#define LOG_UART_RX_PIN       GPIO_Pin_11
#define LOG_UART_PORT    GPIOC
#define LOG_UART_GPIO_CLK     RCC_APB2Periph_GPIOC

/* -------- UART4 外设定义 -------- */
#define LOG_UART_CLK          RCC_APB1Periph_UART4

/* -------- DMA2 定义 (仅大容量产品) -------- */
#define LOG_UART_TX_DMA_CH    DMA2_Channel5
#define LOG_UART_TX_DMA_CLK   RCC_AHBPeriph_DMA2
#define LOG_UART_TX_DMA_FLAG  DMA2_FLAG_TC5    // 传输完成标志

#endif 

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


