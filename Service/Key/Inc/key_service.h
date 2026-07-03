#ifndef __KEY_SERVICE_H_
#define __KEY_SERVICE_H_

#include "sys.h"

#if KEY_IS_USE

#include "KEY_interface.h"

/* 最大支持的按键设备数量 */
#define MAX_KEYS  8

/* API */
/* 注册与查找 */
KeyError key_register(KeyDev *dev);
KeyDev*  key_find_by_type(KeyType type);
KeyDev*  key_find_by_id(KeyID type);
/* 生命周期 */
KeyError key_init(KeyDev *dev);
KeyError key_read(KeyDev *dev, KeyInfo *out);
/* 扫描与状态机 */
KeyError key_scan(KeyDev *dev);
KeyError key_scanAll(void);
/* 控制 */
KeyError key_en_ctl(KeyDev *dev, bool enable);
KeyError key_sleep(KeyDev *dev);
KeyError key_wakeup(KeyDev *dev);
KeyError key_self_test(KeyDev *dev);
/* 回调设置 */
KeyError key_set_callback_withDev(KeyDev *dev, KeyEvtCallback callback, void *user_data);
KeyError key_set_callback_withID(KeyID id, KeyEvtCallback callback, void *user_data);

#endif /* #if KEY_IS_USE */

#endif /* __KEY_SERVICE_H_ */

