/*
 * @file Logger.c
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

static char uart_buf[UART_TX_BUFFER_SIZE];  // Buffer cho uart_printf thuong

#if USING_UART_DMAPHORE
static char uart_buf_dma[UART_TX_BUFFER_SIZE]; // Buffer cho uart_print dung DMA + Semaphore
#endif

SemaphoreHandle_t sem_uart_log = NULL;
SemaphoreHandle_t __attribute__((unused)) sem_uart_tx_done = NULL;
TaskHandle_t logger_task = NULL;
QueueHandle_t logger_queue = NULL;
osThreadId logger_taskId = NULL;

/*-----------------------------------------------------------*/

void Logger_i2c_scanner(I2C_HandleTypeDef *hi2c){
	uart_printf("[LOGGER] Starting I2C Scanner...\r\n");

	for(uint8_t address = 0x08; address <= 0x77; address++){
		HAL_StatusTypeDef result = HAL_I2C_IsDeviceReady(hi2c, address << 1, MAX_RETRY_SCANNER, 50); //Dich 7-bit to 8-bit addr

		// Ham tren se di qua tung dia chi mot de xem xem co dia chi nao tra ve HAL_OK khong
		// Moi vong for gia tri cua result se thay doi lien tuc tuy theo address -> Nen dat lam bien cuc bo
		// De result thanh global thi gia tri cua no se bi ghi de lien tuc -> Anh huong neu co nhieu task goi ham nay
		if(result == HAL_OK){
			uart_printf("[LOGGER] I2C device found at address: 0x%02X\r\n", address); // Chi luu gia tri address phat hien duoc
		}
	}

	uart_printf("[LOGGER] I2C Scanner complete !\r\n");
}

/*-----------------------------------------------------------*/

void isQueueFree(const QueueHandle_t queue, const char *name){
	UBaseType_t used = uxQueueMessagesWaiting(queue);
	UBaseType_t free = uxQueueSpacesAvailable(queue);

	if(free == 0){
		uart_printf("[ERROR] Queue %s is FULL ! Used: %lu\r\n", name, used);
	}else if(free < 5){ // Canh bao khi queue sap day
		uart_printf("[WARN] Queue %s almost FULL ! Used: %lu, Free: %lu", name, used, free);
	}
}

/*-----------------------------------------------------------*/
HAL_StatusTypeDef Logger_init_ver1(void){
	HAL_StatusTypeDef ret = HAL_OK;

	// Tao Queue cho Logger
	logger_queue = xQueueCreate(LOGGER_QUEUE_LENGTH, sizeof(sensor_block_t));
	if(logger_queue == NULL){
	  uart_printf("[LOGGER] Failed to create logger_queue !\r\n");
	  ret |= HAL_ERROR;
	  Error_Handler(); //Thay vi dung while(1) -> Dung quy chuan
	}

	// Tao Semaphore Mutex cho DMA UART cua Logger
	sem_uart_log = xSemaphoreCreateMutex();
	if(sem_uart_log == NULL){
		uart_printf("[LOGGER] Failed to create Semaphores Mutex !\r\n");
		ret |= HAL_ERROR;
	}

	return ret;
}

/*-----------------------------------------------------------*/

__attribute__((weak)) void uart_printf(const char *fmt,...){
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(uart_buf, sizeof(uart_buf), fmt, args);
	va_end(args);

	// Phai vao RTOS xong, khi Scheduler chay thi Semaphore moi chay

	// Neu Semaphore da khoi tao xong va Scheduler dang chay (vao RTOS)
	if((sem_uart_log != NULL) && (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)){
		if(xSemaphoreTake(sem_uart_log, portMAX_DELAY) == pdTRUE){
			HAL_UART_Transmit(&huart2, (uint8_t*)uart_buf, len, pdMS_TO_TICKS(100)); // Blocking mode
			xSemaphoreGive(sem_uart_log);
		}
	}else{
		// Neu khong Semaphore chua duoc tao hoac Scheduler chua chay (vi du goi truoc khi RTOS bat dau)
		HAL_UART_Transmit(&huart2, (uint8_t*)uart_buf, len, pdMS_TO_TICKS(100)); // Blocking mode
	}

}

/*-----------------------------------------------------------*/

