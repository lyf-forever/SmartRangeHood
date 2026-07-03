#include "usart.h"

/* 获取某 USART 的 TX/RX cfg->u_gpio 端口与引脚 */
static void get_usart_gpio(usart_cfg_t *cfg)
{
    if (cfg->instance == USART1) {
        cfg->u_gpio->tx_port = GPIOA; cfg->u_gpio->tx_pin = GPIO_Pin_9;
        cfg->u_gpio->rx_port = GPIOA; cfg->u_gpio->rx_pin = GPIO_Pin_10;
    } else if (cfg->instance == USART2) {
        cfg->u_gpio->tx_port = GPIOA; cfg->u_gpio->tx_pin = GPIO_Pin_2;
        cfg->u_gpio->rx_port = GPIOA; cfg->u_gpio->rx_pin = GPIO_Pin_3;
    } else if (cfg->instance == USART3) {
        cfg->u_gpio->tx_port = GPIOB; cfg->u_gpio->tx_pin = GPIO_Pin_10;   /* 与lcd 背光BL引脚冲突 */
        cfg->u_gpio->rx_port = GPIOB; cfg->u_gpio->rx_pin = GPIO_Pin_11;
    } else if (cfg->instance == UART4) {
        cfg->u_gpio->tx_port = GPIOC; cfg->u_gpio->tx_pin = GPIO_Pin_10;
        cfg->u_gpio->rx_port = GPIOC; cfg->u_gpio->rx_pin = GPIO_Pin_11;
    } else if (cfg->instance == UART5) {
        cfg->u_gpio->tx_port = GPIOC; cfg->u_gpio->tx_pin = GPIO_Pin_12;
        cfg->u_gpio->rx_port = GPIOD; cfg->u_gpio->rx_pin = GPIO_Pin_2;
    }
}

/* 获取某 USART 对应的 DMA 通道及触发标志 */
static void get_usart_dma(usart_cfg_t *cfg)
{
    cfg->u_dma->dma = DMA1;
    if (cfg->instance == USART1) {
        cfg->u_dma->tx_ch = DMA1_Channel4; cfg->u_dma->rx_ch = DMA1_Channel5;
        //cfg->u_dma->tc_flag_tx = DMA1_FLAG_TC4; cfg->u_dma->tc_flag_rx = DMA1_FLAG_TC5;
    } else if (cfg->instance == USART2) {
        cfg->u_dma->tx_ch = DMA1_Channel7; cfg->u_dma->rx_ch = DMA1_Channel6;
        //cfg->u_dma->tc_flag_tx = DMA1_FLAG_TC7; cfg->u_dma->tc_flag_rx = DMA1_FLAG_TC6;
    } else if (cfg->instance == USART3) {
        cfg->u_dma->tx_ch = DMA1_Channel2; cfg->u_dma->rx_ch = DMA1_Channel3;
        //cfg->u_dma->tc_flag_tx = DMA1_FLAG_TC2; cfg->u_dma->tc_flag_rx = DMA1_FLAG_TC3;
    } else if (cfg->instance == UART4) {
        cfg->u_dma->dma = DMA2;
        cfg->u_dma->tx_ch = DMA2_Channel5; cfg->u_dma->rx_ch = DMA2_Channel3;
        //cfg->u_dma->tc_flag_tx = DMA2_FLAG_TC5; cfg->u_dma->tc_flag_rx = DMA2_FLAG_TC3;
    } else {
        cfg->u_dma->dma = NULL;
        return;   // UART5 无 DMA
    }
}

