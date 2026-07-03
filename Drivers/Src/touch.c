#include "touch.h" 

#ifdef USE_LCD_CONTROLLER  /* 系统使用LCD */
#if (USE_LCD_CONTROLLER == 1)  /* 使用ILI9341 */
#if TP_USE 

#include "lcd.h"
#include "GUI.h"
#include "gpio.h"
#include "i2c.h"
#include "delay.h"
#include "stdlib.h"
#include "math.h"
#include "at24cxx.h" 

// 本程序只供学习使用，未经作者许可，不得用于其它任何用途
// STM32开发板
// 触摸屏驱动(支持ADS7843/7846/UH7843/7846/XPT2046/TSC2046/OTT2001A/GT9147/FT5206等)代码	   
// 创建日期:2026/6/16
	
static const i2c_2LineIO Line2io = { .scl_port = SCL_PORT, .scl_pin = SCL_PIN, .sda_port = SDA_PORT, .sda_pin = SDA_PIN };

TP_Dev tp_dev=
{
	tp_init,
	tp_scan,
	tp_adjust,
	0,
	0, 
	0,
	0,
	0,
	0,	  	 		
	0,
	0,	  	 		
};	

// 默认为touchtype=0的数据.
u8 CMD_RDX=0XD0;
u8 CMD_RDY=0X90;
 	 			    					   
