#include "mq2_adp.h"
#include <math.h>
#include "delay.h"

#if USE_SENSOR_MQ2

/**
 * @file    mq2_adapter.c
 * @brief   MQ2 传感器适配器 —— 对接 sensor_device_t 框架
 * @note    依赖：adc（全局 adc_dev_t g_adc1）、mq2 驱动层、sensor_service
 */

/* ==================== 适配器实现 ==================== */
/* 读取传感器原始ADC数据后进行其他相关数据计算 */
static void mq2_data_process(sensor_gas_data_t *data, mq2_priv_t *mq2_priv, uint16_t *adc_raw) {
    /* 记录原始ADC值 */
    data->raw_adc = *adc_raw;
    /* 计算电压 (V) */
    float vrl = (float)(*adc_raw) / 4096.0f * MQ2_VREF;
    data->voltage = vrl;
    //LOG_D("ADC raw: %d, Voltage: %.3fV", *adc_raw, vrl);
    /* 计算传感器电阻 Rs (kΩ) */
    if (vrl > 0.01f) 
        data->rs = (MQ2_VC - vrl) * MQ2_RL / vrl;
    else 
        data->rs = 999.9f;
    /* 计算 Rs/R0 */
    if (mq2_priv->mq2_relate->r0 > 0.01f) 
        data->rs_r0_ratio = data->rs / mq2_priv->mq2_relate->r0;
    else 
        data->rs_r0_ratio = 1.0f;
    /* 计算 ppm 浓度 */
    data->concentration = pow(MQ2_SMOKE_A * 2 / data->rs, MQ2_SMOKE_B) * 100 + mq2_priv->ppm_offset;
    /* 判断是否超出浓度报警阈值 */
    data->alarm = mq2_is_alarm(data->concentration, mq2_priv->mq2_relate->alarm_threshold);
    /* 记录数据处理的时间戳 */
    data->timestamp_ms = get_tick_ms();
}


/**
 * @brief  初始化 MQ2 传感器（预热 + 标定 R0）
 */
static sensor_err_t mq2_adp_init(sensor_device_t *dev)
{
    mq2_priv_t *p = (mq2_priv_t *)dev->priv;
    mq2_rel_t *rel = p->mq2_relate;
    /* 1. 初始化硬件层关联参数 */
    mq2_driver_init(rel);

    /* 3. 在洁净空气中标定 R0 */
    if(mq2_calibrate_r0(rel)) {
        //LOG_E("failed to calibrate r0 for MQ-2");
        return SENSOR_ERR_INIT_FAIL;
    }
    //LOG_I("MQ-2 R0 calibrated = %.2f k", rel->r0);

    return SENSOR_OK;
}

/**
 * @brief  读取 MQ2 气体浓度
 */
static sensor_err_t mq2_adp_read(sensor_device_t *dev, sensor_data_t *data)
{
    uint16_t raw_adc = 0;
    mq2_priv_t *p = (mq2_priv_t *)dev->priv;

    /* 执行一次完整采集（从 ADC 取值、计算 ppm 等） */
    mq2_read(p->mq2_relate, &raw_adc);

    /* 填充传感器数据联合体 */
    mq2_data_process(&data->gas, p, &raw_adc);

    return SENSOR_OK;
}

/* ==================== 操作集定义 ==================== */
static const sensor_ops_t mq2_ops = {
    .name      = "MQ-2",
    .type      = SENSOR_TYPE_GAS,
    .init      = mq2_adp_init,
    .read      = mq2_adp_read,
    .deinit    = NULL,
    .self_test = NULL,
    .sleep     = NULL,
    .wakeup    = NULL,
};

static mq2_priv_t s_mq2_priv = {
    .mq2_relate = &mq2_rel,
    .ppm_offset = 0.0f,
};

static sensor_policy_t s_mq2_policy = {
    .cache_ms    = 800U,             // 慢响应，缓存 1.8 秒
    .max_retry   = 2,                // 允许重试 2 次
    .max_err_cnt = 4,                // 连续 5 次错误后离线
};

static sensor_device_t s_mq2_dev = {
    .ops    = &mq2_ops,
    .type   = SENSOR_TYPE_GAS,
    .priv   = &s_mq2_priv,
    .policy = &s_mq2_policy,
};

/* ==================== 注册接口 ==================== */
void mq2_adapter_register(void) {
    sensor_register(&s_mq2_dev);
}

#endif /* #if USE_SENSOR_MQ2 */

