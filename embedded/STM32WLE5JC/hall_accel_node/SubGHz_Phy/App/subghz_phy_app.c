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
#include <stdbool.h>
#include "utilities_def.h"
#include "stm32_lpm.h"
#include "stm32_seq.h"
#include "stm32_timer.h"
#include <stdio.h>
#include "lora_frame.h"
#include "rtc.h"
#include "main.h"
#include "i2c.h"
/* USER CODE END Includes */

/* External variables ---------------------------------------------------------*/
/* USER CODE BEGIN EV */
/* USER CODE BEGIN EV */

extern ADC_HandleTypeDef hadc;

extern bool loriota_was_standby;
extern bool loriota_woke_from_rtc;

/* USER CODE END EV */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define TX_OUTPUT_POWER                             2        /* dBm */
#define LORA_BANDWIDTH                              0         /* [0: 125 kHz, 1: 250 kHz, 2: 500 kHz, 3: Reserved] */
#define LORA_SPREADING_FACTOR                       7         /* [SF7..SF12] */
#define LORA_CODINGRATE                             1         /* [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8] */
#define LORA_PREAMBLE_LENGTH                        8         /* Same for Tx and Rx */
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false
#define TASK_TX 									(1 << CFG_SEQ_Task_TX)

#define HALL_POWER_GPIO_Port GPIOB
#define HALL_POWER_Pin       GPIO_PIN_3
#define STATE_CHANGE_DELAY   1000

#define HALL_POLL_INTERVAL_MS 10000U
#define TX_REPORT_INTERVAL_MS 900000U
#define REPORT_INTERVAL_POLLS 90U
#define LORIOTA_HALL_MAGIC 0x48414C4CUL
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* Radio events function pointer */
static RadioEvents_t RadioEvents;

/* USER CODE BEGIN PV */
static UTIL_TIMER_Object_t txTimer;
static UTIL_TIMER_Object_t vibrationTimer;
static UTIL_TIMER_Object_t hallPollTimer;

static volatile bool radio_tx_busy = false;

uint16_t global_msg_counter = 0;
const uint32_t MY_NODE_ID = 2222;

volatile bool wake_from_accel = false;
volatile bool wake_from_hall = false;
volatile bool wake_from_timer = false;
volatile bool wake_from_vibration_end = false;
volatile bool is_cooling_down = false;
volatile bool vibration_active = false;

static uint8_t last_hall_state = 0xFF;
static uint32_t state_change_time = 0;

static volatile bool tx_finished = false;
static uint32_t polls_until_report = REPORT_INTERVAL_POLLS;

extern bool loriota_was_standby;
extern bool loriota_rtc_preserved;
extern bool loriota_woke_from_rtc;
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
static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t LoraSnr_FskCfo);

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
static void TxTimerCallback(void *context);
static void VibrationTimerCallback(void *context);
static void TxProcess(void);
extern void Read_ADXL345(int16_t *x, int16_t *y, int16_t *z);
static void HallPollTimerCallback(void *context);
/* USER CODE END PFP */

/* Exported functions ---------------------------------------------------------*/
void SubghzApp_Init(void)
{
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

	UTIL_TIMER_Create(&txTimer,
	TX_REPORT_INTERVAL_MS, UTIL_TIMER_ONESHOT, TxTimerCallback,
	NULL);

	UTIL_TIMER_Create(&vibrationTimer, 10000, UTIL_TIMER_ONESHOT,
			VibrationTimerCallback,
			NULL);

	UTIL_TIMER_Create(&hallPollTimer,
	HALL_POLL_INTERVAL_MS, UTIL_TIMER_PERIODIC, HallPollTimerCallback,
	NULL);

	if (!loriota_was_standby
			|| HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR13) != LORIOTA_HALL_MAGIC) {

		last_hall_state = 0xFF;

		polls_until_report = REPORT_INTERVAL_POLLS;

		global_msg_counter = 0;

		vibration_active = false;

		wake_from_timer = true;
	} else {
		uint32_t saved_hall = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR19);

		last_hall_state = (saved_hall <= 1U) ? (uint8_t) saved_hall : 0xFF;

		polls_until_report = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR18);

		if (polls_until_report > REPORT_INTERVAL_POLLS) {
			polls_until_report = REPORT_INTERVAL_POLLS;
		}

		global_msg_counter = (uint16_t) HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR15);

		vibration_active = (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR14) == 1U);

		wake_from_hall = true;

		if (loriota_woke_from_rtc) {
			if (polls_until_report > 0U) {
				polls_until_report--;
			}

			if (polls_until_report == 0U) {
				wake_from_timer = true;
			}

			if (vibration_active) {
				wake_from_vibration_end = true;
			}
		}
	}

	UTIL_SEQ_SetTask(TASK_TX, CFG_SEQ_Prio_0);

	Radio.Sleep();
  /* USER CODE END SubghzApp_Init_2 */
}

