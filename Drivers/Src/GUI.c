#include "GUI.h"

#if LCD_IS_USE  /* 使用LCD */

#include "FONT.h"
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <stdio.h>

/* 爱心轮廓上的关键点（相对坐标，左上角为原点）*/
// static const int16_t heart_outline_x[] = {
//     8, 9, 10, 11, 12, 13, 14, 14, 15, 15, 14, 13, 12, 11, 10, 9,
//     8, 7, 6, 5, 4, 3, 2, 1, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7
// };
// static const int16_t heart_outline_y[] = {
//     4, 3, 2, 2, 2, 2, 3, 4, 6, 8, 10, 11, 12, 13, 14, 14,
//     15, 14, 14, 13, 12, 11, 10, 8, 6, 4, 3, 2, 2, 2, 2, 3, 4
// };
//static const uint8_t outline_points = sizeof(heart_outline_x) / sizeof(heart_outline_x[0]);

/* 16X16爱心绘图(1=绘制，0=不绘制) */
static const uint8_t heart_filled_map[16][2] = {
    {0x00, 0x00},  // 行0
    {0x0C, 0x60},  // ....##....##....
    {0x1E, 0xF0},  // ...####..####...
    {0x3F, 0xF8},  // ..##########....
    {0x7F, 0xFC},  // .############...
    {0x7F, 0xFC},  // .############...
    {0xFF, 0xFE},  // ##############..
    {0xFF, 0xFE},  // ##############..
    {0xFF, 0xFE},  // ##############..
    {0x7F, 0xFC},  // .############...
    {0x3F, 0xF8},  // ..##########....
    {0x1F, 0xF0},  // ...########.....
    {0x0F, 0xE0},  // ....######......
    {0x07, 0xC0},  // .....####.......
    {0x03, 0x80},  // ......##........
    {0x00, 0x00}   // ...............
};

/* ==================== 基础图形 ==================== */
void GUI_WritePixel(u16 x, u16 y, u16 color) {
#if (USE_LCD_CONTROLLER == 0) /* ST7735S */
    LCD_SetWindow(x, y, x, y);
    LCD_WriteCmd(0x2C); /* RAMWR */
    LCD_WriteData16(color);
#elif (USE_LCD_CONTROLLER == 1) /* ILI9341 */
    if (x >= lcddev.width || y >= lcddev.height) return;
    LCD_SetCursor(x,y);	 //设置光标位置 
	LCD_WRITE_CMD(lcddev.wramcmd);	//开始写入GRAM
	LCD_WRITE_DATA(color);
#endif /* #if (USE_LCD_CONTROLLER == 0) */
}

/**
  * 函数功能: 对LCD显示器的某一点获取颜色rgb值
  * 输入参数: 无
  * 返 回 值: u16:像素数据RGB565
  * 说    明：无
  */
static u16 GUI_Read_PixelData()
{	
	u16 r,g,b = 0;
	
	if(lcddev.id==0X9341||lcddev.id==0X6804||lcddev.id==0X5310||lcddev.id==0X1963)
        LCD_WRITE_CMD(0X2E);   //9341/6804/3510/1963 发送读GRAM指令
	else if(lcddev.id==0X5510)
        LCD_WRITE_CMD(0X2E00);	//5510 发送读GRAM指令
	else LCD_WRITE_CMD(0X22);  //其他IC发送读GRAM指令

	if(lcddev.id==0X9320) opt_delay(2);	//FOR 9320,延时2us	    
 	r = LCD_READ_DATA();				//dummy Read	   
	if(lcddev.id==0X1963) return r;		//1963直接读就可以 
	opt_delay(2);	  
 	r = LCD_READ_DATA();  		  		//实际坐标颜色
 	if(lcddev.id==0X9341||lcddev.id==0X5310||lcddev.id==0X5510)		//9341/NT35310/NT35510要分2次读出
 	{
		opt_delay(2);	  
		b = LCD_READ_DATA(); 
		g = r & 0XFF;	//对于9341/5310/5510,第一次读取的是RG的值,R在前,G在后,各占8位
		g <<= 8;
	} 
	if(lcddev.id==0X9325||lcddev.id==0X4535||lcddev.id==0X4531||lcddev.id==0XB505||lcddev.id==0XC505)
        return r;	//这几种IC直接返回颜色值
	else if(lcddev.id==0X9341||lcddev.id==0X5310||lcddev.id==0X5510)
        return (((r>>11)<<11)|((g>>10)<<5)|(b>>11)); //ILI9341/NT35310/NT35510需要公式转换一下
	else return LCD_BGR2RGB(r);						 //其他IC
}

/**
  * 函数功能: 获取 LCD 上某一个坐标点的像素数据
  * 输入参数: x ：在特定扫描方向下窗口的起点X坐标 
             y ：在特定扫描方向下窗口的起点Y坐标
  * 返 回 值: u16:像素数据RGB565
  * 说    明：无
  */
u16 GUI_GetPixel(u16 x, u16 y) {
	if(x >= lcddev.width || y >= lcddev.height) return 0;	// 超过了范围,直接返回		   
	LCD_SetCursor(x,y);	    
	return GUI_Read_PixelData();
}

