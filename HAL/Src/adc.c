#include "adc.h"
#include "debug_log.h"
#include <string.h>
#include "stm32f10x_adc.h"

/**
 * @brief  使能使用的ADC时钟与DMA时钟
 */
static void adc_clk_init(adc_dev_t *dev) {
    if (dev->adc == ADC1) {    
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    } else if (dev->adc == ADC2) {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC2, ENABLE);
    } else if (dev->adc == ADC3) {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC3, ENABLE);
    } 
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);  // 72M/6=12MHz
    if(dev->dma_mode) {
        if(dev->adc == ADC3)    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA2, ENABLE);
        else RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    }
}

/* 根据ADC通道注册GPIO */
static void adc_gpio_register(ADC_TypeDef *adc, adc_ch_cfg_t *ch) {
    switch (ch->adc_channel) {
            case ADC_Channel_0:  ch->port = GPIOA; ch->pin = GPIO_Pin_0; break;
            case ADC_Channel_1:  ch->port = GPIOA; ch->pin = GPIO_Pin_1; break;
            case ADC_Channel_2:  ch->port = GPIOA; ch->pin = GPIO_Pin_2; break;
            case ADC_Channel_3:  ch->port = GPIOA; ch->pin = GPIO_Pin_3; break;
            case ADC_Channel_4:  
                if(adc == ADC3) { ch->port = GPIOF; ch->pin = GPIO_Pin_6; }
                else            { ch->port = GPIOA; ch->pin = GPIO_Pin_4; }           
                break;
            case ADC_Channel_5: 
                if(adc == ADC3) { ch->port = GPIOF; ch->pin = GPIO_Pin_7; }
                else            { ch->port = GPIOA; ch->pin = GPIO_Pin_5; }           
                break;
            case ADC_Channel_6:  
                if(adc == ADC3) { ch->port = GPIOF; ch->pin = GPIO_Pin_8; }
                else            { ch->port = GPIOA; ch->pin = GPIO_Pin_6; }           
                break;
            case ADC_Channel_7:  
                if(adc == ADC3) { ch->port = GPIOF; ch->pin = GPIO_Pin_9; }
                else            { ch->port = GPIOA; ch->pin = GPIO_Pin_7; }           
                break;
            case ADC_Channel_8:  
                if(adc == ADC3) { ch->port = GPIOF; ch->pin = GPIO_Pin_10; }
                else            { ch->port = GPIOB; ch->pin = GPIO_Pin_0; }   /* 与lcd 相关引脚冲突 */        
                break;
            case ADC_Channel_9: 
                if(adc == ADC3) { LOG_E("ADC3 has no channel_9"); }
                else            { ch->port = GPIOB; ch->pin = GPIO_Pin_1; }   /* 与lcd 相关引脚冲突 */         
                break;
            case ADC_Channel_10: ch->port = GPIOC; ch->pin = GPIO_Pin_0; break;
            case ADC_Channel_11: ch->port = GPIOC; ch->pin = GPIO_Pin_1; break;
            case ADC_Channel_12: ch->port = GPIOC; ch->pin = GPIO_Pin_2; break;
            case ADC_Channel_13: ch->port = GPIOC; ch->pin = GPIO_Pin_3; break;
            case ADC_Channel_14:
                if(adc == ADC3) { LOG_E("ADC3 has no channel_14"); }
                else            { ch->port = GPIOC; ch->pin = GPIO_Pin_4; }           
                break;
            case ADC_Channel_15: 
                if(adc == ADC3) { LOG_E("ADC3 has no channel_15"); }
                else            { ch->port = GPIOC; ch->pin = GPIO_Pin_5; }           
                break;
            default: break;
        }
}

/**
 * @brief  注册一个 ADC 通道，且填补GPIO信息
 * @return 0 成功，-1 通道数已满
 */
