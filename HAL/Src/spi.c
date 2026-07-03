#include "spi.h"
#include <string.h>

/* 弱定义回调 */
__attribute__((weak)) void SPI_TxCpltCallback(SPI_TypeDef* SPIx) {}
__attribute__((weak)) void SPI_RxCpltCallback(SPI_TypeDef* SPIx) {}
__attribute__((weak)) void SPI_TxRxCpltCallback(SPI_TypeDef* SPIx) {}
__attribute__((weak)) void SPI_TxRxErrorCallback(SPI_TypeDef* SPIx) {}   

/* 内部句柄 */
typedef struct {
    uint8_t*  txBuf;
    uint8_t*  rxBuf;
    uint16_t  txLen;
    uint16_t  rxLen;
    uint16_t  txCnt;        /* 已写入 DR 的总字节数（真实数据 + Dummy） */
    uint16_t  rxCnt;
    SPI_CS_GPIO_t* csGPIO;
    __IO uint8_t   busy;
    uint8_t   txDone;       /* 发送侧（含 Dummy）已全部写入 DR */
    uint8_t   rxDone;       /* 接收侧已收满 rxLen */
} SPI_Handle_t;

static SPI_Handle_t s_spiHandle[3] = {0};  /* 0=SPI1, 1=SPI2, 2=SPI3 */

static uint8_t SPIx_To_Index(SPI_TypeDef* SPIx)
{
    if (SPIx == SPI1) return 0;
    if (SPIx == SPI2) return 1;
    return 2; /* SPI3 */
}

/* ------------------- 时钟 & GPIO 内部辅助 ------------------- */
static void spi_rcc_cfg(SPI_TypeDef* SPIx)
{
    if (SPIx == SPI1) {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);
    } else if (SPIx == SPI2) {
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
    } else {
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI3, ENABLE);
    }
}

static void spi_gpio_cfg(SPI_TypeDef* SPIx, uint16_t direction, uint16_t nssMode)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    if (SPIx == SPI1) {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
        
        /* SCK PA5 */
        GPIO_InitStruct.GPIO_Pin = GPIO_Pin_5;
        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
        GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(GPIOA, &GPIO_InitStruct);
        
        /* MOSI PA7 */
        if (direction != SPI_Direction_2Lines_RxOnly) {
            GPIO_InitStruct.GPIO_Pin = GPIO_Pin_7;
            /* 单线接收时，MOSI 作为输入，必须配成浮空/上拉输入 */
            if (direction != SPI_Direction_1Line_Rx) {
                GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;        /* 全双工 / 单线发送 */
                GPIO_Init(GPIOA, &GPIO_InitStruct);
            }  
        }
        /* MISO PA6 */
        /* 只在双线模式下才需要 MISO */
        if (direction == SPI_Direction_2Lines_FullDuplex || direction == SPI_Direction_2Lines_RxOnly) {
            GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6;
            GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
            GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
            GPIO_Init(GPIOA, &GPIO_InitStruct);
        }
        /* NSS PA4 硬件模式 */
        if (nssMode == SPI_NSS_Hard) {
            GPIO_InitStruct.GPIO_Pin = GPIO_Pin_4;
            GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
            GPIO_Init(GPIOA, &GPIO_InitStruct);
        }
    }
    else if (SPIx == SPI2) {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
        
        GPIO_InitStruct.GPIO_Pin = GPIO_Pin_13; /* SCK */
        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
        GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(GPIOB, &GPIO_InitStruct);
        
        if (direction != SPI_Direction_2Lines_RxOnly) {
            GPIO_InitStruct.GPIO_Pin = GPIO_Pin_15;
            /* 单线接收时，MOSI 作为输入，必须配成浮空/上拉输入 */
            if (direction != SPI_Direction_1Line_Rx) {
                GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;        /* 全双工 / 单线发送 */
                GPIO_Init(GPIOB, &GPIO_InitStruct);
            }  
        }
        if (direction == SPI_Direction_2Lines_FullDuplex || direction == SPI_Direction_2Lines_RxOnly) {
            GPIO_InitStruct.GPIO_Pin = GPIO_Pin_14; /* MISO */
            GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
            GPIO_Init(GPIOB, &GPIO_InitStruct);
        }
        if (nssMode == SPI_NSS_Hard) {
            GPIO_InitStruct.GPIO_Pin = GPIO_Pin_12;
            GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
            GPIO_Init(GPIOB, &GPIO_InitStruct);
        }
    }
    else { /* SPI3: PB3=SCK, PB4=MISO, PB5=MOSI, PA15=NSS */
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOA, ENABLE);
        
        GPIO_InitStruct.GPIO_Pin = GPIO_Pin_3; /* SCK */
        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
        GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(GPIOB, &GPIO_InitStruct);
        
        if (direction != SPI_Direction_2Lines_RxOnly) {
            GPIO_InitStruct.GPIO_Pin = GPIO_Pin_5;
            /* 单线接收时，MOSI 作为输入，必须配成浮空/上拉输入 */
            if (direction != SPI_Direction_1Line_Rx) {
                GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;        /* 全双工 / 单线发送 */
                GPIO_Init(GPIOB, &GPIO_InitStruct);
            }  
        }
        if (direction == SPI_Direction_2Lines_FullDuplex || direction == SPI_Direction_2Lines_RxOnly) {
            GPIO_InitStruct.GPIO_Pin = GPIO_Pin_4; /* MISO */
            GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
            GPIO_Init(GPIOB, &GPIO_InitStruct);
        }
        if (nssMode == SPI_NSS_Hard) {
            GPIO_InitStruct.GPIO_Pin = GPIO_Pin_15; /* NSS */
            GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
            GPIO_Init(GPIOA, &GPIO_InitStruct);
        }
    }
}

