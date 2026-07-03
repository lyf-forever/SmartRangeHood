#include "key_adp.h"

#if KEY_IS_USE  /* 有使用到按键 */

#include "gpio.h"
#include "delay.h"
#include "buzzer.h"
 
/*============================================================================*
 * key_ops_t 操作实现
 *============================================================================*/
static KeyError key_ops_init(KeyDev *dev)
{
    key_drv_init(dev->iocfg->gpio);
    return KEY_OK;
}

static KeyError key_ops_deinit(KeyDev *dev)
{
    Key_drv_deinit_all(dev->iocfg->gpio);
    return KEY_OK;
}

static KeyError key_ops_read(KeyDev *dev, KeyInfo *info)
{
    uint8_t readLev = Key_drv_read(dev->iocfg->gpio);
    
    info->name = dev->ops->name;
    info->id = dev->id;
    info->isPressed = (readLev == dev->iocfg->key_io_pressLevel) ? true : false;
    
    return KEY_OK;
}

static KeyError key_ops_sleep(KeyDev *dev)
{
    Key_drv_sleep(dev->iocfg);
    return KEY_OK;
}

static KeyError key_ops_wkup(KeyDev *dev)
{
    Key_drv_wakeup(dev->iocfg);
    return KEY_OK;
}

static KeyError key_ops_selftest(KeyDev *dev)
{
    if(Key_drv_selftest(dev->iocfg))
        return KEY_ERR_SELFTEST;
    return KEY_OK;
}

/* 事件名称查找表（ROM常量，零开销） */
static const char *key_event_name(KeyEvent event)
{
    switch(event) {
        case KEY_EVENT_NONE:    return "NONE";    
        case KEY_EVENT_PRESS:   return "PRESS";   
        case KEY_EVENT_RELEASE: return "RELEASE"; 
        case KEY_EVENT_CLICK:   return "CLICK";   
        case KEY_EVENT_DOUBLE:  return "DOUBLE";  
        case KEY_EVENT_LONG:    return "LONG";    
        case KEY_EVENT_REPEAT:  return "REPEAT";  
        default:                return "UNKNOWN"; 
    }
}

/* 统一日志输出 */
void key_log_event(const KeyInfo *info) 
{
    LOG_D("%s[id=%d]  event=%s, duration=%ums, click=%utimes, repeat=%utimes",
          info->name,
          info->id,
          key_event_name(info->event),
          info->press_duration,
          info->click_count,
          info->repeat_count);
}

/*============================================================================*
 * 分按键处理（需要不同按键不同行为时） 此函数实现我放在app_task.c内，由于多任务对于按键的需求
 *============================================================================*/
__weak void key_eventCallback(const KeyInfo *info, void *user_data) 
{
    if(info == NULL) return;

    /* 先打日志（调试必备） */
    //key_log_event(info);

    /* 利用 user_data 传递上下文（如界面句柄、状态机指针） */
    KeyAppCtx *ctx = (KeyAppCtx *)user_data;
    const led_lightMsg_t *ctx_led = ctx->LED_light;

    switch(info->id) {
        case KEY_DRV_0:  /* userKey0 */
            switch(info->event) {
                case KEY_EVENT_CLICK:
                    
                    buzzer.beep(&buzzer, 1, 100);   /* 短按提示音 */
                    break;
                case KEY_EVENT_LONG:
                    
                    break;
                case KEY_EVENT_DOUBLE:
                    
                    break;
                case KEY_EVENT_REPEAT:
                    buzzer.on(&buzzer);    /* 蜂鸣器响 */
                    break;
                case KEY_EVENT_RELEASE:
                    buzzer.off(&buzzer);   /* 蜂鸣器关 */
                    break;
                default:
                    break;
            }
            break;

        case KEY_DRV_1:  /* userKey1 */
            switch(info->event) {
                case KEY_EVENT_CLICK:
                       
                    buzzer.beep(&buzzer, 1, 100);  /* 短按提示音 */
                    break;
                case KEY_EVENT_LONG:
                    System_ToggleMotor();          /* 长按时关闭或者打开电机，无论电机在何种模式下 */
                    break;
                case KEY_EVENT_DOUBLE:
                    
                    break;
                case KEY_EVENT_REPEAT:
                    buzzer.on(&buzzer);    /* 蜂鸣器响 */
                    break;
                case KEY_EVENT_RELEASE:
                    buzzer.off(&buzzer);   /* 蜂鸣器关 */
                    break;
                default:
                    break;
            }
            break;

        default:
            break;
    }
}

/*============================================================================*
 * 静态ops表
 *============================================================================*/
#ifdef USER_KEY0_PORT  /* 用户按键0 */
static const KeyOps userKey0_ops = {
    .name      = "userKey0",
    .id        = KEY_DRV_0,
    .type      = KEY_TYPE_NORMAL,
    .init      = key_ops_init,
    .deinit    = key_ops_deinit,
    .read      = key_ops_read,
    .self_test = key_ops_selftest,
    .sleep     = key_ops_sleep,
    .wakeup    = key_ops_wkup, 
};

static const led_lightMsg_t userKey0_ledlight = { .led = &Led0, .led_lighttimes = 6, .led_ontime = 500, .led_offtime = 500 };

static KeyAppCtx userKey0_appctx = { .LED_light = &userKey0_ledlight, };