static int adc_ch_register(adc_dev_t *dev, adc_ch_cfg_t *ch_cfg) {
    if (dev->channel_count >= ADC_DMA_BUF_SIZE) { LOG_E("The ADC channel list is already full"); return -1; }

    adc_ch_cfg_t *ch = &dev->channels[dev->channel_count];
    ch->adc_channel = ch_cfg->adc_channel;
    adc_gpio_register(dev->adc, ch);     // 通道对应GPIO配置
    ch->sample_time = ch_cfg->sample_time;
    ch->value = 0;
    dev->channel_count++;

    return 0;
}

/**
 * @brief  为注册的通道配置 GPIO 为模拟输入
 */
static void adc_gpio_init(adc_dev_t *dev) {
    GPIO_InitTypeDef gpio = { .GPIO_Mode = GPIO_Mode_AIN };
    for (int i = 0; i < dev->channel_count; i++) {
        // 开启 GPIO 时钟（此处简化，实际要根据 port 动态开启）
        if (dev->channels[i].port == GPIOA) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
        else if (dev->channels[i].port == GPIOB) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
        else if (dev->channels[i].port == GPIOC) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
        else if (dev->channels[i].port == GPIOF) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOF, ENABLE);
        
        gpio.GPIO_Pin = dev->channels[i].pin;
        GPIO_Init(dev->channels[i].port, &gpio);
    }
}

static void adc_dma_cfg(adc_dev_t *dev) {
    if(dev->adc == ADC3) {
            dev->dma_ch = DMA2_Channel5; // 默认 ADC3 使用 DMA2_Channel5
            dev->dma_tc_flag = DMA2_IT_TC5;
    } else {
        dev->dma_ch = DMA1_Channel1;     // ADC1 使用 DMA1_Channel1
        dev->dma_tc_flag = DMA1_IT_TC1;
    }   
        
    DMA_InitTypeDef dma_init = {
            .DMA_PeripheralBaseAddr = (uint32_t)&dev->adc->DR,
            .DMA_MemoryBaseAddr     = (uint32_t)dev->dma_buf,
            .DMA_DIR                = DMA_DIR_PeripheralSRC,
            .DMA_BufferSize         = 0,
            .DMA_PeripheralInc      = DMA_PeripheralInc_Disable,
            .DMA_MemoryInc          = DMA_MemoryInc_Enable,
            .DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord,
            .DMA_MemoryDataSize     = DMA_MemoryDataSize_HalfWord,
            .DMA_Mode               = DMA_Mode_Circular,
            .DMA_Priority           = DMA_Priority_High,
            .DMA_M2M                = DMA_M2M_Disable,
    };
        
    DMA_DeInit(dev->dma_ch);
    DMA_Init(dev->dma_ch, &dma_init);
    DMA_Cmd(dev->dma_ch, DISABLE);
    DMA_SetCurrDataCounter(dev->dma_ch, dev->channel_count);
    DMA_Cmd(dev->dma_ch, ENABLE);
    
    ADC_DMACmd(dev->adc, ENABLE);
}

/**
 * @brief  初始化 ADC 及其相关外设，如GPIO DMA(根据具体使用情况)
 */
void adc_onChipPeriph_cfg(adc_dev_t *dev, adc_ch_cfg_t *ch_cfg) {
    /* 时钟配置 - ADC DMA */
    adc_clk_init(dev);
    /* 注册ADC通道，包含GPIO ADC_Channel信息 */
    adc_ch_register(dev, ch_cfg);
    /* 配置 GPIO */
    adc_gpio_init(dev);
    /* 复位ADC */
    ADC_DeInit(dev->adc);
    /* ADC部分参数抉择 */
    dev->paras->scanconvmode = (dev->dma_mode) ? ENABLE : ((dev->channel_count > 1) ? ENABLE : DISABLE);
    dev->paras->continusconvmode = (dev->dma_mode) ? ENABLE : DISABLE;
    /* ADC 基础配置 */
    ADC_InitTypeDef adc_init = {
        .ADC_Mode = dev->paras->mode,
        .ADC_ScanConvMode = dev->paras->scanconvmode,
        .ADC_ContinuousConvMode = dev->paras->continusconvmode,  
        .ADC_ExternalTrigConv = dev->paras->extrigconv,
        .ADC_DataAlign = dev->paras->dataalign,
        .ADC_NbrOfChannel = dev->channel_count,  
    };
    ADC_Init(dev->adc, &adc_init);
    /* 使能ADC */
    ADC_Cmd(dev->adc, ENABLE);
    /* 校准 */ 
    ADC_ResetCalibration(dev->adc);
    while (ADC_GetResetCalibrationStatus(dev->adc));
    ADC_StartCalibration(dev->adc);
    while (ADC_GetCalibrationStatus(dev->adc));
    /* 配置DMA */
    if(dev->dma_mode)   adc_dma_cfg(dev);
}

