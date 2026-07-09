#include "aht20.h"
#include "delay.h"
#include "gpio.h"

static uint8_t aht20_addr = 0x00;  // 保存当前设备地址

#if USE_SENSOR_AHT20

static const periph_gpio_t aht20_i2c_gpio = {
    .port  = AHT20_I2C_PORT,
    .pin   = AHT20_I2C_SCL_PIN | AHT20_I2C_SDA_PIN,
#if AHT20_I2C_ACHIVE_WAY == 0
    .mode  = AHT20_I2C_SW_GPIO_MODE,
    .initial_level = highLevel,
    .speed = GPIO_Speed_50MHz,
#elif AHT20_I2C_ACHIVE_WAY == 1
    .mode  = AHT20_I2C_HW_GPIO_MODE
#endif
};

    #if AHT20_I2C_ACHIVE_WAY == 0   /* 软件I2C */

static const i2c_cfg_t aht20_i2c_cfg = {
    .instance   = AHT20_I2Cx,
    .achway     = I2C_ACHWAY_SOFTWARE,
    .gpio       = &aht20_i2c_gpio,
};

static const i2c_2LineIO aht20_i2c_io = {
    .scl_port = AHT20_I2C_PORT, .scl_pin = AHT20_I2C_SCL_PIN,
    .sda_port = AHT20_I2C_PORT, .sda_pin = AHT20_I2C_SDA_PIN
};

static uint8_t aht20_readStatus() 
{
    uint8_t firstByte, flag;	
	I2C_Start(&aht20_i2c_io);
    I2C_SendByte(&aht20_i2c_io, AHT20_READ_ADDR);
	flag = I2C_WaitACK(&aht20_i2c_io);
	I2C_ReadByte(&aht20_i2c_io, &firstByte, flag);
	I2C_NGenerateACK(&aht20_i2c_io);
	I2C_Stop(&aht20_i2c_io);

	return firstByte;
}

/**
 * @brief  初始化 AHT20 驱动（软复位 + 记录地址）
 * @param  i2c_addr  7 位设备地址
 * @return uint8_t  0:成功 1:失败
 */
uint8_t aht20_driver_init(uint8_t i2c_addr)
{
    aht20_addr = i2c_addr;          // 记录设备地址

    /* 首先读取状态字bit[3]，如果为1,为校准输出，无须初始化!!! 正常情况下读回来的状态是0x1C或者是0x18,读回来是0x80表示忙状态 */
    if (aht20_readStatus() & 0x08)                  
        return 0;  // 无需初始化
    
    return 0;
}


/**
 * @brief  校准 AHT20 传感器（发送 0xBE 命令）
 * @return uint8_t  0 成功， 1 校准失败
 */
uint8_t aht20_driver_calibrate(void)
{
    /* 进行初始化 */   
    SCL_OUT(aht20_i2c_io.scl_port, aht20_i2c_io.scl_pin);
    SDA_OUT(aht20_i2c_io.scl_port, aht20_i2c_io.scl_pin);

	I2C_Start(&aht20_i2c_io);
	I2C_SendByte(&aht20_i2c_io, AHT20_WRITE_ADDR);
	I2C_WaitACK(&aht20_i2c_io);
	I2C_SendByte(&aht20_i2c_io, 0xBE); // 0xBE初始化命令，AHT20的初始化命令是0xBE, AHT10的初始化命令是0xE1
	I2C_WaitACK(&aht20_i2c_io);
	I2C_SendByte(&aht20_i2c_io, 0x08); // 相关寄存器bit[3]置1，为校准输出
	I2C_WaitACK(&aht20_i2c_io);
	I2C_SendByte(&aht20_i2c_io, 0x00);
	I2C_WaitACK(&aht20_i2c_io);
	I2C_Stop(&aht20_i2c_io);

	delay_ms(10); // 延时10ms左右
    return 0;
}

/**
 * @brief  触发 AHT20 温湿度测量
 * @return uint8_t  0 成功
 */
uint8_t aht20_driver_trigger_measure(void)
{
    I2C_Start(&aht20_i2c_io);
    I2C_SendByte(&aht20_i2c_io, AHT20_WRITE_ADDR);  // 写地址
    I2C_WaitACK(&aht20_i2c_io);
    I2C_SendByte(&aht20_i2c_io, 0xAC);  // 触发测量命令
    I2C_WaitACK(&aht20_i2c_io);
    I2C_SendByte(&aht20_i2c_io, 0x33);
    I2C_WaitACK(&aht20_i2c_io);
    I2C_SendByte(&aht20_i2c_io, 0x00);
    I2C_WaitACK(&aht20_i2c_io);
    I2C_Stop(&aht20_i2c_io);

    return 0;
}

/**
 * @brief  读取 AHT20 原始温湿度数据
 * @param  temp  输出原始温度值 (20 位)
 * @param  humi  输出原始湿度值 (20 位)
 * @return uint8_t   0 成功， 1 通信异常
 */
