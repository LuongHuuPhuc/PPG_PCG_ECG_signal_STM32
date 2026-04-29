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

// ==== MACROS　====
#define MAX_FIFO_SAMPLE  32
#define MAX_ACTIVE_LEDS  2

// Task & RTOS
extern osSemaphoreId max30102_semId;
extern osThreadId max30102_taskId;

// ==== FUCNTION PROTOTYPE ====

/**
 * @brief Ham khoi tao & cau hinh cam bien
 *
 * @param i2c Con tro den struct dinh nghia cau hinh I2C
 *
 * @note Ham khoi tao khong su dung ngat ngoai (No external interrupt)
 * @note Su dung co che Semaphore Binary de quan ly tai nguyen chung giua cac cam bien
 * \note Dong bo giua cac task khac nhau khi co nhieu thread truy cap chung tai nguyen (vd Memory)
 * \note Semaphore khac voi Mutex o co che Lock/Unlock va Priority
 */
HAL_StatusTypeDef Max30102_init(I2C_HandleTypeDef *i2c);

/**
 * @brief Ham thuc thi luong xu ly
 *
 * @note Task khong dung interrupt ma dung TIMER de trigger
 * TIMER ban semaphore sau moi 32ms de task thuc hien xu ly
 */
void Max30102_task(void const *pvParameter);


#ifdef __cplusplus
}
#endif
#endif //__INC_MAX30102_CONFIG_H
