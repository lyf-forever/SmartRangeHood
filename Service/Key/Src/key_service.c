#include "key_service.h"

#if KEY_IS_USE

#include <string.h>
#include <stddef.h>

/* 全局设备列表 */
static KeyDev* g_keys[MAX_KEYS];
static uint8_t g_key_count = 0;

/* 默认策略实例（当设备 policy 为 NULL 时使用） */
static  KeyPolicy g_default_policy = {
    .debounce_ms = 20,
    .long_press_ms = 1000,
    .double_click_ms = 300,
    .repeat_interval_ms = 300,
    .repeat_start_count = 3,
    .max_retry   = 3,
    .max_err_cnt = 10
};

static void key_dispatch_event(KeyDev *dev, KeyEvent event)
{
    if(dev->callback == NULL)
        return;

    KeyInfo info = {
        .name           = dev->ops->name,
        .id             = dev->id,
        .event          = event,
        .press_duration = 0,
        .click_count    = dev->click_count,
        .repeat_count   = dev->repeat_count,
    };

    if(event == KEY_EVENT_RELEASE || event == KEY_EVENT_CLICK ||
       event == KEY_EVENT_DOUBLE || event == KEY_EVENT_LONG ||
       event == KEY_EVENT_REPEAT) {
        info.press_duration = get_tick_ms() - dev->press_start_ms;
    }

    dev->callback(&info, dev->user_data);
}

static KeyPolicy *get_effective_policy(KeyDev *dev)
{
    return dev->policy ? dev->policy : &g_default_policy;
}

/* 内部辅助函数：判断是否可以使用缓存数据 */
static bool is_cache_valid(KeyDev *dev, uint32_t now_ms)
{
    KeyPolicy *policy = get_effective_policy(dev);
    
    if (dev->last_read_ms == 0) {
        return false;  /* 从未读取过 */
    }
    return (now_ms - dev->last_read_ms) < policy->double_click_ms;
}

KeyError key_register(KeyDev *dev)
{
    if (dev == NULL || dev->ops == NULL) 
        return KEY_ERR_NULL_PTR;
    if (g_key_count >= MAX_KEYS) 
        return KEY_ERR_INIT_FAIL;
    
    /* 检查是否已经注册过（避免重复注册） */
    for (uint8_t i = 0; i < g_key_count; i++) {
        if (g_keys[i] == dev) {
            //LOG_D("This key has been registered");
            return KEY_OK;  /* 已存在，视为成功 */
        }
    }
    g_keys[g_key_count++] = dev;
    dev -> status = KEY_STATUS_UNINIT;
    
    return KEY_OK;
}

KeyError key_logout(KeyDev *dev)
{
    if (dev == NULL) return KEY_ERR_NULL_PTR;

    for (uint8_t i = 0; i < g_key_count; i++) {
        if (g_keys[i] == dev) {
            g_keys[i] = NULL;   // 从表中移除
            //LOG_D("This key logouted successfully");
            g_key_count--;
            return KEY_OK;
        }
    }
    
    //LOG_D("This key is not exist");
    return KEY_OK;
}

KeyDev *key_getDev_byID(KeyID drv_id)
{
    if(drv_id >= KEY_DRV_MAX || drv_id >= g_key_count)
        return NULL;
    return g_keys[drv_id];
}

KeyDev *key_find_by_type(KeyType type)
{
    for (uint8_t i = 0; i < g_key_count; i++) {
        if (g_keys[i]->type == type) {
            return g_keys[i];
        }
    }
    return NULL;
}

KeyDev *key_find_by_id(KeyID id)
{
    for (uint8_t i = 0; i < g_key_count; i++) {
        if (g_keys[i]->id == id) {
            return g_keys[i];
        }
    }
    return NULL;
}

KeyError key_init(KeyDev *dev)
{
    if (!dev || !dev->ops || !dev->ops->init) 
        return KEY_ERR_NULL_PTR;
    
    /* 调用驱动的初始化函数 */
    KeyError ret = dev->ops->init(dev);
    if (ret != KEY_OK) {
        dev->status = KEY_STATUS_ERROR;
        dev->err_count++;
        //LOG_E("Key [%s] init failed:%d", dev->ops->name, ret);
        return KEY_ERR_INIT_FAIL;
    } 

    dev->status = KEY_STATUS_ONLINE;
    dev->err_count = 0;
    dev->last_read_ms = 0;
    //LOG_I("Key [%s] init ok", dev->ops->name);
    memset(&dev->cache, 0, sizeof(KeyInfo));
    
    return KEY_OK;
}

