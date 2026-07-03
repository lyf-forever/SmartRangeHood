#include "tim.h"
#include "led.h"

/* 内部辅助函数声明 */
static void TIM_GPIO_Config(TIM_Config_t *cfg);
static void TIM_NVIC_Config(TIM_Config_t *cfg);

/**
 * @brief 获取定时器对应的 RCC 时钟使能位
 * @param TIMx  定时器寄存器基址指针（如 TIM1、TIM2...）
 * @retval RCC 时钟使能位（APB1 或 APB2），若无效则返回 0
 */
static inline u32 get_tim_rcc(TIM_TypeDef* TIMx)
{
    switch ((uint32_t)TIMx) {
        case (uint32_t)TIM1: return RCC_APB2Periph_TIM1;
        case (uint32_t)TIM8: return RCC_APB2Periph_TIM8;
        case (uint32_t)TIM2: return RCC_APB1Periph_TIM2;
        case (uint32_t)TIM3: return RCC_APB1Periph_TIM3;
        case (uint32_t)TIM4: return RCC_APB1Periph_TIM4;
        case (uint32_t)TIM5: return RCC_APB1Periph_TIM5;
        case (uint32_t)TIM6: return RCC_APB1Periph_TIM6;
        case (uint32_t)TIM7: return RCC_APB1Periph_TIM7;
        default: return 0;
    }
}

/*------------------------------------------------
 * 通用定时器初始化
 *----------------------------------------------*/
