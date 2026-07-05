#ifndef __DEBUG_LOG_H_
#define __DEBUG_LOG_H_

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 等级定义
typedef enum {
    LOG_LVL_ERROR = 1,
    LOG_LVL_WARN  = 2,
    LOG_LVL_INFO  = 3,
    LOG_LVL_DEBUG = 4 
} log_level_t;

// 全局日志等级控制
extern log_level_t g_log_level;

// 底层输出接口，由用户提供
void log_output(const char *str);

// 获取系统运行时间（ms），配合 FreeRTOS
uint32_t log_get_tick(void);

// 带等级判断的输出宏
#define LOG(level, fmt, ...) \
    do { \
        if (level <= g_log_level) { \
            char buf[128]; \
            snprintf(buf, sizeof(buf), "[%lu][%c] " fmt "\r\n", \
                     (unsigned long)log_get_tick(), \
                     (level == LOG_LVL_ERROR ? 'E' : \
                      level == LOG_LVL_WARN  ? 'W' : \
                      level == LOG_LVL_INFO  ? 'I' : 'D'), \
                     ##__VA_ARGS__); \
            log_output(buf); \
        } \
    } while(0)

/* ========== 新增：专用于 VOFA+ FireWater 协议的打印宏 ========== */
#define LOG_VOFA(fmt, ...) \
    do { \
        char buf[128]; \
        snprintf(buf, sizeof(buf), fmt "\n", ##__VA_ARGS__); \
        log_output(buf); \
    } while(0)

#define LOG_E(fmt, ...) LOG(LOG_LVL_ERROR, fmt, ##__VA_ARGS__)
#define LOG_I(fmt, ...) LOG(LOG_LVL_INFO,  fmt, ##__VA_ARGS__)
#define LOG_W(fmt, ...) LOG(LOG_LVL_WARN,  fmt, ##__VA_ARGS__)
#define LOG_D(fmt, ...) LOG(LOG_LVL_DEBUG, fmt, ##__VA_ARGS__)

// 如果完全不想调试，可以全局关闭
#ifdef DISABLE_ALL_LOGS
  #undef LOG_E
  #undef LOG_I
  #undef LOG_W
  #undef LOG_D
  #define LOG_E(...)
  #define LOG_I(...)
  #define LOG_W(...)
  #define LOG_D(...)
#endif

#ifdef __cplusplus
}
#endif

#endif


