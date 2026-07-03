#ifndef __USART_H_
#define __USART_H_

#include "sys.h"

/* USART GPIO配置结构体 */
typedef struct {
    GPIO_TypeDef *tx_port;
    GPIO_TypeDef *rx_port;
    
    uint16_t tx_pin;
    uint16_t rx_pin;  

    GPIOMode_TypeDef tx_mode;
    GPIOMode_TypeDef rx_mode;
} usart_gpio_t;

/* USART参数配置结构体，包含全部参数信息 */
typedef struct {
    uint32_t baudrate;
    uint16_t wordlen;
    uint16_t stopbits;
    uint16_t parity;
    uint16_t mode;
    uint16_t hwflowctl;
} usart_paras_t;

/* USART DMA配置结构体 */
typedef struct {
    DMA_TypeDef *dma;
    uint32_t     dma_rx_mode;
    uint32_t     dma_tx_prio;
    uint32_t     dma_rx_prio;

    uint32_t     dma_rx_periphdatasize;
    uint32_t     dma_rx_memdatasize;
    uint32_t     dma_tx_periphdatasize;
    uint32_t     dma_tx_memdatasize;

    DMA_Channel_TypeDef *tx_ch;
    DMA_Channel_TypeDef *rx_ch;
    // uint32_t tc_flag_tx;
    // uint32_t tc_flag_rx;  
    
    uint16_t rx_buf_size;
    uint16_t tx_buf_size;
    uint8_t  *rx_buf;
    uint8_t  *tx_buf;
} usart_dma_t;

/* 中断使用结构体 */
typedef enum {
    USE_INT_USART_ONLY,     /* 只使用USART中断 */
    USE_INT_DMA_ONLY,       /* 只使用DMA中断 */
    USE_INT_BOTH_USART_DMA, /* DMA USART中断都使用*/
} intr_usage_t;

/* USART 中断信息结构体 */
typedef struct {
    IRQn_Type irq_usart;
    uint16_t  usart_intType;       /* 使用的USART中断的类型 */
    uint8_t   nvic_preprio;
    uint8_t   nvic_subprio;   
} usart_intr_msg_t;

typedef enum {
    DMA_INT_TX = -1,
    DMA_INT_RX =  0, 
    DMA_INT_TR =  1
} dma_intr_ch_t;

/* USART_DMA 中断信息结构体 */
typedef struct {
    IRQn_Type     irq_dma_tx;  
    IRQn_Type     irq_dma_rx;
    uint32_t      dma_txch_int_type;       /* DMA TX通道使用的DMA中断的类型 */
    uint32_t      dma_rxch_int_type;       /* DMA RX通道使用的DMA中断的类型 */
    dma_intr_ch_t dma_int_ch;
    uint8_t       nvic_txch_preprio;
    uint8_t       nvic_txch_subprio;
    uint8_t       nvic_rxch_preprio;
    uint8_t       nvic_rxch_subprio;
} dma_intr_msg_t;

/* USART NVIC中断配置结构体 */
typedef struct {
    intr_usage_t      intrUse;         /* 中断使用情况 */
      
    usart_intr_msg_t *usart_intrMsg;
    dma_intr_msg_t   *dma_intrMsg;
} usart_nvic_t;

/* 串口USART全配置信息结构体 */
typedef struct {
    USART_TypeDef       *instance;      /* 串口外设 */

    usart_gpio_t        *u_gpio;        /* gpio配置 */
    usart_paras_t       *u_paras;       /* 串口参数配置 */

    usart_dma_t         *u_dma;         /* DMA配置 */
    usart_nvic_t        *u_nvic;        /* NVIC配置 */

    uint8_t              useDMA_INT;    /* 是否使用DMA - bit0  是否使用中断 - bit1 */
} usart_cfg_t;

/* API declare */
void usart_concernAll_config(usart_cfg_t *cfg);

#endif



