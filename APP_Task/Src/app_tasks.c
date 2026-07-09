/*
 * 应用层任务源文件
 * 基于FreeRTOS的油烟机控制系统
 * 作者：Lyf
 * 修改日期：2026/3/21
 */
#include "app_tasks.h"

#if SYSTEM_SUPPORT_OS /* 使用OS */

#include "delay.h"
#include "led.h"
#include "aht20_adp.h"
#include "mq2_adp.h"
#include "lcd.h"
#include "GUI.h"
#include "key_adp.h"
#include "buzzer.h"
#include "motor.h"
#include "pid_service.h"
#include "windspeed.h"
#include <string.h>
#include "usart_driver.h"
#include "crc32_service.h"
#include "bootloader.h"

/* 自动模式状态机状态 */
typedef enum {
    AUTO_STATE_STARTUP,             /* 启动阶段（等待cooking event） */
    AUTO_STATE_COOKING,             /* Cooking Event激活 */
    AUTO_STATE_DELAY_OFF            /* 延时关闭阶段 */
} AutoModeState;

static AutoModeState s_autoModeState = AUTO_STATE_STARTUP;

/*-----------------------------------------------------------
 * 模式名称字符串
 *----------------------------------------------------------*/
static const char* ModeNames[] = {
    "Standby",
    "Manual ",
    "Auto   ",
    "Anti-BF"
};

static const char* SpeedLevelNames[] = {
    "LOW ",
    "HIGH"
};

static const char* AutoStateNames[] = {
    "Startup ",
    "Cooking ",
    "DelayOff"
};

/*-----------------------------------------------------------
 * 全局变量定义
 *----------------------------------------------------------*/
RangehoodSystemState s_state;                      /* 系统状态 */
SemaphoreHandle_t s_dataConcernMutex   = NULL;     /* 数据互斥信号量 */
SemaphoreHandle_t s_speedCalcSemaphore = NULL;     /* 速度计算二值信号量 */

/* 任务句柄 */
static TaskHandle_t xStartTaskHandle        = NULL;
static TaskHandle_t xLCDDispUITaskHandle    = NULL;
static TaskHandle_t xKeyScanTaskHandle      = NULL;
static TaskHandle_t xSensorTaskHandle       = NULL;
static TaskHandle_t xWindSpeedTaskHandle    = NULL;
static TaskHandle_t xMotorControlTaskHandle = NULL;
#if PID_DEBUG
static TaskHandle_t xPIDDebugTaskHandle     = NULL;
static QueueHandle_t s_pidDebugQueue        = NULL;  
static TaskHandle_t xVofaPPTaskHandle       = NULL;  
#endif 
static TaskHandle_t xAntiBackflowTaskHandle = NULL;
static TaskHandle_t xSpeedCalcTaskHandle    = NULL;
#if HARDWARE_UPDATE_OPEN
static TaskHandle_t xIAPTaskHandle = NULL;
#endif

#if HARDWARE_UPDATE_OPEN
static QueueHandle_t frame_queue = NULL;   /* 队列句柄 */

extern uint8_t iap_recbuff[IAP_REC_LEN];   /* 接收缓冲区 */
volatile uint32_t rx_rd_idx = 0;    /* 读索引：应用程序已处理到的位置，用于缓冲区溢出判断 */
volatile uint32_t last_wr_idx = 0;  /* 上一次 IDLE 结束时的 DMA 写位置 */

#endif

#if PID_IS_USE
/* PID控制器 */
PID_TypeDef s_speedPID;

typedef struct {
    const MotorSpeedLevel speedLevel;   // 转速挡位
    uc16                  targetRPM;    // 该组参数对应的目标转速
    /* PID参数 */
    const float           kp;        
    const float           ki; 
    const float           kd;
    /* 输出上限&下限 */
    const float           op_max; 
    const float           op_min; 
} PID_GainEntry;

/* PID参数表（按目标转速升序排列）*/
static const PID_GainEntry s_pidGainTable[] = {
    { MOTOR_SPEED_LOW,  SPEED_LOW_RPM,   14.10f, 1.80f, 0.00f, 700.0f, 500.0f },   // 180 RPM 最优参数
    { MOTOR_SPEED_HIGH, SPEED_HIGH_RPM,  20.20f, 2.39f, 0.00f, 1000.0f, 830.0f },   // 220 RPM 最优参数
};
#define PID_GAIN_TABLE_SIZE  (sizeof(s_pidGainTable) / sizeof(s_pidGainTable[0]))

#endif 

/*-----------------------------------------------------------
 * 系统初始化
 *----------------------------------------------------------*/
void System_Init(void)
{
    /* 初始化系统状态 */
    s_state.currentMode = RANGEHOOD_MODE_STANDBY;
    s_state.speedLevel = MOTOR_SPEED_LOW;
    s_state.motorRunning = 0;
    s_state.temperature = 0.0f;
    s_state.humidity = 0.0f;
    s_state.gasConcentration = 0.0f;
    s_state.windSpeedPWM = 0.0f;
    s_state.actualRPM = 0.0f;
    s_state.targetRPM = 0;
    s_state.cookingEventActive = 0;
    s_state.antiBackflowActive = 0;
    s_state.gasThreshold = MQ2_THRESHOLD_NORMAL;
    s_state.autoModeStartupCounter = 0;
    s_state.cookingEventCounter = 0;
     
    /* 创建互斥信号量 */
    s_dataConcernMutex   = xSemaphoreCreateMutex();
#if PID_DEBUG
    /* 创建PID调试二值信号量 */
    s_pidDebugQueue      = xQueueCreate(PID_QUEUE_LEN, sizeof(uint8_t));
#endif
    /* 创建速度计算二值信号量 */
    s_speedCalcSemaphore = xSemaphoreCreateBinary();

#if HARDWARE_UPDATE_OPEN
    /* 创建IAP串口接收数据帧队列 */
    frame_queue = xQueueCreate(FRAME_QUEUE_LEN, sizeof(frame_info_t));
#endif

}

