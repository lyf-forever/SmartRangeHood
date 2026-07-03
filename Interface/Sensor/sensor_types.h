#ifndef __SENSOR_TYPES_H_
#define __SENSOR_TYPES_H_

#include <stdint.h>

typedef enum {
    SENSOR_TYPE_TEMP_HUMIDITY = 0,
    SENSOR_TYPE_PRESSURE,
    SENSOR_TYPE_LIGHT,
    SENSOR_TYPE_IMU,
    SENSOR_TYPE_GAS,
    SENSOR_TYPE_MAX,
} sensor_type_t;

typedef enum {
    SENSOR_STATUS_UNINIT = 0,    /* 未初始化 */
    SENSOR_STATUS_READY,         /* 正常可用 */
    SENSOR_STATUS_BUSY,          /* 正在采样 */
    SENSOR_STATUS_ERROR,         /* 出错，可恢复 */
    SENSOR_STATUS_OFFLINE,       /* 长时间错误，已下线 */
} sensor_status_t;

typedef enum {
    SENSOR_OK                =  0,
    SENSOR_ERR_NOT_FOUND     = -1,
    SENSOR_ERR_INIT_FAIL     = -2, 
    SENSOR_ERR_READ_FAIL     = -3, 
    SENSOR_ERR_TIMEOUT       = -4, 
    SENSOR_ERR_INVALID_DATA  = -5, 
    SENSOR_ERR_NOT_READY     = -6, 
    SENSOR_ERR_NULL_PTR      = -7, 
} sensor_err_t;

/* 温湿度传感器通用数据结构 */
typedef struct {
    /* 已校准、已转换为物理量的数据 —— 给业务层用 */
    float temperature;   /* °C */
    float humidity;      /* %RH */

    /* 原始 ADC 值 —— 给故障排查、二次校准用,不要丢 */
    int32_t  raw_temp;
    int32_t  raw_hum;

    uint32_t timestamp_ms;   /* 采样时间戳 */
} sensor_th_data_t;

/* 气体传感器通用数据结构 */
typedef struct {
    /* —— 物理量（已校准，供业务层直接使用） —— */
    float concentration;        /* 气体浓度 (ppm 或 %LEL，此处 MQ-2 为 ppm) */
    float voltage;              /* 传感器模块输出电压 (V) */
    float rs;                   /* 传感器敏感体电阻 (kΩ) */
    float rs_r0_ratio;          /* Rs/R0 比值 */
    int32_t raw_adc;            /* ADC 原始采样值 (0~4095) */
    uint8_t alarm;              /* 是否超过报警阈值 (1=报警) */
    uint32_t timestamp_ms;      /* 采样时间戳 (ms) */
} sensor_gas_data_t;

typedef union {
    sensor_th_data_t th;   /* 温湿度 */
    sensor_gas_data_t gas; /* 气体浓度 */
    /* 其他类型数据 */
} sensor_data_t;

#endif // !__SENSOR_TYPES_H_