// SPI写数据
// 向触摸屏IC写入1byte数据    
// num:要写入的数据
void TP_Write_Byte(u8 num)    
{  
	u8 count=0;   
	for(count=0;count<8;count++)  
	{ 	  
		if(num&0x80)TDIN=1;  
		else TDIN=0;   
		num<<=1;    
		TCLK=0; 
		delay_us(1);
		TCLK=1;		// 上升沿有效	        
	}		 			    
} 		 
// SPI读数据 
// 从触摸屏IC读取adc值
// CMD:指令
// 返回值:读到的数据	   
u16 TP_Read_AD(u8 CMD)	  
{ 	 
	u8 count=0; 	  
	u16 Num=0; 
	TCLK=0;		// 先拉低时钟 	 
	TDIN=0; 	// 拉低数据线
	TCS=0; 		// 选中触摸屏IC
	TP_Write_Byte(CMD);// 发送命令字
	delay_us(6);// ADS7846的转换时间最长为6us
	TCLK=0; 	     	    
	delay_us(1);    	   
	TCLK=1;		// 给1个时钟，清除BUSY
	delay_us(1);    
	TCLK=0; 	     	    
	for(count=0;count<16;count++)// 读出16位数据,只有高12位有效 
	{ 				  
		Num<<=1; 	 
		TCLK=0;	// 下降沿有效  	    	   
		delay_us(1);    
 		TCLK=1;
 		if(DOUT)Num++; 		 
	}  	
	Num>>=4;   	// 只有高12位有效.
	TCS=1;		// 释放片选	 
	return(Num);   
}
// 读取一个坐标值(x或者y)
// 连续读取READ_TIMES次数据,对这些数据升序排列,
// 然后去掉最低和最高LOST_VAL个数,取平均值 
// xy:指令（CMD_RDX/CMD_RDY）
// 返回值:读到的数据
#define READ_TIMES 5 	// 读取次数
#define LOST_VAL 1	  	// 丢弃值
u16 TP_Read_XOY(u8 xy)
{
	u16 i, j;
	u16 buf[READ_TIMES];
	u16 sum=0;
	u16 temp;
	for(i=0;i<READ_TIMES;i++)buf[i]=TP_Read_AD(xy);		 		    
	for(i=0;i<READ_TIMES-1; i++)// 排序
	{
		for(j=i+1;j<READ_TIMES;j++)
		{
			if(buf[i]>buf[j])// 升序排列
			{
				temp=buf[i];
				buf[i]=buf[j];
				buf[j]=temp;
			}
		}
	}	  
	sum=0;
	for(i=LOST_VAL;i<READ_TIMES-LOST_VAL;i++)sum+=buf[i];
	temp=sum/(READ_TIMES-2*LOST_VAL);
	return temp;   
} 
// 读取x,y坐标
// 最小值不能少于100.
// x,y:读取到的坐标值
// 返回值:0,失败;1,成功。
u8 tp_readXY(u16 *x,u16 *y)
{
	u16 xtemp,ytemp;			 	 		  
	xtemp=TP_Read_XOY(CMD_RDX);
	ytemp=TP_Read_XOY(CMD_RDY);	  												   
	// if(xtemp<100||ytemp<100)return 0;// 读数失败
	*x=xtemp;
	*y=ytemp;
	return 1;// 读数成功
}
// 连续2次读取触摸屏IC,且这两次的偏差不能超过
// ERR_RANGE,满足条件,则认为读数正确,否则读数错误.	   
// 该函数能大大提高准确度
// x,y:读取到的坐标值
// 返回值:0,失败;1,成功。
#define ERR_RANGE 50 // 误差范围 
u8 tp_readXY2(u16 *x,u16 *y) 
{
	u16 x1,y1;
 	u16 x2,y2;
 	u8 flag;    
    flag=tp_readXY(&x1,&y1);   
    if(flag==0)return(0);
    flag=tp_readXY(&x2,&y2);	   
    if(flag==0)return(0);   
    if(((x2<=x1&&x1<x2+ERR_RANGE)||(x1<=x2&&x2<x1+ERR_RANGE))// 前后两次采样在+-50内
    &&((y2<=y1&&y1<y2+ERR_RANGE)||(y1<=y2&&y2<y1+ERR_RANGE)))
    {
        *x=(x1+x2)/2;
        *y=(y1+y2)/2;
        return 1;
    }else return 0;	  
}  
// 与LCD部分有关的函数  
// 画一个触摸点
// 用来校准用的
// x,y:坐标
// color:颜色
void tp_drowTouchPoint(u16 x,u16 y,u16 color)
{
	GUI_DrawLine(x-12,y,x+13,y,color);// 横线
	GUI_DrawLine(x,y-12,x,y+13,color);// 竖线
	GUI_WritePixel(x+1,y+1,color);
	GUI_WritePixel(x-1,y+1,color);
	GUI_WritePixel(x+1,y-1,color);
	GUI_WritePixel(x-1,y-1,color);
	GUI_DrawCircle(x,y,6,color,0); // 画中心圈
}	  
// 画一个大点(2*2的点)		   
// x,y:坐标
// color:颜色
void TP_Draw_Big_Point(u16 x,u16 y,u16 color)
{	    
	GUI_WritePixel(x,y,color);// 中心点 
	GUI_WritePixel(x+1,y,color);
	GUI_WritePixel(x,y+1,color);
	GUI_WritePixel(x+1,y+1,color);	 	  	
}						  

