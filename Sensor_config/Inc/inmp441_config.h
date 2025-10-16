/*
 * inmp441_config.h
 *
 *  Created on: Oct 15, 2025
 *      Author: ADMIN
 */

#ifndef INC_INMP441_CONFIG_H_
#define INC_INMP441_CONFIG_H_

#pragma once

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include "main.h"
#include "stdio.h"
#include <stdbool.h>
#include "FreeRTOS.h"
#include <semphr.h>
#include <queue.h>
#include "FIR_Filter.h"

// ==== MACROS　====

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

// Extern protocol variable cho function trong .c
extern I2S_HandleTypeDef hi2s2;
extern DMA_HandleTypeDef hdma_spi2_rx;
extern FIRFilter fir;

// Task & RTOS
extern TaskHandle_t inmp441_task;
extern SemaphoreHandle_t sem_mic;

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

//==== FUNCTION PROTOTYPE ====

/**
 * @brief Ham khoi tao giao thuc I2S cho INMP441
 *
 * @param i2s Con tro de tro den struct cau hinh protocol I2S
 */
HAL_StatusTypeDef __attribute__((unused))Inmp441_init_ver1(I2S_HandleTypeDef *i2s);

/**
 * @brief Ham khoi tao giao thuc I2S voi double DMA buffer ping-pong
 *
 * @param hi2s Con tro tro den struct cau hinh protocol I2S
 * @param ping Con tro tro den mang chua data buffer ping (buffer 1)
 * @param pong Con tro tro den mang chua data buffer pong (buffer 2)
 * @param Size
 */
HAL_StatusTypeDef __attribute__((unused))Inmp441_init_ver2(I2S_HandleTypeDef *hi2s, uint16_t *ping, uint16_t *pong, uint16_t Size);

/**
 *
 */
void __attribute__((unused))Inmp441_task_ver1(void *pvParameter); //Task xu ly voi DMA Normal

/**
 *
 */
void __attribute__((unused))Inmp441_task_ver2(void *pvParameter); //Task xu ly voi DMA Circular

/**
 *
 */
void __attribute__((unused))Inmp441_process_full_buffer(void);

/**
 *
 */
void __attribute__((unused))Inmp441_task_ver3(void *pvParameter); //Task xu ly DMA double buffer

/**
 *
 */
extern void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s); //Ham callback khi DMA da xong (complete)

/**
 *
 */
extern void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s); //Ham callback khi DMA da xong 1 nua


#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* INC_INMP441_CONFIG_H_ */
