/*
 * 油烟机控制系统主程序
 * 基于STM32F103VET6 + FreeRTOS
 * 
 * 功能说明：
 * 1. 待机模式：风机停止，持续计算风速
 * 2. 手动模式：手动调速，PID控制
 * 3. 自动模式：根据传感器自动调节风速
 * 4. 防回流模式：气体浓度超阈值时启动风机
 * 5. 固件更新：通过usart+dma接收固件，接收完成后跳转到app区，boot+app双区架构
 * 6. UI显示功能
 * 7.编码器测速功能：通过1ms中断检测编码器获取的计数变化量来计算电机转速
 * 8.直流有刷电机驱动功能：通过PWM互补输出驱动H桥
 * 9.CRC32检验功能：通过python脚本预处理app.bin，添加CRC32检验码，用于固件更新
 * 按键功能：
 * - 按键1(PE4)：短按切换模式
 * - 按键2(PE3)：短按切换档位，长按开关风机
 *作者: Lyf
 *修改日期：2026/5/7
 *项目已申请版权，请勿倒卖！
 */
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

/*-----------------------------------------------------------
 * 硬件（片上外设）初始化
 *----------------------------------------------------------*/
static void Hardware_Init(void)
{
	/************************************************************************************************************/
	/*由于共计4个中断，设置中断分组为4，4位抢占优先级，0位亚优先级，因为Free不支持亚优先级处理。
	 由于Free中存在屏蔽优先级阈值的概念，该工程笔者设置为3，因此我们需要调用API函数的中断优先级不能超过3，
	 所以DMA中断优先级为4，定时器4中断优先级为5，TIM2中断优先级为6，从优先级都设置为0--不甘心的咸鱼注*/
	/*************************************************************************************************************/
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
	
	/* 延时函数初始化 */
    delay_init();
	
	/* USART串口配置 */
#if UART_LOG_OUT
    /* 日志信息打印串口初始化 */
    debug_uart_cfg();
#endif /* #if UART_LOG_OUT */

#if HARDWARE_UPDATE_OPEN /* 引入固件升级 */
#if !HW_UPDATE_METHOD  /* 有线IAP */
	iap_uart_cfg();
#else /* 无线OTA */

#endif 

#endif 

#if PRINT_USE /* 引入print打印 */
    print_usart_onChipCfg();
#endif
	/* USART串口配置 */

#if MOTOR_IS_USE
	/* 电机PWM初始化
     * TIM1: PWM频率 = 72MHz / 72 / 1000 = 1kHz
     * 死区时间: 100 * 16/72MHz ≈ 22us
     */
    TIM_motorDrive_Init(1000-1, 72-1, 0, 100);
    motor_init();

    /* 编码器初始化（TIM2）不分频，72MHZ的计数频率,ARR为最大值*/
    TIM_motorEncoder_Init(0xFFFF, 0);
#endif /* #if MOTRO_IS_USE */

#if LED_IS_USE
	/* LED片上外设初始化 */
    LED_onChipCfg(&Led0);
    LED_onChipCfg(&Led1);
#endif /* #if LED_IS_USE */

#if KEY_IS_USE
	/* 按键注册 */
	key_adp_register();
    /* 按键初始化 */
    key_adp_init(); 
#endif /* #if KEY_IS_USE */

#if LCD_IS_USE 
    /* LCD 片上外设初始化 */
    lcd_onChipCfg();
#endif /* #if LCD_IS_USE */

#if BUZZER_IS_USE
    /* 蜂鸣器片上外设初始化 */
    if(buzzer.onChipCfg(&buzzer) != BUZZER_OK) {
		//LOG_D("The buzzer configed failed above the chip");
	}
#endif /* #if BUZZER_IS_USE */

#if USE_SENSOR_MQ2
    delay_ms(200);
    /* MQ-2 片上外设初始化 */
    mq2_onChip_init();
#endif /* #if USE_SENSOR_MQ2 */

#if USE_SENSOR_AHT20
    delay_ms(200);
    /* AHT20 片上外设初始化 */
    aht20_onChip_init();
#endif /* #if USE_SENSOR_AHT20 */

}
/* ====================================传感器接口注册====================================== */
static void Software_Sensor_Init(void)
{
/* ======================================AHT20===================================== */
#if USE_SENSOR_AHT20
    /* 注册AHT20传感器 */
    aht20_adapter_register();
    /* 注册传感器dev信息后通过传感器类型获取列表内对应的传感器dev指针 */
    sensor_device_t *th_sensor = sensor_find_by_type(SENSOR_TYPE_TEMP_HUMIDITY);
    /* 通过对外接口初始化传感器 */
    sensor_init(th_sensor);
#else 
    #error  "no temp&humi sensor selected"
#endif
/* ======================================AHT20===================================== */

/* ======================================MQ2======================================= */
#if USE_SENSOR_MQ2
    /* 注册MQ2传感器 */
    mq2_adapter_register();
    /* 注册传感器dev信息后通过传感器类型获取列表内对应的传感器dev指针 */
    sensor_device_t *gas_sensor = sensor_find_by_type(SENSOR_TYPE_GAS);
    /* 通过对外接口初始化传感器 */
    sensor_init(gas_sensor);
#else 
    #error  "no gas sensor selected"
#endif
/* ======================================MQ2======================================= */

}
/* ====================================传感器接口注册====================================== */

