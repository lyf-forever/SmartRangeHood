#ifndef __AHT20_ADP_H_
#define __AHT20_ADP_H_

#include "sys.h"
#include "sensor_service.h"

#if USE_SENSOR_AHT20

extern tickGet_func_t  get_tick_ms;

typedef struct {
    const uint8_t i2c_addr;
    float   temp_offset;
    float   hum_offset;
} aht20_priv_t;

/* API declaration */
void aht20_adapter_register(void);

#endif 

#endif

