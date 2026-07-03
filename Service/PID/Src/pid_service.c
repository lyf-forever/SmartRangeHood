#include "pid_service.h"
#include <math.h>   /* 如果使用 fabsf, 实际上可以自己实现 */

#if 0
/* 内部辅助：限幅 */
static float clamp(float value, float min, float max) {
    if (value > max) return max;
    if (value < min) return min;
    return value;
}

/**
 * @brief  PID 初始化
 * @param  pid:         句柄指针
 * @param  Kp,Ki,Kd:    初始 PID 系数
 * @param  sampleTime:  采样时间 (秒), 需与调用周期匹配
 * @param  outMin/Max:  输出限幅
 * @param  mode:        位置式 / 增量式
 * @param  direction:   正作用 / 反作用
 */
void PID_Init(PID_Handle *pid,
              float Kp, float Ki, float Kd,
              float sampleTime,
              float outMin, float outMax,
              PID_Mode mode, PID_Dir direction)
{
    pid->Kp = Kp;
    pid->Ki = Ki;
    pid->Kd = Kd;
    pid->SampleTime = sampleTime;
    pid->OutMax = outMax;
    pid->OutMin = outMin;

    pid->IntegralSepEn  = 0;         /* 默认关闭积分分离 */
    pid->IntegralSepErr = 0.0f;

    pid->Integral       = 0.0f;
    pid->PrevError      = 0.0f;
    pid->PrevPrevError  = 0.0f;
    pid->Output         = 0.0f;
    pid->SetPoint       = 0.0f;

    pid->Mode      = mode;
    pid->Direction = direction;
}

/**
 * @brief 设置PID目标值
 * @param pid: PID结构体指针
 * @param target: 目标值
 */
void PID_SetPoint(PID_Handle *pid, float newPoint)
{
    pid->SetPoint = newPoint;
}

/**
 * @brief  动态修改 PID 系数（可在运行中调用）
 */
void PID_SetTunings(PID_Handle *pid, float Kp, float Ki, float Kd)
{
    /* 如果系数未变，直接返回 */
    if (pid->Kp == Kp && pid->Ki == Ki && pid->Kd == Kd) return;

    /* 如果采样时间大于0，重新计算内部 Ki/Kd 以适配采样时间变化 */
    if (pid->SampleTime > 0.0f) {
        float ratio = pid->SampleTime;   /* Ki 已经是 1/s 单位？ 
           这里按标准 PID 库约定：Ki/Kd 直接使用，不进行预缩放 */
    }

    pid->Kp = Kp;
    pid->Ki = Ki;
    pid->Kd = Kd;
}

/**
 * @brief  修改采样时间（会导致 Ki/Kd 的等效效果变化，需配合重调参数）
 */
void PID_SetSampleTime(PID_Handle *pid, float newSampleTime)
{
    if (newSampleTime > 0.0f) {
        float ratio = newSampleTime / pid->SampleTime;
        pid->Ki *= ratio;
        pid->Kd /= ratio;
        pid->SampleTime = newSampleTime;
    }
}

/**
 * @brief  设置输出限幅
 */
void PID_SetOutputLimits(PID_Handle *pid, float min, float max)
{
    if (min >= max) return;
    pid->OutMin = min;
    pid->OutMax = max;

    /* 当前积分和输出可能越界，限制一下 */
    pid->Integral = clamp(pid->Integral, min, max);
    pid->Output   = clamp(pid->Output,   min, max);
}

/**
 * @brief  积分分离配置
 */
void PID_SetIntegralSep(PID_Handle *pid, uint8_t enable, float threshold)
{
    pid->IntegralSepEn  = enable;
    pid->IntegralSepErr = threshold;
}

/**
 * @brief  设置控制方向，并自动反转 Kp/Ki/Kd 符号（内部）
 */
void PID_SetDirection(PID_Handle *pid, PID_Dir direction)
{
    if (pid->Direction != direction) {
        pid->Kp = -pid->Kp;
        pid->Ki = -pid->Ki;
        pid->Kd = -pid->Kd;
        pid->Direction = direction;
    }
}

/**
 * @brief  PID 计算核心（每个采样周期调用一次）
 * @param  pid:       句柄
 * @param  setPoint:  设定值
 * @param  input:     传感器反馈值
 * @return 计算后的输出值 (已限幅)
 */