/* 获取 USART 对应的中断向量号 (若无DMA 则dma_tx dma_rx 填0)*/
static void get_usart_nvic(usart_cfg_t *cfg)
{
    if (cfg->instance == USART1) {
        cfg->u_nvic->usart_intrMsg->irq_usart = USART1_IRQn;
        cfg->u_nvic->dma_intrMsg->irq_dma_tx = DMA1_Channel4_IRQn;
        cfg->u_nvic->dma_intrMsg->irq_dma_rx = DMA1_Channel5_IRQn;
    } else if (cfg->instance == USART2) {
        cfg->u_nvic->usart_intrMsg->irq_usart = USART2_IRQn;
        cfg->u_nvic->dma_intrMsg->irq_dma_tx = DMA1_Channel7_IRQn;
        cfg->u_nvic->dma_intrMsg->irq_dma_rx = DMA1_Channel6_IRQn;
    } else if (cfg->instance == USART3) {
        cfg->u_nvic->usart_intrMsg->irq_usart = USART3_IRQn;
        cfg->u_nvic->dma_intrMsg->irq_dma_tx = DMA1_Channel2_IRQn;
        cfg->u_nvic->dma_intrMsg->irq_dma_rx = DMA1_Channel3_IRQn;
    } else if (cfg->instance == UART4) {
        cfg->u_nvic->usart_intrMsg->irq_usart = UART4_IRQn;
        cfg->u_nvic->dma_intrMsg->irq_dma_tx = DMA2_Channel4_5_IRQn;  
        cfg->u_nvic->dma_intrMsg->irq_dma_rx = DMA2_Channel3_IRQn;
    } else if (cfg->instance == UART5) {
        cfg->u_nvic->usart_intrMsg->irq_usart = UART5_IRQn;     // UART5 无 DMA
        cfg->u_nvic->dma_intrMsg->irq_dma_tx = (IRQn_Type)0;
        cfg->u_nvic->dma_intrMsg->irq_dma_rx = (IRQn_Type)0;
    } else return;
}

/* 获取USART配置的相关信息，如GPIO DMA NVIC */
static void get_usart_message(usart_cfg_t *cfg)
{
    get_usart_gpio(cfg);
    if(cfg->useDMA_INT & 0x01) 
        get_usart_dma(cfg);
    if(cfg->useDMA_INT & 0x02)
        get_usart_nvic(cfg);
}

/* USART外设时钟配置 */
static void usart_periph_clk_cfg(usart_cfg_t *cfg)
{
    if (cfg->instance == USART1) {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
        return;
    }

    static const struct {           
        USART_TypeDef *instance;
        uint32_t periph;
    } apb1_map[] = {
        { USART2, RCC_APB1Periph_USART2 },
        { USART3, RCC_APB1Periph_USART3 },
        { UART4,  RCC_APB1Periph_UART4  },
        { UART5,  RCC_APB1Periph_UART5  },
    };

    for (uint8_t i = 0; i < sizeof(apb1_map)/sizeof(apb1_map[0]); i++) {
        if (apb1_map[i].instance == cfg->instance) {
            RCC_APB1PeriphClockCmd(apb1_map[i].periph, ENABLE);
            return;
        }
    }
}

/* USART GPIO时钟配置 */
static void usart_gpio_clk_cfg(usart_cfg_t *cfg)
{
    static const struct {
        GPIO_TypeDef *port;
        uint32_t periph;
    } gpioMap[] = {
        {GPIOA, RCC_APB2Periph_GPIOA},
        {GPIOB, RCC_APB2Periph_GPIOB},
        {GPIOC, RCC_APB2Periph_GPIOC},
        {GPIOD, RCC_APB2Periph_GPIOD},
        {GPIOE, RCC_APB2Periph_GPIOE},
        {GPIOF, RCC_APB2Periph_GPIOF},
        {GPIOG, RCC_APB2Periph_GPIOG},
    };
    if(cfg->u_gpio->tx_port == cfg->u_gpio->rx_port) {
        for(uint8_t i = 0; i < sizeof(gpioMap)/sizeof(gpioMap[0]); i++) {
            if(gpioMap[i].port == cfg->u_gpio->tx_port) {
                RCC_APB2PeriphClockCmd(gpioMap[i].periph, ENABLE);
                return;
            }
        }
    } else {
        uint8_t times = 0;
        for(uint8_t i = 0; i < sizeof(gpioMap)/sizeof(gpioMap[0]); i++) {
            if(gpioMap[i].port == cfg->u_gpio->tx_port || gpioMap[i].port == cfg->u_gpio->rx_port) {
                RCC_APB2PeriphClockCmd(gpioMap[i].periph, ENABLE);
                times ++;
                if(times == 2) return;
            }
        }  
    }
}

