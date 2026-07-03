#include "bootloader.h"

#if HARDWARE_UPDATE_OPEN  /* 引入固件升级 */

#include "crc32_service.h"
#include "flash.h"

#if !HW_UPDATE_METHOD  /* 有线IAP */

#include "usart_driver.h"

void JumpToApp(uint32_t ram_addr, uint32_t ram_size, uint32_t app_addr, uint32_t app_size)
{
    uint32_t app_sp = *((volatile uint32_t*)app_addr);
    uint32_t app_pc = *((volatile uint32_t*)(app_addr + 4));

    /* 检查栈顶地址合法性：必须位于 SRAM 区且 4 字节对齐 */
    if ((app_sp < ram_addr) || (app_sp >= ram_addr + ram_size) || (app_sp & 0x3))
    {
        //LOG_E("top-stack addr err!");
        return;   // 非法地址，不跳转
    }

    /* 检查复位向量地址合法性：必须位于 Flash APP主存储区 */
    if ((app_pc < app_addr) || (app_pc >= app_addr + app_size))
    {
        //LOG_E("Reset handler addr err!");
        return;   // 非法地址，不跳转  
    }

    /* 关闭全局中断 */
    for(uint8_t i = 0; i < sizeof(irqList)/sizeof(irqList[0]); i++) {
        NVIC_DisableIRQ(irqList[i]);
    }
    //LOG_I("Already shut down all intrs that opened previously");
    
    /* 复位 Systick，确保不产生中断 */
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    /* 关闭 IAP 期间使用的外设，避免干扰应用程序 */
    DMA_Cmd(IAP_USART_DMA_RXCH, DISABLE);
    USART_Cmd(IAP_USART, DISABLE);
    
    __disable_irq();
    
    /* 设置主堆栈指针（MSP）为应用程序的初始栈顶 */
    __set_MSP(app_sp);

    /* 重新映射中断向量表到应用程序起始地址 */
    SCB->VTOR = app_addr;

    /* 数据同步屏障，保证所有内存访问完成 */
    __DSB();
    __ISB();

    /* 跳转到应用程序的复位中断服务函数（Cortex?M 自动识别 Thumb 状态） */
    ((void(*)(void))app_pc)();
}

#else  /* 无线OTA */



#endif /* #if !HW_UPDATE_METHOD */

#endif /* #if HARDWARE_UPDATE_OPEN */