/*-----------------------------------------------------------
 * 创建开始任务
 *----------------------------------------------------------*/
void StartTask_Create(void)
{
    xTaskCreate(StartTask, "StartTask", TASK_START_STK_SIZE, NULL, 
                TASK_START_PRIORITY, &xStartTaskHandle);
}

/*-----------------------------------------------------------
 * 开始任务 - 创建其他任务
 *----------------------------------------------------------*/
void StartTask(void *pvParameters)
{
    taskENTER_CRITICAL();   /* 进入临界区 */
     
    /* 创建按键扫描任务 */
    xTaskCreate(KeyScanTask, "KeyScan", TASK_KEY_STK_SIZE, NULL,
                TASK_KEY_PRIORITY, &xKeyScanTaskHandle);
    
    /* 创建传感器采集任务 */
    xTaskCreate(SensorTask, "Sensor", TASK_SENSOR_STK_SIZE, NULL,
                TASK_SENSOR_PRIORITY, &xSensorTaskHandle);
    
    /* 创建风速计算任务 */
    xTaskCreate(WindSpeedTask, "WindSpeed", TASK_WIND_SPEED_STK_SIZE, NULL,
                TASK_WIND_SPEED_PRIORITY, &xWindSpeedTaskHandle);
    
    /* 创建电机控制任务 */
    xTaskCreate(MotorControlTask, "MotorCtrl", TASK_MOTOR_STK_SIZE, NULL,
                TASK_MOTOR_PRIORITY, &xMotorControlTaskHandle);
    
    /* 创建LCD UI信息显示任务 */
    xTaskCreate(LCDDispUITask, "LCDDispUI", TASK_LCD_STK_SIZE, NULL,
                TASK_LCD_PRIORITY, &xLCDDispUITaskHandle);
#if PID_DEBUG
    /* 创建PID电机转速调试任务 */
    xTaskCreate(PIDDebugTask, "PIDDebug", TASK_PID_DEBUG_STK_SIZE, NULL,
                TASK_PID_DEBUG_PRIORITY, &xPIDDebugTaskHandle);
                
    /* 创建电机参数打印任务（用于PID调参）*/
    xTaskCreate(VofaParaPrintTask, "VofaSpeedPrint", TASK_VOFA_PARAPRINT_STK_SIZE, NULL,
                TASK_VOFA_PARAPRINT_PRIO, &xVofaPPTaskHandle);                      
#endif    
    /* 创建防回流任务 */
    xTaskCreate(AntiBackflowTask, "AntiBackFlow", TASK_ANTI_BACKFLOW_STK_SIZE, NULL,
                TASK_ANTI_BACKFLOW_PRIORITY, &xAntiBackflowTaskHandle);
    
    /* 创建速度计算任务 */
    xTaskCreate(MotorSpeedCalcTask, "MotorSpeedCalc", TASK_SPEED_CALC_STK_SIZE, NULL,
                TASK_SPEED_CALC_PRIORITY, &xSpeedCalcTaskHandle);

#if HARDWARE_UPDATE_OPEN      
    /* 创建IAP任务 */
    xTaskCreate(iap_hardwareUpdate_task, "IAP_HardWareUpdate", TASK_IAP_STK_SIZE, NULL, 
                TASK_IAP_PRIORITY, &xIAPTaskHandle);
#endif                
    
    /* 初始化SpeedCacl_TIM定时器（必须在信号量创建后调用，避免HardFault）
     * 参数：5-1=4, 14400-1=14399
     * 定时器频率 = 72MHz / 14400 = 5kHz
     * 中断周期 = 5 / 5kHz = 1ms */
    TIM_motorSpeedCalc_Init();

    taskEXIT_CRITICAL();    /* 退出临界区 */
    
    /* 删除开始任务 */
    vTaskDelete(xStartTaskHandle);
}

/*-----------------------------------------------------------
 * 按键扫描任务 - 周期10ms
 *----------------------------------------------------------*/
void KeyScanTask(void *pvParameters)
{
    while(1) 
    {
        key_scanAll();
        delay_ms(10);
    }
}

/* 按键不同事件的处理接口实现 */
void key_eventCallback(const KeyInfo *info, void *user_data)
{
    if(info == NULL) return;

    /* 先打日志（调试必备） */
    //key_log_event(info);

    /* 利用 user_data 传递上下文（如界面句柄、状态机指针） */
    //KeyAppCtx *ctx = (KeyAppCtx *)user_data;

    switch(info->id) {
        case KEY_DRV_0:  /* userKey0 */
            switch(info->event) {
                case KEY_EVENT_CLICK:
                    System_SwitchMode();            /* 切换系统烟机模式 */ 
                    buzzer.beep(&buzzer, 1, 100);   /* 短按提示音 */
                    break;
                case KEY_EVENT_LONG:
                    
                    break;
                case KEY_EVENT_DOUBLE:
                    
                    break;
                case KEY_EVENT_REPEAT:
                    buzzer.on(&buzzer);    /* 蜂鸣器响 */
                    break;
                case KEY_EVENT_RELEASE:
                    buzzer.off(&buzzer);   /* 蜂鸣器关 */
                    break;
                default:
                    break;
            }
            break;

        case KEY_DRV_1:  /* userKey1 */
            switch(info->event) {
                case KEY_EVENT_CLICK:
                    System_SwitchSpeedLevel();     /* 切换电机转速挡位（分高低）*/    
                    buzzer.beep(&buzzer, 1, 100);  /* 短按提示音 */
                    break;
                case KEY_EVENT_LONG:
                    System_ToggleMotor();          /* 长按时关闭或者打开电机，无论电机在何种模式下 */
                    break;
                case KEY_EVENT_DOUBLE:
                    
                    break;
                case KEY_EVENT_REPEAT:
                    buzzer.on(&buzzer);    /* 蜂鸣器响 */
                    break;
                case KEY_EVENT_RELEASE:
                    buzzer.off(&buzzer);   /* 蜂鸣器关 */
                    break;
                default:
                    break;
            }
            break;

        default:
            break;
    }
}

