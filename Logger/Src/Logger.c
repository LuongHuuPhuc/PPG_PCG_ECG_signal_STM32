/*
 * @file Logger.c
 *
 *  Created on: Jun 17, 2025
 *  @Author Luong Huu Phuc
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "Logger.h"
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "MicroSD_config.h"

#ifdef USING_UART_DMAPHORE

#if defined(FREERTOS_API_USING)
SemaphoreHandle_t loggerDMA_sem = NULL;
#elif defined(CMSIS_API_USING)
osSemaphoreId loggerDMA_semId = NULL;
#endif // CMSIS_API_USING

static char uart_buf_dma[UART_TX_BUFFER_SIZE]; // Buffer cho uart_print dung DMA + Semaphore

#endif // USING_UART_DMAPHORE

#if defined(FREERTOS_API_USING)

SemaphoreHandle_t logger_mutex = NULL;
TaskHandle_t logger_task = NULL;
QueueHandle_t logger_queue = NULL;

#elif defined(CMSIS_API_USING)

osMailQId logger_queueId = NULL;
osMutexId logger_mutexId = NULL;
osThreadId logger_taskId = NULL;

#endif // CMSIS_API_USING

/*-----------------------------------------------------------*/

HAL_StatusTypeDef Logger_init(void){
	HAL_StatusTypeDef ret = HAL_OK;

#if defined (CMSIS_API_USING)

#ifdef SYNC_TO_LOGGER_MAIL_USING

	// Tao Queue cho Logger de nhan block tu Sync
	osMailQDef(loggerQueueName, LOGGER_QUEUE_LENGTH, sensor_sync_block_t);
	logger_queueId = osMailCreate(osMailQ(loggerQueueName), NULL);
	if(logger_queueId == NULL){
		uart_printf("[LOGGER] Failed to create logger_queue !\r\n");
		ret |= HAL_ERROR;
	}

#else

	osMailQDef(loggerQueueName, LOGGER_QUEUE_LENGTH, sensor_block_t);
		logger_queueId = osMailCreate(osMailQ(loggerQueueName), NULL);
		if(logger_queueId == NULL){
			uart_printf("[LOGGER] Failed to create logger_queue !\r\n");
			ret |= HAL_ERROR;
		}

#endif // SYNC_TO_LOGGER_MAIL_USING

	// Tao Mutex cho UART printf
	osMutexDef(loggerMutexName);
	logger_mutexId = osMutexCreate(osMutex(loggerMutexName));
	if(logger_mutexId == NULL){
		uart_printf("[LOGGER] Failed to create Semaphores Mutex !\r\n");
		ret |= HAL_ERROR;
	}

#elif defined(FREERTOS_API_USING)

	// Tao Queue cho Logger
	logger_queue = xQueueCreate(LOGGER_QUEUE_LENGTH, sizeof(sensor_block_t));
	if(logger_queue == NULL){
	  uart_printf("[LOGGER] Failed to create logger_queue !\r\n");
	  ret |= HAL_ERROR;
	}

	// Tao Semaphore Mutex cho DMA UART cua Logger
	logger_mutex = xSemaphoreCreateMutex();
	if(logger_mutex == NULL){
		uart_printf("[LOGGER] Failed to create Semaphores Mutex !\r\n");
		ret |= HAL_ERROR;
	}

#endif // CMSIS_API_USING

	return ret;
}

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
#ifdef FREERTOS_API_USING

void isQueueFree(const QueueHandle_t queue, const char *name){
	UBaseType_t used = uxQueueMessagesWaiting(queue);
	UBaseType_t free = uxQueueSpacesAvailable(queue);

	if(free == 0){
		uart_printf("[ERROR] Queue %s is FULL ! Used: %lu\r\n", name, used);
	}else if(free < 5){ // Canh bao khi queue sap day
		uart_printf("[WARN] Queue %s almost FULL ! Used: %lu, Free: %lu", name, used, free);
	}
}

#endif // FREERTOS_API_USING

/*-----------------------------------------------------------*/

