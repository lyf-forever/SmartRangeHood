/*  ============================================= ST7735S LCD 驱动源文件  ============================================== */
#include "lcd.h"
#include "tim.h"
#include "delay.h"

#if LCD_IS_USE  /* 使用LCD */

/* 背光引脚 */
#if (LCD_BL_ACHWAY == 0) /* 背光实现-GPIO */
static const periph_gpio_t lcd_ctrlGPIO_BL = {
    .port = LCD_BL_PORT,
    .pin  = LCD_BL_PIN,
    .initial_level = lowLevel,
    .mode = GPIO_Mode_Out_PP,
    .speed = GPIO_Speed_10MHz,
};

#elif (LCD_BL_ACHWAY == 1) /* 背光实现-TIM PWM */

/* 存储配置以用于亮度换算 */
static TIM_TypeDef* backlight_tim = NULL;
static u8  backlight_ch  = 0;
static u16 backlight_period = 0;

/* 背光驱动填充框架配置结构体 */
static TIM_Config_t pwm_cfg = {
        .TIMx       = BL_TIMx,    // TIM4
        .Mode       = TIM_MODE_PWM1,
        .Prescaler  = 7199,       // 72MHz / (7199+1) = 10kHz
        .Period     = 9,          // 10kHz / (9+1) = 1kHz
        .Channel    = BL_TIM_CH,  // CH4
        .OCPolarity = TIM_OC_POLARITY_HIGH,
        .Pulse      = 5,          // 50% 占空比 (5/9)
        .PrePrio    = 7,          // 由于定时器更新中断只需要处理硬件标志，不需要FreeRTOS管理和调用API，这里我们设置抢占优先级低于3
        .SubPrio    = 0, 
};

/**
 * @brief 初始化背光 PWM
 * @note  使用 TIM4_CH4 (PB9) , 频率 1kHz , 初始占空比 50%
 */
static void LCD_Backlight_Init(void)
{
    /* 1. 调用通用初始化 */
    TIM_GeneralInit(&pwm_cfg);

    /* 2. 保存句柄用于亮度设置 */
    backlight_tim    = BL_TIMx;
    backlight_ch     = BL_TIM_CH;
    backlight_period = pwm_cfg.Period;
}

/**
 * @brief 设置背光亮度百分比
 * @param percent 0~100 (0 关, 100 最亮)
 */
void LCD_Backlight_SetPercent(u8 percent)
{
    if (percent > 100) percent = 100;
    if (backlight_tim == NULL) return;

    u16 pulse = (u16)(((u32)percent * (backlight_period + 1)) / 100);
    TIM_SetPWM_Duty(backlight_tim, backlight_ch, pulse);
}

/* 背光使用的定时器TIM4中断服务函数 */
void TIM4_IRQHandler(void) {
    if(TIM_GetITStatus(TIM4, TIM_IT_Update) !=  RESET) {
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
        Timer_IRQ_Callback(TIM4);
    }
}

#endif /* #if (LCD_BL_ACHWAY == 0) */

#if (USE_LCD_CONTROLLER == 0)   /* ST7735S */

#if (TRANS_USE_DMA == 1)  /* 使用DMA */

#endif  /* #if (TRANS_USE_DMA == 1) */

/* 复位引脚 */
static const periph_gpio_t lcd_ctrlGPIO_RES = {
    .port = LCD_RES_PORT,
    .pin  = LCD_RES_PIN,
    .initial_level = highLevel,
    .mode = GPIO_Mode_Out_PP,
    .speed = GPIO_Speed_10MHz,
};

static const SPI_Paras_t lcd_spi_Paras = {
    .SPI_Mode = SPI_Mode_Master,
    .SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2,
    .SPI_Direction = SPI_Direction_2Lines_FullDuplex,
    .SPI_DataSize = SPI_DataSize_8b,
    .SPI_CPOL = SPI_CPOL_Low,
    .SPI_CPHA = SPI_CPHA_1Edge,
    .SPI_NSS = SPI_NSS_Soft,
    .SPI_FirstBit = SPI_FirstBit_MSB
};

static SPI_CS_GPIO_t lcd_spi_csGPIO = {
    .Port = LCD_CS_PORT,
    .Pin  = LCD_CS_PIN
};

static SPI_InitParam_t lcd_spi_param = {
    .SPIx = SPI1,
    .TransferMode = SPI_MODE_POLLING,
    .SPI_Paras = &lcd_spi_Paras,
    .CS_GPIO = &lcd_spi_csGPIO,   /* 软件片选引脚 */
};

/* 数据选择引脚 */
static const periph_gpio_t lcd_ctrlGPIO_DC = {
    .port = LCD_DC_PORT,
    .pin  = LCD_DC_PIN,
    .initial_level = highLevel,
    .mode = GPIO_Mode_Out_PP,
    .speed = GPIO_Speed_50MHz,
};

/* 底层：写命令（DC = 0） */
void LCD_WriteCmd(u8 cmd)
{
    LCD_DC_CMD;
    LCD_CS_LOW;
    SPI_TransmitReceiveByte(lcd_spi_param.SPIx, cmd);
    LCD_CS_HIGH;
}

/* 底层：写 8bit 数据（DC = 1） */
void LCD_WriteData8(u8 data)
{
    LCD_DC_DATA;
    LCD_CS_LOW;
    SPI_TransmitReceiveByte(lcd_spi_param.SPIx, data);
    LCD_CS_HIGH;
}

/* 底层：写 16bit 数据（DC = 1），用于颜色 */
void LCD_WriteData16(u16 data)
{
    u8 buf[2] = { (u8)(data>>8), (u8)(data & 0XFF) };
    LCD_DC_DATA;
    LCD_CS_LOW;
    SPI_Transmit(lcd_spi_param.SPIx, buf, 2);
    LCD_CS_HIGH;
}

/* 软件复位 */
void LCD_Reset(void)
{
    LCD_RES_LOW;
    delay_us(30);   // 至少拉低10us复位才有效
    LCD_RES_HIGH;
    delay_ms(400);  // 等待120ms后LCD肯定已经结束复位操作
}

void LCD_ST7735S_Init(void) {
    /* 复位LCD并等待稳定 */
    LCD_Reset();
    /* 退出睡眠 */
    LCD_WriteCmd(0x11);
    delay_ms(120);
    /* 帧率控制 */
    LCD_WriteCmd(0xB1); LCD_WriteData8(0x01); LCD_WriteData8(0x2C); LCD_WriteData8(0x2D);
    LCD_WriteCmd(0xB2); LCD_WriteData8(0x01); LCD_WriteData8(0x2C); LCD_WriteData8(0x2D);
    LCD_WriteCmd(0xB3); LCD_WriteData8(0x01); LCD_WriteData8(0x2C); LCD_WriteData8(0x2D);
    LCD_WriteData8(0x01); LCD_WriteData8(0x2C); LCD_WriteData8(0x2D);

    LCD_WriteCmd(0xB4); LCD_WriteData8(0x07); /* 反显控制 */
    /* 电源控制 */
    LCD_WriteCmd(0xC0); LCD_WriteData8(0xA2); LCD_WriteData8(0x02); LCD_WriteData8(0x84);
    LCD_WriteCmd(0xC1); LCD_WriteData8(0xC5);
    LCD_WriteCmd(0xC2); LCD_WriteData8(0x0A); LCD_WriteData8(0x00);
    LCD_WriteCmd(0xC3); LCD_WriteData8(0x8A); LCD_WriteData8(0x2A);
    LCD_WriteCmd(0xC4); LCD_WriteData8(0x8A); LCD_WriteData8(0xEE);
    LCD_WriteCmd(0xC5); LCD_WriteData8(0x0E); /* VCOM */
    /* 颜色模式：16bit RGB565 */
    LCD_WriteCmd(0x3A); LCD_WriteData8(0x05);
    /* 方向 */
    LCD_WriteCmd(0x36); LCD_WriteData8(0xC0);
    /* Gamma 正向 */
    LCD_WriteCmd(0xE0);
    LCD_WriteData8(0x0F); LCD_WriteData8(0x1A); LCD_WriteData8(0x0F); LCD_WriteData8(0x18);
    LCD_WriteData8(0x2F); LCD_WriteData8(0x28); LCD_WriteData8(0x20); LCD_WriteData8(0x22);
    LCD_WriteData8(0x1F); LCD_WriteData8(0x1B); LCD_WriteData8(0x23); LCD_WriteData8(0x37);
    LCD_WriteData8(0x00); LCD_WriteData8(0x07); LCD_WriteData8(0x02); LCD_WriteData8(0x10);
    /* Gamma 负向 */
    LCD_WriteCmd(0xE1);
    LCD_WriteData8(0x0F); LCD_WriteData8(0x1B); LCD_WriteData8(0x0F); LCD_WriteData8(0x17);
    LCD_WriteData8(0x33); LCD_WriteData8(0x2C); LCD_WriteData8(0x29); LCD_WriteData8(0x2E);
    LCD_WriteData8(0x30); LCD_WriteData8(0x30); LCD_WriteData8(0x39); LCD_WriteData8(0x3F);
    LCD_WriteData8(0x00); LCD_WriteData8(0x07); LCD_WriteData8(0x03); LCD_WriteData8(0x10);

    LCD_WriteCmd(0x2a); LCD_WriteData8(0x00); LCD_WriteData8(0x00); LCD_WriteData8(0x00); LCD_WriteData8(0x7f);
	LCD_WriteCmd(0x2b); LCD_WriteData8(0x00); LCD_WriteData8(0x00); LCD_WriteData8(0x00); LCD_WriteData8(0x9f);
    /* Enable test command */
	LCD_WriteCmd(0xF0); LCD_WriteData8(0x01); 
    /* Disable ram power save mode */
	LCD_WriteCmd(0xF6); LCD_WriteData8(0x00); 	

    /* 开显示 */
    LCD_WriteCmd(0x29);
    delay_ms(100);
    /* 设置显示方向 */
    LCD_SetRotation(USE_HORIZONTAL);
    /* 清屏并开背光 */
    LCD_ClearAll(WHITE);
    LCD_BL_ON;
}

#elif (USE_LCD_CONTROLLER == 1)  /* ILI9341 */

#include "stm32f10x_fsmc.h"

#if (TRANS_USE_DMA == 1)  /* 使用DMA */

/*  ========================================  DMA 行缓冲  ========================================  */
static u16 s_dma_buffer[LCD_DMA_BUF_SIZE];  /* 30KB，位于内部 RAM */
static volatile u8 s_dma_idle = 1;

#endif /* #if (TRANS_USE_DMA == 1) */

/* LCD的画笔颜色和背景色 */	   
u16 pointColor = 0X0000;	//画笔颜色
u16 bgColor = 0XFFFF;  //背景色 
  
// 管理LCD重要参数 默认为竖屏
lcd_dev lcddev;
scan_dir DefaultScanDir = L2R_U2D;  // 默认的扫描方向

/*  ========================================  FSMC 初始化  ========================================  */
static void LCD_FSMC_Init(void)
{
    /* 1. 使能 GPIO 时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOD | 
                           RCC_APB2Periph_GPIOE | RCC_APB2Periph_GPIOG, ENABLE);

    /* 2. 配置 FSMC 数据/地址/控制引脚 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_AF_PP;

    /* PD: D2,D3,NOE,NWE,D0,D1,D13,D14,D15 */
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_8 | GPIO_Pin_9 | 
                                GPIO_Pin_10 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* PE: D4 D5 D6 D7 D8 D9 D10 D11 D12 */
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12 | 
                                GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_Init(GPIOE, &GPIO_InitStruct);
    
    /* PG: A10 (RS/DC) NE4 */
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_12;
    GPIO_Init(GPIOG, &GPIO_InitStruct);

    /* 3. 使能 FSMC 时钟 */
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_FSMC, ENABLE);

    /* 4. 配置 FSMC Bank1 NOR/SRAM1 (Mode1, 16bit) */
    FSMC_NORSRAMInitTypeDef  FSMC_InitStruct;
    FSMC_NORSRAMTimingInitTypeDef FSMC_Timing;

    /* 读写时序: ILI9341 要求 Tcyc > 66ns (15MHz), 这里配 100ns 左右 */
    /* HCLK = 72MHz, ADDSET = 1(28ns), ADDHLD = 0, DATAST = 15(202ns), 总计 ~230ns */
    FSMC_Timing.FSMC_AddressSetupTime      = 1;     /* 地址建立时间 */
    FSMC_Timing.FSMC_AddressHoldTime       = 0;     /* 地址保持时间 */
    FSMC_Timing.FSMC_DataSetupTime         = 15;    /* 数据建立时间 */
    FSMC_Timing.FSMC_BusTurnAroundDuration = 0;
    FSMC_Timing.FSMC_CLKDivision           = 0;
    FSMC_Timing.FSMC_DataLatency           = 0;
    FSMC_Timing.FSMC_AccessMode            = FSMC_AccessMode_A;

    FSMC_InitStruct.FSMC_Bank                  = FSMC_LCD_BANKx;   // 这里我们使用NE1,也就对应BTCR[0],[1]
    FSMC_InitStruct.FSMC_DataAddressMux        = FSMC_DataAddressMux_Disable;   // 不复用数据地址 
    FSMC_InitStruct.FSMC_MemoryType            = FSMC_MemoryType_NOR;
    FSMC_InitStruct.FSMC_MemoryDataWidth       = FSMC_MemoryDataWidth_16b;   // 存储器数据宽度为16bit
    FSMC_InitStruct.FSMC_BurstAccessMode       = FSMC_BurstAccessMode_Disable;
    FSMC_InitStruct.FSMC_AsynchronousWait      = FSMC_AsynchronousWait_Disable;
    FSMC_InitStruct.FSMC_WaitSignalPolarity    = FSMC_WaitSignalPolarity_Low;
    FSMC_InitStruct.FSMC_WrapMode              = FSMC_WrapMode_Disable;
    FSMC_InitStruct.FSMC_WaitSignalActive      = FSMC_WaitSignalActive_BeforeWaitState;
    FSMC_InitStruct.FSMC_WriteOperation        = FSMC_WriteOperation_Enable;
    FSMC_InitStruct.FSMC_WaitSignal            = FSMC_WaitSignal_Disable;
    FSMC_InitStruct.FSMC_ExtendedMode          = FSMC_ExtendedMode_Disable;  // 读写使用不同的时序
    FSMC_InitStruct.FSMC_WriteBurst            = FSMC_WriteBurst_Disable;
    FSMC_InitStruct.FSMC_ReadWriteTimingStruct = &FSMC_Timing;

	FSMC_Timing.FSMC_AddressSetupTime      = 0;     /* 地址建立时间 */
    FSMC_Timing.FSMC_DataSetupTime         = 3;     /* 数据建立时间 */
    FSMC_InitStruct.FSMC_WriteTimingStruct = &FSMC_Timing;

    FSMC_NORSRAMInit(&FSMC_InitStruct);
    FSMC_NORSRAMCmd(FSMC_LCD_BANKx, ENABLE);
}

/* 从ILI93xx读出的数据为GBR格式，而我们写入的时候为RGB格式。
   通过该函数转换
   c:GBR格式的颜色值
   返回值：RGB格式的颜色值 */
u16 LCD_BGR2RGB(u16 bgrVal)
{
	u16 r,g,b,rgb;   
	b = (bgrVal>>0) & 0X1f;
	g = (bgrVal>>5) & 0X3f;
	r = (bgrVal>>11) & 0X1f;	 
	rgb = (b << 11) + (g << 5) + (r << 0);		 
	return rgb;
} 

//当mdk -O1时间优化时需要设置
//延时i
void opt_delay(u8 i)
{
	while(i--);
}

/* LCD开启显示 */
void LCD_DisplayOn(void)
{					   
	if(lcddev.id == 0X9341 || lcddev.id == 0X6804 || lcddev.id == 0X5310 || lcddev.id == 0X1963 || lcddev.id == 0X9488)   
        LCD_WRITE_CMD(0X29);	 // 开启显示
	else if(lcddev.id == 0X5510)
        LCD_WRITE_CMD(0X2900);	 // 开启显示
	else 
        LCD_WRITE_REG(0X07,0x0173); 	//开启显示
}	 

/* LCD关闭显示 */
void LCD_DisplayOff(void)
{	   
	if(lcddev.id == 0X9341 || lcddev.id == 0X6804 || lcddev.id == 0X5310 || lcddev.id == 0X1963 || lcddev.id == 0X9488)
        LCD_WRITE_CMD(0X28);	   // 关闭显示
	else if(lcddev.id == 0X5510)
        LCD_WRITE_CMD(0X2800);	   // 关闭显示
	else 
        LCD_WRITE_REG(0X07,0x00);// 关闭显示 
}  

