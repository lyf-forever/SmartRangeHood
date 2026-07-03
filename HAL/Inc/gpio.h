#ifndef __GPIO_H_
#define __GPIO_H_

#include "sys.h"

#define IO_IN_READ(port, pin)    GPIO_ReadInputDataBit(port, pin)

// #define GPIO_RCC(GPIOx)     PERIPH_APB2_RCC((GPIOx) == GPIOA ? GPIOA : (GPIOx) == GPIOB ? GPIOB : \
//                                             (GPIOx) == GPIOC ? GPIOC : (GPIOx) == GPIOD ? GPIOD : \
//                                             (GPIOx) == GPIOE ? GPIOE : (GPIOx) == GPIOF ? GPIOF : \
//                                             GPIOG)  

typedef GPIO_TypeDef*  gpio_port;     

typedef enum {
    highLevel = 1,
    lowLevel,
} iniLevel_t;

/* 外设gpio数据结构 */
typedef struct {
    gpio_port port;
    uint16_t pin;
    GPIOMode_TypeDef mode;
    GPIOSpeed_TypeDef speed;
    iniLevel_t initial_level;
} periph_gpio_t;

/* 函数声明 */
void io_set(const periph_gpio_t *gpio);
void io_deset(const periph_gpio_t *gpio);
void io_set_bit(const periph_gpio_t *gpio);
void io_reset_bit(const periph_gpio_t *gpio);
void io_write_bit(const periph_gpio_t *gpio, uint8_t level);
void io_toggle_bit(const periph_gpio_t *gpio);

uint8_t io_getOpLevel(const periph_gpio_t *gpio);
uint8_t io_getIpLevel(const periph_gpio_t *gpio);

void io_deset_all(const periph_gpio_t *gpio);
void io_sleep(const periph_gpio_t *gpio, uint8_t enEXTI);
void io_wkup(const periph_gpio_t *gpio, uint8_t enEXTI);
uint8_t io_selftest(const periph_gpio_t *gpio, uint8_t reaLevel);

uint32_t GPIO_RCC(GPIO_TypeDef *io);
uint8_t io_getPinNumber(uint16_t pinMask);

#endif /* LED_H */  