/*-----------------------------------------------------------
 * 传感器采集任务 - 周期500ms
 *----------------------------------------------------------*/
void SensorTask(void *pvParameters)
{
    sensor_device_t *th = sensor_find_by_type(SENSOR_TYPE_TEMP_HUMIDITY);
    sensor_data_t THdata;

    sensor_device_t *gas = sensor_find_by_type(SENSOR_TYPE_GAS);
    sensor_data_t gasData;

    while (1)
    {
        if(sensor_read(th, &THdata) == SENSOR_OK) 
        {
            if(THdata.th.temperature != 0.0f && THdata.th.humidity != 0.0f) {
                //LOG_D("temperature: %.1f C, humidity: %.1f %%", THdata.th.temperature, THdata.th.humidity);
                if(xSemaphoreTake(s_dataConcernMutex, portMAX_DELAY) == pdTRUE) {
                    s_state.temperature = THdata.th.temperature;
                    s_state.humidity = THdata.th.humidity;
                    xSemaphoreGive(s_dataConcernMutex);
                }
            }  
        }

        if(sensor_read(gas, &gasData) == SENSOR_OK) 
        {
            if(gasData.gas.concentration != 0.0f) {
                //LOG_D("gas concentration: %.2f ppm", gasData.gas.concentration);
                if(xSemaphoreTake(s_dataConcernMutex, portMAX_DELAY) == pdTRUE) {
                    s_state.gasConcentration = gasData.gas.concentration;
                    xSemaphoreGive(s_dataConcernMutex);
                }
            }
        }
        
        delay_ms(500);
    }
}

/*-----------------------------------------------------------
 * 风速计算任务 - 周期100ms
 *----------------------------------------------------------*/
void WindSpeedTask(void *pvParameters)
{
    float gas, temp, humi;
    /*如果此处获取传感器和更新系统状态使用同一把锁，会导致占用资源时间过长，更容易带来优先级反转问题*/
    while (1)
    {
        /* 获取传感器数据 */
        if (xSemaphoreTake(s_dataConcernMutex, portMAX_DELAY) == pdTRUE)
        {
            temp = s_state.temperature;
            humi = s_state.humidity;
            gas  = s_state.gasConcentration;
            xSemaphoreGive(s_dataConcernMutex);
        }
        
        /* 更新风速计算 */
        WindSpeed_Update(temp, humi, gas);
        
        /* 更新系统状态 */
        if (xSemaphoreTake(s_dataConcernMutex, portMAX_DELAY) == pdTRUE)
        {
            s_state.windSpeedPWM = WindSpeed_GetPWM();
            s_state.cookingEventActive = WindSpeed_IsCookingEvent();
            xSemaphoreGive(s_dataConcernMutex);
        }
        
        delay_ms(100);  /* 100ms计算周期 */
    }
}

/*-----------------------------------------------------------
 * 电机控制任务 - 周期25ms
 *----------------------------------------------------------*/
