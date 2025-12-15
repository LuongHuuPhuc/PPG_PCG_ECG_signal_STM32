/*
 * max30102_config.h
 *
 *  Created on: Oct 15, 2025
 *      Author: ADMIN
 */

#ifndef _INC_MAX30102_CONFIG_H
#define _INC_MAX30102_CONFIG_H

#pragma once

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include "main.h"
#include "stdio.h"
#include "stdlib.h"
#include "cmsis_os.h"
#include "max30102_low_level.h"
#include "max30102_lib.h"

// ==== MACROS　====
#define MAX_FIFO_SAMPLE  MAX30102_SAMPLE_LEN_MAX
#define MAX_ACTIVE_LEDS  2

// Extern variables
extern max30102_t max30102_obj;
extern max30102_record record;
extern uint32_t ir_buffer[MAX_FIFO_SAMPLE];
extern uint32_t red_buffer[MAX_FIFO_SAMPLE]; //Bien luu gia tri tu FIFO

// Extern protocol variables
extern I2C_HandleTypeDef hi2c1;

// Task & RTOS
#if defined(FREERTOS_API_USING)

extern TaskHandle_t max30102_task;
extern SemaphoreHandle_t sem_max;

#elif defined(CMSIS_API_USING)

extern osSemaphoreId sem_maxId;
extern osThreadId max30102_taskId;

#endif // CMSIS_API_USING

// ==== FUCNTION PROTOTYPE ====

/**
 * @brief Ham khoi tao & cau hinh cam bien `ver1`
 *
 * @param i2c Con tro den struct dinh nghia cau hinh I2C
 *
 * @note Ham khoi tao su dung ngat (interrupt)
 */
__attribute__((unused)) HAL_StatusTypeDef Max30102_init_ver1(I2C_HandleTypeDef *i2c);

/**
 * @brief Ham khoi tao & cau hinh cam bien `ver2`
 *
 * @param i2c Con tro den struct dinh nghia cau hinh I2C
 *
 * @note Ham khoi tao khong su dung ngat ngoai (No external interrupt)
 * @note Su dung co che Semaphore Binary de quan ly tai nguyen chung giua cac cam bien
 * \note Dong bo giua cac task khac nhau khi co nhieu thread truy cap chung tai nguyen (vd Memory)
 * \note Semaphore khac voi Mutex o co che Lock/Unlock va Priority
 */
HAL_StatusTypeDef Max30102_init_ver2(I2C_HandleTypeDef *i2c);

/**
 * @brief Ham thuc thi luong xu ly `ver1`
 *
 * @note Task dung interrupt de trigger moi khi co data moi
 */
__attribute__((unused)) void Max30102_task_ver1(void const *pvParameter);

/**
 * @brief Ham thuc thi luong xu ly `ver2`
 *
 * @note Task khong dung interrupt ma dung timer de trigger
 */
void Max30102_task_ver2(void const *pvParameter);


#ifdef __cplusplus
}
#endif
#endif //__INC_MAX30102_CONFIG_H
