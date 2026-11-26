#ifndef LORIOTA_DUTY_CYCLE_H
#define LORIOTA_DUTY_CYCLE_H

#include <stdbool.h>
#include <stdint.h>

#define DUTY_CYCLE_MAX_TX_MS 2500UL

void DutyCycle_Init(void);
uint32_t DutyCycle_AirtimeMs(void);
uint32_t DutyCycle_WaitSeconds(uint32_t airtime_ms);
bool DutyCycle_Reserve(uint32_t airtime_ms);
void DutyCycle_ChargeTimeout(void);
void DutyCycle_CompleteTx(void);

void DutyCycle_SetPending(bool pending);
bool DutyCycle_IsPending(void);

#endif