/* 电机运行的要素:必须有三个函数调用(启动、设置转速、设置方向) */
void MotorControlTask(void *pvParameters)
{
    float pidOutput;
   
    while (1)
    {
        uint16_t pwmCompare = 0;

        /* 获取实际转速 */
        if (xSemaphoreTake(s_dataConcernMutex, portMAX_DELAY) == pdTRUE)
        {
            s_state.actualRPM = speed;
            xSemaphoreGive(s_dataConcernMutex);
        }
       
        switch (s_state.currentMode)
        {
            case RANGEHOOD_MODE_STANDBY:
                /* 待机模式：电机停止，但仍计算风速 */
                motor_stop();
                s_state.motorRunning = 0;
                s_state.autoModeStartupCounter = 0;
                s_state.cookingEventCounter = 0;
                s_autoModeState = AUTO_STATE_STARTUP;
                break;
               
            case RANGEHOOD_MODE_MANUAL:
                /* 手动模式：PID控制电机转速 */
                if (!s_state.motorRunning)
                {
                    motor_start();
                    s_state.motorRunning = 1;
                }

                if (s_state.motorRunning)
                {
                    /* 1.设置目标转速;2.计算PID输出;3.设置电机PWM */
                    s_state.targetRPM = WindSpeed_GetTargetRPM(s_state.speedLevel);
                    PID_SetTarget(&s_speedPID, (float)s_state.targetRPM);
                    pidOutput = PID_Calculate(&s_speedPID, speed); 
                    motor_setPWMDuty(pidOutput);
                }
                break;
               
            case RANGEHOOD_MODE_AUTO:
                /* 自动模式状态机 */
                switch (s_autoModeState)
                {
                    case AUTO_STATE_STARTUP:
                        /* 启动阶段：以最小转速运行，等待Cooking Event */
                        if (!s_state.motorRunning)
                        {
                            motor_start();
                            s_state.motorRunning = 1;
                        }

                        /* 使用最小转速代表自动模式开启 */                       
                        motor_setPWMDuty(PWM_MIN*10);

                        s_state.autoModeStartupCounter += 25;
                       
                        if (s_state.cookingEventActive)
                        {
                            /* 检测到Cooking Event */
                            s_autoModeState = AUTO_STATE_COOKING;
                            s_state.autoModeStartupCounter = 0;
                        }
                        else if (s_state.autoModeStartupCounter >= AUTO_MODE_STARTUP_TIME)
                        {
                            /* 60秒内无Cooking Event，关闭自动模式 */
                            s_state.currentMode = RANGEHOOD_MODE_STANDBY;
                            motor_stop();
                            s_state.motorRunning = 0;
                        }
                        break;
                       
                    case AUTO_STATE_COOKING:
                    /* Cooking Event激活：根据传感器自动调节 */
                    {
                        /* 根据风速算法设置速度，MAXCCR为最大占空比对应的CCR值 */
                        pwmCompare = WindSpeed_GetPWMCompare(MAXCCR);
                        motor_setPWMDuty(pwmCompare);
                    }
                        
                        s_state.cookingEventCounter += 25;
                        
                        if (!s_state.cookingEventActive)
                        {
                            /* Cooking Event结束 */
                            s_autoModeState = AUTO_STATE_DELAY_OFF;
                            s_state.cookingEventCounter = 0;
                        }
                        else if (s_state.cookingEventCounter >= COOKING_EVENT_TIMEOUT)
                        {
                            /* Cooking Event持续超过60秒，关闭自动模式 */
                            s_state.currentMode = RANGEHOOD_MODE_STANDBY;
                            motor_stop();
                            s_state.motorRunning = 0;
                        }
                        break;
                       
                    case AUTO_STATE_DELAY_OFF:
                    /* 延时关闭阶段 */
                    {
                        /* 依据风速输出转速，MAXCCR为最大占空比对应的CCR值 */
                        pwmCompare = WindSpeed_GetPWMCompare(MAXCCR);
                        motor_setPWMDuty(pwmCompare);
                    }
                       
                        s_state.cookingEventCounter += 25;
                       
                        if (s_state.cookingEventActive)
                        {
                            /* 又检测到Cooking Event */
                            s_autoModeState = AUTO_STATE_COOKING;
                            s_state.cookingEventCounter = 0;
                        }
                        else if (s_state.cookingEventCounter >= COOKING_EVENT_DELAY_OFF)
                        {
                            /* 10秒延时结束，关闭自动模式 */
                            s_state.currentMode = RANGEHOOD_MODE_STANDBY;
                            motor_stop();
                            s_state.motorRunning = 0;
                        }
                        break;
                }
                break;
               
            case RANGEHOOD_MODE_ANTI_BACKFLOW:
            /* 防回流模式：由防回流任务控制 */
                if (s_state.antiBackflowActive && s_state.motorRunning)
                {
                    /* 使用用户设定的档位转速 */
                    s_state.targetRPM = WindSpeed_GetTargetRPM(s_state.speedLevel);
                    PID_SetTarget(&s_speedPID, (float)s_state.targetRPM);
                    pidOutput = PID_Calculate(&s_speedPID, speed);
                    motor_setPWMDuty(pidOutput);
                }
                break;
        }
        delay_ms(25);   /* 25ms控制周期 */
    }
}

#if PID_DEBUG
/* 接收状态变换枚举 */
typedef enum {
    PID_DBG_STATE_WAIT_FH1 = 0x00,   /* 等待PID传递的命令帧头1 - '#' */
    PID_DBG_STATE_WAIT_FH2,          /* 等待PID传递的命令帧头2 - 'P' */
    PID_DBG_STATE_WAIT_TARGET,       /* 等待PID传递的命令对象 */
    PID_DBG_STATE_WAIT_EQUALSIGN,    /* 等待PID传递的命令等号 - '=' */
    PID_DBG_STATE_RECDATA,           /* 接收传递的命令包含的目标对象数据 */
} pidDbgState;

// 轻量级字符串转浮点（仅处理正负数和两位小数，速度极快）
static float my_atof(const char *str) 
{
    float integer = 0, frac = 0, sign = 1.0f;
    int i = 0, frac_len = 0;
    
    if (str[0] == '-') { sign = -1.0f; i++; }
    
    while (str[i] >= '0' && str[i] <= '9') {
        integer = integer * 10.0f + (str[i] - '0');
        i++;
    }
    
    if (str[i] == '.') {
        i++;
        while (str[i] >= '0' && str[i] <= '9') {
            frac = frac * 10.0f + (str[i] - '0');
            frac_len++;
            i++;
        }
        while (frac_len--) frac /= 10.0f;
    }
    
    return sign * (integer + frac);
}

/*-----------------------------------------------------------
 * 电机转速PID调试任务 - 周期100ms
 *----------------------------------------------------------*/