/* USER CODE BEGIN EF */

/* USER CODE END EF */

/* Private functions ---------------------------------------------------------*/
static void OnTxDone(void)
{
  /* USER CODE BEGIN OnTxDone */

	Radio.Sleep();

	radio_tx_busy = false;

	tx_finished = true;

	UTIL_SEQ_SetTask(TASK_TX, CFG_SEQ_Prio_0);
  /* USER CODE END OnTxDone */
}

static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t LoraSnr_FskCfo)
{
  /* USER CODE BEGIN OnRxDone */
	Radio.Sleep();
  /* USER CODE END OnRxDone */
}

static void OnTxTimeout(void)
{
  /* USER CODE BEGIN OnTxTimeout */
	Radio.Sleep();

	radio_tx_busy = false;

	tx_finished = true;

	UTIL_SEQ_SetTask(TASK_TX, CFG_SEQ_Prio_0);
  /* USER CODE END OnTxTimeout */
}

static void OnRxTimeout(void)
{
  /* USER CODE BEGIN OnRxTimeout */
	Radio.Sleep();
  /* USER CODE END OnRxTimeout */
}

static void OnRxError(void)
{
  /* USER CODE BEGIN OnRxError */
  /* USER CODE END OnRxError */
}

/* USER CODE BEGIN PrFD */
static void HallSensor_On(void) {
	HAL_GPIO_WritePin(
	HALL_POWER_GPIO_Port,
	HALL_POWER_Pin, GPIO_PIN_SET);

	HAL_Delay(20);
}

static void HallSensor_Off(void) {
	HAL_GPIO_WritePin(
	HALL_POWER_GPIO_Port,
	HALL_POWER_Pin, GPIO_PIN_RESET);
}

static GPIO_PinState HallSensor_ReadStable(void) {
	uint8_t low_count = 0;
	uint8_t high_count = 0;

	HallSensor_On();

	for (uint8_t i = 0; i < 5; i++) {
		if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9) == GPIO_PIN_RESET)
			low_count++;
		else
			high_count++;

		HAL_Delay(5);
	}

	HallSensor_Off();

	return (low_count > high_count) ? GPIO_PIN_RESET : GPIO_PIN_SET;
}

void Master_Radio_Send(void) {
	if (radio_tx_busy) {
		return;
	}

	bool is_accel = wake_from_accel;
	bool is_hall_poll = wake_from_hall;
	bool is_timer = wake_from_timer;
	bool is_vib_end = wake_from_vibration_end;

	wake_from_accel = false;
	wake_from_hall = false;
	wake_from_timer = false;
	wake_from_vibration_end = false;

	if (!is_accel && !is_hall_poll && !is_timer && !is_vib_end) {
		return;
	}

	bool should_send = is_timer;

	const uint8_t VAL_CLOSED = 0;
	const uint8_t VAL_OPEN = 1;
	const uint8_t VAL_VIBRATION = 2;

	GPIO_PinState current_hall = HallSensor_ReadStable();

	bool is_door_closed = (current_hall == GPIO_PIN_RESET);

	bool hall_changed = (last_hall_state != 0xFF
			&& current_hall != (GPIO_PinState) last_hall_state);

	last_hall_state = (uint8_t) current_hall;

	if (hall_changed) {
		should_send = true;

		vibration_active = false;

		state_change_time = HAL_GetTick();
	}

	if (is_vib_end) {
		if (vibration_active) {
			should_send = true;
		}

		vibration_active = false;
	}

	if (!is_door_closed && vibration_active) {
		vibration_active = false;

	}

	if (is_accel && is_door_closed && !hall_changed) {
		if (!vibration_active) {
			should_send = true;
		}

		vibration_active = true;
	}

	int16_t acc_x = 0;
	int16_t acc_y = 0;
	int16_t acc_z = 0;

	if (is_accel || should_send) {
		Read_ADXL345(&acc_x, &acc_y, &acc_z);
	}

	if (!should_send) {
		return;
	}

	LoRaNodeData frame = { 0 };

	frame.node_id = MY_NODE_ID;
	frame.node_type = 2;

	frame.acc_x = acc_x;
	frame.acc_y = acc_y;
	frame.acc_z = acc_z;

	if (vibration_active && is_door_closed) {
		frame.sensor_state = VAL_VIBRATION;
	} else if (is_door_closed) {
		frame.sensor_state = VAL_CLOSED;
	} else {
		frame.sensor_state = VAL_OPEN;
	}

	frame.battery_lvl = 0;

	if (HAL_ADC_Start(&hadc) == HAL_OK) {
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
		}

		HAL_ADC_Stop(&hadc);
	}

	frame.msg_counter = global_msg_counter++;
	radio_tx_busy = true;
	Radio.Send((uint8_t*) &frame, sizeof(LoRaNodeData));
}

