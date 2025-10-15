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

#include <stdio.h>
#include <stdarg.h>
#include "stm32f4xx_hal.h"
#include "Sensor_config.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX_COUNT 32

typedef struct {
	sensor_type_t type;
	uint16_t count; //So luong mau trong mang
	uint32_t sample_id; //Dung de dong bo du lieu
	TickType_t timestamp;

	union {
		volatile int16_t ecg[ECG_DMA_BUFFER];
		struct {
			volatile uint32_t ir[MAX_FIFO_SAMPLE];
			volatile uint32_t red[MAX_FIFO_SAMPLE];
		} ppg;
		volatile int16_t mic[DOWNSAMPLE_COUNT]; //PCG
	};
}sensor_block_t;

extern char uart_buf[1024];
extern UART_HandleTypeDef huart2; //Duoc dinh nghia cho khac, chi muon dung o day thoi ti dung extern !
extern QueueHandle_t logger_queue;

void i2c_scanner(I2C_HandleTypeDef *hi2c);
void Logger_task_block(void *pvParameter); //Ham nhan data tu queue theo block 32 samples/lan
void Logger_one_task(void *pvParameter); //Ham debug log de xem thuc te co bao nhieu sample moi task
void isQueueFree(QueueHandle_t *queue);
__weak void uart_printf(const char *fmt,...);


#ifdef __cplusplus
}
#endif //__cplusplus

#endif /* INC_LOGGER_H_ */
