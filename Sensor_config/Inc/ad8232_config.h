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
#include "FreeRTOS.h"
#include <semphr.h>
#include <queue.h>

// ==== MACROS　====
#define ECG_DMA_BUFFER	32

// Extern variable
extern volatile int16_t ecg_buffer[ECG_DMA_BUFFER]; //Bien luu gia tri tu DMA
extern volatile int16_t __attribute__((unused))ecg_dma_value;
extern volatile bool __attribute__((unused))adc_full_ready;

// Extern protocol variable cho function trong .c
extern ADC_HandleTypeDef hadc1;

// Task RTOS
extern TaskHandle_t ad8232_task;
extern SemaphoreHandle_t sem_adc;

//==== FUNCTION PROTOTYPE ====

/**
 * @brief Ham khoi tao cam bien AD8232
 *
 * @param adc Con tro den struct inh nghia cau hinh ADC
 */
HAL_StatusTypeDef __attribute__((unused))Ad8232_init_ver1(ADC_HandleTypeDef *adc);

/**
 * @brief Task doc gia tri ADC `ver1`
 *
 * @note Task khong su dung DMA
 */
void __attribute__((unused))Ad8232_task_ver1(void *pvParameter);

/**
 * @brief Task doc gia tri ADC `ver3`
 *
 * @note Task nay su dung DMA + Conversion callback + Semaphore quan ly tai nguyen (co tre phan me (?))
 */
void Ad8232_task_ver3(void *pvParameter);

/**
 * @brief Task doc gia tri ADC `ver2`
 *
 * @note Task su dung DMA + conversion callback (trigger timer truc tiep) - Khong dung semaphore
 */
void __attribute__((unused))Ad8232_task_ver2(void *pvParameter);

/**
 * @brief Ham conversion callback
 *
 * @note Khi buffer DMA hoan tat thi ham nay se duoc goi
 * \note `vTaskNotifyGiveFromISR()` se duoc truyen con tro cua ham task vao de callback moi lan
 */
extern void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *adc);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif /* INC_AD8232_CONFIG_H_ */
