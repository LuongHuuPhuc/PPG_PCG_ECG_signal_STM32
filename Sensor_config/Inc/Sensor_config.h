/*
 * @file Sensor_config.h
 *
 * @date 2025/06/15
 * @author Luong Huu Phuc
 *
 * @note
 * Chua struct data chung cua 3 cam bien
 * Su dung TIMER(TIM) de dong bo hoa 3 cam bien thay cho osTimer trong FreeRTOS
 * - MAX30102(PPG): FIFO polling + TIM3 Trigger
 * - INMP441(PCG): I2S with DMA + DOWNSAMPLE
 * - AD8232(ECG): ADC with DMA + TIM3 trigger
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

	/**
	 * Cac bien nay chia se chung bo nho nhung khong dung volatile
	 * vi cac buffer nay chi la chua data da thu duoc, khong bi anh
	 * huong boi hardware/DMA/ISR sua truc tiep trong RAM
	 *
	 * @note
	 * `Volatile` chi dung khi bien do co nguy co bi thay doi
	 * dot ngot boi cac yeu to ngoai luong kiem soat truc tiep cua code
	 * (phan cung, ngat,...). Muc tieu chinh cua tu khoa nay la
	 * chan compiler thuc hien toi uu hoa bien do va cho bien doc/ghi truc tiep vao RAM
	 *
	 * 		- Compiler rat thong minh, thong thuong neu thay 1 bien duoc
	 * 		doc nhieu lan ma khong bi thay doi boi cac dong code lien ke,
	 * 		no se toi uu bang cach luu gia tri do vao Register cua CPU de
	 * 		truy xuat nhanh hon, thay vi doc lai tu RAM
	 *
	 * 		- Neu bien do bi bat ngo thay doi tu RAM, chuong trinh ma van doc
	 * 		gia tri cu trong thanh ghi, dan den sai sot. Tu khoa `volatile` ep
	 * 		chuong trinh luon doc va ghi du lieu truc tiep vao RAM
	 */
	union {
		int32_t pcg[32]; // PCG
		int16_t ecg[32]; // ECG

		struct { //PPG
			uint32_t ir[32];
			uint32_t red[32];
		} ppg;
	};
} sensor_block_t;

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

#ifdef QUEUE_FREE_CHECK /* Using FreeRTOS API, not CMSIS-OS */
/**
 * @brief Ham kiem tra xem hang doi con free khong
 * @note Neu khong con trong -> Chuong trinh se dung lai
 */
void QueueCheck(const QueueHandle_t queue, const char *name);
#endif // QUEUE_FREE_CHECK

#ifdef __cpluplus
}
#endif //__cplusplus

#endif /* INC_SENSOR_CONFIG_H_ */
