#ifndef __MODBUS_SLAVE_H
#define __MODBUS_SLAVE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

// 存储8通道频率和温度的全局寄存器池 (共 16 个)
// 偶数索引存储频率(0.1Hz)，奇数索引存储温度(0.1C)
extern uint16_t Modbus_HoldingRegs[16];

/*
 * @brief  初始化 Modbus 从机机制 (开启 USART2 接收中断)
 */
void Modbus_Slave_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __MODBUS_SLAVE_H */