/* 使用SPI 软件NSS 时的片选引脚配置，初始状态为推挽输出高 */
static void spi_nss_sw_cfg(const SPI_CS_GPIO_t *cs_gpio) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_APB2PeriphClockCmd(
            (cs_gpio->Port == GPIOA) ? RCC_APB2Periph_GPIOA :
            (cs_gpio->Port == GPIOB) ? RCC_APB2Periph_GPIOB : RCC_APB2Periph_GPIOC, ENABLE);
    GPIO_InitStruct.GPIO_Pin = cs_gpio->Pin;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(cs_gpio->Port, &GPIO_InitStruct);

    GPIO_SetBits(cs_gpio->Port, cs_gpio->Pin);
}

/* SPI相关参数配置 */
static void spi_paras_cfg(SPI_TypeDef *SPIx, const SPI_Paras_t *paras) {
    SPI_InitTypeDef SPI_InitStruct = {0};

    SPI_StructInit(&SPI_InitStruct);
    SPI_InitStruct.SPI_Direction = paras->SPI_Direction;
    SPI_InitStruct.SPI_Mode = paras->SPI_Mode;
    SPI_InitStruct.SPI_DataSize = paras->SPI_DataSize;
    SPI_InitStruct.SPI_CPOL = paras->SPI_CPOL;
    SPI_InitStruct.SPI_CPHA = paras->SPI_CPHA;
    SPI_InitStruct.SPI_NSS = paras->SPI_NSS;
    SPI_InitStruct.SPI_BaudRatePrescaler = paras->SPI_BaudRatePrescaler;
    SPI_InitStruct.SPI_FirstBit = paras->SPI_FirstBit;
    SPI_InitStruct.SPI_CRCPolynomial = 7;

    SPI_Init(SPIx, &SPI_InitStruct);
}

