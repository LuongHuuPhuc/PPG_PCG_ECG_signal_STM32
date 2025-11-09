/*
 * ad8232_config.h
 *
 *  Created on: Oct 15, 2025
 *      Author: ADMIN
 */

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include "stdio.h"
#include "cmsis_os.h"
#include "Logger.h"
#include "Sensor_config.h"
#include "take_snapsync.h"
#include "ad8232_config.h"

// ====== VARIABLES DEFINITION ======

volatile int16_t ecg_buffer[ECG_DMA_BUFFER] = {0};
TaskHandle_t ad8232_task = NULL;
SemaphoreHandle_t sem_adc = NULL;
osThreadId ad8232_taskId = NULL;

// ====== FUCNTION DEFINITION ======
/*-----------------------------------------------------------*/

HAL_StatusTypeDef __attribute__((unused))Ad8232_init_ver1(ADC_HandleTypeDef *adc){
	uart_printf("[AD8232] initializing...");
	HAL_StatusTypeDef ret = HAL_OK;
	ret |= ((adc == &hadc1) ? HAL_OK : HAL_ERROR);

	sem_adc = xSemaphoreCreateBinary();
	if(sem_adc == NULL){
		uart_printf("[AD8232] Failed to create Semaphores !\r\n");
		ret |= HAL_ERROR;
	}
	return ret;
}
/*-----------------------------------------------------------*/

// === VERSION 3 ===

void Ad8232_task_ver3(void const *pvParameter){
	(void)(pvParameter);

	sensor_block_t block;
	snapshot_sync_t snap;
	uart_printf("Ad8232 dma task started !\r\n");
	vTaskDelay(pdMS_TO_TICKS(5));

	//Ghi du lieu DMA vao ECG_DMA_BUFFER nay den khi du 32 mau
	// Trigger tu TIMER (sample rate = 1000Hz) moi mau 1ms la 1 lan doc ADC
	if(HAL_ADC_Start_DMA(&hadc1, (uint32_t*)&ecg_buffer, ECG_DMA_BUFFER) != HAL_OK){
		uart_printf("[AD8232] HAL_ADC_Start_DMA failed!\r\n");
	}

	while(1){
		if(xSemaphoreTake(sem_adc, portMAX_DELAY) == pdTRUE){ //Sau 32ms thi TIMER nha semaphore
			take_snapshotSYNC(&snap);

			if(ulTaskNotifyTake(pdTRUE, portMAX_DELAY) == pdTRUE){ //Doi notify DMA hoan tat tu ham ConvCpltCallback

				memset(&block, 0, sizeof(sensor_block_t)); //Set ve 0 de tranh byte rac
				block.type = SENSOR_ECG;
				block.timestamp = snap.timestamp;
				block.sample_id = snap.sample_id; //Danh dau thoi diem -> dong bo hoa
				block.count = ECG_DMA_BUFFER;

//				uart_printf("[DEBUG] ECG sample_id = %lu\r\n", block.sample_id);

				for(uint8_t i = 0; i < ECG_DMA_BUFFER; i++){
					block.ecg[i] = ecg_buffer[i]; //Copy gia tri sang bien ecg trong sensor_block_t
				}

				taskENTER_CRITICAL();
				QUEUE_SEND_FROM_TASK(&block); //Gui 32 sample vao queue
				taskEXIT_CRITICAL();

			}else{
				uart_printf("[ADC8232] Timeout waiting for DMA done !\r\n");
			}
		}else{
			uart_printf("[ADC8232] Can not take semaphore !r\n");
		}
	}
}
/*-----------------------------------------------------------*/

//Khi DMA day (du 32 sample ~ 32ms) ham nay se duoc goi
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *adc){
	if(adc == &hadc1){
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		vTaskNotifyGiveFromISR(ad8232_taskId, &xHigherPriorityTaskWoken); //gui thong bao cho task la DMA hoan tat
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}
/*-----------------------------------------------------------*/

// === VERSION 2 ===

void __attribute__((unused))Ad8232_task_ver2(void const *pvParameter){
	(void)(pvParameter);

	sensor_block_t block;
	uart_printf("Ad8232 dma task started !\r\n");
	vTaskDelay(pdMS_TO_TICKS(5));

	//Ghi du lieu DMA vao ECG_DMA_BUFFER nay den khi du 32 mau, trigger moi mau 1ms (sample rate = 1000Hz) la 1 lan doc ADC
	if(HAL_ADC_Start_DMA(&hadc1, (uint32_t*)&ecg_buffer, ECG_DMA_BUFFER) != HAL_OK){
		uart_printf("[AD8232] HAL_ADC_Start_DMA failed!\r\n");
	}

	while(1){
		if(ulTaskNotifyTake(pdTRUE, portMAX_DELAY) == pdTRUE){ //Doi notify DMA hoan tat tu ham ConvCpltCallback
			memset(&block, 0, sizeof(sensor_block_t)); //Set ve 0 de tranh byte rac
			block.type = SENSOR_ECG;
			block.count = ECG_DMA_BUFFER;
			block.sample_id = global_sample_id; //Danh dau thoi diem -> dong bo hoa
			block.timestamp = xTaskGetTickCount();
//			uart_printf("[DEBUG] ECG sample_id = %lu\r\n", block.sample_id);

			for(uint8_t i = 0; i < ECG_DMA_BUFFER; i++){
				block.ecg[i] = ecg_buffer[i]; //Copy gia tri sang bien ecg trong sensor_block_t
			}

			QUEUE_SEND_FROM_TASK(&block); //Gui 32 sample vao queue

		}else{
			uart_printf("[ADC8232] Timeout waiting for DMA done !\r\n");
		}
	}
}
/*-----------------------------------------------------------*/

//void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc){
//	if(hadc->Instance == ADC1){
//		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
//		vTaskNotifyGiveFromISR(ad8232_task, &xHigherPriorityTaskWoken);
//		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
//	}
//}
/*-----------------------------------------------------------*/

// ===== VERSION 1 =====

void __attribute__((unused))Ad8232_task_ver1(void const *pvParameter){
	(void)(pvParameter);

	sensor_data_t sensor_data;
	uart_printf("Ad8232 task started !\r\n");
	while(1){
		if(xSemaphoreTake(sem_adc, portMAX_DELAY) == pdTRUE){ //Trigger tu ham Callback de lay semaphore
			uart_printf("[ADC8232] got Semaphore !\r\n");
			HAL_ADC_Start(&hadc1);
			if(HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK){
				sensor_data.type = SENSOR_ECG;
				sensor_data.ecg = HAL_ADC_GetValue(&hadc1);
				QUEUE_SEND_FROM_TASK(&sensor_data);
			}
			HAL_ADC_Stop(&hadc1);
		}
	}
}
/*-----------------------------------------------------------*/

#ifdef __cplusplus
}
#endif
