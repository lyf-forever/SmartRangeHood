#ifndef __SPI_H
#define __SPI_H

#include "sys.h"

#define SPI_RCC(SPIx)    SPIx == SPI1 ? PERIPH_APB2_RCC(SPI1) : \
                            SPIx == SPI2 ? PERIPH_APB1_RCC(SPI2) : PERIPH_APB1_RCC(SPI3) 

/* 传输模式 */
typedef enum {
    SPI_MODE_POLLING,       /* 轮询（默认，适合配置/控制类外设） */
    SPI_MODE_INTERRUPT,     /* 中断（适合单工流式传输） */
    SPI_MODE_DMA,           /* DMA（适合大批量全双工，如 Flash、屏） */
} SPI_TransferMode_t;        
      
/* 片选 GPIO 封装 */
typedef struct {
    GPIO_TypeDef* Port;
    uint16_t      Pin;
} SPI_CS_GPIO_t;

/* SPI 参数封装 */
typedef struct {
    uint16_t             SPI_Mode;               /* SPI_Mode_Master / Slave */
    uint16_t             SPI_Direction;          /* 2Lines_FullDuplex / 1Line_Tx / 1Line_Rx */
    uint16_t             SPI_DataSize;           /* 8b / 16b */
    uint16_t             SPI_CPOL;
    uint16_t             SPI_CPHA;
    uint16_t             SPI_NSS;                /* Soft / Hard */
    uint16_t             SPI_BaudRatePrescaler;  /* 如 SPI_BaudRatePrescaler_256 */
    uint16_t             SPI_FirstBit;           /* MSB / LSB */
} SPI_Paras_t;

/* DMA通道优先级 */
typedef struct {
    uint8_t TxChannel_Prio_Pre;
    uint8_t TxChannel_Prio_Sub;
    uint8_t RxChannel_Prio_Pre;
    uint8_t RxChannel_Prio_Sub;
} SPI_DMA_NVIC_Prio_t;

/* DMA信息封装 */
typedef struct {
    /* DMA 通道（仅 TransferMode=DMA 时有效） */
    DMA_Channel_TypeDef* DMA_TxCh;   /* SPI1=Ch3, SPI2=Ch5, SPI3=Ch2 */
    DMA_Channel_TypeDef* DMA_RxCh;   /* SPI1=Ch2, SPI2=Ch4, SPI3=Ch1 */

    uint32_t dma_rxCH_TCFlag;
    uint32_t dma_rxCH_TEFlag;

    /* DMA通道 NVIC优先级 */
    SPI_DMA_NVIC_Prio_t *Prio;
} SPI_DMA_Msg_t;

/* SPI中断信息封装 */
typedef struct {
    uint8_t spi_irq_src;
    uint8_t spi_IntPrio_Pre;
    uint8_t spi_IntPrio_Sub;
} SPI_Intr_Msg_t;

/* SPI 初始化参数 */
typedef struct {
    SPI_TypeDef*               SPIx;
    const SPI_Paras_t*         SPI_Paras;

    const SPI_TransferMode_t   TransferMode;    
    /* 软件片选 GPIO（主模式推荐） */
    SPI_CS_GPIO_t*             CS_GPIO;
    /* DMA信息 */
    SPI_DMA_Msg_t*             DMA_Msg;
    /* SPI 中断信息 */
    SPI_Intr_Msg_t*            SPI_Intr_Msg;
} SPI_InitParam_t;

/* 生命周期 */
void spi_concernAll_cfg(SPI_InitParam_t* param);
void spi_deinit(SPI_TypeDef* SPIx);

/* 片选控制（仅当 CS_Pin 不为空时有效） */
void SPI_CS_Low(SPI_TypeDef* SPIx);
void SPI_CS_High(SPI_TypeDef* SPIx);

/* 轮询接口（阻塞，全双工/单工） */
uint8_t SPI_TransmitReceiveByte(SPI_TypeDef* SPIx, uint8_t txByte);
void    SPI_Transmit(SPI_TypeDef* SPIx, uint8_t* txBuf, uint16_t len);
void    SPI_Receive(SPI_TypeDef* SPIx, uint8_t* rxBuf, uint16_t len);
void    SPI_TransmitReceive(SPI_TypeDef* SPIx, uint8_t* txBuf, uint8_t* rxBuf, uint16_t len);

/* 中断接口（非阻塞，单工） */
void SPI_Transmit_IT(SPI_TypeDef* SPIx, uint8_t* txBuf, uint16_t len);
void SPI_Receive_IT(SPI_TypeDef* SPIx, uint8_t* rxBuf, uint16_t len);

/* DMA 接口（非阻塞，全双工，必须同时提供收发缓冲区） */
void SPI_TransmitReceive_DMA(SPI_InitParam_t *param);

/* 状态查询 */
uint8_t SPI_IsBusy(SPI_TypeDef* SPIx);

/* 中断服务入口（在 stm32f10x_it.c 中调用） */
void SPI_IRQHandler(SPI_TypeDef* SPIx);
void SPI_DMA_IRQHandler(SPI_InitParam_t *par);

/* 用户回调（弱定义，按需重写） */
__attribute__((weak)) void SPI_TxCpltCallback(SPI_TypeDef* SPIx);
__attribute__((weak)) void SPI_RxCpltCallback(SPI_TypeDef* SPIx);
__attribute__((weak)) void SPI_TxRxCpltCallback(SPI_TypeDef* SPIx);

#endif /* __SPI_H */