void TIM_GeneralInit(TIM_Config_t *cfg)
{
    /* 定时器时钟使能 */
    if(cfg->TIMx == TIM1)   RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
    else if(cfg->TIMx == TIM2)   RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    else if(cfg->TIMx == TIM3)   RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    else if(cfg->TIMx == TIM4)   RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
    else if(cfg->TIMx == TIM5)   RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);
    else if(cfg->TIMx == TIM6)   RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE);
    else if(cfg->TIMx == TIM7)   RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM7, ENABLE);
    else  RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8, ENABLE);

    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure = {0};
    TIM_OCInitTypeDef        TIM_OCInitStructure = {0};
    TIM_ICInitTypeDef        TIM_ICInitStructure = {0};
    
    /* 1. 配置 GPIO 和时钟（输出时需要 GPIO） */
    if (cfg->Mode != TIM_MODE_BASIC && cfg->Mode != TIM_MODE_ENCODER) {
        TIM_GPIO_Config(cfg);
    }
    
    /* 2. 时基配置 */
    TIM_TimeBaseStructure.TIM_Prescaler = cfg->Prescaler;
    TIM_TimeBaseStructure.TIM_Period = cfg->Period;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; /* 默认向上计数 */
    if(cfg->TIMx == TIM1 || cfg->TIMx == TIM8) 
        TIM_TimeBaseStructure.TIM_RepetitionCounter = cfg->RepetionCnt;

    TIM_TimeBaseInit(cfg->TIMx, &TIM_TimeBaseStructure);
    
    /* 3. 根据模式配置 */
    switch (cfg->Mode)
    {
        case TIM_MODE_BASIC:
            /* 仅时基，打开更新中断 */
            TIM_ITConfig(cfg->TIMx, TIM_IT_Update, ENABLE);
            break;
            
        case TIM_MODE_OC_TOGGLE:
        case TIM_MODE_PWM1:
        case TIM_MODE_PWM2:
        {
            uint16_t oc_mode;
            if (cfg->Mode == TIM_MODE_OC_TOGGLE)
                oc_mode = TIM_OCMode_Toggle;
            else if (cfg->Mode == TIM_MODE_PWM1)
                oc_mode = TIM_OCMode_PWM1;
            else
                oc_mode = TIM_OCMode_PWM2;
            
            TIM_OCInitStructure.TIM_OCMode = oc_mode;
            TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
            TIM_OCInitStructure.TIM_Pulse = cfg->Pulse;
            TIM_OCInitStructure.TIM_OCPolarity = (cfg->OCPolarity == TIM_OC_POLARITY_HIGH) ? 
                                                  TIM_OCPolarity_High : TIM_OCPolarity_Low;
            if(cfg->TIMx == TIM1 || cfg->TIMx == TIM8)
                TIM_OCInitStructure.TIM_OCIdleState = cfg->OCIdleState;

            switch (cfg->Channel) {
                case 1: TIM_OC1Init(cfg->TIMx, &TIM_OCInitStructure);
                        TIM_OC1PreloadConfig(cfg->TIMx, TIM_OCPreload_Enable); break;
                case 2: TIM_OC2Init(cfg->TIMx, &TIM_OCInitStructure);
                        TIM_OC2PreloadConfig(cfg->TIMx, TIM_OCPreload_Enable); break;
                case 3: TIM_OC3Init(cfg->TIMx, &TIM_OCInitStructure);
                        TIM_OC3PreloadConfig(cfg->TIMx, TIM_OCPreload_Enable); break;
                case 4: TIM_OC4Init(cfg->TIMx, &TIM_OCInitStructure);
                        TIM_OC4PreloadConfig(cfg->TIMx, TIM_OCPreload_Enable); break;
                default: break;
            }
            /* 使能更新中断（若需动态修改占空比且使用预装载）*/
            TIM_ITConfig(cfg->TIMx, TIM_IT_Update, ENABLE);
            break;
        }
        
        case TIM_MODE_INPUT_CAPTURE:
        {
            TIM_ICInitStructure.TIM_Channel = (cfg->IC_Channel == 1) ? TIM_Channel_1 : (cfg->IC_Channel == 2) ? TIM_Channel_2 : 
                                                (cfg->IC_Channel == 3) ? TIM_Channel_3 : TIM_Channel_4;
            TIM_ICInitStructure.TIM_ICPolarity = (cfg->IC_Polarity == TIM_IC_POLARITY_RISING) ? 
                                                  TIM_ICPolarity_Rising :
                                                  (cfg->IC_Polarity == TIM_IC_POLARITY_FALLING) ?
                                                  TIM_ICPolarity_Falling : TIM_ICPolarity_BothEdge;
            TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
            TIM_ICInitStructure.TIM_ICPrescaler = cfg->IC_Prescaler;
            TIM_ICInitStructure.TIM_ICFilter = cfg->IC_Filter;
            TIM_ICInit(cfg->TIMx, &TIM_ICInitStructure);
            
            /* 使能捕获中断 */
            if (cfg->IC_Channel == 1) TIM_ITConfig(cfg->TIMx, TIM_IT_CC1, ENABLE);
            else if (cfg->IC_Channel == 2) TIM_ITConfig(cfg->TIMx, TIM_IT_CC2, ENABLE);
            else if (cfg->IC_Channel == 3) TIM_ITConfig(cfg->TIMx, TIM_IT_CC3, ENABLE);
            else if (cfg->IC_Channel == 4) TIM_ITConfig(cfg->TIMx, TIM_IT_CC4, ENABLE);

            break;
        }
        
        case TIM_MODE_ENCODER:
        {
            TIM_EncoderInterfaceConfig(cfg->TIMx, cfg->Encoder_Mode,
                                       TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);
            /* 编码器模式一般不需要中断，用户可自行开启更新中断 */
            break;
        }
        
        default: break;
    }
    
    /* 4. 配置 NVIC（基础模式和 OC/PWM 可共用更新中断） */
    if (cfg->Mode == TIM_MODE_BASIC || cfg->Mode == TIM_MODE_OC_TOGGLE ||
        cfg->Mode == TIM_MODE_PWM1 || cfg->Mode == TIM_MODE_PWM2 ||
        cfg->Mode == TIM_MODE_INPUT_CAPTURE)
    {
        TIM_NVIC_Config(cfg);
    }
    
    /* 5. 启动定时器 */
    TIM_Cmd(cfg->TIMx, ENABLE);
}

/*------------------------------------------------
 * 动态修改 PWM 占空比
 *----------------------------------------------*/