/* 设置光标位置 Xpos:横坐标 Ypos:纵坐标 */
void LCD_SetCursor(u16 Xpos, u16 Ypos)
{	 
    if(lcddev.id == 0X9341 || lcddev.id == 0X5310) {		    
		LCD_WRITE_CMD(lcddev.setxcmd);  
		LCD_WRITE_DATA(Xpos>>8); LCD_WRITE_DATA(Xpos& 0XFF); 			 
		LCD_WRITE_CMD(lcddev.setycmd); 
		LCD_WRITE_DATA(Ypos>>8); LCD_WRITE_DATA(Ypos& 0XFF); 		
	} else if (lcddev.id == 0X9488) { 
        // 若横屏模式，交换 X/Y 坐标
        if (lcddev.dir == 1) { u16 tmp = Xpos; Xpos = Ypos; Ypos = tmp; }
        // 设置列地址 
        LCD_WRITE_CMD(lcddev.setxcmd);
        LCD_WRITE_DATA(Xpos>>8); LCD_WRITE_DATA(Xpos & 0XFF);
        LCD_WRITE_DATA((lcddev.width - 1)>>8); LCD_WRITE_DATA((lcddev.width - 1) & 0XFF);
        // 设置页地址 
        LCD_WRITE_CMD(lcddev.setycmd);
        LCD_WRITE_DATA(Ypos>>8); LCD_WRITE_DATA(Ypos & 0XFF);
        LCD_WRITE_DATA((lcddev.height - 1)>>8); LCD_WRITE_DATA((lcddev.height - 1) & 0XFF);
    } else if(lcddev.id == 0X6804) {
		if(lcddev.dir == 1)Xpos = lcddev.width-1-Xpos;//横屏时处理
		LCD_WRITE_CMD(lcddev.setxcmd); 
		LCD_WRITE_DATA(Xpos>>8); LCD_WRITE_DATA(Xpos& 0XFF); 
		LCD_WRITE_CMD(lcddev.setycmd); 
		LCD_WRITE_DATA(Ypos>>8); LCD_WRITE_DATA(Ypos& 0XFF); 
	} else if(lcddev.id == 0X1963) {  			 		
	    if(lcddev.dir == 0) {   // x坐标需要变换
			Xpos = lcddev.width-1-Xpos;
			LCD_WRITE_CMD(lcddev.setxcmd); 
			LCD_WRITE_DATA(0); LCD_WRITE_DATA(0); 		
			LCD_WRITE_DATA(Xpos>>8); LCD_WRITE_DATA(Xpos& 0XFF);		 	 
		} else {
			LCD_WRITE_CMD(lcddev.setxcmd); 
			LCD_WRITE_DATA(Xpos>>8); LCD_WRITE_DATA(Xpos& 0XFF); 		
			LCD_WRITE_DATA((lcddev.width-1)>>8); LCD_WRITE_DATA((lcddev.width-1)& 0XFF);		 	 			
		}	
		LCD_WRITE_CMD(lcddev.setycmd); 
		LCD_WRITE_DATA(Ypos>>8); LCD_WRITE_DATA(Ypos& 0XFF); 		
		LCD_WRITE_DATA((lcddev.height-1)>>8); LCD_WRITE_DATA((lcddev.height-1)& 0XFF); 			 		
	} else if(lcddev.id == 0X5510) {
		LCD_WRITE_CMD(lcddev.setxcmd); LCD_WRITE_DATA(Xpos>>8); 		
		LCD_WRITE_CMD(lcddev.setxcmd+1); LCD_WRITE_DATA(Xpos& 0XFF);			 
		LCD_WRITE_CMD(lcddev.setycmd); LCD_WRITE_DATA(Ypos>>8);  		
		LCD_WRITE_CMD(lcddev.setycmd+1); LCD_WRITE_DATA(Ypos& 0XFF);			
	} else {
		if(lcddev.dir == 1)Xpos = lcddev.width-1-Xpos;//横屏其实就是调转x,y坐标
		LCD_WRITE_REG(lcddev.setxcmd, Xpos);
		LCD_WRITE_REG(lcddev.setycmd, Ypos);
	}	 
} 		 

/**
  * 函数功能: 设置LCD的GRAM的扫描方向 
  * 输入参数: ucOption ：选择GRAM的扫描方向 
  *           可选值：1 :原点在屏幕左上角 X*Y=320*480
  *                   2 :原点在屏幕右上角 X*Y=480*320
  *                   3 :原点在屏幕右下角 X*Y=320*480
  *                   4 :原点在屏幕左下角 X*Y=480*320
  * 返 回 值: 无
  * 说    明：无
  */
void LCD_SetDirection( u8 ucOption )
{	
/**
  * Memory Access Control (36h)
  * This command defines read/write scanning direction of the frame memory.
  *
  * These 3 bits control the direction from the MPU to memory write/read.
  *
  * Bit  Symbol  Name  Description
  * D7   MY  Row Address Order     -- 以X轴镜像
  * D6   MX  Column Address Order  -- 以Y轴镜像
  * D5   MV  Row/Column Exchange   -- X轴与Y轴交换
  * D4   ML  Vertical Refresh Order  LCD vertical refresh direction control. 
  *
  * D3   BGR RGB-BGR Order   Color selector switch control
  *      (0 = RGB color filter panel, 1 = BGR color filter panel )
  * D2   MH  Horizontal Refresh ORDER  LCD horizontal refreshing direction control.
  * D1   X   Reserved  Reserved
  * D0   X   Reserved  Reserved
  */
	switch ( ucOption )
	{
		case 1:
//   左上角->右下角 
//	(0,0)	___ x(320)
//	     |  
//	     |
//       |	y(480) 
			LCD_WRITE_CMD(0x36); 
			LCD_WRITE_DATA(0x08); 
      
			LCD_WRITE_CMD(0x2A); 
			LCD_WRITE_DATA(0x00);	/* x start */	
			LCD_WRITE_DATA(0x00);
			LCD_WRITE_DATA(0x01);  /* x end */	
			LCD_WRITE_DATA(0x3F);

			LCD_WRITE_CMD(0x2B); 
			LCD_WRITE_DATA(0x00);	/* y start */  
			LCD_WRITE_DATA(0x00);
			LCD_WRITE_DATA(0x01);	/* y end */   
			LCD_WRITE_DATA(0xDF);					
		  break;
		
		case 2:
//		右上角-> 左下角
//		y(320)___ (0,0)            
//		         |
//		         |
//             |x(480)    
			LCD_WRITE_CMD(0x36); 
			LCD_WRITE_DATA(0x68);	
			LCD_WRITE_CMD(0x2A); 
			LCD_WRITE_DATA(0x00);
			LCD_WRITE_DATA(0x00);
			LCD_WRITE_DATA(0x01);
			LCD_WRITE_DATA(0xDF);	

			LCD_WRITE_CMD(0x2B); 
			LCD_WRITE_DATA(0x00);
			LCD_WRITE_DATA(0x00);
			LCD_WRITE_DATA(0x01);
			LCD_WRITE_DATA(0x3F);				
		  break;
		
		case 3:
//		右下角->左上角
//		          |y(480)
//		          |           
//		x(320) ___|(0,0)		
			LCD_WRITE_CMD(0x36); 
			LCD_WRITE_DATA(0xC8);	
			LCD_WRITE_CMD(0x2A); 
			LCD_WRITE_DATA(0x00);
			LCD_WRITE_DATA(0x00);
			LCD_WRITE_DATA(0x01);
			LCD_WRITE_DATA(0x3F);	

			LCD_WRITE_CMD(0x2B); 
			LCD_WRITE_DATA(0x00);
			LCD_WRITE_DATA(0x00);
			LCD_WRITE_DATA(0x01);
			LCD_WRITE_DATA(0x3F);			  
		  break;

		case 4:
//		左下角->右上角
//		|x(480)
//		|
//		|___ y(320)					  
			LCD_WRITE_CMD(0x36); 
			LCD_WRITE_DATA(0xA8);	
    
			LCD_WRITE_CMD(0x2A); 
			LCD_WRITE_DATA(0x00);
			LCD_WRITE_DATA(0x00);
			LCD_WRITE_DATA(0x01);
			LCD_WRITE_DATA(0xDF);	

			LCD_WRITE_CMD(0x2B); 
			LCD_WRITE_DATA(0x00);
			LCD_WRITE_DATA(0x00);
			LCD_WRITE_DATA(0x01);
			LCD_WRITE_DATA(0x3F);				
	    break;		
	}	
	/* 开始向GRAM写入数据 */
	LCD_WRITE_CMD (0x2C);	
}

#if 0
/* 设置LCD的自动扫描方向
   注意:其他函数可能会受到此函数设置的影响(尤其是9341/6804这两个奇葩),
   所以,一般设置为L2R_U2D即可,如果设置为其他扫描方式,可能导致显示不正常.
   dir:0~7,代表8个方向(具体定义见lcd.h)
   9ILI9341_VER_RESOLUTION/9325/9328/9488/4531/4535/1505/b505/5408/9341/5310/5510/1963等IC已经实际测试	 */  	   
void LCD_Scan_Dir(scan_dir dir)
{
    u16 regval = 0;
    u16 dirreg = 0;
    u16 temp;
    
    // 横屏方向转换（特定芯片）
    if((lcddev.dir == 1 && lcddev.id != 0X6804 && lcddev.id != 0X1963 && lcddev.id != 0X9488) || 
       (lcddev.dir == 0 && lcddev.id == 0X1963))   //横屏时，对6804、1963、9488不改变扫描方向！竖屏时1963改变方向
    {
        switch(dir) {
            case L2R_U2D: dir = D2U_L2R; break;
            case L2R_D2U: dir = D2U_R2L; break;
            case R2L_U2D: dir = U2D_L2R; break;
            case R2L_D2U: dir = U2D_R2L; break;
            case U2D_L2R: dir = L2R_D2U; break;
            case U2D_R2L: dir = L2R_U2D; break;
            case D2U_L2R: dir = R2L_D2U; break;
            case D2U_R2L: dir = R2L_U2D; break;
        }
    }
    
    // 使用0X36 命令的芯片：9341/6804/5310/5510/1963/9488
    if(lcddev.id == 0X9341 || lcddev.id == 0X6804 || lcddev.id == 0X5310  || 
       lcddev.id == 0X5510 || lcddev.id == 0X1963 || lcddev.id == 0X9488)  
    {
        switch(dir) {
            case L2R_U2D: regval |= (0<<7)|(0<<6)|(0<<5); break;
            case L2R_D2U: regval |= (1<<7)|(0<<6)|(0<<5); break;
            case R2L_U2D: regval |= (0<<7)|(1<<6)|(0<<5); break;
            case R2L_D2U: regval |= (1<<7)|(1<<6)|(0<<5); break;
            case U2D_L2R: regval |= (0<<7)|(0<<6)|(1<<5); break;
            case U2D_R2L: regval |= (0<<7)|(1<<6)|(1<<5); break;
            case D2U_L2R: regval |= (1<<7)|(0<<6)|(1<<5); break;
            case D2U_R2L: regval |= (1<<7)|(1<<6)|(1<<5); break;
        }
        
        if(lcddev.id == 0X5510) dirreg = 0X3600;
        else dirreg = 0X36;
        
        // BGR 位设置（某些芯片不需要）
        if((lcddev.id != 0X5310) && (lcddev.id != 0X5510) && (lcddev.id != 0X1963))
            regval |= 0X08;   
        if(lcddev.id == 0X6804) regval |= 0X02;   // 6804 特殊位 6804的BIT6和9341的反了	
        
        LCD_WRITE_REG(dirreg, regval);
        
        // 处理宽高交换（1963 除外）
        if(lcddev.id != 0X1963) {
            if(regval & 0X20) {
                if(lcddev.width < lcddev.height) {
                    temp = lcddev.width;
                    lcddev.width = lcddev.height;
                    lcddev.height = temp;
                }
            } else {
                if(lcddev.width > lcddev.height) {
                    temp = lcddev.width;
                    lcddev.width = lcddev.height;
                    lcddev.height = temp;
                }
            }
        }
        
        // 设置显示窗口（不同芯片命令略有差异）
        if(lcddev.id == 0X5510) {
            LCD_WRITE_CMD(lcddev.setxcmd);   LCD_WRITE_DATA(0);
            LCD_WRITE_CMD(lcddev.setxcmd+1); LCD_WRITE_DATA(0);
            LCD_WRITE_CMD(lcddev.setxcmd+2); LCD_WRITE_DATA((lcddev.width-1)>>8);
            LCD_WRITE_CMD(lcddev.setxcmd+3); LCD_WRITE_DATA((lcddev.width-1)& 0XFF);
            LCD_WRITE_CMD(lcddev.setycmd);   LCD_WRITE_DATA(0);
            LCD_WRITE_CMD(lcddev.setycmd+1); LCD_WRITE_DATA(0);
            LCD_WRITE_CMD(lcddev.setycmd+2); LCD_WRITE_DATA((lcddev.height-1)>>8);
            LCD_WRITE_CMD(lcddev.setycmd+3); LCD_WRITE_DATA((lcddev.height-1)& 0XFF);
        } else {
            // 9341、6804、5310、1963、9488 使用标准0X2A/0x2B 命令
            LCD_WRITE_CMD(lcddev.setxcmd);
            LCD_WRITE_DATA(0); LCD_WRITE_DATA(0);
            LCD_WRITE_DATA((lcddev.width-1)>>8); LCD_WRITE_DATA((lcddev.width-1)& 0XFF);
            LCD_WRITE_CMD(lcddev.setycmd);
            LCD_WRITE_DATA(0); LCD_WRITE_DATA(0);
            LCD_WRITE_DATA((lcddev.height-1)>>8); LCD_WRITE_DATA((lcddev.height-1)& 0XFF);
        }
    }
    else  // 其他老式芯片（使用0X03 命令）
    {
        switch(dir) {
            case L2R_U2D: regval |= (1<<5)|(1<<4)|(0<<3); break;
            case L2R_D2U: regval |= (0<<5)|(1<<4)|(0<<3); break;
            case R2L_U2D: regval |= (1<<5)|(0<<4)|(0<<3); break;
            case R2L_D2U: regval |= (0<<5)|(0<<4)|(0<<3); break;
            case U2D_L2R: regval |= (1<<5)|(1<<4)|(1<<3); break;
            case U2D_R2L: regval |= (1<<5)|(0<<4)|(1<<3); break;
            case D2U_L2R: regval |= (0<<5)|(1<<4)|(1<<3); break;
            case D2U_R2L: regval |= (0<<5)|(0<<4)|(1<<3); break;
        }
        dirreg = 0X03;
        regval |= 1<<12;
        LCD_WRITE_REG(dirreg, regval);
    }
}

#endif 

void LCD_Scan_Dir(scan_dir dir)
{
	u16 regval=0;
	u16 dirreg=0;
	u16 temp;
    scan_dir Dir;
	if((lcddev.dir==1 && lcddev.id!= 0X6804 && lcddev.id!= 0X1963) || (lcddev.dir==0 && lcddev.id== 0X1963))
    {   //横屏时，对6804和1963不改变扫描方向！竖屏时1963改变方向
		switch(dir) {
            case L2R_U2D: Dir = D2U_L2R; break;
            case L2R_D2U: Dir = D2U_R2L; break;
            case R2L_U2D: Dir = U2D_L2R; break;
            case R2L_D2U: Dir = U2D_R2L; break;
            case U2D_L2R: Dir = L2R_D2U; break;
            case U2D_R2L: Dir = L2R_U2D; break;
            case D2U_L2R: Dir = R2L_D2U; break;
            case D2U_R2L: Dir = R2L_U2D; break;
        }
	} 
	
	if(lcddev.id==0x9341||lcddev.id==0X6804||lcddev.id==0X5310||lcddev.id==0X5510||lcddev.id==0X1963)
	{   //9341/6804/5310/5510/1963,特殊处理
		switch(Dir)
		{
			case L2R_U2D://从左到右,从上到下
				regval |= (0<<7) | (0<<6) | (0<<5); 
				break;
			case L2R_D2U://从左到右,从下到上
				regval |= (1<<7) | (0<<6) | (0<<5); 
				break;
			case R2L_U2D://从右到左,从上到下
				regval |= (0<<7) | (1<<6) | (0<<5); 
				break;
			case R2L_D2U://从右到左,从下到上
				regval |= (1<<7) | (1<<6) | (0<<5); 
				break;	 
			case U2D_L2R://从上到下,从左到右
				regval |= (0<<7) | (0<<6) | (1<<5); 
				break;
			case U2D_R2L://从上到下,从右到左
				regval |= (0<<7) | (1<<6) | (1<<5); 
				break;
			case D2U_L2R://从下到上,从左到右
				regval |= (1<<7) | (0<<6) | (1<<5); 
				break;
			case D2U_R2L://从下到上,从右到左
				regval |= (1<<7) | (1<<6) | (1<<5); 
				break;	 
		}
		if(lcddev.id==0X5510) dirreg=0X3600;
		else dirreg=0X36;
 		if((lcddev.id!=0X5310)&&(lcddev.id!=0X5510)&&(lcddev.id!=0X1963)) regval |= 0X08;//5310/5510/1963不需要BGR   
		if(lcddev.id==0X6804) regval |= 0x02;//6804的BIT6和9341的反了	   
		LCD_WRITE_REG(dirreg,regval);
		if(lcddev.id!=0X1963) //1963不做坐标处理
		{
			if(regval&0X20) {
				if(lcddev.width<lcddev.height) {
					//交换X,Y
					temp=lcddev.width;
					lcddev.width=lcddev.height;
					lcddev.height=temp;
				}
			} else {
				if(lcddev.width>lcddev.height) {
					//交换X,Y
					temp=lcddev.width;
					lcddev.width=lcddev.height;
					lcddev.height=temp;
				}
			}  
		}
		if(lcddev.id==0X5510) {
			LCD_WRITE_CMD(lcddev.setxcmd); LCD_WRITE_DATA(0); 
			LCD_WRITE_CMD(lcddev.setxcmd+1); LCD_WRITE_DATA(0); 
			LCD_WRITE_CMD(lcddev.setxcmd+2); LCD_WRITE_DATA((lcddev.width-1)>>8); 
			LCD_WRITE_CMD(lcddev.setxcmd+3); LCD_WRITE_DATA((lcddev.width-1)&0XFF); 
			LCD_WRITE_CMD(lcddev.setycmd); LCD_WRITE_DATA(0); 
			LCD_WRITE_CMD(lcddev.setycmd+1); LCD_WRITE_DATA(0); 
			LCD_WRITE_CMD(lcddev.setycmd+2); LCD_WRITE_DATA((lcddev.height-1)>>8); 
			LCD_WRITE_CMD(lcddev.setycmd+3); LCD_WRITE_DATA((lcddev.height-1)&0XFF);
		} else {
			LCD_WRITE_CMD(lcddev.setxcmd); 
			LCD_WRITE_DATA(0); LCD_WRITE_DATA(0);	
			LCD_WRITE_DATA((lcddev.width-1)>>8); LCD_WRITE_DATA((lcddev.width-1)&0XFF);
			LCD_WRITE_CMD(lcddev.setycmd); 
			LCD_WRITE_DATA(0); LCD_WRITE_DATA(0);
			LCD_WRITE_DATA((lcddev.height-1)>>8); LCD_WRITE_DATA((lcddev.height-1)&0XFF);  
		}
  	} else {
		switch(Dir)
		{
			case L2R_U2D://从左到右,从上到下
				regval|=(1<<5)|(1<<4)|(0<<3); 
				break;
			case L2R_D2U://从左到右,从下到上
				regval|=(0<<5)|(1<<4)|(0<<3); 
				break;
			case R2L_U2D://从右到左,从上到下
				regval|=(1<<5)|(0<<4)|(0<<3);
				break;
			case R2L_D2U://从右到左,从下到上
				regval|=(0<<5)|(0<<4)|(0<<3); 
				break;	 
			case U2D_L2R://从上到下,从左到右
				regval|=(1<<5)|(1<<4)|(1<<3); 
				break;
			case U2D_R2L://从上到下,从右到左
				regval|=(1<<5)|(0<<4)|(1<<3); 
				break;
			case D2U_L2R://从下到上,从左到右
				regval|=(0<<5)|(1<<4)|(1<<3); 
				break;
			case D2U_R2L://从下到上,从右到左
				regval|=(0<<5)|(0<<4)|(1<<3); 
				break;	 
		} 
		dirreg = 0X03;
		regval |= 1<<12; 
		LCD_WRITE_REG(dirreg,regval);
	} 
}   

