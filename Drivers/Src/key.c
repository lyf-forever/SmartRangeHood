#include "key.h"

#if KEY_IS_USE  /* 有使用到按键 */

#include "gpio.h"
#include "delay.h"
 
void         key_drv_init(const periph_gpio_t *key_io) { io_set(key_io); }
void       Key_drv_deinit(const periph_gpio_t *key_io) { io_deset(key_io); }
void   Key_drv_deinit_all(const periph_gpio_t *key_io) { io_deset_all(key_io); }
uint8_t      Key_drv_read(const periph_gpio_t *key_io) { return IO_IN_READ(key_io->port, key_io->pin); }
void          Key_drv_sleep(const keyIOCfg *key_iocfg) { io_sleep(key_iocfg->gpio, key_iocfg->key_io_exti_en); }
void         Key_drv_wakeup(const keyIOCfg *key_iocfg) { io_wkup(key_iocfg->gpio, key_iocfg->key_io_exti_en); }
uint8_t    Key_drv_selftest(const keyIOCfg *key_iocfg) { return io_selftest(key_iocfg->gpio, key_iocfg->key_io_releaseLevel); }

	#if USER_KEY_USE   /* 有使用用户按键 */
		#ifdef USER_KEY0_PORT  /* 有使用用户按键0 */
			const periph_gpio_t userKey0 = { .port = USER_KEY0_PORT, .pin = USER_KEY0_PIN, .mode = GPIO_Mode_IPU };
		    const keyIOCfg userKey0_cfg = { .gpio = &userKey0, .key_io_exti_en = 0, .key_io_pressLevel = LOW, .key_io_releaseLevel = HIGH };
			void        userKey0_init(void) { key_drv_init(&userKey0); }
			void      userKey0_deinit(void) { Key_drv_deinit(&userKey0); }
            void  userKey0_deinit_all(void) { Key_drv_deinit_all(&userKey0); }
			uint8_t     userKey0_read(void) { return Key_drv_read(&userKey0); }
            void       userKey0_sleep(void) { Key_drv_sleep(&userKey0_cfg); }
            void      userKey0_wakeup(void) { Key_drv_wakeup(&userKey0_cfg); }
            uint8_t userKey0_selftest(void) { return Key_drv_selftest(&userKey0_cfg); }
            
		#endif /* #ifdef USER_KEY0_PORT */

		#ifdef USER_KEY1_PORT  /* 有使用用户按键1 */
			const periph_gpio_t userKey1 = { .port = USER_KEY1_PORT, .pin = USER_KEY1_PIN, .mode = GPIO_Mode_IPU };
            const keyIOCfg userKey1_cfg = { .gpio = &userKey1, .key_io_exti_en = 0, .key_io_pressLevel = LOW, .key_io_releaseLevel = HIGH };
			void        userKey1_init(void) { key_drv_init(&userKey1); }
			void      userKey1_deinit(void) { Key_drv_deinit(&userKey1); }
            void  userKey1_deinit_all(void) { Key_drv_deinit_all(&userKey1); }
			uint8_t     userKey1_read(void) { return Key_drv_read(&userKey1); }
            void       userKey1_sleep(void) { Key_drv_sleep(&userKey1_cfg); }
            void      userKey1_wakeup(void) { Key_drv_wakeup(&userKey1_cfg); }
            uint8_t userKey1_selftest(void) { return Key_drv_selftest(&userKey1_cfg); }


		#endif /* #ifdef USER_KEY1_PORT */
	#endif /* #if USER_KEY_USE */

	#if WKUP_KEY_USE   /* 有使用唤醒按键 */
		#ifdef WKUP_KEY_PORT
			const periph_gpio_t wkupKey = { .port = WKUP_KEY_PORT, .pin = WKUP_KEY_PIN, .mode = GPIO_Mode_IPD };
			const keyIOCfg wkupKey_cfg = { .gpio = &wkupKey, .key_io_exti_en = 0, .key_io_pressLevel = HIGH, .key_io_releaseLevel = LOW };
			void        wkupKey_init(void) { key_drv_init(&wkupKey); }
			void      wkupKey_deinit(void) { Key_drv_deinit(&wkupKey); }
            void  wkupKey_deinit_all(void) { Key_drv_deinit_all(&wkupKey); }
			uint8_t     wkupKey_read(void) { return Key_drv_read(&wkupKey); }
            void       wkupKey_sleep(void) { Key_drv_sleep(&wkupKey_cfg); }
            void      wkupKey_wakeup(void) { Key_drv_wakeup(&wkupKey_cfg); }
            uint8_t wkupKey_selftest(void) { return Key_drv_selftest(&wkupKey_cfg); }

		#endif /* #ifdef WKUP_KEY_PORT */
	#endif /* #if WKUP_KEY_USE */

#endif /* #if KEY_IS_USE */