/* ====================================LCD软件初始化====================================== */
static void Software_LCD_Init(void)
{
#if (USE_LCD_CONTROLLER == 0) /* ST7735S */
    LCD_ST7735S_Init();
#elif (USE_LCD_CONTROLLER == 1) /* ILI9341 */
    LCD_ILI9341_Init();
	tp_dev.init();
#endif 
}
/* ====================================LCD软件初始化====================================== */

/* 应用配置，包括模块接口注册 */
static void Software_Init(void)
{
#if SENSOR_IS_USE
    Software_Sensor_Init();     /* 传感器接口注册 */
#endif /* #if USE_SENSOR */

#if LCD_IS_USE /* 如果使用LCD */
    Software_LCD_Init();        /* LCD 初始化 */
#endif /* #if LCD_IS_USE */

#if PID_IS_USE 
	/* PID控制器初始化
	 * 参数说明：
	 * Kp=14.0: 比例系数
	 * Ki=1.65: 积分系数
	 * Kd=0.00: 微分系数
	 * 输出限幅：0~1000
	 */
	//PID_Init(&s_speedPID, 14.0f, 1.65f, 0.0f, 0.5f, 0.0f, 1000.0f, PID_MODE_POSITIONAL, PID_DIRECT);
	PID_Init(&s_speedPID, 8.0f, 1.65f, 0.0f, 1000.0f, 0.0f);
#endif /* #if PID_IS_USE */

#if WINDSPEED_IS_USE /* 如果引入风速算法 */
    /* 风速算法模块初始化 */
    WindSpeed_Init();
#endif /* #if WINDSPEED_IS_USE */

}

#if 0
/* 清空屏幕并在正中间显示"Lyf" */
static void Load_Drow_Dialog(void)
{
	LCD_ClearAll(WHITE);	//清屏   
	GUI_ShowString(lcddev.width/2-20,lcddev.height/2-80,"Lyf",GREEN,WHITE,ASCII_2412,0,36,24); //显示清屏区域
	GUI_DrawHeart(lcddev.width/2-15, lcddev.height/2+20, 2, RED, 1);  // 填充爱心（放大2倍，粉色）ww
}

/* 电容触摸屏专有部分 画水平线 x0,y0:坐标 len:线长度 color:颜色 */
static void gui_draw_hline(u16 x0,u16 y0,u16 len,u16 color)
{
	if(len==0)return;
	GUI_Fill(x0,y0,x0+len-1,y0,color);	
}

/* 画实心圆 x0,y0:坐标 r:半径 color:颜色 */
static void gui_fill_circle(u16 x0,u16 y0,u16 r,u16 color)
{											  
	u32 i;
	u32 imax = ((u32)r*707)/1000+1;
	u32 sqmax = (u32)r*(u32)r+(u32)r/2;
	u32 x=r;
	gui_draw_hline(x0-r,y0,2*r,color);
	for (i=1;i<=imax;i++) 
	{
		if ((i*i+x*x)>sqmax)// draw lines from outside  
		{
 			if (x>imax) 
			{
				gui_draw_hline (x0-i+1,y0+x,2*(i-1),color);
				gui_draw_hline (x0-i+1,y0-x,2*(i-1),color);
			}
			x--;
		}
		// draw lines from inside (center)  
		gui_draw_hline(x0-x,y0+i,2*x,color);
		gui_draw_hline(x0-x,y0-i,2*x,color);
	}
}  

/* 画一条粗线 (x1,y1),(x2,y2):线条的起始坐标 size：线条的粗细程度 color：线条的颜色 */
static void lcd_draw_bline(u16 x1, u16 y1, u16 x2, u16 y2,u8 size,u16 color)
{
	u16 t; 
	int xerr=0,yerr=0,delta_x,delta_y,distance; 
	int incx,incy,uRow,uCol; 
	if(x1<size|| x2<size||y1<size|| y2<size)return; 
	delta_x=x2-x1; //计算坐标增量 
	delta_y=y2-y1; 
	uRow=x1; 
	uCol=y1; 
	if(delta_x>0)incx=1; //设置单步方向 
	else if(delta_x==0)incx=0;//垂直线 
	else {incx=-1;delta_x=-delta_x;} 
	if(delta_y>0)incy=1; 
	else if(delta_y==0)incy=0;//水平线 
	else{incy=-1;delta_y=-delta_y;} 
	if( delta_x>delta_y)distance=delta_x; //选取基本增量坐标轴 
	else distance=delta_y; 
	for(t=0;t<=distance+1;t++ )//画线输出 
	{  
		gui_fill_circle(uRow,uCol,size,color);//画点 
		xerr+=delta_x ; 
		yerr+=delta_y ; 
		if(xerr>distance) 
		{ 
			xerr-=distance; 
			uRow+=incx; 
		} 
		if(yerr>distance) 
		{ 
			yerr-=distance; 
			uCol+=incy; 
		} 
	}  
}   

