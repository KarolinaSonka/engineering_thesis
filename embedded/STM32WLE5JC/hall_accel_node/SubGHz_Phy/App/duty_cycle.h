#ifndef LORIOTA_DUTY_CYCLE_H
#define LORIOTA_DUTY_CYCLE_H

#include <stdbool.h>
#include <stdint.h>

#define DUTY_CYCLE_MAX_TX_MS 2500UL

#define DUTY_EVT_OPEN   (1UL << 0)
#define DUTY_EVT_VIB    (1UL << 1)
#define DUTY_EVT_STATE  (1UL << 2)
#define DUTY_EVT_REPORT (1UL << 3)

void DutyCycle_Init(bool rtc_preserved);
uint32_t DutyCycle_GetFlags(void);
void DutyCycle_SetFlags(uint32_t flags);
uint32_t DutyCycle_AirtimeMs(void);
uint32_t DutyCycle_WaitSeconds(uint32_t airtime_ms);
bool DutyCycle_Reserve(uint32_t airtime_ms);
void DutyCycle_ChargeTimeout(void);
void DutyCycle_CompleteTx(void);

#endif
