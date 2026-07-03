#include "flash.h"

static uint8_t last_byte = 0;       /* 上一写入时剩下的末尾奇字节 */

/**
 * @brief 获取当前 MCU 的 Flash 页大小（单位：字节）
 *        根据闪存容量自动判断：
 *          ≤ 128KB → 页大小 1KB
 *          > 128KB → 页大小 2KB
 */
static uint32_t Flash_GetPageSize(void)
{
    // STM32F1 闪存容量寄存器地址：0x1FFFF7E0，值为容量（KB）
    uint16_t flash_size_kb = *(__I uint16_t*)FLASH_CAP_MSG_ADDR;

    if (flash_size_kb <= 128)
        return 1024;   // 低密度/中密度：1KB/页
    else
        return 2048;   // 高密度/互联型/XL：2KB/页
}

/**
 * @brief 获取 Flash 总容量（字节）, 0x1FFFF7E0闪存容量寄存器（16b）为只读
 */
static uint32_t Flash_GetTotalSize(void)
{
    return (uint32_t)(*(__I uint16_t*)FLASH_CAP_MSG_ADDR) * 1024UL;   
}

/**
 * @brief 擦除 Flash 指定区域（从 start_addr 开始，长度为 size 字节）
 * @note  擦除以页为单位，会擦除覆盖到的所有页。
 *        调用前需确保 Flash 已解锁。
 * @param start_addr 起始地址（必须为 Flash 有效地址）
 * @param size       擦除字节数
 * @return FlashError类型枚举
 */
FlashError Flash_EraseArea(uint32_t start_addr, uint32_t size)
{
    uint32_t page_size = Flash_GetPageSize();
    uint32_t flash_end = FLASH_BASE + Flash_GetTotalSize();
    uint32_t end_addr = start_addr + size;

    // 地址合法性检查
    if (start_addr < FLASH_BASE || end_addr > flash_end)
        return FLASH_ERR_INVALID_ADDR;

    // 对齐到页边界：起始地址所在页的首地址
    uint32_t page_start = start_addr & ~(page_size - 1);
    // 结束地址所在页的首地址（若已对齐到页尾，则不需额外擦除）
    uint32_t page_end   = ((end_addr - 1) & ~(page_size - 1)) + page_size;

    // 逐页擦除
    for (uint32_t addr = page_start; addr < page_end; addr += page_size)
    {
        if(FLASH_ErasePage(addr) != FLASH_COMPLETE)
            return FLASH_ERR_ERASE_PAGE;
    }

    return FLASH_ERR_OK;
}

/**
 * @brief 将数据缓冲区写入 Flash 指定地址（必须已擦除过）
 * @param addr Flash 目标地址
 * @param data 源数据指针
 * @param len  写入字节数（需为偶数，推荐偶数长度以保证半字对齐）
 * @return FlashError类型枚举
 * @note   STM32F1 闪存编程必须按 16 位半字写入。
 *         调用前应确保该区域已被擦除（全为 0xFFFF）。
 */
FlashError Flash_Write(uint32_t addr, uint8_t *data, uint32_t len)
{
    FlashError status = FLASH_ERR_OK;
    uint16_t half_word;

    if (addr & 0x01) {
        if(len & 0x01) {
            if(FLASH_ProgramHalfWord(addr - 1, last_byte | ((uint16_t)*data << 8)) != FLASH_COMPLETE) { 
                status = FLASH_ERR_PROGRAM_HALFWORD;  
                goto flash_end;
            }
            for(uint8_t i = 0; i < len - 1; i += 2) {
                if(FLASH_ProgramHalfWord(addr + i + 1, *(data + i + 1) | ((uint16_t)*(data + i + 2) << 8)) != FLASH_COMPLETE) {
                    status = FLASH_ERR_PROGRAM_HALFWORD;  
                    goto flash_end;
                }
            }
        } else {
            last_byte = *(data + len - 1);
            if(FLASH_ProgramHalfWord(addr - 1, last_byte | ((uint16_t)*data << 8)) != FLASH_COMPLETE) {
                status = FLASH_ERR_PROGRAM_HALFWORD;  
                goto flash_end;
            }
            for(uint8_t i = 0; i < len - 2; i += 2) {
                if(FLASH_ProgramHalfWord(addr + i + 1, *(data + i + 1) | ((uint16_t)*(data + i + 2) << 8)) != FLASH_COMPLETE) {
                    status = FLASH_ERR_PROGRAM_HALFWORD;  
                    goto flash_end;
                }
            }
        }
    } else {
        if(len & 0x01) {
            last_byte = *(data + len - 1);
            for(uint8_t i = 0; i < len - 1; i += 2) {
                if(FLASH_ProgramHalfWord(addr + i, *(data + i) | ((uint16_t)*(data + i + 1) << 8)) != FLASH_COMPLETE) {
                    status = FLASH_ERR_PROGRAM_HALFWORD;  
                    goto flash_end;
                }
            }
        } else {
            for(uint8_t i = 0; i < len; i += 2) {
                if(FLASH_ProgramHalfWord(addr + i, *(data + i) | ((uint16_t)*(data + i + 1) << 8)) != FLASH_COMPLETE) {
                    status = FLASH_ERR_PROGRAM_HALFWORD;  
                    goto flash_end;
                }
            }
        }
    }

flash_end:  
    return status;
}

/**
 * @brief 解锁 Flash（使能编程/擦除）
 */
void Flash_Unlock(void)
{
    FLASH_Unlock();
    /* 可在此处清除可能存在的错误标志 */
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
}

/**
 * @brief  擦除整颗芯片的全部 Flash（慎用）
 * @note   这会擦除所有用户 Flash 区，包括 Bootloader 自身区域！
 * @return FlashError类型枚举
 */
FlashError Flash_EraseAllPages(void)
{
    FLASH_Status status;
    uint32_t total_size = Flash_GetTotalSize();
    uint32_t page_size  = Flash_GetPageSize();

    for (uint32_t addr = FLASH_BASE; addr < (FLASH_BASE + total_size); addr += page_size)
    {
        status = FLASH_ErasePage(addr);
        if (status != FLASH_COMPLETE) 
            return FLASH_ERR_ERASE_PAGE;
    }

    return FLASH_ERR_OK;
}

/**
 * @brief 从 Flash 读取指定长度数据到缓冲区
 *        可直接用 memcpy，此处仅作 API 封装
 */
void Flash_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    // Flash 可直接寻址读取
    memcpy(buf, (void*)addr, len);
}

