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
#include "Sensor_config.h"
#include "inmp441_config.h"
#include "ad8232_config.h"
#include "max30102_config.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX_COUNT 32
#define LOGGER_QUEUE_LENGTH 50

// Extern variables
extern char uart_buf[1024];
extern UART_HandleTypeDef huart2; //Duoc dinh nghia cho khac, chi muon dung o day thoi ti dung extern !

// Task & RTOS
extern QueueHandle_t logger_queue;
extern TaskHandle_t logger_task;

// ==== FUNCTION PROTOTYPE ====

/**
 *
 */
void i2c_scanner(I2C_HandleTypeDef *hi2c);

/**
 *
 */
void Logger_task_block(void *pvParameter); //Ham nhan data tu queue theo block 32 samples/lan

/**
 *
 */
void Logger_one_task(void *pvParameter); //Ham debug log de xem thuc te co bao nhieu sample moi task

/**
 *
 */
void isQueueFree(QueueHandle_t *queue);


#ifdef __cplusplus
}
#endif //__cplusplus

#endif /* INC_LOGGER_H_ */
