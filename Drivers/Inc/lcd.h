#ifndef __LCD_H
#define __LCD_H

#include "spi.h"
#include "gpio.h"

#if LCD_IS_USE   /* 使用LCD 作UI显示 */

/* LCD重要参数集 */
typedef struct  
{										    
	u16 width;			// LCD 宽度
	u16 height;		    // LCD 高度
	u16 id;			    // LCD ID
	u8  dir;			// 横屏还是竖屏控制：0，竖屏；1，横屏。	
	u16 wramcmd;        // 开始写gram指令
	u16 setxcmd;		// 设置x坐标指令
	u16 setycmd;		// 设置y坐标指令 
} lcd_dev; 	  

/* LCD参数 */
extern lcd_dev lcddev;	//管理LCD重要参数

/* ==================== 引脚配置（根据实际硬件修改） ==================== */
#if (LCD_BL_ACHWAY == 0)  /* 背光实现-GPIO */
    #define	LCD_BL          PBout(0) 
    #define LCD_BL_PORT     GPIOB
    #define LCD_BL_PIN      GPIO_Pin_0
    #define LCD_BL_ON       GPIO_SetBits(LCD_BL_PORT, LCD_BL_PIN)
    #define LCD_BL_OFF      GPIO_ResetBits(LCD_BL_PORT, LCD_BL_PIN)
#elif (LCD_BL_ACHWAY == 1) /* 背光实现-TIM PWM */
    #define BL_TIMx         TIM4
    #define BL_TIM_CH        4
    #define BL_DUTY          80
#endif /* #if (LCD_BL_ACHWAY == 0) */

#if (USE_LCD_CONTROLLER == 0)  /* ST7735S */
	#define LCD_RES_PORT    GPIOB
	#define LCD_RES_PIN     GPIO_Pin_2

	#define LCD_RES_LOW     GPIO_ResetBits(LCD_RES_PORT, LCD_RES_PIN)
	#define LCD_RES_HIGH    GPIO_SetBits(LCD_RES_PORT, LCD_RES_PIN)

    #define LCD_CS_PORT     GPIOB
    #define LCD_CS_PIN      GPIO_Pin_0
    #define LCD_DC_PORT     GPIOB
    #define LCD_DC_PIN      GPIO_Pin_1

    #define LCD_CS_LOW      GPIO_ResetBits(LCD_CS_PORT, LCD_CS_PIN)
    #define LCD_CS_HIGH     GPIO_SetBits(LCD_CS_PORT, LCD_CS_PIN)
    #define LCD_DC_CMD      GPIO_ResetBits(LCD_DC_PORT, LCD_DC_PIN)
    #define LCD_DC_DATA     GPIO_SetBits(LCD_DC_PORT, LCD_DC_PIN)

/* 尺寸 */
    #define LCD_WIDTH       128
    #define LCD_HEIGHT      160

#elif (USE_LCD_CONTROLLER == 1)   /* ILI9341 */
    //LCD分辨率设置
    #define ILI9341_HOR_RESOLUTION     320		//LCD水平分辨率
    #define ILI9341_VER_RESOLUTION     240		//LCD垂直分辨率

    //LCD驱动参数设置 (基于ILI9341数据手册推荐值)
    #define ILI9341_HOR_PULSE_WIDTH    10	//水平脉宽 (H_Low)
    #define ILI9341_HOR_BACK_PORCH     30	//水平后廊 (HBP)
    #define ILI9341_HOR_FRONT_PORCH    20	//水平前廊 (HFP)

    #define ILI9341_VER_PULSE_WIDTH    2	//垂直脉宽 (V_Low)
    #define ILI9341_VER_BACK_PORCH     16	//垂直后廊 (VBP)
    #define ILI9341_VER_FRONT_PORCH    8	//垂直前廊 (VFP)

    //自动计算的总周期参数
    #define ILI9341_HT  (ILI9341_HOR_RESOLUTION + ILI9341_HOR_PULSE_WIDTH + ILI9341_HOR_BACK_PORCH + ILI9341_HOR_FRONT_PORCH)
    #define ILI9341_HPS (ILI9341_HOR_PULSE_WIDTH + ILI9341_HOR_BACK_PORCH)
    #define ILI9341_VT  (ILI9341_VER_PULSE_WIDTH + ILI9341_VER_BACK_PORCH + ILI9341_VER_FRONT_PORCH + ILI9341_VER_RESOLUTION)
    #define ILI9341_VSP (ILI9341_VER_PULSE_WIDTH + ILI9341_VER_BACK_PORCH)

