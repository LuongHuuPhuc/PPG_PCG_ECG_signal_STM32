/*
 * ad8232_config.h
 *
 *  Created on: Oct 15, 2025
 *      Author: ADMIN
 */

#ifndef INC_AD8232_CONFIG_H_
#define INC_AD8232_CONFIG_H_

#pragma once

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include "main.h"
#include "stdio.h"
#include "string.h"
#include <stdbool.h>
#include "cmsis_os.h"

// MACROS
#define ECG_DMA_BUFFER	32

// Extern variable
extern int16_t ecg_buffer[ECG_DMA_BUFFER]; //Bien luu gia tri tu DMA
extern volatile int16_t __attribute__((unused))ecg_dma_value;
extern volatile bool __attribute__((unused))adc_full_ready;

// Extern protocol variable cho function trong .c
extern ADC_HandleTypeDef hadc1;

// Task RTOS
#if defined(CMSIS_API_USING)

extern osThreadId ad8232_taskId;
extern osSemaphoreId sem_adcId;

#elif defined (FREERTOS_API_USING)

extern TaskHandle_t ad8232_task;
extern SemaphoreHandle_t sem_adc;

#endif // CMSIS_API_USING

//==== FUNCTION PROTOTYPE ====

/**
 * @brief Ham khoi tao cam bien AD8232
 *
 * @param adc Con tro den struct dinh nghia cau hinh ADC
 *
 * @note Su dung co che Semaphore Binary de quan ly tai nguyen chung giua cac cam bien
 * \note Dong bo giua cac task khac nhau khi co nhieu thread truy cap chung tai nguyen (vd Memory)
 * \note Semaphore khac voi Mutex o co che Lock/Unlock va Priority
 */
HAL_StatusTypeDef Ad8232_init(ADC_HandleTypeDef *adc);

/**
 * @brief Task doc gia tri ADC `ver1`
 *
 * @note Task su dung che do Polling for Conversion ADC
 * Khong DMA, khong Semaphore
 */
__attribute__((unused)) void Ad8232_task_ver1(void const *pvParameter);

/**
 * @brief Task doc gia tri ADC `ver2`
 *
 * @note - Task su dung DMA (Circular mode) + conversion callback (trigger timer truc tiep)
 * \note - Khong dung Semaphore
 */
__attribute__((unused)) void Ad8232_task_ver2(void const *pvParameter);

/**
 * @brief Task doc gia tri ADC `ver3`
 *
 * @note - Task nay su dung DMA (Ciruclar mode) + Conversion callback
 * \note - Su dung Semaphore quan ly tai nguyen (co tre phan me (?))
 */
void Ad8232_task_ver3(void const *pvParameter);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif /* INC_AD8232_CONFIG_H_ */
