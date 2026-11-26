#include "duty_cycle.h"
#include "main.h"
#include "radio.h"
#include "rtc.h"
#include "subghz_phy_app.h"
#include "lora_frame.h"

#define DUTY_STATE_SIGNATURE    	0x44545942UL
#define DUTY_LEGACY_STATE_SIGNATURE 0x44545932UL
#define DUTY_TX_TIMEOUT_MS     		3000UL

#define DUTY_HZ                 	1024UL
#define DUTY_MAX_CREDIT_TICKS   	6144L
#define DUTY_REFILL_DIV         	172UL

#define DUTY_BKP_LAST           	RTC_BKP_DR12
#define DUTY_BKP_PENDING        	RTC_BKP_DR13
#define DUTY_BKP_CREDITS        	RTC_BKP_DR14
#define DUTY_BKP_SIGNATURE_REGISTER RTC_BKP_DR15

static uint32_t tx_airtime_ticks = 0UL;

static uint32_t DutyNow(void) {
	uint32_t ssr = RTC->SSR;
	(void) RTC->DR;

	return ~ssr;
}

static uint32_t DutyAirtimeTicks(uint32_t airtime_ms) {
	return (uint32_t) (((uint64_t) airtime_ms * DUTY_HZ + 999ULL) / 1000ULL);
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
	int64_t updated = (int64_t) credits + refill;

	if (updated > DUTY_MAX_CREDIT_TICKS) {
		updated = DUTY_MAX_CREDIT_TICKS;
	}

	HAL_RTCEx_BKUPWrite(&hrtc, DUTY_BKP_CREDITS, (uint32_t) (int32_t) updated);
	HAL_RTCEx_BKUPWrite(&hrtc, DUTY_BKP_LAST, last + refill * DUTY_REFILL_DIV);
}

void DutyCycle_Init(void) {
	HAL_PWR_EnableBkUpAccess();

	uint32_t backup_signature = HAL_RTCEx_BKUPRead(&hrtc,
	DUTY_BKP_SIGNATURE_REGISTER);
	bool app_state_valid = HAL_RTCEx_BKUPRead(&hrtc,
	RTC_BKP_DR17) == LORIOTA_BACKUP_STATE_SIGNATURE;

	if (!app_state_valid || backup_signature != DUTY_STATE_SIGNATURE) {
		uint32_t pending = 0UL;

		if (app_state_valid && backup_signature == DUTY_LEGACY_STATE_SIGNATURE) {
			pending = HAL_RTCEx_BKUPRead(&hrtc, DUTY_BKP_PENDING) & 1UL;
		}

		HAL_RTCEx_BKUPWrite(&hrtc, DUTY_BKP_PENDING, pending);
		HAL_RTCEx_BKUPWrite(&hrtc, DUTY_BKP_CREDITS,
				(uint32_t) DUTY_MAX_CREDIT_TICKS);
		HAL_RTCEx_BKUPWrite(&hrtc, DUTY_BKP_LAST, DutyNow());
		HAL_RTCEx_BKUPWrite(&hrtc, DUTY_BKP_SIGNATURE_REGISTER,
		DUTY_STATE_SIGNATURE);
	}
}

uint32_t DutyCycle_AirtimeMs(void) {
	return Radio.TimeOnAir(MODEM_LORA,
	LORA_BANDWIDTH,
	LORA_SPREADING_FACTOR,
	LORA_CODINGRATE,
	LORA_PREAMBLE_LENGTH,
	LORA_FIX_LENGTH_PAYLOAD_ON, (uint8_t) sizeof(LoRaNodeData),
	true);
}

uint32_t DutyCycle_WaitSeconds(uint32_t airtime_ms) {
	DutyUpdate();

	int32_t credits = (int32_t) HAL_RTCEx_BKUPRead(&hrtc, DUTY_BKP_CREDITS);
	int64_t missing = (int64_t) DutyAirtimeTicks(airtime_ms) - credits;

	if (missing <= 0) {
		return 0UL;
	}

	uint64_t wait_ticks = (uint64_t) missing * DUTY_REFILL_DIV;

	return (uint32_t) ((wait_ticks + DUTY_HZ - 1UL) / DUTY_HZ);
}

bool DutyCycle_Reserve(uint32_t airtime_ms) {
	DutyUpdate();

	int32_t credits = (int32_t) HAL_RTCEx_BKUPRead(&hrtc, DUTY_BKP_CREDITS);
	uint32_t required = DutyAirtimeTicks(airtime_ms);

	if (credits < (int32_t) required) {
		return false;
	}

	HAL_RTCEx_BKUPWrite(&hrtc, DUTY_BKP_CREDITS,
			(uint32_t) (credits - (int32_t) required));

	tx_airtime_ticks = required;
	return true;
}

void DutyCycle_ChargeTimeout(void) {
	DutyUpdate();

	uint32_t timeout_ticks = DutyAirtimeTicks(DUTY_TX_TIMEOUT_MS);

	if (timeout_ticks > tx_airtime_ticks) {
		int32_t credits = (int32_t) HAL_RTCEx_BKUPRead(&hrtc, DUTY_BKP_CREDITS);
		credits -= (int32_t) (timeout_ticks - tx_airtime_ticks);

		HAL_RTCEx_BKUPWrite(&hrtc, DUTY_BKP_CREDITS, (uint32_t) credits);
	}

	DutyCycle_CompleteTx();
}

void DutyCycle_CompleteTx(void) {
	tx_airtime_ticks = 0UL;
}

void DutyCycle_SetPending(bool pending) {
	HAL_RTCEx_BKUPWrite(&hrtc, DUTY_BKP_PENDING, pending ? 1UL : 0UL);
}

bool DutyCycle_IsPending(void) {
	return HAL_RTCEx_BKUPRead(&hrtc, DUTY_BKP_PENDING) != 0UL;
}
