#ifndef __LED_H_
#define __LED_H_ 

#include "sys.h"

#if LED_IS_USE /* 使用LED */

#include "gpio.h"

#define LED0_PORT   GPIOB
#define LED0_PIN   GPIO_Pin_5

#define LED1_PORT   GPIOE
#define LED1_PIN   GPIO_Pin_5

/* LED对象结构体 */
typedef enum {
    LED0 = 0x00,
    LED1 = 0x01,
} led_target_t;


/* LED结构体 */
typedef struct {
    periph_gpio_t *gpio;
    led_target_t ledTarget;
} led_t;

extern led_t Led0, Led1;

/* LED闪烁结构体 */
typedef struct {
    led_t    *led;              /* 具体LED设备 */
    uint16_t led_ontime;        /* LED亮的时长 */
    uint16_t led_offtime;       /* LED灭的时长 */
    uint8_t  led_lighttimes;    /* LED闪烁次数 */
} led_lightMsg_t;

/* 函数声明 */
void LED_onChipCfg(led_t *led);
void LED_DeInit(led_t *led);
void LED_off(led_t *led);
void LED_on(led_t *led);
void LED_toggle(led_t *led);
void LED_light(led_t *led, uint8_t lighttimes, uint16_t on_ms, uint16_t off_ms);

#endif /* #if LED_IS_USE */

#endif /* LED_H */  


