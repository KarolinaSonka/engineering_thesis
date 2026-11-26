#include "hall_sensor.h"
#include "gpio.h"

#define HALL_POWER_GPIO_Port GPIOB
#define HALL_POWER_Pin       GPIO_PIN_3

static void HallSensor_On(void) {
	HAL_GPIO_WritePin(HALL_POWER_GPIO_Port, HALL_POWER_Pin, GPIO_PIN_SET);
	HAL_Delay(10U);
}

void HallSensor_Off(void) {
	HAL_GPIO_WritePin(HALL_POWER_GPIO_Port, HALL_POWER_Pin, GPIO_PIN_RESET);
}

GPIO_PinState HallSensor_ReadStable(void) {
	uint8_t low_count = 0U;
	uint8_t high_count = 0U;

	HallSensor_On();

	for (uint8_t i = 0U; i < 5U; i++) {
		if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9) == GPIO_PIN_RESET) {
			low_count++;
		} else {
			high_count++;
		}

		if (i < 4U) {
			HAL_Delay(5U);
		}
	}

	HallSensor_Off();

	return (low_count > high_count) ? GPIO_PIN_RESET : GPIO_PIN_SET;
}
