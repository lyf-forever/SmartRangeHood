#ifndef __MQ2_H__
#define __MQ2_H__

#include "sys.h"
#include "adc.h"        /* adc_dev_t, adc_ch_register, adc_read_single */
#include <stdint.h>

#if USE_SENSOR_MQ2

/* ==================== 硬件参数配置 ==================== */
#define MQ2_ADC_DEV            ADC1            /* 使用的 ADC 外设 */
#define MQ2_ADC_CHANNEL        ADC_Channel_4   /* 模拟输入通道 */
#define MQ2_SAMPLE_TIME        ADC_SampleTime_55Cycles5 

#define MQ2_VREF               3.3f            /* ADC 基准电压 (V) */
#define MQ2_RL                 0.5f            /* 负载电阻 (kΩ) */
#define MQ2_VC                 5.0f            /* 回路电压 (V) */

/* 预热与报警 */
#define MQ2_WARMUP_MS          10000           /* 预热 1 分钟 */
#define MQ2_DEFAULT_ALARM_PPM  220.0f          /* 默认报警阈值 (ppm) */

/* 气体浓度阈值定义（用于防回流模式） */
#define MQ2_THRESHOLD_NORMAL   130.0f          /* 正常阈值 */
#define MQ2_THRESHOLD_HIGH     2000.0f         /* 切换后的高阈值 */

/* 烟雾特性曲线拟合系数 */
#define MQ2_SMOKE_A            11.5428f
#define MQ2_SMOKE_B            0.6549f

/* ==================== MQ2 设备结构体 ==================== */
typedef struct {
    uint8_t     warmed_up;          /* 预热完成标志 0:预热未完成 1:预热完成*/
    float       r0;                 /* 洁净空气中标定的 R0 (kΩ) */
    float       alarm_threshold;    /* 报警阈值 (ppm) */
    adc_dev_t   *adc;               /* 关联的 ADC 设备 */
    uint32_t    init_tick;          /* 初始化时间戳 (ms) */
} mq2_rel_t;

/* 结构体外部声明 */
extern mq2_rel_t mq2_rel;

/* ==================== API 函数 ==================== */
void mq2_onChip_init(void);
void mq2_driver_init(mq2_rel_t *rel);
uint8_t mq2_calibrate_r0(mq2_rel_t *rel);
uint8_t mq2_read(mq2_rel_t *rel, uint16_t *adc_raw);
uint8_t mq2_is_warmed_up(mq2_rel_t *rel);
uint8_t mq2_is_alarm(float ppm, float alarm_threshold);

#endif /* #if USE_SENSOR_MQ2 */

#endif

