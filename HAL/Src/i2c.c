#include "i2c.h"
#include "debug_log.h"

static __inline void i2c_gpio_cfg(const periph_gpio_t *gpio) 
{
    io_set(gpio);
}

static void i2c_paras_cfg(I2C_TypeDef *instance, const i2c_paras_t *paras) {
    if(paras == NULL) return;
    I2C_InitTypeDef I2C_InitStruct;
    /* 开启I2C外设时钟 */
    RCC_APB1PeriphClockCmd(((instance == I2C1) ? RCC_APB1Periph_I2C1 : RCC_APB1Periph_I2C2), ENABLE);
    /* 配置I2C外设 */
    I2C_InitStruct.I2C_ClockSpeed = paras->clkSpeed;
    I2C_InitStruct.I2C_Mode = paras->mode;
    I2C_InitStruct.I2C_DutyCycle = paras->dutyCycle;
    I2C_InitStruct.I2C_Ack = paras->is_ackEnable;
    I2C_InitStruct.I2C_AcknowledgedAddress = paras->acknowledgeAddr;
    I2C_InitStruct.I2C_OwnAddress1 = paras->ownAddr1; 
    /* 初始化I2C */
    I2C_Init(instance, &I2C_InitStruct);
}

void i2c_onChipPeriph_cfg(const i2c_cfg_t *cfg) {
    /* I2C SCL SDA对应GPIO初始化 */
    i2c_gpio_cfg(cfg->gpio);
    if(cfg->achway == I2C_ACHWAY_SOFTWARE) return; // 如果是软件I2C的话就在初始化GPIO后退出
    /* 将I2C外设的寄存器环境全部重置 */
    I2C_DeInit(cfg->instance);
    /* I2C 对应参数初始化 */
    i2c_paras_cfg(cfg->instance, cfg->paras);
    /* 使能I2C */
    I2C_Cmd(cfg->instance, ENABLE);
}

/**
 * @brief  通过硬件 I2C 发送一串数据
 * @param  I2Cx     使用的I2C外设
 * @param  addr     7位设备地址
 * @param  pData    数据指针
 * @param  len      数据长度
 * @param  timeout  超时时间(tick)
 * @return 0 成功，1 超时或错误
 */
u8 i2c_writebuffer(I2C_TypeDef *I2Cx, uc8 addr, uc8 *pData, u8 len, uc32 exp_timeout)
{
    I2C_AcknowledgeConfig(I2Cx, ENABLE);

    u32 timeout = exp_timeout;
    // 等待总线空闲
    while (I2C_GetFlagStatus(I2Cx, I2C_FLAG_BUSY)) {
        if (--timeout == 0) { 
            //LOG_D("a"); 
            return 1; 
        }
    }

    // 发送起始信号
    I2C_GenerateSTART(I2Cx, ENABLE);
    timeout = exp_timeout;
    while (!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_MODE_SELECT)) {
        if (--timeout == 0) { 
            //LOG_D("b"); 
            return 1; 
        }
    }

    // 发送7位地址（写方向）
    I2C_Send7bitAddress(I2Cx, addr << 1, I2C_Direction_Transmitter);
    timeout = exp_timeout;
    while (!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) {
        if (--timeout == 0) { 
            //LOG_D("c"); 
            return 1; 
        }
    }

    // 逐个发送数据
    while (len--) {
        I2C_SendData(I2Cx, *pData++);
        timeout = exp_timeout;
        while (!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_TRANSMITTING)) {
            if (--timeout == 0) { 
                //LOG_D("d"); 
                return 1; 
            }
        }
    }

    // 发送停止信号
    I2C_GenerateSTOP(I2Cx, ENABLE);
    return 0;
}

/**
 * @brief  通过硬件 I2C 读取一串数据
 * @param  addr  7位设备地址
 * @param  pData 接收缓冲区
 * @param  len   要读取的字节数
 * @return 0 成功，1 超时或错误
 */
u8 i2c_readbuffer(I2C_TypeDef *I2Cx, uc8 addr, u8 *pData, u8 len, uc32 exp_timeout)
{
    u32 timeout = exp_timeout;

    // 等待总线空闲
    while (I2C_GetFlagStatus(I2Cx, I2C_FLAG_BUSY)) {
        if (--timeout == 0) return 1;
    }

    // 发送起始信号
    I2C_GenerateSTART(I2Cx, ENABLE);
    timeout = exp_timeout;
    while (!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_MODE_SELECT)) {
        if (--timeout == 0) return 1;
    }

    // 发送7位地址（读方向）
    I2C_Send7bitAddress(I2Cx, addr << 1, I2C_Direction_Receiver);
    timeout = exp_timeout;
    while (!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED)) {
        if (--timeout == 0) return 1;
    }

    while (len) {
        if (len == 1) {
            // 最后一个字节：关闭应答并产生停止条件
            I2C_AcknowledgeConfig(I2Cx, DISABLE);
            I2C_GenerateSTOP(I2Cx, ENABLE);
        }

        // 等待数据接收
        timeout = exp_timeout;
        while (!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_RECEIVED)) {
            if (--timeout == 0) return 1;
        }
        *pData++ = I2C_ReceiveData(I2Cx);
        len--;
    }

    // 重新开启应答（不影响下次通信）
    I2C_AcknowledgeConfig(I2Cx, ENABLE);
    return 0;
}