static void VibrationTimerCallback(void *context) {
	wake_from_vibration_end = true;
	UTIL_SEQ_SetTask(TASK_TX, CFG_SEQ_Prio_0);
}

static void TxTimerCallback(void *context) {
	wake_from_timer = true;
	UTIL_SEQ_SetTask(TASK_TX, CFG_SEQ_Prio_0);
}

static void Loriota_EnterStandby(void) {
	HallSensor_Off();

	Radio.Sleep();

	if (HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A) != HAL_OK) {
		Error_Handler();
	}

	if (HAL_RTCEx_DeactivateWakeUpTimer(&hrtc) != HAL_OK) {
		Error_Handler();
	}

	uint32_t bcdu = (RTC->ICSR & RTC_ICSR_BCDU) >> RTC_ICSR_BCDU_Pos;

	uint32_t divider = (hrtc.Init.AsynchPrediv + 1UL) * (1UL << (8UL + bcdu));

	uint32_t ticks = (uint32_t) (((uint64_t) 10UL * LSI_VALUE + divider / 2UL)
			/ divider);

	if (ticks == 0UL || ticks > 65536UL) {
		Error_Handler();
	}

	if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, ticks - 1UL,
	RTC_WAKEUPCLOCK_CK_SPRE_16BITS, 0UL) != HAL_OK) {
		Error_Handler();
	}

	HAL_PWR_DisableWakeUpPin(PWR_WAKEUP_PIN1);
	HAL_PWR_DisableWakeUpPin(PWR_WAKEUP_PIN2);
	HAL_PWR_DisableWakeUpPin(PWR_WAKEUP_PIN3);

	__HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);

	uint8_t int_source = 0;

	HAL_I2C_Mem_Read(&hi2c2, 0xA6, 0x30,
	I2C_MEMADD_SIZE_8BIT, &int_source, 1, 100);

	if (HAL_GPIO_ReadPin(
	ADXL_INT1_GPIO_Port,
	ADXL_INT1_Pin) == GPIO_PIN_RESET) {
		HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1_HIGH);
	}

	CLEAR_BIT(PWR->CR3, PWR_CR3_EWRFBUSY | PWR_CR3_EWRFIRQ);
	__HAL_PWR_CLEAR_FLAG(PWR_FLAG_WRFBUSY);

	HAL_NVIC_DisableIRQ(SUBGHZ_Radio_IRQn);
	HAL_NVIC_ClearPendingIRQ(SUBGHZ_Radio_IRQn);

	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);

	HAL_PWR_EnableBkUpAccess();

	HAL_NVIC_DisableIRQ(TIM17_IRQn);
	HAL_NVIC_ClearPendingIRQ(TIM17_IRQn);

	SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;

	__DSB();
	__ISB();

	HAL_PWR_EnterSTANDBYMode();

	Error_Handler();
}

static void Loriota_SaveBackupState(void) {
	HAL_PWR_EnableBkUpAccess();

	HAL_RTCEx_BKUPWrite(&hrtc,
	RTC_BKP_DR13,
	LORIOTA_HALL_MAGIC);

	HAL_RTCEx_BKUPWrite(&hrtc,
	RTC_BKP_DR14, vibration_active ? 1U : 0U);

	HAL_RTCEx_BKUPWrite(&hrtc,
	RTC_BKP_DR15, global_msg_counter);

	HAL_RTCEx_BKUPWrite(&hrtc,
	RTC_BKP_DR18, polls_until_report);

	HAL_RTCEx_BKUPWrite(&hrtc,
	RTC_BKP_DR19, last_hall_state);
}

static void TxProcess(void) {
	if (tx_finished) {
		tx_finished = false;

		polls_until_report = REPORT_INTERVAL_POLLS;

		Loriota_SaveBackupState();

		Loriota_EnterStandby();
		return;
	}

	Master_Radio_Send();

	Loriota_SaveBackupState();

	if (!radio_tx_busy && !tx_finished) {
		Loriota_EnterStandby();
	}
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if (GPIO_Pin == ADXL_INT1_Pin) {
		wake_from_accel = true;
		UTIL_SEQ_SetTask(TASK_TX, CFG_SEQ_Prio_0);
	}
}

static void HallPollTimerCallback(void *context) {
	(void) context;

	wake_from_hall = true;

	UTIL_SEQ_SetTask(TASK_TX, CFG_SEQ_Prio_0);
}
/* USER CODE END PrFD */