// Tinh chenh lech thoi gian so voi time hien tai cua Logger
static inline void debugSYNC(sensor_block_t *ecg_block, sensor_block_t *ppg_block, sensor_block_t *pcg_block){
	TickType_t logger_timestamp = xTaskGetTickCount(); //Lay tick de so sanh do tre voi cac task khac

	uart_printf("[SYNC] ECG_tick: %lu, PPG_tick: %lu, PCG_tick: %lu, Logger_tick: %lu | delECG: %ld, delPPG: %ld, delPCG: %ld\r\n",
			ecg_block->timestamp,
			ppg_block->timestamp,
			pcg_block->timestamp,
			logger_timestamp,
			(int32_t)(logger_timestamp - ecg_block->timestamp),
			(int32_t)(logger_timestamp - ppg_block->timestamp),
			(int32_t)(logger_timestamp - pcg_block->timestamp));
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
	uart_printf("LOGGER task started !\r\n");

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

					debugSYNC(&ecg_block, &ppg_block, &pcg_block);

					// Neu ID cua cac sample match nhau
					if(ecg_block.sample_id == ppg_block.sample_id && ecg_block.sample_id == pcg_block.sample_id){

						// Lay so sample be nhat trong 3 kenh thu duoc
						uint16_t min_count = MIN(MIN(ecg_block.count, ppg_block.count), pcg_block.count);

						uart_printf("[LOGGER] ECG count - ID: %d - %lu, PPG count - ID: %d - %lu, PCG count - ID: %d - %lu\r\n",
								ecg_block.count, ecg_block.sample_id,
								ppg_block.count, ppg_block.sample_id,
								pcg_block.count, pcg_block.sample_id);

						for(uint16_t i = 0; i < min_count; i++){ //In theo so luong sample nho nhat trong 3 data
							uart_printf("%d,%lu,%d\n",
									ecg_block.ecg[i],
									ppg_block.ppg.ir[i],
									pcg_block.mic[i]);
						}

						// Kiem tra hang doi
						isQueueFree(logger_queue, "LOGGER");

						//Reset flag
						ResetSensor_flag(&sensor_check);

					}else{
						uart_printf("[LOGGER] Sample_id mismatch ! ECG: %lu, PPG: %lu, PCG: %lu\r\n",
								ecg_block.sample_id, ppg_block.sample_id, pcg_block.sample_id);

						//Reset de sync lai vong sau
						ResetSensor_flag(&sensor_check);
					}
				}else{
					uart_printf("[LOGGER] Timestamp mismatch ! ECG: %lu, PPG: %lu, PCG: %lu\r\n",
							ecg_block.timestamp, ppg_block.timestamp, pcg_block.timestamp);

					ResetSensor_flag(&sensor_check);
				}

			}else{
				uart_printf("[LOGGER] Some data is not ready ! ECG: %d, PPG: %d, PCG: %d\r\n",
						sensor_check.ecg_ready, sensor_check.max_ready, sensor_check.mic_ready);
			}
		}else{
			uart_printf("[LOGGER] Logger queue timeout !\r\n");
		}
	}
}
/*-----------------------------------------------------------*/

void Logger_two_task(void const *pvParameter){
	(void)(pvParameter);
	uart_printf("LOGGER task started !\r\n");

	sensor_block_t block;
	sensor_block_t __attribute__((unused))ppg_block = {0};
	sensor_block_t __attribute__((unused))pcg_block = {0};
	sensor_block_t __attribute__((unused))ecg_block = {0};

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
			if((sensor_check.ecg_ready && sensor_check.max_ready) || (sensor_check.max_ready && sensor_check.mic_ready)
																|| (sensor_check.ecg_ready && sensor_check.mic_ready)){
				// Neu timestamp match nhau
				if((ecg_block.timestamp == ppg_block.timestamp) || (ecg_block.timestamp == pcg_block.timestamp)
															  || (ppg_block.timestamp == pcg_block.timestamp)){

//					debugSYNC(&ecg_block, &ppg_block, &pcg_block);

					// Neu ID cua cac sample match nhau
					if((ecg_block.sample_id == ppg_block.sample_id) || (ecg_block.sample_id == pcg_block.sample_id)
																  || (ppg_block.sample_id == pcg_block.sample_id)){

						// Lay so sample be nhat trong 3 kenh thu duoc
						uint16_t min_count = MIN(ecg_block.count, ppg_block.count);

//						uart_printf("[LOGGER] ECG count - ID: %d - %lu, PPG count - ID: %d - %lu, PCG count - ID: %d - %lu\r\n",
//								ecg_block.count, ecg_block.sample_id,
//								ppg_block.count, ppg_block.sample_id,
//								pcg_block.count, pcg_block.sample_id);

						for(uint16_t i = 0; i < min_count; i++){ //In theo so luong sample nho nhat trong 3 data
							uart_printf("%d,%lu,%d\n",
									ecg_block.ecg[i],
									ppg_block.ppg.ir[i],
									pcg_block.mic[i]);
						}

						// Kiem tra hang doi
						isQueueFree(logger_queue, "LOGGER");

						//Reset flag
						ResetSensor_flag(&sensor_check);

					}else{
						uart_printf("[LOGGER] Sample_id mismatch ! ECG: %lu, PPG: %lu, PCG: %lu\r\n",
								ecg_block.sample_id, ppg_block.sample_id, pcg_block.sample_id);

						//Reset de sync lai vong sau
						ResetSensor_flag(&sensor_check);
					}
				}else{
					uart_printf("[LOGGER] Timestamp mismatch ! ECG: %lu, PPG: %lu, PCG: %lu\r\n",
							ecg_block.timestamp, ppg_block.timestamp, pcg_block.timestamp);

					ResetSensor_flag(&sensor_check);
				}

			}else{
//				uart_printf("[LOGGER] Some data is not ready ! ECG: %d, PPG: %d, PCG: %d\r\n",
//						sensor_check.ecg_ready, sensor_check.max_ready, sensor_check.mic_ready);
			}
		}else{
			uart_printf("[LOGGER] Logger queue timeout !\r\n");
		}
	}
}
/*-----------------------------------------------------------*/

