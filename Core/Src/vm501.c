#include "vm501.h"
#include "usart.h"
#include "modbus_crc.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart3;

void VM501_SelectChannel(uint8_t channel)
{
    // 复位所有通道
    HAL_GPIO_WritePin(channel_8_GPIO_Port, channel_8_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(channel_7_GPIO_Port, channel_7_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(channel_6_GPIO_Port, channel_6_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(channel_5_GPIO_Port, channel_5_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(channel_4_GPIO_Port, channel_4_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(channel_3_GPIO_Port, channel_3_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(channel_2_GPIO_Port, channel_2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(channel_1_GPIO_Port, channel_1_Pin, GPIO_PIN_RESET);

    // 设置目标通道
    switch (channel) {
        case 1: HAL_GPIO_WritePin(channel_1_GPIO_Port, channel_1_Pin, GPIO_PIN_SET); break;
        case 2: HAL_GPIO_WritePin(channel_2_GPIO_Port, channel_2_Pin, GPIO_PIN_SET); break;
        case 3: HAL_GPIO_WritePin(channel_3_GPIO_Port, channel_3_Pin, GPIO_PIN_SET); break;
        case 4: HAL_GPIO_WritePin(channel_4_GPIO_Port, channel_4_Pin, GPIO_PIN_SET); break;
        case 5: HAL_GPIO_WritePin(channel_5_GPIO_Port, channel_5_Pin, GPIO_PIN_SET); break;
        case 6: HAL_GPIO_WritePin(channel_6_GPIO_Port, channel_6_Pin, GPIO_PIN_SET); break;
        case 7: HAL_GPIO_WritePin(channel_7_GPIO_Port, channel_7_Pin, GPIO_PIN_SET); break;
        case 8: HAL_GPIO_WritePin(channel_8_GPIO_Port, channel_8_Pin, GPIO_PIN_SET); break;
        default: break;
    }
    
    // 延时等待多路选择器稳定 (使用 RTOS 延时释放 CPU)
    if (osKernelGetState() == osKernelRunning) {
        osDelay(50);
    } else {
        HAL_Delay(50);
    }
}

// 基础 Modbus 读寄存器函数 (功能码 03)
static int Modbus_ReadRegs(uint16_t regAddr, uint16_t numRegs, uint8_t *outData, uint16_t timeout)
{
    uint8_t tx_buf[8];
    tx_buf[0] = VM501_MODBUS_ADDR;
    tx_buf[1] = 0x03;
    tx_buf[2] = (regAddr >> 8) & 0xFF;
    tx_buf[3] = regAddr & 0xFF;
    tx_buf[4] = (numRegs >> 8) & 0xFF;
    tx_buf[5] = numRegs & 0xFF;
    
    uint16_t crc = Modbus_CRC16(tx_buf, 6);
    tx_buf[6] = crc & 0xFF;
    tx_buf[7] = (crc >> 8) & 0xFF;

    // 清空接收缓存
    uint8_t dummy;
    while (HAL_UART_Receive(&huart3, &dummy, 1, 1) == HAL_OK) {}

    HAL_UART_Transmit(&huart3, tx_buf, 8, 100);

    // 预期接收长度：地址(1) + 功能码(1) + 字节数(1) + 数据(2*numRegs) + CRC(2)
    uint16_t rx_len = 5 + 2 * numRegs;
    uint8_t rx_buf[64] = {0};
    uint16_t rx_idx = 0;
    uint32_t start_tick = HAL_GetTick();

    while (HAL_GetTick() - start_tick < timeout) {
        if (HAL_UART_Receive(&huart3, &rx_buf[rx_idx], 1, 10) == HAL_OK) {
            rx_idx++;
            if (rx_idx >= rx_len) break;
        }
    }

    if (rx_idx == rx_len) {
        uint16_t calc_crc = Modbus_CRC16(rx_buf, rx_len - 2);
        uint16_t recv_crc = rx_buf[rx_len - 2] | (rx_buf[rx_len - 1] << 8);
        if (calc_crc == recv_crc && rx_buf[0] == VM501_MODBUS_ADDR && rx_buf[1] == 0x03) {
            memcpy(outData, &rx_buf[3], 2 * numRegs);
            return 0; // 校验成功
        }
    }
    return -1;
}

// 基础 Modbus 写单个寄存器函数 (功能码 06)
static int Modbus_WriteReg(uint16_t regAddr, uint16_t value)
{
    uint8_t tx_buf[8];
    tx_buf[0] = VM501_MODBUS_ADDR;
    tx_buf[1] = 0x06;
    tx_buf[2] = (regAddr >> 8) & 0xFF;
    tx_buf[3] = regAddr & 0xFF;
    tx_buf[4] = (value >> 8) & 0xFF;
    tx_buf[5] = value & 0xFF;
    
    uint16_t crc = Modbus_CRC16(tx_buf, 6);
    tx_buf[6] = crc & 0xFF;
    tx_buf[7] = (crc >> 8) & 0xFF;

    uint8_t dummy;
    while (HAL_UART_Receive(&huart3, &dummy, 1, 1) == HAL_OK) {}

    HAL_UART_Transmit(&huart3, tx_buf, 8, 100);

    uint8_t rx_buf[8] = {0};
    uint16_t rx_idx = 0;
    uint32_t start_tick = HAL_GetTick();

    while (HAL_GetTick() - start_tick < 500) {
        if (HAL_UART_Receive(&huart3, &rx_buf[rx_idx], 1, 10) == HAL_OK) {
            rx_idx++;
            if (rx_idx >= 8) break;
        }
    }

    if (rx_idx == 8) {
        uint16_t calc_crc = Modbus_CRC16(rx_buf, 6);
        uint16_t recv_crc = rx_buf[6] | (rx_buf[7] << 8);
        if (calc_crc == recv_crc && rx_buf[0] == VM501_MODBUS_ADDR && rx_buf[1] == 0x06) {
            return 0; // 成功
        }
    }
    return -1;
}

int VM501_Init(void)
{
    // 默认选择通道1以建立配置通讯
    VM501_SelectChannel(1);
    
    // 配置为自动连续测量模式 WKMOD(0x05) = 1
    // 在这套框架下，依靠模块连续测量，切换通道后等待其稳定出值再读取
    Modbus_WriteReg(VM501_REG_WKMOD, 1);
    HAL_Delay(50);
    
    // 关闭主动上传 ATSD_SEL(0x07) = 0，确保只接受主机Modbus查询
    Modbus_WriteReg(VM501_REG_ATSD_SEL, 0);
    HAL_Delay(50);
    
    return 0;
}

int VM501_ReadSensor(uint8_t channel, float *freq, float *temp)
{
    if (channel >= 1 && channel <= 8) {
        VM501_SelectChannel(channel);
    }
    
    // 延时等待模块自动对新接通的通道进行测量
    // 模块默认连续扫频间隔为500ms，加上扫频本身的时间，这里给予 10000ms 充足时间
    if (osKernelGetState() == osKernelRunning) {
        osDelay(10000);
    } else {
        HAL_Delay(10000);
    }

    // 一次性读取从 0x23 开始的 7 个寄存器，覆盖频率和温度
    // 0x23: 频率 (0.1Hz) -> out[0, 1]
    // 0x24: 预留/高位模数
    // 0x25: 预留/低位模数
    // 0x26: 预留
    // 0x27: 电阻
    // 0x28: 电压
    // 0x29: 温度 (0.1C) -> out[12, 13]
    uint8_t data[14];
    if (Modbus_ReadRegs(VM501_REG_S_FRQ, 7, data, 1000) == 0) {
        // 大端模式合并字节
        uint16_t raw_freq = (data[0] << 8) | data[1];
        int16_t raw_temp = (data[12] << 8) | data[13]; 

        *freq = (float)raw_freq / 10.0f;
        *temp = (float)raw_temp / 10.0f;
        
        // 如果读取到的频率大于1Hz，则视为测量成功
        if (*freq > 1.0f) {
            return 0; 
        }
    }
    
    // 返回 -1 代表读取失败、校验错误或模块未检测到有效频率
    return -1; 
}
