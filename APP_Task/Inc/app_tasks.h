/*
 * 应用层任务头文件
 * 基于FreeRTOS的油烟机控制系统
 *
 * 作者：不甘心的咸鱼--闲鱼/不搭(414192836)--小红书
 * 闲鱼号：tb43915564
 * 修改日期：2026/2/1
 * 项目已申请版权，请勿倒卖！
 */
#ifndef __APP_TASKS_H
#define __APP_TASKS_H

#include "sys.h"

#if SYSTEM_SUPPORT_OS /* 使用OS */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "flash.h"
#include "pid_service.h"

extern PID_TypeDef s_speedPID;

/*-----------------------------------------------------------
 * 工作模式定义
 *----------------------------------------------------------*/
typedef enum {
    RANGEHOOD_MODE_STANDBY = 0,       /* 待机模式 */
    RANGEHOOD_MODE_MANUAL,            /* 手动模式 */
    RANGEHOOD_MODE_AUTO,              /* 自动模式 */
    RANGEHOOD_MODE_ANTI_BACKFLOW      /* 防回流模式 */
} RangehoodWorkMode;

/*-----------------------------------------------------------
 * 手动模式档位定义
 *----------------------------------------------------------*/
typedef enum {
    MOTOR_SPEED_LOW = 0,          /* 低档 */
    MOTOR_SPEED_HIGH              /* 高档 */
} MotorSpeedLevel;

/*-----------------------------------------------------------
 * 系统状态结构体
 *----------------------------------------------------------*/
typedef struct {
    RangehoodWorkMode currentMode;             /* 当前工作模式 */
    MotorSpeedLevel   speedLevel;              /* 当前档位 */
    u8                motorRunning;            /* 电机运行状态,0表示停止，1表示运行 */
    
    float             temperature;             /* 温度值 */
    float             humidity;                /* 湿度值 */
    float             gasConcentration;        /* 气体浓度值 */
    
    float             windSpeedPWM;            /* 风速PWM值(%) */
    float             actualRPM;               /* 实际转速 */
    u16               targetRPM;               /* 目标转速 */
    
    u8                cookingEventActive;      /* Cooking Event激活标志 */
    
    u8                antiBackflowActive;      /* 防回流激活标志 */
    float             gasThreshold;            /* 当前气体浓度阈值 */
    
    u32               autoModeStartupCounter;  /* 自动模式启动阶段计时器(ms) */
    u32               cookingEventCounter;     /* Cooking Event计时器(ms) */
} RangehoodSystemState;

/*-----------------------------------------------------------
 * 时序参数定义(ms)
 *----------------------------------------------------------*/
#define AUTO_MODE_STARTUP_TIME      60000   /* 自动模式启动等待时间：60秒 */
#define COOKING_EVENT_TIMEOUT       60000   /* Cooking Event超时时间：60秒 */
#define COOKING_EVENT_DELAY_OFF     10000   /* Cooking Event结束后延时关闭：10秒 */


// /*-----------------------------------------------------------
//  * 任务优先级定义
//  *----------------------------------------------------------*/
#define TASK_START_PRIORITY             1       /* 开始任务优先级 */
#define TASK_LCD_PRIORITY               1       /* LCD任务优先级 */
#define TASK_KEY_PRIORITY               4       /* 按键扫描任务优先级 */
#define TASK_SENSOR_PRIORITY            3       /* 传感器采集任务优先级 */
#define TASK_WIND_SPEED_PRIORITY        3       /* 风速计算任务优先级 */
#define TASK_MOTOR_PRIORITY             5       /* 电机控制任务优先级 */
#if PID_DEBUG
#define TASK_PID_DEBUG_PRIORITY         3       /* 电机转速PID调试任务优先级 */
#define TASK_VOFA_PARAPRINT_PRIO        2       /* 电机参数打印任务优先级 */
#endif
#define TASK_ANTI_BACKFLOW_PRIORITY     2       /* 防回流任务优先级 */  
#define TASK_SPEED_CALC_PRIORITY        6       /* 电机转速计算任务优先级（最高，保证及时响应定时器中断） */