/* USART DMA时钟配置 */
static void usart_dma_clk_cfg(usart_cfg_t *cfg) {
    if(cfg->u_dma->dma == DMA2) {
        RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA2, ENABLE);
        return;
    }
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
}

/* USART时钟配置,包含GPIO USART DMA   */
static void usart_clock_cfg(usart_cfg_t *cfg) {
    usart_periph_clk_cfg(cfg);
    usart_gpio_clk_cfg(cfg);
    if(cfg->useDMA_INT & 0x01)    
        usart_dma_clk_cfg(cfg);
}

/* USART GPIO 配置(只考虑TX RX，流控相关引脚有需要再优化)*/
static void usart_gpio_cfg(usart_cfg_t *cfg) {
    GPIO_InitTypeDef gpio_initStruct = {0};
    /* TX */
    gpio_initStruct.GPIO_Pin = cfg->u_gpio->tx_pin;
    gpio_initStruct.GPIO_Mode = cfg->u_gpio->tx_mode;
    gpio_initStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(cfg->u_gpio->tx_port, &gpio_initStruct);
    /* RX */
    gpio_initStruct.GPIO_Pin = cfg->u_gpio->rx_pin;
    gpio_initStruct.GPIO_Mode = cfg->u_gpio->rx_mode;
    GPIO_Init(cfg->u_gpio->rx_port, &gpio_initStruct);
}

/* USART 参数配置 */
static void usart_paras_cfg(usart_cfg_t *cfg) {
    USART_InitTypeDef usart_initStruct = {0};
    usart_paras_t *para = cfg->u_paras;

    /* USART参数配置 */
    usart_initStruct.USART_BaudRate    = para->baudrate;
    usart_initStruct.USART_WordLength  = para->wordlen;
    usart_initStruct.USART_StopBits    = para->stopbits;
    usart_initStruct.USART_Parity      = para->parity;
    usart_initStruct.USART_HardwareFlowControl = para->hwflowctl;
    usart_initStruct.USART_Mode = para->mode;      
    /* 初始化USART */
    USART_Init(cfg->instance, &usart_initStruct);
}

