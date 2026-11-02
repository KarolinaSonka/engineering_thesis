/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    subghz_phy_app.c
 * @brief   Aplikacja SubGHz_Phy do cyklicznego wysyłania (LoRa P2P)
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "platform.h"
#include "sys_app.h"
#include "subghz_phy_app.h"
#include "radio.h"

/* USER CODE BEGIN Includes */
#include <string.h>
#include "utilities_def.h"
#include "stm32_lpm.h"
#include "stm32_seq.h"
#include "stm32_timer.h"
#include <stdio.h>
#include "lora_frame.h"
#include <stdlib.h>
#include "main.h"
#include "rtc.h"
#include "i2c.h"
/* USER CODE END Includes */

/* External variables ---------------------------------------------------------*/
/* USER CODE BEGIN EV */
extern ADC_HandleTypeDef hadc;
/* USER CODE END EV */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define LORA_BANDWIDTH                              0         /* [0: 125 kHz, 1: 250 kHz, 2: 500 kHz, 3: Reserved] */
#define LORA_SPREADING_FACTOR                       7         /* [SF7..SF12] */
#define LORA_CODINGRATE                             1         /* [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8] */
#define LORA_PREAMBLE_LENGTH                        8         /* Same for Tx and Rx */
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false
#define TASK_TX 									(1 << CFG_SEQ_Task_TX)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* Radio events function pointer */
static RadioEvents_t RadioEvents;

/* USER CODE BEGIN PV */
uint16_t global_msg_counter = 0;
const uint32_t MY_NODE_ID = 0000;

static volatile bool tx_finished = false;

static uint32_t next_sleep_seconds = 900;
static uint32_t next_sleep_stage = LORIOTA_STAGE_PERIODIC;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/*!
 * @brief Function to be executed on Radio Tx Done event
 */
static void OnTxDone(void);

/**
 * @brief Function to be executed on Radio Rx Done event
 * @param  payload ptr of buffer received
 * @param  size buffer size
 * @param  rssi
 * @param  LoraSnr_FskCfo
 */
static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi,
		int8_t LoraSnr_FskCfo);

/**
 * @brief Function executed on Radio Tx Timeout event
 */
static void OnTxTimeout(void);

/**
 * @brief Function executed on Radio Rx Timeout event
 */
static void OnRxTimeout(void);

/**
 * @brief Function executed on Radio Rx Error event
 */
static void OnRxError(void);

/* USER CODE BEGIN PFP */
static void TxProcess(void);
extern void Read_ADXL345(int16_t *x, int16_t *y, int16_t *z);
/* USER CODE END PFP */

/* Exported functions ---------------------------------------------------------*/
void SubghzApp_Init(void) {
	/* USER CODE BEGIN SubghzApp_Init_1 */

	/* USER CODE END SubghzApp_Init_1 */

	/* Radio initialization */
	RadioEvents.TxDone = OnTxDone;
	RadioEvents.RxDone = OnRxDone;
	RadioEvents.TxTimeout = OnTxTimeout;
	RadioEvents.RxTimeout = OnRxTimeout;
	RadioEvents.RxError = OnRxError;

	Radio.Init(&RadioEvents);

	/* USER CODE BEGIN SubghzApp_Init_2 */

	Radio.SetChannel(RF_FREQUENCY);
	Radio.SetTxConfig(MODEM_LORA,
	TX_OUTPUT_POWER, 0,
	LORA_BANDWIDTH,
	LORA_SPREADING_FACTOR,
	LORA_CODINGRATE,
	LORA_PREAMBLE_LENGTH,
	LORA_FIX_LENGTH_PAYLOAD_ON,
	true, // CRC on
			0,    // FreqHopOn
			0,    // HopPeriod
			LORA_IQ_INVERSION_ON, 3000  // TX timeout
			);

	Radio.SetPublicNetwork(false);

	UTIL_SEQ_RegTask(TASK_TX, 0, TxProcess);

	if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR17) != LORIOTA_BKP_MAGIC_VALUE) {
		HAL_RTCEx_BKUPWrite(&hrtc,
		RTC_BKP_DR18,
		LORIOTA_STAGE_PERIODIC);

		HAL_RTCEx_BKUPWrite(&hrtc,
		RTC_BKP_DR19, 0);

		HAL_RTCEx_BKUPWrite(&hrtc,
		RTC_BKP_DR17,
		LORIOTA_BKP_MAGIC_VALUE);
	}

	global_msg_counter = (uint16_t) HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR19);

	Radio.Sleep();

	if (loriota_wake_reason != LORIOTA_WAKE_UNEXPECTED) {
		UTIL_SEQ_SetTask(TASK_TX, CFG_SEQ_Prio_0);
	}
	/* USER CODE END SubghzApp_Init_2 */
}