__attribute__((weak)) void uart_printf(const char *fmt,...){
	// Giam stack usage
	static char uart_buf[UART_TX_BUFFER_SIZE];  // Buffer cho uart_printf thuong
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(uart_buf, sizeof(uart_buf), fmt, args);
	va_end(args);

	if(len <= 0) return;

	// Clamp len ve dung so byte thuc co trong buffer
	if(len >= (int)sizeof(uart_buf)){
		len = (int)sizeof(uart_buf) - 1; // vi vnsprintf da dam bao co '\0'
	}

#if defined(CMSIS_API_USING)

	// Phai vao RTOS xong, khi Scheduler chay thi Semaphore moi chay
	// Neu Semaphore da khoi tao xong va Scheduler dang chay (vao RTOS)
	if((logger_mutexId != NULL) && (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)){

		// Lay Semaphore de doc quyen truy cap UART (Tranh overwrite)
		if(osMutexWait(logger_mutexId, 100) == osOK){
			HAL_UART_Transmit(&huart2, (uint8_t*)uart_buf, (uint16_t)len, portMAX_DELAY); // Blocking mode

			// Tha Semaphore sau khi truyen UART xong
			osMutexRelease(logger_mutexId);
		}
	}

#elif defined (FREERTOS_API_USING)

	// Phai vao RTOS xong, khi Scheduler chay thi Semaphore moi chay
	// Neu Semaphore da khoi tao xong va Scheduler dang chay (vao RTOS)
	if((logger_mutex != NULL) && (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)){

		// Lay Semaphore de doc quyen truy cap UART (Tranh overwrite)
		if(xSemaphoreTake(logger_mutex, pdMS_TO_TICKS(100)) == pdTRUE){
			HAL_UART_Transmit(&huart2, (uint8_t*)uart_buf, len, portMAX_DELAY); // Blocking mode

			// Tha Semaphore sau khi truyen UART xong
			xSemaphoreGive(logger_mutex);
		}
	}

#endif // CMSIS_API_USING

	else{
		// Neu khong Semaphore chua duoc tao hoac Scheduler chua chay (non-RTOS context) (vi du goi truoc khi RTOS bat dau)
		HAL_UART_Transmit(&huart2, (uint8_t*)uart_buf, (uint16_t)len, pdMS_TO_TICKS(100)); // Blocking mode
	}
}

__attribute__((weak, unused)) void uart_printf_safe(const char *fmt,...){
	static char uart_isr_buf[256];
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf((char*)uart_isr_buf, sizeof(uart_isr_buf), fmt, args);
	va_end(args);

	if(len < 0) return;

	// Clamp len ve dung so byte thuc co trong buffer
	if(len >= (int)sizeof(uart_isr_buf)){
		len = (int)sizeof(uart_isr_buf) - 1; // vi vnsprintf da dam bao co '\0'
	}

	HAL_UART_Transmit(&huart2, (uint8_t*)uart_isr_buf, (uint16_t)len, pdMS_TO_TICKS(100));
	// ....
}

/*-----------------------------------------------------------*/

