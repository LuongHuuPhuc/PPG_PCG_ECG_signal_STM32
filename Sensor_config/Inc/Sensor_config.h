/*
 * Sensor_config.h
 *
 *  @date 2025/06/15
 *  @Author Luong Huu Phuc
 *
 *  Su dung TIMER(TIM) de dong bo hoa 3 cam bien thay cho osTimer trong FreeRTOS
 *  MAX30102(PPG): FIFO polling + TIM3 Trigger
 *  INMP441(PCG): I2S with DMA + TIM3 trigger + DOWNSAMPLE
 *  AD8232(ECG): ADC with DMA + TIM3 trigger
 */

#ifndef INC_SENSOR_CONFIG_H_
#define INC_SENSOR_CONFIG_H_

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stdio.h"
#include "cmsis_os.h"
#include "ad8232_config.h"
#include "max30102_config.h"
#include "inmp441_config.h"
#include "Logger.h"

#define SERROR_CHECK(expr) do {\
	/* expr la bieu thuc ban muon truyen vao */ \
	if((expr) != HAL_OK){ \
		uart_printf("Fail HAL at line %d:\r\n", __LINE__, #expr);\
		HAL_Delay(100);\
		while(1); \
	}\
} while(0)

#define QUEUE_SEND_FROM_TASK(data_ptr) do { \
	if(xQueueSend(logger_queue, (data_ptr), portMAX_DELAY) != pdTRUE){ \
		uart_printf("[WARN] Logger queue full!\r\n"); \
	} \
} while(0)

#define QUEUE_SEND_FROM_ISR(data_ptr) do { \
	if(xQueueSendFromISR(logger_queue, (data_ptr), pxHigherPriorityTaskWoken) != pdTRUE){\
		uart_printf("[WARN] Logger queue full from ISR! \r\n "); \
	} \
} while(0) // Dung khi muon send quueu trong cac ham HAL_callback

#define TASK_ERR_CHECK(func, name, stack, param, prio, handle) do { \
	if(xTaskCreate((void*)func, name, stack, param, prio, handle) != pdPASS){ \
		uart_printf("[ERROR] Failed to create task: %s\r\n", name); \
		Error_Handler(); \
	}\
} while(0) //Dung khi muon kiem tra loi task

#ifndef MIC_WARN_OVERWRITTEN
#define MIC_WARN_OVERWRITTEN 1
#endif

#define PLACE_IN_SECTION(__x__)   __attribute__(section(__x__))

typedef enum{
	SENSOR_ECG,
	SENSOR_PPG,
	SENSOR_PCG
} sensor_type_t;


// Cau truc de luu trang thai va gia tri cua cac cam bien
typedef struct {
	sensor_type_t type;
	uint32_t ir;
	uint32_t red;
	int16_t mic_frame[I2S_SAMPLE_COUNT]; //Dung khi muon luu nhieu mau/lan
	int16_t mic; //Real-time, in 1 gia tri tai 1 thoi diem nhung mat mau (khong nen dung)
	uint16_t byte_read; //So mau thuc te
	int16_t ecg;
} __attribute__((unused))sensor_data_t;


typedef struct SENSOR_BLOCK_t { // Neu de struct anoymous se khong khop voi forward declaration
	sensor_type_t type;
	uint16_t count; //So luong mau trong mang
	uint32_t sample_id; //Dung de dong bo du lieu
	TickType_t timestamp;

	union { // Cac bien nay chia se chung bo nho
		volatile int16_t ecg[ECG_DMA_BUFFER];
		struct {
			volatile uint32_t ir[MAX_FIFO_SAMPLE];
			volatile uint32_t red[MAX_FIFO_SAMPLE];
		} ppg;
		volatile int16_t mic[DOWNSAMPLE_COUNT]; //PCG
	};
} sensor_block_t;

// Extern protocol variables
extern UART_HandleTypeDef huart2;
extern TIM_HandleTypeDef htim3;
extern I2C_HandleTypeDef hi2c1;
extern ADC_HandleTypeDef hadc1;
extern I2S_HandleTypeDef hi2s2;

// Other externs
extern volatile uint32_t global_sample_id;
extern volatile TickType_t global_timestamp;

// ==== FUNCTION PROTOTYPE ====

/**
 * @brief Ham khoi tao cam bien (Semaphore, Queue,...)
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
