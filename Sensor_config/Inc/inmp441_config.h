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
#include "cmsis_os.h"
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
extern osThreadId inmp441_taskId;

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

// Forward Declaration (Khai bao tam thoi de compiler biet kieu du lieu nay se duoc dinh nghia o noi khac sau nay)
// Tranh include cheo nhau (A can B, B can A)
typedef struct SENSOR_BLOCK_t sensor_block_t; // From Sensor_config.h
typedef struct SNAPSHOT_SYNC_t snapshot_sync_t; // From

//==== FUNCTION PROTOTYPE ====

/**
 * @brief Ham khoi tao giao thuc I2S cho INMP441
 *
 * @param i2s Con tro de tro den struct cau hinh protocol I2S
 *
 * @note Su dung co che Semaphore Binary de quan ly tai nguyen chung giua cac cam bien
 * \note Dong bo giua cac task khac nhau khi co nhieu thread truy cap chung tai nguyen (vd Memory)
 * \note Semaphore khac voi Mutex o co che Lock/Unlock va Priority
 */
HAL_StatusTypeDef __attribute__((unused))Inmp441_init_ver1(I2S_HandleTypeDef *i2s);

/**
 * @brief Ham thuc thi va xu ly `ver1`
 *
 * @note Task nay xu ly du lieu su dung DMA (Mode: Normal)
 */
void __attribute__((unused))Inmp441_task_ver1(void const *pvParameter);

/**
 * @brief Ham khoi tao giao thuc I2S voi double DMA buffer ping-pong
 *
 * @param hi2s Con tro tro den struct cau hinh protocol I2S
 * @param ping Con tro tro den mang chua data buffer ping (buffer 1)
 * @param pong Con tro tro den mang chua data buffer pong (buffer 2)
 * @param Size Kich thuoc so mau cua moi buffer
 */
HAL_StatusTypeDef __attribute__((unused))Inmp441_init_ver2(I2S_HandleTypeDef *hi2s, uint16_t *ping, uint16_t *pong, uint16_t Size);

/**
 * @brief Ham thuc thi va xu ly `ver2`
 *
 * @note Task nay xu ly du lieu su dung DMA (Mode: Circular)
 * \note - Thu thap data vao DMA buffer
 * \note - Gan flags`ready -> false` moi lan buffer day
 * \note - Callback se lat flags `ready -> true` de tiep tuc vong lap
 */
void __attribute__((unused))Inmp441_task_ver2(void const *pvParameter);

/**
 * @brief Ham xu ly du lieu khi full buffer
 *
 * @note Ham con cua `Inmp441_task_ver2`
 * \note - Sau khi DMA ghi du `I2S_SAMPLE_COUNT = 256` mau thi downsample ve 32 mau + FIR Filter
 */
void Inmp441_process_full_buffer(sensor_block_t *block, snapshot_sync_t *snap);

/**
 * @brief Ham thuc thi va xu ly `ver3`
 *
 * @note Do toc do lay mau nhanh va de tranh bi overwritten du lieu cu khi chua xu ly xong -> Su dung Double Buffer Ping-Pong
 */
void __attribute__((unused))Inmp441_task_ver3(void const *pvParameter); //Task xu ly DMA double buffer


#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* INC_INMP441_CONFIG_H_ */
