#ifndef __KEY_H_
#define __KEY_H_	 

#include "sys.h"
#include "led.h"
 
#if KEY_IS_USE

/* 按键涉及IO信息 */
typedef struct {
    const periph_gpio_t *gpio;
    uint8_t key_io_exti_en;
    uint8_t key_io_pressLevel;
    uint8_t key_io_releaseLevel;
} keyIOCfg;

/* 按键事件反馈 APP */
typedef struct {
    const led_lightMsg_t  *LED_light;    /* LED */
    // buzzer_beepMsg_t 

} KeyAppCtx;

    #if USER_KEY_USE
        #define USER_KEY0_PORT   GPIOE
        #define USER_KEY0_PIN    GPIO_Pin_4
        #define USER_KEY0_EXTI_EN          0      /* 不使用EXTI中断 */
        #define USER_KEY0_PRESSED_LEVEL    0      /* 按下为低电平 */
        #define USER_KEY0_RELEASED_LEVEL   1      /* 释放为高电平 */
        #define USER_KEY0_DEBOUNCE_TIME    20     /* 消抖时间：20ms */
        #define USER_KEY0_DOUBLECLICK_TIME 300    /* 双击间隔时间：300ms */
        #define USER_KEY0_LONG_PRESS_TIME  600    /* 长按时间：600ms */
        #define USER_KEY0_REPEAT_INTERVAL  500    /* 连发间隔时间：500ms */
        #define USER_KEY0_REPEAT_SCNT      8      /* 连发启动次数：8 */
        #define USER_KEY0_MAX_RETRY        3      /* 读取重试次数：3 */
        #define USER_KEY0_MAX_ERR          10     /* 最大错误次数：3 */
        extern const periph_gpio_t userKey0;
        extern const keyIOCfg userKey0_cfg;
        
        #define USER_KEY1_PORT   GPIOE
        #define USER_KEY1_PIN    GPIO_Pin_3
        #define USER_KEY1_EXTI_EN          0      /* 不使用EXTI中断 */
        #define USER_KEY1_PRESSED_LEVEL    0      /* 按下为低电平 */
        #define USER_KEY1_RELEASED_LEVEL   1      /* 释放为高电平 */
        #define USER_KEY1_DEBOUNCE_TIME    20     /* 消抖时间：20ms */
        #define USER_KEY1_DOUBLECLICK_TIME 300    /* 双击间隔时间：300ms */
        #define USER_KEY1_LONG_PRESS_TIME  500    /* 长按时间：500ms */
        #define USER_KEY1_REPEAT_INTERVAL  600    /* 连发间隔时间：600ms */
        #define USER_KEY1_REPEAT_SCNT      8      /* 连发启动次数：8 */
        #define USER_KEY1_MAX_RETRY        3      /* 读取重试次数：3 */
        #define USER_KEY1_MAX_ERR          10     /* 最大错误次数：3 */
        extern const periph_gpio_t userKey1;
        extern const keyIOCfg userKey1_cfg;
        
    #endif /* #if USER_KEY_USE */

    #if WKUP_KEY_USE 
        #define WKUP_KEY_PORT   GPIOA
        #define WKUP_KEY_PIN    GPIO_Pin_0
        #define WKUP_KEY_EXTI_EN           0      /* 不使用EXTI中断 */
        #define WKUP_KEY_PRESSED_LEVEL     1      /* 按下为高电平 */
        #define WKUP_KEY_RELEASED_LEVEL    0      /* 释放为低电平 */
        #define WKUP_KEY_MAX_RETRY         3      /* 读取重试次数：3 */
        #define WKUP_KEY_MAX_ERR           10     /* 最大错误次数：3 */
        extern const periph_gpio_t wkupKey;
        extern const keyIOCfg wkupKey_cfg;

    #endif /* #if WKUP_KEY_USE */

void key_onChipCfg(void);

void         key_drv_init(const periph_gpio_t *key_io);
void       Key_drv_deinit(const periph_gpio_t *key_io);
void   Key_drv_deinit_all(const periph_gpio_t *key_io);
uint8_t      Key_drv_read(const periph_gpio_t *key_io);
void          Key_drv_sleep(const keyIOCfg *key_iocfg);
void         Key_drv_wakeup(const keyIOCfg *key_iocfg);
uint8_t    Key_drv_selftest(const keyIOCfg *key_iocfg);

#endif /* #if KEY_IS_USE */
					    
#endif /* #ifndef __KEY_H_ */