void TIM_SetPWM_Duty(TIM_TypeDef* TIMx, uint8_t channel, uint16_t duty)
{
    switch (channel) {
        case 1: TIM_SetCompare1(TIMx, duty); break;
        case 2: TIM_SetCompare2(TIMx, duty); break;
        case 3: TIM_SetCompare3(TIMx, duty); break;
        case 4: TIM_SetCompare4(TIMx, duty); break;
        default: break;
    }
}

/*------------------------------------------------
 * 启动/停止定时器
 *----------------------------------------------*/
void TIM_Start(TIM_TypeDef* TIMx)
{
    TIM_Cmd(TIMx, ENABLE);
}

void TIM_Stop(TIM_TypeDef* TIMx)
{
    TIM_Cmd(TIMx, DISABLE);
}

/*------------------------------------------------
 * 动态修改输入捕获极性（用于交替捕获）
 *----------------------------------------------*/
void TIM_IC_SetPolarity(TIM_Config_t *cfg, TIM_IC_Polarity_t polarity)
{
    uint16_t ic_polarity;
    if (polarity == TIM_IC_POLARITY_RISING)
        ic_polarity = TIM_ICPolarity_Rising;
    else if (polarity == TIM_IC_POLARITY_FALLING)
        ic_polarity = TIM_ICPolarity_Falling;
    else
        ic_polarity = TIM_ICPolarity_BothEdge;
    
    TIM_ICInitTypeDef ic = {
        .TIM_Channel = (cfg->IC_Channel == 1) ? TIM_Channel_1 : (cfg->IC_Channel == 2) ? TIM_Channel_2 : 
                                                (cfg->IC_Channel == 3) ? TIM_Channel_3 : TIM_Channel_4,
        .TIM_ICSelection = TIM_ICSelection_DirectTI,
        .TIM_ICFilter = cfg->IC_Filter,
        .TIM_ICPrescaler = cfg->IC_Prescaler,
        .TIM_ICPolarity = ic_polarity,
    };

    TIM_ICInit(cfg->TIMx, &ic);
}

/*------------------------------------------------
 * GPIO 获取（根据定时器和通道）
 *----------------------------------------------*/
static void TIM_GPIO_GetMsg(TIM_Config_t *cfg) {
    if(cfg->TIMx == TIM1) {
        cfg->gpio.port = GPIOA;
        if(cfg->Channel == 1)        cfg->gpio.pin = GPIO_Pin_8;
        else if(cfg->Channel == 2)   cfg->gpio.pin = GPIO_Pin_9;
        else if(cfg->Channel == 3)   cfg->gpio.pin = GPIO_Pin_10;
        else if(cfg->Channel == 4)   cfg->gpio.pin = GPIO_Pin_11;
    } else if(cfg->TIMx == TIM2) {
        cfg->gpio.port = GPIOA;
        if(cfg->Channel == 1)        cfg->gpio.pin = GPIO_Pin_0;
        else if(cfg->Channel == 2)   cfg->gpio.pin = GPIO_Pin_1;
        else if(cfg->Channel == 3)   cfg->gpio.pin = GPIO_Pin_2;
        else if(cfg->Channel == 4)   cfg->gpio.pin = GPIO_Pin_3;
    } else if(cfg->TIMx == TIM3) {
        cfg->gpio.port = GPIOA;
        if(cfg->Channel == 1)        cfg->gpio.pin = GPIO_Pin_6;
        else if(cfg->Channel == 2)   cfg->gpio.pin = GPIO_Pin_7;
        else if(cfg->Channel == 3) { cfg->gpio.port = GPIOB; cfg->gpio.pin = GPIO_Pin_0; } 
        else if(cfg->Channel == 4) { cfg->gpio.port = GPIOB; cfg->gpio.pin = GPIO_Pin_1; }  
    } else if(cfg->TIMx == TIM4) {
        cfg->gpio.port = GPIOB;
        if(cfg->Channel == 1)        cfg->gpio.pin = GPIO_Pin_6;
        else if(cfg->Channel == 2)   cfg->gpio.pin = GPIO_Pin_7;  /* 与LCD BL引脚冲突 */
        else if(cfg->Channel == 3)   cfg->gpio.pin = GPIO_Pin_8;  /* 与Beep蜂鸣器引脚冲突 */
        else if(cfg->Channel == 4)   cfg->gpio.pin = GPIO_Pin_9;  
    } else if(cfg->TIMx == TIM5) {
        cfg->gpio.port = GPIOA;
        if(cfg->Channel == 1)        cfg->gpio.pin = GPIO_Pin_0;
        else if(cfg->Channel == 2)   cfg->gpio.pin = GPIO_Pin_1;
        else if(cfg->Channel == 3)   cfg->gpio.pin = GPIO_Pin_2;
        else if(cfg->Channel == 4)   cfg->gpio.pin = GPIO_Pin_3;
    } else if(cfg->TIMx == TIM8) {
        cfg->gpio.port = GPIOC;
        if(cfg->Channel == 1)        cfg->gpio.pin = GPIO_Pin_6;
        else if(cfg->Channel == 2)   cfg->gpio.pin = GPIO_Pin_7;
        else if(cfg->Channel == 3)   cfg->gpio.pin = GPIO_Pin_8;
        else if(cfg->Channel == 4)   cfg->gpio.pin = GPIO_Pin_9;
    } 
}

