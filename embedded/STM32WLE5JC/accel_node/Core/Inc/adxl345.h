#ifndef LORIOTA_ADXL345_H
#define LORIOTA_ADXL345_H

#include <stdint.h>
#include "stm32wlxx_hal.h"

void ADXL345_ConfigureActivityDetection(void);
HAL_StatusTypeDef ADXL345_ReadInterruptSource(uint8_t *source);
HAL_StatusTypeDef ADXL345_ClearInterrupt(void);
void ADXL345_ReadXYZ(int16_t *x, int16_t *y, int16_t *z);

#endif
