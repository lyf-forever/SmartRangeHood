#include "gpio.h"

/* GPIO初始化函数 */
void io_set(const periph_gpio_t *gpio)
{
    GPIO_InitTypeDef  GPIO_InitStructure = {0};
	
	RCC_APB2PeriphClockCmd(GPIO_RCC(gpio->port), ENABLE); 
	
	GPIO_InitStructure.GPIO_Pin = gpio->pin;  
	GPIO_InitStructure.GPIO_Mode = gpio->mode;  		
	GPIO_InitStructure.GPIO_Speed = (gpio->speed == 0 ? (GPIO_Speed_50MHz) : (gpio->speed));
	
	GPIO_Init(gpio->port, &GPIO_InitStructure);  // 初始化GPIO

	if(gpio->initial_level) {
		if(gpio->initial_level == highLevel) 
			GPIO_SetBits(gpio->port, gpio->pin);
		else 
			GPIO_ResetBits(gpio->port, gpio->pin);
	} else return;
}

/* GPIO去初始化 */
void io_deset(const periph_gpio_t *gpio)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 配置引脚为浮空输入（复位默认状态） */
    GPIO_InitStruct.GPIO_Pin = gpio->pin;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(gpio->port, &GPIO_InitStruct);
    
    /* 关闭GPIO时钟 */
    if(gpio->port == GPIOA) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, DISABLE); 
	else if(gpio->port == GPIOB) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, DISABLE);
	else if(gpio->port == GPIOC) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, DISABLE);
	else if(gpio->port == GPIOD) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, DISABLE);
	else if(gpio->port == GPIOE) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, DISABLE);
	else if(gpio->port == GPIOF) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOF, DISABLE);
	else if(gpio->port == GPIOG) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOG, DISABLE);
}

/* 设置GPIO引脚电平为高 */
__inline void io_set_bit(const periph_gpio_t *gpio)
{
	GPIO_SetBits(gpio->port,gpio->pin);
}

/* 设置GPIO引脚电平为低 */
__inline void io_reset_bit(const periph_gpio_t *gpio)
{
	GPIO_ResetBits(gpio->port,gpio->pin);
}

__inline void io_write_bit(const periph_gpio_t *gpio, uint8_t level)
{
    if (level) gpio->port->BSRR = gpio->pin;
    else       gpio->port->BRR = gpio->pin; 
}

uint8_t io_getOpLevel(const periph_gpio_t *gpio) 
{
    return GPIO_ReadOutputDataBit(gpio->port, gpio->pin);
}

uint8_t io_getIpLevel(const periph_gpio_t *gpio) 
{
    return GPIO_ReadInputDataBit(gpio->port, gpio->pin);
}

/* 翻转GPIO引脚电平 */
void io_toggle_bit(const periph_gpio_t *gpio)
{
	if(GPIO_ReadOutputDataBit(gpio->port, gpio->pin) == 0) 
		GPIO_SetBits(gpio->port, gpio->pin);
	else GPIO_ResetBits(gpio->port,gpio->pin);
}

/**
 * @brief 获取指定 GPIO 引脚的当前模式（标准库模式值）
 * @param gpio  包含 port 和 pin 的结构体指针
 * @retval 模式值（如 GPIO_Mode_Out_PP），若引脚无效则返回 0xFF
 */
