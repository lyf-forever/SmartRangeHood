#ifndef __AHT20_H_
#define __AHT20_H_

#include "i2c.h"

#if USE_SENSOR_AHT20 

#define AHT20_I2C_ADDR  0x38     /* AHT20从机地址 */

#define AHT20_I2C_PORT           GPIOB
#define AHT20_I2C_SCL_PIN        GPIO_Pin_10
#define AHT20_I2C_SDA_PIN        GPIO_Pin_11
#define AHT20_I2C_CLK            RCC_APB1Periph_I2C2
#define AHT20_I2Cx               I2C2

    #if AHT20_I2C_ACHIVE_WAY == 0   /* 软件I2C */

/* ----------------------- 宏定义---------------------------- */
#define AHT20_I2C_SW_GPIO_MODE   GPIO_Mode_Out_OD 

#define AHT20_READ_ADDR    0x71
#define AHT20_WRITE_ADDR   0x70

/* ==================== 引脚操作宏（开漏模式） ==================== */
#define SCL_H       GPIO_SetBits(AHT20_I2C_PORT, AHT20_I2C_SCL_PIN)
#define SCL_L       GPIO_ResetBits(AHT20_I2C_PORT, AHT20_I2C_SCL_PIN)
#define SDA_H       GPIO_SetBits(AHT20_I2C_PORT, AHT20_I2C_SDA_PIN)
#define SDA_L       GPIO_ResetBits(AHT20_I2C_PORT, AHT20_I2C_SDA_PIN)
#define SDA_READ    GPIO_ReadInputDataBit(AHT20_I2C_PORT, AHT20_I2C_SDA_PIN)

/* ------------------------------ 函数声明 ------------------------------------ */
void    aht20_i2c_init(void);
void    aht20_i2c_start(void);
void    aht20_i2c_stop(void);
uint8_t aht20_i2c_send_byte(uint8_t data);
uint8_t aht20_i2c_read_byte(uint8_t ack);   /* ack=1 主机发送ACK，0 发送NAK */

    #elif AHT20_I2C_ACHIVE_WAY == 1  /* 硬件I2C */

/* ----------------------- 宏定义---------------------------- */
#define AHT20_I2C_HW_GPIO_MODE   GPIO_Mode_AF_OD 
#define I2C_TIMEOUT              100000   /* 超时计数器 */

/* 变量外部声明 - extern */


/* ------------------------------ 函数声明 ------------------------------------ */
void aht20_onChip_init(void);

    #endif   /* #if AHT20_I2C_ACHIVE_WAY == 0 */

void aht20_onChip_init(void);
uint8_t aht20_driver_init(uint8_t i2c_addr);
uint8_t aht20_driver_trigger_measure(void);
uint8_t aht20_driver_read_raw(uint32_t *temp, uint32_t *humi);

#endif

#endif

