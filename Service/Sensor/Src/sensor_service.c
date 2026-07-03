#include "sensor_service.h"
#include <stdio.h>
#include "debug_log.h"
#include "sys.h"

static sensor_device_t *g_sensors[MAX_SENSORS];
static uint8_t g_sensor_count = 0;

sensor_err_t sensor_register(sensor_device_t *dev) {
    if(dev == NULL || dev->ops == NULL) 
        return  SENSOR_ERR_NULL_PTR;
    if(g_sensor_count >= MAX_SENSORS) 
        return  SENSOR_ERR_INIT_FAIL;

    g_sensors[g_sensor_count++] = dev;
    dev->status = SENSOR_STATUS_UNINIT;
    
    return  SENSOR_OK;
}

sensor_device_t *sensor_find_by_type(sensor_type_t type) {
    for(uint8_t i = 0; i < g_sensor_count; i++) {
        if(g_sensors[i]->type == type) 
            return  g_sensors[i];
    }
    return NULL;
}

sensor_err_t sensor_init(sensor_device_t *dev) {
    if(!dev || !dev->ops || !dev->ops->init)
        return  SENSOR_ERR_NULL_PTR;

    sensor_err_t ret = dev->ops->init(dev);
    if(ret != SENSOR_OK) {
        dev->status = SENSOR_STATUS_ERROR;
        dev->err_count ++;
        //LOG_E("sensor [%s] init failed: %d", dev->ops->name, ret);
        return  SENSOR_ERR_INIT_FAIL;
    }

    dev-> status    =  SENSOR_STATUS_READY;
    dev-> err_count  =  0;
    //LOG_I("sensor [%s] init ok", dev->ops->name);

    return  SENSOR_OK;
}

sensor_err_t sensor_read(sensor_device_t *dev,sensor_data_t *out) {
    if(!dev || !dev->ops || !dev->ops->read || !out)
        return  SENSOR_ERR_NULL_PTR;

#if 1
    if(dev->status == SENSOR_STATUS_OFFLINE) {
        //LOG_E("sensor [%s] read failed,due to the sensor offline", dev->ops->name);
        return  SENSOR_ERR_NOT_READY;
    }
#endif

    if(dev->status != SENSOR_STATUS_READY) {
        //LOG_E("sensor [%s] is unready along with read failure", dev->ops->name);
        return  SENSOR_ERR_NOT_READY;
    }
        
    /* —— 缓存:防止业务层一个循环里多次调用打爆 I2C (策略可关) —— */
    uint32_t now = get_tick_ms();
    if(dev->policy->cache_ms > 0 && now - dev->last_read_ms < dev->policy->cache_ms) {
        *out = dev->cache;
        return  SENSOR_OK;
    }

    /* —— 重试(策略可关) —— */
    sensor_err_t ret = SENSOR_ERR_READ_FAIL;
    uint8_t max_attempt = dev->policy->max_retry + 1;  /* 至少读取一次 */
    for(uint8_t i = 0; i < max_attempt; i++) {
        ret = dev->ops->read(dev, out);
        if(ret == SENSOR_OK)    break;
    }

    if(ret != SENSOR_OK) {
        dev->err_count ++;
        if(dev->err_count >= dev->policy->max_err_cnt) {
            dev->status = SENSOR_STATUS_OFFLINE;
            //LOG_E("sensor [%s] offline", dev->ops->name);
        }
        return ret;
    }

    /* —— 合理性检查(在 Service 层做,因为合理范围是业务定义的) —— */
    if(dev->type == SENSOR_TYPE_TEMP_HUMIDITY) {
        if(out->th.temperature < -40.0f || out->th.temperature > 125.0f ||
            out->th.humidity < 0.0f || out->th.humidity > 100.0f) {
            return  SENSOR_ERR_INVALID_DATA;
        }
    }

    if (dev->type == SENSOR_TYPE_GAS) {
        if (out->gas.concentration < 0.0f || out->gas.concentration > 10000.0f ||
             out->gas.voltage < 0.0f || out->gas.voltage > 3.6f) {
            return SENSOR_ERR_INVALID_DATA;
        }
    }

    dev->cache        = *out;
    dev->last_read_ms = now;
    dev->err_count    = 0;

    return SENSOR_OK; 
}