/* 设置LCD显示方向 dir:0,竖屏；1,横屏 */
void LCD_Display_Dir(u8 dir)
{
	if(dir == 0) {	//竖屏
		lcddev.dir = 0;	//竖屏
		lcddev.width = ILI9341_VER_RESOLUTION;
		lcddev.height = ILI9341_HOR_RESOLUTION;
		if(lcddev.id == 0X9341 || lcddev.id == 0X6804 || lcddev.id == 0X5310 || lcddev.id == 0X9488) {
			lcddev.wramcmd = 0X2C;
	 		lcddev.setxcmd = 0X2A;
			lcddev.setycmd = 0X2B;  	 	 
			if(lcddev.id == 0X6804 || lcddev.id == 0X5310 || lcddev.id == 0X9488) {
				lcddev.width = 320;
				lcddev.height = 480;
			}
		} else if(lcddev.id == 0X5510) {
			lcddev.wramcmd = 0X2C00;
	 		lcddev.setxcmd = 0X2A00;
			lcddev.setycmd = 0X2B00; 
			lcddev.width = 480;
			lcddev.height = 800;
		} else if(lcddev.id == 0X1963) {
			lcddev.wramcmd = 0X2C;	//设置写入GRAM的指令 
			lcddev.setxcmd = 0X2B;	//设置写X坐标指令
			lcddev.setycmd = 0X2A;	//设置写Y坐标指令
			lcddev.width = 480;		//设置宽度480
			lcddev.height = 800;    //设置高度800  
		} else {
			lcddev.wramcmd = 0X22;
	 		lcddev.setxcmd = 0X20;
			lcddev.setycmd = 0X21;  
		}
	} else { 	//横屏	  				
		lcddev.dir = 1;	//横屏
		lcddev.width = ILI9341_HOR_RESOLUTION;
		lcddev.height = ILI9341_VER_RESOLUTION;
		if(lcddev.id == 0X9341 || lcddev.id == 0X5310 || lcddev.id == 0X9488) {
			lcddev.wramcmd = 0X2C;
	 		lcddev.setxcmd = 0X2A;
			lcddev.setycmd = 0X2B;  	 	 
		} else if(lcddev.id == 0X6804) {
 			lcddev.wramcmd = 0X2C;
	 		lcddev.setxcmd = 0X2B;
			lcddev.setycmd = 0X2A; 
		} else if(lcddev.id == 0X5510) {
			lcddev.wramcmd = 0X2C00;
	 		lcddev.setxcmd = 0X2A00;
			lcddev.setycmd = 0X2B00; 
			lcddev.width = 800;
			lcddev.height = 480;
		} else if(lcddev.id == 0X1963) {
			lcddev.wramcmd = 0X2C;	//设置写入GRAM的指令 
			lcddev.setxcmd = 0X2A;	//设置写X坐标指令
			lcddev.setycmd = 0X2B;	//设置写Y坐标指令
			lcddev.width = 800;		//设置宽度800
			lcddev.height = 480;    //设置高度480  
		} else {
			lcddev.wramcmd = 0X22;
	 		lcddev.setxcmd = 0X21;
			lcddev.setycmd = 0X20;  
		}
		if(lcddev.id == 0X6804 || lcddev.id == 0X5310 || lcddev.id == 0X9488) { 	 
			lcddev.width = 480;
			lcddev.height = 320; 			
		}
	} 
	LCD_Scan_Dir(DefaultScanDir);	//默认扫描方向
}
 
