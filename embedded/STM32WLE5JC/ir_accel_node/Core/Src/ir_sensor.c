#include "ir_sensor.h"
#include "main.h"

#define IR_SENSOR_POWER_GPIO_Port GPIOB
#define IR_SENSOR_POWER_Pin       GPIO_PIN_3

static void IrSensor_On(void) {
	HAL_GPIO_WritePin(IR_SENSOR_POWER_GPIO_Port, IR_SENSOR_POWER_Pin,
			GPIO_PIN_SET);
	HAL_Delay(10U);
}

void IrSensor_Off(void) {
	HAL_GPIO_WritePin(IR_SENSOR_POWER_GPIO_Port, IR_SENSOR_POWER_Pin,
			GPIO_PIN_RESET);
}

GPIO_PinState IrSensor_ReadStable(void) {
	uint8_t low_count = 0U;
	uint8_t high_count = 0U;

	IrSensor_On();

	for (uint8_t i = 0U; i < 5U; i++) {
		if (HAL_GPIO_ReadPin(IR_GPIO_Port, IR_Pin) == GPIO_PIN_RESET) {
			low_count++;
		} else {
			high_count++;
		}

		if (i < 4U) {
			HAL_Delay(5U);
		}
	}

	IrSensor_Off();

	return (low_count > high_count) ? GPIO_PIN_RESET : GPIO_PIN_SET;
}
