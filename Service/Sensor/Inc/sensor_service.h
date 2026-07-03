#ifndef __SENSOR_SERVICE_H_
#define __SENSOR_SERVICE_H_
 
#include "sensor_interface.h"

#define   MAX_SENSORS         8         /* 支持注册的最大传感器数目 */
// #define   SENSOR_CACHE_MS     100       /* 100ms内不重复读取数据 */
// #define   SENSOR_MAX_RETRY    3         /* 单次读最多重试 3 次 */
// #define   SENSOR_MAX_ERR_CNT  10        /* 连续错误超过则标记离线 */

#define SENSOR_TH_DATA_TEMP_LOWEST       5.0f
#define SENSOR_TH_DATA_TEMP_LARGEST      78.0f
#define SENSOR_TH_DATA_HUMI_LOWEST       12.0f
#define SENSOR_TH_DATA_HUMI_LARGEST      80.0f

/* API声明 */
sensor_err_t sensor_register(sensor_device_t *dev);
sensor_device_t *sensor_find_by_type(sensor_type_t type);
sensor_err_t sensor_init(sensor_device_t *dev);
sensor_err_t sensor_read(sensor_device_t *dev,sensor_data_t *out);

#endif // !__SENSOR_SERVICE_H_