KeyError key_read(KeyDev *dev, KeyInfo *out)
{
    if (!dev || !dev->ops || !dev->ops->read || !out) 
        return KEY_ERR_NULL_PTR;

    if(dev->status == KEY_STATUS_OFFLINE) {
        //LOG_E("Key [%s] read failed,due to the key offline", dev->ops->name);
        return KEY_ERR_NOT_READY;
    }

    if(dev->status != KEY_STATUS_ONLINE) {
        //LOG_E("Key [%s] is unready along with read failure", dev->ops->name);
        return KEY_ERR_NOT_READY;
    }
    
    uint32_t now_ms = get_tick_ms();
    KeyPolicy *policy = get_effective_policy(dev);

    /* 执行带重试的读取 */
    uint8_t retry = 0;
    uint8_t max_attempt = policy->max_retry + 1;
    KeyError ret = KEY_ERR_READ_FAIL;

    do {
        ret = dev->ops->read(dev, out);
        if (ret == KEY_OK) break;
        retry++;
    } while (retry < max_attempt);

    if(ret != KEY_OK) {
        dev->err_count++;
        if(dev->err_count >= dev->policy->max_err_cnt) {
            dev->status = KEY_STATUS_OFFLINE;
            //LOG_E("Key [%s] offlin", dev->ops->name);
        }
        return ret;
    }

    /* 读取成功，更新缓存和状态 */
    dev->cache = *out;
    dev->last_read_ms = now_ms;
    dev->err_count = 0;

    if (dev->status != KEY_STATUS_ONLINE) {
        dev->status = KEY_STATUS_ONLINE;
    }
    
    return KEY_OK;
}

/*============================================================================*
 * 按键扫描与状态机处理 (核心)
 *============================================================================*/
KeyError key_scan(KeyDev *dev)
{
    static u8 isRepeat, isDouble = 0;

    if(!dev || !dev->ops || !dev->ops->read)
        return KEY_ERR_NULL_PTR;

    if(dev->status != KEY_STATUS_ONLINE || !dev->isEnabled)
        return KEY_ERR_NOT_READY;

    KeyInfo data;
    KeyError ret = key_read(dev, &data);    
    if(ret != KEY_OK)
        return ret;

    uint32_t now = get_tick_ms();
    KeyPolicy *policy = get_effective_policy(dev);
    bool hw_pressed = data.isPressed;
    
    switch(dev->state) {
        case KEY_STATE_IDLE:
            if(hw_pressed) {
                dev->state = KEY_STATE_DEBOUNCE;
                dev->press_start_ms = now;
                //LOG_I("key [%s] debounce start", dev->ops->name);
                dev->click_count = 0;    // 确保初始为0
            }
            break;

        case KEY_STATE_DEBOUNCE:
            if(!hw_pressed) {
                dev->state = KEY_STATE_IDLE;
                dev->click_count = 0;
                //LOG_I("key [%s] debounce cancel", dev->ops->name);
            } else if(now - dev->press_start_ms >= policy->debounce_ms) {
                /* 消抖完成，判断是单击还是双击 */
                if(dev->click_count == 2) {
                    // 双击确认
                    dev->state = KEY_STATE_PRESSED;
                    dev->click_count = 0;   // 清除计数，防止释放时触发单击
                    key_dispatch_event(dev, KEY_EVENT_DOUBLE);
                    isDouble = 1;
                } else {
                    // 普通单击
                    dev->state = KEY_STATE_PRESSED;
                    dev->click_count = 1;
                    //key_dispatch_event(dev, KEY_EVENT_PRESS);
                }
                //LOG_I("key [%s] pressed", dev->ops->name);
            }
            break;

        case KEY_STATE_PRESSED:
            if(!hw_pressed) {
                dev->state = KEY_STATE_RELEASED;
                //key_dispatch_event(dev, KEY_EVENT_RELEASE);
                // if(dev->click_count == 1) {
                //     key_dispatch_event(dev, KEY_EVENT_CLICK);
                // }
                // 若 click_count == 0，说明是双击后的释放，不触发单击
                //LOG_I("key [%s] click", dev->ops->name);
            } else if(now - dev->press_start_ms >= policy->long_press_ms) {
                dev->state = KEY_STATE_LONG_PRESS;
                //key_dispatch_event(dev, KEY_EVENT_LONG);
                dev->repeat_count = 1;
                //LOG_I("key [%s] long press", dev->ops->name);
            }
            break;

        case KEY_STATE_LONG_PRESS:
            if(!hw_pressed) {
                dev->state = KEY_STATE_IDLE;
                if(isRepeat == 1) {
                    isRepeat = 0;
                    key_dispatch_event(dev, KEY_EVENT_RELEASE);
                    break;
                }
                key_dispatch_event(dev, KEY_EVENT_LONG);
                dev->click_count = 0;
                //LOG_I("key [%s] long press release", dev->ops->name);
            } else if(now - dev->press_start_ms >= policy->long_press_ms +
                      dev->repeat_count * policy->repeat_interval_ms) {
                dev->repeat_count++;
                key_dispatch_event(dev, KEY_EVENT_REPEAT);
                isRepeat = 1;
                //LOG_I("key [%s] repeat %d", dev->ops->name, dev->repeat_count);
            }
            break;

        case KEY_STATE_RELEASED:
            if(hw_pressed) {
                /* 双击检测窗口内再次按下 */
                if(now - dev->press_start_ms < policy->double_click_ms) {
                    dev->state = KEY_STATE_DEBOUNCE;
                    dev->press_start_ms = now;
                    dev->click_count = 2;  // 标记为双击检测
                    //LOG_I("key [%s] double click debounce", dev->ops->name);
                } else {
                    dev->state = KEY_STATE_DEBOUNCE;
                    dev->press_start_ms = now;
                    dev->click_count = 0;  // 新的单击
                }
            } else if(now - dev->press_start_ms >= policy->double_click_ms) {
                dev->state = KEY_STATE_IDLE;
                dev->click_count = 0;      // 超时未双击，清除计数
                if(isDouble == 1) {
                    isDouble = 0;
                    break;
                }
                key_dispatch_event(dev, KEY_EVENT_CLICK);
            }
            break;

        default:
            dev->state = KEY_STATE_IDLE;
            dev->click_count = 0;
            break;
    }

    return KEY_OK;
}

