#ifndef __FLASH_H
#define __FLASH_H

#include "sys.h"

/* define lists */
#define FLASH_CAP_MSG_ADDR  0x1FFFF7E0UL
#define Flash_Lock()   FLASH_Lock()

typedef enum {
    FLASH_ERR_OK = 0x01,        /* flash操作顺利正常 */
    FLASH_ERR_INVALID_ADDR,     /* flash操作涉及的地址不合法 */
    FLASH_ERR_ERASE_PAGE,       /* flash操作擦除某页flash出错 */
    FLASH_ERR_HALFWORD_ALIGN,   /* 写地址未进行半字对齐 */
    FLASH_ERR_PROGRAM_HALFWORD, /* flash写入半字数据出错 */
} FlashError;

/* API */
FlashError  Flash_EraseArea(uint32_t start_addr, uint32_t size);
FlashError  Flash_Write(uint32_t addr, uint8_t *data, uint32_t len);
FlashError  Flash_EraseAllPages(void);
void Flash_Read(uint32_t addr, uint8_t *buf, uint32_t len);
void Flash_Unlock(void);

#endif

