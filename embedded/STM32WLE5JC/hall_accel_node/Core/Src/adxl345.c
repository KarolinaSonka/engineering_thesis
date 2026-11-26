#include "adxl345.h"

#include "i2c.h"

#define ADXL345_I2C_ADDRESS       0xA6U
#define ADXL345_REG_POWER_CTL     0x2DU
#define ADXL345_REG_THRESH_ACT    0x24U
#define ADXL345_REG_ACT_INACT_CTL 0x27U
#define ADXL345_REG_INT_MAP       0x2FU
#define ADXL345_REG_INT_ENABLE    0x2EU
#define ADXL345_REG_INT_SOURCE    0x30U
#define ADXL345_REG_BW_RATE       0x2CU
#define ADXL345_REG_DATA_X0       0x32U

static void ADXL345_WriteRegister(uint8_t reg, uint8_t value) {
	(void) HAL_I2C_Mem_Write(&hi2c2, ADXL345_I2C_ADDRESS, reg,
	I2C_MEMADD_SIZE_8BIT, &value, 1U, 100U);
}

void ADXL345_Initialize(void) {
	uint8_t init_data = 0x08U;
	(void) HAL_I2C_Mem_Write(&hi2c2, ADXL345_I2C_ADDRESS,
	ADXL345_REG_POWER_CTL, I2C_MEMADD_SIZE_8BIT, &init_data, 1U, 100U);

	ADXL345_ConfigureActivityDetection();
}

void ADXL345_ConfigureActivityDetection(void) {
	ADXL345_WriteRegister(ADXL345_REG_POWER_CTL, 0x00U);
	ADXL345_WriteRegister(ADXL345_REG_THRESH_ACT, 0x0FU); // 0,94g
	ADXL345_WriteRegister(ADXL345_REG_ACT_INACT_CTL, 0xF0U);
	ADXL345_WriteRegister(ADXL345_REG_INT_MAP, 0x00U);
	ADXL345_WriteRegister(ADXL345_REG_INT_ENABLE, 0x10U);

	uint8_t dummy = 0U;
	(void) ADXL345_ReadInterruptSource(&dummy);

	ADXL345_WriteRegister(ADXL345_REG_BW_RATE, 0x17U); // 12,5 Hz - low power
	ADXL345_WriteRegister(ADXL345_REG_POWER_CTL, 0x08U);
}

HAL_StatusTypeDef ADXL345_ReadInterruptSource(uint8_t *source) {
	return HAL_I2C_Mem_Read(&hi2c2, ADXL345_I2C_ADDRESS,
	ADXL345_REG_INT_SOURCE, I2C_MEMADD_SIZE_8BIT, source, 1U, 100U);
}

HAL_StatusTypeDef ADXL345_ClearInterrupt(void) {
	uint8_t source = 0U;
	return ADXL345_ReadInterruptSource(&source);
}

void ADXL345_ReadXYZ(int16_t *x, int16_t *y, int16_t *z) {
	uint8_t data[6] = { 0U };
	uint8_t int_source = 0U;

	HAL_Delay(5U);
	(void) ADXL345_ReadInterruptSource(&int_source);

	if (HAL_I2C_Mem_Read(&hi2c2, ADXL345_I2C_ADDRESS,
	ADXL345_REG_DATA_X0, I2C_MEMADD_SIZE_8BIT, data, 6U, 100U) == HAL_OK) {
		*x = (int16_t) ((data[1] << 8) | data[0]);
		*y = (int16_t) ((data[3] << 8) | data[2]);
		*z = (int16_t) ((data[5] << 8) | data[4]);
	} else {
		*x = 9999;
		*y = 9999;
		*z = 9999;
	}
}