/* ==========================软件I2C API实现=========================== */
/**
 * @brief  I2C起始信号: SCL高时，SDA从高到低跳变
 */
void I2C_Start(const i2c_2LineIO *io)
{
    gpio_port scl_port = io->scl_port, sda_port = io->sda_port;
    u8 scl_pinNum = io_getPinNumber(io->scl_pin), sda_pinNum = io_getPinNumber(io->sda_pin);

    SDA_OUT(sda_port, sda_pinNum);  // SDA配置为输出
    SDA_OP(sda_port, sda_pinNum, HIGH);
    SCL_OP(scl_port, scl_pinNum, HIGH);
    delay_us(4);
    SDA_OP(sda_port, sda_pinNum, LOW);        // 起始条件：SCL高时，SDA下降沿
    delay_us(4);
    SCL_OP(scl_port, scl_pinNum, LOW);        // 钳住总线，准备发送数据
}

/**
 * @brief  I2C停止信号: SCL高时，SDA从低到高跳变
 */
void I2C_Stop(const i2c_2LineIO *io)
{
    gpio_port scl_port = io->scl_port, sda_port = io->sda_port;
    u8 scl_pinNum = io_getPinNumber(io->scl_pin), sda_pinNum = io_getPinNumber(io->sda_pin);
#if 0
    SDA_OUT(sda_port, sda_pinNum);
    SDA_OP(sda_port, sda_pinNum, LOW);
    SCL_OP(scl_port, scl_pinNum, HIGH);
    delay_us(4);
    SDA_OP(sda_port, sda_pinNum, HIGH);   // 停止条件：SCL高时，SDA上升沿
    delay_us(4);
#else
    SDA_OUT(sda_port, sda_pinNum);//sda线输出
	SCL_OP(scl_port, scl_pinNum, LOW);
	SDA_OP(sda_port, sda_pinNum, LOW);   //STOP:when CLK is high DATA change form low to high
 	delay_us(4);
	SCL_OP(scl_port, scl_pinNum, HIGH); 
	SDA_OP(sda_port, sda_pinNum, HIGH);   //发送I2C总线结束信号
	delay_us(4);			
#endif
}

/**
 * @brief  等待从机应答信号
 * @retval 0: 收到应答(ACK)   1: 未收到应答(NACK)
 */
u8 I2C_WaitACK(const i2c_2LineIO *io)
{
    gpio_port scl_port = io->scl_port, sda_port = io->sda_port;
    u8 scl_pinNum = io_getPinNumber(io->scl_pin), sda_pinNum = io_getPinNumber(io->sda_pin);
    u32 readVal;
#if 0
    u16 timeout = 0x0FFF;
    SDA_IN(sda_port, sda_pinNum);   // SDA切换为输入
    SCL_OP(scl_port, scl_pinNum, HIGH);
    delay_us(1);
    while(--timeout != 0) {
        READ_SDA(sda_port, sda_pinNum, readVal);
        if(readVal == 0) {
            SCL_OP(scl_port, scl_pinNum, LOW);
            return 0; // 收到应答
        }
    }
    I2C_Stop(io);
    return 0;  // 未收到应答
#else 
    u8 ucErrTime = 0;
	SDA_IN(sda_port, sda_pinNum);      //SDA设置为输入  
	SDA_OP(sda_port, sda_pinNum, HIGH);
    delay_us(1);	   
	SCL_OP(scl_port, scl_pinNum, HIGH);
    delay_us(1);	 
	while(1) {
		ucErrTime++;
		if(ucErrTime > 250) { I2C_Stop(io); return 1; }
        READ_SDA(sda_port, sda_pinNum, readVal);
        if(readVal == 0) {
            SCL_OP(scl_port, scl_pinNum, LOW);  // 时钟输出0 	   
	        return 0;
        }
	}	  
#endif 
}

/**
 * @brief  主机发送应答信号(ACK)
 */
void I2C_GenerateACK(const i2c_2LineIO *io)
{
    gpio_port scl_port = io->scl_port, sda_port = io->sda_port;
    u8 scl_pinNum = io_getPinNumber(io->scl_pin), sda_pinNum = io_getPinNumber(io->sda_pin);
#if 0
    SDA_OUT(sda_port, sda_pinNum);
    SDA_OP(sda_port, sda_pinNum, LOW);   // 拉低SDA表示ACK
    SCL_OP(scl_port, scl_pinNum, HIGH);
    delay_us(2);
    SCL_OP(scl_port, scl_pinNum, LOW);
    SDA_OP(sda_port, sda_pinNum, HIGH);  // 释放SDA
#else
    SCL_OP(scl_port, scl_pinNum, LOW);
	SDA_OUT(sda_port, sda_pinNum);
	SDA_OP(sda_port, sda_pinNum, LOW);
	delay_us(2);
	SCL_OP(scl_port, scl_pinNum, HIGH);
	delay_us(2);
	SCL_OP(scl_port, scl_pinNum, LOW);
#endif
}

