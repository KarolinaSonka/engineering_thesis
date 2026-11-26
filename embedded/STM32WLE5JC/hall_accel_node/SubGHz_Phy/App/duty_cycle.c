#include "duty_cycle.h"
#include "lora_frame.h"
#include "main.h"
#include "radio.h"
#include "rtc.h"
#include "subghz_phy_app.h"

#define DUTY_STATE_SIGNATURE    	0x4454594AUL
#define DUTY_LEGACY_STATE_SIGNATURE 0x44545948UL
#define DUTY_TX_TIMEOUT_MS  		3000UL

#define DUTY_HZ               		1024UL
#define DUTY_MAX_CREDIT_TICKS		6144L
#define DUTY_REFILL_DIV       		172UL

#define DUTY_BKP_LAST    			RTC_BKP_DR9
#define DUTY_BKP_FLAGS   			RTC_BKP_DR10
#define DUTY_BKP_CREDITS 			RTC_BKP_DR11
#define DUTY_BKP_SIGNATURE_REGISTER RTC_BKP_DR12

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
	int64_t updated = (int64_t) credits + (int64_t) refill;

	if (updated > DUTY_MAX_CREDIT_TICKS) {
		updated = DUTY_MAX_CREDIT_TICKS;
	}

	HAL_RTCEx_BKUPWrite(&hrtc, DUTY_BKP_CREDITS, (uint32_t) (int32_t) updated);
	HAL_RTCEx_BKUPWrite(&hrtc, DUTY_BKP_LAST, last + refill * DUTY_REFILL_DIV);
}

uint32_t DutyCycle_GetFlags(void) {
	return HAL_RTCEx_BKUPRead(&hrtc, DUTY_BKP_FLAGS);
}

void DutyCycle_SetFlags(uint32_t flags) {
	HAL_RTCEx_BKUPWrite(&hrtc, DUTY_BKP_FLAGS, flags);
}

void DutyCycle_Init(bool rtc_preserved) {
	HAL_PWR_EnableBkUpAccess();

	uint32_t backup_signature = HAL_RTCEx_BKUPRead(&hrtc,
	DUTY_BKP_SIGNATURE_REGISTER);

	if (!rtc_preserved || backup_signature != DUTY_STATE_SIGNATURE) {
		uint32_t pending = 0UL;

		if (rtc_preserved && backup_signature == DUTY_LEGACY_STATE_SIGNATURE) {
			pending = DutyCycle_GetFlags();
		}

		DutyCycle_SetFlags(pending);
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
	uint32_t cost = DutyAirtimeTicks(airtime_ms);

	if (credits < (int32_t) cost) {
		return false;
	}

	HAL_RTCEx_BKUPWrite(&hrtc, DUTY_BKP_CREDITS,
			(uint32_t) (credits - (int32_t) cost));

	tx_airtime_ticks = cost;
	return true;
}

void DutyCycle_ChargeTimeout(void) {
	DutyUpdate();

	uint32_t timeout_ticks = DutyAirtimeTicks(DUTY_TX_TIMEOUT_MS);

	if (timeout_ticks > tx_airtime_ticks) {
		int32_t credits = (int32_t) HAL_RTCEx_BKUPRead(&hrtc,
		DUTY_BKP_CREDITS);
		int32_t extra = (int32_t) (timeout_ticks - tx_airtime_ticks);

		HAL_RTCEx_BKUPWrite(&hrtc, DUTY_BKP_CREDITS,
				(uint32_t) (credits - extra));
	}

	DutyCycle_CompleteTx();
}

void DutyCycle_CompleteTx(void) {
	tx_airtime_ticks = 0UL;
}
