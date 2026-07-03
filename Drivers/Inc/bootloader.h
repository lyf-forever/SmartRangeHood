#ifndef __BOOTLOADER_H_
#define __BOOTLOADER_H_

#include "sys.h"

#if HARDWARE_UPDATE_OPEN  /* 引入固件升级 */

#if !HW_UPDATE_METHOD  /* 有线IAP */
/* RAM */
#define RAM_START_ADDR          0x20000000UL
#define RAM_SIZE                0x10000UL       // 64KB
#define RAM_END_ADDR            (RAM_START_ADDR + RAM_SIZE)
/* Flash 分区 (以 F103ZE 512KB 为例，可按实际调整) */
#define BOOTLOADER_START_ADDR   0x08000000UL                                    // BOOT区起始
#define BOOTLOADER_SIZE         0x20000UL                                       // BOOT - 128KB
#define BOOT_END_ADDR           (BOOTLOADER_START_ADDR + BOOTLOADER_SIZE - 1UL) // BOOT 结束
#define APP_START_ADDR          (BOOTLOADER_START_ADDR + BOOTLOADER_SIZE)       // APP  起始
#define APP_SIZE                0x20000UL                                       // APP - 128KB 
#define APP_END_ADDR            (APP_START_ADDR + APP_SIZE - 1UL)               // APP结束
#define STORAGE_START_ADDR      (APP_START_ADDR + APP_SIZE)                     // 暂存区起始
#define STORAGE_SIZE            0x20000UL                                       // 128KB 暂存区大小 (≥ APP_SIZE)
#define STORAGE_END_ADDR        (STORAGE_START_ADDR + STORAGE_SIZE - 1UL)       // 暂存区结束

/* 升级标志 —— 使用备份寄存器 BKP_DR1 */
#define UPGRADE_FLAG_VALUE      0x5A5A5A5A
#define BKP_UPGRADE_FLAG        BKP->DR1

/* 协议 */
#define CMD_ACK                 0xAA
#define CMD_NAK                 0x55

/* 函数API声明 */
void JumpToApp(uint32_t ram_addr, uint32_t ram_size, uint32_t app_addr, uint32_t app_size);

#else  /* 无线OTA */



#endif /* #if !HW_UPDATE_METHOD */

#endif /* #if HARDWARE_UPDATE_OPEN */

#endif /* #ifndef __BOOTLOADER_H_ */

