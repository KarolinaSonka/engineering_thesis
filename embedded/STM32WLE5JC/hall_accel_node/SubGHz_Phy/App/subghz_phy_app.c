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

extern bool loriota_was_shutdown;
extern bool loriota_woke_from_rtc;
extern bool loriota_rtc_preserved;

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

#define HALL_POWER_GPIO_Port GPIOB
#define HALL_POWER_Pin       GPIO_PIN_3
#define STATE_CHANGE_DELAY   1000

#define HALL_POLL_INTERVAL_MS 10000U
#define TX_REPORT_INTERVAL_MS 900000U
#define REPORT_INTERVAL_POLLS 90U
#define LORIOTA_HALL_MAGIC 0x48414C4CUL

#define DUTY_MAGIC              0x4454594AUL
#define DUTY_PREVIOUS_MAGIC     0x44545948UL
#define DUTY_HZ                 1024UL
#define DUTY_MAX_TX_MS          2500UL
#define DUTY_TX_TIMEOUT_MS      3000UL
#define DUTY_MAX_CREDIT_TICKS   6144L
#define DUTY_REFILL_DIV         172UL

#define DUTY_BKP_LAST           RTC_BKP_DR9
#define DUTY_BKP_FLAGS          RTC_BKP_DR10
#define DUTY_BKP_CREDITS        RTC_BKP_DR11
#define DUTY_BKP_MAGIC          RTC_BKP_DR12

#define DUTY_EVT_OPEN     (1UL << 0)
#define DUTY_EVT_VIB      (1UL << 1)
#define DUTY_EVT_STATE    (1UL << 2)
#define DUTY_EVT_REPORT   (1UL << 3)
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
static volatile bool tx_done_ok = false;
static uint32_t duty_event_in_flight = 0UL;
static uint32_t tx_airtime_ticks = 0UL;
static uint32_t polls_until_report = REPORT_INTERVAL_POLLS;

