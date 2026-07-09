#ifndef __MQ2_ADP_H_
#define __MQ2_ADP_H_

#include "sys.h"
#include "mq2.h"
#include "sensor_service.h"

#if USE_SENSOR_MQ2

typedef struct 
{
    mq2_rel_t  *mq2_relate;  /* 关联的MQ2结构体 */
    float      ppm_offset;   /* 烟雾浓度（ppm）补偿偏移 */
} mq2_priv_t;

/* API declare */
void mq2_adapter_register(void);

#endif /* #if USE_SENSOR_MQ2 */

#endif /* #ifndef __MQ2_ADP_H_ */

