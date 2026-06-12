#ifndef __VM501_H
#define __VM501_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

// VM501 Modbus 相关宏定义
#define VM501_MODBUS_ADDR     0x01   // 默认设备地址
#define VM501_REG_WKMOD       0x0005 // 工作模式
#define VM501_REG_ATSD_SEL    0x0007 // 自动上传
#define VM501_REG_S_FRQ       0x0023 // 传感器频率值 (0.1Hz)
#define VM501_REG_TEMP        0x0029 // 温度值 (0.1C)

/*
 * @brief  初始化VM501，通过 Modbus 设置为连续测量模式并关闭主动上传
 * @retval 0: 成功, -1: 失败
 */
int VM501_Init(void);

/*
 * @brief  选择VM501测量通道
 * @param  channel: 1 到 8
 */
void VM501_SelectChannel(uint8_t channel);

/*
 * @brief  单次读取振弦传感器频率与温度 (基于 Modbus RTU)
 * @param  channel: 1 到 8
 * @param  freq: 获取的频率指针 (Hz)
 * @param  temp: 获取的温度指针 (C)
 * @retval 0: 成功, -1: 失败/超时
 */
int VM501_ReadSensor(uint8_t channel, float *freq, float *temp);

#ifdef __cplusplus
}
#endif

#endif /* __VM501_H */