/*  ========================================  LCD 驱动芯片 初始化序列  ========================================  */
void LCD_ILI9341_Init(void)
{
    delay_ms(50); 				
  	LCD_READ_REG(0x0,lcddev.id);	//读ID（9320/9325/9328/4531/4535等IC）   
  	if(lcddev.id<0XFF || lcddev.id==0XFFFF || lcddev.id==0X9300)
	{	//读到ID不正确,新增lcddev.id==0X9300判断，因为9341在未被复位的情况下会被读成9300
 		//尝试9341 ID的读取		
		LCD_WRITE_CMD(0XD3);				   
		lcddev.id=LCD_READ_DATA();	//dummy read 	
 		lcddev.id=LCD_READ_DATA();	//读到0X00
  		lcddev.id=LCD_READ_DATA();  //读取93								   
 		lcddev.id<<=8;
		lcddev.id|=LCD_READ_DATA(); //读取41 	   			   
 		if(lcddev.id!=0X9341)		//非9341,尝试是不是6804
		{	
 			LCD_WRITE_CMD(0XBF);				   
			lcddev.id=LCD_READ_DATA(); 	//dummy read 	 
	 		lcddev.id=LCD_READ_DATA();   	//读回0X01			   
	 		lcddev.id=LCD_READ_DATA(); 	//读回0XD0 			  	
	  		lcddev.id=LCD_READ_DATA();	//这里读回0X68 
			lcddev.id<<=8;
	  		lcddev.id|=LCD_READ_DATA();	//这里读回0X04	  
			if(lcddev.id!=0X6804)		//也不是6804,尝试看看是不是NT35310
			{ 
				LCD_WRITE_CMD(0XD4);				   
				lcddev.id=LCD_READ_DATA();//dummy read  
				lcddev.id=LCD_READ_DATA();//读回0X01	 
				lcddev.id=LCD_READ_DATA();//读回0X53	
				lcddev.id<<=8;	 
				lcddev.id|=LCD_READ_DATA();	//这里读回0X10	 
				if(lcddev.id!=0X5310)		//也不是NT35310,尝试看看是不是NT35510
				{
					LCD_WRITE_CMD(0XDA00);	
					lcddev.id=LCD_READ_DATA();		//读回0X00	 
					LCD_WRITE_CMD(0XDB00);	
					lcddev.id=LCD_READ_DATA();		//读回0X80
					lcddev.id<<=8;	
					LCD_WRITE_CMD(0XDC00);	
					lcddev.id|=LCD_READ_DATA();		//读回0X00		
					if(lcddev.id==0x8000) lcddev.id=0x5510;//NT35510读回的ID是8000H,为方便区分,我们强制设置为5510
					if(lcddev.id!=0X5510)			//也不是NT5510,尝试看看是不是SSD1963
					{
						LCD_WRITE_CMD(0XA1);
						lcddev.id=LCD_READ_DATA();
						lcddev.id=LCD_READ_DATA();	//读回0X57
						lcddev.id<<=8;	 
						lcddev.id|=LCD_READ_DATA();	//读回0X61	
						if(lcddev.id==0X5761) lcddev.id=0X1963;//SSD1963读回的ID是5761H,为方便区分,我们强制设置为1963
					}
				}
			}
 		}  	
	} 
 	//LOG_D(" LCD ID:%x\r\n",lcddev.id); //打印LCD ID   
	if(lcddev.id==0X9341)	//9341初始化
	{	 
		LCD_WRITE_CMD(0xCF);  
		LCD_WRITE_DATA(0x00); 
		LCD_WRITE_DATA(0xC1); 
		LCD_WRITE_DATA(0X30); 
		LCD_WRITE_CMD(0xED);  
		LCD_WRITE_DATA(0x64); 
		LCD_WRITE_DATA(0x03); 
		LCD_WRITE_DATA(0X12); 
		LCD_WRITE_DATA(0X81); 
		LCD_WRITE_CMD(0xE8);  
		LCD_WRITE_DATA(0x85); 
		LCD_WRITE_DATA(0x10); 
		LCD_WRITE_DATA(0x7A); 
		LCD_WRITE_CMD(0xCB);  
		LCD_WRITE_DATA(0x39); 
		LCD_WRITE_DATA(0x2C); 
		LCD_WRITE_DATA(0x00); 
		LCD_WRITE_DATA(0x34); 
		LCD_WRITE_DATA(0x02); 
		LCD_WRITE_CMD(0xF7);  
		LCD_WRITE_DATA(0x20); 
		LCD_WRITE_CMD(0xEA);  
		LCD_WRITE_DATA(0x00); 
		LCD_WRITE_DATA(0x00); 
		LCD_WRITE_CMD(0xC0);    //Power control 
		LCD_WRITE_DATA(0x1B);   //VRH[5:0] 
		LCD_WRITE_CMD(0xC1);    //Power control 
		LCD_WRITE_DATA(0x01);   //SAP[2:0];BT[3:0] 
		LCD_WRITE_CMD(0xC5);    //VCM control 
		LCD_WRITE_DATA(0x30); 	 //3F
		LCD_WRITE_DATA(0x30); 	 //3C
		LCD_WRITE_CMD(0xC7);    //VCM control2 
		LCD_WRITE_DATA(0XB7); 
		LCD_WRITE_CMD(0x36);    // Memory Access Control 
		LCD_WRITE_DATA(0x48); 
		LCD_WRITE_CMD(0x3A);   
		LCD_WRITE_DATA(0x55); 
		LCD_WRITE_CMD(0xB1);   
		LCD_WRITE_DATA(0x00);   
		LCD_WRITE_DATA(0x1A); 
		LCD_WRITE_CMD(0xB6);    // Display Function Control 
		LCD_WRITE_DATA(0x0A); 
		LCD_WRITE_DATA(0xA2); 
		LCD_WRITE_CMD(0xF2);    // 3Gamma Function Disable 
		LCD_WRITE_DATA(0x00); 
		LCD_WRITE_CMD(0x26);    //Gamma curve selected 
		LCD_WRITE_DATA(0x01); 
		LCD_WRITE_CMD(0xE0);    //Set Gamma 
		LCD_WRITE_DATA(0x0F); 
		LCD_WRITE_DATA(0x2A); 
		LCD_WRITE_DATA(0x28); 
		LCD_WRITE_DATA(0x08); 
		LCD_WRITE_DATA(0x0E); 
		LCD_WRITE_DATA(0x08); 
		LCD_WRITE_DATA(0x54); 
		LCD_WRITE_DATA(0XA9); 
		LCD_WRITE_DATA(0x43); 
		LCD_WRITE_DATA(0x0A); 
		LCD_WRITE_DATA(0x0F); 
		LCD_WRITE_DATA(0x00); 
		LCD_WRITE_DATA(0x00); 
		LCD_WRITE_DATA(0x00); 
		LCD_WRITE_DATA(0x00); 		 
		LCD_WRITE_CMD(0XE1);    //Set Gamma 
		LCD_WRITE_DATA(0x00); 
		LCD_WRITE_DATA(0x15); 
		LCD_WRITE_DATA(0x17); 
		LCD_WRITE_DATA(0x07); 
		LCD_WRITE_DATA(0x11); 
		LCD_WRITE_DATA(0x06); 
		LCD_WRITE_DATA(0x2B); 
		LCD_WRITE_DATA(0x56); 
		LCD_WRITE_DATA(0x3C); 
		LCD_WRITE_DATA(0x05); 
		LCD_WRITE_DATA(0x10); 
		LCD_WRITE_DATA(0x0F); 
		LCD_WRITE_DATA(0x3F); 
		LCD_WRITE_DATA(0x3F); 
		LCD_WRITE_DATA(0x0F); 
		LCD_WRITE_CMD(0x2B); 
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x01);
		LCD_WRITE_DATA(0x3f);
		LCD_WRITE_CMD(0x2A); 
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xef);	 
		LCD_WRITE_CMD(0x11); //Exit Sleep
		delay_ms(120);
		LCD_WRITE_CMD(0x29); //display on	
	} else if(lcddev.id==0x6804) //6804初始化
	{
		LCD_WRITE_CMD(0X11);
		delay_ms(20);
		LCD_WRITE_CMD(0XD0);//VCI1  VCL  VGH  VGL DDVDH VREG1OUT power amplitude setting
		LCD_WRITE_DATA(0X07); 
		LCD_WRITE_DATA(0X42); 
		LCD_WRITE_DATA(0X1D); 
		LCD_WRITE_CMD(0XD1);//VCOMH VCOM_AC amplitude setting
		LCD_WRITE_DATA(0X00);
		LCD_WRITE_DATA(0X1a);
		LCD_WRITE_DATA(0X09); 
		LCD_WRITE_CMD(0XD2);//Operational Amplifier Circuit Constant Current Adjust , charge pump frequency setting
		LCD_WRITE_DATA(0X01);
		LCD_WRITE_DATA(0X22);
		LCD_WRITE_CMD(0XC0);//REV SM GS 
		LCD_WRITE_DATA(0X10);
		LCD_WRITE_DATA(0X3B);
		LCD_WRITE_DATA(0X00);
		LCD_WRITE_DATA(0X02);
		LCD_WRITE_DATA(0X11);
		
		LCD_WRITE_CMD(0XC5);// Frame rate setting = 72HZ  when setting 0x03
		LCD_WRITE_DATA(0X03);
		
		LCD_WRITE_CMD(0XC8);//Gamma setting
		LCD_WRITE_DATA(0X00);
		LCD_WRITE_DATA(0X25);
		LCD_WRITE_DATA(0X21);
		LCD_WRITE_DATA(0X05);
		LCD_WRITE_DATA(0X00);
		LCD_WRITE_DATA(0X0a);
		LCD_WRITE_DATA(0X65);
		LCD_WRITE_DATA(0X25);
		LCD_WRITE_DATA(0X77);
		LCD_WRITE_DATA(0X50);
		LCD_WRITE_DATA(0X0f);
		LCD_WRITE_DATA(0X00);	  
						  
   		LCD_WRITE_CMD(0XF8);
		LCD_WRITE_DATA(0X01);	  

 		LCD_WRITE_CMD(0XFE);
 		LCD_WRITE_DATA(0X00);
 		LCD_WRITE_DATA(0X02);
		
		LCD_WRITE_CMD(0X20);//Exit invert mode

		LCD_WRITE_CMD(0X36);
		LCD_WRITE_DATA(0X08);//原来是a
		
		LCD_WRITE_CMD(0X3A);
		LCD_WRITE_DATA(0X55);//16位模式	  
		LCD_WRITE_CMD(0X2B);
		LCD_WRITE_DATA(0X00);
		LCD_WRITE_DATA(0X00);
		LCD_WRITE_DATA(0X01);
		LCD_WRITE_DATA(0X3F);
		
		LCD_WRITE_CMD(0X2A);
		LCD_WRITE_DATA(0X00);
		LCD_WRITE_DATA(0X00);
		LCD_WRITE_DATA(0X01);
		LCD_WRITE_DATA(0XDF);
		delay_ms(120);
		LCD_WRITE_CMD(0X29); 	 
 	} else if(lcddev.id==0x5310)
	{ 
		LCD_WRITE_CMD(0xED);
		LCD_WRITE_DATA(0x01);
		LCD_WRITE_DATA(0xFE);

		LCD_WRITE_CMD(0xEE);
		LCD_WRITE_DATA(0xDE);
		LCD_WRITE_DATA(0x21);

		LCD_WRITE_CMD(0xF1);
		LCD_WRITE_DATA(0x01);
		LCD_WRITE_CMD(0xDF);
		LCD_WRITE_DATA(0x10);

		//VCOMvoltage//
		LCD_WRITE_CMD(0xC4);
		LCD_WRITE_DATA(0x8F);	  //5f

		LCD_WRITE_CMD(0xC6);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xE2);
		LCD_WRITE_DATA(0xE2);
		LCD_WRITE_DATA(0xE2);
		LCD_WRITE_CMD(0xBF);
		LCD_WRITE_DATA(0xAA);

		LCD_WRITE_CMD(0xB0);
		LCD_WRITE_DATA(0x0D);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x0D);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x11);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x19);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x21);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x2D);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x3D);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x5D);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x5D);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xB1);
		LCD_WRITE_DATA(0x80);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x8B);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x96);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xB2);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x02);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x03);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xB3);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xB4);
		LCD_WRITE_DATA(0x8B);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x96);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xA1);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xB5);
		LCD_WRITE_DATA(0x02);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x03);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x04);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xB6);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xB7);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x3F);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x5E);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x64);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x8C);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xAC);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xDC);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x70);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x90);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xEB);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xDC);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xB8);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xBA);
		LCD_WRITE_DATA(0x24);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xC1);
		LCD_WRITE_DATA(0x20);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x54);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xFF);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xC2);
		LCD_WRITE_DATA(0x0A);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x04);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xC3);
		LCD_WRITE_DATA(0x3C);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x3A);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x39);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x37);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x3C);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x36);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x32);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x2F);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x2C);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x29);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x26);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x24);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x24);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x23);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x3C);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x36);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x32);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x2F);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x2C);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x29);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x26);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x24);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x24);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x23);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xC4);
		LCD_WRITE_DATA(0x62);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x05);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x84);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xF0);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x18);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xA4);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x18);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x50);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x0C);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x17);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x95);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xF3);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xE6);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xC5);
		LCD_WRITE_DATA(0x32);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x44);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x65);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x76);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x88);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xC6);
		LCD_WRITE_DATA(0x20);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x17);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x01);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xC7);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xC8);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xC9);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xE0);
		LCD_WRITE_DATA(0x16);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x1C);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x21);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x36);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x46);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x52);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x64);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x7A);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x8B);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x99);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xA8);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xB9);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xC4);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xCA);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xD2);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xD9);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xE0);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xF3);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xE1);
		LCD_WRITE_DATA(0x16);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x1C);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x22);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x36);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x45);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x52);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x64);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x7A);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x8B);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x99);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xA8);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xB9);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xC4);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xCA);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xD2);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xD8);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xE0);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xF3);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xE2);
		LCD_WRITE_DATA(0x05);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x0B);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x1B);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x34);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x44);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x4F);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x61);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x79);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x88);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x97);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xA6);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xB7);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xC2);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xC7);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xD1);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xD6);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xDD);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xF3);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_CMD(0xE3);
		LCD_WRITE_DATA(0x05);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xA);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x1C);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x33);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x44);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x50);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x62);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x78);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x88);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x97);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xA6);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xB7);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xC2);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xC7);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xD1);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xD5);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xDD);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xF3);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xE4);
		LCD_WRITE_DATA(0x01);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x01);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x02);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x2A);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x3C);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x4B);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x5D);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x74);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x84);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x93);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xA2);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xB3);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xBE);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xC4);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xCD);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xD3);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xDD);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xF3);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_CMD(0xE5);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x02);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x29);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x3C);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x4B);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x5D);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x74);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x84);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x93);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xA2);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xB3);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xBE);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xC4);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xCD);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xD3);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xDC);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xF3);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xE6);
		LCD_WRITE_DATA(0x11);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x34);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x56);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x76);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x77);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x66);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x88);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x99);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xBB);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x99);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x66);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x55);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x55);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x45);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x43);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x44);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xE7);
		LCD_WRITE_DATA(0x32);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x55);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x76);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x66);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x67);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x67);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x87);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x99);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xBB);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x99);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x77);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x44);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x56);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x23); 
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x33);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x45);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xE8);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x99);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x87);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x88);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x77);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x66);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x88);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xAA);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0xBB);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x99);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x66);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x55);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x55);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x44);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x44);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x55);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xE9);
		LCD_WRITE_DATA(0xAA);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0x00);
		LCD_WRITE_DATA(0xAA);

		LCD_WRITE_CMD(0xCF);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xF0);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x50);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xF3);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0xF9);
		LCD_WRITE_DATA(0x06);
		LCD_WRITE_DATA(0x10);
		LCD_WRITE_DATA(0x29);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0x3A);
		LCD_WRITE_DATA(0x55);	//66

		LCD_WRITE_CMD(0x11);
		delay_ms(100);
		LCD_WRITE_CMD(0x29);
		LCD_WRITE_CMD(0x35);
		LCD_WRITE_DATA(0x00);

		LCD_WRITE_CMD(0x51);
		LCD_WRITE_DATA(0xFF);
		LCD_WRITE_CMD(0x53);
		LCD_WRITE_DATA(0x2C);
		LCD_WRITE_CMD(0x55);
		LCD_WRITE_DATA(0x82);
		LCD_WRITE_CMD(0x2c);
	} else if(lcddev.id==0x5510)
	{
		LCD_WRITE_REG(0xF000,0x55);
		LCD_WRITE_REG(0xF001,0xAA);
		LCD_WRITE_REG(0xF002,0x52);
		LCD_WRITE_REG(0xF003,0x08);
		LCD_WRITE_REG(0xF004,0x01);
		//AVDD Set AVDD 5.2V
		LCD_WRITE_REG(0xB000,0x0D);
		LCD_WRITE_REG(0xB001,0x0D);
		LCD_WRITE_REG(0xB002,0x0D);
		//AVDD ratio
		LCD_WRITE_REG(0xB600,0x34);
		LCD_WRITE_REG(0xB601,0x34);
		LCD_WRITE_REG(0xB602,0x34);
		//AVEE -5.2V
		LCD_WRITE_REG(0xB100,0x0D);
		LCD_WRITE_REG(0xB101,0x0D);
		LCD_WRITE_REG(0xB102,0x0D);
		//AVEE ratio
		LCD_WRITE_REG(0xB700,0x34);
		LCD_WRITE_REG(0xB701,0x34);
		LCD_WRITE_REG(0xB702,0x34);
		//VCL -2.5V
		LCD_WRITE_REG(0xB200,0x00);
		LCD_WRITE_REG(0xB201,0x00);
		LCD_WRITE_REG(0xB202,0x00);
		//VCL ratio
		LCD_WRITE_REG(0xB800,0x24);
		LCD_WRITE_REG(0xB801,0x24);
		LCD_WRITE_REG(0xB802,0x24);
		//VGH 15V (Free pump)
		LCD_WRITE_REG(0xBF00,0x01);
		LCD_WRITE_REG(0xB300,0x0F);
		LCD_WRITE_REG(0xB301,0x0F);
		LCD_WRITE_REG(0xB302,0x0F);
		//VGH ratio
		LCD_WRITE_REG(0xB900,0x34);
		LCD_WRITE_REG(0xB901,0x34);
		LCD_WRITE_REG(0xB902,0x34);
		//VGL_REG -10V
		LCD_WRITE_REG(0xB500,0x08);
		LCD_WRITE_REG(0xB501,0x08);
		LCD_WRITE_REG(0xB502,0x08);
		LCD_WRITE_REG(0xC200,0x03);
		//VGLX ratio
		LCD_WRITE_REG(0xBA00,0x24);
		LCD_WRITE_REG(0xBA01,0x24);
		LCD_WRITE_REG(0xBA02,0x24);
		//VGMP/VGSP 4.5V/0V
		LCD_WRITE_REG(0xBC00,0x00);
		LCD_WRITE_REG(0xBC01,0x78);
		LCD_WRITE_REG(0xBC02,0x00);
		//VGMN/VGSN -4.5V/0V
		LCD_WRITE_REG(0xBD00,0x00);
		LCD_WRITE_REG(0xBD01,0x78);
		LCD_WRITE_REG(0xBD02,0x00);
		//VCOM
		LCD_WRITE_REG(0xBE00,0x00);
		LCD_WRITE_REG(0xBE01,0x64);
		//Gamma Setting
		LCD_WRITE_REG(0xD100,0x00);
		LCD_WRITE_REG(0xD101,0x33);
		LCD_WRITE_REG(0xD102,0x00);
		LCD_WRITE_REG(0xD103,0x34);
		LCD_WRITE_REG(0xD104,0x00);
		LCD_WRITE_REG(0xD105,0x3A);
		LCD_WRITE_REG(0xD106,0x00);
		LCD_WRITE_REG(0xD107,0x4A);
		LCD_WRITE_REG(0xD108,0x00);
		LCD_WRITE_REG(0xD109,0x5C);
		LCD_WRITE_REG(0xD10A,0x00);
		LCD_WRITE_REG(0xD10B,0x81);
		LCD_WRITE_REG(0xD10C,0x00);
		LCD_WRITE_REG(0xD10D,0xA6);
		LCD_WRITE_REG(0xD10E,0x00);
		LCD_WRITE_REG(0xD10F,0xE5);
		LCD_WRITE_REG(0xD110,0x01);
		LCD_WRITE_REG(0xD111,0x13);
		LCD_WRITE_REG(0xD112,0x01);
		LCD_WRITE_REG(0xD113,0x54);
		LCD_WRITE_REG(0xD114,0x01);
		LCD_WRITE_REG(0xD115,0x82);
		LCD_WRITE_REG(0xD116,0x01);
		LCD_WRITE_REG(0xD117,0xCA);
		LCD_WRITE_REG(0xD118,0x02);
		LCD_WRITE_REG(0xD119,0x00);
		LCD_WRITE_REG(0xD11A,0x02);
		LCD_WRITE_REG(0xD11B,0x01);
		LCD_WRITE_REG(0xD11C,0x02);
		LCD_WRITE_REG(0xD11D,0x34);
		LCD_WRITE_REG(0xD11E,0x02);
		LCD_WRITE_REG(0xD11F,0x67);
		LCD_WRITE_REG(0xD120,0x02);
		LCD_WRITE_REG(0xD121,0x84);
		LCD_WRITE_REG(0xD122,0x02);
		LCD_WRITE_REG(0xD123,0xA4);
		LCD_WRITE_REG(0xD124,0x02);
		LCD_WRITE_REG(0xD125,0xB7);
		LCD_WRITE_REG(0xD126,0x02);
		LCD_WRITE_REG(0xD127,0xCF);
		LCD_WRITE_REG(0xD128,0x02);
		LCD_WRITE_REG(0xD129,0xDE);
		LCD_WRITE_REG(0xD12A,0x02);
		LCD_WRITE_REG(0xD12B,0xF2);
		LCD_WRITE_REG(0xD12C,0x02);
		LCD_WRITE_REG(0xD12D,0xFE);
		LCD_WRITE_REG(0xD12E,0x03);
		LCD_WRITE_REG(0xD12F,0x10);
		LCD_WRITE_REG(0xD130,0x03);
		LCD_WRITE_REG(0xD131,0x33);
		LCD_WRITE_REG(0xD132,0x03);
		LCD_WRITE_REG(0xD133,0x6D);
		LCD_WRITE_REG(0xD200,0x00);
		LCD_WRITE_REG(0xD201,0x33);
		LCD_WRITE_REG(0xD202,0x00);
		LCD_WRITE_REG(0xD203,0x34);
		LCD_WRITE_REG(0xD204,0x00);
		LCD_WRITE_REG(0xD205,0x3A);
		LCD_WRITE_REG(0xD206,0x00);
		LCD_WRITE_REG(0xD207,0x4A);
		LCD_WRITE_REG(0xD208,0x00);
		LCD_WRITE_REG(0xD209,0x5C);
		LCD_WRITE_REG(0xD20A,0x00);

		LCD_WRITE_REG(0xD20B,0x81);
		LCD_WRITE_REG(0xD20C,0x00);
		LCD_WRITE_REG(0xD20D,0xA6);
		LCD_WRITE_REG(0xD20E,0x00);
		LCD_WRITE_REG(0xD20F,0xE5);
		LCD_WRITE_REG(0xD210,0x01);
		LCD_WRITE_REG(0xD211,0x13);
		LCD_WRITE_REG(0xD212,0x01);
		LCD_WRITE_REG(0xD213,0x54);
		LCD_WRITE_REG(0xD214,0x01);
		LCD_WRITE_REG(0xD215,0x82);
		LCD_WRITE_REG(0xD216,0x01);
		LCD_WRITE_REG(0xD217,0xCA);
		LCD_WRITE_REG(0xD218,0x02);
		LCD_WRITE_REG(0xD219,0x00);
		LCD_WRITE_REG(0xD21A,0x02);
		LCD_WRITE_REG(0xD21B,0x01);
		LCD_WRITE_REG(0xD21C,0x02);
		LCD_WRITE_REG(0xD21D,0x34);
		LCD_WRITE_REG(0xD21E,0x02);
		LCD_WRITE_REG(0xD21F,0x67);
		LCD_WRITE_REG(0xD220,0x02);
		LCD_WRITE_REG(0xD221,0x84);
		LCD_WRITE_REG(0xD222,0x02);
		LCD_WRITE_REG(0xD223,0xA4);
		LCD_WRITE_REG(0xD224,0x02);
		LCD_WRITE_REG(0xD225,0xB7);
		LCD_WRITE_REG(0xD226,0x02);
		LCD_WRITE_REG(0xD227,0xCF);
		LCD_WRITE_REG(0xD228,0x02);
		LCD_WRITE_REG(0xD229,0xDE);
		LCD_WRITE_REG(0xD22A,0x02);
		LCD_WRITE_REG(0xD22B,0xF2);
		LCD_WRITE_REG(0xD22C,0x02);
		LCD_WRITE_REG(0xD22D,0xFE);
		LCD_WRITE_REG(0xD22E,0x03);
		LCD_WRITE_REG(0xD22F,0x10);
		LCD_WRITE_REG(0xD230,0x03);
		LCD_WRITE_REG(0xD231,0x33);
		LCD_WRITE_REG(0xD232,0x03);
		LCD_WRITE_REG(0xD233,0x6D);
		LCD_WRITE_REG(0xD300,0x00);
		LCD_WRITE_REG(0xD301,0x33);
		LCD_WRITE_REG(0xD302,0x00);
		LCD_WRITE_REG(0xD303,0x34);
		LCD_WRITE_REG(0xD304,0x00);
		LCD_WRITE_REG(0xD305,0x3A);
		LCD_WRITE_REG(0xD306,0x00);
		LCD_WRITE_REG(0xD307,0x4A);
		LCD_WRITE_REG(0xD308,0x00);
		LCD_WRITE_REG(0xD309,0x5C);
		LCD_WRITE_REG(0xD30A,0x00);

		LCD_WRITE_REG(0xD30B,0x81);
		LCD_WRITE_REG(0xD30C,0x00);
		LCD_WRITE_REG(0xD30D,0xA6);
		LCD_WRITE_REG(0xD30E,0x00);
		LCD_WRITE_REG(0xD30F,0xE5);
		LCD_WRITE_REG(0xD310,0x01);
		LCD_WRITE_REG(0xD311,0x13);
		LCD_WRITE_REG(0xD312,0x01);
		LCD_WRITE_REG(0xD313,0x54);
		LCD_WRITE_REG(0xD314,0x01);
		LCD_WRITE_REG(0xD315,0x82);
		LCD_WRITE_REG(0xD316,0x01);
		LCD_WRITE_REG(0xD317,0xCA);
		LCD_WRITE_REG(0xD318,0x02);
		LCD_WRITE_REG(0xD319,0x00);
		LCD_WRITE_REG(0xD31A,0x02);
		LCD_WRITE_REG(0xD31B,0x01);
		LCD_WRITE_REG(0xD31C,0x02);
		LCD_WRITE_REG(0xD31D,0x34);
		LCD_WRITE_REG(0xD31E,0x02);
		LCD_WRITE_REG(0xD31F,0x67);
		LCD_WRITE_REG(0xD320,0x02);
		LCD_WRITE_REG(0xD321,0x84);
		LCD_WRITE_REG(0xD322,0x02);
		LCD_WRITE_REG(0xD323,0xA4);
		LCD_WRITE_REG(0xD324,0x02);
		LCD_WRITE_REG(0xD325,0xB7);
		LCD_WRITE_REG(0xD326,0x02);
		LCD_WRITE_REG(0xD327,0xCF);
		LCD_WRITE_REG(0xD328,0x02);
		LCD_WRITE_REG(0xD329,0xDE);
		LCD_WRITE_REG(0xD32A,0x02);
		LCD_WRITE_REG(0xD32B,0xF2);
		LCD_WRITE_REG(0xD32C,0x02);
		LCD_WRITE_REG(0xD32D,0xFE);
		LCD_WRITE_REG(0xD32E,0x03);
		LCD_WRITE_REG(0xD32F,0x10);
		LCD_WRITE_REG(0xD330,0x03);
		LCD_WRITE_REG(0xD331,0x33);
		LCD_WRITE_REG(0xD332,0x03);
		LCD_WRITE_REG(0xD333,0x6D);
		LCD_WRITE_REG(0xD400,0x00);
		LCD_WRITE_REG(0xD401,0x33);
		LCD_WRITE_REG(0xD402,0x00);
		LCD_WRITE_REG(0xD403,0x34);
		LCD_WRITE_REG(0xD404,0x00);
		LCD_WRITE_REG(0xD405,0x3A);
		LCD_WRITE_REG(0xD406,0x00);
		LCD_WRITE_REG(0xD407,0x4A);
		LCD_WRITE_REG(0xD408,0x00);
		LCD_WRITE_REG(0xD409,0x5C);
		LCD_WRITE_REG(0xD40A,0x00);
		LCD_WRITE_REG(0xD40B,0x81);

		LCD_WRITE_REG(0xD40C,0x00);
		LCD_WRITE_REG(0xD40D,0xA6);
		LCD_WRITE_REG(0xD40E,0x00);
		LCD_WRITE_REG(0xD40F,0xE5);
		LCD_WRITE_REG(0xD410,0x01);
		LCD_WRITE_REG(0xD411,0x13);
		LCD_WRITE_REG(0xD412,0x01);
		LCD_WRITE_REG(0xD413,0x54);
		LCD_WRITE_REG(0xD414,0x01);
		LCD_WRITE_REG(0xD415,0x82);
		LCD_WRITE_REG(0xD416,0x01);
		LCD_WRITE_REG(0xD417,0xCA);
		LCD_WRITE_REG(0xD418,0x02);
		LCD_WRITE_REG(0xD419,0x00);
		LCD_WRITE_REG(0xD41A,0x02);
		LCD_WRITE_REG(0xD41B,0x01);
		LCD_WRITE_REG(0xD41C,0x02);
		LCD_WRITE_REG(0xD41D,0x34);
		LCD_WRITE_REG(0xD41E,0x02);
		LCD_WRITE_REG(0xD41F,0x67);
		LCD_WRITE_REG(0xD420,0x02);
		LCD_WRITE_REG(0xD421,0x84);
		LCD_WRITE_REG(0xD422,0x02);
		LCD_WRITE_REG(0xD423,0xA4);
		LCD_WRITE_REG(0xD424,0x02);
		LCD_WRITE_REG(0xD425,0xB7);
		LCD_WRITE_REG(0xD426,0x02);
		LCD_WRITE_REG(0xD427,0xCF);
		LCD_WRITE_REG(0xD428,0x02);
		LCD_WRITE_REG(0xD429,0xDE);
		LCD_WRITE_REG(0xD42A,0x02);
		LCD_WRITE_REG(0xD42B,0xF2);
		LCD_WRITE_REG(0xD42C,0x02);
		LCD_WRITE_REG(0xD42D,0xFE);
		LCD_WRITE_REG(0xD42E,0x03);
		LCD_WRITE_REG(0xD42F,0x10);
		LCD_WRITE_REG(0xD430,0x03);
		LCD_WRITE_REG(0xD431,0x33);
		LCD_WRITE_REG(0xD432,0x03);
		LCD_WRITE_REG(0xD433,0x6D);
		LCD_WRITE_REG(0xD500,0x00);
		LCD_WRITE_REG(0xD501,0x33);
		LCD_WRITE_REG(0xD502,0x00);
		LCD_WRITE_REG(0xD503,0x34);
		LCD_WRITE_REG(0xD504,0x00);
		LCD_WRITE_REG(0xD505,0x3A);
		LCD_WRITE_REG(0xD506,0x00);
		LCD_WRITE_REG(0xD507,0x4A);
		LCD_WRITE_REG(0xD508,0x00);
		LCD_WRITE_REG(0xD509,0x5C);
		LCD_WRITE_REG(0xD50A,0x00);
		LCD_WRITE_REG(0xD50B,0x81);

		LCD_WRITE_REG(0xD50C,0x00);
		LCD_WRITE_REG(0xD50D,0xA6);
		LCD_WRITE_REG(0xD50E,0x00);
		LCD_WRITE_REG(0xD50F,0xE5);
		LCD_WRITE_REG(0xD510,0x01);
		LCD_WRITE_REG(0xD511,0x13);
		LCD_WRITE_REG(0xD512,0x01);
		LCD_WRITE_REG(0xD513,0x54);
		LCD_WRITE_REG(0xD514,0x01);
		LCD_WRITE_REG(0xD515,0x82);
		LCD_WRITE_REG(0xD516,0x01);
		LCD_WRITE_REG(0xD517,0xCA);
		LCD_WRITE_REG(0xD518,0x02);
		LCD_WRITE_REG(0xD519,0x00);
		LCD_WRITE_REG(0xD51A,0x02);
		LCD_WRITE_REG(0xD51B,0x01);
		LCD_WRITE_REG(0xD51C,0x02);
		LCD_WRITE_REG(0xD51D,0x34);
		LCD_WRITE_REG(0xD51E,0x02);
		LCD_WRITE_REG(0xD51F,0x67);
		LCD_WRITE_REG(0xD520,0x02);
		LCD_WRITE_REG(0xD521,0x84);
		LCD_WRITE_REG(0xD522,0x02);
		LCD_WRITE_REG(0xD523,0xA4);
		LCD_WRITE_REG(0xD524,0x02);
		LCD_WRITE_REG(0xD525,0xB7);
		LCD_WRITE_REG(0xD526,0x02);
		LCD_WRITE_REG(0xD527,0xCF);
		LCD_WRITE_REG(0xD528,0x02);
		LCD_WRITE_REG(0xD529,0xDE);
		LCD_WRITE_REG(0xD52A,0x02);
		LCD_WRITE_REG(0xD52B,0xF2);
		LCD_WRITE_REG(0xD52C,0x02);
		LCD_WRITE_REG(0xD52D,0xFE);
		LCD_WRITE_REG(0xD52E,0x03);
		LCD_WRITE_REG(0xD52F,0x10);
		LCD_WRITE_REG(0xD530,0x03);
		LCD_WRITE_REG(0xD531,0x33);
		LCD_WRITE_REG(0xD532,0x03);
		LCD_WRITE_REG(0xD533,0x6D);
		LCD_WRITE_REG(0xD600,0x00);
		LCD_WRITE_REG(0xD601,0x33);
		LCD_WRITE_REG(0xD602,0x00);
		LCD_WRITE_REG(0xD603,0x34);
		LCD_WRITE_REG(0xD604,0x00);
		LCD_WRITE_REG(0xD605,0x3A);
		LCD_WRITE_REG(0xD606,0x00);
		LCD_WRITE_REG(0xD607,0x4A);
		LCD_WRITE_REG(0xD608,0x00);
		LCD_WRITE_REG(0xD609,0x5C);
		LCD_WRITE_REG(0xD60A,0x00);
		LCD_WRITE_REG(0xD60B,0x81);

		LCD_WRITE_REG(0xD60C,0x00);
		LCD_WRITE_REG(0xD60D,0xA6);
		LCD_WRITE_REG(0xD60E,0x00);
		LCD_WRITE_REG(0xD60F,0xE5);
		LCD_WRITE_REG(0xD610,0x01);
		LCD_WRITE_REG(0xD611,0x13);
		LCD_WRITE_REG(0xD612,0x01);
		LCD_WRITE_REG(0xD613,0x54);
		LCD_WRITE_REG(0xD614,0x01);
		LCD_WRITE_REG(0xD615,0x82);
		LCD_WRITE_REG(0xD616,0x01);
		LCD_WRITE_REG(0xD617,0xCA);
		LCD_WRITE_REG(0xD618,0x02);
		LCD_WRITE_REG(0xD619,0x00);
		LCD_WRITE_REG(0xD61A,0x02);
		LCD_WRITE_REG(0xD61B,0x01);
		LCD_WRITE_REG(0xD61C,0x02);
		LCD_WRITE_REG(0xD61D,0x34);
		LCD_WRITE_REG(0xD61E,0x02);
		LCD_WRITE_REG(0xD61F,0x67);
		LCD_WRITE_REG(0xD620,0x02);
		LCD_WRITE_REG(0xD621,0x84);
		LCD_WRITE_REG(0xD622,0x02);
		LCD_WRITE_REG(0xD623,0xA4);
		LCD_WRITE_REG(0xD624,0x02);
		LCD_WRITE_REG(0xD625,0xB7);
		LCD_WRITE_REG(0xD626,0x02);
		LCD_WRITE_REG(0xD627,0xCF);
		LCD_WRITE_REG(0xD628,0x02);
		LCD_WRITE_REG(0xD629,0xDE);
		LCD_WRITE_REG(0xD62A,0x02);
		LCD_WRITE_REG(0xD62B,0xF2);
		LCD_WRITE_REG(0xD62C,0x02);
		LCD_WRITE_REG(0xD62D,0xFE);
		LCD_WRITE_REG(0xD62E,0x03);
		LCD_WRITE_REG(0xD62F,0x10);
		LCD_WRITE_REG(0xD630,0x03);
		LCD_WRITE_REG(0xD631,0x33);
		LCD_WRITE_REG(0xD632,0x03);
		LCD_WRITE_REG(0xD633,0x6D);
		//LV2 Page 0 enable
		LCD_WRITE_REG(0xF000,0x55);
		LCD_WRITE_REG(0xF001,0xAA);
		LCD_WRITE_REG(0xF002,0x52);
		LCD_WRITE_REG(0xF003,0x08);
		LCD_WRITE_REG(0xF004,0x00);
		//Display control
		LCD_WRITE_REG(0xB100, 0xCC);
		LCD_WRITE_REG(0xB101, 0x00);
		//Source hold time
		LCD_WRITE_REG(0xB600,0x05);
		//Gate EQ control
		LCD_WRITE_REG(0xB700,0x70);
		LCD_WRITE_REG(0xB701,0x70);
		//Source EQ control (Mode 2)
		LCD_WRITE_REG(0xB800,0x01);
		LCD_WRITE_REG(0xB801,0x03);
		LCD_WRITE_REG(0xB802,0x03);
		LCD_WRITE_REG(0xB803,0x03);
		//Inversion mode (2-dot)
		LCD_WRITE_REG(0xBC00,0x02);
		LCD_WRITE_REG(0xBC01,0x00);
		LCD_WRITE_REG(0xBC02,0x00);
		//Timing control 4H w/ 4-delay
		LCD_WRITE_REG(0xC900,0xD0);
		LCD_WRITE_REG(0xC901,0x02);
		LCD_WRITE_REG(0xC902,0x50);
		LCD_WRITE_REG(0xC903,0x50);
		LCD_WRITE_REG(0xC904,0x50);
		LCD_WRITE_REG(0x3500,0x00);
		LCD_WRITE_REG(0x3A00,0x55);  //16-bit/pixel
		LCD_WRITE_CMD(0x1100);
		delay_us(120);
		LCD_WRITE_CMD(0x2900);
	} else if(lcddev.id==0x9325)//9325
	{
		LCD_WRITE_REG(0x00E5,0x78F0); 
		LCD_WRITE_REG(0x0001,0x0100); 
		LCD_WRITE_REG(0x0002,0x0700); 
		LCD_WRITE_REG(0x0003,0x1030); 
		LCD_WRITE_REG(0x0004,0x0000); 
		LCD_WRITE_REG(0x0008,0x0202);  
		LCD_WRITE_REG(0x0009,0x0000);
		LCD_WRITE_REG(0x000A,0x0000); 
		LCD_WRITE_REG(0x000C,0x0000); 
		LCD_WRITE_REG(0x000D,0x0000);
		LCD_WRITE_REG(0x000F,0x0000);
		//power on sequence VGHVGL
		LCD_WRITE_REG(0x0010,0x0000);   
		LCD_WRITE_REG(0x0011,0x0007);  
		LCD_WRITE_REG(0x0012,0x0000);  
		LCD_WRITE_REG(0x0013,0x0000); 
		LCD_WRITE_REG(0x0007,0x0000); 
		//vgh 
		LCD_WRITE_REG(0x0010,0x1690);   
		LCD_WRITE_REG(0x0011,0x0227);
		//delayms(100);
		//vregiout 
		LCD_WRITE_REG(0x0012,0x009D); //0x001b
		//delayms(100); 
		//vom amplitude
		LCD_WRITE_REG(0x0013,0x1900);
		//delayms(100); 
		//vom H
		LCD_WRITE_REG(0x0029,0x0025); 
		LCD_WRITE_REG(0x002B,0x000D); 
		//gamma
		LCD_WRITE_REG(0x0030,0x0007);
		LCD_WRITE_REG(0x0031,0x0303);
		LCD_WRITE_REG(0x0032,0x0003);// 0006
		LCD_WRITE_REG(0x0035,0x0206);
		LCD_WRITE_REG(0x0036,0x0008);
		LCD_WRITE_REG(0x0037,0x0406); 
		LCD_WRITE_REG(0x0038,0x0304);//0200
		LCD_WRITE_REG(0x0039,0x0007); 
		LCD_WRITE_REG(0x003C,0x0602);// 0504
		LCD_WRITE_REG(0x003D,0x0008); 
		//ram
		LCD_WRITE_REG(0x0050,0x0000); 
		LCD_WRITE_REG(0x0051,0x00EF);
		LCD_WRITE_REG(0x0052,0x0000); 
		LCD_WRITE_REG(0x0053,0x013F);  
		LCD_WRITE_REG(0x0060,0xA700); 
		LCD_WRITE_REG(0x0061,0x0001); 
		LCD_WRITE_REG(0x006A,0x0000); 
		//
		LCD_WRITE_REG(0x0080,0x0000); 
		LCD_WRITE_REG(0x0081,0x0000); 
		LCD_WRITE_REG(0x0082,0x0000); 
		LCD_WRITE_REG(0x0083,0x0000); 
		LCD_WRITE_REG(0x0084,0x0000); 
		LCD_WRITE_REG(0x0085,0x0000); 
		//
		LCD_WRITE_REG(0x0090,0x0010); 
		LCD_WRITE_REG(0x0092,0x0600); 
		
		LCD_WRITE_REG(0x0007,0x0133);
		LCD_WRITE_REG(0x00,0x0022);//
	} else if(lcddev.id==0x9328)//ILI9328   OK  
	{
  		LCD_WRITE_REG(0x00EC,0x108F);// internal timeing      
 		LCD_WRITE_REG(0x00EF,0x1234);// ADD        
		//LCD_WRITE_REG(0x00e7,0x0010);      
        //LCD_WRITE_REG(0x0000,0x0001);//开启内部时钟
        LCD_WRITE_REG(0x0001,0x0100);     
        LCD_WRITE_REG(0x0002,0x0700);//电源开启                    
		//LCD_WRITE_REG(0x0003,(1<<3)|(1<<4) ); 	//65K  RGB
		//DRIVE TABLE(寄存器 03H)
		//BIT3=AM BIT4:5=ID0:1
		//AM ID0 ID1   FUNCATION
		// 0  0   0	   R->L D->U
		// 1  0   0	   D->U	R->L
		// 0  1   0	   L->R D->U
		// 1  1   0    D->U	L->R
		// 0  0   1	   R->L U->D
		// 1  0   1    U->D	R->L
		// 0  1   1    L->R U->D 正常就用这个.
		// 1  1   1	   U->D	L->R
        LCD_WRITE_REG(0x0003,(1<<12)|(3<<4)|(0<<3) );//65K    
        LCD_WRITE_REG(0x0004,0x0000);                                   
        LCD_WRITE_REG(0x0008,0x0202);	           
        LCD_WRITE_REG(0x0009,0x0000);         
        LCD_WRITE_REG(0x000a,0x0000);//display setting         
        LCD_WRITE_REG(0x000c,0x0001);//display setting          
        LCD_WRITE_REG(0x000d,0x0000);//0f3c          
        LCD_WRITE_REG(0x000f,0x0000);
		//电源配置
        LCD_WRITE_REG(0x0010,0x0000);   
        LCD_WRITE_REG(0x0011,0x0007);
        LCD_WRITE_REG(0x0012,0x0000);                                                                 
        LCD_WRITE_REG(0x0013,0x0000);                 
     	LCD_WRITE_REG(0x0007,0x0001);                 
       	delay_ms(50); 
        LCD_WRITE_REG(0x0010,0x1490);   
        LCD_WRITE_REG(0x0011,0x0227);
        delay_ms(50); 
        LCD_WRITE_REG(0x0012,0x008A);                  
        delay_ms(50); 
        LCD_WRITE_REG(0x0013,0x1a00);   
        LCD_WRITE_REG(0x0029,0x0006);
        LCD_WRITE_REG(0x002b,0x000d);
        delay_ms(50); 
        LCD_WRITE_REG(0x0020,0x0000);                                                            
        LCD_WRITE_REG(0x0021,0x0000);           
		delay_ms(50); 
		//伽马校正
        LCD_WRITE_REG(0x0030,0x0000); 
        LCD_WRITE_REG(0x0031,0x0604);   
        LCD_WRITE_REG(0x0032,0x0305);
        LCD_WRITE_REG(0x0035,0x0000);
        LCD_WRITE_REG(0x0036,0x0C09); 
        LCD_WRITE_REG(0x0037,0x0204);
        LCD_WRITE_REG(0x0038,0x0301);        
        LCD_WRITE_REG(0x0039,0x0707);     
        LCD_WRITE_REG(0x003c,0x0000);
        LCD_WRITE_REG(0x003d,0x0a0a);
        delay_ms(50); 
        LCD_WRITE_REG(0x0050,0x0000); //水平GRAM起始位置 
        LCD_WRITE_REG(0x0051,0x00ef); //水平GRAM终止位置                    
        LCD_WRITE_REG(0x0052,0x0000); //垂直GRAM起始位置                    
        LCD_WRITE_REG(0x0053,0x013f); //垂直GRAM终止位置  
 
         LCD_WRITE_REG(0x0060,0xa700);        
        LCD_WRITE_REG(0x0061,0x0001); 
        LCD_WRITE_REG(0x006a,0x0000);
        LCD_WRITE_REG(0x0080,0x0000);
        LCD_WRITE_REG(0x0081,0x0000);
        LCD_WRITE_REG(0x0082,0x0000);
        LCD_WRITE_REG(0x0083,0x0000);
        LCD_WRITE_REG(0x0084,0x0000);
        LCD_WRITE_REG(0x0085,0x0000);
      
        LCD_WRITE_REG(0x0090,0x0010);     
        LCD_WRITE_REG(0x0092,0x0600);  
        //开启显示设置    
        LCD_WRITE_REG(0x0007,0x0133); 
	} else if(lcddev.id==0x9320)//测试OK.
	{
		LCD_WRITE_REG(0x00,0x0000);
		LCD_WRITE_REG(0x01,0x0100);	//Driver Output Contral.
		LCD_WRITE_REG(0x02,0x0700);	//LCD Driver Waveform Contral.
		LCD_WRITE_REG(0x03,0x1030);//Entry Mode Set.
		//LCD_WRITE_REG(0x03,0x1018);	//Entry Mode Set.
	
		LCD_WRITE_REG(0x04,0x0000);	//Scalling Contral.
		LCD_WRITE_REG(0x08,0x0202);	//Display Contral 2.(0x0207)
		LCD_WRITE_REG(0x09,0x0000);	//Display Contral 3.(0x0000)
		LCD_WRITE_REG(0x0a,0x0000);	//Frame Cycle Contal.(0x0000)
		LCD_WRITE_REG(0x0c,(1<<0));	//Extern Display Interface Contral 1.(0x0000)
		LCD_WRITE_REG(0x0d,0x0000);	//Frame Maker Position.
		LCD_WRITE_REG(0x0f,0x0000);	//Extern Display Interface Contral 2.	    
		delay_ms(50); 
		LCD_WRITE_REG(0x07,0x0101);	//Display Contral.
		delay_ms(50); 								  
		LCD_WRITE_REG(0x10,(1<<12)|(0<<8)|(1<<7)|(1<<6)|(0<<4));	//Power Control 1.(0x16b0)
		LCD_WRITE_REG(0x11,0x0007);								//Power Control 2.(0x0001)
		LCD_WRITE_REG(0x12,(1<<8)|(1<<4)|(0<<0));				//Power Control 3.(0x0138)
		LCD_WRITE_REG(0x13,0x0b00);								//Power Control 4.
		LCD_WRITE_REG(0x29,0x0000);								//Power Control 7.
	
		LCD_WRITE_REG(0x2b,(1<<14)|(1<<4));	    
		LCD_WRITE_REG(0x50,0);	//Set X Star
		//水平GRAM终止位置Set X End.
		LCD_WRITE_REG(0x51,239);	//Set Y Star
		LCD_WRITE_REG(0x52,0);	//Set Y End.t.
		LCD_WRITE_REG(0x53,319);	//
	
		LCD_WRITE_REG(0x60,0x2700);	//Driver Output Control.
		LCD_WRITE_REG(0x61,0x0001);	//Driver Output Control.
		LCD_WRITE_REG(0x6a,0x0000);	//Vertical Srcoll Control.
	
		LCD_WRITE_REG(0x80,0x0000);	//Display Position? Partial Display 1.
		LCD_WRITE_REG(0x81,0x0000);	//RAM Address Start? Partial Display 1.
		LCD_WRITE_REG(0x82,0x0000);	//RAM Address End-Partial Display 1.
		LCD_WRITE_REG(0x83,0x0000);	//Displsy Position? Partial Display 2.
		LCD_WRITE_REG(0x84,0x0000);	//RAM Address Start? Partial Display 2.
		LCD_WRITE_REG(0x85,0x0000);	//RAM Address End? Partial Display 2.
	
		LCD_WRITE_REG(0x90,(0<<7)|(16<<0));	//Frame Cycle Contral.(0x0013)
		LCD_WRITE_REG(0x92,0x0000);	//Panel Interface Contral 2.(0x0000)
		LCD_WRITE_REG(0x93,0x0001);	//Panel Interface Contral 3.
		LCD_WRITE_REG(0x95,0x0110);	//Frame Cycle Contral.(0x0110)
		LCD_WRITE_REG(0x97,(0<<8));	//
		LCD_WRITE_REG(0x98,0x0000);	//Frame Cycle Contral.	   
		LCD_WRITE_REG(0x07,0x0173);	//(0x0173)
	} else if(lcddev.id==0X9331)//OK |/|/|			 
	{
		LCD_WRITE_REG(0x00E7, 0x1014);
		LCD_WRITE_REG(0x0001, 0x0100); // set SS and SM bit
		LCD_WRITE_REG(0x0002, 0x0200); // set 1 line inversion
        LCD_WRITE_REG(0x0003,(1<<12)|(3<<4)|(1<<3));//65K    
		//LCD_WRITE_REG(0x0003, 0x1030); // set GRAM write direction and BGR=1.
		LCD_WRITE_REG(0x0008, 0x0202); // set the back porch and front porch
		LCD_WRITE_REG(0x0009, 0x0000); // set non-display area refresh cycle ISC[3:0]
		LCD_WRITE_REG(0x000A, 0x0000); // FMARK function
		LCD_WRITE_REG(0x000C, 0x0000); // RGB interface setting
		LCD_WRITE_REG(0x000D, 0x0000); // Frame marker Position
		LCD_WRITE_REG(0x000F, 0x0000); // RGB interface polarity
		//*************Power On sequence ****************//
		LCD_WRITE_REG(0x0010, 0x0000); // SAP, BT[3:0], AP, DSTB, SLP, STB
		LCD_WRITE_REG(0x0011, 0x0007); // DC1[2:0], DC0[2:0], VC[2:0]
		LCD_WRITE_REG(0x0012, 0x0000); // VREG1OUT voltage
		LCD_WRITE_REG(0x0013, 0x0000); // VDV[4:0] for VCOM amplitude
		delay_ms(200); // Dis-charge capacitor power voltage
		LCD_WRITE_REG(0x0010, 0x1690); // SAP, BT[3:0], AP, DSTB, SLP, STB
		LCD_WRITE_REG(0x0011, 0x0227); // DC1[2:0], DC0[2:0], VC[2:0]
		delay_ms(50); // Delay 50ms
		LCD_WRITE_REG(0x0012, 0x000C); // Internal reference voltage= Vci;
		delay_ms(50); // Delay 50ms
		LCD_WRITE_REG(0x0013, 0x0800); // Set VDV[4:0] for VCOM amplitude
		LCD_WRITE_REG(0x0029, 0x0011); // Set VCM[5:0] for VCOMH
		LCD_WRITE_REG(0x002B, 0x000B); // Set Frame Rate
		delay_ms(50); // Delay 50ms
		LCD_WRITE_REG(0x0020, 0x0000); // GRAM horizontal Address
		LCD_WRITE_REG(0x0021, 0x013f); // GRAM Vertical Address
		// ----------- Adjust the Gamma Curve ----------//
		LCD_WRITE_REG(0x0030, 0x0000);
		LCD_WRITE_REG(0x0031, 0x0106);
		LCD_WRITE_REG(0x0032, 0x0000);
		LCD_WRITE_REG(0x0035, 0x0204);
		LCD_WRITE_REG(0x0036, 0x160A);
		LCD_WRITE_REG(0x0037, 0x0707);
		LCD_WRITE_REG(0x0038, 0x0106);
		LCD_WRITE_REG(0x0039, 0x0707);
		LCD_WRITE_REG(0x003C, 0x0402);
		LCD_WRITE_REG(0x003D, 0x0C0F);
		//------------------ Set GRAM area ---------------//
		LCD_WRITE_REG(0x0050, 0x0000); // Horizontal GRAM Start Address
		LCD_WRITE_REG(0x0051, 0x00EF); // Horizontal GRAM End Address
		LCD_WRITE_REG(0x0052, 0x0000); // Vertical GRAM Start Address
		LCD_WRITE_REG(0x0053, 0x013F); // Vertical GRAM Start Address
		LCD_WRITE_REG(0x0060, 0x2700); // Gate Scan Line
		LCD_WRITE_REG(0x0061, 0x0001); // NDL,VLE, REV 
		LCD_WRITE_REG(0x006A, 0x0000); // set scrolling line
		//-------------- Partial Display Control ---------//
		LCD_WRITE_REG(0x0080, 0x0000);
		LCD_WRITE_REG(0x0081, 0x0000);
		LCD_WRITE_REG(0x0082, 0x0000);
		LCD_WRITE_REG(0x0083, 0x0000);
		LCD_WRITE_REG(0x0084, 0x0000);
		LCD_WRITE_REG(0x0085, 0x0000);
		//-------------- Panel Control -------------------//
		LCD_WRITE_REG(0x0090, 0x0010);
		LCD_WRITE_REG(0x0092, 0x0600);
		LCD_WRITE_REG(0x0007, 0x0133); // 262K color and display ON
	} else if(lcddev.id==0x5408)
	{
		LCD_WRITE_REG(0x01,0x0100);								  
		LCD_WRITE_REG(0x02,0x0700);//LCD Driving Waveform Contral 
		LCD_WRITE_REG(0x03,0x1030);//Entry Mode设置 	   
		//指针从左至右自上而下的自动增模式
		//Normal Mode(Window Mode disable)
		//RGB格式
		//16位数据2次传输的8总线设置
		LCD_WRITE_REG(0x04,0x0000); //Scalling Control register     
		LCD_WRITE_REG(0x08,0x0207); //Display Control 2 
		LCD_WRITE_REG(0x09,0x0000); //Display Control 3	 
		LCD_WRITE_REG(0x0A,0x0000); //Frame Cycle Control	 
		LCD_WRITE_REG(0x0C,0x0000); //External Display Interface Control 1 
		LCD_WRITE_REG(0x0D,0x0000); //Frame Maker Position		 
		LCD_WRITE_REG(0x0F,0x0000); //External Display Interface Control 2 
 		delay_ms(20);
		//TFT 液晶彩色图像显示方法14
		LCD_WRITE_REG(0x10,0x16B0); //0x14B0 //Power Control 1
		LCD_WRITE_REG(0x11,0x0001); //0x0007 //Power Control 2
		LCD_WRITE_REG(0x17,0x0001); //0x0000 //Power Control 3
		LCD_WRITE_REG(0x12,0x0138); //0x013B //Power Control 4
		LCD_WRITE_REG(0x13,0x0800); //0x0800 //Power Control 5
		LCD_WRITE_REG(0x29,0x0009); //NVM read data 2
		LCD_WRITE_REG(0x2a,0x0009); //NVM read data 3
		LCD_WRITE_REG(0xa4,0x0000);	 
		LCD_WRITE_REG(0x50,0x0000); //设置操作窗口的X轴开始列
		LCD_WRITE_REG(0x51,0x00EF); //设置操作窗口的X轴结束列
		LCD_WRITE_REG(0x52,0x0000); //设置操作窗口的Y轴开始行
		LCD_WRITE_REG(0x53,0x013F); //设置操作窗口的Y轴结束行
		LCD_WRITE_REG(0x60,0x2700); //Driver Output Control
		//设置屏幕的点数以及扫描的起始行
		LCD_WRITE_REG(0x61,0x0001); //Driver Output Control
		LCD_WRITE_REG(0x6A,0x0000); //Vertical Scroll Control
		LCD_WRITE_REG(0x80,0x0000); //Display Position – Partial Display 1
		LCD_WRITE_REG(0x81,0x0000); //RAM Address Start – Partial Display 1
		LCD_WRITE_REG(0x82,0x0000); //RAM address End - Partial Display 1
		LCD_WRITE_REG(0x83,0x0000); //Display Position – Partial Display 2
		LCD_WRITE_REG(0x84,0x0000); //RAM Address Start – Partial Display 2
		LCD_WRITE_REG(0x85,0x0000); //RAM address End – Partail Display2
		LCD_WRITE_REG(0x90,0x0013); //Frame Cycle Control
		LCD_WRITE_REG(0x92,0x0000);  //Panel Interface Control 2
		LCD_WRITE_REG(0x93,0x0003); //Panel Interface control 3
		LCD_WRITE_REG(0x95,0x0110);  //Frame Cycle Control
		LCD_WRITE_REG(0x07,0x0173);		 
		delay_ms(50);
	} else if(lcddev.id==0x1505)//OK
	{
		// second release on 3/5  ,luminance is acceptable,water wave appear during camera preview
        LCD_WRITE_REG(0x0007,0x0000);
        delay_ms(50); 
        LCD_WRITE_REG(0x0012,0x011C);//0x011A   why need to set several times?
        LCD_WRITE_REG(0x00A4,0x0001);//NVM	 
        LCD_WRITE_REG(0x0008,0x000F);
        LCD_WRITE_REG(0x000A,0x0008);
        LCD_WRITE_REG(0x000D,0x0008);	    
  		//伽马校正
        LCD_WRITE_REG(0x0030,0x0707);
        LCD_WRITE_REG(0x0031,0x0007); //0x0707
        LCD_WRITE_REG(0x0032,0x0603); 
        LCD_WRITE_REG(0x0033,0x0700); 
        LCD_WRITE_REG(0x0034,0x0202); 
        LCD_WRITE_REG(0x0035,0x0002); //?0x0606
        LCD_WRITE_REG(0x0036,0x1F0F);
        LCD_WRITE_REG(0x0037,0x0707); //0x0f0f  0x0105
        LCD_WRITE_REG(0x0038,0x0000); 
        LCD_WRITE_REG(0x0039,0x0000); 
        LCD_WRITE_REG(0x003A,0x0707); 
        LCD_WRITE_REG(0x003B,0x0000); //0x0303
        LCD_WRITE_REG(0x003C,0x0007); //?0x0707
        LCD_WRITE_REG(0x003D,0x0000); //0x1313//0x1f08
        delay_ms(50); 
        LCD_WRITE_REG(0x0007,0x0001);
        LCD_WRITE_REG(0x0017,0x0001);//开启电源
        delay_ms(50); 
  		//电源配置
        LCD_WRITE_REG(0x0010,0x17A0); 
        LCD_WRITE_REG(0x0011,0x0217);//reference voltage VC[2:0]   Vciout = 1.00*Vcivl
        LCD_WRITE_REG(0x0012,0x011E);//0x011c  //Vreg1out = Vcilvl*1.80   is it the same as Vgama1out ?
        LCD_WRITE_REG(0x0013,0x0F00);//VDV[4:0]-->VCOM Amplitude VcomL = VcomH - Vcom Ampl
        LCD_WRITE_REG(0x002A,0x0000);  
        LCD_WRITE_REG(0x0029,0x000A);//0x0001F  Vcomh = VCM1[4:0]*Vreg1out    gate source voltage??
        LCD_WRITE_REG(0x0012,0x013E);// 0x013C  power supply on
        //Coordinates Control//
        LCD_WRITE_REG(0x0050,0x0000);//0x0e00
        LCD_WRITE_REG(0x0051,0x00EF); 
        LCD_WRITE_REG(0x0052,0x0000); 
        LCD_WRITE_REG(0x0053,0x013F); 
    	//Pannel Image Control//
        LCD_WRITE_REG(0x0060,0x2700); 
        LCD_WRITE_REG(0x0061,0x0001); 
        LCD_WRITE_REG(0x006A,0x0000); 
        LCD_WRITE_REG(0x0080,0x0000); 
    	//Partial Image Control//
        LCD_WRITE_REG(0x0081,0x0000); 
        LCD_WRITE_REG(0x0082,0x0000); 
        LCD_WRITE_REG(0x0083,0x0000); 
        LCD_WRITE_REG(0x0084,0x0000); 
        LCD_WRITE_REG(0x0085,0x0000); 
  		//Panel Interface Control//
        LCD_WRITE_REG(0x0090,0x0013);//0x0010 frenqucy
        LCD_WRITE_REG(0x0092,0x0300); 
        LCD_WRITE_REG(0x0093,0x0005); 
        LCD_WRITE_REG(0x0095,0x0000); 
        LCD_WRITE_REG(0x0097,0x0000); 
        LCD_WRITE_REG(0x0098,0x0000); 
  
        LCD_WRITE_REG(0x0001,0x0100); 
        LCD_WRITE_REG(0x0002,0x0700); 
        LCD_WRITE_REG(0x0003,0x1038);//扫描方向 上->下  左->右 
        LCD_WRITE_REG(0x0004,0x0000); 
        LCD_WRITE_REG(0x000C,0x0000); 
        LCD_WRITE_REG(0x000F,0x0000); 
        LCD_WRITE_REG(0x0020,0x0000); 
        LCD_WRITE_REG(0x0021,0x0000); 
        LCD_WRITE_REG(0x0007,0x0021); 
        delay_ms(20);
        LCD_WRITE_REG(0x0007,0x0061); 
        delay_ms(20);
        LCD_WRITE_REG(0x0007,0x0173); 
        delay_ms(20);
	} else if(lcddev.id==0xB505)
	{
		LCD_WRITE_REG(0x0000,0x0000);
		LCD_WRITE_REG(0x0000,0x0000);
		LCD_WRITE_REG(0x0000,0x0000);
		LCD_WRITE_REG(0x0000,0x0000);
		
		LCD_WRITE_REG(0x00a4,0x0001);
		delay_ms(20);		  
		LCD_WRITE_REG(0x0060,0x2700);
		LCD_WRITE_REG(0x0008,0x0202);
		
		LCD_WRITE_REG(0x0030,0x0214);
		LCD_WRITE_REG(0x0031,0x3715);
		LCD_WRITE_REG(0x0032,0x0604);
		LCD_WRITE_REG(0x0033,0x0e16);
		LCD_WRITE_REG(0x0034,0x2211);
		LCD_WRITE_REG(0x0035,0x1500);
		LCD_WRITE_REG(0x0036,0x8507);
		LCD_WRITE_REG(0x0037,0x1407);
		LCD_WRITE_REG(0x0038,0x1403);
		LCD_WRITE_REG(0x0039,0x0020);
		
		LCD_WRITE_REG(0x0090,0x001a);
		LCD_WRITE_REG(0x0010,0x0000);
		LCD_WRITE_REG(0x0011,0x0007);
		LCD_WRITE_REG(0x0012,0x0000);
		LCD_WRITE_REG(0x0013,0x0000);
		delay_ms(20);
		
		LCD_WRITE_REG(0x0010,0x0730);
		LCD_WRITE_REG(0x0011,0x0137);
		delay_ms(20);
		
		LCD_WRITE_REG(0x0012,0x01b8);
		delay_ms(20);
		
		LCD_WRITE_REG(0x0013,0x0f00);
		LCD_WRITE_REG(0x002a,0x0080);
		LCD_WRITE_REG(0x0029,0x0048);
		delay_ms(20);
		
		LCD_WRITE_REG(0x0001,0x0100);
		LCD_WRITE_REG(0x0002,0x0700);
        LCD_WRITE_REG(0x0003,0x1038);//扫描方向 上->下  左->右 
		LCD_WRITE_REG(0x0008,0x0202);
		LCD_WRITE_REG(0x000a,0x0000);
		LCD_WRITE_REG(0x000c,0x0000);
		LCD_WRITE_REG(0x000d,0x0000);
		LCD_WRITE_REG(0x000e,0x0030);
		LCD_WRITE_REG(0x0050,0x0000);
		LCD_WRITE_REG(0x0051,0x00ef);
		LCD_WRITE_REG(0x0052,0x0000);
		LCD_WRITE_REG(0x0053,0x013f);
		LCD_WRITE_REG(0x0060,0x2700);
		LCD_WRITE_REG(0x0061,0x0001);
		LCD_WRITE_REG(0x006a,0x0000);
		//LCD_WRITE_REG(0x0080,0x0000);
		//LCD_WRITE_REG(0x0081,0x0000);
		LCD_WRITE_REG(0x0090,0X0011);
		LCD_WRITE_REG(0x0092,0x0600);
		LCD_WRITE_REG(0x0093,0x0402);
		LCD_WRITE_REG(0x0094,0x0002);
		delay_ms(20);
		
		LCD_WRITE_REG(0x0007,0x0001);
		delay_ms(20);
		LCD_WRITE_REG(0x0007,0x0061);
		LCD_WRITE_REG(0x0007,0x0173);
		
		LCD_WRITE_REG(0x0020,0x0000);
		LCD_WRITE_REG(0x0021,0x0000);	  
		LCD_WRITE_REG(0x00,0x22);  
	} else if(lcddev.id==0xC505)
	{
		LCD_WRITE_REG(0x0000,0x0000);
		LCD_WRITE_REG(0x0000,0x0000);
		delay_ms(20);		  
		LCD_WRITE_REG(0x0000,0x0000);
		LCD_WRITE_REG(0x0000,0x0000);
		LCD_WRITE_REG(0x0000,0x0000);
		LCD_WRITE_REG(0x0000,0x0000);
 		LCD_WRITE_REG(0x00a4,0x0001);
		delay_ms(20);		  
		LCD_WRITE_REG(0x0060,0x2700);
		LCD_WRITE_REG(0x0008,0x0806);
		
		LCD_WRITE_REG(0x0030,0x0703);//gamma setting
		LCD_WRITE_REG(0x0031,0x0001);
		LCD_WRITE_REG(0x0032,0x0004);
		LCD_WRITE_REG(0x0033,0x0102);
		LCD_WRITE_REG(0x0034,0x0300);
		LCD_WRITE_REG(0x0035,0x0103);
		LCD_WRITE_REG(0x0036,0x001F);
		LCD_WRITE_REG(0x0037,0x0703);
		LCD_WRITE_REG(0x0038,0x0001);
		LCD_WRITE_REG(0x0039,0x0004);
		
		
		
		LCD_WRITE_REG(0x0090, 0x0015);	//80Hz
		LCD_WRITE_REG(0x0010, 0X0410);	//BT,AP
		LCD_WRITE_REG(0x0011,0x0247);	//DC1,DC0,VC
		LCD_WRITE_REG(0x0012, 0x01BC);
		LCD_WRITE_REG(0x0013, 0x0e00);
		delay_ms(120);
		LCD_WRITE_REG(0x0001, 0x0100);
		LCD_WRITE_REG(0x0002, 0x0200);
		LCD_WRITE_REG(0x0003, 0x1030);
		
		LCD_WRITE_REG(0x000A, 0x0008);
		LCD_WRITE_REG(0x000C, 0x0000);
		
		LCD_WRITE_REG(0x000E, 0x0020);
		LCD_WRITE_REG(0x000F, 0x0000);
		LCD_WRITE_REG(0x0020, 0x0000);	//H Start
		LCD_WRITE_REG(0x0021, 0x0000);	//V Start
		LCD_WRITE_REG(0x002A,0x003D);	//vcom2
		delay_ms(20);
		LCD_WRITE_REG(0x0029, 0x002d);
		LCD_WRITE_REG(0x0050, 0x0000);
		LCD_WRITE_REG(0x0051, 0xD0EF);
		LCD_WRITE_REG(0x0052, 0x0000);
		LCD_WRITE_REG(0x0053, 0x013F);
		LCD_WRITE_REG(0x0061, 0x0000);
		LCD_WRITE_REG(0x006A, 0x0000);
		LCD_WRITE_REG(0x0092,0x0300); 
 
 		LCD_WRITE_REG(0x0093, 0x0005);
		LCD_WRITE_REG(0x0007, 0x0100);
	} else if(lcddev.id==0x4531)//OK |/|/|
	{
		LCD_WRITE_REG(0X00,0X0001);   
		delay_ms(10);   
		LCD_WRITE_REG(0X10,0X1628);   
		LCD_WRITE_REG(0X12,0X000e);//0x0006    
		LCD_WRITE_REG(0X13,0X0A39);   
		delay_ms(10);   
		LCD_WRITE_REG(0X11,0X0040);   
		LCD_WRITE_REG(0X15,0X0050);   
		delay_ms(10);   
		LCD_WRITE_REG(0X12,0X001e);//16    
		delay_ms(10);   
		LCD_WRITE_REG(0X10,0X1620);   
		LCD_WRITE_REG(0X13,0X2A39);   
		delay_ms(10);   
		LCD_WRITE_REG(0X01,0X0100);   
		LCD_WRITE_REG(0X02,0X0300);   
		LCD_WRITE_REG(0X03,0X1038);//改变方向的   
		LCD_WRITE_REG(0X08,0X0202);   
		LCD_WRITE_REG(0X0A,0X0008);   
		LCD_WRITE_REG(0X30,0X0000);   
		LCD_WRITE_REG(0X31,0X0402);   
		LCD_WRITE_REG(0X32,0X0106);   
		LCD_WRITE_REG(0X33,0X0503);   
		LCD_WRITE_REG(0X34,0X0104);   
		LCD_WRITE_REG(0X35,0X0301);   
		LCD_WRITE_REG(0X36,0X0707);   
		LCD_WRITE_REG(0X37,0X0305);   
		LCD_WRITE_REG(0X38,0X0208);   
		LCD_WRITE_REG(0X39,0X0F0B);   
		LCD_WRITE_REG(0X41,0X0002);   
		LCD_WRITE_REG(0X60,0X2700);   
		LCD_WRITE_REG(0X61,0X0001);   
		LCD_WRITE_REG(0X90,0X0210);   
		LCD_WRITE_REG(0X92,0X010A);   
		LCD_WRITE_REG(0X93,0X0004);   
		LCD_WRITE_REG(0XA0,0X0100);   
		LCD_WRITE_REG(0X07,0X0001);   
		LCD_WRITE_REG(0X07,0X0021);   
		LCD_WRITE_REG(0X07,0X0023);   
		LCD_WRITE_REG(0X07,0X0033);   
		LCD_WRITE_REG(0X07,0X0133);   
		LCD_WRITE_REG(0XA0,0X0000); 
	} else if(lcddev.id==0x4535)
	{			      
		LCD_WRITE_REG(0X15,0X0030);   
		LCD_WRITE_REG(0X9A,0X0010);   
 		LCD_WRITE_REG(0X11,0X0020);   
 		LCD_WRITE_REG(0X10,0X3428);   
		LCD_WRITE_REG(0X12,0X0002);//16    
 		LCD_WRITE_REG(0X13,0X1038);   
		delay_ms(40);   
		LCD_WRITE_REG(0X12,0X0012);//16    
		delay_ms(40);   
  		LCD_WRITE_REG(0X10,0X3420);   
 		LCD_WRITE_REG(0X13,0X3038);   
		delay_ms(70);   
		LCD_WRITE_REG(0X30,0X0000);   
		LCD_WRITE_REG(0X31,0X0402);   
		LCD_WRITE_REG(0X32,0X0307);   
		LCD_WRITE_REG(0X33,0X0304);   
		LCD_WRITE_REG(0X34,0X0004);   
		LCD_WRITE_REG(0X35,0X0401);   
		LCD_WRITE_REG(0X36,0X0707);   
		LCD_WRITE_REG(0X37,0X0305);   
		LCD_WRITE_REG(0X38,0X0610);   
		LCD_WRITE_REG(0X39,0X0610); 
		  
		LCD_WRITE_REG(0X01,0X0100);   
		LCD_WRITE_REG(0X02,0X0300);   
		LCD_WRITE_REG(0X03,0X1030);//改变方向的   
		LCD_WRITE_REG(0X08,0X0808);   
		LCD_WRITE_REG(0X0A,0X0008);   
 		LCD_WRITE_REG(0X60,0X2700);   
		LCD_WRITE_REG(0X61,0X0001);   
		LCD_WRITE_REG(0X90,0X013E);   
		LCD_WRITE_REG(0X92,0X0100);   
		LCD_WRITE_REG(0X93,0X0100);   
 		LCD_WRITE_REG(0XA0,0X3000);   
 		LCD_WRITE_REG(0XA3,0X0010);   
		LCD_WRITE_REG(0X07,0X0001);   
		LCD_WRITE_REG(0X07,0X0021);   
		LCD_WRITE_REG(0X07,0X0023);   
		LCD_WRITE_REG(0X07,0X0033);   
		LCD_WRITE_REG(0X07,0X0133);   
	} else if(lcddev.id==0X1963)
	{
		LCD_WRITE_CMD(0xE2);		//Set PLL with OSC = 10MHz (hardware),	Multiplier N = 35, 250MHz < VCO < 800MHz = OSC*(N+1), VCO = 360MHz
		LCD_WRITE_DATA(0x23);		//参数1 
		LCD_WRITE_DATA(0x02);		//参数2 Divider M = 2, PLL = 360/(M+1) = 120MHz
		LCD_WRITE_DATA(0x04);		//参数3 Validate M and N values   
		delay_us(100);
		LCD_WRITE_CMD(0xE0);		// Start PLL command
		LCD_WRITE_DATA(0x01);		// enable PLL
		delay_ms(10);
		LCD_WRITE_CMD(0xE0);		// Start PLL command again
		LCD_WRITE_DATA(0x03);		// now, use PLL output as system clock	
		delay_ms(12);  
		LCD_WRITE_CMD(0x01);		//软复位
		delay_ms(10);
		
		LCD_WRITE_CMD(0xE6);		//设置像素频率
		LCD_WRITE_DATA(0x03);
		LCD_WRITE_DATA(0xFF);
		LCD_WRITE_DATA(0xFF);
		
		LCD_WRITE_CMD(0xB0);		//设置LCD模式
		LCD_WRITE_DATA(0x20);		//24位模式
		LCD_WRITE_DATA(0x00);		//TFT 模式 
	
		LCD_WRITE_DATA((ILI9341_HOR_RESOLUTION-1)>>8);//设置LCD水平像素
		LCD_WRITE_DATA(ILI9341_HOR_RESOLUTION-1);		 
		LCD_WRITE_DATA((ILI9341_VER_RESOLUTION-1)>>8);//设置LCD垂直像素
		LCD_WRITE_DATA(ILI9341_VER_RESOLUTION-1);		 
		LCD_WRITE_DATA(0x00);		//RGB序列 
		
		LCD_WRITE_CMD(0xB4);		//Set horizontal period
		LCD_WRITE_DATA((ILI9341_HT-1)>>8);
		LCD_WRITE_DATA(ILI9341_HT-1);
		LCD_WRITE_DATA((ILI9341_HPS-1)>>8);
		LCD_WRITE_DATA(ILI9341_HPS-1);
		LCD_WRITE_DATA(ILI9341_HOR_PULSE_WIDTH-1);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_CMD(0xB6);		//Set vertical period
		LCD_WRITE_DATA((ILI9341_VT-1)>>8);
		LCD_WRITE_DATA(ILI9341_VT-1);
		LCD_WRITE_DATA((ILI9341_VSP-1)>>8);
		LCD_WRITE_DATA(ILI9341_VSP-1);
		LCD_WRITE_DATA(ILI9341_VER_FRONT_PORCH-1);
		LCD_WRITE_DATA(0x00);
		LCD_WRITE_DATA(0x00);
		
		LCD_WRITE_CMD(0xF0);	//设置SSD1963与CPU接口为16bit  
		LCD_WRITE_DATA(0x03);	//16-bit(565 format) data for 16bpp 

		LCD_WRITE_CMD(0x29);	//开启显示
		//设置PWM输出  背光通过占空比可调 
		LCD_WRITE_CMD(0xD0);	//设置自动白平衡DBC
		LCD_WRITE_DATA(0x00);	//disable

		LCD_WRITE_CMD(0xBE);	//配置PWM输出
		LCD_WRITE_DATA(0x05);	//1设置PWM频率
		LCD_WRITE_DATA(0xFE);	//2设置PWM占空比
		LCD_WRITE_DATA(0x01);	//3设置C
		LCD_WRITE_DATA(0xFF);	//4设置D
		LCD_WRITE_DATA(0x00);	//5设置E 
		
		LCD_WRITE_CMD(0xB8);	//设置GPIO配置
		LCD_WRITE_DATA(0x0F);	//4个IO口设置成输出
		LCD_WRITE_DATA(0x01);	//GPIO使用正常的IO功能 
		LCD_WRITE_CMD(0xBA);
		LCD_WRITE_DATA(0X01);	//GPIO[1:0]=01,控制LCD方向
	#if (USE_LCD_CONTROLLER == 2) /* SSD1963 */		
		LCD_SSD_BackLightSet(100);//背光设置为最亮
	#endif
	}		 
	LCD_Display_Dir(0);		//默认为竖屏
	LCD_BL = 1;				//点亮背光
	LCD_ClearAll(WHITE);      
}

