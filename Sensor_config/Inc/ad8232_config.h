/*
 * @file ad8232_config.h
 *
 * @date Oct 15, 2025
 * @author LuongHuuPhuc
 */

#ifndef INC_AD8232_CONFIG_H_
#define INC_AD8232_CONFIG_H_

#pragma once

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include "main.h"
#include "stdio.h"
#include "stdbool.h"

#include "cmsis_os.h"

// ==== MACROS　====
#define ECG_DMA_BUFFER			32
#define ECG_DEFAULT_GAIN		100
#define ECG_OPAMP_A1_GAIN		11
#define ECG_TOTAL_GAIN			((ECG_DEFAULT_GAIN) * (ECG_OPAMP_A1_GAIN))
#define ADC_REF_VOL				3.3f
#define VIRTUAL_GND_VOL			ADC_REF_VOL / 2.0f // V
#define ADC_VOL_LEVEL_12B		((1UL << 12) - 1.0f) // 12-bit ADC

// Task RTOS
extern osThreadId ad8232_taskId;
extern osSemaphoreId ad8232_semId;

// ==== FUNCTION PROTOTYPE ====

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
 * @brief Task doc gia tri ADC
 *
 * @note - Task nay su dung DMA (Ciruclar mode) + Conversion callback
 * \note - Su dung Semaphore quan ly tai nguyen (co tre phan me (?))
 */
void Ad8232_task(void const *pvParameter);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif /* INC_AD8232_CONFIG_H_ */
