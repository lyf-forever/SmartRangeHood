#ifndef __BUZZER_H_
#define __BUZZER_H_

#include "sys.h"

#if BUZZER_IS_USE /* 使用蜂鸣器 */

#include "gpio.h"

#define BUZZER_PORT GPIOB
#define BUZZER_PIN  GPIO_Pin_8
#define BUZZER_ACTIVE_LEVEL  BUZZER_ACTIVE_HIGH

// 蜂鸣器有效电平
typedef enum {
    BUZZER_ACTIVE_HIGH = 0,   // 高电平有效（输出高电平响）
    BUZZER_ACTIVE_LOW         // 低电平有效（输出低电平响）
} BuzzerActiveLevel;

// 蜂鸣器状态
typedef enum {
    BUZZER_OFF = 0,
    BUZZER_ON
} BuzzerState;

// 错误码
typedef enum {
    BUZZER_OK                 =  0,
    BUZZER_ERR_NULL_PTR       = -1,
    BUZZER_ERR_INVALID_PARAM  = -2,
    BUZZER_ERR_NOT_INIT       = -3,
    BUZZER_ERR_RATIONAL_STATE = -4,
} BuzzerError;

// 蜂鸣器对象结构体
typedef struct Buzzer_t {
    const periph_gpio_t    *gpio;       // GPIO
    const BuzzerActiveLevel active;     // 有效电平
    BuzzerState             state;      // 当前状态
    uint8_t                 isInit;     // 初始化标志

    /* 操作函数指针（便于扩展或回调）*/
    BuzzerError (*const onChipCfg)(struct Buzzer_t*);
    BuzzerError (*const on)(struct Buzzer_t*);
    BuzzerError (*const off)(struct Buzzer_t*);
    BuzzerError (*const toggle)(struct Buzzer_t*);
    BuzzerError (*const beep)(struct Buzzer_t*, uint8_t times, uint16_t ms);
    BuzzerError (*const beep_ex)(struct Buzzer_t*, uint8_t times, uint16_t ms, uint16_t interval_ms);
} Buzzer;

/* 全局变量，对外接口/变量 */
extern Buzzer buzzer;

// // 初始化蜂鸣器（需传入配置）
// BuzzerError buzzer_onChipCfg(BuzzerIOCfg *cfg);

// // 关闭蜂鸣器
// BuzzerError buzzer_off(BuzzerIOCfg *cfg);

// // 打开蜂鸣器
// BuzzerError buzzer_on(BuzzerIOCfg *cfg);

// // 翻转蜂鸣器状态
// BuzzerError buzzer_toggle(BuzzerIOCfg *cfg);

// // 获取当前状态
// BuzzerState buzzer_getState(BuzzerIOCfg *cfg);

// // 鸣叫指定次数且相同时间间隔
// BuzzerError buzzer_beep(BuzzerIOCfg *cfg, uint8_t beep_times, uint16_t ms);

// // 鸣叫指定次数且不同时间间隔
// BuzzerError buzzer_beep_ex(BuzzerIOCfg *cfg, uint8_t beep_times, uint16_t ms, uint16_t interval_ms);

#endif /* #if BUZZER_IS_USE */

#endif /* #ifndef __BUZZER_H_ */