#if (TRANS_USE_DMA == 1)  /* 使用DMA */

/*  ========================================  DMA 初始化  ========================================  */
static void LCD_DMA_Init(void)
{
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    NVIC_InitTypeDef nvic = {0};
    nvic.NVIC_IRQChannelPreemptionPriority = 4;
    nvic.NVIC_IRQChannelSubPriority        = 0;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    nvic.NVIC_IRQChannel                   = DMA1_Channel1_IRQn; /* MEM2MEM 用 Channel1 */
    
    NVIC_Init(&nvic);
}

/*  ========================================  DMA 内存到内存传输启动  ========================================  */
static void LCD_DMA_Start(const u16 *src, u32 count)
{
    if (count == 0) return;
    if (count > 65535) count = 65535;  /* DMA 单次最大 */

    DMA_InitTypeDef dma = {0};
    DMA_StructInit(&dma);

    dma.DMA_PeripheralBaseAddr  = (u32)LCD_DATA_ADDR;  /* 目标: FSMC 数据寄存器 */
    dma.DMA_MemoryBaseAddr      = (u32)src;            /* 源: RAM 缓冲 */
    dma.DMA_DIR                 = DMA_DIR_PeripheralDST;    /* 内存 -> 外设 */
    dma.DMA_BufferSize          = (u16)count;
    dma.DMA_PeripheralInc       = DMA_PeripheralInc_Disable;  /* FSMC 地址固定 */
    dma.DMA_MemoryInc           = DMA_MemoryInc_Enable;       /* RAM 地址递增 */
    dma.DMA_PeripheralDataSize  = DMA_PeripheralDataSize_HalfWord;
    dma.DMA_MemoryDataSize      = DMA_MemoryDataSize_HalfWord;
    dma.DMA_Mode                = DMA_Mode_Normal;
    dma.DMA_Priority            = DMA_Priority_High;
    dma.DMA_M2M                 = DMA_M2M_Enable;          /* 内存到内存模式 */

    DMA_Init(DMA1_Channel1, &dma);
    DMA_ITConfig(DMA1_Channel1, DMA_IT_TC, ENABLE);
    DMA_Cmd(DMA1_Channel1, ENABLE);

    s_dma_idle = 0;
}