uint8_t aht20_driver_read_raw(uint32_t *temp, uint32_t *humi)
{
    uint8_t flag;
    uint8_t buf[6];
    uint16_t cnt = 0;

    if (!temp || !humi)
        return 1;

    while(aht20_readStatus() & 0x80)//直到状态bit[7]为0，表示为空闲状态，若为1，表示忙状态
	{
		delay_ms(1);
		if(cnt++ >= 100) break;
	}

    I2C_Start(&aht20_i2c_io);
    I2C_SendByte(&aht20_i2c_io, AHT20_READ_ADDR);

    flag = I2C_WaitACK(&aht20_i2c_io);
    for(uint8_t i = 0; i < 6; i++) {
        I2C_ReadByte(&aht20_i2c_io, (uint8_t*)(buf + i), i < 5 ? !flag : flag);
    } 
	I2C_Stop(&aht20_i2c_io);

    // 拼接原始湿度
    uint32_t raw_hum = ((uint32_t)buf[1] << 12) |
                       ((uint32_t)buf[2] << 4) |
                       ((uint32_t)buf[3] >> 4);

    // 拼接原始温度 (低 4 位 + 中 8 位 + 低 8 位)
    uint32_t raw_temp = ((uint32_t)(buf[3] & 0x0F) << 16) |
                        ((uint32_t)buf[4] << 8) |
                        ((uint32_t)buf[5]);   

    *temp = raw_temp;
    *humi = raw_hum;

    return 0;
}

    #elif AHT20_I2C_ACHIVE_WAY == 1   /* 硬件I2C */

static const i2c_paras_t aht20_i2c_hw_para = {
    .clkSpeed        = I2C_CLKSPEED_STANDARD,
    .mode            = I2C_Mode_I2C,
    .dutyCycle       = I2C_DutyCycle_2,
    .is_ackEnable    = I2C_Ack_Enable,
    .acknowledgeAddr = I2C_AcknowledgedAddress_7bit,
    .ownAddr1        = 0xB7,
};

static const i2c_cfg_t aht20_i2c_cfg = {
    .instance   = AHT20_I2Cx,
    .achway     = I2C_ACHWAY_HARDWARE,
    .gpio       = &aht20_i2c_gpio,
    .paras      = &aht20_i2c_hw_para
};

static uint8_t aht20_i2c_writebuffer(uc8 *pData, uint8_t len) 
{
    if(i2c_writebuffer(AHT20_I2Cx, aht20_addr, pData, len, I2C_TIMEOUT) != 0) 
        return 1;
    return 0;
}

static uint8_t aht20_i2c_readbuffer(uint8_t *pData, uint8_t len) 
{
    if(i2c_readbuffer(AHT20_I2Cx, aht20_addr, pData, len, I2C_TIMEOUT) != 0) 
        return 1;        
    return 0;
}

/**
 * @brief  AHT20 初始化（包含上电稳定、软复位、不包含校准）
 * @param  i2c_addr: 7位 I2C 地址（通常为 0x38）
 * @retval 0: 成功  1: 失败
 */
uint8_t aht20_driver_init(uint8_t i2c_addr)
{
    aht20_addr = i2c_addr;
    
    // 软复位
    uint8_t cmd = 0xBA;
    if (aht20_i2c_writebuffer(&cmd, 1) != 0)
        return 1;

    delay_ms(80);
    return 0;
}

uint8_t aht20_driver_calibrate(void)
{
    uint8_t status;
    const uint8_t cmd[3] = {0xBE, 0x08, 0x00}; // 校准命令

    // 1. 读取状态字
    if (aht20_i2c_readbuffer(&status, 1) != 0)
        return 1;

    // 2. 如果已校准 (Bit3=1) ，直接返回成功
    if (status & 0x08)
        return 0;

    // 3. 发送校准命令：0xBE 0x08 0x00
    if (aht20_i2c_writebuffer(cmd, 3) != 0)
        return 1;

    delay_ms(10);

    // 4. 再次确认状态
    if (aht20_i2c_readbuffer(&status, 1) != 0)
        return 1;

    return (status & 0x08) ? 0 : 1;
}

uint8_t aht20_driver_trigger_measure(void)
{
    uint8_t cmd[3] = {0xAC, 0x33, 0x00};

    if (aht20_i2c_writebuffer(cmd, 3) != 0) 
        return 1;
    return 0;
}

uint8_t aht20_driver_read_raw(uint32_t *temp, uint32_t *humi)
{
    uint8_t buf[6];

    if (!temp || !humi)
        return 1;

    /* 连续读出 6 字节数据 */
    if (aht20_i2c_readbuffer(buf, 6) != 0)
        return 1;

    /* 检查数据有效性（状态字不应包含忙标志）*/
    if (buf[0] & 0x80)  return 1;

    /* 拼接原始湿度 (20位) */
    uint32_t raw_hum = ((uint32_t)buf[1] << 12) |
                       ((uint32_t)buf[2] << 4) |
                       ((uint32_t)buf[3] >> 4);

    /* 拼接原始温度 (20位) */
    uint32_t raw_temp = ((uint32_t)(buf[3] & 0x0F) << 16) |
                        ((uint32_t)buf[4] << 8) |
                        ((uint32_t)buf[5]);

    *temp = raw_temp;
    *humi = raw_hum;

    return 0;
}

    #endif  /* #if AHT20_I2C_ACHIVE_WAY == 0 */
   

void aht20_onChip_init(void)
{
    i2c_onChipPeriph_cfg(&aht20_i2c_cfg);
}

#endif  /* #if USE_SENSOR_AHT20 */ 
   


