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

#pragma once

#include <stdio.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "max30102_for_stm32_hal.h"
#include <semphr.h>
#include <queue.h>
//#include "Logger.h"
#include "FIR_Filter.h"


#define LOGGER_QUEUE_LENGTH 50

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
	if(xTaskCreate(func, name, stack, param, prio, handle) != pdPASS){ \
		uart_printf("[ERROR] Failed to create task: %s\r\n", name); \
		Error_Handler(); \
	}\
} while(0) //Dung khi muon kiem tra loi task

#ifndef MIC_WARN_OVERWRITTEN
#define MIC_WARN_OVERWRITTEN 1
#endif

// ====  INMP441_DMA　====

//1000Hz sample rate -> 1ms/sample -> Can xu ly 32 mau/lan (32ms) -> 32 x 4 bytes/sample = 128 bytes
//8000Hz sample rate (toi thieu cua cubeMX) -> 0.125ms/sample -> Gap 8 lan -> Mac dinh trong 32ms thu duoc 256 samples -> 1024 bytes

#define PCG_SAMPLE_RATE	 		8000 // 8000Hz (Hardware)
#define TARGET_SAMPLE_RATE		1000 // 1000Hz (desired)
#define DOWNSAMPLE_FACTOR		(PCG_SAMPLE_RATE / TARGET_SAMPLE_RATE) //8

#define PCG_DMA_BUFFER   		1024 //bytes (8000Hz trong 32ms)
#define I2S_SAMPLE_COUNT 		(PCG_DMA_BUFFER / sizeof(int32_t)) //256 samples
#define I2S_HALF_SAMPLE_COUNT 	(I2S_SAMPLE_COUNT / 2)
#define DOWNSAMPLE_COUNT		(I2S_SAMPLE_COUNT / DOWNSAMPLE_FACTOR) //32 samples (mat mau do ep du lieu xuong 32)

#define MAX_TIMEOUT     		100

// ==== MAX30102 FIFO ====
#define MAX_FIFO_SAMPLE  MAX30102_SAMPLE_LEN_MAX
#define MAX_ACTIVE_LEDS  2

//==== AD8232 DMA ====
#define ECG_DMA_BUFFER	32

// ======= Cac bien nay duoc bi phu thuoc boi ham main.c va Sensor_config.c nen xoa di thi bi loi =====

// Protocol - dinh nghia trong main.c, dung cho Sensor_config.c va main.c
extern ADC_HandleTypeDef hadc1;
extern I2C_HandleTypeDef hi2c1;
extern I2S_HandleTypeDef hi2s2;
extern DMA_HandleTypeDef hdma_spi2_rx;
extern UART_HandleTypeDef huart2;
extern TIM_HandleTypeDef htim3;

//Task - dinh nghia trong Sensor_config.c, dung cho ca main.c va Sensor_config.c
extern TaskHandle_t inmp441_task;
extern TaskHandle_t max30102_task;
extern TaskHandle_t ad8232_task;
extern TaskHandle_t logger_task;
extern TaskHandle_t test_task;

//Semaphore
extern SemaphoreHandle_t sem_adc, sem_mic, sem_max;

//FIR FIlter
extern FIRFilter fir; //Muon bien fir tu file main.c

// ===== Cac bien duoi day chi noi bo Sensor_config dung nen du co comment ca doan nay ma dinh nghia trong file.c thi van khong loi ======

// INMP441
extern volatile int16_t __attribute__((unused))buffer16[I2S_SAMPLE_COUNT];
extern volatile int32_t __attribute__((unused))buffer32[I2S_SAMPLE_COUNT];
extern volatile bool __attribute__((unused))mic_half_ready;
extern volatile bool __attribute__((unused))mic_full_ready;

// Flag do DMA set de bao buffer nao vua day
extern volatile bool __attribute__((unused))mic_ping_ready;
extern volatile bool __attribute__((unused))mic_pong_ready;

