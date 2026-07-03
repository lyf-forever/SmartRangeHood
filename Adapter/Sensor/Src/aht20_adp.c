#include "aht20_adp.h"
#include "aht20.h"
#include "delay.h"
#include "debug_log.h"

#if USE_SENSOR_AHT20

static sensor_err_t aht20_adp_init(sensor_device_t *dev) {
    aht20_priv_t *p = (aht20_priv_t *)dev->priv;

    delay_ms(40); // 上电后需要延时40ms才可以读取状态
    if(aht20_driver_init(p->i2c_addr) != 0) 
        return  SENSOR_ERR_INIT_FAIL; 

    delay_ms(10);
    if(aht20_driver_calibrate() != 0)
        return  SENSOR_ERR_INIT_FAIL;
    return  SENSOR_OK;
}

static sensor_err_t aht20_adp_read(sensor_device_t *dev, sensor_data_t *data) {
    aht20_priv_t *p = (aht20_priv_t *)dev->priv;
    uint32_t raw_t, raw_h = 0;

    if(aht20_driver_trigger_measure() != 0) 
        return  SENSOR_ERR_READ_FAIL;
    delay_ms(80);    
    if(aht20_driver_read_raw(&raw_t, &raw_h) != 0)
        return  SENSOR_ERR_READ_FAIL;

    /* AHT20 20bit ADC */
    float t = ((float)raw_t / (1 << 20)) * 200.0f - 50.0f;
    float h = ((float)raw_h / (1 << 20)) * 100.0f;

    data->th.raw_temp     = (int32_t)raw_t;
    data->th.raw_hum      = (int32_t)raw_h;
    data->th.temperature  = t + p->temp_offset;
    data->th.humidity     = h + p->hum_offset;
    data->th.timestamp_ms = get_tick_ms();

    return SENSOR_OK;
}

static const sensor_ops_t aht20_ops = {
    .name      = "AHT20",
    .type      = SENSOR_TYPE_TEMP_HUMIDITY,
    .init      = aht20_adp_init,
    .read      = aht20_adp_read,
};

static aht20_priv_t s_aht20_priv = {
    .i2c_addr    = AHT20_I2C_ADDR,
    .temp_offset = 0.0f,
    .hum_offset  = 0.0f,
};

static sensor_policy_t s_aht20_policy = {
    .cache_ms    = 800U,
    .max_retry   = 2,
    .max_err_cnt = 4,
};

static sensor_device_t s_aht20_dev = {
    .ops    = &aht20_ops,
    .type   = SENSOR_TYPE_TEMP_HUMIDITY,
    .priv   = &s_aht20_priv,
    .policy = &s_aht20_policy,
};

void aht20_adapter_register(void) {
    sensor_register(&s_aht20_dev);
}

#endif /* #if USE_SENSOR_AHT20 */


