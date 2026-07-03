/*
 * LED(LED0 LED1)驱动源文件
 * 引脚: LED0 - PE4  LED1 - PE3
 * LED:高电平熄灭 低电平点亮
 *
 * 作者：不甘心的咸鱼--闲鱼/不搭(414192836)--小红书
 * 闲鱼号：tb43915564
 * 修改日期：2026/2/1
 * 项目已申请版权，请勿倒卖！
 */
#include "led.h"

#if LED_IS_USE /* 使用LED */

#include "delay.h"
#include <stdio.h>

periph_gpio_t   led0_gpio = {
    .port  = LED0_PORT,
    .pin   = LED0_PIN,
    .mode  = GPIO_Mode_Out_PP,
    .speed = GPIO_Speed_50MHz,
    .initial_level = highLevel
},              led1_gpio = {
    .port  = LED1_PORT,
    .pin   = LED1_PIN,
    .mode  = GPIO_Mode_Out_PP,
    .speed = GPIO_Speed_50MHz,
    .initial_level = highLevel
};

led_t Led0 = {
    .gpio = &led0_gpio,
    .ledTarget = LED0
},
      Led1 = {
    .gpio = &led1_gpio,
    .ledTarget = LED1
};

/**
* @brief: 初始化LED的io口
* @param: LED结构体变量
* @param: 端口（GPIOA-G）
* @param: 管脚（0-16）
*/
void LED_onChipCfg(led_t *led)
{
  	io_set(led->gpio);
}

/**
* @brief: 去初始化LED的io口
* @param: LED结构体变量
* @param: 端口（GPIOA-G）
* @param: 管脚（0-16）
*/
void LED_DeInit(led_t *led)
{
  	io_deset(led->gpio);
}

/** 
* @brief:  关闭LED
* @param:  LED结构体变量
* @return: none
**/ 
void LED_off(led_t *led)
{
    io_set_bit(led->gpio);
}

/** 
* @brief:  打开LED
* @param:  LED结构体变量
* @return: none
**/ 
void LED_on(led_t *led)
{
	io_reset_bit(led->gpio);
}

/** 
* @brief:  翻转LED
* @param:  LED结构体变量
* @return: none
**/ 
void LED_toggle(led_t *led)
{
    io_toggle_bit(led->gpio);
}

/**
 * @brief  使LED闪烁指定次数
 * @param  led      LED控制结构体（传值或传址均可，此处传值）
 * @param  lighttimes 闪烁次数（亮灭一次算一次，0则不操作）
 * @return none
 * @note   每次亮灭各持续 200ms，可修改宏或参数调整
 */
void LED_light(led_t *led, uint8_t lighttimes, uint16_t on_ms, uint16_t off_ms)
{
    if (lighttimes == 0) return;
    for (uint8_t i = 0; i < lighttimes; i++) {
        LED_on(led);
        delay_ms(on_ms);
        LED_off(led);
        if (i < lighttimes - 1) delay_ms(off_ms);
    }
}

#endif /* #if LED_IS_USE */