/* SPI DMA 配置 */
static void spi_dma_cfg(SPI_InitParam_t *param) {
    RCC_AHBPeriphClockCmd(param->SPIx == SPI3 ? RCC_AHBPeriph_DMA2 : RCC_AHBPeriph_DMA1, ENABLE);
    
    SPI_DMA_Msg_t *dma_msg = param->DMA_Msg;
    dma_msg->DMA_TxCh = (param->SPIx == SPI1) ? DMA1_Channel3 : (param->SPIx == SPI2) ? 
                        DMA1_Channel5 : DMA2_Channel2;
    dma_msg->DMA_RxCh = (param->SPIx == SPI1) ? DMA1_Channel2 : (param->SPIx == SPI2) ? 
                        DMA1_Channel4 : DMA2_Channel1;

    if(dma_msg->DMA_RxCh == DMA1_Channel2) {
        dma_msg->dma_rxCH_TCFlag = DMA1_IT_TC2; dma_msg->dma_rxCH_TEFlag = DMA1_IT_TE2; 
    } else if(dma_msg->DMA_RxCh == DMA1_Channel4) { 
        dma_msg->dma_rxCH_TCFlag = DMA1_IT_TC4; dma_msg->dma_rxCH_TEFlag = DMA1_IT_TE4; 
    } else { 
        dma_msg->dma_rxCH_TCFlag = DMA2_IT_TC1; dma_msg->dma_rxCH_TEFlag = DMA2_IT_TE1; 
    }

    DMA_InitTypeDef DMA_InitStruct = {0};
        
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&param->SPIx->DR;
    DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStruct.DMA_PeripheralDataSize = (param->SPI_Paras->SPI_DataSize == SPI_DataSize_8b) 
                                                ? DMA_PeripheralDataSize_Byte 
                                                : DMA_PeripheralDataSize_HalfWord;
    DMA_InitStruct.DMA_MemoryDataSize =     (param->SPI_Paras->SPI_DataSize == SPI_DataSize_8b) 
                                                ? DMA_MemoryDataSize_Byte 
                                                : DMA_MemoryDataSize_HalfWord; 
    DMA_InitStruct.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStruct.DMA_Priority = DMA_Priority_High;
    DMA_InitStruct.DMA_M2M = DMA_M2M_Disable;        
    /* TX */
    DMA_InitStruct.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_Init(dma_msg->DMA_TxCh, &DMA_InitStruct);
    /* RX */
    DMA_InitStruct.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_Init(dma_msg->DMA_RxCh, &DMA_InitStruct);
     
    SPI_I2S_DMACmd(param->SPIx, SPI_I2S_DMAReq_Tx | SPI_I2S_DMAReq_Rx, ENABLE); 
    /* DMA 中断（传输完成） */
    //DMA_ITConfig(dma_msg->DMA_TxCh, DMA_IT_TC, ENABLE);
    DMA_ITConfig(dma_msg->DMA_RxCh, DMA_IT_TC, ENABLE);
    /* NVIC配置 */
    NVIC_InitTypeDef NVIC_InitStruct = {0};
    // NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = dma_msg->Prio->TxChannel_Prio_Pre;
    // NVIC_InitStruct.NVIC_IRQChannelSubPriority = dma_msg->Prio->TxChannel_Prio_Sub;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
        
    // if (dma_msg->DMA_TxCh == DMA1_Channel3) NVIC_InitStruct.NVIC_IRQChannel = DMA1_Channel3_IRQn;
    // else if (dma_msg->DMA_TxCh == DMA1_Channel5) NVIC_InitStruct.NVIC_IRQChannel = DMA1_Channel5_IRQn;
    // else NVIC_InitStruct.NVIC_IRQChannel = DMA2_Channel2_IRQn;
    // NVIC_Init(&NVIC_InitStruct);
      
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = dma_msg->Prio->RxChannel_Prio_Pre;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = dma_msg->Prio->RxChannel_Prio_Sub;
    if (dma_msg->DMA_RxCh == DMA1_Channel2) NVIC_InitStruct.NVIC_IRQChannel = DMA1_Channel2_IRQn;
    else if (dma_msg->DMA_RxCh == DMA1_Channel4) NVIC_InitStruct.NVIC_IRQChannel = DMA1_Channel4_IRQn;
    else NVIC_InitStruct.NVIC_IRQChannel = DMA2_Channel1_IRQn;
    NVIC_Init(&NVIC_InitStruct);
}

