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
#if 0
/* ========================= LOG_USARTx - 日志调试专用 ==============================*/
static usart_gpio_t log_uart_gpio = {
	.tx_mode = GPIO_Mode_AF_PP,
	.rx_mode = GPIO_Mode_IPU,
};

static usart_paras_t log_uart_paras = {
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
static usart_intr_msg_t log_uart_intrMsg = { 
	.nvic_preprio  = 4, 
	.nvic_subprio  = 0, 
	.usart_intType = LOG_USART_IT 
};

static dma_intr_msg_t log_dma_intr_msg = {0};

static usart_dma_t log_uart_dma = {
	.dma_tx_prio 		   = DMA_Priority_High,
	.dma_tx_periphdatasize = DMA_PeripheralDataSize_Byte,
	.dma_tx_memdatasize    = DMA_MemoryDataSize_Byte,
	.tx_buf 			   = 0,
	.tx_buf_size 		   = 0
};

static usart_nvic_t log_uart_nvic = {
	.intrUse 	   = USE_INT_USART_ONLY,
	.usart_intrMsg = &log_uart_intrMsg,
	.dma_intrMsg   = &log_dma_intr_msg
};
#endif
static usart_cfg_t log_uart_cfg = {
	.instance	= LOG_USARTx,
	.u_gpio  	= &log_uart_gpio,
	.u_paras  	= &log_uart_paras,
#if PID_DEBUG
	.useDMA_INT = 0x03,  // 使用串口接受中断,发送使用DMA
	.u_dma      = &log_uart_dma,
	.u_nvic     = &log_uart_nvic
#else
	.useDMA_INT = 0x00
#endif
};
#else
static void log_uart_gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 使能 GPIOC 时钟 */
    RCC_APB2PeriphClockCmd(LOG_UART_GPIO_CLK, ENABLE);

    /* PC10 - LOG_UART_PORT_TX: 复用推挽输出 */
    GPIO_InitStructure.GPIO_Pin   = LOG_UART_TX_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LOG_UART_PORT, &GPIO_InitStructure);

    /* PC11 - LOG_UART_PORT_RX: 浮空输入 */
    GPIO_InitStructure.GPIO_Pin   = LOG_UART_RX_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_Init(LOG_UART_PORT, &GPIO_InitStructure);
}

static void log_uart_init(void)
{
    USART_InitTypeDef USART_InitStructure;

    /* 使能 LOG_UART_PORT 时钟 */
    RCC_APB1PeriphClockCmd(LOG_UART_CLK, ENABLE);

    USART_InitStructure.USART_BaudRate            = 115200U;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(LOG_USARTx, &USART_InitStructure);

	USART_ITConfig(LOG_USARTx, LOG_USART_IT, ENABLE);

	NVIC_InitTypeDef log_nvic;
	log_nvic.NVIC_IRQChannel = LOG_USART_IRQ;
	log_nvic.NVIC_IRQChannelPreemptionPriority = 0x05;
	log_nvic.NVIC_IRQChannelSubPriority = 0x00;
	log_nvic.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&log_nvic);

	USART_ClearFlag(LOG_USARTx, USART_FLAG_RXNE);
    USART_Cmd(LOG_USARTx, ENABLE);  /* 使能 LOG_USARTx */
}

static void log_uart_dma_tx_init(void)
{
    DMA_InitTypeDef DMA_InitStructure;

    /* 使能 DMA2 时钟 */
    RCC_AHBPeriphClockCmd(LOG_UART_TX_DMA_CLK, ENABLE);

    /* 配置 DMA2 Channel5 */
    DMA_DeInit(LOG_TX_DMA_CH);

    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&LOG_USARTx->DR;  // 外设地址
    DMA_InitStructure.DMA_MemoryBaseAddr      = 0;                         // 后续动态设置
    DMA_InitStructure.DMA_DIR                 = DMA_DIR_PeripheralDST;     // 内存 → 外设
    DMA_InitStructure.DMA_BufferSize          = 0;                    	   // 后续动态设置
    DMA_InitStructure.DMA_PeripheralInc       = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc           = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize  = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize      = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode                = DMA_Mode_Normal;     	   // 单次传输
    DMA_InitStructure.DMA_Priority            = DMA_Priority_Medium;
    DMA_InitStructure.DMA_M2M                 = DMA_M2M_Disable;

    DMA_Init(LOG_UART_TX_DMA_CH, &DMA_InitStructure);
	USART_DMACmd(LOG_USARTx, USART_DMAReq_Tx, ENABLE);
}
#endif /* #if 0 */

/* 初始化调试时日志打印使用的串口LOG_USARTx */
void log_uartOnChipCfg(void) {
#if 0
	usart_concernAll_config(&log_uart_cfg);
#else
	log_uart_gpio_init();
	log_uart_init();
	log_uart_dma_tx_init();
#endif
}

/**
 * @brief  阻塞式发送字符串，供日志宏使用
 * @param  str  要发送的字符串（以 '\0' 结尾）
 */
void log_usart_sendStr_block(const char *str)
{
    while (*str) {
        // 等待发送数据寄存器空
        while (USART_GetFlagStatus(LOG_USARTx, USART_FLAG_TXE) == RESET);
        USART_SendData(LOG_USARTx, *str++);
    }
}

/**
 * @brief 启动 DMA 发送，并轮询等待发送完成
 * @param data  要发送的数据缓冲区（必须保持有效直到发送完成）
 * @param len   发送字节数
 * @return u8 0:发送成功 1：发送失败
 */
void log_usart_sendData_dma(uint8_t *data, uint16_t len)
{
    /* 参数检查 */
    if (!data || !len) return;

    /* ----- 步骤1：等待并关闭 DMA，确保无残留传输 ----- */
    DMA_Cmd(LOG_TX_DMA_CH, DISABLE);
    /* 等待 DMA 完全停止（EN 位被硬件清零） */
    while ((LOG_TX_DMA_CH->CCR & 0x0001U) != 0);

    /* ----- 步骤2：配置传输参数 ----- */
    LOG_TX_DMA_CH->CMAR = (uint32_t)data;
    LOG_TX_DMA_CH->CNDTR = len;

    /* 清除上次传输可能残留的标志 */
    DMA_ClearFlag(LOG_TX_DMA_TC_FLAG);

    /* ----- 步骤3：启动 DMA 传输 ----- */
    DMA_Cmd(LOG_TX_DMA_CH, ENABLE);
    USART_DMACmd(LOG_USARTx, USART_DMAReq_Tx, ENABLE);

    /* ----- 步骤4：【关键】等待 DMA 传输完成（不关中断） ----- */
    while (DMA_GetFlagStatus(LOG_TX_DMA_TC_FLAG) == RESET);

    /* ----- 步骤5：清除标志，关闭 DMA（释放总线） ----- */
    DMA_ClearFlag(LOG_TX_DMA_TC_FLAG);
    DMA_Cmd(LOG_TX_DMA_CH, DISABLE);
    /* 等待 DMA 完全停止，为下一次传输做准备 */
    while ((LOG_TX_DMA_CH->CCR & 0x0001U) != 0);
}
/* ========================= LOG_USARTx - 日志调试打印专用 ==============================*/

#endif /* #if UART_LOG_OUT */