// Tinh chenh lech thoi gian so voi time hien tai cua Logger
__attribute__((unused)) static inline void debugSYNC(sensor_block_t *ecg_block, sensor_block_t *ppg_block, sensor_block_t *pcg_block){
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
__attribute__((unused)) static inline void ResetSensor_flag(sensor_check_t *check){
	check->ecg_ready = false;
	check->mic_ready = false;
	check->max_ready = false;
}

/*-----------------------------------------------------------*/

#ifdef SYNC_TO_LOGGER_MAIL_USING
void Logger_three_task_ver2(void const *pvParameter){
	(void)(pvParameter);
	uart_printf("LOGGER task started !\r\n");

	osEvent evt;
	sensor_sync_block_t *data;

	while(1){
		evt = osMailGet(logger_queueId, osWaitForever);
		if(evt.status != osEventMail){
			uart_printf("[LOGGER] Mail error: %d\r\n", evt.status);
		}

		// Lay con tro Mail
		data = evt.value.p;

//		uart_printf("[LOGGER] ID: %lu | COUNT: %u | TIME: %lu\r\n", data->sample_id, data->count, data->timestamp);

		for(uint16_t i = 0; i < data->count; i++){ //In theo so luong sample nho nhat trong 3 data
			uart_printf("%d,%lu,%ld\n",
					data->ecg[i],
					data->ppg_ir[i],
					data->mic[i]);
		}
		osMailFree(logger_queueId, data); // Giai phong Mail
	}
}
#endif // SYNC_TO_LOGGER_MAIL_USING

/*-----------------------------------------------------------*/

__attribute__((unused)) void Logger_three_task_ver1(void const *pvParameter){
	(void)(pvParameter);
	uart_printf("LOGGER task started !\r\n");

#if defined(CMSIS_API_USING)
	sensor_block_t *block;
	osEvent evt;
#elif defined(FREERTOS_API_USING)
	sensor_block_t block;
#endif // CMSIS_API_USING

	sensor_block_t ecg_block = {0}, ppg_block = {0}, pcg_block = {0};
	sensor_check_t sensor_check = {
			.ecg_ready = false,
			.max_ready = false,
			.mic_ready = false
	};

	while(1){

#ifdef CMSIS_API_USING

		evt = osMailGet(logger_queueId, 200);
		if(evt.status == osEventMail){
			block = evt.value.p;

			switch(block->type){
			case SENSOR_ECG:
				ecg_block = *block; // Copy du lieu tu block sang ecg_block neu la type ECG
				sensor_check.ecg_ready = true;
				break;
			case SENSOR_PPG:
				ppg_block = *block; //Copy du lieu tu block sang ppg_block neu la type PPG
				sensor_check.max_ready = true;
				break;
			case SENSOR_PCG:
				pcg_block = *block; //Copy du lieu tu block sang pcg_block neu la type PCG
				sensor_check.mic_ready = true;
				break;
			}

			osMailFree(logger_queueId, block); // Giai phong bo nho heap ngay de cho block khac gui

			// Neu 3 data da ready
			if(sensor_check.ecg_ready && sensor_check.max_ready && sensor_check.mic_ready){

				// Neu timestamp match nhau
				if(ecg_block.timestamp == ppg_block.timestamp && ecg_block.timestamp == pcg_block.timestamp){

//					debugSYNC(&ecg_block, &ppg_block, &pcg_block);

					// Neu ID cua cac sample match nhau
					if(ecg_block.sample_id == ppg_block.sample_id && ecg_block.sample_id == pcg_block.sample_id){

						// Lay so sample be nhat trong 3 kenh thu duoc
						uint16_t min_count = MIN(MIN(ecg_block.count, ppg_block.count), pcg_block.count);

						uart_printf("[LOGGER] ECG count - ID: %d - %lu, PPG count - ID: %d - %lu, PCG count - ID: %d - %lu\r\n",
								ecg_block.count, ecg_block.sample_id,
								ppg_block.count, ppg_block.sample_id,
								pcg_block.count, pcg_block.sample_id);

						for(uint16_t i = 0; i < min_count; i++){ //In theo so luong sample nho nhat trong 3 data
							uart_printf("%d,%lu,%ld\n",
									ecg_block.ecg[i],
									ppg_block.ppg.ir[i],
									pcg_block.mic[i]);
						}

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

		}else if(evt.status != osEventTimeout){ // Neu ticks != 0 thi se tra ve timeout
			uart_printf("[LOGGER] Mail error: %d\r\n", evt.status);
		}

#elif FREERTOS_API_USING

		memset(&block, 0, sizeof(sensor_block_t));
		if(xQueueReceive(logger_queue, &block, pdMS_TO_TICKS(200)) == pdTRUE){
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
							uart_printf("%d,%lu,%ld\n",
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

#endif // CMSIS_API_USING

	}
}
/*-----------------------------------------------------------*/

__attribute__((unused)) void Logger_two_task(void const *pvParameter){
	(void)(pvParameter);
	uart_printf("LOGGER task started !\r\n");

#if defined(CMSIS_API_USING)
	sensor_block_t *block;
	osEvent evt;
#elif defined(FREERTOS_API_USING)
	sensor_block_t block;
#endif // CMSIS_API_USING

	sensor_block_t __attribute__((unused))ppg_block = {0};
	sensor_block_t __attribute__((unused))pcg_block = {0};
	sensor_block_t __attribute__((unused))ecg_block = {0};

	sensor_check_t sensor_check = {
			.ecg_ready = false,
			.max_ready = false,
			.mic_ready = false
	};

	while(1){

#if defined(CMSIS_API_USING)

		evt = osMailGet(logger_queueId, 150);
		if(evt.status == osEventMail){
			block = evt.value.p;

			switch(block->type){
			case SENSOR_ECG:
				ecg_block = *block; // Copy du lieu tu block sang ecg_block neu la type ECG
				sensor_check.ecg_ready = true;
				break;
			case SENSOR_PPG:
				ppg_block = *block; //Copy du lieu tu block sang ppg_block neu la type PPG
				sensor_check.max_ready = true;
				break;
			case SENSOR_PCG:
				pcg_block = *block; //Copy du lieu tu block sang pcg_block neu la type PCG
				sensor_check.mic_ready = true;
				break;
			}

			osMailFree(logger_queueId, block); // Giai phong bo nho heap ngay de cho block khac gui

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

#if defined(AD8232_ONLY_LOGGER) && defined(MAX30102_ONLY_LOGGER)

						// Lay so sample be nhat trong 3 kenh thu duoc
						uint16_t min_count = MIN(ecg_block.count, ppg_block.count);

#elif defined(AD8232_ONLY_LOGGER) && defined(INMP441_ONLY_LOGGER)

						// Lay so sample be nhat trong 3 kenh thu duoc
						uint16_t min_count = MIN(ecg_block.count, pcg_block.count);

#elif defined(MAX30102_ONLY_LOGGER) && defined(INMP441_ONLY_LOGGER)

						// Lay so sample be nhat trong 3 kenh thu duoc
						uint16_t min_count = MIN(ppg_block.count, pcg_block.count);
#else
						uint16_t min_count = MAX_COUNT; // default
#endif // SENSOR_COUNT
						uart_printf("[LOGGER] ECG count - ID: %d - %lu, PPG count - ID: %d - %lu, PCG count - ID: %d - %lu\r\n",
								ecg_block.count, ecg_block.sample_id,
								ppg_block.count, ppg_block.sample_id,
								pcg_block.count, pcg_block.sample_id);

						for(uint16_t i = 0; i < min_count; i++){ //In theo so luong sample nho nhat trong 3 data
							uart_printf("%d,%lu,%ld\n",
									ecg_block.ecg[i],
									ppg_block.ppg.ir[i],
									pcg_block.mic[i]);
						}

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
		}else if(evt.status != osEventTimeout){ // Neu ticks != 0 thi se tra ve timeout
			uart_printf("[LOGGER] Mail error: %d\r\n", evt.status);
		}

#elif deifned (FREERTOS_API_USING)

		memset(&block, 0, sizeof(sensor_block_t));
		if(xQueueReceive(logger_queue, &block, pdMS_TO_TICKS(150)) == pdTRUE){
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

#if defined(AD8232_ONLY_LOGGER) && defined(MAX30102_ONLY_LOGGER)

						// Lay so sample be nhat trong 3 kenh thu duoc
						uint16_t min_count = MIN(ecg_block.count, ppg_block.count);

#elif defined(AD8232_ONLY_LOGGER) && defined(INMP441_ONLY_LOGGER)

						// Lay so sample be nhat trong 3 kenh thu duoc
						uint16_t min_count = MIN(ecg_block.count, pcg_block.count);

#elif defined(MAX30102_ONLY_LOGGER) && defined(INMP441_ONLY_LOGGER)

						// Lay so sample be nhat trong 3 kenh thu duoc
						uint16_t min_count = MIN(ppg_block.count, pcg_block.count);
#else
						uint16_t min_count = MAX_COUNT; // default
#endif // SENSOR_COUNT
						uart_printf("[LOGGER] ECG count - ID: %d - %lu, PPG count - ID: %d - %lu, PCG count - ID: %d - %lu\r\n",
								ecg_block.count, ecg_block.sample_id,
								ppg_block.count, ppg_block.sample_id,
								pcg_block.count, pcg_block.sample_id);

						for(uint16_t i = 0; i < min_count; i++){ //In theo so luong sample nho nhat trong 3 data
							uart_printf("%d,%lu,%ld\n",
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
#endif // CMSIS_API_USING

	}
}
/*-----------------------------------------------------------*/

__attribute__((unused)) void Logger_one_task(void const *pvParameter){
	UNUSED(pvParameter);
	uart_printf("LOGGER task started !\r\n");

#if defined(CMSIS_API_USING)
	sensor_block_t *block;
	osEvent evt;
#elif defined(FREERTOS_API_USING)
	sensor_block_t block;
#endif // CMSIS_API_USING

	sensor_block_t __attribute__((unused))ppg_block = {0};
	sensor_block_t __attribute__((unused))pcg_block = {0};
	sensor_block_t __attribute__((unused))ecg_block = {0};

	sensor_check_t check = {
			.ecg_ready = false,
			.max_ready = false,
			.mic_ready = false
	};

	while(1){

#if defined(CMSIS_API_USING)

		evt = osMailGet(logger_queueId, 100);
		if(evt.status == osEventMail){
			block = evt.value.p;  // Data o day

			switch(block->type){
			case SENSOR_PCG:
				pcg_block = *block;
				check.mic_ready = true;
				break;
			case SENSOR_PPG:
				ppg_block = *block;
				check.max_ready = true;
				break;
			case SENSOR_ECG:
				ecg_block = *block;
				check.ecg_ready = true;
				break;
			}

			osMailFree(logger_queueId, block); // Giai phong bo nho heap ngay de cho block khac gui

			if(check.ecg_ready || check.max_ready || check.mic_ready){

#if defined(MAX30102_ONLY_LOGGER)

//				uart_printf("[LOGGER] ID: %lu | COUNT: %u | TIME: %lu\r\n", ppg_block.sample_id, ppg_block.count, ppg_block.timestamp);

				for(uint16_t i = 0; i < ppg_block.count; i++){
					uart_printf("%lu\r\n", ppg_block.ppg.ir[i]);
				}

				ResetSensor_flag(&check);

#elif defined(AD8232_ONLY_LOGGER)

//				uart_printf("[LOGGER] ID: %lu | COUNT: %u | TIME: %ld\r\n", ecg_block.sample_id, ecg_block.count, ecg_block.timestamp);

				for(uint16_t i = 0; i < ecg_block.count; i++){
					uart_printf("%d\r\n", ecg_block.ecg[i]);
				}

				ResetSensor_flag(&check);

#elif defined(INMP441_ONLY_LOGGER)

//				uart_printf("[LOGGER] ID: %lu | COUNT: %u | TIME: %lu\r\n", pcg_block.sample_id, pcg_block.count, pcg_block.timestamp);

				for(uint16_t i = 0; i < pcg_block.count; i++){
					uart_printf("%ld\r\n", pcg_block.mic[i]);
				}

				ResetSensor_flag(&check);

#endif // SENSOR_MACROS

			}
		}else if(evt.status != osEventTimeout){ // Neu ticks != 0 thi se tra ve timeout
			uart_printf("[LOGGER] Mail error: %d\r\n", evt.status);
		}

#elif defined(FREERTOS_API_USING)

		memset(&block, 0, sizeof(sensor_block_t));
		if(xQueueReceive(logger_queue, &block, pdMS_TO_TICKS(100)) == pdTRUE){
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

#if defined(MAX30102_ONLY_LOGGER)

				uart_printf("[LOGGER] ID: %u | COUNT: %u | TIME: %lu\r\n", ppg_block.sample_id, ppg_block.count, ppg_block.timestamp);

				for(uint16_t i = 0; i < ppg_block.count; i++){
					uart_printf("%lu\r\n", ppg_block.ppg.ir[i]);
				}

				isQueueFree(logger_queue, "LOGGER");

				ResetSensor_flag(&check);

#elif defined(AD8232_ONLY_LOGGER)

//				uart_printf("[LOGGER] ID: %u | COUNT: %u | TIME: %lu\r\n", ecg_block.sample_id, ecg_block.count, ecg_block.timestamp);

				for(uint16_t i = 0; i < ecg_block.count; i++){
					uart_printf("%d\r\n", ecg_block.ecg[i]);
				}

				isQueueFree(logger_queue, "LOGGER");

				ResetSensor_flag(&check);

#elif defined(INMP441_ONLY_LOGGER)

//				uart_printf("[LOGGER] ID: %u | COUNT: %u | TIME: %lu\r\n", pcg_block.sample_id, pcg_block.count, pcg_block.timestamp);

				for(uint16_t i = 0; i < pcg_block.count; i++){
					uart_printf("%ld\r\n", pcg_block.mic[i]);
				}

				isQueueFree(logger_queue, "LOGGER");

				ResetSensor_flag(&check);

#endif // SENSOR_MACROS

			}
		}else{
			uart_printf("[LOGGER] Logger queue timeout ! \r\n");
		}
#endif // CMSIS_API_USING

	}
}

/*-----------------------------------------------------------*/
/* FIXME Doan code nay dang can duoc fix loi de su dung UART theo DMA (Hien tai chua dung duoc) */

#ifdef USING_UART_DMAPHORE

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
	logger_mutex = xSemaphoreCreateMutex();
	if(logger_mutex == NULL){
		uart_printf("[LOGGER] Failed to create Semaphores Mutex !\r\n");
		ret |= HAL_ERROR;
	}

	// Tao Semaphore Binary de kiem tra khi DMA cua UART xong
	loggerDMA_sem = xSemaphoreCreateBinary();
	if(loggerDMA_sem == NULL){
		uart_printf("[LOGGER] Failed to create Semaphores Binary !\r\n");
		ret |= HAL_ERROR;
	}

	// Give Semaphores lan dau tien cho DMA de Take khong bi block
	if(xSemaphoreGive(loggerDMA_sem) != pdTRUE){
		uart_printf("[LOGGER] Failed to Give Semaphores Binary for first time !\r\n");
		ret |= HAL_ERROR;
	}

	return ret;
}
/*-----------------------------------------------------------*/

__attribute__((weak, unused)) void uart_printf_dmaphore(const char *buffer, uint16_t buflen){

	// Neu Semaphore da khoi tao xong va Scheduler dang chay (vao RTOS)
	if((logger_mutex != NULL) && (loggerDMA_sem != NULL) && (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)){

		// Mutex khi khoi tao mac dinh no o trang thai "available" nen Take Mutex duoc luon
		if(xSemaphoreTake(logger_mutex, portMAX_DELAY) == pdTRUE){
			if(buflen > 0){

				// Nhan Semaphore tu callback de bat dau ghi data vao DMA
				if(xSemaphoreTake(loggerDMA_sem, portMAX_DELAY) == pdTRUE){

					// Copy du lieu an toan
					uint16_t copy_len = buflen;
					if(copy_len > sizeof(uart_buf_dma)) copy_len = sizeof(uart_buf_dma);
					memcpy(uart_buf_dma, buffer, copy_len);

					// Bat dau ghi vao DMA bang Non-blocking mode
					HAL_StatusTypeDef ret = HAL_UART_Transmit_DMA(&huart2, (uint8_t*)uart_buf_dma, (uint16_t)copy_len);
					if(ret != HAL_OK){
						xSemaphoreGive(loggerDMA_sem); // Tra token cho DMA
						xSemaphoreGive(logger_mutex); // Release mutex
						return;
					}
				}else{
					// Neu khong Take duoc Semaphore callback -> Give Mutex de tranh Mutex bi giu mai
					xSemaphoreGive(logger_mutex);
					return;
				}
			}else{
				// Neu khong co gi de truyen -> Give Mutex de tranh Mutex bi giu mai
				xSemaphoreGive(logger_mutex);
			}
		}

	}else{
		// Neu khong Semaphore chua duoc tao hoac Scheduler chua chay (vi du goi truoc khi RTOS bat dau)
		HAL_UART_Transmit(&huart2, (uint8_t*)buffer, buflen, pdMS_TO_TICKS(100)); // Blocking mode
	}
}
/*-----------------------------------------------------------*/

// DMA callback
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart){
	if(huart == &huart2){
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;

		// Giai phong Binary Semaphore sau khi ghi DMA hoan tat
		xSemaphoreGiveFromISR(loggerDMA_sem, &xHigherPriorityTaskWoken);
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

#if defined(AD8232_ONLY_LOGGER) && defined(MAX30102_ONLY_LOGGER)

						// Lay so sample be nhat trong 3 kenh thu duoc
						uint16_t min_count = MIN(ecg_block.count, ppg_block.count);

#elif defined(AD8232_ONLY_LOGGER) && defined(INMP441_ONLY_LOGGER)

						// Lay so sample be nhat trong 3 kenh thu duoc
						uint16_t min_count = MIN(ecg_block.count, pcg_block.count);

#elif defined(MAX30102_ONLY_LOGGER) && defined(INMP441_ONLY_LOGGER)

						// Lay so sample be nhat trong 3 kenh thu duoc
						uint16_t min_count = MIN(ppg_block.count, pcg_block.count);
#else
						uint16_t min_count = MAX_COUNT; // default
#endif

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
