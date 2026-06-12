#ifndef __MODBUS_CRC_H
#define __MODBUS_CRC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief  计算 Modbus RTU 的 CRC16 校验码
 * @param  nData: 待校验的数据指针
 * @param  wLength: 数据长度
 * @retval 16位 CRC 校验码
 */
uint16_t Modbus_CRC16(const uint8_t *nData, uint16_t wLength);

#ifdef __cplusplus
}
#endif

#endif /* __MODBUS_CRC_H */
