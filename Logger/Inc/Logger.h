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
#include "stdbool.h"
#include <stdarg.h>
#include "cmsis_os.h"
#include "Sensor_config.h"
#include "inmp441_config.h"
#include "ad8232_config.h"
#include "max30102_config.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX_COUNT 32
#define MAX_RETRY_SCANNER 2
#define LOGGER_QUEUE_LENGTH 70 // Tang chieu dai queue de chong tran
#define UART_TX_BUFFER_SIZE 2048

//Kiem tra trang thai du lieu da san sang hay chua
typedef struct {
	bool ecg_ready;
	bool max_ready;
	bool mic_ready;
} __attribute__((unused))sensor_check_t;

// Extern variables
extern UART_HandleTypeDef huart2; //Duoc dinh nghia cho khac, chi muon dung o day thoi ti dung extern !

// Task & RTOS
extern SemaphoreHandle_t sem_uart_log; // Mutex de bao ve thao tac UART (UART khong bi tranh chap, chi 1 task dung UART)
extern SemaphoreHandle_t __attribute__((unused))sem_uart_tx_done; // Binary de bao trang thai DMA cua UART (tranh ghi de buffer)
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
 * @brief Ham khoi tao Queue de log du lieu theo hang doi.
 *
 * @note Do UART tai nguyen chung nen su dung Semaphore Mutex de uu tien 1 thread UART Logger chay
 * \note - Co che Lock/Unlock (Quyen so huu): chi 1 thread lock va unlock duoc no
 * \note - Co che Priority Inheritane (Uu tien): Chi 1 thread chay duoc trong 1 thoi diem
 * \note - Mutex khong dung duoc trong ISR
 * \note - Mutex tuong duong voi 1 Binary Semaphore
 */
HAL_StatusTypeDef Logger_init_ver1(void);

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
void isQueueFree(const QueueHandle_t queue, const char *name);

/**
* @brief Ham thay the cho printf su dung UART
* @note Su dung truyen UART theo Blocking Mode + Mutex de bao ve UART
*/
__attribute__((weak)) void uart_printf(const char *fmt,...);

#if USING_UART_DMAPHORE
/**
 * @brief Ham khoi tao Queue de log du lieu theo hang doi.
 * Su dung DMA UART de tang toc do doc (non-blocking mode tranh chan CPU) thay cho UART thong thuong (blocking mode)
 *
 * @note Do UART tai nguyen chung nen su dung Semaphore Mutex de uu tien 1 thread UART Logger chay
 * \note - Co che Lock/Unlock (Quyen so huu): chi 1 thread lock va unlock duoc no
 * \note - Co che Priority Inheritane (Uu tien): Chi 1 thread chay duoc trong 1 thoi diem
 * \note - Mutex khong dung duoc trong ISR
 * \note - Mutex tuong duong voi 1 Binary Semaphore
 */
HAL_StatusTypeDef Logger_init_ver2(void);

/**
 * @brief Ham in ra 2 kenh du lieu dong thoi su dung DMA + Semaphore (Mutex, Binary)
 * @warning Loi khong chay duoc data nhu y muon (cac gia tri lap lai) => Khong dung duoc
 */
__attribute__((unused)) void Logger_two_task_dmaphore(void const *pvParameter);

/**
 * @brief Ham thay the cho printf su dung UART
 * @note Su dung DMA + Semaphore Mutex de quan ly tai nguyen UART tranh du lieu buffer bi ghi de
 */
__attribute__((weak, unused)) void uart_printf_dmaphore(const char *buffer, uint16_t buflen);

#endif

#ifdef __cplusplus
}
#endif //__cplusplus

#endif /* INC_LOGGER_H_ */
