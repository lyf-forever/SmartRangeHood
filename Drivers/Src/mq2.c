#include "mq2.h"
#include "debug_log.h"
#include "stm32f10x_adc.h"
#include "delay.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#if USE_SENSOR_MQ2

/* DMA缓冲区 */
static uint16_t mq2_dma_buf[ADC_DMA_BUF_SIZE] = {0};

static adc_ch_cfg_t mq2_ch_cfg = {
    .adc_channel = MQ2_ADC_CHANNEL,
    .sample_time = MQ2_SAMPLE_TIME,
};

static adc_paras_t mq2_paras = {
    .mode = ADC_Mode_Independent,            /* 独立模式 */
    .extrigconv = ADC_ExternalTrigConv_None, /* 软件触发 */
    .dataalign = ADC_DataAlign_Right,        /* 右对齐 */
};

static adc_dev_t mq2_adc_dev = {
    .adc = MQ2_ADC_DEV,
    .paras = &mq2_paras,
    .channel_count = 0,
    .dma_buf = mq2_dma_buf,
    .dma_ch = (DMA_Channel_TypeDef*)0,
    .dma_mode = 0,       
    .dma_tc_flag = (uint32_t)0,
};

mq2_rel_t mq2_rel = {
    .adc = &mq2_adc_dev,
    .warmed_up = 0,    
};

/* MQ-2 片上外设初始化 */
void mq2_onChip_init(void) {
    adc_onChipPeriph_cfg(&mq2_adc_dev, &mq2_ch_cfg);
}

/**
 * @brief  检查是否已完成预热
 * @retval 0:预热未完成 1：预热完成
 */
uint8_t mq2_is_warmed_up(mq2_rel_t *rel)
{
    if ((!rel->warmed_up) && (get_tick_ms() - rel->init_tick >= MQ2_WARMUP_MS)) {
        rel->warmed_up = 1;
    }

    return rel->warmed_up;
}

/**
 * @brief  初始化 MQ2 设备，绑定 ADC 并记录启动时间
 * @param  rel   MQ2 关联信息指针
 */
void mq2_driver_init(mq2_rel_t *rel) {
    rel->r0 = 10.0f;   /* 默认值，标定后覆盖 */
    rel->alarm_threshold = MQ2_DEFAULT_ALARM_PPM;
    rel->init_tick = get_tick_ms();
}

/**
 * @brief  在洁净空气中标定 R0
 * @note   调用前确保传感器已在洁净空气中预热至少 1 分钟
 * @retval  0:校准成功，1:校准失败
 */
uint8_t mq2_calibrate_r0(mq2_rel_t *rel)
{
    float rs_sum = 0.0f;
    uint16_t adc_raw;
    float vrl, rs;
    const uint8_t samples = 10;

    for (uint8_t i = 0; i < samples; i++) {
        /* 单次读取 ADC 值（标定期间使用单次模式，不影响 DMA 循环） */
        if(adc_read_single(rel->adc, MQ2_ADC_CHANNEL, &adc_raw) != 0) return 1;
        
        vrl = (float)adc_raw / 4095.0f * MQ2_VREF;
        
        if (vrl > 0.01f) rs = (MQ2_VC - vrl) * MQ2_RL / vrl;
        else rs = 999.9f;   /* 异常值 */
    
        rs_sum += rs;

        /* 延时 100us */
        delay_us(100);
    }

    rel->r0 = rs_sum / samples;
    
    return 0;
}

/**
 * @brief  执行一次完整的传感器读取（电压、Rs、Rs/R0、ppm）
 * @note   若 ADC 工作在 DMA 模式，直接读取 channels[i].value；
 *         否则降级为 adc_read_single()
 * @retval 0:读取成功 1:读取失败
 */
uint8_t mq2_read(mq2_rel_t *rel, uint16_t *adc_raw)
{
    /* 从 ADC 设备中获取通道值 */
    // for (uint8_t i = 0; i < rel->adc->channel_count; i++) {
    //     if (rel->adc->channels[i].adc_channel == MQ2_ADC_CHANNEL) {
    //         /* DMA 循环模式下 value 会自动更新 */
    //         adc_raw = rel->adc->channels[i].value;
    //         found = 1;
    //         break;
    //     }
    // }
    uint32_t sum = 0;
    uint16_t adcVal;

    for(uint8_t i = 0; i < 10; i++) {
        if(adc_read_single(rel->adc, MQ2_ADC_CHANNEL, &adcVal) != 0) return 1;  // 读取失败则返回1
        sum += adcVal;
        delay_us(100);
    }
    
    *adc_raw = sum / 10;
    
    return 0;
}

/**
 * @brief  检查是否超过报警阈值
 * @param  ppm                气体浓度 (ppm)
 * @param  alarm_threshold    MQ2 报警浓度阈值
 * @return 1 = 报警, 0 = 正常
 */
uint8_t mq2_is_alarm(float ppm, float alarm_threshold)
{
    return (ppm >= alarm_threshold) ? 1 : 0;
}

#endif /* #if USE_SENSOR_MQ2 */



