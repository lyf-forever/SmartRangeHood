#include "usart_driver.h"
#include "usart.h"
#include "stdio.h"	  

#if SYSTEM_SUPPORT_OS
#include "FreeRTOS.h"	/* freertos使用 */
#endif
   
// 加入以下代码,支持printf函数,而不需要选择use MicroLIB	 
#pragma import(__use_no_semihosting)             
// 标准库需要的支持函数                 
struct __FILE 
{ 
	int handle; 
}; 

FILE __stdout;       
// 定义_sys_exit()以避免使用半主机模式    
void _sys_exit(int x) 
{ 
	x = x; 
} 

#if !PRINT_USE
int fputc(int ch, FILE *f)
{      
	while((USART1->SR & 0X40) == 0);//循环发送,直到发送完毕   
    USART1->DR = (uint8_t)ch;      
	return ch;
}

#endif

#if PRINT_USE 
static usart_gpio_t pu_gpio = { .tx_mode = GPIO_Mode_AF_PP, .rx_mode = GPIO_Mode_IPU };

static usart_paras_t pu_paras = {
		.baudrate  = 115200,
		.wordlen   = USART_WordLength_8b,
		.stopbits  = USART_StopBits_1,
		.parity    = USART_Parity_No,
		.hwflowctl = USART_HardwareFlowControl_None,
		.mode	   = USART_Mode_Tx 
	};

static usart_cfg_t pu_cfg = {
	.instance   = PRINT_USART_PERIPH,
	.u_gpio     = &pu_gpio,
	.u_paras    = &pu_paras,
	.useDMA_INT = 0x00     // 仅发送不接收
};

void print_usart_onChipCfg(void)
{
	usart_concernAll_config(&pu_cfg);
}

// 重定义fputc函数 
int fputc(int ch, FILE *f)
{      
	while((PRINT_USART_PERIPH->SR & 0X40) == 0);//循环发送,直到发送完毕   
    PRINT_USART_PERIPH->DR = (uint8_t)ch;      
	return ch;
}
#endif /* #if PRINT_USE */

#if HARDWARE_UPDATE_OPEN /* 固件升级 */
	#if !HW_UPDATE_METHOD /* 有线IAP */

uint8_t iap_recbuff[IAP_REC_LEN];

	static usart_gpio_t iap_usart_gpio = {
		.tx_mode = GPIO_Mode_AF_PP,
		.rx_mode = GPIO_Mode_IPU,
	};

	static usart_paras_t iap_usart_paras = {
		.baudrate  = 115200,
		.wordlen   = USART_WordLength_8b,
		.stopbits  = USART_StopBits_1,
		.parity    = USART_Parity_No,
		.hwflowctl = USART_HardwareFlowControl_None,
		.mode	   = USART_Mode_Tx | USART_Mode_Rx,
	};

	static usart_dma_t iap_usart_dma = {
		.dma_rx_mode 		   = DMA_Mode_Circular,
		.dma_rx_prio 		   = DMA_Priority_High,
		.dma_rx_periphdatasize = DMA_PeripheralDataSize_Byte,
		.dma_rx_memdatasize    = DMA_MemoryDataSize_Byte,
		.rx_buf 			   = iap_recbuff,
		.rx_buf_size		   = IAP_REC_LEN,
	};

	static usart_intr_msg_t iap_usart_intr_msg = {
		.usart_intType = USART_IT_IDLE,
		.nvic_preprio  = 5,
		.nvic_subprio  = 0,
	};

	static dma_intr_msg_t iap_dma_intr_msg = {0};

	static usart_nvic_t iap_usart_nvic = {
		.intrUse       =  USE_INT_USART_ONLY,
		.usart_intrMsg = &iap_usart_intr_msg,
		.dma_intrMsg   = &iap_dma_intr_msg
	};

	static usart_cfg_t iap_usart_cfg = {
		.instance   = IAP_USART,
		.u_gpio     = &iap_usart_gpio,
		.u_paras    = &iap_usart_paras,
		.u_dma      = &iap_usart_dma,
		.u_nvic     = &iap_usart_nvic,
		.useDMA_INT = 0x03,
	};

/* ======================= IAP_USART API ===========================*/
void iap_uart_cfg(void) {
	usart_concernAll_config(&iap_usart_cfg);
}

	#else /* 无线OTA */



	#endif /* #if !HW_UPDATE_METHOD */

#endif /* #if HARDWARE_UPDATE_OPEN */

#if UART_LOG_OUT /* 串口日志打印 */

/* ========================= LOG_USART_PERIPH - 日志调试专用 ==============================*/
static usart_gpio_t dbg_uart_gpio = {
	.tx_mode = GPIO_Mode_AF_PP,
	.rx_mode = GPIO_Mode_IPU,
};

static usart_paras_t dbg_uart_paras = {
	.baudrate  = 115200,
	.wordlen   = USART_WordLength_8b,
	.stopbits  = USART_StopBits_1,
	.parity    = USART_Parity_No,
	.hwflowctl = USART_HardwareFlowControl_None,
#if PID_DEBUG
	.mode	   = USART_Mode_Tx | USART_Mode_Rx
#else
	.mode  	   = USART_Mode_Tx
#endif
};

#if PID_DEBUG
static usart_intr_msg_t dbg_uart_intrMsg = { 
	.nvic_preprio = 4, 
	.nvic_subprio = 0, 
	.usart_intType = PID_USART_IT 
};

static dma_intr_msg_t dbg_dma_intr_msg = {0};

static usart_nvic_t dbg_uart_nvic = {
	.intrUse = USE_INT_USART_ONLY,
	.usart_intrMsg = &dbg_uart_intrMsg,
	.dma_intrMsg   = &dbg_dma_intr_msg
};
#endif
static usart_cfg_t dbg_uart_cfg = {
	.instance	= LOG_USART_PERIPH,
	.u_gpio  	= &dbg_uart_gpio,
	.u_paras  	= &dbg_uart_paras,
#if PID_DEBUG
	.useDMA_INT = 0x02,  // 使用串口接受中断
	.u_nvic     = &dbg_uart_nvic
#else
	.useDMA_INT = 0x00
#endif
};

/* 初始化调试时日志打印使用的串口LOG_USART_PERIPH */
void debug_uart_cfg(void) {
	usart_concernAll_config(&dbg_uart_cfg);
}

/**
 * @brief  阻塞式发送字符串，供日志宏使用
 * @param  str  要发送的字符串（以 '\0' 结尾）
 */
void log_usart_send_string(const char *str)
{
    while (*str) {
        // 等待发送数据寄存器空
        while (USART_GetFlagStatus(dbg_uart_cfg.instance, USART_FLAG_TXE) == RESET);
        USART_SendData(dbg_uart_cfg.instance, *str++);
    }
}
/* ========================= LOG_USART_PERIPH - 日志调试打印专用 ==============================*/

#endif /* #if UART_LOG_OUT */
