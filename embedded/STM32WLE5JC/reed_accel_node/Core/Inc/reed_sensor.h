#ifndef LORIOTA_REED_SENSOR_H
#define LORIOTA_REED_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

uint8_t GetReedState(void);
bool ReedChangedSinceLastTx(void);
void SaveReedState(uint8_t state);

#endif
