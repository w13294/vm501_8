#include "rs485.h"
#include "usart.h"

extern UART_HandleTypeDef huart2;

void RS485_SendData(uint8_t *pData, uint16_t Size)
{
    // 1. 将 RS485_EN (PA1) 拉高，使能 RS485 发送模式
    HAL_GPIO_WritePin(RS485_EN_GPIO_Port, RS485_EN_Pin, GPIO_PIN_SET);
    
    // 给予芯片非常短暂的电平建立时间
    // (在中断中绝对不能使用 HAL_Delay，否则如果滴答定时器优先级低会导致死锁！)
    for (volatile int i = 0; i < 5000; i++) {
        __NOP();
    }

    // 2. 阻塞发送数据
    HAL_UART_Transmit(&huart2, pData, Size, 1000);

    // 3. 将 RS485_EN (PA1) 拉低，恢复接收/隔离模式
    HAL_GPIO_WritePin(RS485_EN_GPIO_Port, RS485_EN_Pin, GPIO_PIN_RESET);
}