static GPIOMode_TypeDef io_getMode(const periph_gpio_t *gpio)
{
    if (!gpio || !gpio->port || !gpio->pin || (gpio->pin & (gpio->pin - 1)) != 0) return 0xFF;  

    // 1. 计算引脚编号 (0~15)
    uint8_t pin_num = io_getPinNumber(gpio->pin);
    
    // 2. 读取配置寄存器中的 4 位 (CNF+MODE)
    uint32_t reg_val;
    if (pin_num < 8) 
        reg_val = (gpio->port->CRL >> (pin_num * 4)) & 0x0F;
    else
        reg_val = (gpio->port->CRH >> ((pin_num - 8) * 4)) & 0x0F;

    uint8_t mode = reg_val & 0x03;      // MODE[1:0]
    uint8_t cnf  = (reg_val >> 2) & 0x03; // CNF[1:0]

    // 3. 解析模式
    if (mode == 0x00) { // 输入模式
        switch (cnf) {
            case 0x00: return GPIO_Mode_AIN;          // 模拟输入
            case 0x01: return GPIO_Mode_IN_FLOATING;  // 浮空输入
            case 0x10: // 上拉/下拉输入，需检查 ODR
                if (gpio->port->ODR & gpio->pin) 
                    return GPIO_Mode_IPU;              // 上拉输入
                else 
                    return GPIO_Mode_IPD;              // 下拉输入
            
            default:   return 0xFF;                    // 保留
        }
    } else { // 输出模式（包括复用）
        switch (cnf) {
            case 0x00: return GPIO_Mode_Out_PP;        // 推挽输出
            case 0x01: return GPIO_Mode_Out_OD;        // 开漏输出
            case 0x02: return GPIO_Mode_AF_PP;         // 复用推挽
            case 0x03: return GPIO_Mode_AF_OD;         // 复用开漏
            default:   return 0xFF;
        }
    }
}

/**
 * @brief  将 GPIO 位掩码转换为引脚编号（0~15）
 * @param  pinMask  必须是单个位的掩码（如 GPIO_Pin_0）
 * @retval 引脚编号（0~15），若为多个位则返回 0xFF
 */
uint8_t io_getPinNumber(uint16_t pinMask)
{
    uint8_t pinNum = 0; 
    
    /* 如果输入为0或多位，直接返回0xFF */
    if (pinMask == 0 || (pinMask & (pinMask - 1)) != 0) {
        return 0xFF;
    }
    
    while ((pinMask & 0x01) == 0) {
        pinMask >>= 1;
        pinNum++;
    }

	if (pinNum > 15) return 0xFF;  // 检查合法性
    return pinNum;
}

// 辅助函数：根据 EXTI 线号获取 NVIC 中断号
static IRQn_Type getIRQ_fromLine(uint8_t line)
{
    if (line <= 4)      return (IRQn_Type)(EXTI0_IRQn + line);
    else if (line <= 9) return EXTI9_5_IRQn;
    else                return EXTI15_10_IRQn;
}

/**
 * @brief 去初始化单个按键的 EXTI 配置（清除映射、禁用中断、恢复GPIO）
 * @param io  包含端口和引脚信息的结构体指针
 * @note  该函数会：
 *          1. 将 AFIO->EXTICR 中对应的映射位清零（恢复为 PAx）
 *          2. 清除 EXTI->IMR 中的对应位（禁用中断线）
 *          3. 禁用 NVIC 中对应的中断（若该中断线未被其他引脚使用，则完全禁用）
 *          4. 将 GPIO 恢复为浮空输入（最低功耗）
 */
void io_deset_all(const periph_gpio_t *gpio)
{
    if (!gpio) return;

    /* 1. 获取引脚编号（0~15）*/
    uint8_t pin_num = io_getPinNumber(gpio->pin);
    
    /* 2. 计算 EXTI 线号（等于引脚号）*/
    uint8_t exti_line = pin_num;

    /* 3. 计算 AFIO->EXTICR 寄存器索引和位偏移 */
    uint8_t reg_index = exti_line / 4;          // 0~3 对应 EXTICR1~4
    uint8_t bit_offset = (exti_line % 4) * 4;   // 0,4,8,12
    __IO uint32_t *exticr_reg = &AFIO->EXTICR[reg_index];

    /* 4. 清除该组映射（写入 0，即选择 PAx，相当于取消原映射）*/
    *exticr_reg &= ~(0xF << bit_offset);

    /* 5. 清除 EXTI 中断屏蔽（禁用该中断线）*/
    EXTI->IMR &= ~(1 << exti_line);

    /* 6. 禁用 NVIC 中对应的中断（如果该中断线未被其他引脚使用，但这里为了安全直接禁用）
          注意：EXTI0~4 有独立中断，EXTI5~9 共享，EXTI10~15 共享 */
    NVIC_DisableIRQ(getIRQ_fromLine(exti_line));

    /* 7. 将 GPIO 恢复为模拟输入（最低功耗，并清除复用功能）*/
    io_deset(gpio);
}

