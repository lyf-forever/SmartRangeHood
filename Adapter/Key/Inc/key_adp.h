#ifndef __KEY_ADP_H_
#define __KEY_ADP_H_

#include "key.h"

#if KEY_IS_USE /* 如果使用按键 */

#include "key_service.h"    

void key_adp_register(void);
void key_adp_init(void);

void key_log_event(const KeyInfo *info);
void key_eventCallback(const KeyInfo *info, void *user_data);

#endif /* #if KEY_IS_USE */

#endif /* __KEY_ADP_H_ */