/* SPI 中断配置 */
static void spi_intr_cfg(SPI_InitParam_t *param) {
    param->SPI_Intr_Msg->spi_irq_src = (param->SPIx == SPI1) ? SPI1_IRQn : (param->SPIx == SPI2) ? SPI2_IRQn : SPI3_IRQn;
    
    NVIC_InitTypeDef NVIC_InitStruct = {0};
    NVIC_InitStruct.NVIC_IRQChannel = param->SPI_Intr_Msg->spi_irq_src;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = param->SPI_Intr_Msg->spi_IntPrio_Pre;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = param->SPI_Intr_Msg->spi_IntPrio_Sub;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
        
    NVIC_Init(&NVIC_InitStruct);
}
/* ------------------- 对外接口 ------------------- */

void spi_concernAll_cfg(SPI_InitParam_t* param)
{
    spi_rcc_cfg(param->SPIx);
    spi_gpio_cfg(param->SPIx, param->SPI_Paras->SPI_Direction, param->SPI_Paras->SPI_NSS);

    uint8_t idx = SPIx_To_Index(param->SPIx);
    memset(&s_spiHandle[idx], 0, sizeof(SPI_Handle_t));
    s_spiHandle[idx].csGPIO = param->CS_GPIO;
    /* SPI cs gpio config if software NSS usage */
    if (param->SPI_Paras->SPI_NSS == SPI_NSS_Soft && param->CS_GPIO != NULL) {
        spi_nss_sw_cfg(param->CS_GPIO);
    }
    /* SPI 参数配置 */
    spi_paras_cfg(param->SPIx, param->SPI_Paras);
    
    /* DMA配置（仅使能外设请求，不启动通道） */
    if (param->TransferMode == SPI_MODE_DMA)    spi_dma_cfg(param);
    /* 中断配置 */
    if (param->TransferMode == SPI_MODE_INTERRUPT)  spi_intr_cfg(param);
    
    SPI_Cmd(param->SPIx, ENABLE);
}

void spi_deinit(SPI_TypeDef* SPIx)
{
    SPI_Cmd(SPIx, DISABLE);
    SPI_I2S_DMACmd(SPIx, SPI_I2S_DMAReq_Tx | SPI_I2S_DMAReq_Rx, DISABLE);
    
    if (SPIx == SPI1) RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, DISABLE);
    else if (SPIx == SPI2) RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, DISABLE);
    else RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI3, DISABLE);
}

/* 软件NSS GPIO片选控制 */
void SPI_CS_Low(SPI_TypeDef* SPIx)
{
    SPI_Handle_t* h = &s_spiHandle[SPIx_To_Index(SPIx)];
    if (h->csGPIO != NULL) GPIO_ResetBits(h->csGPIO->Port, h->csGPIO->Pin);
}

void SPI_CS_High(SPI_TypeDef* SPIx)
{
    SPI_Handle_t* h = &s_spiHandle[SPIx_To_Index(SPIx)];
    if (h->csGPIO != NULL) GPIO_SetBits(h->csGPIO->Port, h->csGPIO->Pin);
}

/* --------------------------- 轮询方式 ------------------------------ */

uint8_t SPI_TransmitReceiveByte(SPI_TypeDef* SPIx, uint8_t txByte)
{
    while (SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(SPIx, txByte);
    while (SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_RXNE) == RESET);
    return (uint8_t)SPI_I2S_ReceiveData(SPIx);
}

void SPI_Transmit(SPI_TypeDef* SPIx, uint8_t* txBuf, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        while (SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_TXE) == RESET);
        SPI_I2S_SendData(SPIx, txBuf[i]);
        while (SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_RXNE) == RESET);
        SPI_I2S_ReceiveData(SPIx); /* 清 RXNE */
    }
    while (SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_BSY) == SET);
}