/*  ========================================  DMA 异步填充（纯色大块）  ========================================  */
void LCD_FillRect_DMA(u16 x0, u16 y0, u16 x1, u16 y1, u16 color)
{
    while (!s_dma_idle);  /* 等待上一次 DMA 完成 */

    if (x0 > x1) { u16 t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { u16 t = y0; y0 = y1; y1 = t; }

    u16 w = x1 - x0 + 1;
    u16 h = y1 - y0 + 1;

    /* 填充 DMA 缓冲 */
    for (u16 i = 0; i < LCD_DMA_BUF_SIZE; i++) {
        s_dma_buffer[i] = color;
    }

    LCD_SetWindow(x0, y0, x1, y1);
    LCD_WriteCmd(0x2C);

    /* 分批次 DMA 传输 */
    u32 total_pixels = (u32)w * h;
    u32 sent = 0;
    
    while (sent < total_pixels) {
        while (!s_dma_idle);  /* 等待前一次完成 */
        
        u16 chunk = (total_pixels - sent > LCD_DMA_BUF_SIZE) 
                         ? LCD_DMA_BUF_SIZE : (total_pixels - sent);
        
        LCD_DMA_Start(s_dma_buffer, chunk);
        sent += chunk;
    }
}

/*  ========================================  LVGL 核心：刷新区域（从 RAM 缓冲到屏幕）  ========================================  */
void LCD_FlushArea(u16 x0, u16 y0, u16 x1, u16 y1, const u16 *color_p)
{
    while (!s_dma_idle);  /* 等待前一次 DMA 完成 */

    if (x0 > x1) { u16 t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { u16 t = y0; y0 = y1; y1 = t; }

    u16 w = x1 - x0 + 1;
    u16 h = y1 - y0 + 1;

    LCD_SetWindow(x0, y0, x1, y1);
    LCD_WriteCmd(0x2C);

    u32 total_pixels = (u32)w * h;
    u32 sent = 0;

    while (sent < total_pixels) {
        while (!s_dma_idle);
        
        u16 chunk = (total_pixels - sent > LCD_DMA_BUF_SIZE)
                         ? LCD_DMA_BUF_SIZE : (total_pixels - sent);
        
        /* 注意: color_p 是 LVGL 的缓冲，可能需要字节序转换 */
        LCD_DMA_Start(color_p + sent, chunk);
        sent += chunk;
    }
}

u8 LCD_DMA_IsIdle(void) { return s_dma_idle; }

/*  ========================================  DMA 中断处理  ========================================  */
void DMA1_Channel1_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_IT_TC1) !=  RESET) {
        DMA_ClearITPendingBit(DMA1_IT_TC1);
        DMA_Cmd(DMA1_Channel1, DISABLE);
        s_dma_idle = 1;
        LCD_DMA_CpltCallback();
    }
}

