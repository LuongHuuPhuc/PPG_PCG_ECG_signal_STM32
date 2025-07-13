/*
 * Logger.c
 *
 *  Created on: Jun 17, 2025
 *  @Author Luong Huu Phuc
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "Sensor_config.h"
#include "Logger.h"

char uart_buf[1024];

void i2c_scanner(I2C_HandleTypeDef *hi2c){
	uart_printf("Starting I2C Scanner...\r\n");

	for(uint8_t address = 0x08; address <= 0x77; address++){
		HAL_StatusTypeDef result;
		result = HAL_I2C_IsDeviceReady(hi2c, address << 1, 2, 10); //Dich 7-bit to 8-bit addr

		if(result == HAL_OK){
			uart_printf("I2C device found at address: 0x%02X\r\n", address);
		}
	}
	uart_printf("I2C Scanner complete !\r\n");
}

__weak void uart_printf(const char *fmt,...){
	va_list args;
	va_start(args, fmt);
	vsnprintf(uart_buf, sizeof(uart_buf), fmt, args);
	va_end(args);
	HAL_UART_Transmit(&huart2, (uint8_t*)uart_buf, strlen(uart_buf), HAL_MAX_DELAY);
}

void Logger_task_block(void *pvParameter){
	sensor_block_t ecg_block = {0}, ppg_block = {0}, block;
	sensor_check_t sensor_check = {
			.ecg_ready = false,
			.max_ready = false,
			.mic_ready = false
	};

	while(1){
		memset(&block, 0, sizeof(sensor_block_t));
		if(xQueueReceive(logger_queue, &block, portMAX_DELAY) == pdTRUE){
			switch(block.type){
			case SENSOR_ECG:
				memcpy(&ecg_block, &block, sizeof(sensor_block_t)); //Copy du lieu tu block sang ecg_block neu la type ECG
				sensor_check.ecg_ready = true;
				break;
			case SENSOR_PPG:
				memcpy(&ppg_block, &block, sizeof(sensor_block_t)); //Copy du lieu tu block sang ppg_block neu la type PPG
				sensor_check.max_ready = true;
				break;
			case SENSOR_PCG:
				break;
			}

			if(sensor_check.ecg_ready && sensor_check.max_ready){
				if(ecg_block.sample_id == ppg_block.sample_id){ //Neu dong bo thi moi in
					uint16_t min_count = MIN(ecg_block.count, ppg_block.count);
//					uart_printf("[ID: %lu], Printing %d samples \n", ecg_block.sample_id, min_count); //why 24 samples (?)
					for(uint16_t i = 0; i < min_count; i++){
						uart_printf("%d,%lu,%lu\n",
								ecg_block.ecg[i],
								ppg_block.ppg.red[i],
								ppg_block.ppg.ir[i]);
					}
				}else{
					uart_printf("[WARN] Sample_id mismatch ! ECG: %lu, PPG: %lu\r\n",
							ecg_block.sample_id, ppg_block.sample_id);
				}
				sensor_check.ecg_ready = sensor_check.max_ready = false;
			}else{
//				uart_printf("[WARN] Some data is not ready !\r\n");
			}
		}
	}
}

void __attribute__((unused))Logger_task_data(void *pvParameter){
	sensor_data_t __attribute__((unused))sensor_data;
	sensor_check_t sensor_check = {
			.ecg_ready = false,
			.max_ready = false,
			.mic_ready = false
	};

	//Bien luu gia tri duoc day vao queue tu cac cam bien
	static int16_t mic_value[I2S_SAMPLE_COUNT] = {0};
	static int16_t __attribute__((unused))ecg_value = 0;
	static uint32_t ir_value = 0, red_value = 0;

	uart_printf("Logger task started !\r\n");
	vTaskDelay(pdMS_TO_TICKS(5));
	while(1){
		if(xQueueReceive(logger_queue, &sensor_data, portMAX_DELAY) != pdTRUE){
			uart_printf("[QUEUE] Logger queue full ! \r\n");
		}else{
			switch(sensor_data.type){
			case SENSOR_ECG:
				ecg_value = sensor_data.ecg;
				sensor_check.ecg_ready = true;
				break;
			case SENSOR_PPG:
				ir_value = sensor_data.ir;
				red_value = sensor_data.red;
				sensor_check.max_ready = true;
				break;
			case SENSOR_PCG:
				if(sensor_data.byte_read <= I2S_SAMPLE_COUNT){ //Tranh tran bo dem
					memcpy(mic_value, sensor_data.mic_frame, sensor_data.byte_read * sizeof(int16_t));
					sensor_check.mic_ready = true;
				}
				break;
			}

			if(sensor_check.ecg_ready && sensor_check.max_ready){
				uart_printf("%d,%lu,%lu\n", ecg_value, ir_value, red_value);
				sensor_check.ecg_ready = false;
				sensor_check.max_ready = false;
			}
			// Khi du ca 3 du lieu -> In ra 1 dong
//			if(sensor_check.max_ready && sensor_check.ecg_ready && sensor_check.mic_ready){
//				for(int16_t i = 0; i < I2S_SAMPLE_COUNT; i++)
//					uart_printf("%ld,%lu,%lu,%d\r\n", ecg_value, ir_value, red_value, mic_buffer[i]);
//				sensor_check.ecg_ready = sensor_check.max_ready = sensor_check.mic_ready = false;
//			}
		}
	}
}

#ifdef __cplusplus
}
#endif