void Logger_one_task(void const *pvParameter){
	UNUSED(pvParameter);
	uart_printf("LOGGER task started !\r\n");

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
//				uart_printf("[LOGGER] ID: %u | COUNT: %u | TIME: %lu\r\n", ecg_block.sample_id, ecg_block.count, ecg_block.timestamp);

				for(uint16_t i = 0; i < MAX_COUNT; i++){
					uart_printf("%d\r\n", ecg_block.ecg[i]);
				}

				isQueueFree(logger_queue, "LOGGER");

				ResetSensor_flag(&check);
			}
		}else{
			uart_printf("[LOGGER] Logger queue timeout ! \r\n");
		}

	}

}

/*-----------------------------------------------------------*/
#if USING_UART_DMAPHORE

HAL_StatusTypeDef Logger_init_ver2(void){
	HAL_StatusTypeDef ret = HAL_OK;

	// Tao Queue cho Logger
	logger_queue = xQueueCreate(LOGGER_QUEUE_LENGTH, sizeof(sensor_block_t));
	if(logger_queue == NULL){
	  uart_printf("[LOGGER] Failed to create logger_queue !\r\n");
	  ret |= HAL_ERROR;
	  Error_Handler(); //Thay vi dung while(1) -> Dung quy chuan
	}

	// Tao Semaphore Mutex cho DMA UART cua Logger
	sem_uart_log = xSemaphoreCreateMutex();
	if(sem_uart_log == NULL){
		uart_printf("[LOGGER] Failed to create Semaphores Mutex !\r\n");
		ret |= HAL_ERROR;
	}

	// Tao Semaphore Binary de kiem tra khi DMA cua UART xong
	sem_uart_tx_done = xSemaphoreCreateBinary();
	if(sem_uart_tx_done == NULL){
		uart_printf("[LOGGER] Failed to create Semaphores Binary !\r\n");
		ret |= HAL_ERROR;
	}

	// Give Semaphores lan dau tien cho DMA de Take khong bi block
	if(xSemaphoreGive(sem_uart_tx_done) != pdTRUE){
		uart_printf("[LOGGER] Failed to Give Semaphores Binary for first time !\r\n");
		ret |= HAL_ERROR;
	}

	return ret;
}

__attribute__((weak, unused)) void uart_printf_dmaphore(const char *buffer, uint16_t buflen){

	// Neu Semaphore da khoi tao xong va Scheduler dang chay (vao RTOS)
	if((sem_uart_log != NULL) && (sem_uart_tx_done != NULL) && (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)){

		// Mutex khi khoi tao mac dinh no o trang thai "available" nen Take Mutex duoc luon
		if(xSemaphoreTake(sem_uart_log, portMAX_DELAY) == pdTRUE){
			if(buflen > 0){

				// Nhan Semaphore tu callback de bat dau ghi data vao DMA
				if(xSemaphoreTake(sem_uart_tx_done, portMAX_DELAY) == pdTRUE){

					// Copy du lieu an toan
					uint16_t copy_len = buflen;
					if(copy_len > sizeof(uart_buf_dma)) copy_len = sizeof(uart_buf_dma);
					memcpy(uart_buf_dma, buffer, copy_len);

					// Bat dau ghi vao DMA bang Non-blocking mode
					HAL_StatusTypeDef ret = HAL_UART_Transmit_DMA(&huart2, (uint8_t*)uart_buf_dma, (uint16_t)copy_len);
					if(ret != HAL_OK){
						xSemaphoreGive(sem_uart_tx_done); // Tra token cho DMA
						xSemaphoreGive(sem_uart_log); // Release mutex
						return;
					}
				}else{
					// Neu khong Take duoc Semaphore callback -> Give Mutex de tranh Mutex bi giu mai
					xSemaphoreGive(sem_uart_log);
					return;
				}
			}else{
				// Neu khong co gi de truyen -> Give Mutex de tranh Mutex bi giu mai
				xSemaphoreGive(sem_uart_log);
			}
		}

	}else{
		// Neu khong Semaphore chua duoc tao hoac Scheduler chua chay (vi du goi truoc khi RTOS bat dau)
		HAL_UART_Transmit(&huart2, (uint8_t*)buffer, buflen, pdMS_TO_TICKS(100)); // Blocking mode
	}
}

