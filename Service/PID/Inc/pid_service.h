#ifndef __PID_SERVICE_H_
#define __PID_SERVICE_H_

#include "sys.h"

#if 0
/* PID 模式选择 */
typedef enum {
    PID_MODE_POSITIONAL = 0,   /* 位置式 PID */
    PID_MODE_INCREMENTAL       /* 增量式 PID */
} PID_Mode;

/* PID 方向选择（正向/反向调节） */
typedef enum {
    PID_DIRECT  = 0,           /* 误差 = 设定值 - 反馈值 */
    PID_REVERSE = 1            /* 误差 = 反馈值 - 设定值 */
} PID_Dir;

/* PID 控制器句柄 */
typedef struct {
    /* 系数 */
    float Kp;
    float Ki;
    float Kd;

    /* 控制参数 */
    float    SampleTime;        /* 采样时间 (秒) */
    float    OutMax;            /* 输出上限 */
    float    OutMin;            /* 输出下限 */
    uint8_t  IntegralSepEn;     /* 是否开启积分分离 */
    float    IntegralSepErr;    /* 积分分离误差阈值 */

    /* 内部状态 */
    float    Integral;          /* 积分累加值 */
    float    PrevError;         /* 上次误差 */
    float    PrevPrevError;     /* 上上次误差（增量式用） */
    float    Output;            /* 当前输出值 */
    float    SetPoint;          /* 期望值 */

    PID_Mode Mode;              /* 工作模式 */
    PID_Dir  Direction;         /* 控制方向 */
} PID_Handle;

/* 函数声明 */
void PID_Init(PID_Handle *pid, 
              float Kp, float Ki, float Kd,
              float sampleTime, 
              float outMin, float outMax,
              PID_Mode mode, PID_Dir direction);

void PID_SetPoint(PID_Handle *pid, float newPoint);        
void PID_SetTunings(PID_Handle *pid, float Kp, float Ki, float Kd);
void PID_SetSampleTime(PID_Handle *pid, float newSampleTime);
void PID_SetOutputLimits(PID_Handle *pid, float min, float max);
void PID_SetIntegralSep(PID_Handle *pid, uint8_t enable, float threshold);
void PID_SetDirection(PID_Handle *pid, PID_Dir direction);

float PID_Compute(PID_Handle *pid, float input);
void PID_Reset(PID_Handle *pid);

#else
/**
 * @brief PID控制器结构体（位置式PID）
 */
typedef struct {
    float Kp;           /* 比例系数 */
    float Ki;           /* 积分系数 */
    float Kd;           /* 微分系数 */
    
    float target;       /* 目标值（设定值） */
    float actual;       /* 实际值（反馈值） */
    
    float error;        /* 当前误差 */
    float last_error;   /* 上次误差 */
    float integral;     /* 误差积分累加值 */
    
    float output;       /* PID输出值 */
    float output_max;   /* 输出上限 */
    float output_min;   /* 输出下限 */
    
    float integral_max; /* 积分上限（防止积分饱和） */
} PID_TypeDef;

/**
 * @brief 初始化PID控制器
 * @param pid: PID结构体指针
 * @param Kp: 比例系数
 * @param Ki: 积分系数
 * @param Kd: 微分系数
 * @param out_max: 输出上限
 * @param out_min: 输出下限
 */
void PID_Init(PID_TypeDef *pid, float Kp, float Ki, float Kd, float out_max, float out_min);

/**
 * @brief 设置PID目标值
 * @param pid: PID结构体指针
 * @param target: 目标值
 */
void PID_SetTarget(PID_TypeDef *pid, float target);

/**
 * @brief PID计算（位置式）
 * @param pid: PID结构体指针
 * @param actual: 当前实际值
 * @return PID输出值
 */
float PID_Calculate(PID_TypeDef *pid, float actual);

/**
 * @brief 复位PID控制器
 * @param pid: PID结构体指针
 */
void PID_Reset(PID_TypeDef *pid);

#endif /* #if 0 */

#endif /* __PID_SERVICE_H_ */


