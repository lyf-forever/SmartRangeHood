#include "at24cxx.h"  
#include "delay.h"

static const periph_gpio_t at24c_gpio = { .port = SCL_PORT, .pin = SCL_PIN | SDA_PIN, .initial_level = highLevel, 
											.speed = GPIO_Speed_50MHz, .mode = GPIO_Mode_Out_PP };
static const i2c_cfg_t iCfg = { .achway = I2C_ACHWAY_SOFTWARE, .gpio = &at24c_gpio };

/* 初始化I2C接口 */
__inline void at24cxx_onChipCfg(void)
{
	i2c_onChipPeriph_cfg(&iCfg);
}

/* 在AT24CXX指定地址读出一个数据
   ReadAddr:开始读数的地址  
   返回值:bool : false - 读数据失败 true - 读取数据成功 */
bool at24cxx_readByte(const i2c_2LineIO *io, u16 ReadAddr, u8 *recVal)
{			  		  	    																 
    I2C_Start(io);  
	if(EEP_MEMCAP > (16*128-1)) {  // 如果使用的AT24C为AT24C32起步
		I2C_SendByte(io, 0xA0);    // 发送写命令
		if(I2C_WaitACK(io)) { 
			//LOG_E("The command[write] has no respond"); 
			return false; 
		}
		I2C_SendByte(io, ReadAddr>>8); // 发送高地址
		if(I2C_WaitACK(io)) { 
			//LOG_E("The command[write targetAddr(high8bits)] has no respond"); 
			return false; 
		}		 
	} else { 
		I2C_SendByte(io, 0XA0+((ReadAddr/256)<<1));   // 如果使用的AT24C为AT24C16及以下，则起步发送器件地址0XA0,写数据 	 
		if(I2C_WaitACK(io)) { 
			//LOG_E("The command[write targetAddr] has no respond"); 
			return false; 
		}
	}
    I2C_SendByte(io, ReadAddr%256);   //发送低地址
	if(I2C_WaitACK(io)) { 
		//LOG_E("The command[write targetAddr(low8bits)] has no respond"); 
		return false; 
	}	    
	I2C_Start(io);  	 	   
	I2C_SendByte(io, 0XA1);   // 进入接收模式			   
	if(I2C_WaitACK(io)) { 
		//LOG_E("The command[enter receive mode] has no respond"); 
		return false; 
	} 
    I2C_ReadByte(io, recVal, 0);		   
    I2C_Stop(io); //产生一个停止条件	
	
	return true;
}

/* 在AT24CXX指定地址写入一个数据 
	WriteAddr:写入数据的目的地址
	DataToWrite:要写入的数据 
	bool: false-写入一个数据失败，true-写入一个数据成功 */
bool at24cxx_writeByte(const i2c_2LineIO *io, u16 WriteAddr, u8 DataToWrite)
{				   	  	    																 
    I2C_Start(io);  
	if(EEP_MEMCAP > (16*128-1)) {
		I2C_SendByte(io, 0XA0);	    // 发送写命令
		if(I2C_WaitACK(io)) { 
			//LOG_E("The command[write] has no respond"); 
			return false; 
		}
		I2C_SendByte(io, WriteAddr>>8); // 发送高地址
		if(I2C_WaitACK(io)) { 
			//LOG_E("The command[write targetAddr(high8bits)] has no respond"); 
			return false; 
		}
 	} else {
		I2C_SendByte(io, 0XA0+((WriteAddr/256)<<1));   //发送器件地址0XA0,写数据
		if(I2C_WaitACK(io)) { 
			//LOG_E("The command[write targetAddr] has no respond"); 
			return false; 
		} 
	}	 
		   
    I2C_SendByte(io, WriteAddr%256);   // 发送低地址
	if(I2C_WaitACK(io)) { 
		//LOG_E("The command[write targetAddr(low8bits)] has no respond"); 
		return false; 
	} 	 										  		   
	I2C_SendByte(io, DataToWrite);     // 发送字节							   
	if(I2C_WaitACK(io)) { 
		//LOG_E("The command[write 8bits data] has no respond"); 
		return false; 
	}		    	   
    I2C_Stop(io);  // 产生一个停止条件 
	delay_ms(10);
	
	return true;
}

/* 在AT24CXX里面的指定地址开始写入长度为Len的数据
   该函数用于写入16bit或者32bit的数据.
   WriteAddr  :开始写入的地址  
   DataToWrite:数据数组首地址
   Len :要写入数据的长度(2/4 bytes)
   @retval bool: false - 写数据失败（未写满/写） true - 写全部数据成功
*/
bool at24cxx_writeLenBytes(const i2c_2LineIO *io, u16 WriteAddr, u32 DataToWrite, u8 Len)
{  	
	for(u8 t = 0; t < Len; t++)
	{
		//LOG_I("Start to write byte%d", t+1);
		if(!at24cxx_writeByte(io, WriteAddr+t,(DataToWrite>>(8*t))&0xff)) 
		{
			//LOG_E("Write byte%d failed, exit from the write process", t+1);
			//LOG_I("Totally write %d bytes until exited from the write process", t);
			return false; 
		}
		//LOG_I("Write byte %d successfully!", t+1);
	}	
	return true;
}

