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

typedef struct {
	sensor_type_t type;
	uint16_t count; //So luong mau trong mang
	uint32_t sample_id; //Dung de dong bo du lieu
	union {
		int16_t ecg[ECG_DMA_BUFFER];
		struct {
			uint32_t ir[MAX_FIFO_SAMPLE];
			uint32_t red[MAX_FIFO_SAMPLE];
		} ppg;
		int16_t mic[32]; //PCG
	};
}sensor_block_t;

extern char uart_buf[1024];
extern UART_HandleTypeDef huart2; //Duoc dinh nghia cho khac, chi muon dung o day thoi ti dung extern !
extern QueueHandle_t logger_queue;

void i2c_scanner(I2C_HandleTypeDef *hi2c);

void Logger_task_block(void *pvParameter);

void __attribute__((unused))Logger_task_data(void *pvParameter);

__weak void uart_printf(const char *fmt,...);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif /* INC_LOGGER_H_ */
