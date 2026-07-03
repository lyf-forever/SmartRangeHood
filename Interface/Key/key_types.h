#ifndef __KEY_TYPES_H_
#define __KEY_TYPES_H_

#include "sys.h"

#if KEY_IS_USE

#include <stdint.h>

typedef enum {
    KEY_TYPE_NORMAL = 0,  /* 普通独立按键 */
    KEY_TYPE_MATRIX,      /* 矩阵按键 */
    KEY_TYPE_ENCODER,     /* 旋转编码器 */
    KEY_TYPE_TOUCH,       /* 触摸按键 */
    KEY_TYPE_MAX,         
    /* ... */
} KeyType;

/* 按键设备状态 */
typedef enum {
    KEY_STATUS_UNINIT,           /* 未初始化 */
    KEY_STATUS_OFFLINE,          /*  离线 */
    KEY_STATUS_ONLINE,           /* 在线正常工作 */
    KEY_STATUS_ERROR,            /* 发生错误(如连续读取失败) */
    KEY_STATUS_SLEEP,            /* 休眠状态 */
    KEY_STATUS_BUSY,             /* 忙状态(正在处理) */
} KeyStatus;

/* 按键状态 */
typedef enum {
    KEY_STATE_IDLE,           /* 空闲,正常可用 */
    KEY_STATE_DEBOUNCE,       /* 消抖 */
    KEY_STATE_PRESSED,        /* 已按下（未松开） */
    KEY_STATE_LONG_PRESS,     /* 确认长按 */
    KEY_STATE_RELEASED,       /* 已释放 */
    KEY_STATE_REPEAT,         /* 连发中 */
} KeyState;

/* 按键错误码 */
typedef enum {
    KEY_OK                =  0,  /* 按键操作没有问题 */
    KEY_ERR_NOT_FOUND     = -1,  /* 按键未找到 */
    KEY_ERR_INIT_FAIL     = -2,  /* 按键初始化失败 */
    KEY_ERR_READ_FAIL     = -3,  /* 按键读取失败*/
    KEY_ERR_TIMEOUT       = -4,  /* 按键操作超时 */
    KEY_ERR_INVALID_PARAM = -5,  /* 按键参数非法 */
    KEY_ERR_NOT_READY     = -6,  /* 按键未准备好 */
    KEY_ERR_NULL_PTR      = -7,  /* 按键操作涉及指针未找到 */
    KEY_ERR_ENTER_SLEEP   = -8,  /* 按键未能进入休眠状态 */
    KEY_ERR_WAKEUP        = -9,  /* 按键未能被唤醒（退出休眠） */
    KEY_ERR_SELFTEST      = -10, /* 按键自检失败 */
    KEY_ERR_SCAN          = -11, /* 按键扫描失败 */
} KeyError;

/** 按键事件类型 */
typedef enum {
    KEY_EVENT_NONE      = 0x00,  /* 无事件 */
    KEY_EVENT_PRESS     = 0x01,  /* 按下事件 */
    KEY_EVENT_RELEASE   = 0x02,  /* 释放事件 */
    KEY_EVENT_CLICK     = 0x04,  /* 单击事件 */
    KEY_EVENT_DOUBLE    = 0x08,  /* 双击事件 */
    KEY_EVENT_LONG      = 0x10,  /* 长按事件 */
    KEY_EVENT_REPEAT    = 0x20,  /* 连发事件 */
} KeyEvent;

/* 按键ID */
typedef enum {
    KEY_DRV_0  = 0x00,
    KEY_DRV_1,
    KEY_DRV_2,
    KEY_DRV_3,
    KEY_DRV_4,
    KEY_DRV_5,
    KEY_DRV_6,
    KEY_DRV_7,
    KEY_DRV_8,
    KEY_DRV_9,
    KEY_DRV_10,
    KEY_DRV_11,
    KEY_DRV_12,
    KEY_DRV_13,
    KEY_DRV_14,
    KEY_DRV_15,
    KEY_DRV_MAX
} KeyID;

/* 按键事件信息 */
typedef struct {
    const char  *name;           /* 按键名称 */
    KeyID       id;              /* 按键ID */
    KeyEvent    event;           /* 事件类型 */
    bool        isPressed;       /* 是否按下 */
    uint32_t    press_duration;  /* 按下持续时间 (ms) */
    uint8_t     click_count;     /* 单击次数 */
    uint8_t     repeat_count;    /* 连发次数 */
} KeyInfo;

/* 按键句柄 (不透明类型) */
typedef struct key_handle_s *key_handle_t;

/*============================================================================*
 * 回调函数类型
 *============================================================================*/

/**
 * @brief 按键事件回调函数类型
 * @param[in] info 事件信息指针
 * @param[in] user_data 用户自定义数据
 */
typedef void (*KeyEvtCallback)(const KeyInfo *info, void *user_data);

#endif /* #if KEY_IS_USE */

#endif // !__KEY_TYPES_H_