/* 在AT24CXX里面的指定地址开始读出长度为Len的数据
   该函数用于读出16bit或者32bit的数据.
   ReadAddr :开始读出的地址 
   返回值: bool值 true - 读取数据成功 false - 读取数据失败
   Len :要读出数据的长度2,4
*/
bool at24cxx_readLenBytes(const i2c_2LineIO *io, u16 ReadAddr, u8 Len, u32 *recVal)
{  
	u8 temp = 0;	
	for(u8 t = 0; t < Len; t++)
	{
		(*recVal) <<= 8;
		//LOG_I("Start to read byte%d", t+1);
		if(!at24cxx_readByte(io, ReadAddr+Len-t-1, &temp))
		{
			//LOG_E("Read byte%d failed, exit from the read process", t+1);
			//LOG_E("Totally read %d bytes until exited from the read process", t); 
			return false; 
		}
		//LOG_I("Read byte %d successfully!", t+1);
		(*recVal) += temp;
	}
	return true;
}

/* 检查AT24CXX是否正常
   这里用了24XX的最后一个地址(255)来存储标志字.
   如果用其他24C系列,这个地址要修改
   返回false:检测失败
   返回true:检测成功
*/
bool at24cxx_check(const i2c_2LineIO *io)
{
	u8 temp;
	//LOG_I("Start to read byte from AT24Cxx");
	if(!at24cxx_readByte(io, 255, &temp)) {   // 避免每次开机都写AT24CXX
		//LOG_E("Read one byte failed"); 
		return false;
	} 
	//LOG_I("Read byte successfully!");

	if(temp == 0X55) return true;	
	else {   //排除第一次初始化的情况
		//LOG_I("Start to write byte 0x55 into AT24Cxx Addr 0xFF");
		if(!at24cxx_writeByte(io, 255, 0X55)) { 
			//LOG_E("Write byte failed"); 
			return false; 
		}
		//LOG_I("Write byte 0x55 successfully");
		//LOG_I("Start to read byte from AT24Cxx Addr 0xFF");
	    if(!at24cxx_readByte(io, 255, &temp)) { 
			//LOG_E("Read byte failed"); 
			return false; 
		}
		//LOG_I("Read byte successfully!");	  
		if(temp==0X55) return true;
	}
	return false;											  
}

/* 	在AT24CXX里面的指定地址开始读出指定个数的数据
	ReadAddr :开始读出的地址 对24c02为0~255
	pBuffer  :数据数组首地址
	NumToRead:要读出数据的个数
	return : bool - false: 读取失败 true - 读取成功 */
bool at24cxx_read(const i2c_2LineIO *io, u16 ReadAddr, u8 *pBuffer, u16 NumToRead)
{
	u16 readLen = NumToRead;
	while(NumToRead)
	{
		//LOG_I("Start to read byte%d from Addr %x", readLen-NumToRead+1, ReadAddr);
		if(!at24cxx_readByte(io, ReadAddr++, pBuffer++)) {
			//LOG_E("Read byte%d failed, exit from the read process", readLen-NumToRead+1);
			//LOG_E("Totally read %d bytes until exited from the read process", readLen-NumToRead);
			return false;
		}
		//LOG_I("Read byte successfully");	
		NumToRead--;
	}
	return true;
}  

/*  在AT24CXX里面的指定地址开始写入指定个数的数据
	WriteAddr :开始写入的地址 对24c02为0~255
	pBuffer   :数据数组首地址
	NumToWrite:要写入数据的个数
	return : bool - false: 写入失败/部分写入 true - 写入全部数据成功 */
bool at24cxx_write(const i2c_2LineIO *io, u16 WriteAddr, u8 *pBuffer, u16 NumToWrite)
{
	u16 wrLen = NumToWrite;
	while(NumToWrite--)
	{
		//LOG_I("Start to write byte%d into Addr %x", wrLen-NumToWrite, WriteAddr);
		if(!at24cxx_writeByte(io, WriteAddr++, *pBuffer++)) {
			//LOG_E("Write byte%d failed, exit from the write process", wrLen-NumToWrite);
			//LOG_E("Totally write %d bytes until exited from the write process", wrLen-NumToWrite-1);
			return false;
		}
		//LOG_I("Write byte%d successfully!", wrLen-NumToWrite);
	}
	return true;
}
 