/**
 * @brief  主机发送非应答信号(NACK)
 */
void I2C_NGenerateACK(const i2c_2LineIO *io)
{
    gpio_port scl_port = io->scl_port, sda_port = io->sda_port;
    u8 scl_pinNum = io_getPinNumber(io->scl_pin), sda_pinNum = io_getPinNumber(io->sda_pin);
#if 0
    SDA_OUT(sda_port, sda_pinNum);
    SDA_OP(sda_port, sda_pinNum, HIGH);  // 拉高SDA表示NACK
    SCL_OP(scl_port, scl_pinNum, HIGH);
    delay_us(2);
    SCL_OP(scl_port, scl_pinNum, LOW);
    SDA_OP(sda_port, sda_pinNum, LOW);   // 释放SDA（可选）
#else
    SCL_OP(scl_port, scl_pinNum, LOW);
	SDA_OUT(sda_port, sda_pinNum);
	SDA_OP(sda_port, sda_pinNum, HIGH);
	delay_us(2);
	SCL_OP(scl_port, scl_pinNum, HIGH);
	delay_us(2);
	SCL_OP(scl_port, scl_pinNum, LOW);
#endif 
}

/**
 * @brief  发送一个字节
 * @param  txd: 待发送的数据
 * @retval 0: 发送成功(收到ACK)   1: 发送失败(收到NACK)
 */
void I2C_SendByte(const i2c_2LineIO *io, u8 txd)
{
    gpio_port scl_port = io->scl_port, sda_port = io->sda_port;
    u8 scl_pinNum = io_getPinNumber(io->scl_pin), sda_pinNum = io_getPinNumber(io->sda_pin);
#if 0
    u8 i;
    SDA_OUT(sda_port, sda_pinNum);
    for (i = 0; i < 8; i++) {
        if (txd & 0x80) SDA_OP(sda_port, sda_pinNum, HIGH);
        else            SDA_OP(sda_port, sda_pinNum, LOW);
        txd <<= 1;
        SCL_OP(scl_port, scl_pinNum, HIGH);
        delay_us(2);
        SCL_OP(scl_port, scl_pinNum, LOW);
        delay_us(1);
    }
    return I2C_WaitACK(io);   // 返回应答状态
#else
    u8 t;   
	SDA_OUT(sda_port, sda_pinNum); 	    
    SCL_OP(scl_port, scl_pinNum, LOW);  // 拉低时钟开始数据传输
    for(t = 0;t < 8;t++) {              
        // SDA_OP(sda_port, sda_pinNum, ((txd&0x80)>>7));
		if((txd & 0x80) >> 7) 
            SDA_OP(sda_port, sda_pinNum, HIGH);
		else                  
            SDA_OP(sda_port, sda_pinNum, LOW);;
		txd <<= 1; 	  
		delay_us(2);   //对TEA5767这三个延时都是必须的
		SCL_OP(scl_port, scl_pinNum, HIGH);
		delay_us(2); 
		SCL_OP(scl_port, scl_pinNum, LOW);	
		delay_us(2);
    }	 
#endif 
}

/**
 * @brief  读取一个字节
 * @param  ack: 1-发送ACK(继续读), 0-发送NACK(停止读)
 * @retval 读取到的字节
 */
void I2C_ReadByte(const i2c_2LineIO *io, u8 *rec, u8 ack)
{
    gpio_port scl_port = io->scl_port, sda_port = io->sda_port;
    u8 scl_pinNum = io_getPinNumber(io->scl_pin), sda_pinNum = io_getPinNumber(io->sda_pin);
    u32 readVal;
#if 0
    u8 i, recv = 0;
    SDA_IN(sda_port, sda_pinNum);
    for (i = 0; i < 8; i++) {
        SCL_OP(scl_port, scl_pinNum, HIGH);
        delay_us(1);
        READ_SDA(sda_port, sda_pinNum, readVal);
        recv = (recv << 1) | readVal;
        SCL_OP(scl_port, scl_pinNum, LOW);
        delay_us(1);
    }
    if (ack) I2C_GenerateACK(io);    // 发送ACK
    else     I2C_NGenerateACK(io);   // 发送NACK
    return recv;
#else
    u8 i = 0;
    *rec = 0x00;
	SDA_IN(sda_port, sda_pinNum); // SDA设置为输入
    for(i = 0; i < 8; i++) {
        SCL_OP(scl_port, scl_pinNum, LOW); 
        delay_us(2);
		SCL_OP(scl_port, scl_pinNum, HIGH);
        *rec <<= 1;
        READ_SDA(sda_port, sda_pinNum, readVal);
        if(readVal) (*rec)++;   
		delay_us(1); 
    }					 
    if (ack) I2C_GenerateACK(io);  // 发送ACK
    else     I2C_NGenerateACK(io); // 发送NACK   
#endif 
}