/* USART DMA 配置*/
static void usart_dma_cfg(usart_cfg_t *cfg) {
    /* ----------------------- 接受 DMA ----------------------------- */ 
    DMA_DeInit(cfg->u_dma->rx_ch);
    if(cfg->u_dma->rx_buf && cfg->u_dma->rx_buf_size) {
        DMA_InitTypeDef dma_rx = {
            .DMA_PeripheralBaseAddr = (uint32_t)&(cfg->instance->DR),
            .DMA_MemoryBaseAddr     = (uint32_t)cfg->u_dma->rx_buf,
            .DMA_DIR                = DMA_DIR_PeripheralSRC,
            .DMA_BufferSize         = cfg->u_dma->rx_buf_size,
            .DMA_PeripheralInc      = DMA_PeripheralInc_Disable,
            .DMA_MemoryInc          = DMA_MemoryInc_Enable,
            .DMA_PeripheralDataSize = cfg->u_dma->dma_rx_periphdatasize,
            .DMA_MemoryDataSize     = cfg->u_dma->dma_rx_memdatasize,
            .DMA_Mode               = cfg->u_dma->dma_rx_mode,
            .DMA_Priority           = cfg->u_dma->dma_rx_prio,
            .DMA_M2M                = DMA_M2M_Disable,
        };
        DMA_Init(cfg->u_dma->rx_ch, &dma_rx);
        if((!(cfg->useDMA_INT & 0x02)) || 
                ((cfg->useDMA_INT & 0x02) && (cfg->u_nvic->intrUse == 0)) || 
                    ((cfg->useDMA_INT & 0x02) && (cfg->u_nvic->intrUse >= 1) && (cfg->u_nvic->dma_intrMsg->dma_int_ch == -1))) {
            DMA_Cmd(cfg->u_dma->rx_ch, ENABLE);
            USART_DMACmd(cfg->instance, USART_DMAReq_Rx, ENABLE);
        }
    }
    /* ------------------- 发送 DMA (普通模式) ------------------- */ 
    if (cfg->u_dma->tx_buf && cfg->u_dma->tx_buf_size) {
        DMA_InitTypeDef dma_tx = {
            .DMA_PeripheralBaseAddr = (uint32_t)&cfg->instance->DR,
            .DMA_MemoryBaseAddr     = (uint32_t)cfg->u_dma->tx_buf,
            .DMA_DIR                = DMA_DIR_PeripheralDST,
            .DMA_BufferSize         = cfg->u_dma->tx_buf_size,
            .DMA_PeripheralInc      = DMA_PeripheralInc_Disable,
            .DMA_MemoryInc          = DMA_MemoryInc_Enable,
            .DMA_PeripheralDataSize = cfg->u_dma->dma_tx_periphdatasize,
            .DMA_MemoryDataSize     = cfg->u_dma->dma_tx_memdatasize,
            .DMA_Mode               = DMA_Mode_Normal,
            .DMA_Priority           = cfg->u_dma->dma_tx_prio,
            .DMA_M2M                = DMA_M2M_Disable,
        };
        DMA_Init(cfg->u_dma->tx_ch, &dma_tx);
        // DMA_Cmd(*tx_ch, ENABLE) 在需要发送时才打开，避免一初始化就发送
        if((!(cfg->useDMA_INT & 0x02)) || 
                ((cfg->useDMA_INT & 0x02) && (cfg->u_nvic->intrUse == 0)) || 
                    ((cfg->useDMA_INT & 0x02) && (cfg->u_nvic->intrUse >= 1) && (cfg->u_nvic->dma_intrMsg->dma_int_ch == 0)))
            USART_DMACmd(cfg->instance, USART_DMAReq_Tx, ENABLE);
    } 
}

static void nvic_cfg(NVIC_InitTypeDef *nvic, IRQn_Type irq, uint8_t nvic_preprio, uint8_t nvic_subprio)
{
    nvic->NVIC_IRQChannel = irq;  
    nvic->NVIC_IRQChannelPreemptionPriority = nvic_preprio;
    nvic->NVIC_IRQChannelSubPriority = nvic_subprio;
    nvic->NVIC_IRQChannelCmd = ENABLE;
}