// DMA callback
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart){
	if(huart == &huart2){
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;

		// Giai phong Binary Semaphore sau khi ghi DMA hoan tat
		xSemaphoreGiveFromISR(sem_uart_tx_done, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

/*-----------------------------------------------------------*/

void __attribute__((unused)) Logger_two_task_dmaphore(void const *pvParameter){
	(void)(pvParameter);
	uart_printf("LOGGER task started !\r\n");

	static char batch_buffer[2048]; // Buffer cuc bo tren stack cua task Logger
	sensor_block_t block;
	sensor_block_t __attribute__((unused))ppg_block = {0};
	sensor_block_t __attribute__((unused))pcg_block = {0};
	sensor_block_t __attribute__((unused))ecg_block = {0};

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
			if((sensor_check.ecg_ready && sensor_check.max_ready) || (sensor_check.max_ready && sensor_check.mic_ready)
																  || (sensor_check.ecg_ready && sensor_check.mic_ready)){
				// Neu timestamp match nhau
				if((ecg_block.timestamp == ppg_block.timestamp) || (ecg_block.timestamp == pcg_block.timestamp)
															    || (ppg_block.timestamp == pcg_block.timestamp)){

//					debugSYNC(&ecg_block, &ppg_block, &pcg_block);

					// Neu ID cua cac sample match nhau
					if((ecg_block.sample_id == ppg_block.sample_id) || (ecg_block.sample_id == pcg_block.sample_id)
																    || (ppg_block.sample_id == pcg_block.sample_id)){

						// Lay so sample be nhat trong 3 kenh thu duoc
						uint16_t min_count = MIN(ecg_block.count, ppg_block.count);
						int offset = 0; // Vi tri ghi

//						uart_printf("[LOGGER] ECG count - ID: %d - %lu, PPG count - ID: %d - %lu, PCG count - ID: %d - %lu\r\n",
//								ecg_block.count, ecg_block.sample_id,
//								ppg_block.count, ppg_block.sample_id,
//								pcg_block.count, pcg_block.sample_id);

						// Gop du lieu
						for(uint16_t i = 0; i < min_count; i++){

							// Cong don so ky tu vao offset -> Lan lap tiep theo se ghi tiep noi chuoi vua roi
							offset += snprintf(batch_buffer + offset, sizeof(batch_buffer) - offset,
												"%d,%lu,%d\n",
												ecg_block.ecg[i],
												ppg_block.ppg.ir[i],
												pcg_block.mic[i]);

							if(offset >= sizeof(batch_buffer) - 50){
								break; // Neu buffer gan day, thoat vong lap
							}
						}

						// Goi ham in qua Mutex/DMA chi mot lan
						if(offset > 0){
							uart_printf_dmaphore(batch_buffer, offset);
							vTaskDelay(pdMS_TO_TICKS(2)); // Cho DMA hoan tat
						}

						// Kiem tra hang doi
						isQueueFree(logger_queue, "LOGGER");

						//Reset flag
						ResetSensor_flag(&sensor_check);

					}else{
						uart_printf("[LOGGER] Sample_id mismatch ! ECG: %lu, PPG: %lu, PCG: %lu\r\n",
								ecg_block.sample_id, ppg_block.sample_id, pcg_block.sample_id);

						//Reset de sync lai vong sau
						ResetSensor_flag(&sensor_check);
					}
				}else{
					uart_printf("[LOGGER] Timestamp mismatch ! ECG: %lu, PPG: %lu, PCG: %lu\r\n",
							ecg_block.timestamp, ppg_block.timestamp, pcg_block.timestamp);

					ResetSensor_flag(&sensor_check);
				}

			}else{
//				uart_printf("[LOGGER] Some data is not ready ! ECG: %d, PPG: %d, PCG: %d\r\n",
//						sensor_check.ecg_ready, sensor_check.max_ready, sensor_check.mic_ready);
			}
		}else{
			uart_printf("[LOGGER] Logger queue timeout !\r\n");
		}
	}
}

#endif // USING_UART_DMAPHORE
/*-----------------------------------------------------------*/

#ifdef __cplusplus
}
#endif
