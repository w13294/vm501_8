#ifndef __RS485_H
#define __RS485_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/*
 * @brief  通过 RS485 (USART2) 发送二进制数据
 * @note   自动控制 PA1 (RS485_EN) 引脚进行收发方向切换
 * @param  pData: 待发送的数据指针
 * @param  Size: 数据字节数
 */
void RS485_SendData(uint8_t *pData, uint16_t Size);

#ifdef __cplusplus
}
#endif

#endif /* __RS485_H */
