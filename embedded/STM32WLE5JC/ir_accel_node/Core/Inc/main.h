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
#include "stm32wlxx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
extern bool loriota_was_standby;
extern bool loriota_rtc_preserved;
extern bool loriota_woke_from_rtc;
extern bool loriota_woke_from_accel;

#define LORIOTA_RTC_MAGIC 0x52544349UL
#define LORIOTA_IR_MAGIC  0x49525333UL

#define LORIOTA_BKP_APP_MAGIC  RTC_BKP_DR13
#define LORIOTA_BKP_VIBRATION  RTC_BKP_DR14
#define LORIOTA_BKP_COUNTER    RTC_BKP_DR15
#define LORIOTA_BKP_RTC_MAGIC  RTC_BKP_DR17
#define LORIOTA_BKP_POLLS      RTC_BKP_DR18
#define LORIOTA_BKP_IR_STATE   RTC_BKP_DR19
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_Pin GPIO_PIN_5
#define LED_GPIO_Port GPIOB
#define ADXL_INT1_Pin GPIO_PIN_0
#define ADXL_INT1_GPIO_Port GPIOA
#define IR_Pin GPIO_PIN_9
#define IR_GPIO_Port GPIOA
#define RF_CTRL1_Pin GPIO_PIN_4
#define RF_CTRL1_GPIO_Port GPIOA
#define RF_CTRL2_Pin GPIO_PIN_5
#define RF_CTRL2_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