#if 0
/* 画线 x1,y1:起点坐标 x2,y2:终点坐标 */
void GUI_DrawLine(u16 x0, u16 y0, u16 x1, u16 y1, u16 color) 
{
#if (USE_LCD_CONTROLLER == 0) /* ST7735S */
    int16_t dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int16_t dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;

    while (1) {
        GUI_WritePixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
#elif (USE_LCD_CONTROLLER == 1) /* ILI9341 */
	u16 t; 
	int xerr=0,yerr=0,delta_x,delta_y,distance; 
	int incx,incy,uRow,uCol; 
	delta_x=x1-x0; //计算坐标增量 
	delta_y=y1-y0; 
	uRow=x0; 
	uCol=y0; 
	if(delta_x>0) incx=1; // 设置单步方向 
	else if(delta_x==0) incx=0; // 垂直线 
	else {incx=-1;delta_x=-delta_x;} 
	if(delta_y>0)incy=1; 
	else if(delta_y==0)incy=0;//水平线 
	else{incy=-1;delta_y=-delta_y;} 
	if(delta_x>delta_y)distance=delta_x; //选取基本增量坐标轴 
	else distance=delta_y; 
	for(t=0;t <= distance+1;t++ ) { // 画线输出  
		GUI_WritePixel(uRow,uCol,color); //画点 
		xerr+=delta_x ; 
		yerr+=delta_y ; 
		if(xerr>distance) { 
			xerr-=distance; 
			uRow+=incx; 
		} 
        if(yerr>distance) { 
			yerr-=distance; 
			uCol+=incy; 
		} 
	}  
#endif /* #if (USE_LCD_CONTROLLER == 0) */
}    
#else 
/**
  * 函数功能: 在 LCD 显示器上使用 Bresenham 算法画线段
  * 输入参数: x0 ：在特定扫描方向下窗口的起点X坐标
  *           y0 ：在特定扫描方向下窗口的起点Y坐标
  *           x1 ：在特定扫描方向下线段的另一个端点X坐标
  *           y1 ：在特定扫描方向下线段的另一个端点Y坐标
  *           color ：线段的颜色
  * 返 回 值: 无
  * 说    明：无
  */
void GUI_DrawLine(u16 x0,u16 y0,u16 x1,u16 y1,u16 color)
{
	u16 us; 
	u16 usX_Current, usY_Current;
	int32_t lError_X=0,lError_Y=0,lDelta_X,lDelta_Y,lDistance; 
	int32_t lIncrease_X, lIncrease_Y;
	
	lDelta_X=x1-x0; //计算坐标增量 
	lDelta_Y=y1-y0; 
	usX_Current = x0; 
	usY_Current = y0; 
	
	if(lDelta_X>0) lIncrease_X=1; //设置单步方向 
	else if(lDelta_X==0) lIncrease_X=0; //垂直线 
	else { lIncrease_X=-1; lDelta_X=-lDelta_X; }

    if(lDelta_Y>0) lIncrease_Y=1;
    else if(lDelta_Y==0) lIncrease_Y=0; //水平线 
    else { lIncrease_Y=-1; lDelta_Y=-lDelta_Y; }
	
	if(lDelta_X>lDelta_Y) lDistance=lDelta_X; //选取基本增量坐标轴 
	else lDistance=lDelta_Y; 
	
	for(us=0;us<=lDistance+1;us++) {   // 画线输出 
		GUI_WritePixel(usX_Current,usY_Current,color);  // 画点 
		lError_X+=lDelta_X; 
		lError_Y+=lDelta_Y;
		if(lError_X>lDistance) { 
			lError_X-=lDistance; 
			usX_Current+=lIncrease_X; 
		}
		if(lError_Y>lDistance) { 
			lError_Y-=lDistance; 
			usY_Current+=lIncrease_Y; 
		}		
	}
}
#endif 

/**
  * 函数功能: 在LCD显示器上画一个矩形
  * 输入参数: usX_Start ：在特定扫描方向下窗口的起点X坐标
  *           usY_Start ：在特定扫描方向下窗口的起点Y坐标
  *           usWidth：矩形的宽度（单位：像素）
  *           usHeight：矩形的高度（单位：像素）
  *           color ：矩形的颜色
  *           isFill ：选择是否填充该矩形
  *             可选值：0：空心矩形
  *                     1：实心矩形
  * 返 回 值: 无
  * 说    明：无
  */
void GUI_DrawRect(u16 usX_Start, u16 usY_Start, u16 usWidth, u16 usHeight, u16 color, u8 isFill )
{
	if(isFill) {
	    LCD_ClearWindow(usX_Start, usY_Start, usWidth, usHeight, color);
    } else {
		GUI_DrawLine(usX_Start, usY_Start, usX_Start + usWidth - 1, usY_Start, color);
		GUI_DrawLine(usX_Start, usY_Start + usHeight - 1, usX_Start + usWidth - 1, usY_Start + usHeight - 1, color);
		GUI_DrawLine(usX_Start, usY_Start, usX_Start, usY_Start + usHeight - 1, color);
		GUI_DrawLine(usX_Start + usWidth - 1, usY_Start, usX_Start + usWidth - 1, usY_Start + usHeight - 1, color);		
	}
    LCD_SetWindow(0,0,lcddev.width-1,lcddev.height-1);
}

/**
 * @brief 用指定颜色填充矩形区域
 * @param x0 矩形左上角（或任意对角点）X 坐标
 * @param y0 矩形左上角（或任意对角点）Y 坐标
 * @param x1 矩形右下角（或另一对角点）X 坐标
 * @param y1 矩形右下角（或另一对角点）Y 坐标
 * @param color 填充颜色（16位RGB565格式）
 */
void GUI_FillRect(u16 x0, u16 y0, u16 x1, u16 y1, u16 color)
{
#if (USE_LCD_CONTROLLER == 0) /* ST7735S */
    if (x0 > x1) { u16 t=x0; x0=x1; x1=t; }
    if (y0 > y1) { u16 t=y0; y0=y1; y1=t; }

    u16 w=x1 - x0 + 1;
    u16 h=y1 - y0 + 1;
    LCD_SetWindow(x0, y0, x1, y1);
    LCD_WriteCmd(0x2C);
    LCD_FillColor(color, (u32)w * h);
#elif (USE_LCD_CONTROLLER == 1) /* ILI9341 */
    u16 temp;
    u16 width, height;
    u16 i, j;

    // 确保坐标顺序：x0 <= x1, y0 <= y1
    if (x0 > x1) {
        temp = x0;
        x0 = x1;
        x1 = temp;
    }
    if (y0 > y1) {
        temp = y0;
        y0 = y1;
        y1 = temp;
    }

    width  = x1 - x0 + 1;
    height = y1 - y0 + 1;

    // 逐行填充
    for (i = 0; i < height; i++) {
        LCD_SetCursor(x0, y0 + i);          // 设置光标到当前行起始位置
        LCD_WRITE_CMD(lcddev.wramcmd);     // 准备写入GRAM（如果需要）
        for (j = 0; j < width; j++) {
            LCD_WRITE_DATA(color);      // 写入颜色数据
        }
    }
#endif /* #if (USE_LCD_CONTROLLER == 0) */
}

/**
 * @brief 绘制圆形轮廓（Bresenham算法）
 * @param x0   圆心 X 坐标
 * @param y0   圆心 Y 坐标
 * @param r    半径
 * @param color 颜色值
 */
void GUI_DrawCircle(u16 x0, u16 y0, u16 r, u16 color, u8 isFill)
{
#if (USE_LCD_CONTROLLER == 0)  /* ST7735S */
    int16_t x = 0;
    int16_t y = r;
    int16_t d = 1 - r;          // 决策参数（初始值）
    int16_t deltaE = 3;         // 用于更新决策参数（实际可用公式，此处保留原算法）

    // 如果半径无效，直接返回
    if (r <= 0) return;

    // 绘制上下左右四个轴点（防止因循环条件而遗漏）
    GUI_WritePixel(x0, y0 + r, color);
    GUI_WritePixel(x0, y0 - r, color);
    GUI_WritePixel(x0 + r, y0, color);
    GUI_WritePixel(x0 - r, y0, color);

    while (x < y) {
        if (d < 0) {
            d += 2 * x + 3;
        } else {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;

        // 利用八对称性绘制点
        GUI_WritePixel(x0 + x, y0 + y, color);
        GUI_WritePixel(x0 - x, y0 + y, color);
        GUI_WritePixel(x0 + x, y0 - y, color);
        GUI_WritePixel(x0 - x, y0 - y, color);
        GUI_WritePixel(x0 + y, y0 + x, color);
        GUI_WritePixel(x0 - y, y0 + x, color);
        GUI_WritePixel(x0 + y, y0 - x, color);
        GUI_WritePixel(x0 - y, y0 - x, color);
    }
#elif (USE_LCD_CONTROLLER == 1)
    int16_t x = 0, y = r;
	int16_t sError = 3 - (r<<1);   //判断下个点位置的标志
	
	while(x <= y)
	{
		int16_t sCountY;		
		if(isFill)
        {			
			for(sCountY = x;sCountY <= y;sCountY++) {                      
				GUI_WritePixel(x0+x,y0+sCountY,color);        //1，研究对象 
				GUI_WritePixel(x0-x,y0+sCountY,color);        //2       
				GUI_WritePixel(x0-sCountY,y0+x,color);        //3
				GUI_WritePixel(x0-sCountY,y0-x,color);        //4
				GUI_WritePixel(x0-x,y0-sCountY,color);        //5    
                GUI_WritePixel(x0+x,y0-sCountY,color);        //6
				GUI_WritePixel(x0+sCountY,y0-x,color);        //7 	
                GUI_WritePixel(x0+sCountY,y0+x,color);        //0				
			}
        } else {          
			GUI_WritePixel(x0+x,y0+y,color);             //1，研究对象
			GUI_WritePixel(x0-x,y0+y,color);             //2      
			GUI_WritePixel(x0-y,y0+x,color);             //3
			GUI_WritePixel(x0-y,y0-x,color);             //4
			GUI_WritePixel(x0-x,y0-y,color);             //5       
			GUI_WritePixel(x0+x,y0-y,color);             //6
			GUI_WritePixel(x0+y,y0-x,color);             //7 
			GUI_WritePixel(x0+y,y0+x,color);             //0
        }			
				
		if(sError<0) sError+=(4*x+6);	  
		else {
			sError +=(10+4*(x-y));   
			y--;
		} 
        x++;
	}
#endif /* #if (USE_LCD_CONTROLLER == 0) */
}

void GUI_FillCircle(u16 x0, u16 y0, u16 r, u16 color)
{
#if (USE_LCD_CONTROLLER == 0) /* ST7735S */
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    GUI_DrawLine(x0 - r, y0, x0 + r, y0, color);
    while (x < y) {
        if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
        x++; ddF_x += 2; f += ddF_x;
        GUI_DrawLine(x0 - x, y0 + y, x0 + x, y0 + y, color);
        GUI_DrawLine(x0 - x, y0 - y, x0 + x, y0 - y, color);
        GUI_DrawLine(x0 - y, y0 + x, x0 + y, y0 + x, color);
        GUI_DrawLine(x0 - y, y0 - x, x0 + y, y0 - x, color);
    }
#elif (USE_LCD_CONTROLLER == 1)  /* ILI9341 */
    int16_t x = 0;
    int16_t y = r;
    int16_t d = 1 - r;               // 决策参数
    int16_t y_min = (y0 > r) ? (y0 - r) : 0;
    int16_t y_max = (y0 + r < lcddev.height) ? (y0 + r) : (lcddev.height - 1);
    
    // 为每个可能出现的 y 坐标分配左右边界数组（动态栈数组，半径不大时安全）
    // 注意：如果半径很大（> 512），可能栈溢出，此时可改用全局数组或 malloc
    int16_t left_border[lcddev.height];   // 假设 lcddev.height ≤ 480，栈空间足够
    int16_t right_border[lcddev.height];
    
    // 初始化边界：左边界设为一个大数，右边界设为 -1
    for (u16 i = y_min; i <= y_max; i++) {
        left_border[i] = lcddev.width;
        right_border[i] = -1;
    }
    
    // 1. 利用 Bresenham 算法扫描圆上的点，记录每行的最左和最右 x
    while (x <= y) {
        // 八个对称点
        int16_t x1 = x, y1 = y;
        int16_t x2 = x, y2 = -y;
        int16_t x3 = -x, y3 = y;
        int16_t x4 = -x, y4 = -y;
        int16_t x5 = y, y5 = x;
        int16_t x6 = y, y6 = -x;
        int16_t x7 = -y, y7 = x;
        int16_t x8 = -y, y8 = -x;
        
        // 更新对应行的左右边界
        // 注意：实际屏幕坐标 = 圆心 + 偏移
        int16_t rows[] = {y0 + y1, y0 + y2, y0 + y3, y0 + y4,
                          y0 + y5, y0 + y6, y0 + y7, y0 + y8};
        int16_t cols[] = {x0 + x1, x0 + x2, x0 + x3, x0 + x4,
                          x0 + x5, x0 + x6, x0 + x7, x0 + x8};
                          
        for (int i = 0; i < 8; i++) {
            int16_t row = rows[i];
            int16_t col = cols[i];
            if (row >= y_min && row <= y_max) {
                if (col < left_border[row]) left_border[row] = col;
                if (col > right_border[row]) right_border[row] = col;
            }
        }
        
        // 更新决策变量
        x++;
        if (d < 0) {
            d += 2 * x + 1;
        } else {
            y--;
            d += 2 * (x - y) + 1;
        }
    }
    
    // 2. 根据记录的边界逐行填充
    for (int16_t row = y_min; row <= y_max; row++) {
        if (left_border[row] <= right_border[row]) {
            // 裁剪到屏幕范围内（已保证不越界，但边界值可能超出，再保险一次）
            int16_t l = left_border[row];
            int16_t r = right_border[row];
            if (l < 0) l = 0;
            if (r >= lcddev.width) r = lcddev.width - 1;
            // 调用快速水平填充函数（FSMC 方式，直接写显存）
            GUI_FillRect(l, row, r, row, color);
        }
    }
#endif 
}

/* 画水平线 x0,y0:坐标 len:线长度 color:颜色 */
void GUI_DrawhLine(u16 x0,u16 y0,u16 len,u16 color)
{
#if (USE_LCD_CONTROLLER == 0) /* ST7735S */
    if (len == 0) return;
    if (y0 >= lcddev.height) return;                     // 超出垂直范围
    u16 x1 = x0 + len - 1;
    if (x0 >= lcddev.width || x1 < 0) return;            // 完全超出水平范围

    // 边界裁剪
    if (x0 < 0) {
        len += x0;
        x0 = 0;
    }
    if (x1 >= lcddev.width) {
        len = lcddev.width - x0;
    }
    if (len == 0) return;

    // 设置列地址范围（命令 0x2A）
    LCD_WriteCmd(0x2A);
    LCD_WriteData8(x0 >> 8);
    LCD_WriteData8(x0 & 0xFF);
    LCD_WriteData8((x0 + len - 1) >> 8);
    LCD_WriteData8((x0 + len - 1) & 0xFF);

    // 设置行地址范围（命令 0x2B），起始和结束 y 相同
    LCD_WriteCmd(0x2B);
    LCD_WriteData8(y0 >> 8);
    LCD_WriteData8(y0 & 0xFF);
    LCD_WriteData8(y0 >> 8);
    LCD_WriteData8(y0 & 0xFF);

    // 写内存命令（命令 0x2C）
    LCD_WriteCmd(0x2C);

    // 连续发送 len 个颜色数据（每个数据由 LCD_WriteData16 完成 SPI 传输）
    for (u16 i = 0; i < len; i++) {
        LCD_WriteData16(color);
    }
#elif (USE_LCD_CONTROLLER == 1) /* ILI9341 */
    if(len==0) return;
	GUI_Fill(x0,y0,x0+len-1,y0,color);
#endif /* #if (USE_LCD_CONTROLLER == 0) */
}

/**
 * @brief 在指定位置显示一个字符，支持多种字体和叠加/非叠加模式
 * @param x       左上角 X 坐标
 * @param y       左上角 Y 坐标
 * @param chr     要显示的字符（ASCII 32~126）
 * @param color   前景色
 * @param bgcolor 背景色（仅在 mode=0 时有效）
 * @param size    字体大小：ASCII_1206 / ASCII_1608 / ASCII_2412
 * @param mode    显示模式：0=非叠加（背景填充），1=叠加（背景透明，只画前景色）
 */
void GUI_ShowChar(u16 x, u16 y, const unsigned char chr, u16 color, u16 bgcolor,
                  GUI_FontSize_t size, u8 mode) 
{
    u8 Chr = chr;
    if(!IS_FONT(size)) { 
        //LOG_D("font not found!"); 
        return; 
    }
#if (USE_LCD_CONTROLLER == 0)  /* ST7735S */
    if (x >= lcddev.width || y >= lcddev.height) return;
    if (Chr < 32 || Chr > 126) Chr = 32;   // 不可打印字符替换为空格

    u8 idx = Chr - 32;

    if (size == ASCII_1608) {
        const u8 *p = asc2_1608[idx];
        for (u8 row = 0; row < 16; row++) {
            u8 byte = p[row];
            for (u8 col = 0; col < 8; col++) {
                u16 px = x + col, py = y + row;
                if (px < lcddev.width && py < lcddev.height) {
                    if (byte & (0x80 >> col)) GUI_WritePixel(px, py, color);
                    else if (!mode) GUI_WritePixel(px, py, bgcolor);
                }
            }
        }
    } else if (size == ASCII_2412) {
        const u16 *p = asc2_2412[idx];
        for (u8 row = 0; row < 24; row++) {
            u16 line = p[row];               // 低12位有效
            for (u8 col = 0; col < 12; col++) {
                u16 px = x + col, py = y + row;
                if (px < lcddev.width && py < lcddev.height) {
                    if (line & (0x0800 >> col)) GUI_WritePixel(px, py, color);
                    else if (!mode) GUI_WritePixel(px, py, bgcolor);
                }
            }
        }
    } else if (size == ASCII_1206) {
        const u8 *p = asc2_1206[idx];
        for (u8 row = 0; row < 12; row++) {
            u8 byte = p[row];
            for (u8 col = 0; col < 6; col++) {
                u16 px = x + col, py = y + row;
                if (px < lcddev.width && py < lcddev.height) {
                    if (byte & (0x20 >> col)) GUI_WritePixel(px, py, color);
                    else if (!mode) GUI_WritePixel(px, py, bgcolor);
                }
            }
        }
    }
#elif (USE_LCD_CONTROLLER == 1) /* ILI9341 */
    u8 temp,t1,t;
	u16 y0=y;
	u8 csize=(size/8+((size%8)?1:0))*(size/2);		//得到字体一个字符对应点阵集所占的字节数	
 	Chr = Chr - ' ';  //得到偏移后的值（ASCII字库是从空格开始取模，所以-' '就是对应字符的字库）
	for(t=0;t<csize;t++) {   
		if(size==ASCII_1206) temp=asc2_1206[Chr][t]; 	 	//调用1206字体
		else if(size==ASCII_1608) temp=asc2_1608[Chr][t];	//调用1608字体
		else if(size==ASCII_2412) temp=asc2_2412[Chr][t];	//调用2412字体
		else return;  //没有的字库
		for(t1 = 0;t1 < 8;t1++) {			    
			if(temp & 0x80) GUI_WritePixel_Fast(x,y,color);
			else if(mode == 0) GUI_WritePixel_Fast(x,y,bgcolor);
			temp <<= 1; y++;
			if(y >= lcddev.height) return;	// 超区域
			if((y - y0) == size) {
				y = y0; x++;
                if(x >= lcddev.width) return;	//超区域了
				break;
			}
		}  	 
	}  	    	   	 	
#endif /* #if (USE_LCD_CONTROLLER == 0) */
}

#if 0
/**
 * @brief 显示字符串（支持区域限制、自动换行）
 * @param x         起始X坐标
 * @param y         起始Y坐标
 * @param str       要显示的字符串（ASCII）
 * @param color     前景色
 * @param bgcolor   背景色
 * @param size      字体大小（ASCII_1608 / ASCII_2412 / ASCII_1206）
 * @param mode      显示模式（0=非叠加，1=叠加）
 * @param max_width 最大显示宽度（0表示不限制，超出则换行）
 * @param max_height 最大显示高度（0表示不限制，超出则裁剪）
 */
void GUI_ShowString(u16 x, u16 y, const char *str,
                    u16 color, u16 bgcolor,
                    GUI_FontSize_t size, u8 mode,
                    u16 max_width, u16 max_height)
{
    if (str == NULL) return;

    u16 start_x = x;
    u16 start_y = y;
    u16 cur_x = x;
    u16 cur_y = y;
    u8  char_width = 0;
    u8  char_height = 0;

    // 根据字体获取字符宽高
    switch (size) {
        case ASCII_1608:
            char_width = 8;
            char_height = 16;
            break;
        case ASCII_2412:
            char_width = 12;
            char_height = 24;
            break;
        case ASCII_1206:
            char_width = 6;
            char_height = 12;
            break;
        default:
            return;
    }

    while (*str) {
        // 检查是否超出最大高度（裁剪）
        if (max_height > 0 && (cur_y + char_height - 1) >= (start_y + max_height)) {
            break; // 超出显示区，停止绘制
        }

        // 处理换行：如果当前行剩余宽度不足以放下一个字符（且未超出最大宽度）
        if (max_width > 0 && (cur_x + char_width - 1) >= (start_x + max_width)) {
            cur_x = start_x;          // X 回到起始
            cur_y += char_height;     // Y 增加一行
            // 换行后再次检查高度
            if (max_height > 0 && (cur_y + char_height - 1) >= (start_y + max_height)) {
                break;
            }
            // 跳过可能因换行产生的首字符空格？不需要，直接继续循环显示下一个字符
        }

        // 显示当前字符（利用已有的 GUI_ShowChar）
        GUI_ShowChar(cur_x, cur_y, *str, color, bgcolor, size, mode);

        // 移动 X 坐标到下一个字符位置
        cur_x += char_width;
        str++;
    } 
}
#endif 

void GUI_ShowString(u16 x, u16 y, const char *str,
                    u16 color, u16 bgcolor,
                    GUI_FontSize_t size, u8 mode,
                    u16 max_width, u16 max_height) 
{
#if (USE_LCD_CONTROLLER == 0)  /* ST7735S */
    u8 x_offset = x;
    while (*str) {
        if (size == ASCII_1608) {
            if (x_offset + 8 > lcddev.width) { /* 换行 */
                x_offset = x;
                y += 16;
                if (y + 16 > lcddev.height) break;
            }
            GUI_ShowChar(x_offset, y, *str, color, bgcolor, size, mode);
            x_offset += 8;
        }
        str++;
    }
#elif (USE_LCD_CONTROLLER == 1)  /* ILI9341 */
    u8 x0 = x;
	max_width += x;
    max_height += y;
    while((*str <= '~')&&(*str >= ' '))   // 判断是不是非法字符!
    {       
        if(x >= max_width) { x = x0; y += size; }
        if(y >= max_height) break;   // 退出
        GUI_ShowChar(x, y, *str, color, bgcolor, size, mode);
        x += size/2;
        str++;
    }  
#endif /* #if (USE_LCD_CONTROLLER == 0) */
}

/* 获取字体宽度（像素） */
static u8 GUI_GetFontWidth(GUI_FontSize_t size)
{
    switch (size) {
        case ASCII_1608:  return 8;
        case ASCII_2412:  return 12;
        case ASCII_1206:  return 6;
        default:          return 8;
    }
}

/* m^n函数 返回值:m^n次方 */
static u32 GUI_Pow(u8 m, u8 n)
{
    u32 result = 1;
    while (n--) result *= m;
    return result;
}

/**
 * @brief 显示整数（支持负数）
 * @param x       起始X坐标
 * @param y       起始Y坐标
 * @param num     要显示的整数（int32_t范围）
 * @param len     数字部分的位数（不包括负号）
 * @param color   前景色
 * @param bgcolor 背景色
 * @param size    字体大小（FONT_8X16 / FONT_12X24 / FONT_6X12）
 * @param mode    显示模式（0=非叠加，1=叠加）
 */
void GUI_ShowNum(u16 x, u16 y, int32_t num,
                 u8 len, GUI_FontSize_t size,
                 u16 color, u16 bgcolor, u8 mode)
{
    u8 font_width = GUI_GetFontWidth(size);
    u8 t, temp;
    u8 enshow = 0;
    u32 abs_num;        // 绝对值
    u16 start_x = x;    // 数字显示起始X坐标

    // 处理负数
    if (num < 0) {
        GUI_ShowChar(x, y, '-', color, bgcolor, size, mode);
        start_x += font_width;          // 数字部分向右偏移一个字符宽度
        abs_num = (u32)(-num);     // 取绝对值，转为无符号
    } else abs_num = (u32)num;

    // 显示数字部分
    for (t = 0; t < len; t++) {
        temp = (abs_num / GUI_Pow(10, len - t - 1)) % 10;
        // 高位为0时不显示，除非已经是最后一位
        if (enshow == 0 && t < (len - 1)) {
            if (temp == 0) {
                GUI_ShowChar(start_x + font_width * t, y, ' ', color, bgcolor, size, mode);
                continue;
            } else {
                enshow = 1;
            }
        }
        GUI_ShowChar(start_x + font_width * t, y, (char)(temp + '0'), color, bgcolor, size, mode);
    }
}

/**
 * @brief 显示数字（支持高位补零和叠加模式）
 * @param x       起始X坐标
 * @param y       起始Y坐标
 * @param num     要显示的数值（0~4294967295）
 * @param len     显示位数（若实际位数不足，高位按 mode 补零或空格）
 * @param size    字体大小（枚举）
 * @param color   前景色
 * @param bgcolor 背景色
 * @param mode    显示模式：
 *                bit7: 1=高位补零，0=高位补空格
 *                bit0: 1=叠加模式（背景透明），0=非叠加（背景填充）
 */
void GUI_ShowxNum(u16 x, u16 y, u32 num, u8 len,
                  GUI_FontSize_t size, u16 color, u16 bgcolor,
                  u8 mode)
{
    u8 font_width = GUI_GetFontWidth(size);
    u8 t, temp;
    u8 enshow = 0;                // 是否已经开始显示非零高位
    u8 fill_zero = (mode & 0x80) ? 1 : 0;   // 高位补零标志
    u8 overlay = mode & 0x01;               // 叠加模式标志

    for (t = 0; t < len; t++) {
        // 提取当前位的数字（从左到右）
        temp = (num / GUI_Pow(10, len - t - 1)) % 10;

        // 处理高位（不足 len 位的部分）
        if (enshow == 0 && t < (len - 1)) {
            if (temp == 0) {
                // 高位为0：根据 fill_zero 决定显示 '0' 还是空格
                if (fill_zero) {
                    GUI_ShowChar(x + font_width * t, y, '0', color, bgcolor, size, overlay);
                } else {
                    GUI_ShowChar(x + font_width * t, y, ' ', color, bgcolor, size, overlay);
                }
                continue;   // 继续处理下一位
            } else {
                enshow = 1; // 遇到非零高位，之后的高位不再特殊处理
            }
        }
        // 显示当前数字字符
        GUI_ShowChar(x + font_width * t, y, (char)(temp + '0'), color, bgcolor, size, overlay);
    }
}

/**
 * @brief 绘制爱心轮廓（不填充）
 * @param cx,cy   中心坐标
 * @param size    缩放系数（1为原始大小，原始心形高度约30像素,推荐20起步）
 * @param color   颜色
 */
void GUI_DrawHeartOutline(uint16_t cx, uint16_t cy, uint16_t size, uint16_t color)
{
    float t, scale = size / 16.0f;
    int x, y, prev_x = -1, prev_y = -1;
    
    for (t = 0; t <= 2 * 3.1415926f; t += 0.02f)
    {
        float xt = 16 * sin(t) * sin(t) * sin(t);
        float yt = 13 * cos(t) - 5 * cos(2*t) - 2 * cos(3*t) - cos(4*t);
        
        x = cx + (int)(xt * scale);
        y = cy - (int)(yt * scale);   // 负号：屏幕Y轴向下
        
        if (prev_x >= 0)
            GUI_DrawLine(prev_x, prev_y, x, y, color);
        
        prev_x = x;  prev_y = y;
    }
    // 闭合到起点
    GUI_DrawLine(prev_x, prev_y, cx, cy - (int)(5 * scale), color);
}

/**
 * @brief 绘制填充爱心
 * @param x0,y0   左上角坐标
 * @param size    缩放倍数（1为16x16原始大小）
 * @param color   颜色
 */
void GUI_DrawHeartFilled(uint16_t x0, uint16_t y0, uint16_t size, uint16_t color)
{
    uint16_t i, j, m, n;
    for (i = 0; i < 16; i++) {
        for (j = 0; j < 16; j++) {
            if (heart_filled_map[i][j/8] & (0x80 >> (j%8))) {
                // 绘制一个 size x size 的方块
                for (m = 0; m < size; m++) {
                    for (n = 0; n < size; n++) {
                        GUI_WritePixel(x0 + j*size + m, y0 + i*size + n, color);
                    }
                }
            }
        }
    }
}

/**
 * @brief 绘制爱心
 * @param x,y      坐标（填充时为左上角，轮廓时为中心）
 * @param size     大小（填充：缩放倍数；轮廓：缩放倍数）
 * @param fill     1=填充，0=轮廓
 * @param color    颜色
 */
void GUI_DrawHeart(uint16_t x, uint16_t y, uint16_t size, uint16_t color, uint8_t fill)
{
    if (fill) {
        GUI_DrawHeartFilled(x, y, size, color);
    } else {
        GUI_DrawHeartOutline(x, y, size, color);
    }
}

// /* ==================== 图片 ==================== */
// void GUI_DrawImage(u8 x, u8 y, u8 w, u8 h, const u16 *img)
// {
//     if (x + w > lcddev.width || y + h > lcddev.height) return;
//     LCD_SetWindow(x, y, x + w - 1, y + h - 1);
//     LCD_WriteCmd(0x2C);
//     LCD_FillColor(0, 0); /* 占位，实际应连续发送 img 数据 */
    
//     /* 优化：直接批量发送 img 数组（RGB565，大端） */
//     LCD_DC_DATA;
//     LCD_CS_LOW;
//     for (u16 i = 0; i < w * h; i++) {
//         u8 buf[2] = { (u8)(img[i] >> 8), (u8)(img[i] & 0xFF) };
//         SPI_Transmit(SPI1, buf, 2);
//     }
//     LCD_CS_HIGH;
// }


#if (USE_LCD_CONTROLLER == 0) /* ST7735S */

/* 批量填充同一颜色（CS 保持低，减少开销） */
void LCD_FillColor(u16 color, u32 count)
{
    u8 buf[2]={ (u8)(color >> 8), (u8)(color & 0xFF) };
    LCD_DC_DATA;
    LCD_CS_LOW;
    for (u32 i=0; i < count; i++) {
        SPI_Transmit(lcd_spi_param.SPIx, buf, 2);
    }
    LCD_CS_HIGH;
}

#elif (USE_LCD_CONTROLLER == 1) /* ILI9341 */

#if 1
/* 在指定区域内填充单个颜色 (sx,sy),(ex,ey):填充矩形对角坐标,区域大小为:(ex-sx+1)*(ey-sy+1)  color:要填充的颜色 */
void GUI_Fill(u16 sx,u16 sy,u16 ex,u16 ey,u16 color)
{          
	u16 i,j;
	u16 xlen=0;
	u16 temp;
	if((lcddev.id==0X6804)&&(lcddev.dir==1)) {	//6804横屏的时候特殊处理  
		temp=sx;
		sx=sy;
		sy=lcddev.width-ex-1;	  
		ex=ey;
		ey=lcddev.width-temp-1;
 		lcddev.dir=0;	 
 		lcddev.setxcmd=0X2A;
		lcddev.setycmd=0X2B;  	 			
		GUI_Fill(sx,sy,ex,ey,color);  
 		lcddev.dir=1;	 
  		lcddev.setxcmd=0X2B;
		lcddev.setycmd=0X2A;  	 
 	} else {
        xlen=ex-sx+1;	 
        for(i=sy;i <= ey;i++) {
            LCD_SetCursor(sx,i);      		//设置光标位置 
            LCD_WRITE_CMD(lcddev.wramcmd);  //开始写入GRAM	  
            for(j=0;j<xlen;j++)
                LCD_WRITE_DATA(color);	//显示颜色 	    
        }
	}	 
}  
#else 
/**
 * @brief   填充矩形区域（支持任意坐标顺序）
 * @param   sx, sy  起始坐标
 * @param   ex, ey  结束坐标
 * @param   color   填充颜色
 */
void GUI_Fill(u16 sx, u16 sy, u16 ex, u16 ey, u16 color)
{
    u16 x_min, x_max, y_min, y_max;
    u32 total_points;
    
    // 1. 排序确保 x_min<=x_max, y_min<=y_max
    x_min = sx < ex ? sx : ex;
    x_max = sx < ex ? ex : sx;
    y_min = sy < ey ? sy : ey;
    y_max = sy < ey ? ey : sy;
    
    // 2. 如果是横屏且需要特殊处理（如某些控制器方向不同），可在此转换坐标
    // 此部分根据 lcddev 的方向标志决定是否交换 x/y
    if (lcddev.id == 0X6804 && lcddev.dir == 1) {
        // 例如：横屏时屏幕宽高交换，需调整坐标
        // 但更通用的做法是维护一个统一的坐标映射函数，这里简化处理
        // 注意：很多驱动已经通过 setxcmd/setycmd 内部处理，不需要额外转换
        // 这里假设需要交换 x/y 并修正边界
        u16 temp;
        temp = x_min; x_min = y_min; y_min = temp;
        temp = x_max; x_max = y_max; y_max = temp;
        // 注意：可能需要根据实际屏幕尺寸计算反向坐标，此处略
    }
    
    // 3. 使用窗口命令一次性设置区域
    LCD_SetWindow(x_min, y_min, x_max, y_max);
    
    // 4. 准备写入 GRAM
    LCD_WriteRAM_Prepare();  // 通常是发送 0x2C 命令
    
    // 5. 计算总点数并循环写入
    total_points = (u32)(x_max - x_min + 1) * (y_max - y_min + 1);
    while (total_points--) {
        LCD_WriteData(color);
    }
}
#endif 

/* 在指定区域内填充指定颜色块 (sx,sy),(ex,ey):填充矩形对角坐标,区域大小为:(ex-sx+1)*(ey-sy+1) color:要填充的颜色 */
void GUI_FillColor(u16 sx,u16 sy,u16 ex,u16 ey,u16 *color)
{  
    u16 height,width;
	u16 i,j;
	width=ex-sx+1; 			//得到填充的宽度
	height=ey-sy+1;			//高度
 	for(i=0;i<height;i++) {
 		LCD_SetCursor(sx,sy+i);   	//设置光标位置 
		LCD_WRITE_CMD(lcddev.wramcmd);     //开始写入GRAM
		for(j=0;j<width;j++) LCD_WRITE_DATA(color[i*width+j]); //写入数据 
	}			  
}  

/* 快速画点 x,y:坐标 color:颜色 */
void GUI_WritePixel_Fast(u16 x, u16 y, u16 color)
{	   
	if(lcddev.id==0X9341||lcddev.id==0X5310||lcddev.id==0X9488) {
		LCD_WRITE_CMD(lcddev.setxcmd); 
		LCD_WRITE_DATA(x>>8);LCD_WRITE_DATA(x&0XFF); 
		LCD_WRITE_CMD(lcddev.setycmd); 
		LCD_WRITE_DATA(y>>8);LCD_WRITE_DATA(y&0XFF); 	  	 	 
	} else if(lcddev.id==0X5510) {
		LCD_WRITE_CMD(lcddev.setxcmd);LCD_WRITE_DATA(x>>8);  
		LCD_WRITE_CMD(lcddev.setxcmd+1);LCD_WRITE_DATA(x&0XFF);	  
		LCD_WRITE_CMD(lcddev.setycmd);LCD_WRITE_DATA(y>>8);  
		LCD_WRITE_CMD(lcddev.setycmd+1);LCD_WRITE_DATA(y&0XFF); 
	} else if(lcddev.id==0X1963) {
		if(lcddev.dir==0)x=lcddev.width-1-x;
		LCD_WRITE_CMD(lcddev.setxcmd); 
		LCD_WRITE_DATA(x>>8);LCD_WRITE_DATA(x&0XFF); 		
		LCD_WRITE_DATA(x>>8);LCD_WRITE_DATA(x&0XFF); 		
		LCD_WRITE_CMD(lcddev.setycmd); 
		LCD_WRITE_DATA(y>>8);LCD_WRITE_DATA(y&0XFF); 		
		LCD_WRITE_DATA(y>>8);LCD_WRITE_DATA(y&0XFF); 		
	} else if(lcddev.id==0X6804) {		    
		if(lcddev.dir==1)x=lcddev.width-1-x;//横屏时处理
		LCD_WRITE_CMD(lcddev.setxcmd); 
		LCD_WRITE_DATA(x>>8);LCD_WRITE_DATA(x&0XFF);			 
		LCD_WRITE_CMD(lcddev.setycmd); 
		LCD_WRITE_DATA(y>>8);LCD_WRITE_DATA(y&0XFF); 		
	} else {
 		if(lcddev.dir==1)x=lcddev.width-1-x;//横屏其实就是调转x,y坐标
		LCD_WRITE_REG(lcddev.setxcmd,x);
		LCD_WRITE_REG(lcddev.setycmd,y);
	}			 
	LCD_WRITE_CMD(lcddev.wramcmd); 
	LCD_WRITE_DATA(color); 
}

			 
#endif /* #if (USE_LCD_CONTROLLER == 0) */

#endif /* #if LCD_IS_USE */
