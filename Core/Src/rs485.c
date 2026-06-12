#include "rs485.h"
#include "usart.h"

extern UART_HandleTypeDef huart2;

void RS485_SendData(uint8_t *pData, uint16_t Size)
{
    // 1. 将 RS485_EN (PA1) 拉高，使能 RS485 发送模式
    HAL_GPIO_WritePin(RS485_EN_GPIO_Port, RS485_EN_Pin, GPIO_PIN_SET);
    
    // 给予芯片非常短暂的电平建立时间
    // (通常不用延时，或者稍微堵塞几个指令周期即可，这里为了稳妥加1ms)
    HAL_Delay(1);

    // 2. 阻塞发送数据
    HAL_UART_Transmit(&huart2, pData, Size, 1000);

    // 3. 将 RS485_EN (PA1) 拉低，恢复接收/隔离模式
    HAL_GPIO_WritePin(RS485_EN_GPIO_Port, RS485_EN_Pin, GPIO_PIN_RESET);
}