/* 5个触控点的颜色 */										 
const u16 POINT_COLOR_TBL[CT_MAX_TOUCH]={RED,GREEN,BLUE,BROWN,GRED};  

// 电阻触摸屏测试函数
void rtp_test(void)
{
	u8 key;
	u8 i=0;	  
	while(1)
	{
	 	key=KEY_Scan(0);
		tp_dev.scan(0); 		 
		if(tp_dev.sta&TP_PRES_DOWN)			//触摸屏被按下
		{	
		 	if(tp_dev.x[0]<lcddev.width&&tp_dev.y[0]<lcddev.height)
			{	
				if(tp_dev.x[0]>(lcddev.width-24)&&tp_dev.y[0]<16)Load_Drow_Dialog();//清除
				else TP_Draw_Big_Point(tp_dev.x[0],tp_dev.y[0],RED);		//画图	  			   
			}
		}else delay_ms(10);	//没有按键按下的时候 	    
		if(key==KEY0_PRES)	//KEY0按下,则执行校准程序
		{
			LCD_ClearAll(WHITE); //清屏
		    tp_adjust();  	//屏幕校准  
			Load_Drow_Dialog();
		}
		i++;
		if(i%20==0) LED_toggle(&Led0);
	}
}

//电容触摸屏测试函数
void ctp_test(void)
{
	u8 t=0;
	u8 i=0;	  	    
 	u16 lastpos[5][2];		//记录最后一次的数据 
	while(1)
	{
		tp_dev.scan(0);
		for(t=0;t<CT_MAX_TOUCH;t++)
		{
			if((tp_dev.sta)&(1<<t))
			{
				if(tp_dev.x[t]<lcddev.width&&tp_dev.y[t]<lcddev.height)
				{
					if(lastpos[t][0]==0XFFFF)
					{
						lastpos[t][0] = tp_dev.x[t];
						lastpos[t][1] = tp_dev.y[t];
					}
					lcd_draw_bline(lastpos[t][0],lastpos[t][1],tp_dev.x[t],tp_dev.y[t],2,POINT_COLOR_TBL[t]);//画线
					lastpos[t][0]=tp_dev.x[t];
					lastpos[t][1]=tp_dev.y[t];
					if(tp_dev.x[t]>(lcddev.width-24)&&tp_dev.y[t]<16)
					{
						Load_Drow_Dialog();//清除
					}
				}
			}else lastpos[t][0]=0XFFFF;
		}
		
		delay_ms(5);i++;
		if(i%20==0) LED_toggle(&Led0);
	}	
}

/* LCD测试 */
static void App_LCD_Test(void) 
{ 
	GUI_ShowString(60,50,"ELITE STM32",RED,WHITE,ASCII_1608,0,200,16);	
	GUI_ShowString(60,70,"TOUCH TEST",RED,WHITE,ASCII_1608,0,200,16);	
	GUI_ShowString(60,90,"TBW",RED,WHITE,ASCII_1608,0,200,16);	
	GUI_ShowString(60,110,"2026/6/15",RED,WHITE,ASCII_1608,0,200,16);	
	GUI_ShowString(60,130,"Press KEY0 to Adjust",RED,WHITE,ASCII_1608,0,200,16);	
	
   	if(tp_dev.touchtype!=0XFF) GUI_ShowString(60,130,"Press KEY0 to Adjust",RED,WHITE,ASCII_1608,0,200,16); // 电阻屏才显示
	delay_ms(1500);
	Load_Drow_Dialog();	 	
	if(tp_dev.touchtype & 0X80) ctp_test();	// 电容屏测试
	else return;//rtp_test(); 						// 电阻屏测试
}
#endif

/* 应用测试 */
static void App_Test(void) 
{
/* LCD测试 */
#if LCD_IS_USE 
    // App_LCD_Test();
#endif /* #if LCD_IS_USE */

}

/*-----------------------------------------------------------
 * 主函数
 *----------------------------------------------------------*/
int main(void)
{
    /* 硬件初始化 */
    Hardware_Init();

    /* 应用配置，包括模块接口注册 */
    Software_Init();
	
	/* 系统状态初始化 */
	System_Init();
	
    /* 应用测试 */
    // App_Test();

    /* 创建开始任务 */
    StartTask_Create();
    
    /* 启动调度器 */ 
    vTaskStartScheduler();

    /* 正常情况下不会执行到这里 */
    while(1)
    {
        
    }
}



