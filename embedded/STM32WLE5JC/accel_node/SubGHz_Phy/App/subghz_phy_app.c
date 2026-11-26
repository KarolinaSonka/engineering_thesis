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
#include <stdbool.h>
#include <stdint.h>
#include "utilities_def.h"
#include "stm32_seq.h"
#include "lora_frame.h"
#include "main.h"
#include "rtc.h"
#include "adxl345.h"
#include "power_monitor.h"
#include "duty_cycle.h"
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

#define TASK_TX 				(1 << CFG_SEQ_Task_TX)
#define LORIOTA_MIN_TX_VDD_MV    2400UL
#define LORIOTA_COOLDOWN_SECONDS 10UL
#define LORIOTA_REPORT_SECONDS   900UL
#define ACCEL_NODE_TYPE          0U
#define ACCEL_STATE_NORMAL       0U
#define ACCEL_STATE_ALERT        2U
#define BATTERY_PERCENT_BASE_MV  2200L
#define BATTERY_PERCENT_RANGE_MV 800L
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
static uint32_t next_sleep_seconds = LORIOTA_REPORT_SECONDS;
static uint32_t next_sleep_stage = LORIOTA_STAGE_PERIODIC;
static volatile bool tx_done_ok = false;
static bool tx_was_alert = false;
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
static bool ReadBattery(uint32_t *supply_mV, uint8_t *battery_level);
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

	DutyCycle_Init();

	UTIL_SEQ_RegTask(TASK_TX, 0, TxProcess);

	if (HAL_RTCEx_BKUPRead(&hrtc,
	RTC_BKP_DR17) != LORIOTA_BACKUP_STATE_SIGNATURE) {
		HAL_RTCEx_BKUPWrite(&hrtc,
		RTC_BKP_DR18,
		LORIOTA_STAGE_PERIODIC);

		HAL_RTCEx_BKUPWrite(&hrtc,
		RTC_BKP_DR19, 0);

		HAL_RTCEx_BKUPWrite(&hrtc,
		RTC_BKP_DR17,
		LORIOTA_BACKUP_STATE_SIGNATURE);
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
	tx_done_ok = true;
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
	tx_done_ok = false;
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
static bool ReadBattery(uint32_t *supply_mV, uint8_t *battery_level) {
	*supply_mV = 0UL;
	*battery_level = 0U;

	bool battery_adc_ok = false;

	if (HAL_ADC_Start(&hadc) == HAL_OK) {
		if (HAL_ADC_PollForConversion(&hadc, 100U) == HAL_OK) {
			uint32_t raw_adc = HAL_ADC_GetValue(&hadc);
			*supply_mV = __HAL_ADC_CALC_VREFANALOG_VOLTAGE(raw_adc,
					ADC_RESOLUTION_12B);

			int32_t percent = ((int32_t) *supply_mV - BATTERY_PERCENT_BASE_MV)
					* 100L / BATTERY_PERCENT_RANGE_MV;

			if (percent > 100L) {
				percent = 100L;
			}
			if (percent < 0L) {
				percent = 0L;
			}

			*battery_level = (uint8_t) percent;
			battery_adc_ok = true;
		}

		HAL_ADC_Stop(&hadc);
	}

	return battery_adc_ok;
}

static void SetRTCWakeup(uint32_t seconds, uint32_t next_stage) {
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

void EnterShutdown(uint32_t seconds, uint32_t next_stage) {
	if (next_stage == LORIOTA_STAGE_PERIODIC) {
		for (uint8_t attempt = 0; attempt < 20; attempt++) {
			if (ADXL345_ClearInterrupt() != HAL_OK) {
				Error_Handler();
			}

			if (HAL_GPIO_ReadPin(
			ADXL_INT1_GPIO_Port,
			ADXL_INT1_Pin) == GPIO_PIN_RESET) {
				break;
			}

			HAL_Delay(50);
		}
	}

	Radio.Sleep();

	SetRTCWakeup(seconds, next_stage);

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
		if (ADXL345_ClearInterrupt() != HAL_OK) {
			Error_Handler();
		}

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
	if (loriota_wake_reason == LORIOTA_WAKE_ACCEL) {
		DutyCycle_SetPending(true);
	}

	uint32_t airtime = DutyCycle_AirtimeMs();

	if (airtime == 0UL || airtime > DUTY_CYCLE_MAX_TX_MS) {
		EnterShutdown(LORIOTA_REPORT_SECONDS,
		LORIOTA_STAGE_PERIODIC);

		return;
	}

	uint32_t wait = DutyCycle_WaitSeconds(airtime);

	if (wait > 0UL) {
		EnterShutdown(wait + 1UL,
		LORIOTA_STAGE_PERIODIC);

		return;
	}

	LoRaNodeData frame = { 0 };

	frame.node_id = MY_NODE_ID;
	frame.node_type = ACCEL_NODE_TYPE;

	uint32_t supply_mV = 0UL;
	bool battery_adc_ok = ReadBattery(&supply_mV, &frame.battery_lvl);

	ADXL345_ReadXYZ(&frame.acc_x, &frame.acc_y, &frame.acc_z);

	if (loriota_wake_reason == LORIOTA_WAKE_ACCEL || DutyCycle_IsPending()) {
		frame.sensor_state = ACCEL_STATE_ALERT;

		next_sleep_seconds = LORIOTA_COOLDOWN_SECONDS;
		next_sleep_stage = LORIOTA_STAGE_COOLDOWN;
	} else {
		frame.sensor_state = ACCEL_STATE_NORMAL;

		next_sleep_seconds = LORIOTA_REPORT_SECONDS;
		next_sleep_stage = LORIOTA_STAGE_PERIODIC;
	}

	if (!battery_adc_ok || supply_mV < LORIOTA_MIN_TX_VDD_MV || IsSupplyLow()) {
		Radio.Sleep();

		EnterShutdown(next_sleep_seconds, next_sleep_stage);

		return;
	}

	if (!DutyCycle_Reserve(airtime)) {
		uint32_t retry_wait = DutyCycle_WaitSeconds(airtime);

		EnterShutdown(retry_wait + 1UL,
		LORIOTA_STAGE_PERIODIC);

		return;
	}

	frame.msg_counter = global_msg_counter++;

	HAL_RTCEx_BKUPWrite(&hrtc,
	RTC_BKP_DR19, global_msg_counter);

	tx_was_alert = (frame.sensor_state == ACCEL_STATE_ALERT);
	tx_done_ok = false;

	HAL_GPIO_WritePin(
	GPIOB,
	GPIO_PIN_5, GPIO_PIN_SET);

	Radio.Send((uint8_t*) &frame, sizeof(LoRaNodeData));
}

static void TxProcess(void) {
	if (tx_finished) {
		tx_finished = false;

		if (tx_done_ok) {
			if (tx_was_alert) {
				DutyCycle_SetPending(false);
			}

			DutyCycle_CompleteTx();

			EnterShutdown(next_sleep_seconds, next_sleep_stage);
		} else {
			DutyCycle_ChargeTimeout();

			uint32_t airtime = DutyCycle_AirtimeMs();
			uint32_t wait = 0UL;

			if (airtime != 0UL && airtime <= DUTY_CYCLE_MAX_TX_MS) {
				wait = DutyCycle_WaitSeconds(airtime);
			}

			if (wait < LORIOTA_COOLDOWN_SECONDS) {
				wait = LORIOTA_COOLDOWN_SECONDS;
			}

			EnterShutdown(wait + 1UL,
			LORIOTA_STAGE_PERIODIC);
		}

		return;
	}

	Master_Radio_Send();
}

/* USER CODE END PrFD */
