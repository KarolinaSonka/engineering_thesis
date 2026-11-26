#ifndef LORIOTA_HALL_SENSOR_H
#define LORIOTA_HALL_SENSOR_H

#include "stm32wlxx_hal.h"

void HallSensor_Off(void);
GPIO_PinState HallSensor_ReadStable(void);

#endif