float PID_Compute(PID_Handle *pid, float input)
{
    float error;
    float p_term, i_term, d_term;

    /* 1. 计算误差（考虑正反作用） */
    if (pid->Direction == PID_DIRECT)
        error = pid->SetPoint - input;
    else
        error = input - pid->SetPoint;

    /* 2. 比例项 */
    p_term = pid->Kp * error;

    /* 3. 积分项（带积分分离和抗积分饱和） */
    if (pid->Mode == PID_MODE_POSITIONAL) {
        /* 是否开启积分分离 */
        if (pid->IntegralSepEn) {
            /* 误差大于阈值时关闭积分作用 */
            if (fabs(error) > pid->IntegralSepErr) {
                pid->Integral = 0.0f;   /* 清零积分 */
            }
            else {
                pid->Integral += pid->Ki * error * pid->SampleTime;
            }
        }
        else {
            pid->Integral += pid->Ki * error * pid->SampleTime;
        }
        /* 抗积分饱和：累加后立即限幅 */
        pid->Integral = clamp(pid->Integral, pid->OutMin, pid->OutMax);
        i_term = pid->Integral;
    }
    else {
        /* 增量式积分累加在输出上，这里不单独计算 i_term */
        i_term = 0.0f;
    }

    /* 4. 微分项（使用微分先行或标准微分，此处用误差微分） */
    d_term = pid->Kd * (error - pid->PrevError) / pid->SampleTime;

    /* 5. 计算输出 */
    if (pid->Mode == PID_MODE_POSITIONAL) {
        pid->Output = p_term + i_term + d_term;
    }
    else { /* 增量式 */
        /* 增量 = Kp*(e(k)-e(k-1)) + Ki*e(k)*T + Kd*(e(k)-2e(k-1)+e(k-2))/T */
        float delta;
        float p_inc = pid->Kp * (error - pid->PrevError);
        float i_inc = pid->Ki * error * pid->SampleTime;
        float d_inc = pid->Kd * (error - 2.0f * pid->PrevError + pid->PrevPrevError) / pid->SampleTime;
        delta = p_inc + i_inc + d_inc;
        pid->Output += delta;
    }

    /* 6. 输出限幅 */
    pid->Output = clamp(pid->Output, pid->OutMin, pid->OutMax);

    /* 7. 保存误差历史 */
    pid->PrevPrevError = pid->PrevError;
    pid->PrevError     = error;

    return pid->Output;
}

/**
 * @brief  重置 PID 内部状态（积分和误差历史清零，输出清零）
 */
void PID_Reset(PID_Handle *pid)
{
    pid->Integral      = 0.0f;
    pid->PrevError     = 0.0f;
    pid->PrevPrevError = 0.0f;
    pid->Output        = 0.0f;
}

#else
/**
 * @brief 初始化PID控制器
 * @param pid: PID结构体指针
 * @param Kp: 比例系数
 * @param Ki: 积分系数
 * @param Kd: 微分系数
 * @param out_max: 输出上限
 * @param out_min: 输出下限
 */
void PID_Init(PID_TypeDef *pid, float Kp, float Ki, float Kd, float out_max, float out_min)
{
    pid->Kp = Kp;
    pid->Ki = Ki;
    pid->Kd = Kd;
    
    pid->target = 0.0f;
    pid->actual = 0.0f;
    
    pid->error = 0.0f;
    pid->last_error = 0.0f;
    pid->integral = 0.0f;
    
    pid->output = 0.0f;
    pid->output_max = out_max;
    pid->output_min = out_min;
    
    /* 积分限幅设为输出限幅的一半，防止积分饱和 */
    pid->integral_max = out_max / 2.0f;
}

/**
 * @brief 设置PID目标值
 * @param pid: PID结构体指针
 * @param target: 目标值
 */
void PID_SetTarget(PID_TypeDef *pid, float target)
{
    pid->target = target;
}

/**
 * @brief PID计算（位置式）
 * @note 位置式PID公式：output = Kp*e(k) + Ki*Σe(k) + Kd*[e(k)-e(k-1)]
 * @param pid: PID结构体指针
 * @param actual: 当前实际值
 * @return PID输出值
 */
float PID_Calculate(PID_TypeDef *pid, float actual)
{
    float p_out, i_out, d_out;
    
    /* 更新实际值 */
    pid->actual = actual;
    
    /* 计算当前误差 */
    pid->error = pid->target - pid->actual;
    
    /* 积分累加 */
    pid->integral += pid->error;
    
    /* 积分限幅，防止积分饱和 */
    if (pid->integral > pid->integral_max)
    {
        pid->integral = pid->integral_max;
    }
    else if (pid->integral < -pid->integral_max)
    {
        pid->integral = -pid->integral_max;
    }
    
    /* 计算PID三个分量 */
    p_out = pid->Kp * pid->error;                           /* 比例项 */
    i_out = pid->Ki * pid->integral;                        /* 积分项 */
    d_out = pid->Kd * (pid->error - pid->last_error);       /* 微分项 */
    
    /* 计算PID输出 */
    pid->output = p_out + i_out + d_out;
    
    /* 输出限幅 */
    if (pid->output > pid->output_max)
    {
        pid->output = pid->output_max;
    }
    else if (pid->output < pid->output_min)
    {
        pid->output = pid->output_min;
    }
    
    /* 保存当前误差供下次使用 */
    pid->last_error = pid->error;
    
    return pid->output;
}

/**
 * @brief 复位PID控制器
 * @param pid: PID结构体指针
 */
void PID_Reset(PID_TypeDef *pid)
{
    pid->error = 0.0f;
    pid->last_error = 0.0f;
    pid->integral = 0.0f;
    pid->output = 0.0f;
}

#endif 


