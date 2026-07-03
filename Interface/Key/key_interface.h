#ifndef __KEY_INTERFACE_H_
#define __KEY_INTERFACE_H_

#include "KEY_types.h"
#include "key.h"

#if KEY_IS_USE

/* 前向声明 */
struct KeyDevice;

/* 按键操作接口（驱动层实现） */
typedef struct {
    const char *name;    /* 按键名称 */
    KeyID       id;      /* 按键ID */
    KeyType     type;    /* 按键类型 */

    KeyError (*init)     (struct KeyDevice *dev);    /* 初始化设备 */
    KeyError (*deinit)   (struct KeyDevice *dev);    /* 反初始化 */
    KeyError (*read)     (struct KeyDevice *dev, KeyInfo *data); /* 读取按键事件 */
    KeyError (*self_test)(struct KeyDevice *dev);    /* 自检 */
    KeyError (*sleep)    (struct KeyDevice *dev);    /* 睡眠 */
    KeyError (*wakeup)   (struct KeyDevice *dev);    /* 唤醒 */
} KeyOps;

/* 按键策略 */
typedef struct {
    uint32_t debounce_ms;         /* 消抖时间 (ms) 0: 使用默认值 */
    uint32_t long_press_ms;       /* 长按判定时间 (ms) 0: 使用默认值 */
    uint32_t double_click_ms;     /* 双击间隔时间 (ms) 0: 使用默认值 */
    uint32_t repeat_interval_ms;  /* 连发间隔时间 (ms) 0: 使用默认值 */
    uint8_t  repeat_start_count;  /* 连发启动次数 */
    uint8_t  max_retry;           /* 读取重试次数 0: 不重试 */
    uint8_t  max_err_cnt;         /* 连续错误超此值则标记离线 */
} KeyPolicy;

/* 按键设备实例 */
typedef struct KeyDevice {
    const KeyOps        *ops;           /* 指向具体按键硬件的实现 */
    KeyID                id;            /* 按键ID */
    KeyType              type;          /* 按键类型 */
    KeyStatus            status;        /* 按键状态 (在线/离线/错误等) */
    KeyPolicy           *policy;        /* 设备独立策略 */
    const keyIOCfg      *iocfg;         /* 按键(如GPIO端口号) */
    KeyInfo              cache;         /* 上次读到的按键数据 */
    uint32_t             last_read_ms;  /* 最近一次读取按键电平的时刻 */
    uint32_t             err_count;     /* 连续出错次数 */

    /* 按键特有扩展 */
    KeyEvtCallback       callback;        /* 事件回调函数 */
    void                *user_data;       /* 回调用户数据 */
    KeyState             state;           /* 按键状态机当前状态 */
    uint32_t             press_start_ms;  /* 按下起始时刻 */
    uint32_t             press_duration;  /* 按下持续时间 (ms) */
    bool                 isEnabled;       /* 是否使能 */  
    uint8_t              click_count;     /* 单击次数 */
    uint8_t              repeat_count;    /* 连发次数 */  
} KeyDev;

#endif /* #if KEY_IS_USE */

#endif /* __KEY_INTERFACE_H_ */

