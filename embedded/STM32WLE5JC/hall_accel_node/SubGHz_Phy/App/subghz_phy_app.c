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
#include "usart.h"
#include "stm32_seq.h"
#include "stm32_timer.h"
#include <stdio.h>
#include "lora_frame.h"
/* USER CODE END Includes */

/* External variables ---------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define TX_OUTPUT_POWER                             14        /* dBm */
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
static UTIL_TIMER_Object_t txTimer;
static UTIL_TIMER_Object_t vibrationTimer;
uint16_t global_msg_counter = 0;
const uint32_t MY_NODE_ID = 2222;

volatile bool wake_from_accel = false;
volatile bool wake_from_hall = false;
volatile bool wake_from_timer = false;
volatile bool wake_from_vibration_end = false;
volatile bool is_cooling_down = false;
volatile bool vibration_active = false;

static uint8_t last_hall_state = 0xFF;
static uint32_t last_hall_time = 0;
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

	UTIL_TIMER_Create(&txTimer, 180000, UTIL_TIMER_ONESHOT, TxTimerCallback,
	NULL); // 3 minutes
	UTIL_TIMER_Create(&vibrationTimer, 10000, UTIL_TIMER_ONESHOT,
			VibrationTimerCallback, NULL);

	UTIL_TIMER_Start(&txTimer);

	wake_from_timer = true;
	UTIL_SEQ_SetTask(TASK_TX, CFG_SEQ_Prio_0);
	/* USER CODE END SubghzApp_Init_2 */
}

/* USER CODE BEGIN EF */

/* USER CODE END EF */

/* Private functions ---------------------------------------------------------*/
static void OnTxDone(void) {
	/* USER CODE BEGIN OnTxDone */
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);

	UTIL_TIMER_Start(&txTimer);
	/* USER CODE END OnTxDone */
}

static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi,
		int8_t LoraSnr_FskCfo) {
	/* USER CODE BEGIN OnRxDone */
	/* USER CODE END OnRxDone */
}

static void OnTxTimeout(void) {
	/* USER CODE BEGIN OnTxTimeout */
	/* USER CODE END OnTxTimeout */
}

static void OnRxTimeout(void) {
	/* USER CODE BEGIN OnRxTimeout */
	/* USER CODE END OnRxTimeout */
}

static void OnRxError(void) {
	/* USER CODE BEGIN OnRxError */
	/* USER CODE END OnRxError */
}

/* USER CODE BEGIN PrFD */
void Master_Radio_Send(void) {
	LoRaNodeData frame = { 0 };
	bool should_send = false;

	bool is_accel = wake_from_accel;
	bool is_hall = wake_from_hall;
	bool is_timer = wake_from_timer;
	bool is_vib_end = wake_from_vibration_end;

	wake_from_accel = false;
	wake_from_hall = false;
	wake_from_timer = false;
	wake_from_vibration_end = false;

	frame.node_id = MY_NODE_ID;
	frame.node_type = 2; // hall switch + accel
	frame.battery_lvl = 95;

	Read_ADXL345(&frame.acc_x, &frame.acc_y, &frame.acc_z);

	uint8_t current_hall = HAL_GPIO_ReadPin(HALL_GPIO_Port, HALL_Pin);

	uint8_t VAL_CLOSED = 0;    // magnet
	uint8_t VAL_OPEN = 1;      // no magnet
	uint8_t VAL_VIBRATION = 2; // vibrations

	bool is_door_closed = (current_hall == GPIO_PIN_RESET);

	// routine state report
	if (is_timer) {
		last_hall_state = current_hall;
		should_send = true;
	}

	// no vibrations after 10 seconds
	if (is_vib_end) {
		vibration_active = false;
		if (is_door_closed) {
			should_send = true;
		}
	}

	// hall switch changed state
	if (is_hall) {
		uint32_t now = HAL_GetTick();
		if (now - last_hall_time > 500) {
			if (current_hall != last_hall_state) {
				last_hall_state = current_hall;
				last_hall_time = now;
				should_send = true;
				if (!is_door_closed) {
					vibration_active = false;
				}
			}
		}
	}

	// vibrations
	if (is_accel) {
		if (is_door_closed) {
			if (vibration_active == false) {
				should_send = true;
			}

			vibration_active = true;
			UTIL_TIMER_Start(&vibrationTimer);
		}
	}

	if (should_send) {
		if (is_door_closed) {
			// closed
			if (vibration_active) {
				frame.sensor_state = VAL_VIBRATION; // vibration alert
			} else {
				frame.sensor_state = VAL_CLOSED;    // safe
			}
		} else {
			// open
			frame.sensor_state = VAL_OPEN;          // unsafe
		}
		frame.msg_counter = global_msg_counter++;
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
		Radio.Send((uint8_t*) &frame, sizeof(LoRaNodeData));
	}
}

static void VibrationTimerCallback(void *context) {
	wake_from_vibration_end = true;
	UTIL_SEQ_SetTask(TASK_TX, CFG_SEQ_Prio_0);
}

static void TxTimerCallback(void *context) {
	wake_from_timer = true;
	UTIL_SEQ_SetTask(TASK_TX, CFG_SEQ_Prio_0);
}

static void TxProcess(void) {
	Master_Radio_Send();
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if (GPIO_Pin == ADXL_INT1_Pin) {
		wake_from_accel = true;
		UTIL_SEQ_SetTask(TASK_TX, CFG_SEQ_Prio_0);
	} else if (GPIO_Pin == HALL_Pin) {
		wake_from_hall = true;
		UTIL_SEQ_SetTask(TASK_TX, CFG_SEQ_Prio_0);
	}
}
/* USER CODE END PrFD */
