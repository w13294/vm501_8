#include "vm501.h"
#include "cmsis_os.h"
#include "modbus_crc.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart3;

void VM501_SelectChannel(uint8_t channel) {
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
  case 1:
    HAL_GPIO_WritePin(channel_1_GPIO_Port, channel_1_Pin, GPIO_PIN_SET);
    break;
  case 2:
    HAL_GPIO_WritePin(channel_2_GPIO_Port, channel_2_Pin, GPIO_PIN_SET);
    break;
  case 3:
    HAL_GPIO_WritePin(channel_3_GPIO_Port, channel_3_Pin, GPIO_PIN_SET);
    break;
  case 4:
    HAL_GPIO_WritePin(channel_4_GPIO_Port, channel_4_Pin, GPIO_PIN_SET);
    break;
  case 5:
    HAL_GPIO_WritePin(channel_5_GPIO_Port, channel_5_Pin, GPIO_PIN_SET);
    break;
  case 6:
    HAL_GPIO_WritePin(channel_6_GPIO_Port, channel_6_Pin, GPIO_PIN_SET);
    break;
  case 7:
    HAL_GPIO_WritePin(channel_7_GPIO_Port, channel_7_Pin, GPIO_PIN_SET);
    break;
  case 8:
    HAL_GPIO_WritePin(channel_8_GPIO_Port, channel_8_Pin, GPIO_PIN_SET);
    break;
  default:
    break;
  }

  // 延时等待多路选择器稳定 (使用 RTOS 延时释放 CPU)
  // 【终极方案】：大幅增加到 500ms，确保继电器触点死死咬合，电感稳定。
  // 防止模块在不稳定期检测出异常电阻而将激励电压阉割到 50V 以下！
  if (osKernelGetState() == osKernelRunning) {
    osDelay(500);
  } else {
    HAL_Delay(500);
  }
}

// 基础 Modbus 读寄存器函数 (功能码 03)
static int Modbus_ReadRegs(uint16_t regAddr, uint16_t numRegs, uint8_t *outData,
                           uint16_t timeout) {
  // 【底层防死锁】：强行清除可能因继电器 EMI 干扰或 RTS 串扰引发的 ORE
  // (溢出错误) 标志位 如果不清除，HAL_UART_Receive 将永久返回
  // HAL_ERROR，导致所有通道“秒挂”
  if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_ORE) != RESET) {
    __HAL_UART_CLEAR_OREFLAG(&huart3);
  }

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
  while (HAL_UART_Receive(&huart3, &dummy, 1, 1) == HAL_OK) {
  }

  HAL_UART_Transmit(&huart3, tx_buf, 8, 100);

  // 预期接收长度：地址(1) + 功能码(1) + 字节数(1) + 数据(2*numRegs) + CRC(2)
  uint16_t rx_len = 5 + 2 * numRegs;
  uint8_t rx_buf[64] = {0};
  uint16_t rx_idx = 0;
  uint32_t start_tick = HAL_GetTick();

  while (HAL_GetTick() - start_tick < timeout) {
    if (HAL_UART_Receive(&huart3, &rx_buf[rx_idx], 1, 10) == HAL_OK) {
      rx_idx++;
      if (rx_idx >= rx_len)
        break;
    }
  }

  if (rx_idx == rx_len) {
    uint16_t calc_crc = Modbus_CRC16(rx_buf, rx_len - 2);
    uint16_t recv_crc = rx_buf[rx_len - 2] | (rx_buf[rx_len - 1] << 8);
    if (calc_crc == recv_crc && rx_buf[0] == VM501_MODBUS_ADDR &&
        rx_buf[1] == 0x03) {
      memcpy(outData, &rx_buf[3], 2 * numRegs);
      return 0; // 校验成功
    }
  }
  return -1;
}

// 基础 Modbus 写单个寄存器函数 (功能码 06)
static int Modbus_WriteReg(uint16_t regAddr, uint16_t value) {
  // 【底层防死锁】：强行清除可能因继电器 EMI 干扰或 RTS 串扰引发的 ORE
  // (溢出错误) 标志位
  if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_ORE) != RESET) {
    __HAL_UART_CLEAR_OREFLAG(&huart3);
  }

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
  while (HAL_UART_Receive(&huart3, &dummy, 1, 1) == HAL_OK) {
  }

  HAL_UART_Transmit(&huart3, tx_buf, 8, 100);

  uint8_t rx_buf[8] = {0};
  uint16_t rx_idx = 0;
  uint32_t start_tick = HAL_GetTick();

  while (HAL_GetTick() - start_tick < 500) {
    if (HAL_UART_Receive(&huart3, &rx_buf[rx_idx], 1, 10) == HAL_OK) {
      rx_idx++;
      if (rx_idx >= 8)
        break;
    }
  }

  if (rx_idx == 8) {
    uint16_t calc_crc = Modbus_CRC16(rx_buf, 6);
    uint16_t recv_crc = rx_buf[6] | (rx_buf[7] << 8);
    if (calc_crc == recv_crc && rx_buf[0] == VM501_MODBUS_ADDR &&
        rx_buf[1] == 0x06) {
      return 0; // 成功
    }
  }
  return -1;
}