/*------------------------------------------------
 * GPIO 配置（根据定时器和通道复用）
 *----------------------------------------------*/
static void TIM_GPIO_Config(TIM_Config_t *cfg)
{
    TIM_GPIO_GetMsg(cfg);
    if(cfg->gpio.port == GPIOA)      
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    else if(cfg->gpio.port == GPIOB) 
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    else if(cfg->gpio.port == GPIOC)
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    else return;

    GPIO_InitTypeDef GPIO_InitStructure = {0};
    
    GPIO_InitStructure.GPIO_Mode = (cfg->Mode == TIM_MODE_INPUT_CAPTURE) ? 
                                       GPIO_Mode_IN_FLOATING : GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin = cfg->gpio.pin;
    
    GPIO_Init(cfg->gpio.port, &GPIO_InitStructure); 
}

/*------------------------------------------------
 * NVIC 配置
 *----------------------------------------------*/
static void TIM_NVIC_Config(TIM_Config_t *cfg)
{
    NVIC_InitTypeDef NVIC_InitStructure = {0};
    uint8_t irq_channel;
    
    if (cfg->TIMx == TIM1) irq_channel = TIM1_UP_IRQn;
    else if (cfg->TIMx == TIM2) irq_channel = TIM2_IRQn;
    else if (cfg->TIMx == TIM3) irq_channel = TIM3_IRQn;
    else if (cfg->TIMx == TIM4) irq_channel = TIM4_IRQn;
    else if (cfg->TIMx == TIM5) irq_channel = TIM5_IRQn;
    else if (cfg->TIMx == TIM6) irq_channel = TIM6_IRQn;
    else if (cfg->TIMx == TIM7) irq_channel = TIM7_IRQn;
    else if (cfg->TIMx == TIM8) irq_channel = TIM8_UP_IRQn;
    else return;
    
    NVIC_InitStructure.NVIC_IRQChannel = irq_channel;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = cfg->PrePrio;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = cfg->SubPrio;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

/*------------------------------------------------
 * 回调函数，用户自定义
 *----------------------------------------------*/
void Timer_IRQ_Callback(TIM_TypeDef* TIMx)
{
    static uint16_t led_cnt = 0;  // 用于TIM4中断次数记录，当达到500次，周期1ms * 500 = 500ms时翻转LED
    /* 用户在此处理中断事件 */
    if(TIMx == TIM4) {
        /* 每1ms进入1次，500次为500ms */
        led_cnt ++;
        if (led_cnt >= 500) {
            led_cnt = 0;
            /* 翻转 LED */
            LED_toggle(&Led0);
        }
    }
}

