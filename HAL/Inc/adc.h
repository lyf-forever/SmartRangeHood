#ifndef __ADC_H_
#define __ADC_H_

#include "sys.h"

#define ADC_RCC(ADCx)    PERIPH_APB2_RCC((ADCx == ADC1 ? ADC1 : ADCx == ADC2 ? ADC2 : ADC3))   

/* ==================== 可配置参数 ==================== */
#define ADC_DMA_BUF_SIZE    8           // 最多支持的通道数（DMA 缓冲大小）
#define ADC_DEFAULT_SAMPLETIME ADC_SampleTime_55Cycles5

/* ==================== 参数配置结构体 ==================== */
typedef struct {
    uint32_t mode;
    FunctionalState continusconvmode;
    FunctionalState scanconvmode;
    uint32_t extrigconv;
    uint32_t dataalign;
} adc_paras_t;

/* ==================== 通道配置结构体 ==================== */
typedef struct {
    uint8_t  adc_channel;               // ADC_Channel_x
    GPIO_TypeDef *port;                 // 对应 GPIO 组
    uint16_t pin;                       // 对应引脚
    uint16_t sample_time;               // 采样时间
    volatile uint16_t value;            // 最近一次转换结果（DMA 模式下自动更新）
} adc_ch_cfg_t;

/* ==================== ADC 设备结构体 ==================== */
typedef struct {
    ADC_TypeDef *adc;                        // 例如 ADC1
    adc_paras_t *paras;                      // ADC配置参数
    uint8_t  dma_mode;                       // 1 = 使用 DMA 循环扫描，0 = 单次软件触发  注意：ADC1 - DMA1_CH1, ADC3 - DMA2_CH5 ADC2不能使用DMA
    uint8_t  channel_count;                  // 已注册的通道数
    adc_ch_cfg_t channels[ADC_DMA_BUF_SIZE]; // 通道列表
    DMA_Channel_TypeDef *dma_ch;             // DMA 通道
    uint32_t dma_tc_flag;                    // 传输完成标志
    uint16_t *dma_buf;                       // DMA 缓冲区（与通道一一对应）
} adc_dev_t;

/* ==================== API 函数 ==================== */
void adc_onChipPeriph_cfg(adc_dev_t *dev, adc_ch_cfg_t *ch_cfg);
/* ==============================不使用DMA==================================== */
int adc_read_single(adc_dev_t *dev, uint8_t channel, uint16_t *val);
uint8_t adc_read_multi(adc_dev_t *dev, uint16_t *results);
/* ==============================不使用DMA==================================== */
void adc_dma_isr_handler(adc_dev_t *dev); // 在 DMA 传输完成中断中调用

#endif
