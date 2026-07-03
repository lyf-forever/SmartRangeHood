#include "windspeed.h"

#if WINDSPEED_IS_USE  /* 系统需要计算风速 */

/* 全局风速数据 */
static WindSpeed_t s_windSpeedData = {0};

#if 1
/**
 * @brief 限制浮点数在指定范围内
 * @param value: 输入值
 * @param min: 最小值
 * @param max: 最大值
 * @return 限制后的值
 */
static inline float Constrain(float value, float min, float max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

/* 初始化风速算法模块 */
void WindSpeed_Init(void)
{
    s_windSpeedData.f_T = 0.0f;
    s_windSpeedData.f_H = 0.0f;
    s_windSpeedData.f_G = 0.0f;
    s_windSpeedData.fusionValue = 0.0f;
    s_windSpeedData.pwmDuty = 0.0f;
    s_windSpeedData.isCookingEvent = 0;
}

/**
 * @brief 更新传感器数据并计算风速 PWM
 * @param temp     温度（℃）
 * @param humidity 湿度（%）
 * @param gas      气体浓度（ppm）
 */
void WindSpeed_Update(float temp, float humidity, float gas)
{
    /* 1. 传感器归一化 */
    /* 温度影响系数：f_T = (T - Tbase) / (Tmax - Tbase) */
    s_windSpeedData.f_T = (temp - TEMP_BASE) / (TEMP_MAX - TEMP_BASE);
    s_windSpeedData.f_T = Constrain(s_windSpeedData.f_T, 0.0f, 1.0f);
    /* 湿度影响系数: f_H = (H - Hbase) / (Hmax - Hbase) */
    s_windSpeedData.f_H = (humidity - HUMIDITY_BASE) / (HUMIDITY_MAX - HUMIDITY_BASE);
    s_windSpeedData.f_H = Constrain(s_windSpeedData.f_H, 0.0f, 1.0f);
    /* 气体浓度影响系数: f_G = (G - Gbase) / (Gmax - Gbase) */
    s_windSpeedData.f_G = (gas - GAS_BASE) / (GAS_MAX - GAS_BASE);
    s_windSpeedData.f_G = Constrain(s_windSpeedData.f_G, 0.0f, 1.0f);

    /* 2. 权重融合 F = w_t * f_T + w_h * f_H + w_g * f_G */
    s_windSpeedData.fusionValue = WEIGHT_TEMP * s_windSpeedData.f_T +
                                  WEIGHT_HUMIDITY * s_windSpeedData.f_H +
                                  WEIGHT_GAS * s_windSpeedData.f_G;

    /* 3. 映射到 PWM = PWM_min + (PWM_max - PWM_min) * F */
    s_windSpeedData.pwmDuty = PWM_MIN + (PWM_MAX - PWM_MIN) * s_windSpeedData.fusionValue;
    s_windSpeedData.pwmDuty = Constrain(s_windSpeedData.pwmDuty, PWM_MIN, PWM_MAX);

    /* 4. 烹饪事件判断 */
    if (( temp > COOKING_TEMP_THRESHOLD ) && ( humidity > COOKING_HUMIDITY_THRESHOLD ) && ( gas > COOKING_GAS_THRESHOLD ) )
        s_windSpeedData.isCookingEvent = 1;
    else
        s_windSpeedData.isCookingEvent = 0;
}

/**
 * @brief 获取计算得到的PWM占空比
 * @return PWM占空比(1-100%)
 */
float WindSpeed_GetPWM(void)
{
    return s_windSpeedData.pwmDuty;
}

/**
 * @brief 获取PWM占空比对应的CCR值
 * @param maxCompare: 定时器最大比较值（ARR值）
 * @return 定时器比较值
 */
u16 WindSpeed_GetPWMCompare(u16 maxCompare)
{
    u16 compare = (u16)(s_windSpeedData.pwmDuty * maxCompare / 100.0f);
    if (compare > maxCompare) compare = maxCompare;
    return compare;
}

/**
 * @brief 检测是否为Cooking Event
 * @return 1:是Cooking Event; 0:不是
 */
u8 WindSpeed_IsCookingEvent(void)
{
    return s_windSpeedData.isCookingEvent;
}

/**
 * @brief 获取风速数据结构体指针
 * @return 风速数据结构体指针
 */
WindSpeed_t* WindSpeed_GetData(void)
{
    return &s_windSpeedData;
}

/**
 * @brief 根据档位获取目标转速
 * @param level: 档位 (0:LOW, 1:HIGH)
 * @return 目标转速 (RPM)
 */
u16 WindSpeed_GetTargetRPM(u8 level)  
{
    switch (level)
    {
        case 0:
            return SPEED_LOW_RPM;
        case 1:
            return SPEED_HIGH_RPM;
        default:
            return SPEED_LOW_RPM;
    }
}
#else
/**
 * @brief 限制浮点数在指定范围内
 * @param value: 输入值
 * @param min: 最小值
 * @param max: 最大值
 * @return 限制后的值
 */
static float Constrain(float value, float min, float max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

/**
 * @brief 初始化风速算法模块
 */
void WindSpeed_Init(void)
{
    s_windSpeedData.f_T = 0.0f;
    s_windSpeedData.f_H = 0.0f;
    s_windSpeedData.f_G = 0.0f;
    s_windSpeedData.fusionValue = 0.0f;
    s_windSpeedData.pwmValue = 0.0f;
    s_windSpeedData.isCookingEvent = 0;
}

/**
 * @brief 更新传感器数据并计算风速
 * @param temp: 温度值 (℃)
 * @param humidity: 湿度值 (%)
 * @param gas: 气体浓度值 
 */
void WindSpeed_Update(u8 temp, u8 humidity, float gas)
{
    /* 1. 传感器归一化 */
    /* 温度影响系数: f_T = (T - T_base) / (T_max - T_base) */
    s_windSpeedData.f_T = (temp - TEMP_BASE) / (TEMP_MAX - TEMP_BASE);
    s_windSpeedData.f_T = Constrain(s_windSpeedData.f_T, 0.0f, 1.0f);
    
    /* 湿度影响系数: f_H = (H - H_base) / (H_max - H_base) */
    s_windSpeedData.f_H = (humidity - HUMIDITY_BASE) / (HUMIDITY_MAX - HUMIDITY_BASE);
    s_windSpeedData.f_H = Constrain(s_windSpeedData.f_H, 0.0f, 1.0f);
    
    /* 气体浓度影响系数: f_G = (G - G_base) / (G_max - G_base) */
    s_windSpeedData.f_G = (gas - GAS_BASE) / (GAS_MAX - GAS_BASE);
    s_windSpeedData.f_G = Constrain(s_windSpeedData.f_G, 0.0f, 1.0f);
    
    /* 2. 权重融合: F = w_t * f_T + w_h * f_H + w_g * f_G */
    s_windSpeedData.fusionValue = WEIGHT_TEMP * s_windSpeedData.f_T +
                                  WEIGHT_HUMIDITY * s_windSpeedData.f_H +
                                  WEIGHT_GAS * s_windSpeedData.f_G;
    
    /* 3. 映射到PWM: PWM = PWM_min + (PWM_max - PWM_min) * F */
    s_windSpeedData.pwmValue = PWM_MIN + (PWM_MAX - PWM_MIN) * s_windSpeedData.fusionValue;
    s_windSpeedData.pwmValue = Constrain(s_windSpeedData.pwmValue, PWM_MIN, PWM_MAX);
    
    /* 4. 判断是否为Cooking Event */
    if ((temp > COOKING_TEMP_THRESHOLD) &&
        ((humidity > COOKING_HUMIDITY_THRESHOLD) && (gas > COOKING_GAS_THRESHOLD)))
    {
        s_windSpeedData.isCookingEvent = 1;
    }
    else
    {
        s_windSpeedData.isCookingEvent = 0;
    }
}

/**
 * @brief 获取计算得到的PWM占空比
 * @return PWM占空比(1-100%)
 */
float WindSpeed_GetPWM(void)
{
    return s_windSpeedData.pwmValue;
}

/**
 * @brief 获取PWM占空比对应的CCR值
 * @param maxCompare: 定时器最大比较值（ARR值）
 * @return 定时器比较值
 */
u16 WindSpeed_GetPWMCompare(u16 maxCompare)
{
    return (u16)(s_windSpeedData.pwmValue * maxCompare / 100.0f);
}

/**
 * @brief 检测是否为Cooking Event
 * @return 1:是Cooking Event; 0:不是
 */
u8 WindSpeed_IsCookingEvent(void)
{
    return s_windSpeedData.isCookingEvent;
}

/**
 * @brief 获取风速数据结构体指针
 * @return 风速数据结构体指针
 */
WindSpeed_t* WindSpeed_GetData(void)
{
    return &s_windSpeedData;
}

/**
 * @brief 根据档位获取目标转速
 * @param level: 档位 (0:LOW, 1:HIGH)
 * @return 目标转速 (RPM)
 */
u16 WindSpeed_GetTargetRPM(u8 level)
{
    switch (level)
    {
        case 0:
            return SPEED_LOW_RPM;
        case 1:
            return SPEED_HIGH_RPM;
        default:
            return SPEED_LOW_RPM;
    }
}
#endif

#endif 

