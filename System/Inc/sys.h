#ifndef __SYS_H
#define __SYS_H	

#include "stm32f10x.h"
#include "debug_log.h"
#include <stdio.h>
#include <stdbool.h>

/* 系统tick获取 */
typedef uint32_t (*tickGet_func_t)(void);  /* 时间戳获取（FreeRTOS）函数类型定义 */
extern tickGet_func_t  get_tick_ms;

extern IRQn_Type irqList[4];
 
/* =====================外设模块使用开启宏===================== */
#define  UART_LOG_OUT            1      /* 使用串口打印日志信息 */

#define  LED_IS_USE              1      /* 使用LED */

#define  KEY_IS_USE              1      /* 使用按键 */

#define  BUZZER_IS_USE           1      /* 使用蜂鸣器 */

#define  MOTOR_IS_USE            1      /* 使用无刷直流电机 */
#if MOTOR_IS_USE
    #define MOTOR_TYPE           0      /* 0:BLDC-无刷直流电机  1:BDC-有刷直流电机 */
#endif

#define  PID_IS_USE              1      /* 1:系统某部分引入PID算法 */

#define  CRC32_IS_USE            1      /* 1:系统某部分引入CRC32校验 */

#define  SENSOR_IS_USE           1      /* 使用传感器 */
#if SENSOR_IS_USE
   #define  USE_SENSOR_AHT20     1      /* 使用AHT20传感器 */
   #define  AHT20_I2C_ACHIVE_WAY 0      /* I2C驱动实现 - 0:软件I2C  1:硬件I2C */

   #define  USE_SENSOR_MQ2       1      /* 使用MQ2传感器 */
#endif /* #if USE_SENSOR */

#define  LCD_IS_USE              1      /* 使用LCD */
#if LCD_IS_USE
   #define USE_LCD_CONTROLLER    1      /* 不同驱动主控 LCD选择  var 0:ST7735S  1:ILI9341 2:SSD1963 */
   #define LCD_BL_ACHWAY         0      /* LCD背光实现驱动方式 0:普通GPIO 1:定时器PWM */
   #define TRANS_USE_DMA         0      /* 0：不使用DMA 1：使用DMA */
   #define LCD_DISP_DIR          1      /* LCD显示方向：1-横屏显示 0-竖屏显示 */
   #define TP_USE                1      /* 0：不使用LCD的触摸功能 1：使用触摸功能 */
#endif /* #if LCD_IS_USE */


#define  KEY_IS_USE              1      /* 使用按键 */
#if KEY_IS_USE 
   #define USER_KEY_USE          1      /* 使用用户按键 */
   #define WKUP_KEY_USE          1      /* 使用唤醒按键 */
#endif /* KEY_IS_USE */

#define EXTEEP_IS_USE            1      /* 使用EEPROM */
#if EXTEEP_IS_USE
   #define USE_EEP_TP  0 /* 使用的具体EEP型号 0:AT24C系列 1:... */
   #if (USE_EEP_TP == 0) /* AT24CXX */
      #define USE_AT24CXX  2 /* 使用的具体AT24C型号 - val: AT24C[val] 如 2 - AT24C02 */
      #define EEP_MEMCAP (USE_AT24CXX * 128 - 1)
   #elif (USE_EEP_TP == 1) /* ... */
   /* ... */
   #endif /* #if !USE_EEP_TYPE */
#endif /* #if EXTEEP_IS_USE */

#define  WINDSPEED_IS_USE        1   /* 烟机系统需要风速计算 */

#define  SYSTEM_SUPPORT_OS	     1	 /* 定义系统文件夹是否支持OS  0:不支持os 1:支持os */

#define  PID_DEBUG               1   /* PID调试 0：关闭 1：开启 */
#define  PRINT_USE               0   /* 使用printf打印 */

#define  HARDWARE_UPDATE_OPEN	 0   /* 固件升级功能开关,1打开,0关闭 */

#if HARDWARE_UPDATE_OPEN  /* 如果需要固件升级 */
    #define HW_UPDATE_METHOD     0   /* 固件升级的媒介 0: 有线IAP 1: 无线OTA */

#endif /* #if HARDWARE_UPDATE_OPEN */

/**************功能开关********************************/

/* 位带操作,实现51类似的GPIO控制功能
   具体实现思想,参考<<CM3权威指南>>第五章(87页~92页).
   IO口操作宏定义  */
#define BITBAND(addr, bitnum) ((addr&0xF0000000)+0x2000000+((addr&0xFFFFF)<<5)+(bitnum<<2)) 
#define MEM_ADDR(addr)  *((volatile unsigned long *)(addr)) 
#define BIT_ADDR(addr, bitnum)   MEM_ADDR(BITBAND(addr, bitnum)) 
//IO口地址映射
#define GPIOA_ODR_Addr    (GPIOA_BASE+12) //0x4001080C 
#define GPIOB_ODR_Addr    (GPIOB_BASE+12) //0x40010C0C 
#define GPIOC_ODR_Addr    (GPIOC_BASE+12) //0x4001100C 
#define GPIOD_ODR_Addr    (GPIOD_BASE+12) //0x4001140C 
#define GPIOE_ODR_Addr    (GPIOE_BASE+12) //0x4001180C 
#define GPIOF_ODR_Addr    (GPIOF_BASE+12) //0x40011A0C    
#define GPIOG_ODR_Addr    (GPIOG_BASE+12) //0x40011E0C    