void SPI_Receive(SPI_TypeDef* SPIx, uint8_t* rxBuf, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        while (SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_TXE) == RESET);
        SPI_I2S_SendData(SPIx, 0xFF); /* 发 Dummy 产生时钟 */
        while (SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_RXNE) == RESET);
        rxBuf[i] = (uint8_t)SPI_I2S_ReceiveData(SPIx);
    }
    while (SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_BSY) == SET);
}

void SPI_TransmitReceive(SPI_TypeDef* SPIx, uint8_t* txBuf, uint8_t* rxBuf, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        while (SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_TXE) == RESET);
        SPI_I2S_SendData(SPIx, txBuf[i]);
        while (SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_RXNE) == RESET);
        rxBuf[i] = (uint8_t)SPI_I2S_ReceiveData(SPIx);
    }
    while (SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_BSY) == SET);
}

/* --------------------------- 轮询方式 ------------------------------ */

/* ------------------------ 中断方式（单工） --------------------------- */

void SPI_Transmit_IT(SPI_TypeDef* SPIx, uint8_t* txBuf, uint16_t len)
{
    uint8_t idx = SPIx_To_Index(SPIx);
    s_spiHandle[idx].txBuf = txBuf;
    s_spiHandle[idx].txLen = len;
    s_spiHandle[idx].txCnt = 0;
    s_spiHandle[idx].busy  = 1;
    
    SPI_I2S_ITConfig(SPIx, SPI_I2S_IT_TXE, ENABLE);
}