#if LCD_DISP_DIR /* 横屏显示 */
    #define LCD_WIDTH     ILI9341_HOR_RESOLUTION
    #define LCD_HEIGHT    ILI9341_VER_RESOLUTION
#else /* 竖屏显示 */
    #define LCD_WIDTH     ILI9341_VER_RESOLUTION
    #define LCD_HEIGHT    ILI9341_HOR_RESOLUTION
#endif /* #if LCD_DISP_DIR */

/* LCD的画笔颜色和背景色 */ 
extern u16  pointColor; // 画笔默认红色    
extern u16  bgColor;    // 背景颜色，默认为白色

/******************************************************************************
2^26 =0X0400 0000 = 64MB,每个 BANK 有4*64MB = 256MB
64MB:FSMC_Bank1_NORSRAM4:0X6000 0000 ~ 0X63FF FFFF
64MB:FSMC_Bank1_NORSRAM2:0X6400 0000 ~ 0X67FF FFFF
64MB:FSMC_Bank1_NORSRAM3:0X6800 0000 ~ 0X6BFF FFFF
64MB:FSMC_Bank1_NORSRAM4:0X6C00 0000 ~ 0X6FFF FFFF

选择BANK1-NORSRAM1 连接 TFT，地址范围为0X6000 0000 ~ 0X63FF FFFF
此LCD选择 FSMC_A10 接LCD的DC(寄存器/数据选择)脚
寄存器基地址 = 0X6C00 0000
RAM基地址 = 0X6C00 2000 = 0X6000 0000+(1<<(12+1))
如果电路设计时选择不同的地址线时，地址要重新计算  
*******************************************************************************/
/******************************* ILI9341 显示屏的 FSMC 参数定义 ***************/
#define FSMC_LCD_CMD       ((u32)(0x6C000000 | 0x000007FE))	 // FSMC_Bank1_NORSRAM4 用于LCD命令操作的地址
#define FSMC_LCD_DATA      ((u32)(FSMC_LCD_CMD + 2))    // FSMC_Bank1_NORSRAM4 用于LCD数据操作的地址--对应地址线 A10-PG0 
#define LCD_WRITE_CMD(x)   *(__IO u16 *)FSMC_LCD_CMD  = x 
#define LCD_WRITE_DATA(x)  *(__IO u16 *)FSMC_LCD_DATA = x
#define LCD_READ_DATA()    *(__IO u16 *)FSMC_LCD_DATA

#define LCD_WRITE_REG(lcd_reg, lcd_regVal)   do{ LCD_WRITE_CMD(lcd_reg); LCD_WRITE_DATA(lcd_regVal); } while(0)
#define LCD_READ_REG(lcd_reg, tarVal) do { LCD_WRITE_CMD(lcd_reg); delay_us(5); tarVal = LCD_READ_DATA(); } while(0)

#define FSMC_LCD_BANKx     FSMC_Bank1_NORSRAM4    // NE4

/* 扫描方向定义 */
typedef enum {
    L2R_U2D = 0,   //从左到右,从上到下
    L2R_D2U,       //从左到右,从下到上
    R2L_U2D,       //从右到左,从上到下
    R2L_D2U,       //从右到左,从下到上
    U2D_L2R,       //从上到下,从左到右
    U2D_R2L,       //从上到下,从右到左
    D2U_L2R,       //从下到上,从左到右   
    D2U_R2L,       //从下到上,从右到左
} scan_dir;

#if (TRANS_USE_DMA == 1) /* 使用DMA */

/* ==================== DMA 行缓冲大小 ==================== */
#define LCD_DMA_BUF_ROWS    32   /* 一次 DMA 传输的行数 */
#define LCD_DMA_BUF_SIZE    (LCD_HOR_WIDTH * LCD_DMA_BUF_ROWS)  /* 480*32 = 15360 像素 = 30KB */

