#include "main.h"
#include "power_monitor.h"

static volatile bool loriota_pvd_low = false;

void PowerMonitor_Init(void) {
	PWR_PVDTypeDef sConfigPVD = { 0 };

	sConfigPVD.PVDLevel = PWR_PVDLEVEL_2; // 2,4 V treshold
	sConfigPVD.Mode = PWR_PVD_MODE_IT_FALLING;

	if (HAL_PWR_ConfigPVD(&sConfigPVD) != HAL_OK) {
		Error_Handler();
	}

	__HAL_PWR_PVD_EXTI_CLEAR_FLAG();

	HAL_NVIC_SetPriority(PVD_PVM_IRQn, 0U, 0U);
	HAL_NVIC_EnableIRQ(PVD_PVM_IRQn);

	HAL_PWR_EnablePVD();
}

void HAL_PWR_PVDCallback(void) {
	if (__HAL_PWR_GET_FLAG(PWR_FLAG_PVDO) != RESET) {
		loriota_pvd_low = true;
	}
}

bool IsSupplyLow(void) {
	return loriota_pvd_low || (__HAL_PWR_GET_FLAG(PWR_FLAG_PVDO) != RESET);
}