/* USER CODE BEGIN EF */

/* USER CODE END EF */

/* Private functions ---------------------------------------------------------*/
static void OnTxDone(void) {
	/* USER CODE BEGIN OnTxDone */
	Radio.Sleep();

	tx_finished = true;
	UTIL_SEQ_SetTask(TASK_TX, CFG_SEQ_Prio_0);
	/* USER CODE END OnTxDone */
}

static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi,
		int8_t LoraSnr_FskCfo) {
	/* USER CODE BEGIN OnRxDone */
	Radio.Sleep();
	/* USER CODE END OnRxDone */
}

static void OnTxTimeout(void) {
	/* USER CODE BEGIN OnTxTimeout */
	Radio.Sleep();

	tx_finished = true;
	UTIL_SEQ_SetTask(TASK_TX, CFG_SEQ_Prio_0);
	/* USER CODE END OnTxTimeout */
}

static void OnRxTimeout(void) {
	/* USER CODE BEGIN OnRxTimeout */
	Radio.Sleep();
	/* USER CODE END OnRxTimeout */
}

static void OnRxError(void) {
	/* USER CODE BEGIN OnRxError */
	Radio.Sleep();
	/* USER CODE END OnRxError */
}

/* USER CODE BEGIN PrFD */

static void ADXL345_ClearInterrupt(void) {
	uint8_t int_source = 0;

	if (HAL_I2C_Mem_Read(&hi2c2, 0xA6, 0x30,
	I2C_MEMADD_SIZE_8BIT, &int_source, 1, 100) != HAL_OK) {
		Error_Handler();
	}
}

static void Loriota_SetRTCWakeup(uint32_t seconds, uint32_t next_stage) {
	HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR18, next_stage);

	if (HAL_RTCEx_DeactivateWakeUpTimer(&hrtc) != HAL_OK) {
		Error_Handler();
	}

	uint32_t bcdu = (RTC->ICSR & RTC_ICSR_BCDU) >> RTC_ICSR_BCDU_Pos;

	uint32_t divider = (hrtc.Init.AsynchPrediv + 1UL) * (1UL << (8UL + bcdu));

	uint32_t ticks =
			(uint32_t) (((uint64_t) seconds * LSE_VALUE + divider / 2UL)
					/ divider);

	if (ticks == 0UL) {
		ticks = 1UL;
	}

	if (ticks > 65536UL) {
		Error_Handler();
	}

	if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, ticks - 1UL,
	RTC_WAKEUPCLOCK_CK_SPRE_16BITS, 0UL) != HAL_OK) {
		Error_Handler();
	}
}

