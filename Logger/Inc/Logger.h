/*
 * Logger.h
 *
 *  Created on: Jun 17, 2025
 *  @Author Luong Huu Phuc
 */

#ifndef INC_LOGGER_H_
#define INC_LOGGER_H_

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdio.h>
#include <stdarg.h>
#include "cmsis_os.h"
#include "Sensor_config.h"
#include "inmp441_config.h"
#include "ad8232_config.h"
#include "max30102_config.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX_COUNT 32
#define LOGGER_QUEUE_LENGTH 70 // Tang chieu dai queue de chong tran


//Kiem tra trang thai du lieu da san sang hay chua
typedef struct {
	bool ecg_ready;
	bool max_ready;
	bool mic_ready;
} __attribute__((unused))sensor_check_t;

// Extern variables
extern char uart_buf[1024];
extern UART_HandleTypeDef huart2; //Duoc dinh nghia cho khac, chi muon dung o day thoi ti dung extern !

// Task & RTOS
extern SemaphoreHandle_t sem_uart_log; // Dung de bao ve thao tac enqueue
extern QueueHandle_t logger_queue;
extern TaskHandle_t logger_task;
extern osThreadId logger_taskId;

// ==== FUNCTION PROTOTYPE ====

/**
 * @brief Ham scan cac bus I2C de xem co dang ton tai dia chi nao khong
 *
 * @param hi2c Con tro den struct cau hinh I2C
 */
void Logger_i2c_scanner(I2C_HandleTypeDef *hi2c);

/**
 * @brief Ham khoi tao Queue de log du lieu theo hang doi
 * Tranh bi nghen
 *
 * @note Do UART tai nguyen chung nen su dung Semaphore Mutex de uu tien 1 thread UART Logger chay
 * \note - Co che Lock/Unlock (Quyen so huu): chi 1 thread lock va unlock duoc no
 * \note - Co che Priority Inheritane (Uu tien): Chi 1 thread chay duoc trong 1 thoi diem
 * \note - Mutex khong dung duoc trong ISR
 * \note - Mutex tuong duong voi 1 Binary Semaphore
 */
HAL_StatusTypeDef Logger_queue_init(void);

/**
 * @brief Ham in ra 3 kenh du lieu PPG PCG ECG cung luc
 */
void Logger_task_block(void const *pvParameter); //Ham nhan data tu queue theo block 32 samples/lan

/**
 * @brief Ham in ra 2 kenh du lieu dong thoi
 */
void Logger_two_task(void const *pvParameter);

/**
 * @brief Ham in ra 1 kenh du lieu
 */
void Logger_one_task(void const *pvParameter); //Ham debug log de xem thuc te co bao nhieu sample moi task

/**
 * @brief Ham kiem tra xem hang doi con free khong
 * @note Neu khong con trong -> Chuong trinh se dung lai
 */
void __attribute__((unused))isQueueFree(const QueueHandle_t queue, const char *name);

/**
 * @brief Ham thay the cho printf su dung UART
 */
__attribute__((weak)) void uart_printf(const char *fmt,...);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif /* INC_LOGGER_H_ */
