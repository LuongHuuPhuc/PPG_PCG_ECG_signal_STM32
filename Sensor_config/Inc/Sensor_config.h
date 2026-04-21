/*
 * Sensor_config.h
 *
 *  @date 2025/06/15
 *  @Author Luong Huu Phuc
 *
 *  Su dung TIMER(TIM) de dong bo hoa 3 cam bien thay cho osTimer trong FreeRTOS
 *  MAX30102(PPG): FIFO polling + TIM3 Trigger
 *  INMP441(PCG): I2S with DMA + DOWNSAMPLE
 *  AD8232(ECG): ADC with DMA + TIM3 trigger
 */

#ifndef INC_SENSOR_CONFIG_H_
#define INC_SENSOR_CONFIG_H_

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stdio.h"
#include "main.h"
#include "cmsis_os.h"

extern void uart_printf(const char *fmt,...);

#define SERROR_CHECK(expr) do { \
	/* expr la bieu thuc ban muon truyen vao */ \
	if((expr) != HAL_OK){ \
		uart_printf(" [SENCONF] Fail HAL at line %d: %s\r\n", __LINE__, #expr);\
		HAL_Delay(100); \
		Error_Handler(); \
	} \
} while(0)

typedef enum SENSOR_TAG{
	SENSOR_ECG,
	SENSOR_PPG,
	SENSOR_PCG
} sensor_type_t;

/* STRUCT luu data chinh truc tiep tu Sensor Task */
typedef struct SENSOR_BLOCK_t { // Neu de struct anoymous se khong khop voi forward declaration
	uint32_t sample_id; 	// Dung de dong bo du lieu
	TickType_t timestamp;	// Danh dau thoi gian dong bo
	sensor_type_t type;
	uint16_t count; 		// So luong mau trong mang

	union { // Cac bien nay chia se chung bo nho
		volatile int32_t pcg[32];

		struct { //PPG
			volatile uint32_t ir[32];
			volatile uint32_t red[32];
		} ppg;

		volatile int16_t ecg[32]; // ECG
	};
} sensor_block_t;

// Extern protocol variables
extern UART_HandleTypeDef huart2;
extern TIM_HandleTypeDef htim3;
extern I2C_HandleTypeDef hi2c1;
extern ADC_HandleTypeDef hadc1;
extern I2S_HandleTypeDef hi2s2;

// ==== FUNCTION PROTOTYPE ====

/**
 * @brief Ham khoi tao cam bien (Semaphore, Queue,...)
 *
 * @note Ham chi dung de gom cac ham khoi tao MAX30102 - AD8232 - INMP441
 */
void SensorConfig_Init(void);

/**
 * @brief Ham kiem tra bo nho stack con lai
 */
void StackCheck(void);

/**
 * @brief Ham kiem tra bo nho heap con lai
*/
void HeapCheck(void);

#ifdef __cpluplus
}
#endif //__cplusplus

#endif /* INC_SENSOR_CONFIG_H_ */