#define GPIOA_IDR_Addr    (GPIOA_BASE+8) //0x40010808 
#define GPIOB_IDR_Addr    (GPIOB_BASE+8) //0x40010C08 
#define GPIOC_IDR_Addr    (GPIOC_BASE+8) //0x40011008 
#define GPIOD_IDR_Addr    (GPIOD_BASE+8) //0x40011408 
#define GPIOE_IDR_Addr    (GPIOE_BASE+8) //0x40011808 
#define GPIOF_IDR_Addr    (GPIOF_BASE+8) //0x40011A08 
#define GPIOG_IDR_Addr    (GPIOG_BASE+8) //0x40011E08 
 
/* IO口操作,只对单一的IO口! 需要确保n的值小于16! */
#define PAout(n)   BIT_ADDR(GPIOA_ODR_Addr,n)  //输出 
#define PAin(n)    BIT_ADDR(GPIOA_IDR_Addr,n)  //输入 

#define PBout(n)   BIT_ADDR(GPIOB_ODR_Addr,n)  //输出 
#define PBin(n)    BIT_ADDR(GPIOB_IDR_Addr,n)  //输入 

#define PCout(n)   BIT_ADDR(GPIOC_ODR_Addr,n)  //输出 
#define PCin(n)    BIT_ADDR(GPIOC_IDR_Addr,n)  //输入 

#define PDout(n)   BIT_ADDR(GPIOD_ODR_Addr,n)  //输出 
#define PDin(n)    BIT_ADDR(GPIOD_IDR_Addr,n)  //输入 

#define PEout(n)   BIT_ADDR(GPIOE_ODR_Addr,n)  //输出 
#define PEin(n)    BIT_ADDR(GPIOE_IDR_Addr,n)  //输入

#define PFout(n)   BIT_ADDR(GPIOF_ODR_Addr,n)  //输出 
#define PFin(n)    BIT_ADDR(GPIOF_IDR_Addr,n)  //输入

#define PGout(n)   BIT_ADDR(GPIOG_ODR_Addr,n)  //输出 
#define PGin(n)    BIT_ADDR(GPIOG_IDR_Addr,n)  //输入

#define IO_OP(port, pin, n)         do{ if(port == GPIOA) PAout(pin) = n; \
                                        else if(port == GPIOB) PBout(pin) = n; \
                                        else if(port == GPIOC) PCout(pin) = n; \
                                        else if(port == GPIOD) PDout(pin) = n; \
                                        else if(port == GPIOE) PEout(pin) = n; \
                                        else if(port == GPIOF) PFout(pin) = n; \
                                        else if(port == GPIOG) PGout(pin) = n; } while(0)

#define IO_READ(port, pin, val)     do{ if(port == GPIOA) val = PAin(pin); \
                                        else if(port == GPIOB) val = PBin(pin); \
                                        else if(port == GPIOC) val = PCin(pin); \
                                        else if(port == GPIOD) val = PDin(pin); \
                                        else if(port == GPIOE) val = PEin(pin); \
                                        else if(port == GPIOF) val = PFin(pin); \
                                        else if(port == GPIOG) val = PGin(pin); } while(0)


/* ========== 输入模式（浮空/上拉输入） ========== */
#define IO_IN(PORT, PIN)  do { \
    if ((PIN) < 8) { \
        (PORT)->CRL &= ~(0x0F << ((PIN) * 4)); \
        (PORT)->CRL |= ((uint32_t)8 << ((PIN) * 4)); \
    } else { \
        (PORT)->CRH &= ~(0x0F << (((PIN) - 8) * 4)); \
        (PORT)->CRH |= ((uint32_t)8 << (((PIN) - 8) * 4)); \
    } \
} while(0)

/* ============ 输出模式（推挽输出） ============ */
#define IO_OUT(PORT, PIN) do { \
    if ((PIN) < 8) { \
        (PORT)->CRL &= ~(0x0F << ((PIN) * 4)); \
        (PORT)->CRL |= ((uint32_t)3 << ((PIN) * 4)); \
    } else { \
        (PORT)->CRH &= ~(0x0F << (((PIN) - 8) * 4)); \
        (PORT)->CRH |= ((uint32_t)3 << (((PIN) - 8) * 4)); \
    } \
} while(0)

#define HIGH  1
#define LOW   0

#define ON  true
#define OFF false

#define EXTI_Line(x)   (((u32)1) << x)

#define PORT_NUM(GPIO_PORT)   (((((u32)(GPIO_PORT))-APB2PERIPH_BASE)>>10)-2)  /* GPIOA-0 ... GPIOG-6 */

// ========== 总线级时钟宏（推荐） ==========
#define PERIPH_APB2_RCC(PERIPH)   RCC_APB2Periph_##PERIPH  
#define PERIPH_APB1_RCC(PERIPH)   RCC_APB1Periph_##PERIPH  
#define PERIPH_AHB_RCC(PERIPH)    RCC_AHBPeriph_##PERIPH

/* 函数指针类型定义 */
typedef void(*KeyOnChip)(void);

/* 以下为汇编函数 */
void WFI_SET(void);		//执行WFI指令
void INTX_DISABLE(void);	//关闭所有中断
void INTX_ENABLE(void);		//开启所有中断
void MSR_MSP(u32 addr);		//设置堆栈地址

#endif