void SPI_Receive_IT(SPI_TypeDef* SPIx, uint8_t* rxBuf, uint16_t len)
{
    uint8_t idx = SPIx_To_Index(SPIx);
    s_spiHandle[idx].rxBuf = rxBuf;
    s_spiHandle[idx].rxLen = len;
    s_spiHandle[idx].rxCnt = 0;
    s_spiHandle[idx].busy  = 1;
    
    /* 先发送一个 Dummy 启动时钟，后续在 TXE 中自动补发 先开 RXNE，再手动发第一个 Dummy 启动时钟  */
    SPI_I2S_ITConfig(SPIx, SPI_I2S_IT_RXNE | SPI_I2S_IT_TXE, ENABLE);
    while (SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(SPIx, 0xFF);
}



/* ------------------------ 中断方式（单工） --------------------------- */

/* ---------------------------------- DMA 方式（全双工） -------------------------------------- */

void SPI_TransmitReceive_DMA(SPI_InitParam_t *param)
{
    SPI_Handle_t *handle = &s_spiHandle[SPIx_To_Index(param->SPIx)];

    SPI_DMA_Msg_t *h = param->DMA_Msg; 

    DMA_Cmd(h->DMA_TxCh, DISABLE);
    DMA_Cmd(h->DMA_RxCh, DISABLE);
    
    h->DMA_TxCh->CMAR = (uint32_t)handle->txBuf;
    h->DMA_RxCh->CMAR = (uint32_t)handle->rxBuf;
    DMA_SetCurrDataCounter(h->DMA_TxCh, handle->txLen);
    DMA_SetCurrDataCounter(h->DMA_RxCh, handle->rxLen);
    
    handle->busy = 1;
    
    DMA_Cmd(h->DMA_RxCh, ENABLE);
    DMA_Cmd(h->DMA_TxCh, ENABLE);  /* TX 最后开，启动传输 */
}

/* ---------------------------------- DMA 方式（全双工） -------------------------------------- */

/* -------------------------------- 中断服务 ---------------------------------------- */

/* 内部统一收尾函数 */
static void SPI_IRQHandler_Close(SPI_Handle_t* h, SPI_TypeDef* SPIx)
{
    h->busy   = 0;
    h->txDone = 0;
    h->rxDone = 0;

    if (h->txLen > 0 && h->rxLen > 0)   
        SPI_TxRxCpltCallback(SPIx);
    else if (h->txLen > 0)  
        SPI_TxCpltCallback(SPIx);
    else  
        SPI_RxCpltCallback(SPIx);
}  

void SPI_IRQHandler(SPI_TypeDef* SPIx)
{
    SPI_Handle_t* h = &s_spiHandle[SPIx_To_Index(SPIx)];

    /* ==================== RXNE ==================== */
    if (SPI_I2S_GetITStatus(SPIx, SPI_I2S_IT_RXNE) != RESET) {
        uint16_t data = SPI_I2S_ReceiveData(SPIx);

        if (h->rxCnt < h->rxLen && h->rxBuf != NULL) {
            h->rxBuf[h->rxCnt++] = (uint8_t)data;
        }
        /* else: 纯发模式下读出的回显数据，或溢出数据，直接丢弃防止 OVR */

        /* 接收完成判定 */
        if (h->rxCnt >= h->rxLen) {
            SPI_I2S_ITConfig(SPIx, SPI_I2S_IT_RXNE, DISABLE);
            h->rxDone = 1;

            /* 若发送侧也完成了（或本来就没有发送任务），统一收尾 */
            if (h->txDone)  SPI_IRQHandler_Close(h, SPIx);
        }
    }

    /* ==================== TXE ==================== */
    if (SPI_I2S_GetITStatus(SPIx, SPI_I2S_IT_TXE) != RESET) {
        /* 总时钟数 = 真实发送与接收需求的最大值 */
        uint16_t totalLen = (h->txLen > h->rxLen) ? h->txLen : h->rxLen;

        if (h->txCnt < totalLen) {
            if (h->txCnt < h->txLen) {
                SPI_I2S_SendData(SPIx, h->txBuf[h->txCnt]);   /* 真实数据 */
            } else {
                SPI_I2S_SendData(SPIx, 0xFF);                 /* 补时钟 Dummy */
            }
            h->txCnt++;
        } else {
            /* 所有字节（含 Dummy）已写入 DR，关闭 TXE 中断 */
            SPI_I2S_ITConfig(SPIx, SPI_I2S_IT_TXE, DISABLE);
            h->txDone = 1;

            /* 无接收任务，或接收已先完成，直接收尾 */
            if (h->rxLen == 0 || h->rxDone) {
                SPI_IRQHandler_Close(h, SPIx);
            }
            /* 否则：等 RXNE 收完最后一个字节后，在 RXNE 分支收尾 */
        }
    }

    /* ==================== 错误处理（防止中断死循环） ==================== */
    if (SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_OVR) == SET) {
        /* 清 OVR 标准序列：先读 DR，再读 SR */
        (void)SPI_I2S_ReceiveData(SPIx);
        (void)SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_OVR);
    }
}

void SPI_DMA_IRQHandler(SPI_InitParam_t *par)
{
    if (DMA_GetITStatus(par->DMA_Msg->dma_rxCH_TCFlag)) {
        DMA_ClearITPendingBit(par->DMA_Msg->dma_rxCH_TCFlag);
        s_spiHandle[SPIx_To_Index(par->SPIx)].busy = 0;
        SPI_TxRxCpltCallback(par->SPIx);
    }

    /* 错误处理 */
    if (DMA_GetITStatus(par->DMA_Msg->dma_rxCH_TEFlag)) {
        DMA_ClearITPendingBit(par->DMA_Msg->dma_rxCH_TEFlag);
        SPI_TxRxErrorCallback(par->SPIx);
    }
}

/* -------------------------------- 中断服务 ---------------------------------------- */

/* 判断SPI是否在忙 0：空闲 1：工作中*/
uint8_t SPI_IsBusy(SPI_TypeDef* SPIx)
{
    return s_spiHandle[SPIx_To_Index(SPIx)].busy;
}