// 触摸按键扫描
// tp:0,屏幕坐标;1,物理坐标(校准等特殊场合用)
// 返回值:当前触屏状态.
// 0,触屏无触摸;1,触屏有触摸
u8 tp_scan(u8 tp)
{			   
	if(PEN==0)// 有按键按下
	{
		if(tp)tp_readXY2(&tp_dev.x[0],&tp_dev.y[0]);// 读取物理坐标
		else if(tp_readXY2(&tp_dev.x[0],&tp_dev.y[0]))// 读取屏幕坐标
		{
	 		tp_dev.x[0]=tp_dev.xfac*tp_dev.x[0]+tp_dev.xoff;// 将结果转换为屏幕坐标
			tp_dev.y[0]=tp_dev.yfac*tp_dev.y[0]+tp_dev.yoff;  
	 	} 
		if((tp_dev.sta&TP_PRES_DOWN)==0)// 之前没有被按下
		{		 
			tp_dev.sta=TP_PRES_DOWN|TP_CATH_PRES;// 按键按下  
			tp_dev.x[4]=tp_dev.x[0];// 记录第一次按下时的坐标
			tp_dev.y[4]=tp_dev.y[0];  	   			 
		}			   
	}else
	{
		if(tp_dev.sta&TP_PRES_DOWN)// 之前是被按下的
		{
			tp_dev.sta&=~(1<<7);// 标记按键松开	
		}else// 之前就没有被按下
		{
			tp_dev.x[4]=0;
			tp_dev.y[4]=0;
			tp_dev.x[0]=0xffff;
			tp_dev.y[0]=0xffff;
		}	    
	}
	return tp_dev.sta&TP_PRES_DOWN;// 返回当前的触屏状态
}	  
// 保存在EEPROM里面的地址区间基址,占用14个字节(RANGE:SAVE_ADDR_BASE~SAVE_ADDR_BASE+13)
#define SAVE_ADDR_BASE 40
// 保存校准参数										    
static void tp_saveAdjustData(void)
{
	at24cxx_write(&Line2io, SAVE_ADDR_BASE, (u8*)&tp_dev.xfac, 14); // 强制保存&tp_dev.xfac地址开始的14个字节数据，即保存到tp_dev.touchtype
 	at24cxx_writeByte(&Line2io, SAVE_ADDR_BASE+14, 0X0A);		// 在最后，写0X0A标记校准过了
}
// 得到保存在EEPROM里面的校准值
// 返回值：1，成功获取数据
//         0，获取失败，要重新校准
static u8 tp_getAdjustData(void)
{					  
	u8 temp;
	at24cxx_readByte(&Line2io, SAVE_ADDR_BASE+14, &temp);  // 读取标记字,看是否校准过！ 		 
	if(temp==0X0A)  // 触摸屏已经校准过了			   
 	{ 
		at24cxx_read(&Line2io, SAVE_ADDR_BASE, (u8*)&tp_dev.xfac, 14); // 读取之前保存的校准数据 
		if(tp_dev.touchtype)   // X,Y方向与屏幕相反
		{
			CMD_RDX=0X90;
			CMD_RDY=0XD0;	 
		}else				   // X,Y方向与屏幕相同
		{
			CMD_RDX=0XD0;
			CMD_RDY=0X90;	 
		}		 
		return 1;	 
	}
	return 0;
}	 
// 提示字符串
char* const TP_REMIND_MSG_TBL="Please use the stylus click the cross on the screen.The cross will always move until the screen adjustment is completed.";
 					  
// 提示校准结果(各个参数)
void tp_adjustInfo_show(u16 x0,u16 y0,u16 x1,u16 y1,u16 x2,u16 y2,u16 x3,u16 y3,u16 fac)
{	  
	GUI_ShowString(40,160,"x1:",RED,WHITE,ASCII_1608,0,lcddev.width,lcddev.height);
 	GUI_ShowString(40+80,160,"y1:",RED,WHITE,ASCII_1608,0,lcddev.width,lcddev.height);
 	GUI_ShowString(40,180,"x2:",RED,WHITE,ASCII_1608,0,lcddev.width,lcddev.height);
 	GUI_ShowString(40+80,180,"y2:",RED,WHITE,ASCII_1608,0,lcddev.width,lcddev.height);
	GUI_ShowString(40,200,"x3:",RED,WHITE,ASCII_1608,0,lcddev.width,lcddev.height);
 	GUI_ShowString(40+80,200,"y3:",RED,WHITE,ASCII_1608,0,lcddev.width,lcddev.height);
	GUI_ShowString(40,220,"x4:",RED,WHITE,ASCII_1608,0,lcddev.width,lcddev.height);
 	GUI_ShowString(40+80,220,"y4:",RED,WHITE,ASCII_1608,0,lcddev.width,lcddev.height);  
 	GUI_ShowString(40,240,"fac is:",RED,WHITE,ASCII_1608,0,lcddev.width,lcddev.height);     
	GUI_ShowNum(40+24,160,x0,4,ASCII_1608,GREEN,WHITE,0);		// 显示数值
	GUI_ShowNum(40+24+80,160,y0,4,ASCII_1608,GREEN,WHITE,0);	// 显示数值
	GUI_ShowNum(40+24,180,x1,4,ASCII_1608,GREEN,WHITE,0);		// 显示数值
	GUI_ShowNum(40+24+80,180,y1,4,ASCII_1608,GREEN,WHITE,0);	// 显示数值
	GUI_ShowNum(40+24,200,x2,4,ASCII_1608,GREEN,WHITE,0);		// 显示数值
	GUI_ShowNum(40+24+80,200,y2,4,ASCII_1608,GREEN,WHITE,0);	// 显示数值
	GUI_ShowNum(40+24,220,x3,4,ASCII_1608,GREEN,WHITE,0);		// 显示数值
	GUI_ShowNum(40+24+80,220,y3,4,ASCII_1608,GREEN,WHITE,0);	// 显示数值
 	GUI_ShowNum(40+56,240,fac,3,ASCII_1608,GREEN,WHITE,0); 	    // 显示数值,该数值必须在95~105范围之内.
}
		 