void PIDDebugTask(void *pvParameters)
{
    uint8_t pid_dbg_target,
            pid_rxData,
            pid_rxFlag,
            data_bitNum,
            pRxPacket = 0;
    pidDbgState pid_dbgState = PID_DBG_STATE_WAIT_FH1;
    uint8_t pid_rxPacket[LOG_RXPACKET_LEN];
    
    float parsedValue = 0.0f;

    while (1)
    {   
        /* 无限阻塞等待一个字节（只有当队列有数据时才会返回） */
        if (xQueueReceive(s_pidDebugQueue, &pid_rxData, portMAX_DELAY) == pdPASS)
        {
            
            /* ----- 状态机处理一个字节 ----- */
            switch (pid_dbgState)
            {
                case PID_DBG_STATE_WAIT_FH1:
                    if (pid_rxData == 0x23)   // 0x23 - '#'
                        pid_dbgState = PID_DBG_STATE_WAIT_FH2;
                    break;

                case PID_DBG_STATE_WAIT_FH2:  
                    pid_dbgState = (pid_rxData == 0x50) ? PID_DBG_STATE_WAIT_TARGET : PID_DBG_STATE_WAIT_FH1;  // 0x50 - 'P'
                    break;

                case PID_DBG_STATE_WAIT_TARGET:
                    pid_dbg_target = pid_rxData - '0';
                    pid_dbgState = (pid_dbg_target > PID_DBG_MAX_TARGET) ? PID_DBG_STATE_WAIT_FH1 : PID_DBG_STATE_WAIT_EQUALSIGN;
                    break;

                case PID_DBG_STATE_WAIT_EQUALSIGN:
                    pid_dbgState = (pid_rxData == 0x3D) ? PID_DBG_STATE_RECDATA : PID_DBG_STATE_WAIT_FH1;   // 0x3D - '='
                    break;

                case PID_DBG_STATE_RECDATA:
                    if (pid_rxData == 0x21)          // '!' - 帧尾
                    {
                        data_bitNum = pRxPacket;
                        pRxPacket = 0;
                        pid_dbgState = PID_DBG_STATE_WAIT_FH1;
                        pid_rxFlag = 1;
                    }
                    else /* 存入有效目标数据 */
                    {
                        if (pRxPacket < LOG_RXPACKET_LEN)
                            pid_rxPacket[pRxPacket++] = pid_rxData;
                        else
                        {
                            // 溢出处理：复位状态机
                            pRxPacket = 0;
                            pid_dbgState = PID_DBG_STATE_WAIT_FH1;
                            pid_rxFlag = 0;
                        }
                    }
                    break;

                default:
                    pid_dbgState = PID_DBG_STATE_WAIT_FH1;
                    pRxPacket = 0;
                    break;
            }

            /* ----- 如果收到完整数据包，解析并更新PID参数 ----- */
            if (pid_rxFlag)
            {
                pid_rxFlag = 0;
                pid_rxPacket[data_bitNum] = '\0';
                parsedValue = my_atof((const char *)pid_rxPacket);
                
                taskENTER_CRITICAL();
                switch (pid_dbg_target)
                {
                    case 1: 
                        s_speedPID.Kp = parsedValue; 
                        break;
                    case 2: 
                        s_speedPID.Ki = parsedValue; 
                        break;
                    case 3: 
                        s_speedPID.Kd = parsedValue; 
                        break;
                    default:
                        break;
                }
                taskEXIT_CRITICAL();
                memset(pid_rxPacket, 0, sizeof(pid_rxPacket));
            }
        }
    }
}

