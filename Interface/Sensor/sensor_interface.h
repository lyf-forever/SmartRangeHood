#ifndef __SENSOR_INTERFACE_H_
#define __SENSOR_INTERFACE_H_

#include "sensor_types.h"

struct sensor_device;    /* 前向声明 */

typedef struct {
    const char      *name;
    sensor_type_t    type;

    sensor_err_t (*init)     (struct sensor_device *dev);
    sensor_err_t (*deinit)   (struct sensor_device *dev);
    sensor_err_t (*read)     (struct sensor_device *dev, sensor_data_t *data);
    sensor_err_t (*self_test)(struct sensor_device *dev);
    sensor_err_t (*sleep)    (struct sensor_device *dev);
    sensor_err_t (*wakeup)   (struct sensor_device *dev);
} sensor_ops_t;

typedef struct {
    uint32_t cache_ms;       /* 数据读取缓冲时间 0: 禁用缓存 */
    uint8_t  max_retry;      /* 最多重试次数 0: 不重试,只读一次 */
    uint8_t  max_err_cnt;    /* 连续错误超此值则离线 */
} sensor_policy_t;

typedef struct sensor_device {
    const sensor_ops_t  *ops;           /* 指向具体所用传感器的实现 */
    sensor_type_t        type;          /* 传感器类型 */
    sensor_status_t      status;        /* 传感器状态 */
    sensor_policy_t     *policy;        /* 设备独立策略 */
    void                *priv;          /* 适配器私有数据(I2C 地址、校准表) */
    sensor_data_t        cache;         /* 上次读到的数据 */
    uint32_t             last_read_ms;  /* 最近一次读取数据的时刻 */
    uint32_t             err_count;     /* 出错次数 */
} sensor_device_t;


#endif /* __SENSOR_INTERFACE_H_ */

