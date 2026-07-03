#ifndef __GUI_H
#define __GUI_H

#include "lcd.h"

#if LCD_IS_USE  /* 使用LCD */

/* 字体大小枚举 */
typedef enum {
    ASCII_1206 = 12,
    ASCII_1608 = 16,
    ASCII_2412 = 24,
} GUI_FontSize_t;

#define IS_FONT(FONT)   (((FONT) == ASCII_1206) || ((FONT) == ASCII_1608) || ((FONT) == ASCII_2412))

#if (USE_LCD_CONTROLLER == 0) /* ST7735S */
void LCD_FillColor(u16 color, u32 count);

#elif (USE_LCD_CONTROLLER == 1) /* ILI9341 */
void GUI_Fill(u16 sx,u16 sy,u16 ex,u16 ey,u16 color);  // 填充同一颜色
void GUI_FillColor(u16 sx,u16 sy,u16 ex,u16 ey,u16 *color); // 填充颜色库
void GUI_WritePixel_Fast(u16 x,u16 y,u16 color);
u16  GUI_ReadPoint(u16 x,u16 y);

#endif /* #if (USE_LCD_CONTROLLER == 0) */

/* 通用API */
void GUI_WritePixel(u16 x, u16 y, u16 color);
u16 GUI_GetPixel(u16 x, u16 y);
void GUI_DrawLine(u16 x0, u16 y0, u16 x1, u16 y1, u16 color);
void GUI_DrawRect(u16 usX_Start,u16 usY_Start,u16 usWidth,
                        u16 usHeight,u16 usColor,u8 isFill);
void GUI_FillRect(u16 x0, u16 y0, u16 x1, u16 y1, u16 color);
void GUI_DrawCircle(u16 x0, u16 y0, u16 r, u16 color, u8 isFill);
void GUI_FillCircle(u16 x0, u16 y0, u16 r, u16 color); 
void GUI_DrawhLine(u16 x0,u16 y0,u16 len,u16 color);
void GUI_DrawHeartOutline(uint16_t cx, uint16_t cy, uint16_t size, uint16_t color);
void GUI_DrawHeartFilled(uint16_t x0, uint16_t y0, uint16_t size, uint16_t color);
void GUI_DrawHeart(uint16_t x, uint16_t y, uint16_t size, uint16_t color, uint8_t fill);
void GUI_ShowChar(u16 x, u16 y, const unsigned char chr, u16 color, u16 bgcolor, GUI_FontSize_t size, u8 mode);
void GUI_ShowString(u16 x, u16 y, const char *str, u16 color, u16 bgcolor, GUI_FontSize_t size, 
                     u8 mode, u16 max_width, u16 max_height);
void GUI_ShowNum(u16 x, u16 y, int32_t num, u8 len, GUI_FontSize_t size,
                    u16 color, u16 bgcolor, u8 mode);
void GUI_ShowxNum(u16 x, u16 y, u32 num, u8 len,
                  GUI_FontSize_t size, u16 color, u16 bgcolor, u8 mode);				
		
/* 图片：img 为 RGB565 数组，w/h 为宽高 */
//void GUI_DrawImage(u8 x, u8 y, u8 w, u8 h, const u16 *img);

#endif /* #if LCD_IS_USE */

#endif /* __GUI_H */