void Loriota_EnterShutdown(uint32_t seconds, uint32_t next_stage) {
	if (next_stage == LORIOTA_STAGE_PERIODIC) {
		for (uint8_t attempt = 0; attempt < 20; attempt++) {
			ADXL345_ClearInterrupt();

			if (HAL_GPIO_ReadPin(
			ADXL_INT1_GPIO_Port,
			ADXL_INT1_Pin) == GPIO_PIN_RESET) {
				break;
			}

			HAL_Delay(50);
		}
	}

	Radio.Sleep();

	Loriota_SetRTCWakeup(seconds, next_stage);

	if (HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A) != HAL_OK) {
		Error_Handler();
	}

	CLEAR_BIT(PWR->CR3, PWR_CR3_EWRFBUSY | PWR_CR3_EWRFIRQ);
	__HAL_PWR_CLEAR_FLAG(PWR_FLAG_WRFBUSY);

	HAL_NVIC_DisableIRQ(SUBGHZ_Radio_IRQn);
	HAL_NVIC_ClearPendingIRQ(SUBGHZ_Radio_IRQn);

	if ((RTC->CR & (RTC_CR_WUTE | RTC_CR_WUTIE))
			!= (RTC_CR_WUTE | RTC_CR_WUTIE)) {
		Error_Handler();
	}

	HAL_PWR_DisableWakeUpPin(PWR_WAKEUP_PIN1);
	__HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);

	if (next_stage == LORIOTA_STAGE_PERIODIC) {
		ADXL345_ClearInterrupt();

		if (HAL_GPIO_ReadPin(
		ADXL_INT1_GPIO_Port,
		ADXL_INT1_Pin) == GPIO_PIN_RESET) {
			HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1_HIGH);
		}
	}

	HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);

	HAL_PWR_EnableBkUpAccess();

	HAL_RTCEx_BKUPWrite(&hrtc,
	RTC_BKP_DR16,
	LORIOTA_SHUTDOWN_ARMED);

	HAL_NVIC_DisableIRQ(TIM17_IRQn);
	HAL_NVIC_ClearPendingIRQ(TIM17_IRQn);

	SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;

	__DSB();
	__ISB();

	HAL_PWREx_EnterSHUTDOWNMode();

	Error_Handler();
}

void Master_Radio_Send(void) {
	LoRaNodeData frame = { 0 };

	frame.node_id = MY_NODE_ID;
	frame.node_type = 0; // accelerometer only

	// battery measurement
	HAL_ADC_Start(&hadc);

	if (HAL_ADC_PollForConversion(&hadc, 100) == HAL_OK) {
		uint32_t raw_adc = HAL_ADC_GetValue(&hadc);

		uint32_t supply_mV = __HAL_ADC_CALC_VREFANALOG_VOLTAGE(raw_adc,
				ADC_RESOLUTION_12B);

		int32_t bat_percent = ((int32_t) supply_mV - 2200) * 100 / 800;

		if (bat_percent > 100)
			bat_percent = 100;

		if (bat_percent < 0)
			bat_percent = 0;

		frame.battery_lvl = (uint8_t) bat_percent;
	} else {
		frame.battery_lvl = 0;
	}

	HAL_ADC_Stop(&hadc);

	// read accelerometer
	Read_ADXL345(&frame.acc_x, &frame.acc_y, &frame.acc_z);

	if (loriota_wake_reason == LORIOTA_WAKE_ACCEL) {
		// vibration detected
		frame.sensor_state = 2;

		next_sleep_seconds = 10;
		next_sleep_stage = LORIOTA_STAGE_COOLDOWN;
	} else {
		// boot, periodic report or end of vibration
		frame.sensor_state = 0;

		// next periodic report after 15 minutes
		next_sleep_seconds = 900;
		next_sleep_stage = LORIOTA_STAGE_PERIODIC;
	}

	// restore and increment message counter
	frame.msg_counter = global_msg_counter++;

	HAL_RTCEx_BKUPWrite(&hrtc,
	RTC_BKP_DR19, global_msg_counter);

	HAL_GPIO_WritePin(
	GPIOB,
	GPIO_PIN_5, GPIO_PIN_SET);

	Radio.Send((uint8_t*) &frame, sizeof(LoRaNodeData));
}

static void TxProcess(void) {
	if (tx_finished) {
		tx_finished = false;

		Loriota_EnterShutdown(next_sleep_seconds, next_sleep_stage);
	} else {
		Master_Radio_Send();
	}
}

/* USER CODE END PrFD */