#if PID_DEBUG

#define PID_QUEUE_LEN                   16
#define PID_DBG_MAX_TARGET              3       /* pid调试最大参数量，这里代表Kp Ki Kd */
#endif 

#if HARDWARE_UPDATE_OPEN /* 固件升级 */

#if !HW_UPDATE_METHOD /* 有线IAP */

#define TASK_IAP_PRIORITY               7       /* IAP任务优先级 */    
#define TASK_IAP_STK_SIZE               256     /* IAP任务栈大小 */

/* IAP 串口接收帧数据 */
#define FRAME_QUEUE_LEN                 16
#define MAX_FRAME_LENGTH                512

/* 帧结构体 */
typedef struct {
    uint32_t start_idx;     // 帧在环形缓冲区中的起始索引
    uint32_t length;        // 帧长度（字节）
} frame_info_t;

typedef enum { 
    STAGE_GET_LENGTH = 0,   
    STAGE_RECV_DATA, 
    STAGE_CHECK 
} IAP_Stage;

#else /* 无线OTA */

#endif

#endif

// /*-----------------------------------------------------------
//  * 任务栈大小定义
//  *----------------------------------------------------------*/
#define TASK_START_STK_SIZE             64       /* 开始任务栈大小 */
#define TASK_LCD_STK_SIZE               256      /* LCD显示任务栈大小 */
#define TASK_KEY_STK_SIZE               64       /* 按键扫描任务栈大小 */
#define TASK_SENSOR_STK_SIZE            256      /* 传感器采集任务栈大小 */
#define TASK_WIND_SPEED_STK_SIZE        64       /* 风速计算任务栈大小 */
#define TASK_MOTOR_STK_SIZE             256      /* 电机控制任务栈大小 */
#if PID_DEBUG
#define TASK_PID_DEBUG_STK_SIZE         512      /* 电机转速PID调试任务栈大小 */
#define TASK_VOFA_PARAPRINT_STK_SIZE    512      /* 电机参数打印任务栈大小 */
#endif
#define TASK_ANTI_BACKFLOW_STK_SIZE     64       /* 防回流任务栈大小 */
#define TASK_SPEED_CALC_STK_SIZE        128      /* 速度计算任务栈大小 */

/*-----------------------------------------------------------
 * 函数声明
 *----------------------------------------------------------*/
/* 系统初始化 */
void System_Init(void);

/* 创建开始任务 */
void StartTask_Create(void);

/* 任务函数声明 */
void StartTask(void *pvParameters);                /* 开始任务 */
void LCDDispUITask(void *pvParameters);            /* LCD UI显示任务 */
void KeyScanTask(void *pvParameters);              /* 按键扫描任务 */
void SensorTask(void *pvParameters);               /* 传感器采集任务 */
void WindSpeedTask(void *pvParameters);            /* 风速计算任务 */
void MotorControlTask(void *pvParameters);         /* 电机控制任务 */
#if PID_DEBUG
void PIDDebugTask(void *pvParameters);             /* 电机转速PID调试任务 */
void VofaParaPrintTask(void *pvParameters);        /* 电机参数打印任务 */
#endif
void AntiBackflowTask(void *pvParameters);         /* 防回流任务 */
void MotorSpeedCalcTask(void *pvParameters);       /* 速度计算任务（由SpeedCacl_TIM中断(周期1ms)触发） */
void iap_hardwareUpdate_task(void *pvParameters);  /* IAP任务 */

// /* 模式切换函数 */
void System_SwitchMode(void);                  /* 切换工作模式 */
void System_SwitchSpeedLevel(void);            /* 切换档位 */
void System_ToggleMotor(void);                 /* 切换电机开关 */

// /* 获取系统状态 */
// RangehoodSystemState* System_GetState(void);

#endif /* #if SYSTEM_SUPPORT_OS */

#endif /* __APP_TASKS_H */