static void usart_nvic_cfg(usart_cfg_t *cfg) {
    NVIC_InitTypeDef nvic_initStruct = {0};
    USART_TypeDef *usartx = cfg->instance;

    switch(cfg->u_nvic->intrUse) {
        case USE_INT_USART_ONLY:
            USART_ITConfig(usartx, cfg->u_nvic->usart_intrMsg->usart_intType, ENABLE);
            nvic_cfg(&nvic_initStruct, cfg->u_nvic->usart_intrMsg->irq_usart, cfg->u_nvic->usart_intrMsg->nvic_preprio,
                      cfg->u_nvic->usart_intrMsg->nvic_subprio);
            break;
        case USE_INT_DMA_ONLY:
            if(cfg->u_nvic->dma_intrMsg->dma_int_ch < 0) {    /* TX CH */
                if(IS_DMA_CONFIG_IT(cfg->u_nvic->dma_intrMsg->dma_txch_int_type) != 0) {
                    DMA_ITConfig(cfg->u_dma->tx_ch, cfg->u_nvic->dma_intrMsg->dma_txch_int_type, ENABLE);
                    nvic_cfg(&nvic_initStruct, cfg->u_nvic->dma_intrMsg->irq_dma_tx,
                                cfg->u_nvic->dma_intrMsg->nvic_txch_preprio, cfg->u_nvic->dma_intrMsg->nvic_txch_subprio);
                }
            } else if(cfg->u_nvic->dma_intrMsg->dma_int_ch == 0) {  /* RX CH */
                if(IS_DMA_CONFIG_IT(cfg->u_nvic->dma_intrMsg->dma_rxch_int_type) != 0) {
                    DMA_ITConfig(cfg->u_dma->rx_ch, cfg->u_nvic->dma_intrMsg->dma_rxch_int_type, ENABLE);
                    nvic_cfg(&nvic_initStruct, cfg->u_nvic->dma_intrMsg->irq_dma_rx,
                                cfg->u_nvic->dma_intrMsg->nvic_rxch_preprio, cfg->u_nvic->dma_intrMsg->nvic_rxch_subprio);
                }
            } else {    /* TX RX CH BOTH */
                if((IS_DMA_CONFIG_IT(cfg->u_nvic->dma_intrMsg->dma_txch_int_type) != 0) && 
                    (IS_DMA_CONFIG_IT(cfg->u_nvic->dma_intrMsg->dma_rxch_int_type) != 0)) {
                        DMA_ITConfig(cfg->u_dma->tx_ch, cfg->u_nvic->dma_intrMsg->dma_txch_int_type, ENABLE);
                        nvic_cfg(&nvic_initStruct, cfg->u_nvic->dma_intrMsg->irq_dma_tx,
                                    cfg->u_nvic->dma_intrMsg->nvic_txch_preprio, cfg->u_nvic->dma_intrMsg->nvic_txch_subprio);
                        NVIC_Init(&nvic_initStruct);
                        DMA_ITConfig(cfg->u_dma->rx_ch, cfg->u_nvic->dma_intrMsg->dma_rxch_int_type, ENABLE);
                        nvic_cfg(&nvic_initStruct, cfg->u_nvic->dma_intrMsg->irq_dma_rx,
                                    cfg->u_nvic->dma_intrMsg->nvic_rxch_preprio, cfg->u_nvic->dma_intrMsg->nvic_rxch_subprio);
                    }
            }
            break;
        case USE_INT_BOTH_USART_DMA:
            /* USART interrupt cfg */
            USART_ITConfig(usartx, cfg->u_nvic->usart_intrMsg->usart_intType, ENABLE);
            nvic_cfg(&nvic_initStruct, cfg->u_nvic->usart_intrMsg->irq_usart, cfg->u_nvic->usart_intrMsg->nvic_preprio,
                      cfg->u_nvic->usart_intrMsg->nvic_subprio);
            NVIC_Init(&nvic_initStruct);
            /* DMA interrupt cfg */
            if(cfg->u_nvic->dma_intrMsg->dma_int_ch < 0) {    /* TX CH */
                if(IS_DMA_CONFIG_IT(cfg->u_nvic->dma_intrMsg->dma_txch_int_type) != 0) {
                    DMA_ITConfig(cfg->u_dma->tx_ch, cfg->u_nvic->dma_intrMsg->dma_txch_int_type, ENABLE);
                    nvic_cfg(&nvic_initStruct, cfg->u_nvic->dma_intrMsg->irq_dma_tx,
                                cfg->u_nvic->dma_intrMsg->nvic_txch_preprio, cfg->u_nvic->dma_intrMsg->nvic_txch_subprio);
                }
            } else if(cfg->u_nvic->dma_intrMsg->dma_int_ch == 0) {  /* RX CH */
                if(IS_DMA_CONFIG_IT(cfg->u_nvic->dma_intrMsg->dma_rxch_int_type) != 0) {
                    DMA_ITConfig(cfg->u_dma->rx_ch, cfg->u_nvic->dma_intrMsg->dma_rxch_int_type, ENABLE);
                    nvic_cfg(&nvic_initStruct, cfg->u_nvic->dma_intrMsg->irq_dma_rx,
                                cfg->u_nvic->dma_intrMsg->nvic_rxch_preprio, cfg->u_nvic->dma_intrMsg->nvic_rxch_subprio);
                }
            } else {    /* TX RX CH BOTH */
                if((IS_DMA_CONFIG_IT(cfg->u_nvic->dma_intrMsg->dma_txch_int_type) != 0) && 
                    (IS_DMA_CONFIG_IT(cfg->u_nvic->dma_intrMsg->dma_rxch_int_type) != 0)) {
                        DMA_ITConfig(cfg->u_dma->tx_ch, cfg->u_nvic->dma_intrMsg->dma_txch_int_type, ENABLE);
                        nvic_cfg(&nvic_initStruct, cfg->u_nvic->dma_intrMsg->irq_dma_tx,
                                    cfg->u_nvic->dma_intrMsg->nvic_txch_preprio, cfg->u_nvic->dma_intrMsg->nvic_txch_subprio);
                        NVIC_Init(&nvic_initStruct);
                        DMA_ITConfig(cfg->u_dma->rx_ch, cfg->u_nvic->dma_intrMsg->dma_rxch_int_type, ENABLE);
                        nvic_cfg(&nvic_initStruct, cfg->u_nvic->dma_intrMsg->irq_dma_rx,
                                    cfg->u_nvic->dma_intrMsg->nvic_rxch_preprio, cfg->u_nvic->dma_intrMsg->nvic_rxch_subprio);
                    }
            }
            break;
        default:
            break;
    }
    /* NVIC初始化 */
    NVIC_Init(&nvic_initStruct);
}

