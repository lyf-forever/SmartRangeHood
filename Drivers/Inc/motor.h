#ifndef __MOTOR_H_
#define __MOTOR_H_

#include "sys.h"

/***********************
项目名：电机驱动头文件
作者：Lyf
闲鱼号：tb43915564
修改日期：2026/2/1
项目已申请版权，请勿倒卖！
************************/

#if MOTOR_IS_USE  /* 使用电机 */
#if (MOTOR_TYPE == 0) /* 无刷直流电机BLDC */
#define TIM_ENCODER               TIM2
#define TIM_ENCODER_IRQn          TIM2_IRQn
#define TIM_ENCODER_A_CH          TIM_Channel_1
#define TIM_ENCODER_B_CH          TIM_Channel_2
#define RCC_TIM_ENCODER           RCC_APB1Periph_TIM2
#define TIM_ENCODER_IRQHandler    TIM2_IRQHandler

#define TIM_ENCODER_CHA_PORT      GPIOA   
#define TIM_ENCODER_CHA_PIN       GPIO_Pin_0    
#define TIM_ENCODER_CHB_PORT      GPIOA   
#define TIM_ENCODER_CHB_PIN       GPIO_Pin_1

#define MOTOR_CTRL_PORT           GPIOA
#define MOTOR_CTRL_PIN            GPIO_Pin_2

#define SpeedCacl_TIM             TIM4
#define SpeedCacl_TIM_IRQHandler  TIM4_IRQHandler
#define SpeedCacl_TIM_Freq        1000               /* 电机转速计算定时器频率 /Hz */ 
#define SpeedCacl_INTERVAL        50                 /* 电机转速计算时间间隔   /ms */

#define MOTOR_DRIVE_TIM           TIM1
#define RCC_MOTOR_DRIVE_TIM       RCC_APB2Periph_TIM1

#define MOTOR_DRIVE_TIM_CH_PORT   GPIOA
#define MOTOR_DRIVE_TIM_CH_PIN    GPIO_Pin_8
#define MOTOR_DRIVE_TIM_CHN_PORT  GPIOB
#define MOTOR_DRIVE_TIM_CHN_PIN   GPIO_Pin_13
#define MOTOR_DRIVE_TIM_CH        TIM_Channel_1

/*扩展作用域*/
extern volatile float   speed;                /* 电机实际转速 */
extern volatile int32_t overflow;           /* 编码器溢出计数器 */

/* 电机转动方向枚举 */
typedef enum {
    straight = 0,
    invert
} motor_dir_t;

/* 函数声明 */
void TIM_motorDrive_Init(u16 arr, u16 psc, u16 ccr, u16 dtg);   /* 带有死区控制的TIM初始化 */
void motor_stop(void);                                          /* 电机停止 */
void motor_start(void);                                         /* 电机启动 */
void motor_dir_ctrl(motor_dir_t dir);                           /* 电机转向控制 */
void motor_init(void);                                          /* 电机状态初始化 */
void motor_speedCtrl(uint16_t ccr);                             /* 电机转速调节 */
void motor_setPWMDuty(float para);                              /* 电机控制 */
void TIM_motorEncoder_Init(uint16_t arr, uint16_t psc);         /*  编码器定时器TIM_ENCODER功能初始化 */
void TIM_motorSpeedCalc_Init(void);                             /* 定时器SpeedCacl_TIM初始化，用于计算转速 */

//int32_t get_encoder_value(void);                                /* 获取编码器计数值 */
int get_encoder_value(void);                                    /* 获取编码器计数值 */
uint8_t TIM_GetDirection(TIM_TypeDef* TIMx);                    /* 获取计数方向 */
//float motor_getSpeed(int32_t encoder_value, uint16_t ms);       /* 获取电机转速（RPM）*/
float motor_getSpeed(int encoder_value, uint16_t ms);           /* 获取电机转速（RPM）*/

#elif (MOTOR_TYPE == 1) /* 有刷直流电机BDC */

#endif /* #if (MOTOR_TYPE == 0) */

#endif /* #if MOTOR_IS_USE */

#endif 