/* PID串口接收中断处理 */
void LOG_USART_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t received_byte;
    
    if(USART_GetITStatus(LOG_USARTx, LOG_USART_IT) != RESET)
    {
        /* 1. 立刻读取 DR，清空硬件标志，防止数据被覆盖 */
        received_byte = USART_ReceiveData(LOG_USARTx);
        /* 2. 将读到的字节发送到 RTOS 队列（队列自带缓冲，不需要额外信号量）*/
        xQueueSendFromISR(s_pidDebugQueue, &received_byte, &xHigherPriorityTaskWoken);
        /* 3. 如果有任务在等待队列，将任务切换使能 */
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/* 电机转速打印任务 - 50ms */
void VofaParaPrintTask(void *pvParameters)
{
    float actualRPM, targetRPM, pid_Kp, pid_Ki, pid_Kd, pid_op = 0.0f;

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xParaPrintPeriod = pdMS_TO_TICKS(50);   // 50ms 发送一次（20Hz）

    while (1)
    {
        /* ----- 步骤1：从全局变量或队列中读取数据（临界区保护） ----- */ 
        taskENTER_CRITICAL();
        actualRPM = s_state.actualRPM;
        pid_op = PID_Calculate(&s_speedPID, actualRPM);
        targetRPM = s_state.targetRPM;
        pid_Kp    = s_speedPID.Kp;
        pid_Ki    = s_speedPID.Ki;
        pid_Kd    = s_speedPID.Kd;
        taskEXIT_CRITICAL();
        /* ----- 步骤2：发送 VOFA+ FireWater 协议字符串 ----- */
        LOG_VOFA("%.1f,%.1f,%.1f,%.2f,%.2f,%.2f", pid_op, actualRPM, targetRPM, pid_Kp, pid_Ki, pid_Kd);
        /* ----- 步骤3：精确的 50ms 周期延时 ----- */
        vTaskDelayUntil(&xLastWakeTime, xParaPrintPeriod);
    }
}

#endif /* #if PID_DEBUG */

/*-----------------------------------------------------------
 * UI显示任务 - 周期200ms
 *----------------------------------------------------------*/
void LCDDispUITask(void *pvParameters)
{
    char dispBuf[50];

    LCD_ClearAll(WHITE);   /* 清屏 */
    
    while (1)
    {
        /* sprintf该函数用于数字转字符串，或者是构建复杂字符串，如最后一个参数值是manual,则dispBuf的结果为Mode:manual */
        /* UI信息显示提示 */
        GUI_ShowString(35, 20,  "System Message(200ms)", RED, WHITE, ASCII_1608, 0, 200, 16);

        /* 显示当前烟机系统的温湿度 */
        sprintf(dispBuf, "Temp&humi:%.1f C %.1f %%  ", s_state.temperature, s_state.humidity);
        GUI_ShowString(20, 60,  (const char*)dispBuf, BLUE, WHITE, ASCII_1608, 0, 200, 16);

        /* 显示当前烟机系统的烟雾浓度 */
        sprintf(dispBuf, "Gas:%.1f ppm  ", s_state.gasConcentration);
        GUI_ShowString(20, 85,  (const char*)dispBuf, BLUE, WHITE, ASCII_1608, 0, 200, 16);
        /* 显示当前烟机系统工作模式 */
        sprintf(dispBuf, "RangehoodMode:%s  ", ModeNames[s_state.currentMode]);
        GUI_ShowString(20, 110,  (const char*)dispBuf, BLUE, WHITE, ASCII_1608, 0, 200, 16);
        
        /* 显示电机转速档位 */
        sprintf(dispBuf, "MotorSpeedLevel:%s  ", SpeedLevelNames[s_state.speedLevel]);
        GUI_ShowString(20, 135,  (const char*)dispBuf, BLUE, WHITE, ASCII_1608, 0, 200, 16);
 
        /* 显示风速PWM(%) */
        sprintf(dispBuf, "WindSpeedPWM:%.1f%%  ", s_state.windSpeedPWM);
        GUI_ShowString(20, 160,  (const char*)dispBuf, BLUE, WHITE, ASCII_1608, 0, 200, 16);
        
        /* 显示实际电机转速 */
        sprintf(dispBuf, "MotorRPM:%.0f    ", s_state.actualRPM);
        GUI_ShowString(20, 185,  (const char*)dispBuf, BLUE, WHITE, ASCII_1608, 0, 200, 16);
        
        /* 显示自动模式状态 */
        sprintf(dispBuf, "AutoModeState:%s  ", AutoStateNames[s_autoModeState]);
        GUI_ShowString(20, 210, (const char*)dispBuf, BLUE, WHITE, ASCII_1608, 0, 200, 16);
        
        /* 显示自动模式启动计时 */
        sprintf(dispBuf, "AutoModeCnt:%ds  ", s_state.autoModeStartupCounter / 1000);
        GUI_ShowString(20, 235, (const char*)dispBuf, BLUE, WHITE, ASCII_1608, 0, 200, 16);
        
        /* 显示Cooking Event计时 */
        sprintf(dispBuf, "CookingEventCnt:%ds  ", s_state.cookingEventCounter / 1000);
        GUI_ShowString(20, 260, (const char*)dispBuf, BLUE, WHITE, ASCII_1608, 0, 200, 16);
        
        delay_ms(200);  /* 200ms刷新周期 */
    }
}

/*-----------------------------------------------------------
 * 防回流任务 - 周期100ms
 *----------------------------------------------------------*/
void AntiBackflowTask(void *pvParameters)
{
    while (1)
    {
        /* 判断是否是防回流模式 */
        if (s_state.currentMode == RANGEHOOD_MODE_ANTI_BACKFLOW)
        {
            if(s_state.gasConcentration >= s_state.gasThreshold)
            {
                if(s_state.gasConcentration < MQ2_THRESHOLD_HIGH) /* 防止在极端情况下会重启风机 */
                {
                        /* 气体超过NORMAL阈值，启动风机 */
                        s_state.antiBackflowActive = 1;
                        s_state.motorRunning = 1;
                        motor_start();
                        
                        /* 立即切换到HIGH阈值，防止重复触发 */
                        s_state.gasThreshold = MQ2_THRESHOLD_HIGH;
                }
            }
            else if(s_state.gasConcentration < MQ2_THRESHOLD_NORMAL)
            {
                /* 气体浓度降到NORMAL阈值以下，停止风机 */
                s_state.antiBackflowActive = 0;
                s_state.motorRunning = 0;
                motor_stop();
                s_state.gasThreshold = MQ2_THRESHOLD_NORMAL;
            }
        }
        else
        {
            /* 非防回流模式，重置所有状态 */
            s_state.antiBackflowActive = 0;
            s_state.gasThreshold = MQ2_THRESHOLD_NORMAL;
        }
        
        delay_ms(100);  /* 100ms检测周期 */
    }
}

#if HARDWARE_UPDATE_OPEN /* 如果引入固件升级 */
#if !HW_UPDATE_METHOD /* 有线IAP */
/*-----------------------------------------------------------
 * IAP 任务 
 *----------------------------------------------------------*/
void iap_hardwareUpdate_task(void *pvParameters)
{
    frame_info_t frame;
    uint8_t local_buf[MAX_FRAME_LENGTH];

    /* ---------- 状态机变量 ---------- */
    IAP_Stage stage = STAGE_GET_LENGTH;
    uint32_t  total_len = 0, received_len = 0, store_addr = STORAGE_START_ADDR;

    while (1)   
    {
        switch (stage)
        {
            case STAGE_GET_LENGTH:
            {
                //LOG_I("IAP stage: Get total length");
                if (xQueueReceive(frame_queue, &frame, portMAX_DELAY) != pdPASS) break;

                /* 缓冲区拷贝 */
                uint32_t start = frame.start_idx;
                if (start + frame.length <= IAP_REC_LEN) {
                    memcpy(local_buf, &iap_recbuff[start], 4);
                } else {
                    // 回绕处理
                    uint32_t first = IAP_REC_LEN - start;
                    memcpy(local_buf, &iap_recbuff[start], first);
                    memcpy(local_buf + first, iap_recbuff, frame.length - first);
                }

                /* 更新读指针 */
                taskENTER_CRITICAL();
                rx_rd_idx = (start + frame.length) % IAP_REC_LEN;
                taskEXIT_CRITICAL();

                if (frame.length != 4) {
                    //LOG_E("%x", CMD_NAK);
                    break;   // 仍然在此状态，等待有效长度帧
                }

                total_len = *(uint32_t*)local_buf;
                if (total_len == 0 || total_len > STORAGE_SIZE) {
                    //LOG_E("%x", CMD_NAK);
                    break;
                }

                //LOG_I("Total len of the coming data stream: %d", total_len);

                Flash_EraseArea(STORAGE_START_ADDR, STORAGE_SIZE);
                //LOG_I("The storage has been erased");
                received_len = 0;
                store_addr = STORAGE_START_ADDR;
                stage = STAGE_RECV_DATA;
                //LOG_I("%x", CMD_ACK);
                break;
            }

            case STAGE_RECV_DATA:
            {
                //LOG_I("IAP stage: Receive data stream");
                if (xQueueReceive(frame_queue, &frame, portMAX_DELAY) != pdPASS) break;

                /* 拷贝、写 Flash、回复 ACK */
                uint32_t start = frame.start_idx;
                uint32_t len   = frame.length;

                if (len == 0 || len > MAX_FRAME_LENGTH) continue;

                /* 拷贝到本地 */
                if (start + len <= IAP_REC_LEN) {
                    memcpy(local_buf, &iap_recbuff[start], len);
                } else {
                    uint32_t first = IAP_REC_LEN - start;
                    memcpy(local_buf, &iap_recbuff[start], first);
                    memcpy(local_buf + first, iap_recbuff, len - first);
                }
                //LOG_I("Just now receive %d bytes", len);
                /* 更新读指针 */
                taskENTER_CRITICAL();
                rx_rd_idx = (start + len) % IAP_REC_LEN;
                taskEXIT_CRITICAL();

                /* 写入暂存区 Flash */
                Flash_Write(store_addr, local_buf, len);
                //LOG_I("Succeed to write %d bytes into flash storage", len);
                store_addr += len;
                received_len += len;

                /* 回复 ACK 表明本帧已写入 */
                //LOG_I("%x", CMD_ACK);

                if (received_len >= total_len) {
                    if(total_len & 0x01) {
                        uint8_t lastBuff[2] = {local_buf[len - 1], 0xFF};
                        Flash_Write(store_addr - 1, lastBuff, 2);
                    }

                    stage = STAGE_CHECK;
                }
                break;
            }

            case STAGE_CHECK:
            {
                //LOG_I("IAP stage: Check and transfer the received data stream");
                uint32_t fw_len = total_len - 4;
                uint32_t expected_crc = *((volatile uint32_t*)(STORAGE_START_ADDR + fw_len));
                uint32_t actual_crc = CRC32_Flash(STORAGE_START_ADDR, fw_len);  

                if (actual_crc == expected_crc) {
                    /* 复制到 APP 区，清标志，跳转（永不返回） */
                    Flash_EraseArea(APP_START_ADDR, APP_SIZE);
                    //LOG_I("Already erase the APP area");
                    /* 按块复制（注意 Flash 写入需要 16 位对齐，这里按半字编程）*/
                    for (uint32_t i = 0; i < fw_len; i += 2) 
                    {
                        uint16_t half = *((volatile uint16_t*)(STORAGE_START_ADDR + i));
                        FLASH_ProgramHalfWord(APP_START_ADDR + i, half);
                    }
                    //LOG_I("Already transfer the data from storage to APP area");
                    BKP_UPGRADE_FLAG = 0;
                    //LOG_I("%x", CMD_ACK);
                    delay_ms(100);
                    JumpToApp(RAM_START_ADDR, RAM_SIZE, APP_START_ADDR, APP_SIZE);   // 这里一去不复返
                } else {
                    //LOG_E("%x", CMD_NAK);
                    //LOG_E("The data stream transferred error after CRC, please restart the trans by send totalLen");
                    stage = STAGE_GET_LENGTH;    // 回到初始状态重新等待
                }
                break;
            }
        } 
    } 
}

void IAP_USART_IRQHandler(void)
{
    if (USART_GetITStatus(IAP_USART, USART_IT_IDLE) != RESET)
    {
        (void)USART_ReceiveData(IAP_USART);   // 清除 IDLE 标志

        uint16_t remain = DMA_GetCurrDataCounter(IAP_USART_DMA_RXCH);
        uint32_t cur_wr_idx = IAP_REC_LEN - remain;
        uint32_t rd_snapshot = rx_rd_idx;

        uint32_t start = last_wr_idx;
        uint32_t length;
        if (cur_wr_idx >= start) {
            length = cur_wr_idx - start;
        } else {
            length = IAP_REC_LEN - start + cur_wr_idx;
        }

        /* 溢出检查 */
        uint32_t used_bytes;
        if (cur_wr_idx >= rd_snapshot) {
            used_bytes = cur_wr_idx - rd_snapshot;
        } else {
            used_bytes = IAP_REC_LEN - rd_snapshot + cur_wr_idx;
        }
        uint32_t free_bytes = IAP_REC_LEN - used_bytes;

        if (length == 0 || length > MAX_FRAME_LENGTH || length > free_bytes) {
            last_wr_idx = cur_wr_idx;
            return;
        }

        frame_info_t info = { start, length };
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        if (xQueueSendFromISR(frame_queue, &info, &xHigherPriorityTaskWoken) != pdPASS) {
            last_wr_idx = cur_wr_idx;   // 队列满，丢帧
        } else {
            last_wr_idx = cur_wr_idx;
        }
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

#else /* 无线OTA */

#endif /* #if !HW_UPDATE_METHOD */

#endif /* #if HARDWARE_UPDATE_OPEN */

/* 切换到自动模式时需要改变PID参数值 */
static void pidPara_change_modeAutoAndAntiBack(void) 
{
    s_speedPID.output_max = 800.0f;
    s_speedPID.output_min = 0.0f;
}

/*-----------------------------------------------------------
 * 切换工作模式
 *----------------------------------------------------------*/
void System_SwitchMode(void)
{
    if (xSemaphoreTake(s_dataConcernMutex, portMAX_DELAY) == pdTRUE)
    {
        /* 模式循环切换：待机->手动->自动->防回流->待机 */
        switch (s_state.currentMode)
        {
            case RANGEHOOD_MODE_STANDBY:
                s_state.currentMode = RANGEHOOD_MODE_MANUAL;
                break;
            case RANGEHOOD_MODE_MANUAL:
                s_state.currentMode = RANGEHOOD_MODE_AUTO;
                s_autoModeState = AUTO_STATE_STARTUP;
                break;
            case RANGEHOOD_MODE_AUTO:
                s_state.currentMode = RANGEHOOD_MODE_ANTI_BACKFLOW;
                pidPara_change_modeAutoAndAntiBack();
                break;
            case RANGEHOOD_MODE_ANTI_BACKFLOW:
                s_state.currentMode = RANGEHOOD_MODE_STANDBY;
                pidPara_change_modeAutoAndAntiBack();
                break;
        }
        
        /* 切换模式时停止电机（除自动模式外） */
        if (s_state.currentMode != RANGEHOOD_MODE_AUTO)
        {
            s_state.motorRunning = 0;
            motor_stop();
        }
        
        xSemaphoreGive(s_dataConcernMutex);
    }
}

/* 切换电机目标转速(手动模式)时需要改变PID参数值 */
static void pidPara_change_modeManual(MotorSpeedLevel speedLevel) 
{
    for(uint8_t i = 0; i < PID_GAIN_TABLE_SIZE; i++) {
        if(s_pidGainTable[i].speedLevel == speedLevel) {
            taskENTER_CRITICAL();
            /* 改变PID参数值 */
            s_speedPID.Kp = s_pidGainTable[i].kp;
            s_speedPID.Ki = s_pidGainTable[i].ki;
            s_speedPID.Kd = s_pidGainTable[i].kd;
            /* 改变积分上限&下限值 */
            s_speedPID.output_max = s_pidGainTable[i].op_max;
            s_speedPID.output_min = s_pidGainTable[i].op_min;
            taskEXIT_CRITICAL();
        } 
    }
}

/*-----------------------------------------------------------
 * 切换档位
 *----------------------------------------------------------*/
void System_SwitchSpeedLevel(void)
{
    if (xSemaphoreTake(s_dataConcernMutex, portMAX_DELAY) == pdTRUE)
    {
        /* 档位循环切换：LOW->HIGH->LOW */
        switch (s_state.speedLevel)
        {
            case MOTOR_SPEED_LOW:
                s_state.speedLevel = MOTOR_SPEED_HIGH;
                
                break;
            case MOTOR_SPEED_HIGH:
                s_state.speedLevel = MOTOR_SPEED_LOW;
                break;
        }
        pidPara_change_modeManual(s_state.speedLevel);
        xSemaphoreGive(s_dataConcernMutex);
    }
}

/*-----------------------------------------------------------
 * 切换电机开关
 *----------------------------------------------------------*/
void System_ToggleMotor(void)
{
    if (xSemaphoreTake(s_dataConcernMutex, portMAX_DELAY) == pdTRUE)
    {
        if (s_state.motorRunning)
        {
            /* 关闭电机：强制切换到待机模式，确保MotorControlTask不会重新拉起电机 */
            motor_stop();
            s_state.motorRunning = 0;
            s_state.currentMode = RANGEHOOD_MODE_STANDBY;
            GUI_ShowString(55, 36, "Motor stopped ", PINK, WHITE, ASCII_1608, 0, 200, 16);
        }
        else
        {
            /* 开启电机：强制切换到手动模式，确保MotorControlTask持续驱动电机 */
            motor_start();
            s_state.motorRunning = 1;
            s_state.currentMode = RANGEHOOD_MODE_MANUAL;
            GUI_ShowString(55, 36, "Motor started ", PINK, WHITE, ASCII_1608, 0, 200, 16);
        }
        
        xSemaphoreGive(s_dataConcernMutex);
    }
}

/*-----------------------------------------------------------
 * 电机转速计算任务(每隔250ms电机转速有效值更新) - 由SpeedCacl_TIM中断触发
 *----------------------------------------------------------*/
void MotorSpeedCalcTask(void *pvParameters)
{
    int encoderCount;

    while (1)
    {
        /* 等待SpeedCacl_TIM中断发送的信号量 */
        if (xSemaphoreTake(s_speedCalcSemaphore, portMAX_DELAY) == pdTRUE)
        {
            /* 获取编码器计数值 */
            encoderCount = get_encoder_value();
            
            /* 计算电机转速 */
            speed = motor_getSpeed(encoderCount, (uint16_t)(SpeedCacl_INTERVAL / (1000 / SpeedCacl_TIM_Freq)));
        }
    }
}

/*-----------------------------------------------------------
 * TIM_ENCODER 中断服务程序 - 编码器溢出处理
 *----------------------------------------------------------*/
void TIM_ENCODER_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM_ENCODER, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM_ENCODER, TIM_IT_Update);
        
        /* 根据计数方向更新溢出计数器 */
        if (TIM_GetDirection(TIM_ENCODER))
            overflow--;     /* 递减计数 */
        else
            overflow++;     /* 递增计数 */
    }
}

/*-----------------------------------------------------------
 * SpeedCacl_TIM 中断服务程序 - 触发速度计算任务
 *----------------------------------------------------------*/
void SpeedCacl_TIM_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if (TIM_GetITStatus(SpeedCacl_TIM, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(SpeedCacl_TIM, TIM_IT_Update);
        
        /* 发送信号量通知SpeedCalcTask执行速度计算 */
        xSemaphoreGiveFromISR(s_speedCalcSemaphore, &xHigherPriorityTaskWoken);
        
        /* 如果需要，触发上下文切换 */
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

#endif /* #if SYSTEM_SUPPORT_OS */