/*============================================================================*
 * 批量扫描接口
 *============================================================================*/
KeyError key_scanAll(void)
{
    uint8_t isScanFail = 0;
    KeyError ret;
    
    for(uint8_t i = 0; i < g_key_count; i++) {
        ret = key_scan(g_keys[i]);
        if(ret != KEY_OK) {
            //LOG_E("%s[id=%u] has been scanned failed, reason serialnum:%d", g_keys[i]->ops->name, g_keys[i]->id, ret); 
            isScanFail = 1;
        }
    }

    return (isScanFail ? KEY_ERR_SCAN : KEY_OK);
}

/*============================================================================*
 * 控制接口
 *============================================================================*/

KeyError key_enCtl(KeyDev *dev, bool enable)
{
    if(!dev)
        return KEY_ERR_NULL_PTR;
    dev->isEnabled = enable;
    if(!enable) {
        dev->state = KEY_STATE_IDLE;
        dev->click_count = 0;
        dev->repeat_count = 0;
    }
    //LOG_I("[%s] %s", dev->ops->name, enable ? "enabled" : "disabled");
    return KEY_OK;
}

KeyError key_sleep(KeyDev *dev)
{
    if(!dev || !dev->ops || !dev->ops->sleep)
        return KEY_ERR_NULL_PTR;

    KeyError ret = dev->ops->sleep(dev);
    if(ret != KEY_OK) {
        //LOG_E("[%s] Enter sleep mode failed", dev->ops->name);
        return KEY_ERR_ENTER_SLEEP;
    }
    
    dev->status = KEY_STATUS_SLEEP;
    //LOG_I("key [%s] entered sleep mode", dev->ops->name);
    
    return KEY_OK;
}

KeyError key_wakeup(KeyDev *dev)
{
    if(!dev || !dev->ops || !dev->ops->wakeup)
        return KEY_ERR_NULL_PTR;

    KeyError ret = dev->ops->wakeup(dev);
    if(ret != KEY_OK) {
        //LOG_E("[%s] wakeup failed", dev->ops->name);
        return KEY_ERR_WAKEUP;
    }

    dev->status = KEY_STATUS_ONLINE;
    //LOG_I("key [%s] wakeup now", dev->ops->name);
    
    return KEY_OK;
}

KeyError key_self_test(KeyDev *dev)
{
    if(!dev || !dev->ops || !dev->ops->self_test)
        return KEY_ERR_NULL_PTR;
    return dev->ops->self_test(dev);
}

KeyError key_set_callback_withDev(KeyDev *dev, KeyEvtCallback callback, void *user_data)
{
    if(!dev || !callback)
        return KEY_ERR_NULL_PTR;
    dev->callback  = callback;
    dev->user_data = user_data;
    return KEY_OK;
}

KeyError key_set_callback_withID(KeyID id, KeyEvtCallback callback, void *user_data)
{
    KeyDev *dev = key_getDev_byID(id);
    if(!dev)
        return KEY_ERR_NULL_PTR;

    return key_set_callback_withDev(dev, callback, user_data);
}

KeyError key_register(KeyDev *dev);
KeyDev*  key_find_by_type(KeyType type);
KeyDev*  key_find_by_id(KeyID type);
KeyError key_init(KeyDev *dev);
KeyError key_read(KeyDev *dev, KeyInfo *out);

KeyError key_scan(KeyDev *dev);
KeyError key_scanAll(void);

KeyError key_enCtl(KeyDev *dev, bool enable);
KeyError key_sleep(KeyDev *dev);
KeyError key_wakeup(KeyDev *dev);
KeyError key_self_test(KeyDev *dev);

KeyError key_set_callback(KeyDev *dev, KeyEvtCallback cb, void *user_data);

#endif /* #if KEY_IS_USE */