static KeyPolicy userKey0_policy = { .debounce_ms = USER_KEY0_DEBOUNCE_TIME, .double_click_ms = USER_KEY0_DOUBLECLICK_TIME,
                                        .long_press_ms = USER_KEY0_LONG_PRESS_TIME, .repeat_interval_ms = USER_KEY0_REPEAT_INTERVAL, 
                                            .repeat_start_count = USER_KEY0_REPEAT_SCNT, .max_retry = USER_KEY0_MAX_RETRY,
                                                .max_err_cnt = USER_KEY0_MAX_ERR };

static KeyDev userKey0_dev = { .ops = &userKey0_ops, .id = KEY_DRV_0, .type = KEY_TYPE_NORMAL, .status = KEY_STATUS_UNINIT,
                                .policy = &userKey0_policy, .iocfg = &userKey0_cfg, .last_read_ms = 0, .err_count = 0, 
                                .callback = key_eventCallback, .user_data = &userKey0_appctx, .state = KEY_STATE_IDLE,
                                .press_start_ms = 0, .press_duration = 0, .isEnabled = true, .click_count = 0, .repeat_count = 0 };
      
#endif /* #ifdef USER_KEY0_PORT */                              

#ifdef USER_KEY1_PORT  /* 用户按键1 */
static const KeyOps userKey1_ops = {
    .name      = "userKey1",
    .id        = KEY_DRV_1,
    .type      = KEY_TYPE_NORMAL,
    .init      = key_ops_init,
    .deinit    = key_ops_deinit,
    .read      = key_ops_read,
    .self_test = key_ops_selftest,
    .sleep     = key_ops_sleep,
    .wakeup    = key_ops_wkup, 
};

static const led_lightMsg_t userKey1_ledlight = { .led = &Led1, .led_lighttimes = 6, .led_ontime = 500, .led_offtime = 500 };

static KeyAppCtx userKey1_appctx = { .LED_light = &userKey1_ledlight, };

static KeyPolicy userKey1_policy = { .debounce_ms = USER_KEY1_DEBOUNCE_TIME, .double_click_ms = USER_KEY1_DOUBLECLICK_TIME,
                                        .long_press_ms = USER_KEY1_LONG_PRESS_TIME, .repeat_interval_ms = USER_KEY1_REPEAT_INTERVAL, 
                                            .repeat_start_count = USER_KEY1_REPEAT_SCNT, .max_retry = USER_KEY1_MAX_RETRY,
                                                .max_err_cnt = USER_KEY1_MAX_ERR };

static KeyDev userKey1_dev = { .ops = &userKey1_ops, .id = KEY_DRV_1, .type = KEY_TYPE_NORMAL, .status = KEY_STATUS_UNINIT,
                                .policy = &userKey1_policy, .iocfg = &userKey1_cfg, .last_read_ms = 0, .err_count = 0, 
                                .callback = key_eventCallback, .user_data = &userKey1_appctx, .state = KEY_STATE_IDLE,
                                .press_start_ms = 0, .press_duration = 0, .isEnabled = true, .click_count = 0, .repeat_count = 0 };
      
#endif /* #ifdef USER_KEY1_PORT */

#ifdef WKUP_KEY_PORT  /* 唤醒按键 */
static const KeyOps wkupKey_ops = {
    .name      = "wkupKey",
    .id        = KEY_DRV_2,
    .type      = KEY_TYPE_NORMAL,
    .init      = key_ops_init,
    .deinit    = key_ops_deinit,
    .read      = key_ops_read,
    .self_test = key_ops_selftest,
    .sleep     = key_ops_sleep,
    .wakeup    = key_ops_wkup, 
};

static KeyPolicy wkupKey_policy = { .max_retry = WKUP_KEY_MAX_RETRY, .max_err_cnt = WKUP_KEY_MAX_ERR };

static KeyDev wkupKey_dev = { .ops = &wkupKey_ops, .id = KEY_DRV_2, .type = KEY_TYPE_NORMAL, .status = KEY_STATUS_UNINIT,
                                .policy = &wkupKey_policy, .iocfg = &wkupKey_cfg, .last_read_ms = 0, .err_count = 0, 
                                .callback = key_eventCallback, .state = KEY_STATE_IDLE, .isEnabled = true };
      
#endif /* #ifdef WKUP_KEY_PORT */

void key_adp_register(void)
{
#if USER_KEY_USE /* 使用用户按键 */
    #ifdef USER_KEY0_PORT /* 用户按键0 */
        key_register(&userKey0_dev);
    #endif /* #ifdef USER_KEY0_PORT */

    #ifdef USER_KEY1_PORT /* 用户按键1 */
        key_register(&userKey1_dev);

    #endif /* #ifdef USER_KEY1_PORT */
#endif /* #if USER_KEY_USE */
}

void key_adp_init(void)
{
#if USER_KEY_USE /* 使用用户按键 */
    #ifdef USER_KEY0_PORT /* 用户按键0 */
        key_init(&userKey0_dev);
    #endif /* #ifdef USER_KEY0_PORT */

    #ifdef USER_KEY1_PORT /* 用户按键1 */
        key_init(&userKey1_dev); 
    #endif /* #ifdef USER_KEY1_PORT */

    #ifdef WKUP_KEY_PORT /* 唤醒按键 */
        key_init(&wkupKey_dev); 
    #endif /* #ifdef USER_KEY1_PORT */
#endif /* #if USER_KEY_USE */
}

#endif /* #if KEY_IS_USE */


