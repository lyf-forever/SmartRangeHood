#ifndef __TIM_H_
#define __TIM_H_

#include "sys.h"

/* 定时器模式 */
typedef enum {
    TIM_MODE_BASIC = 0,          /* 基本定时（时基中断） */
    TIM_MODE_OC_TOGGLE,          /* 输出比较 - 翻转 */
    TIM_MODE_PWM1,               /* PWM 模式1 */
    TIM_MODE_PWM2,               /* PWM 模式2 */
    TIM_MODE_INPUT_CAPTURE,      /* 输入捕获 */
    TIM_MODE_ENCODER             /* 编码器模式 */
} TIM_RangehoodWorkMode;

/* 输入捕获极性 */
typedef enum {
    TIM_IC_POLARITY_RISING  = 1,
    TIM_IC_POLARITY_FALLING = 2,
    TIM_IC_POLARITY_BOTH    = 3
} TIM_IC_Polarity_t;

/* 输出比较极性 */
typedef enum {
    TIM_OC_POLARITY_HIGH = 1,
    TIM_OC_POLARITY_LOW  = 2
} TIM_OC_Polarity_t;

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} TIM_GPIO_Msg_t;

/* 通用定时器配置结构体 */
typedef struct {
    TIM_TypeDef*          TIMx;               /* 定时器外设，如 TIM2、TIM3 */
    TIM_RangehoodWorkMode Mode;               /* 工作模式 */
    
    TIM_GPIO_Msg_t        gpio;

    /* 时基参数 */
    uint16_t              Prescaler;          /* 预分频值，0~65535 */
    uint16_t              Period;             /* 自动重装值，0~65535 */
    uint8_t               RepetionCnt;        /* 高级定时器重复值，0-255 */
    
    /* 输出通道（OC/PWM 模式有效） */
    uint8_t               Channel;            /* 通道号 1/2/3/4 */
    TIM_OC_Polarity_t     OCPolarity;         /* 输出极性 */
    uint16_t              Pulse;              /* 输出比较值 / 占空比（若为 PWM，可后续修改） */
    uint16_t              OCIdleState;        /* 输出比较空闲状态，仅TIM1 TIM8 */
    
    /* 输入捕获参数（仅输入捕获模式有效） */
    uint8_t               IC_Channel;         /* 捕获通道 1/2/3/4 */
    TIM_IC_Polarity_t     IC_Polarity;        /* 捕获极性 */
    uint8_t               IC_Filter;          /* 输入滤波器值 0~15 */
    uint8_t               IC_Prescaler;       /* 输入分频 1/2/4/8 对应 0~3 */

    /* 编码器模式参数 */
    uint16_t              Encoder_Mode;       /* 编码器模式：TIM_EncoderMode_TI1/TI2/TI12 */

    /* 中断优先级 */
    uint8_t               PrePrio;            /* 中断抢占优先级 */
    uint8_t               SubPrio;            /* 中断响应优先级 */
} TIM_Config_t;

/* 功能函数 */
void TIM_GeneralInit(TIM_Config_t *cfg);
void TIM_SetPWM_Duty(TIM_TypeDef* TIMx, uint8_t channel, uint16_t duty);
void TIM_Start(TIM_TypeDef* TIMx);
void TIM_Stop(TIM_TypeDef* TIMx);
void TIM_IC_SetPolarity(TIM_Config_t *cfg, TIM_IC_Polarity_t polarity);
/* 中断回调（用户实现） */
void Timer_IRQ_Callback(TIM_TypeDef* TIMx);

// u32 get_tim_rcc(TIM_TypeDef* TIMx); 

#endif