void usart_concernAll_config(usart_cfg_t *cfg)
{
    get_usart_message(cfg);
    usart_clock_cfg(cfg);
    usart_gpio_cfg(cfg);
    usart_paras_cfg(cfg);
    if(cfg->useDMA_INT & 0x01)
        usart_dma_cfg(cfg);
    if(cfg->useDMA_INT & 0x02)
        usart_nvic_cfg(cfg);
    if(cfg->useDMA_INT & 0x02 && cfg->u_nvic->intrUse >= 1) {
        if(cfg->u_nvic->dma_intrMsg->dma_int_ch != 0) {
            USART_DMACmd(cfg->instance, USART_DMAReq_Tx, ENABLE);
            if(cfg->u_nvic->dma_intrMsg->dma_int_ch == 1) {
                DMA_Cmd(cfg->u_dma->rx_ch, ENABLE);
                USART_DMACmd(cfg->instance, USART_DMAReq_Rx, ENABLE);
            }
        } else {
            DMA_Cmd(cfg->u_dma->rx_ch, ENABLE);
            USART_DMACmd(cfg->instance, USART_DMAReq_Rx, ENABLE);
        }
    }
    USART_Cmd(cfg->instance, ENABLE);
}

/**
 * @brief 获取串口外设对应的 RCC 时钟使能位
 * @param UARTx  串口寄存器基址指针（USART1、USART2、USART3、UART4、UART5）
 * @retval RCC 时钟使能位（APB1 或 APB2），若无效则返回 0
 */
static inline u32 get_uart_rcc(USART_TypeDef* UARTx)
{
    switch ((uint32_t)UARTx) {
        case (uint32_t)USART1: return RCC_APB2Periph_USART1;
        case (uint32_t)USART2: return RCC_APB1Periph_USART2;
        case (uint32_t)USART3: return RCC_APB1Periph_USART3;
        case (uint32_t)UART4:  return RCC_APB1Periph_UART4;
        case (uint32_t)UART5:  return RCC_APB1Periph_UART5;
        default: return 0;
    }
}