/* ==============================不使用DMA==================================== */
/**
 * @brief  单次读取某个通道的 ADC 值（会临时改变配置，适合低频使用）
 * @note   使用前确保该通道已注册
 * @retval 0:读取成功 -1：读取失败
 */
int adc_read_single(adc_dev_t *dev, uint8_t channel, uint16_t *val) {
    if (dev->channel_count == 0) { LOG_E("no channel register"); return -1; }

    int idx = -1;
    for(int i = 0; i < dev->channel_count; i++) {
        if(dev->channels[i].adc_channel == channel) {
            idx = i;
            break;
        }
    }

    if(idx < 0) { LOG_E("find no this channel"); return idx; }

    ADC_RegularChannelConfig(dev->adc, dev->channels[idx].adc_channel, 1, dev->channels[idx].sample_time);
    ADC_SoftwareStartConvCmd(dev->adc, ENABLE);
    while (!ADC_GetFlagStatus(dev->adc, ADC_FLAG_EOC));
    *val = ADC_GetConversionValue(dev->adc);
    dev->channels[idx].value = *val;

    return 0;
}

/**
 * @brief  启动一次扫描转换，并通过轮询依次读取多个通道的值
 * @param  dev      ADC 设备指针
 * @param  results  存储转换结果的数组（长度需 ≥ channel_count）
 * @return 0 成功，-1 未配置通道
 */
uint8_t adc_read_multi(adc_dev_t *dev, uint16_t *results)
{
    if (dev->channel_count == 0) { LOG_E("no channel register"); return 1; }

    /* 配置规则组通道（无论是否使用DMA，都需要）*/
    for (uint8_t i = 0; i < dev->channel_count; i++) {
        ADC_RegularChannelConfig(dev->adc,
                                 dev->channels[i].adc_channel,
                                 i + 1,
                                 dev->channels[i].sample_time);
    }
    /* 清空可能残留的 EOC 标志（读一次 DR 以防万一）*/
    ADC_GetConversionValue(dev->adc);
    /* 软件触发一次转换 */
    ADC_SoftwareStartConvCmd(dev->adc, ENABLE);

    /* 逐个读取通道结果 */
    for (uint8_t i = 0; i < dev->channel_count; i++) {
        // 等待当前通道转换完成
        while (!ADC_GetFlagStatus(dev->adc, ADC_FLAG_EOC));
        // 读取转换值（读取后 EOC 自动清零）
        results[i] = ADC_GetConversionValue(dev->adc);
    }

    // 所有通道完成后，EOC 不再置位，扫描序列结束
    return 0;
}
/* ==============================不使用DMA==================================== */

/**
 * @brief  DMA 传输完成中断处理
 * @note   需要用户在主 DMA ISR 中调用，或由具体 DMA 通道 ISR 调用
 */
void adc_dma_isr_handler(adc_dev_t *dev) {
    if (DMA_GetITStatus(dev->dma_tc_flag)) {
        DMA_ClearITPendingBit(dev->dma_tc_flag);
        /* 将 DMA 缓冲的最新值更新到各通道 */
        for (int i = 0; i < dev->channel_count; i++) {
            dev->channels[i].value = dev->dma_buf[i];
        }
    }
}







