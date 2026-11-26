#include "main.h"
#include "reed_sensor.h"
#include "rtc.h"

#define LORIOTA_REED_STATE_SIGNATURE 0x4C524545UL
#define LORIOTA_REED_BKP_SIGNATURE_REGISTER RTC_BKP_DR8
#define LORIOTA_REED_BKP_STATE RTC_BKP_DR9

uint8_t GetReedState(void) {
	return (HAL_GPIO_ReadPin(REED_GPIO_Port, REED_Pin) == GPIO_PIN_RESET) ?
			0U : 1U;
}

bool ReedChangedSinceLastTx(void) {
	uint32_t backup_signature = HAL_RTCEx_BKUPRead(&hrtc,
	LORIOTA_REED_BKP_SIGNATURE_REGISTER);

	if (backup_signature != LORIOTA_REED_STATE_SIGNATURE) {
		return true;
	}

	uint32_t previous_state = HAL_RTCEx_BKUPRead(&hrtc,
	LORIOTA_REED_BKP_STATE);
	return GetReedState() != previous_state;
}

void SaveReedState(uint8_t state) {
	HAL_PWR_EnableBkUpAccess();
	HAL_RTCEx_BKUPWrite(&hrtc, LORIOTA_REED_BKP_STATE, state);
	HAL_RTCEx_BKUPWrite(&hrtc, LORIOTA_REED_BKP_SIGNATURE_REGISTER,
	LORIOTA_REED_STATE_SIGNATURE);
}
