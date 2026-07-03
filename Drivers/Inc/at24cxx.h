#ifndef __AT24CXX_H_
#define __AT24CXX_H_

#include "sys.h" 
#include "i2c.h"   
	 
#define SCL_PORT  GPIOB
#define SCL_PIN   GPIO_Pin_6
#define SDA_PORT  GPIOB
#define SDA_PIN   GPIO_Pin_7

/* STM32开发板
   24CXX驱动函数(适合24C01~24C16,24C32~256未经过测试!有待验证!)	   
   All rights reserved */
void at24cxx_onChipCfg(void);     // 初始化AT24C I2C
bool at24cxx_readByte(const i2c_2LineIO *io, u16 ReadAddr, u8 *recVal);	// 指定地址读取一个字节
bool at24cxx_writeByte(const i2c_2LineIO *io, u16 WriteAddr, u8 DataToWrite);		// 指定地址写入一个字节
bool at24cxx_writeLenBytes(const i2c_2LineIO *io, u16 WriteAddr, u32 DataToWrite, u8 Len);// 指定地址开始写入指定长度的数据
bool at24cxx_readLenBytes(const i2c_2LineIO *io, u16 ReadAddr, u8 Len, u32 *recVal);					// 指定地址开始读取指定长度数据
bool at24cxx_write(const i2c_2LineIO *io, u16 WriteAddr, u8 *pBuffer, u16 NumToWrite);	// 从指定地址开始写入指定长度的数据
bool at24cxx_read(const i2c_2LineIO *io, u16 ReadAddr, u8 *pBuffer, u16 NumToRead);   	// 从指定地址开始读出指定长度的数据
bool at24cxx_check(const i2c_2LineIO *io);  // 检查器件


#endif
