/* 弱定义回调 */
__attribute__((weak)) void LCD_DMA_CpltCallback(void) {}

#endif /* #if (TRANS_USE_DMA == 1) */

#elif (USE_LCD_CONTROLLER == 2) /* SSD1963 */
void SSD_Backlight_set(u8 pwm) {
	LCD_WRITE_CMD(0xBE);	//配置PWM输出
	LCD_WRITE_DATA(0x05);	//1设置PWM频率
	LCD_WRITE_DATA(pwm*2.55);//2设置PWM占空比
	LCD_WRITE_DATA(0x01);	//3设置C
	LCD_WRITE_DATA(0xFF);	//4设置D
	LCD_WRITE_DATA(0x00);	//5设置E
	LCD_WRITE_DATA(0x00);	//6设置F
}

#endif /* #if (USE_LCD_CONTROLLER == 0) */

/* 内部：初始化 LCD 控制引脚 */
static void LCD_Rel_Cfg(void)
{
#if (USE_LCD_CONTROLLER == 0)  /* ST7735S */
    io_set(&lcd_ctrlGPIO_DC);
	io_set(&lcd_ctrlGPIO_RES);
#endif /* #if (USE_LCD_CONTROLLER == 0) */

#if (LCD_BL_ACHWAY == 0) /* 背光实现-GPIO */
    io_set(&lcd_ctrlGPIO_BL);
#elif (LCD_BL_ACHWAY == 1) /* 背光实现-TIM PWM */
    LCD_Backlight_Init();
    LCD_Backlight_SetPercent(BL_DUTY);
#endif 
}