#endif /* #if (TRANS_USE_DMA == 1) */

#endif /* #if (USE_LCD_CONTROLLER == 0) */

/* ====================  颜色 ==================== */
/* 画笔颜色 */
#define WHITE         	 0xFFFF
#define BLACK         	 0x0000	  
#define BLUE         	 0x001F  
#define BRED             0XF81F
#define GRED 			 0XFFE0
#define GBLUE			 0X07FF
#define RED           	 0xF800
#define PINK          	 0xF81F
#define MAGENTA       	 0xF81F
#define GREEN         	 0x07E0
#define CYAN          	 0x7FFF
#define YELLOW        	 0xFFE0
#define BROWN 			 0XBC40 //棕色
#define BRRED 			 0XFC07 //棕红色
#define GRAY  			 0X8430 //灰色
/* GUI颜色 */
#define DARKBLUE      	 0X01CF	//深蓝色
#define LIGHTBLUE      	 0X7D7C	//浅蓝色  
#define GRAYBLUE       	 0X5458 //灰蓝色
/* 以上三色为PANEL的颜色 */
#define LIGHTGREEN     	 0X841F //浅绿色
//#define LIGHTGRAY        0XEF5B //浅灰色(PANNEL)
#define LGRAY 			 0XC618 //浅灰色(PANNEL),窗体背景色
#define LGRAYBLUE        0XA651 //浅灰蓝色(中间层颜色)
#define LBBLUE           0X2B12 //浅棕蓝色(选择条目的反色)

#define RGB565(r,g,b)   ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3))

typedef enum {
    USE_HORIZONTAL     = 0x00,
    USE_VERTICAL       = 0x01,
    USE_HORIZONTAL_INV = 0x02,
    USE_VERTICAL_INV   = 0x03,
} lcd_dir;

/* ==================== API ==================== */
void lcd_onChipCfg(void);
void LCD_ClearWindow(u16 usX,u16 usY,u16 usWidth,u16 usHeight,u16 usColor);
void LCD_ClearAll(u16 color);
void LCD_SetRotation(lcd_dir rot);
void LCD_SetWindow(u16 sx, u16 sy, u16 width, u16 height);
void LCD_Backlight_Init(void);
void LCD_Backlight_SetPercent(u8 percent);   // 0 ~ 100

/* 底层接口（也可供 GUI 直接使用） */
#if (USE_LCD_CONTROLLER == 0)   /* ST7735S */
void LCD_WriteCmd(u8 cmd);
void LCD_WriteData8(u8 data);
void LCD_WriteData16(u16 data);
void LCD_Reset(void);
void LCD_ST7735S_Init(void);

#elif (USE_LCD_CONTROLLER == 1)  /* ILI9341 */
extern void opt_delay(u8 i);
u16 LCD_BGR2RGB(u16 bgrVal);
void LCD_DisplayOff(void);
void LCD_DisplayOn(void);
void LCD_SetCursor(u16 Xpos, u16 Ypos);
void LCD_Scan_Dir(scan_dir dir);
void LCD_Display_Dir(u8 dir);

void LCD_ILI9341_Init(void);

#if (TRANS_USE_DMA == 1)   /* 使用DMA */
void LCD_FillRect_DMA(u16 x0, u16 y0, u16 x1, u16 y1, u16 color);        /* DMA 异步填充 */
void LCD_DrawImage_DMA(u16 x, u16 y, u16 w, u16 h, const u16 *img);
void LCD_FlushArea(u16 x0, u16 y0, u16 x1, u16 y1, const u16 *color_p);  /* LVGL 专用：刷新指定区域（从 RAM 缓冲到屏幕） */
u8 LCD_DMA_IsIdle(void);      /* 查询 DMA 是否空闲 */
void LCD_DMA_CpltCallback(void);   /* DMA 完成回调（弱定义，可被 LVGL 重写） */
#endif /* #if (TRANS_USE_DMA == 1) */

#elif (USE_LCD_CONTROLLER == 1) /* SSD1963 */
void SSD_Backlight_set(u8 pwm);

#endif /* #if (USE_LCD_CONTROLLER == 0) */

#endif  /* #if LCD_IS_USE */

#endif /* __LCD_H */

