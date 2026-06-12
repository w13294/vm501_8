/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define RS485_EN_Pin GPIO_PIN_1
#define RS485_EN_GPIO_Port GPIOA
#define channel_8_Pin GPIO_PIN_4
#define channel_8_GPIO_Port GPIOA
#define channel_7_Pin GPIO_PIN_5
#define channel_7_GPIO_Port GPIOA
#define channel_6_Pin GPIO_PIN_6
#define channel_6_GPIO_Port GPIOA
#define channel_5_Pin GPIO_PIN_7
#define channel_5_GPIO_Port GPIOA
#define channel_4_Pin GPIO_PIN_4
#define channel_4_GPIO_Port GPIOC
#define channel_3_Pin GPIO_PIN_5
#define channel_3_GPIO_Port GPIOC
#define channel_2_Pin GPIO_PIN_10
#define channel_2_GPIO_Port GPIOB
#define channel_1_Pin GPIO_PIN_11
#define channel_1_GPIO_Port GPIOB
#define WDI_Pin GPIO_PIN_8
#define WDI_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
