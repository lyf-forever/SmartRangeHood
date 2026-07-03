#ifndef __CRC32_SERVICE_H_
#define __CRC32_SERVICE_H_

#include "sys.h"

#if CRC32_IS_USE  /* 如果系统引入CRC32校验 */

uint32_t CRC32_Calculate(const uint8_t *data, uint32_t len);
uint32_t CRC32_Calculate_Stream(uint32_t prev_crc, const uint8_t *data, uint32_t len);

uint32_t crc32_calculate(uint8_t *buf, uint32_t len);
uint32_t CRC32_Flash(uint32_t addr, uint32_t len);

#endif /* #if CRC32_IS_USE */

#endif /* #ifndef __CRC32_SERVICE_H_ */