/**
 * @brief 使 GPIO 进入睡眠状态（可选择性关闭 EXTI 中断）
 * @param gpio    包含 port 和 pin 的结构体指针
 * @param enEXTI  1-关闭 EXTI 中断并保存状态；0-不处理 EXTI
 * @note  如果 enEXTI=1，会清除 EXTI IMR 对应位，并禁用对应的 NVIC 中断（注意共享组问题）
 */
void io_sleep(const periph_gpio_t *gpio, uint8_t enEXTI)
{
    if (!gpio) return;
    uint8_t pin_num = io_getPinNumber(gpio->pin);
    if (pin_num == 0xFF) return;

    if (enEXTI) {
        uint8_t line = pin_num;
        // 检查当前 EXTI 是否使能
        if (EXTI->IMR & (1 << line)) {
            // 关闭 EXTI 中断线
            EXTI->IMR &= ~(1 << line);
            // 禁用 NVIC 中断（注意共享组可能影响其他引脚）
            IRQn_Type irq = getIRQ_fromLine(line);
            NVIC_DisableIRQ(irq);
        } 
    }
    RCC_APB2PeriphClockCmd(GPIO_RCC(gpio->port), DISABLE);
}

/**
 * @brief 唤醒 GPIO（可选择性恢复 EXTI 中断）
 * @param gpio    包含 port 和 pin 的结构体指针
 * @param enEXTI  1-恢复 EXTI 中断（如果之前有保存状态）；0-不处理 EXTI
 * @note  仅当之前通过 io_sleep 关闭了中断且 enEXTI=1 时才会恢复
 */
void io_wkup(const periph_gpio_t *gpio, uint8_t enEXTI)
{
    if (!gpio) return;
    uint8_t pin_num = io_getPinNumber(gpio->pin);
    if (pin_num == 0xFF) return;

    if (enEXTI) {
        uint8_t line = pin_num;
        // 检查当前 EXTI 是否使能
        if (!(EXTI->IMR & (1 << line))) {
            
            // 恢复 EXTI IMR 使能
            EXTI->IMR |= (1 << line);
            // 恢复 NVIC 中断使能
            IRQn_Type irq = getIRQ_fromLine(line);
            NVIC_EnableIRQ(irq);   
        }     
    }

    RCC_APB2PeriphClockCmd(GPIO_RCC(gpio->port), ENABLE); 
}

uint8_t io_selftest(const periph_gpio_t *gpio, uint8_t reaLevel)
{
    uint8_t pinNum = io_getPinNumber(gpio->pin);
    /* 尝试读取按键，检查是否处于合理电平（由于下拉，释放时应为低）*/
    if (GPIO_ReadInputDataBit(gpio->port, gpio->pin) != reaLevel) return 1;
    /* 检查寄存器配置是否为预期 */
    GPIOMode_TypeDef mode = io_getMode(gpio);
    if (mode != gpio->mode) return 1;   // 不是下拉输入模式，配置错误  
    return 0;   // 自检通过
}        

/**
 * @brief  获取 GPIO 端口对应的 RCC 时钟使能位
 * @param  io  GPIO 端口指针（如 GPIOA, GPIOB...）
 * @retval RCC 时钟使能位（RCC_APB2Periph_GPIOx），若无效则返回 0
 */
uint32_t GPIO_RCC(GPIO_TypeDef *io)
{
    switch ((uint32_t)io) {
        case (uint32_t)GPIOA: return RCC_APB2Periph_GPIOA;
        case (uint32_t)GPIOB: return RCC_APB2Periph_GPIOB;
        case (uint32_t)GPIOC: return RCC_APB2Periph_GPIOC;
        case (uint32_t)GPIOD: return RCC_APB2Periph_GPIOD;
        case (uint32_t)GPIOE: return RCC_APB2Periph_GPIOE;
        case (uint32_t)GPIOF: return RCC_APB2Periph_GPIOF;
        case (uint32_t)GPIOG: return RCC_APB2Periph_GPIOG;
        default: return 0;
    }
}

