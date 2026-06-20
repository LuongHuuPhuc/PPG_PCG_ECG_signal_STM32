/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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

#define MIN(a, b) 				 	((a) < (b) ? (a) : (b))
#define SYNC_INTERMEDIARY_USING		1 	/* Su dung sync task trung gian cho dong bo, uncomment neu muon logger nhan truc tiep tu sensor task */
#define SENSOR_BINARY_PACKET		1	/* Dung UART de gui Binary packet */
//#define SENSOR_LOGGER_USING			1	/* Dung UART de log data */
//#define SENSOR_SD_CARD_USING		1	/* Dung SD Card de luu data */

/* Debug Macros */
//#define SYNC_BLOCK_COUNT_DEBUG  	1	/* Debug block data tu Sync Task */
//#define DEBUG_DWT					1  	/* Data Watchdog & Trace de tinh toan thoi gian */
//#define DEBUG_SEGGER_RTT			1	/* Debug bang SEGGER Real-time Transfer J-Link thay cho UART */
//#define DEBUG_SWV_ITM				1	/* Debug bang Serial Wire Viewer thay cho UART */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define CS_PIN_Pin GPIO_PIN_4
#define CS_PIN_GPIO_Port GPIOA
#define BUTTON_Pin GPIO_PIN_10
#define BUTTON_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
