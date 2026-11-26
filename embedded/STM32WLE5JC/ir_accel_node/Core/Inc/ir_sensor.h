#ifndef LORIOTA_IR_SENSOR_H
#define LORIOTA_IR_SENSOR_H

#include "stm32wlxx_hal.h"

void IrSensor_Off(void);
GPIO_PinState IrSensor_ReadStable(void);

#endif
