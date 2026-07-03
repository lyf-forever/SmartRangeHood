#ifndef __MAIN_H
#define __MAIN_H
/*
 * 油烟机控制系统主程序头文件
 * 基于STM32F103VET6 + FreeRTOS
 * 
 * 功能说明：
 * 1. 待机模式：风机停止，持续计算风速
 * 2. 手动模式：两档调速（LOW/HIGH），PID控制
 * 3. 自动模式：根据传感器自动调节风速
 * 4. 防回流模式：气体浓度超阈值时启动风机
 * 5. 固件更新：通过usart+dma接收固件，接收完成后跳转到app区，boot+app双区架构
 * 6. UI显示功能
 * 按键功能：
 * - 按键1(PE4)：短按切换模式
 * - 按键2(PE3)：短按切换档位，长按开关风机
 * 作者：Lyf
 * 修改日期：2026/5/7
 * 项目已申请版权，请勿倒卖！
 */
#include "sys.h"
#include "gpio.h"
#include "delay.h"
#include "led.h"
#include "key_adp.h"
#include "usart_driver.h"
#include "lcd.h"
#include "GUI.h"
#include "touch.h"
#include "buzzer.h"
#include "motor.h"
#include "windspeed.h"
#include "pid_service.h"
#include "sensor_service.h"
#include "aht20.h"
#include "aht20_adp.h"
#include "mq2.h"
#include "mq2_adp.h"

/* 外部变量声明 */
extern PID_TypeDef s_speedPID;

/* 函数API声明 */
void StartTask_Create(void);

#endif