// 触摸屏校准代码
// 得到四个校准参数
void tp_adjust(void)
{								 
	u16 pos_temp[4][2];// 坐标缓存值
	u8  cnt=0;	
	u16 d1,d2;
	u32 tem1,tem2;
	double fac; 	
	u16 outtime=0;
 	cnt=0;				
	LCD_ClearAll(WHITE);// 清屏   
	LCD_ClearAll(WHITE);// 清屏 
	GUI_ShowString(40,40,TP_REMIND_MSG_TBL,BLACK,WHITE,ASCII_1608,0,160,100); // 显示提示信息
	tp_drowTouchPoint(20,20,RED); // 画点1 
	tp_dev.sta=0; // 消除触发信号 
	tp_dev.xfac=0; // xfac用来标记是否校准过,所以校准之前必须清掉!以免错误	 
	while(1) // 如果连续10秒钟没有按下,则自动退出
	{
		tp_dev.scan(1);// 扫描物理坐标
		if((tp_dev.sta&0xc0)==TP_CATH_PRES)// 按键按下了一次(此时按键松开了.)
		{	
			outtime=0;		
			tp_dev.sta&=~(1<<6);// 标记按键已经被处理过了.
						   			   
			pos_temp[cnt][0]=tp_dev.x[0];
			pos_temp[cnt][1]=tp_dev.y[0];
			cnt++;	  
			switch(cnt)
			{			   
				case 1:						 
					tp_drowTouchPoint(20,20,WHITE);				// 清除点1 
					tp_drowTouchPoint(lcddev.width-20,20,RED);	// 画点2
					break;
				case 2:
 					tp_drowTouchPoint(lcddev.width-20,20,WHITE);	// 清除点2
					tp_drowTouchPoint(20,lcddev.height-20,RED);	// 画点3
					break;
				case 3:
 					tp_drowTouchPoint(20,lcddev.height-20,WHITE);			// 清除点3
 					tp_drowTouchPoint(lcddev.width-20,lcddev.height-20,RED);	// 画点4
					break;
				case 4:	 // 全部四个点已经得到
	    		    // 对边相等
					tem1=abs(pos_temp[0][0]-pos_temp[1][0]);// x1-x2
					tem2=abs(pos_temp[0][1]-pos_temp[1][1]);// y1-y2
					tem1*=tem1;
					tem2*=tem2;
					d1=sqrt(tem1+tem2);// 得到1,2的距离
					
					tem1=abs(pos_temp[2][0]-pos_temp[3][0]);// x3-x4
					tem2=abs(pos_temp[2][1]-pos_temp[3][1]);// y3-y4
					tem1*=tem1;
					tem2*=tem2;
					d2=sqrt(tem1+tem2);// 得到3,4的距离
					fac=(float)d1/d2;
					if(fac<0.95||fac>1.05||d1==0||d2==0)// 不合格
					{
						cnt=0;
 				    	tp_drowTouchPoint(lcddev.width-20,lcddev.height-20,WHITE);	// 清除点4
   	 					tp_drowTouchPoint(20,20,RED);								// 画点1
 						tp_adjustInfo_show(pos_temp[0][0],pos_temp[0][1],pos_temp[1][0],pos_temp[1][1],pos_temp[2][0],pos_temp[2][1],pos_temp[3][0],pos_temp[3][1],fac*100);// 显示数据   
 						continue;
					}
					tem1=abs(pos_temp[0][0]-pos_temp[2][0]);// x1-x3
					tem2=abs(pos_temp[0][1]-pos_temp[2][1]);// y1-y3
					tem1*=tem1;
					tem2*=tem2;
					d1=sqrt(tem1+tem2);// 得到1,3的距离
					
					tem1=abs(pos_temp[1][0]-pos_temp[3][0]);// x2-x4
					tem2=abs(pos_temp[1][1]-pos_temp[3][1]);// y2-y4
					tem1*=tem1;
					tem2*=tem2;
					d2=sqrt(tem1+tem2);// 得到2,4的距离
					fac=(float)d1/d2;
					if(fac<0.95||fac>1.05)// 不合格
					{
						cnt=0;
 				    	tp_drowTouchPoint(lcddev.width-20,lcddev.height-20,WHITE);	// 清除点4
   	 					tp_drowTouchPoint(20,20,RED);								// 画点1
 						tp_adjustInfo_show(pos_temp[0][0],pos_temp[0][1],pos_temp[1][0],pos_temp[1][1],pos_temp[2][0],pos_temp[2][1],pos_temp[3][0],pos_temp[3][1],fac*100);// 显示数据   
						continue;
					}// 正确了
								   
					// 对角线相等
					tem1=abs(pos_temp[1][0]-pos_temp[2][0]);// x1-x3
					tem2=abs(pos_temp[1][1]-pos_temp[2][1]);// y1-y3
					tem1*=tem1;
					tem2*=tem2;
					d1=sqrt(tem1+tem2);// 得到1,4的距离
	
					tem1=abs(pos_temp[0][0]-pos_temp[3][0]);// x2-x4
					tem2=abs(pos_temp[0][1]-pos_temp[3][1]);// y2-y4
					tem1*=tem1;
					tem2*=tem2;
					d2=sqrt(tem1+tem2);// 得到2,3的距离
					fac=(float)d1/d2;
					if(fac<0.95||fac>1.05)// 不合格
					{
						cnt=0;
 				    	tp_drowTouchPoint(lcddev.width-20,lcddev.height-20,WHITE);	// 清除点4
   	 					tp_drowTouchPoint(20,20,RED);								// 画点1
 						tp_adjustInfo_show(pos_temp[0][0],pos_temp[0][1],pos_temp[1][0],pos_temp[1][1],pos_temp[2][0],pos_temp[2][1],pos_temp[3][0],pos_temp[3][1],fac*100);// 显示数据   
						continue;
					} // 正确了
					// 计算结果
					tp_dev.xfac=(float)(lcddev.width-40)/(pos_temp[1][0]-pos_temp[0][0]);// 得到xfac		 
					tp_dev.xoff=(lcddev.width-tp_dev.xfac*(pos_temp[1][0]+pos_temp[0][0]))/2;// 得到xoff
						  
					tp_dev.yfac=(float)(lcddev.height-40)/(pos_temp[2][1]-pos_temp[0][1]);// 得到yfac
					tp_dev.yoff=(lcddev.height-tp_dev.yfac*(pos_temp[2][1]+pos_temp[0][1]))/2;// 得到yoff  
					if(abs(tp_dev.xfac)>2||abs(tp_dev.yfac)>2)// 触屏和预设的相反了.
					{
						cnt=0;
 				    	tp_drowTouchPoint(lcddev.width-20,lcddev.height-20,WHITE);	// 清除点4
   	 					tp_drowTouchPoint(20,20,RED);								// 画点1
						GUI_ShowString(40,26,"TP Need readjust!",BLACK,WHITE,ASCII_1608,0,lcddev.width,lcddev.height);
						tp_dev.touchtype=!tp_dev.touchtype; // 修改触屏类型
						if(tp_dev.touchtype) // X,Y方向与屏幕相反
						{
							CMD_RDX=0X90;
							CMD_RDY=0XD0;	 
						} else				   // X,Y方向与屏幕相同
						{
							CMD_RDX=0XD0;
							CMD_RDY=0X90;	 
						}			    
						continue;
					}		
					LCD_ClearAll(WHITE);// 清屏
					GUI_ShowString(35,110,"Touch Screen Adjust OK!",RED,WHITE,ASCII_1608,0,lcddev.width,lcddev.height);// 校正完成
					delay_ms(1000);
					tp_saveAdjustData();  
 					LCD_ClearAll(WHITE);// 清屏   
					return;// 校正完成				 
			}
		}
		delay_ms(10);
		outtime++;
		if(outtime>1000)
		{
			tp_getAdjustData();
			break;
	 	} 
 	}
}	 
// 触摸屏初始化  		    
// 返回值:0,没有进行校准
//        1,进行过校准
u8 tp_init(void)
{	
	if(lcddev.id==0X5510)				// 4.3寸电容触摸屏
	{
		if(gt9147_init()==0) {			// 是GT9147 
			tp_dev.scan=gt9147_scan;	// 扫描函数指向GT9147触摸屏扫描
		} else {
			ott2001a_init();
			tp_dev.scan=ott2001a_scan;	// 扫描函数指向OTT2001A触摸屏扫描
		}
		tp_dev.touchtype |= 0X80;			// 电容屏 
		tp_dev.touchtype |= lcddev.dir&0X01;// 横屏还是竖屏 
		return 0;
	} else if(lcddev.id==0X1963)			// 7寸电容触摸屏
	{
		ft5206_init();
		tp_dev.scan=ft5206_scan;		// 扫描函数指向GT9147触摸屏扫描		
		tp_dev.touchtype|=0X80;			// 电容屏 
		tp_dev.touchtype|=lcddev.dir&0X01;// 横屏还是竖屏 
		return 0;
	} else {
		periph_gpio_t GenP = {
			.port = GPIOB,
			.pin  = GPIO_Pin_1,
			.initial_level = highLevel,
			.mode = GPIO_Mode_Out_PP,
			.speed = GPIO_Speed_50MHz,
		};
		io_set(&GenP);
		
		GenP.pin = GPIO_Pin_2;
		GenP.mode = GPIO_Mode_IPU;
		io_set(&GenP);		
		
		GenP.port = GPIOF;
		GenP.pin = GPIO_Pin_11|GPIO_Pin_9;
		GenP.mode = GPIO_Mode_Out_PP;
		io_set(&GenP);	
		
		GenP.pin = GPIO_Pin_10;
		GenP.mode = GPIO_Mode_IPU;
		io_set(&GenP);
		 
		tp_readXY(&tp_dev.x[0],&tp_dev.y[0]);// 第一次读取初始化	 
		at24cxx_onChipCfg();			 // 初始化AT24CXX
		if(tp_getAdjustData()) return 0; // 已经校准
		else			  		// 未校准?
		{ 										    
			LCD_ClearAll(WHITE);	// 清屏
			tp_adjust();  		// 屏幕校准  
		}			
		tp_getAdjustData();	
	}
	return 1; 									 
}


#endif /* #if TP_USE */

#endif /* #if (USE_LCD_CONTROLLER == 1) */

#endif /* #ifdef USE_LCD_CONTROLLER */
