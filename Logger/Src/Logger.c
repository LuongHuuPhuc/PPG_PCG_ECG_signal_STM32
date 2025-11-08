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
#include "cmsis_os.h"
#include "Sensor_config.h"
#include "Logger.h"

char uart_buf[1024];
TaskHandle_t logger_task = NULL;
QueueHandle_t logger_queue = NULL;
osThreadId logger_taskId = NULL;

/*-----------------------------------------------------------*/

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
/*-----------------------------------------------------------*/

__attribute__((weak)) void uart_printf(const char *fmt,...){
	va_list args;
	va_start(args, fmt);
	vsnprintf(uart_buf, sizeof(uart_buf), fmt, args);
	va_end(args);
	HAL_UART_Transmit(&huart2, (uint8_t*)uart_buf, strlen(uart_buf), HAL_MAX_DELAY);
}
/*-----------------------------------------------------------*/

void __attribute__((unused))isQueueFree(const QueueHandle_t queue, const char *name){
	UBaseType_t used = uxQueueMessagesWaiting(queue);
	UBaseType_t free = uxQueueSpacesAvailable(queue);

	if(free == 0){
		uart_printf("[ERROR] Queue %s is FULL ! Used: %lu\r\n", name, used);
	}else if(free < 5){ // Canh bao khi queue sap day
		uart_printf("[WARN] Queue %s almost FULL ! Used: %lu, Free: %lu", name, used, free);
	}

}
/*-----------------------------------------------------------*/

HAL_StatusTypeDef Logger_queue_init(void){
	logger_queue = xQueueCreate(LOGGER_QUEUE_LENGTH, sizeof(sensor_block_t));
	if(logger_queue == NULL){
	  uart_printf("[ERROR] Failed to create logger_queue !\r\n");
	  return HAL_ERROR;
	  Error_Handler(); //Thay vi dung while(1) -> Dung quy chuan
	}
	return HAL_OK;
}
/*-----------------------------------------------------------*/

static inline void debugSYNC(sensor_block_t *ecg_block, sensor_block_t *ppg_block, sensor_block_t *pcg_block){
	TickType_t now = xTaskGetTickCount(); //Lay tick de so sanh do tre voi cac task khac

	uart_printf("[SYNC] ECG_tick: %lu, PPG_tick: %lu, PCG_tick: %lu, Logger: %lu | delECG: %ld, delPPG: %ld, delPCG: %ld\r\n",
			ecg_block->timestamp,
			ppg_block->timestamp,
			pcg_block->timestamp,
			now,
			(int32_t)(now - ecg_block->timestamp),
			(int32_t)(now - ppg_block->timestamp),
			(int32_t)(now - pcg_block->timestamp));

}
/*-----------------------------------------------------------*/

//Reset trang thai flag ready cua sensor
static inline void ResetSensor_flag(sensor_check_t *check){
	check->ecg_ready = false;
	check->mic_ready = false;
	check->max_ready = false;
}
/*-----------------------------------------------------------*/