int VM501_Init(void) {
  // 默认选择通道1建立配置通讯
  VM501_SelectChannel(1);
  HAL_Delay(100);

  // 1. 工作模式配置 WKMOD(0x05) = 0x4000
  // bit14=1: 修改参数时不保存至 EEPROM (降低擦写损耗)
  // bit0=0: 单次测量模式 (适配多通道轮询)
  Modbus_WriteReg(VM501_REG_WKMOD, 0x4000);
  HAL_Delay(100);

  // 2. 激励方法配置 EX_METH(0x0A) = 0x000D
  // 选用 0x000D (13)：全频段扫频法(FMIN~FMAX)
  // 这种扫频类似快速的 Chirp 信号，扫频速度极快且不依赖历史记忆。
  // 经实际测试，它在多通道切换时的起振效果和速度平衡最好，是当前方案的最优解。
  Modbus_WriteReg(0x000A, 0x000D);
  HAL_Delay(50);

  // 3. 扫频范围配置 FMIN(0x0F)=400, FMAX(0x10)=5000
  // 恢复出厂默认频段：400Hz ~ 5000Hz
  Modbus_WriteReg(0x000F, 400);
  HAL_Delay(50);
  Modbus_WriteReg(0x0010, 5000);
  HAL_Delay(50);

  // 4. 通讯模式配置 ATSD_SEL(0x07) = 0
  // 禁用主动上传，仅响应主机 Modbus 轮询指令
  Modbus_WriteReg(VM501_REG_ATSD_SEL, 0);
  HAL_Delay(100);

  return 0;
}

int VM501_ReadSensor(uint8_t channel, float *freq, float *temp) {
  if (channel >= 1 && channel <= 8) {
    VM501_SelectChannel(channel);
    HAL_Delay(200); // 预留通道稳定时间

    Modbus_WriteReg(0x0003, 0x0031); // 触发第一次测量
  }

  int timeout_ms = 12000; // 基础 12 秒总超时
  int valid_count = 0;
  int zero_freq_count = 0;
  int disconnect_count = 0; // 新增：空通道容错计数器

  while (timeout_ms > 0) {
    // 1. 科学轮询 SYS_STA (0x0020) 判断测量是否完成，单次测量最多等 3000ms
    int wait_ms = 0;
    int measure_done = 0;
    int is_disconnected = 0;

    while (wait_ms < 3000 && timeout_ms > 0) {
      if (osKernelGetState() == osKernelRunning) {
        osDelay(100);
      } else {
        HAL_Delay(100);
      }
      wait_ms += 100;
      timeout_ms -= 100;

      uint8_t sta_buf[2];
      // 读取 SYS_STA，地址 0x0020
      if (Modbus_ReadRegs(0x0020, 1, sta_buf, 100) == 0) {
        uint16_t sys_sta = (sta_buf[0] << 8) | sta_buf[1];
        
        // 【容错升级：防继电器抖动】
        // 检查 bit15 (0x8000)：1 表示“线圈未接入”。
        if (sys_sta & 0x8000) {
          is_disconnected = 1;
          break; // 发现未接入，立刻跳出本轮状态查询
        }

        // 检查 bit4 (0x0010)：1 表示当前单次测量已完成
        if (sys_sta & 0x0010) {
          measure_done = 1;
          // 硬件经验：模块状态虽已置 1，但底层可能还需要一点时间将浮点数据搬运到保持寄存器
          // 延时 1000ms 缓冲，确保接下来读到的数据 100% 完整稳定！
          if (osKernelGetState() == osKernelRunning) {
            osDelay(1000);
          } else {
            HAL_Delay(1000);
          }
          break; 
        }
      }
    }

    if (is_disconnected) {
      disconnect_count++;
      if (disconnect_count >= 10) {
        // 连续 10 次（约 1000ms / 1秒）都确认未接入，确保电感和继电器绝对稳定后，才判定为空通道
        return -1; 
      } else {
        // 可能是继电器正在弹跳、电感未稳定导致的误判！
        // 给它机会，重新下发测量指令再试一次！
        Modbus_WriteReg(0x0003, 0x0031);
        continue;
      }
    } else {
      disconnect_count = 0; // 只要有一次没报 0x8000，说明接触良好了，清零容错
    }

    if (!measure_done) {
      // 如果 3 秒都没测完（或通信异常），补发指令重新触发
      Modbus_WriteReg(0x0003, 0x0031);
      continue;
    }

    // 2. 测量完成，读取频率和温度 (从 0x0022 开始读 7 个寄存器)
    uint8_t data[14];
    if (Modbus_ReadRegs(VM501_REG_S_FRQ, 7, data, 500) == 0) {
      uint16_t raw_freq = (data[0] << 8) | data[1];
      int16_t raw_temp = (data[12] << 8) | data[13];

      float current_freq = (float)raw_freq / 10.0f;
      float current_temp = (float)raw_temp / 10.0f;

      if (current_freq > 1.0f) {
        valid_count++;
        zero_freq_count = 0; // 读到有效数据，清零
        if (valid_count >= 3) {
          *freq = current_freq;
          *temp = current_temp;
          return 0; // 成功读到 3 次有效数据，返回
        } else {
          // 还没凑够 3 次，火速触发下一次测量
          Modbus_WriteReg(0x0003, 0x0031);
        }
      } else {
        zero_freq_count++;
        // 读到 0Hz，说明钢弦没振起来，立即重新触发
        Modbus_WriteReg(0x0003, 0x0031);
        if (zero_freq_count >= 6) {
          return -1; // 连续 6 次 0Hz，判定为彻底失败（断线或损坏）
        }
      }
    } else {
      // 读取寄存器失败（超时/校验错），重新触发测量
      Modbus_WriteReg(0x0003, 0x0031);
    }
  }

  return -1;
}
