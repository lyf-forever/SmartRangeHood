#include "buzzer.h"
#include "delay.h"

#if BUZZER_IS_USE /* 使用蜂鸣器 */

// 根据开关指示设置IO电平
static void set_buzzerLevel(Buzzer *buzzer, bool on)
{
    io_write_bit(buzzer->gpio, (on ^ buzzer->active) ? 1 : 0);
}

// 内部：获取当前实际电平（高=1，低=0）
static uint8_t get_currentLevel(Buzzer *buzzer)
{
    return io_getOpLevel(buzzer->gpio);
}

// 初始化
static BuzzerError buzzer_onChipCfg(Buzzer *buzzer)
{
    if (!buzzer || !buzzer->gpio) return BUZZER_ERR_NULL_PTR;
    if (buzzer->state) return BUZZER_ERR_RATIONAL_STATE;
    io_set(buzzer->gpio);

    // 初始状态关闭
    set_buzzerLevel(buzzer, false);
    buzzer->state = BUZZER_OFF;
    buzzer->isInit = 1;

    return BUZZER_OK;
}

// 打开
static BuzzerError buzzer_on(Buzzer *buzzer)
{
    if (!buzzer->isInit) return BUZZER_ERR_NOT_INIT;

    set_buzzerLevel(buzzer, ON);
    buzzer->state = BUZZER_ON;

    return BUZZER_OK;
}

// 关闭
static BuzzerError buzzer_off(Buzzer *buzzer)
{
    if (!buzzer->isInit) return BUZZER_ERR_NOT_INIT;

    set_buzzerLevel(buzzer, OFF);
    buzzer->state = BUZZER_OFF;

    return BUZZER_OK;
}

// 翻转
static BuzzerError buzzer_toggle(Buzzer *buzzer)
{
    if (!buzzer->isInit) return BUZZER_ERR_NOT_INIT;

    uint8_t level = get_currentLevel(buzzer);
    // 根据有效电平判断当前逻辑状态
    bool current_on;
    if (buzzer->active == BUZZER_ACTIVE_HIGH)
        current_on = (level == 1);
    else
        current_on = (level == 0);
    // 翻转
    set_buzzerLevel(buzzer, !current_on);
    buzzer->state = (buzzer->state == BUZZER_ON) ? BUZZER_OFF : BUZZER_ON;

    return BUZZER_OK;
}

// 获取状态
static BuzzerState buzzer_getState(Buzzer *buzzer)
{
    return buzzer->isInit ? buzzer->state : BUZZER_OFF;
}

/**
 * @brief 蜂鸣器鸣叫指定次数，每次持续指定毫秒
 * @param buzzer        蜂鸣器配置结构体指针
 * @param beep_times 鸣叫次数（0表示不操作）
 * @param ms         每次鸣叫的持续时间（毫秒）
 * @return BuzzerError 执行结果
 * @note 每次鸣叫之间间隔与鸣叫时长相同（即响 ms，停 ms）
 */
static BuzzerError buzzer_beep(Buzzer *buzzer, uint8_t beep_times, uint16_t ms)
{
    if (!buzzer->isInit) return BUZZER_ERR_NOT_INIT;
    if (beep_times == 0 || ms == 0) return BUZZER_OK;

    for (uint8_t i = 0; i < beep_times; i++) {
        buzzer_on(buzzer);              // 打开蜂鸣器
        delay_ms(ms);                // 持续鸣叫
        buzzer_off(buzzer);             // 关闭蜂鸣器
        if (i < beep_times - 1) {
            delay_ms(ms);            // 间隔时间与鸣叫时长相同
        }
    }

    return BUZZER_OK;
}

static BuzzerError buzzer_beep_ex(Buzzer *buzzer, uint8_t beep_times, uint16_t ms, uint16_t interval_ms)
{
    if (!buzzer->isInit) return BUZZER_ERR_NOT_INIT;
    if (beep_times == 0 || ms == 0) return BUZZER_OK;

    for (uint8_t i = 0; i < beep_times; i++) {
        buzzer_on(buzzer);
        delay_ms(ms);
        buzzer_off(buzzer);
        if (i < beep_times - 1) {
            delay_ms(interval_ms);
        }
    }
    return BUZZER_OK;
}

static const periph_gpio_t buzzer_gpio = { .port = BUZZER_PORT, .pin = BUZZER_PIN, .initial_level = lowLevel,
                                            .mode = GPIO_Mode_Out_PP, .speed = GPIO_Speed_50MHz };

Buzzer buzzer = { 
    .gpio    = &buzzer_gpio,
    .active  = BUZZER_ACTIVE_LEVEL,
    .state   = BUZZER_OFF,
    .isInit  = 0,
    
    .onChipCfg = buzzer_onChipCfg,
    .on = buzzer_on,
    .off = buzzer_off,
    .toggle = buzzer_toggle,
    .beep = buzzer_beep, 
    .beep_ex = buzzer_beep_ex, 
};

#endif /* #if BUZZER_IS_USE */