void Logger_task_block(void const *pvParameter){
	// Do PCG - 8000Hz (32ms thu dc 256 samples), con MAX va ADC la 1000Hz (32ms thu duoc 32 sample)
	// -> 1 sample cua PCG ung voi 8 samples cua PPG + ECG
	// -> Log voi 1 sample ECG + 1 sample PPG + 1 sample PCG sau khi downsample tu 8kHz ve 1kHz (lay trung binh mau)

	(void)(pvParameter);
	sensor_block_t ecg_block = {0}, ppg_block = {0}, pcg_block = {0}, block;
	sensor_check_t sensor_check = {
			.ecg_ready = false,
			.max_ready = false,
			.mic_ready = false
	};

	while(1){
		memset(&block, 0, sizeof(sensor_block_t));
		if(xQueueReceive(logger_queue, &block, pdMS_TO_TICKS(portMAX_DELAY)) == pdTRUE){
			switch(block.type){
			case SENSOR_ECG:
				ecg_block = block; // Copy du lieu tu block sang ecg_block neu la type ECG
				sensor_check.ecg_ready = true;
				break;
			case SENSOR_PPG:
				ppg_block = block; //Copy du lieu tu block sang ppg_block neu la type PPG
				sensor_check.max_ready = true;
				break;
			case SENSOR_PCG:
				pcg_block = block; //Copy du lieu tu block sang pcg_block neu la type PCG
				sensor_check.mic_ready = true;
				break;
			}

			// Neu 3 data da ready
			if(sensor_check.ecg_ready && sensor_check.max_ready && sensor_check.mic_ready){
				// Neu timestamp match nhau
				if(ecg_block.timestamp == ppg_block.timestamp && ecg_block.timestamp == pcg_block.timestamp){

					// Tinh chenh lech thoi gian so voi time hien tai
					debugSYNC(&ecg_block, &ppg_block, &pcg_block);

					// Neu ID cua cac sample match nhau
					if(ecg_block.sample_id == ppg_block.sample_id && ecg_block.sample_id == pcg_block.sample_id){

						// Lay so sample be nhat trong 3 kenh thu duoc
						uint16_t min_count = MIN(MIN(ecg_block.count, ppg_block.count), pcg_block.count);

//						uart_printf("[DEBUG] ECG count: %d, PPG count: %d, PCG count: %d, ID: %lu\r\n",
//								ecg_block.count,
//								ppg_block.count,
//								pcg_block.count,
//								ecg_block.sample_id);

						for(uint16_t i = 0; i < min_count; i++){ //In theo so luong sample nho nhat trong 3 data
							uart_printf("%d,%lu,%lu,%d\n",
									ecg_block.ecg[i],
									ppg_block.ppg.red[i],
									ppg_block.ppg.ir[i],
									pcg_block.mic[i]);
						}

						// Kiem tra hang doi
						isQueueFree(logger_queue, "LOGGER");

						//Reset flag
						ResetSensor_flag(&sensor_check);

					}else{
						uart_printf("[WARN] Sample_id mismatch ! ECG: %lu, PPG: %lu, PCG: %lu\r\n",
								ecg_block.sample_id, ppg_block.sample_id, pcg_block.sample_id);

						//Reset de sync lai vong sau
						ResetSensor_flag(&sensor_check);
					}
				}else{
					uart_printf("[WARN] Timestamp mismatch ! ECG: %lu, PPG: %lu, PCG: %lu\r\n",
							ecg_block.timestamp, ppg_block.timestamp, pcg_block.timestamp);

					ResetSensor_flag(&sensor_check);
				}

			}else{
				uart_printf("[WARN] Some data is not ready ! ECG: %d, PPG: %d, PCG: %d\r\n",
						sensor_check.ecg_ready, sensor_check.max_ready, sensor_check.mic_ready);
			}
		}else{
			uart_printf("[ERROR] Logger queue timeout !\r\n");
		}
	}
}
/*-----------------------------------------------------------*/

void Logger_one_task(void const *pvParameter){
	UNUSED(pvParameter);

	sensor_block_t block;
	sensor_block_t __attribute__((unused))ppg_block = {0};
	sensor_block_t __attribute__((unused))pcg_block = {0};
	sensor_block_t __attribute__((unused))ecg_block = {0};

	sensor_check_t check = {
			.ecg_ready = false,
			.max_ready = false,
			.mic_ready = false
	};

	memset(&block, 0, sizeof(sensor_block_t));
	while(1){
		if(xQueueReceive(logger_queue, &block, pdMS_TO_TICKS(MAX_TIMEOUT)) == pdTRUE){
			switch(block.type){
			case SENSOR_PCG:
				pcg_block = block;
				check.mic_ready = true;
				break;
			case SENSOR_PPG:
				ppg_block = block;
				check.max_ready = true;
				break;
			case SENSOR_ECG:
				ecg_block = block;
				check.ecg_ready = true;
				break;
			}

			if(check.ecg_ready || check.max_ready || check.mic_ready){
				uart_printf("[DEBUG] ID: %u | COUNT: %u | TIME: %lu\r\n", ppg_block.sample_id, ppg_block.count, ppg_block.timestamp);

				for(uint16_t i = 0; i < MAX_COUNT; i++){
					uart_printf("%lu,%lu\r\n", ppg_block.ppg.ir[i], ppg_block.ppg.red[i]);
				}

				isQueueFree(logger_queue, "LOGGER");

				ResetSensor_flag(&check);
			}
		}else{
			uart_printf("Logger queue timeout ! \r\n");
		}

	}

}
/*-----------------------------------------------------------*/

#ifdef __cplusplus
}
#endif