extern bool loriota_was_shutdown;
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
static void TxTimerCallback(void *context);
static void VibrationTimerCallback(void *context);
static void TxProcess(void);
extern void Read_ADXL345(int16_t *x, int16_t *y, int16_t *z);
static void HallPollTimerCallback(void *context);
static void DutyInit(void);
static void DutyUpdate(void);
static uint32_t DutyGetFlags(void);
static void DutySetFlags(uint32_t flags);
static uint32_t DutyAirtimeMs(void);
static uint32_t DutyWaitSeconds(uint32_t airtime_ms);
static bool DutyReserve(uint32_t airtime_ms);
static void DutyChargeTimeout(void);
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

	DutyInit();

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

	if (!loriota_was_shutdown
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
static void OnTxDone(void) {
	/* USER CODE BEGIN OnTxDone */
	Radio.Sleep();

	radio_tx_busy = false;

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

	radio_tx_busy = false;

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
	/* USER CODE END OnRxError */
}

/* USER CODE BEGIN PrFD */
static uint32_t DutyNow(void) {
	uint32_t ssr = RTC->SSR;
	(void) RTC->DR;
	return ~ssr;
}

static uint32_t DutyAirtimeTicks(uint32_t airtime_ms) {
	return (uint32_t) (((uint64_t) airtime_ms * DUTY_HZ + 999ULL) / 1000ULL);
}

static uint32_t DutyGetFlags(void) {
	return HAL_RTCEx_BKUPRead(&hrtc, DUTY_BKP_FLAGS);
}

static void DutySetFlags(uint32_t flags) {
	HAL_RTCEx_BKUPWrite(&hrtc, DUTY_BKP_FLAGS, flags);
}

static void DutyInit(void) {
	HAL_PWR_EnableBkUpAccess();

	uint32_t previous_magic = HAL_RTCEx_BKUPRead(&hrtc, DUTY_BKP_MAGIC);

	if (!loriota_rtc_preserved || previous_magic != DUTY_MAGIC) {
		uint32_t pending = 0UL;

		if (loriota_rtc_preserved && previous_magic == DUTY_PREVIOUS_MAGIC) {
			pending = DutyGetFlags();
		}

		DutySetFlags(pending);

		HAL_RTCEx_BKUPWrite(&hrtc,
		DUTY_BKP_CREDITS, (uint32_t) DUTY_MAX_CREDIT_TICKS);

		HAL_RTCEx_BKUPWrite(&hrtc,
		DUTY_BKP_LAST, DutyNow());

		HAL_RTCEx_BKUPWrite(&hrtc,
		DUTY_BKP_MAGIC,
		DUTY_MAGIC);
	}
}

static void DutyUpdate(void) {
	uint32_t now = DutyNow();

	uint32_t last = HAL_RTCEx_BKUPRead(&hrtc, DUTY_BKP_LAST);

	uint32_t elapsed = now - last;

	uint32_t refill = elapsed / DUTY_REFILL_DIV;

	if (refill == 0UL) {
		return;
	}

	int32_t credits = (int32_t) HAL_RTCEx_BKUPRead(&hrtc, DUTY_BKP_CREDITS);

	int64_t updated = (int64_t) credits + (int64_t) refill;

	if (updated > DUTY_MAX_CREDIT_TICKS) {
		updated = DUTY_MAX_CREDIT_TICKS;
	}

	HAL_RTCEx_BKUPWrite(&hrtc,
	DUTY_BKP_CREDITS, (uint32_t) (int32_t) updated);

	HAL_RTCEx_BKUPWrite(&hrtc,
	DUTY_BKP_LAST, last + refill * DUTY_REFILL_DIV);
}

static uint32_t DutyAirtimeMs(void) {
	return Radio.TimeOnAir(MODEM_LORA,
	LORA_BANDWIDTH,
	LORA_SPREADING_FACTOR,
	LORA_CODINGRATE,
	LORA_PREAMBLE_LENGTH,
	LORA_FIX_LENGTH_PAYLOAD_ON, (uint8_t) sizeof(LoRaNodeData),
	true);
}

static uint32_t DutyWaitSeconds(uint32_t airtime_ms) {
	DutyUpdate();

	int32_t credits = (int32_t) HAL_RTCEx_BKUPRead(&hrtc, DUTY_BKP_CREDITS);

	int64_t missing = (int64_t) DutyAirtimeTicks(airtime_ms) - credits;

	if (missing <= 0) {
		return 0UL;
	}

	uint64_t wait_ticks = (uint64_t) missing * DUTY_REFILL_DIV;

	return (uint32_t) ((wait_ticks + DUTY_HZ - 1UL) / DUTY_HZ);
}

static bool DutyReserve(uint32_t airtime_ms) {
	DutyUpdate();

	int32_t credits = (int32_t) HAL_RTCEx_BKUPRead(&hrtc, DUTY_BKP_CREDITS);

	uint32_t cost = DutyAirtimeTicks(airtime_ms);

	if (credits < (int32_t) cost) {
		return false;
	}

	HAL_RTCEx_BKUPWrite(&hrtc,
	DUTY_BKP_CREDITS, (uint32_t) (credits - (int32_t) cost));

	tx_airtime_ticks = cost;

	return true;
}

static void DutyChargeTimeout(void) {
	DutyUpdate();

	uint32_t timeout_ticks = DutyAirtimeTicks(DUTY_TX_TIMEOUT_MS);

	if (timeout_ticks > tx_airtime_ticks) {
		int32_t credits = (int32_t) HAL_RTCEx_BKUPRead(&hrtc,
		DUTY_BKP_CREDITS);

		int32_t extra = (int32_t) (timeout_ticks - tx_airtime_ticks);

		HAL_RTCEx_BKUPWrite(&hrtc,
		DUTY_BKP_CREDITS, (uint32_t) (credits - extra));
	}

	tx_airtime_ticks = 0UL;
}

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
	if (radio_tx_busy)
		return;

	bool is_accel = wake_from_accel;
	bool is_hall_poll = wake_from_hall;
	bool is_timer = wake_from_timer;
	bool is_vib_end = wake_from_vibration_end;

	wake_from_accel = false;
	wake_from_hall = false;
	wake_from_timer = false;
	wake_from_vibration_end = false;

	if (!is_accel && !is_hall_poll && !is_timer && !is_vib_end)
		return;

	GPIO_PinState current_hall = HallSensor_ReadStable();

	bool is_door_closed = (current_hall == GPIO_PIN_RESET);

	bool hall_changed = (last_hall_state != 0xFF
			&& current_hall != (GPIO_PinState) last_hall_state);

	last_hall_state = (uint8_t) current_hall;

	uint32_t flags = DutyGetFlags();

	if (is_timer) {
		flags |= DUTY_EVT_REPORT;
	}

	if (hall_changed) {
		flags |= is_door_closed ? DUTY_EVT_STATE : DUTY_EVT_OPEN;

		vibration_active = false;

		state_change_time = HAL_GetTick();
	}

	if (is_vib_end) {
		if (vibration_active) {
			flags |= DUTY_EVT_STATE;
		}

		vibration_active = false;
	}

	if (!is_door_closed && vibration_active) {
		vibration_active = false;
	}

	if (is_accel && is_door_closed && !hall_changed) {
		if (!vibration_active) {
			flags |= DUTY_EVT_VIB;
		}

		vibration_active = true;
	}

	DutySetFlags(flags);

	if (flags == 0UL) {
		return;
	}

	uint32_t airtime = DutyAirtimeMs();

	if (airtime == 0UL || airtime > DUTY_MAX_TX_MS) {
		return;
	}

	if (DutyWaitSeconds(airtime) != 0UL) {
		return;
	}

	LoRaNodeData frame = { 0 };

	frame.node_id = MY_NODE_ID;
	frame.node_type = 2;

	if ((flags & DUTY_EVT_OPEN) != 0UL) {
		frame.sensor_state = 1U;

		duty_event_in_flight = DUTY_EVT_OPEN;
	} else if ((flags & DUTY_EVT_VIB) != 0UL) {
		frame.sensor_state = 2U;

		duty_event_in_flight = DUTY_EVT_VIB;
	} else if ((flags & DUTY_EVT_STATE) != 0UL) {
		frame.sensor_state = is_door_closed ? 0U : 1U;

		duty_event_in_flight = DUTY_EVT_STATE;
	} else {
		frame.sensor_state =
				(vibration_active && is_door_closed) ?
						2U : (is_door_closed ? 0U : 1U);

		duty_event_in_flight = DUTY_EVT_REPORT;
	}

	Read_ADXL345(&frame.acc_x, &frame.acc_y, &frame.acc_z);

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

	if (!DutyReserve(airtime)) {
		return;
	}

	frame.msg_counter = global_msg_counter++;

	tx_done_ok = false;
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

static void Loriota_EnterShutdown(void) {
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

	uint32_t ticks = (uint32_t) (((uint64_t) 10UL * LSE_VALUE + divider / 2UL)
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

		if (tx_done_ok) {
			uint32_t flags = DutyGetFlags();

			flags &= ~(duty_event_in_flight |
			DUTY_EVT_REPORT);

			DutySetFlags(flags);

			polls_until_report = REPORT_INTERVAL_POLLS;
		} else {
			DutyChargeTimeout();
		}

		tx_airtime_ticks = 0UL;
		duty_event_in_flight = 0UL;

		Loriota_SaveBackupState();
		Loriota_EnterShutdown();

		return;
	}

	Master_Radio_Send();

	Loriota_SaveBackupState();

	if (!radio_tx_busy && !tx_finished) {
		Loriota_EnterShutdown();
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
