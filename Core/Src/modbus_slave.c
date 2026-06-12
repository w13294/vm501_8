#include "modbus_slave.h"
#include "modbus_crc.h"
#include "rs485.h"
#include "usart.h"

uint16_t Modbus_HoldingRegs[16] = {0};
uint8_t modbus_rx_buf[8];

void Modbus_Slave_Init(void)
{
    // 开启 USART2 的 8 字节中断接收
    // 网关发送读保持寄存器查询 (03 功能码) 通常正好是 8 个字节
    HAL_UART_Receive_IT(&huart2, modbus_rx_buf, 8);
}

// 串口接收完成回调函数
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart2) {
        // 校验 CRC16
        uint16_t crc = Modbus_CRC16(modbus_rx_buf, 6);
        uint16_t recv_crc = modbus_rx_buf[6] | (modbus_rx_buf[7] << 8);
        
        // 判断 CRC 正确，并且地址为 0x01，功能码为 0x03
        if (crc == recv_crc && modbus_rx_buf[0] == 0x01 && modbus_rx_buf[1] == 0x03) {
            
            uint16_t start_reg = (modbus_rx_buf[2] << 8) | modbus_rx_buf[3];
            uint16_t num_regs = (modbus_rx_buf[4] << 8) | modbus_rx_buf[5];
            
            // 确保查询范围不越界
            if (start_reg + num_regs <= 16) {
                uint8_t tx_buf[64];
                tx_buf[0] = 0x01;
                tx_buf[1] = 0x03;
                tx_buf[2] = num_regs * 2; // 字节数
                
                // 从全局缓存池中提取数据填入应答帧
                for (int i = 0; i < num_regs; i++) {
                    uint16_t val = Modbus_HoldingRegs[start_reg + i];
                    tx_buf[3 + i*2] = (val >> 8) & 0xFF;
                    tx_buf[4 + i*2] = val & 0xFF;
                }
                
                // 计算发送 CRC
                uint16_t tx_crc = Modbus_CRC16(tx_buf, 3 + num_regs * 2);
                tx_buf[3 + num_regs * 2] = tx_crc & 0xFF;
                tx_buf[4 + num_regs * 2] = (tx_crc >> 8) & 0xFF;
                
                // 通过 RS485 推送数据给网关
                RS485_SendData(tx_buf, 5 + num_regs * 2);
            }
        }
        
        // 重新武装中断接收，准备下一次网关查询
        HAL_UART_Receive_IT(&huart2, modbus_rx_buf, 8);
    }
}