/* lcd 片上外设初始化 */
void lcd_onChipCfg() {
#if (USE_LCD_CONTROLLER == 0) /* ST7735S */
    spi_concernAll_cfg(&lcd_spi_param); 
#elif (USE_LCD_CONTROLLER == 1) /* ILI9341 */
    LCD_FSMC_Init();
#if (TRANS_USE_DMA == 1)
    LCD_DMA_Init();
#endif /* #if (TRANS_USE_DMA == 1) */
#endif /* #if (USE_LCD_CONTROLLER == 0) */
    LCD_Rel_Cfg();
}

/**
  * 函数功能: 对LCD显示器的某一窗口以某种颜色进行清屏
  * 输入参数: usX ：在特定扫描方向下窗口的起点X坐标
  *           usY ：在特定扫描方向下窗口的起点Y坐标
  *           usWidth ：窗口的宽度
  *           usHeight ：窗口的高度
  *           usColor ：颜色
  * 返 回 值: 无
  * 说    明：无
  */
void LCD_ClearWindow(u16 usX,u16 usY,u16 usWidth,u16 usHeight,u16 usColor)
{	
#if 1 
    u32 i,n,m;
    /* 在LCD显示器上开辟一个窗口 */
    LCD_SetWindow(usX,usY,usWidth,usHeight); 
    /* 开始向GRAM写入数据 */
    LCD_WRITE_CMD(lcddev.wramcmd);
    
    m = usWidth * usHeight;
    n = m/8;
    m = m - 8*n;
	for(i=0;i<n;i++) {
		LCD_WRITE_DATA(usColor);	
        LCD_WRITE_DATA(usColor);	
        LCD_WRITE_DATA(usColor);	
        LCD_WRITE_DATA(usColor);	
        
        LCD_WRITE_DATA(usColor);	
        LCD_WRITE_DATA(usColor);	
        LCD_WRITE_DATA(usColor);	
        LCD_WRITE_DATA(usColor);	
	}
    for(i=0;i<m;i++) {
        LCD_WRITE_DATA(usColor);	
    }
#else 
	u32 total_point = (u32)usWidth * usHeight;
    u32 index = 0;
    // 设置显示窗口区域
    LCD_SetWindow(usX, usY, usX + usWidth - 1, usY + usHeight - 1);
    // 准备写入GRAM
    LCD_WRITE_CMD(lcddev.wramcmd);
    // 循环填充颜色
    for (index = 0; index < total_point; index++) {
        LCD_WRITE_DATA(usColor);
    }
#endif
}

/* 清屏 */
void LCD_ClearAll(u16 color)
{
#if (USE_LCD_CONTROLLER == 0) /* ST7735S */
    LCD_FillRect(0, 0, ILI9341_HOR_RESOLUTION - 1, ILI9341_VER_RESOLUTION - 1, color);
#elif (USE_LCD_CONTROLLER == 1) /* ILI9341 */
#if 1
    u32 index = 0;      
	u32 totalpoint = lcddev.width;
	totalpoint *= lcddev.height; 			// 得到总点数
	if((lcddev.id == 0X6804)&&(lcddev.dir == 1)) {   // 6804横屏的时候特殊处理				    
 		lcddev.dir = 0;	 
 		lcddev.setxcmd = 0X2A;
		lcddev.setycmd = 0X2B;  	 			
		LCD_SetCursor(0,0);		// 设置光标位置  
 		lcddev.dir = 1;	 
  		lcddev.setxcmd = 0X2B;
		lcddev.setycmd = 0X2A;  	 
 	} else LCD_SetCursor(0, 0);	// 设置光标位置 
	LCD_WRITE_CMD(lcddev.wramcmd);     		// 开始写入GRAM	 	  
	for(index = 0;index<totalpoint;index++)
	{
		LCD_WRITE_DATA(color);	
	}
#else
    LCD_ClearWindow(0,0,lcddev.width,lcddev.height,color);
#endif 
#endif /* #if (USE_LCD_CONTROLLER == 0) */
}

/**
 * @brief 设置LCD显示窗口（起始坐标+宽高）
 * @param sx     起始X坐标
 * @param sy     起始Y坐标
 * @param width  窗口宽度（像素）
 * @param height 窗口高度（像素）
 */
void LCD_SetWindow(u16 sx,u16 sy,u16 width,u16 height)
{ 
#if (USE_LCD_CONTROLLER == 0)  /* ST7735S */
    if (width == 0 || height == 0) return;          // 无效尺寸，直接返回

    u16 ex = sx + width - 1;                   // 计算结束X坐标
    u16 ey = sy + height - 1;                  // 计算结束Y坐标

    // 边界裁剪
    if (sx > =  ILI9341_HOR_RESOLUTION || sy > =  ILI9341_VER_RESOLUTION) return;
    if (ex > =  ILI9341_HOR_RESOLUTION) ex = ILI9341_HOR_RESOLUTION - 1;
    if (ey > =  ILI9341_VER_RESOLUTION) ey = ILI9341_VER_RESOLUTION - 1;
    if (ex < sx || ey < sy) return;                 // 裁剪后无有效区域

    // 设置列地址范围 (CASET)
    LCD_WriteCmd(0x2A);
    LCD_WriteData8(sx>>8);
    LCD_WriteData8(sx & 0XFF);
    LCD_WriteData8(ex>>8);
    LCD_WriteData8(ex & 0XFF);

    // 设置行地址范围 (RASET)
    LCD_WriteCmd(0x2B);
    LCD_WriteData8(sy>>8);
    LCD_WriteData8(sy & 0XFF);
    LCD_WriteData8(ey>>8);
    LCD_WriteData8(ey & 0XFF);
#elif (USE_LCD_CONTROLLER == 1) /* ILI9341 */
	u8 hsareg,heareg,vsareg,veareg;
	u16 hsaval,heaval,vsaval,veaval; 
	u16 twidth,theight;
	twidth = sx+width-1;
	theight = sy+height-1;
	if(lcddev.id == 0X9341 || lcddev.id == 0X5310 || lcddev.id == 0X6804 || lcddev.id == 0X9488 || (lcddev.dir == 1 &&lcddev.id == 0X1963)) {
		LCD_WRITE_CMD(lcddev.setxcmd); 
		LCD_WRITE_DATA(sx>>8); LCD_WRITE_DATA(sx & 0XFF);	 
		LCD_WRITE_DATA(twidth>>8); LCD_WRITE_DATA(twidth& 0XFF);  
		LCD_WRITE_CMD(lcddev.setycmd); 
		LCD_WRITE_DATA(sy>>8); LCD_WRITE_DATA(sy & 0XFF); 
		LCD_WRITE_DATA(theight>>8); LCD_WRITE_DATA(theight& 0XFF); 
	} else if(lcddev.id == 0X1963) { // 1963竖屏特殊处理
		sx = lcddev.width-width-sx; 
		height = sy+height-1; 
		LCD_WRITE_CMD(lcddev.setxcmd); 
		LCD_WRITE_DATA(sx>>8); LCD_WRITE_DATA(sx & 0XFF);	 
		LCD_WRITE_DATA((sx+width-1)>>8); LCD_WRITE_DATA((sx+width-1)& 0XFF);  
		LCD_WRITE_CMD(lcddev.setycmd); 
        LCD_WRITE_DATA(sy>>8); LCD_WRITE_DATA(sy & 0XFF); 
		LCD_WRITE_DATA(height>>8); LCD_WRITE_DATA(height& 0XFF); 		
	} else if(lcddev.id == 0X5510) {
		LCD_WRITE_CMD(lcddev.setxcmd); LCD_WRITE_DATA(sx>>8);  
		LCD_WRITE_CMD(lcddev.setxcmd+1); LCD_WRITE_DATA(sx & 0XFF);	  
		LCD_WRITE_CMD(lcddev.setxcmd+2); LCD_WRITE_DATA(twidth>>8);   
		LCD_WRITE_CMD(lcddev.setxcmd+3); LCD_WRITE_DATA(twidth& 0XFF);   
		LCD_WRITE_CMD(lcddev.setycmd);  LCD_WRITE_DATA(sy>>8);   
		LCD_WRITE_CMD(lcddev.setycmd+1); LCD_WRITE_DATA(sy & 0XFF);  
		LCD_WRITE_CMD(lcddev.setycmd+2); LCD_WRITE_DATA(theight>>8);   
		LCD_WRITE_CMD(lcddev.setycmd+3); LCD_WRITE_DATA(theight& 0XFF);  
	} else {  //其他驱动IC
		if(lcddev.dir == 1) {  //横屏
			//窗口值
			hsaval = sy;				
			heaval = theight;
			vsaval = lcddev.width-twidth-1;
			veaval = lcddev.width-sx-1;				
		} else { 
			hsaval = sx;				
			heaval = twidth;
			vsaval = sy;
			veaval = theight;
		} 
		hsareg = 0X50;heareg = 0X51;//水平方向窗口寄存器
		vsareg = 0X52;veareg = 0X53;//垂直方向窗口寄存器	   							  
		//设置寄存器值
        LCD_WRITE_REG(hsareg, hsaval);
        LCD_WRITE_REG(heareg, heaval);
        LCD_WRITE_REG(vsareg, vsaval);
        LCD_WRITE_REG(veareg, veaval);
        		
		LCD_SetCursor(sx,sy);	//设置光标位置
	}
#endif /* #if (USE_LCD_CONTROLLER == 0) */
}

/**
 * @brief 设置LCD显示方向（旋转）
 * @param rot 旋转方向，取值：
 *            USE_HORIZONTAL     (0x00) 横屏，左上角为原点
 *            USE_VERTICAL       (0x01) 竖屏，左上角为原点（默认）
 *            USE_HORIZONTAL_INV (0x02) 横屏，镜像（左右翻转）
 *            USE_VERTICAL_INV   (0x03) 竖屏，镜像（上下翻转）
 */
void LCD_SetRotation(lcd_dir rot)
{
#if USE_LCD_CONTROLLER /* ILI9341 */
    u8 madctl = 0;   // 用于设置 MADCTL 寄存器

    // 根据旋转方向配置 MADCTL 的位
    switch (rot) {
        case USE_HORIZONTAL:      // 横屏（宽度>高度）
            // 设置 MY = 0, MX = 0, MV = 1, ML = 0, BGR = 1 (假设 RGB 格式)
            madctl = 0X60;        //0X60: 0110 0000  (MV = 1, BGR = 1) 根据实际调整
            break;
        case USE_VERTICAL:        // 竖屏（默认）
            madctl = 0X40;        //0X40: 0100 0000  (BGR = 1, 其他0)
            break;
        case USE_HORIZONTAL_INV:  // 横屏反向
            madctl = 0XA0;        //0XA0: 1010 0000  (MX = 1, MV = 1, BGR = 1)
            break;
        case USE_VERTICAL_INV:    // 竖屏反向
            madctl = 0X80;        //0X80: 1000 0000  (MY = 1, BGR = 1)
            break;
        default:
            return;
    }

    // 写入 MADCTL 寄存器（命令0X36）
    LCD_WRITE_REG(0x36, madctl);

    // 更新 lcddev 中的宽高信息（假设 lcddev 为全局结构体）
    switch (rot) {
        case USE_HORIZONTAL:
        case USE_HORIZONTAL_INV:
            // 横屏：宽 > 高，假设原始物理屏竖屏时宽度 = ILI9341_VER_RESOLUTION，高度 = ILI9341_HOR_RESOLUTION，则横屏时交换
            lcddev.width = ILI9341_VER_RESOLUTION;   
            lcddev.height = ILI9341_HOR_RESOLUTION;   
            break;
        case USE_VERTICAL:
        case USE_VERTICAL_INV:
            // 竖屏：恢复物理宽高
            lcddev.width = ILI9341_HOR_RESOLUTION;   
            lcddev.height = ILI9341_VER_RESOLUTION;  
            break;
    }
#else /* ST7735S */
    u8 madctl = 0X00;
    switch (rot) {
        case USE_VERTICAL: madctl = 0XC0; break;       /* 竖屏，默认 */
        case USE_HORIZONTAL: madctl = 0X60; break;     /* 横屏 90° */
        case USE_VERTICAL_INV: madctl = 0X00; break;   /* 竖屏 180° */
        case USE_HORIZONTAL_INV: madctl = 0XA0; break; /* 横屏 270° */
        default: madctl = 0XC0; break;
    }
    LCD_WriteCmd(0x36); /* MADCTL */
    LCD_WriteData8(madctl);
#endif /* #if USE_LCD_CONTROLLER */
}

#endif /* #if LCD_IS_USE */

