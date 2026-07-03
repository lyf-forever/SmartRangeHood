#ifndef __I2C_H_
#define __I2C_H_

#include "sys.h"
#include "gpio.h"
#include "delay.h"

#define I2C_RCC(i2c)   PERIPH_APB1_RCC((i2c == I2C1 ? I2C1 : I2C2))

/* ==========================软件I2C ============================ */
// IO方向设置
#define SDA_IN(PORT, PIN)    IO_IN(PORT, PIN)
#define SDA_OUT(PORT, PIN)   IO_OUT(PORT, PIN)
#define SCL_IN(PORT, PIN)    IO_IN(PORT, PIN)
#define SCL_OUT(PORT, PIN)   IO_OUT(PORT, PIN)

// IO操作函数	 
#define SCL_OP(port, pin, n)        IO_OP(port, pin, n)
#define SDA_OP(port, pin, n)        IO_OP(port, pin, n) 	 
#define READ_SDA(port, pin, val)    IO_READ(port, pin, val)

#define I2C_CHECK_EVENT(EVENT, TIMEOUT) \
    do { \
        u32 timeout = TIMEOUT; \
        while (!I2C_CheckEvent(I2C1, EVENT) && timeout > 0) { \
            delay_us(10); \
            timeout -= 10; \
        } \
        if (timeout <= 0) \
            return false; \
    } while (0)

/* ==========================软件I2C ============================ */

/* 使用硬件I2C时的可用枚举定义 */
/* 速度 */
typedef enum {
    I2C_CLKSPEED_STANDARD = 100000U,    /* 标准模式100kHz */
    I2C_CLKSPEED_FAST = 400000U         /* 快速模式400kHz */
} i2c_clkSpeed_t;  

/* I2C参数配置结构体，包含全部参数信息 */
typedef struct {
    i2c_clkSpeed_t clkSpeed;     /* I2C速度 */
    u16       mode;              /* I2C模式 */
    u16       dutyCycle;         /* 占空比 */
    u16       is_ackEnable;      /* 是否支持ACK机制 */
    u16       acknowledgeAddr;   /* I2C从机设备地址类型 */
    u8        ownAddr1;          /* 主机模式下自身地址可任意设置 */
} i2c_paras_t;

typedef enum {
    I2C_ACHWAY_SOFTWARE = 0,
    I2C_ACHWAY_HARDWARE,
} i2c_achway;

typedef struct {
    gpio_port scl_port;
    gpio_port sda_port;  
    u16 scl_pin;
    u16 sda_pin;
} i2c_2LineIO;

/* 整体配置结构体，包含全部信息 */
typedef struct {
    I2C_TypeDef          *instance;
    i2c_achway            achway;
    const periph_gpio_t  *gpio;
    const i2c_paras_t    *paras;
} i2c_cfg_t;

/* 对外公共函数声明 */
/* 硬件I2C */
void i2c_onChipPeriph_cfg(const i2c_cfg_t *cfg);
u8 i2c_writebuffer(I2C_TypeDef *I2Cx, uc8 addr, uc8 *pData, u8 len, uc32 exp_timeout);
u8 i2c_readbuffer(I2C_TypeDef *I2Cx, uc8 addr, u8 *pData, u8 len, uc32 exp_timeout);

/* 软件I2C */				 
void I2C_Start(const i2c_2LineIO *io);				//发送I2C开始信号
void I2C_Stop(const i2c_2LineIO *io);	  			//发送I2C停止信号
void I2C_SendByte(const i2c_2LineIO *io, u8 txd);	//I2C发送一个字节
void I2C_ReadByte(const i2c_2LineIO *io, u8 *rec, u8 ack);   //I2C读取一个字节
u8 I2C_WaitACK(const i2c_2LineIO *io); 				//I2C等待ACK信号
void I2C_GenerateACK(const i2c_2LineIO *io);		//I2C发送ACK信号
void I2C_NGenerateACK(const i2c_2LineIO *io);		//I2C不发送ACK信号

#endif // !__I2C_H_