// 2 buffer ping/pong (DMA se tu ghi luan phien)
extern volatile int16_t __attribute__((unused))mic_ping[I2S_SAMPLE_COUNT];
extern volatile int16_t __attribute__((unused))mic_pong[I2S_SAMPLE_COUNT];


// AD8232
extern volatile int16_t ecg_buffer[ECG_DMA_BUFFER]; //Bien luu gia tri tu DMA
extern volatile int16_t __attribute__((unused))ecg_dma_value;
extern volatile bool __attribute__((unused))adc_full_ready;

// MAX30102
extern max30102_t max30102_obj;
extern max30102_record record;
extern __attribute__((unused))uint32_t ir_buffer[MAX_FIFO_SAMPLE];
extern __attribute__((unused))uint32_t red_buffer[MAX_FIFO_SAMPLE]; //Bien luu gia tri tu FIFO

//Queue
extern QueueHandle_t logger_queue;

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
} sensor_data_t;


//Kiem tra trang thai du lieu da san sang hay chua
typedef struct {
	bool ecg_ready;
	bool max_ready;
	bool mic_ready;
} sensor_check_t;


//Snapshot cho dong bo
typedef struct {
	TickType_t timestamp;
	uint32_t sample_id;
} snapshot_sync_t;


//==== MAX30102 ====

/**
 * @note - Ham xu ly ngat (ISR) tu chan INT_Pin cua cam bien
 * \note - Ham callback duoc goi moi khi co ngat ngoài (EXTI) xay ra tren 1 chan GPIO
 * \note - Luc do chan GPIO ngat se duoc active-low de thuc thi ngat
 */
extern void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);

HAL_StatusTypeDef __attribute__((unused))Max30102_init_int(I2C_HandleTypeDef *i2c); //Khoi tao co interrupt
uint8_t __attribute__((unused))Max30102_interrupt_process(max30102_t *obj); //Xu ly ngat
void __attribute__((unused))Max30102_task_int(void *pvParameter); //Task dung interrupt de trigger
HAL_StatusTypeDef Max30102_init_no_int(I2C_HandleTypeDef *i2c); //Khoi tao khong co interrupt
void Max30102_task_no_int(void *pvParameter); //Task dung timer de trigger


//==== INMP441 ====

HAL_StatusTypeDef __attribute__((unused))Inmp441_init(I2S_HandleTypeDef *i2s);
HAL_StatusTypeDef __attribute__((unused))Inmp441_init_dma_doubleBuffer(I2S_HandleTypeDef *hi2s, uint16_t *ping, uint16_t *pong, uint16_t Size);
void __attribute__((unused))Inmp441_dma_normal_task(void *pvParameter); //Task xu ly voi DMA Normal
void __attribute__((unused))Inmp441_dma_circular_task(void *pvParameter); //Task xu ly voi DMA Circular
void __attribute__((unused))Inmp441_process_full_buffer(void);
void __attribute__((unused))Inmp441_dma_pingpong_task(void *pvParameter); //Task xu ly DMA double buffer
extern void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s); //Ham callback khi DMA da xong (complete)
extern void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s); //Ham callback khi DMA da xong 1 nua


//==== AD8232 ====

HAL_StatusTypeDef __attribute__((unused))Ad8232_init(ADC_HandleTypeDef *adc);
void __attribute__((unused))Ad8232_task(void *pvParameter); //Task nay khong dung DMA
void Ad8232_dma_sema_task(void *pvParameter); //Task dug DMA + callback + semaphore (co tre phan mem)
void __attribute__((unused))Ad8232_dma_nosema_task(void *pvParameter); //Task dung DMA + callback (trigger timer truc tiep)
extern void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *adc); //Ham callback cho DMA cua ADC


// ==== Terminal log ====

extern void uart_printf(const char *fmt,...); //Muon dung ham nay cua Logger.h thi extern o day la xong
void __attribute__((unused))StackCheck(void);
void __attribute__((unused))HeapCheck(void);


#ifdef __cpluplus
}
#endif //__cplusplus

#endif /* INC_SENSOR_CONFIG_H_ */
