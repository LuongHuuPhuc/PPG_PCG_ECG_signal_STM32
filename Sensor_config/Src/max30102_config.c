	/*
 * max30102_config.c
 *
 *  Created on: Oct 15, 2025
 *      Author: ADMIN
 */

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "max30102_config.h"
#include "stdio.h"
#include "max30102_low_level.h" // max30102_t
#include "max30102_lib.h" // max30102_record
#include "Sensor_config.h"
#include "take_snapsync.h"

//====== VARIABLES DEFINITION ======

uint32_t ir_buffer[MAX_FIFO_SAMPLE] = {0};
uint32_t red_buffer[MAX_FIFO_SAMPLE] = {0};
max30102_t max30102_obj;
max30102_record record;

#if defined(CMSIS_API_USING)

osSemaphoreId max30102_semId = NULL;
osThreadId max30102_taskId = NULL;

#elif defined(FREERTOS_API_USING)

TaskHandle_t max30102_task = NULL;
SemaphoreHandle_t max30102_sem = NULL;

#endif // FREERTOS_API_USING

//====== FUCNTION DEFINITION ======

/*-----------------------------------------------------------*/
// === VERSION 1 ===

__attribute__((unused)) HAL_StatusTypeDef Max30102_init_ver1(I2C_HandleTypeDef *i2c){
	uart_printf("[MAX30102] initializing...!\r\n");
	HAL_StatusTypeDef ret = HAL_OK;

#if defined(CMSIS_API_USING)

	osSemaphoreDef(max30102SemaphoreName);
	max30102_semId = osSemaphoreCreate(osSemaphore(max30102SemaphoreName), 1);
	if(max30102_semId == NULL){
		uart_printf("[MAX30102] Failed to create Semaphores !\r\n");
		return HAL_ERROR;
	}

#elif defined(FREERTOS_API_USING)

	max30102_sem = xSemaphoreCreateBinary();
	if(max30102_sem == NULL){
		uart_printf("[MAX30102] Failed to create Semaphores !\r\n");
		return HAL_ERROR;
	}

#endif // CMSIS_API_USING

	// Khoi tao cam bien voi cac thong so co ban
	max30102_init(&max30102_obj, i2c);

	// Scan dia chi I2C co tren bus
	Logger_i2c_scanner(&hi2c1);

	for(uint8_t retry = 0; retry < 3; retry++){
		max30102_reset(&max30102_obj);
		if(max30102_clear_fifo(&max30102_obj) == HAL_OK){
			uart_printf("[MAX30102] clear_fifo OK !\r\n");
			break;
		}
		uart_printf("[MAX30102] clear_fifo Failed!\r\n");
		ret |= HAL_ERROR;
	}

	//Sensor settings
	max30102_set_led_pulse_width(&max30102_obj, max30102_pw_118_us);
	max30102_set_adc_resolution(&max30102_obj, max30102_adc_18_bit);
	max30102_set_sampling_rate(&max30102_obj, max30102_sr_1000); // 1ms/sample => Fixed sample rate
	max30102_set_led_current_ir(&max30102_obj, 12.6f);
	max30102_set_led_current_red(&max30102_obj, 8.0f);
	max30102_set_led_mode(&max30102_obj, &record, max30102_spo2);

	//Interrupt
	max30102_set_a_full(&max30102_obj, 1);
	max30102_set_ppg_rdy(&max30102_obj, 1);

	//Initialize 1 temperature measurement
	max30102_set_die_temp_en(&max30102_obj, 1);
	max30102_set_die_temp_rdy(&max30102_obj, 1);

	// FIFO settings (Phai de sau cung khong se bi ghi de)
	max30102_set_fifo_config(&max30102_obj, max30102_smp_ave_1, 1, 0); // Gia tri mong muon: 0x10

	/**
	 * Doc thanh ghi ngat (interrupt register) 0x00 & 0x01 thi interrupt flag se tu dong xoa (trong datasheet)
	 * Tat cac cac interrupt flag se duoc clear ve 0
	 * Khi nao can doc du lieu thi se reset interrupt flag
	 */
	uint8_t en_reg[2] = {0};
	if(max30102_read(&max30102_obj, 0x00, en_reg, 1) != HAL_OK){ //Clear flag
		uart_printf("[MAX30102] init Failed !\r\n");
		ret |= HAL_ERROR;
	}
	return ret;
}
/*-----------------------------------------------------------*/

/**
 * @brief Ham doc du lieu va xu ly ngat
 *
 * @note Ham phuc vu cho version 1 (static function)
 */
__attribute__((unused)) static inline uint8_t Max30102_interrupt_process(max30102_t *obj){
	uint8_t reg[2] = {0x00};
	uint8_t samples = 0;

	//Doc thanh ghi 0x00 va 0x01 de clear interrupt va xu ly viet doc FIFO
	max30102_read(obj, MAX30102_INTERRUPT_STATUS_1, reg, 2);

	if ((reg[0] >> MAX30102_INTERRUPT_A_FULL) & 0x01){ //FIFO chuan bi day thi gui lenh doc
		samples = max30102_read_fifo_ver2_1(obj, &record, ir_buffer, red_buffer, MAX_FIFO_SAMPLE);
	}

	if ((reg[1] >> MAX30102_INTERRUPT_DIE_TEMP_RDY) & 0x01){
		int8_t temp_int;
		uint8_t temp_frac;
		max30102_read_temp(obj, &temp_int, &temp_frac);
		// float temp = temp_int + 0.0625f * temp_frac;
	}
	obj->_interrupt_flag = 0; //Clear co ngat
	return samples;
}
/*-----------------------------------------------------------*/

__attribute__((unused)) void Max30102_task_ver1(void const *pvParameter){
	(void)(pvParameter);

	sensor_block_t block;
	uint8_t num_samples = 0;
#ifdef sensor_config_SYNC_USING
	snapshot_sync_t snap;
#endif // sensor_config_SYNC_USING

	uart_printf("MAX30102 task started !\r\n");
	memset(&block, 0, sizeof(sensor_block_t)); //Set ve 0 de tranh byte rac

	while(1){
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY); //Cho callback ngat INT thuc su tu sensor

#ifdef sensor_config_SYNC_USING
			take_snapshotSYNC(&snap);
#endif // sensor_config_SYNC_USING

		if(max30102_has_interrupt(&max30102_obj)){ //Neu flag interrupt dang la 1
			num_samples = Max30102_interrupt_process(&max30102_obj);

			if(num_samples > 0 && num_samples <= MAX_FIFO_SAMPLE){
				block.type = SENSOR_PPG;
				block.count = num_samples;

#ifdef sensor_config_SYNC_USING

				block.timestamp = snap.timestamp;
				block.sample_id = snap.sample_id; //Danh dau thoi diem -> dong bo

#else
				block.timestamp = global_snapshot.timestamp;
				block.sample_id = global_snapshot.sample_id; //Danh dau thoi diem -> dong bo

#endif // sensor_config_SYNC_USING

				for(uint8_t i = 0; i < num_samples; i++){
					block.ppg.ir[i] = ir_buffer[i];
					block.ppg.red[i] = red_buffer[i];
				}

#ifdef CMSIS_API_USING

				MAIL_SEND_FROM_TASK(block); // Gui 32 sample vao Sync Task

#elif defined(FREERTOS_API_USING)

				QUEUE_SEND_FROM_TASK(&block); //Gui 32 sample vao queue

#endif // CMSIS_API_USING
			}
		}
	}
}
/*-----------------------------------------------------------*/

// === VERSION 2 ===

HAL_StatusTypeDef Max30102_init_ver2(I2C_HandleTypeDef *i2c){
	uart_printf("[MAX30102] initializing...!\r\n");
	HAL_StatusTypeDef ret = HAL_OK;

#if defined(CMSIS_API_USING)

	osSemaphoreDef(max30102SemaphoreName);
	max30102_semId = osSemaphoreCreate(osSemaphore(max30102SemaphoreName), 1);
	if(max30102_semId == NULL){
		uart_printf("[MAX30102] Failed to create Semaphores !\r\n");
		return HAL_ERROR;
	}

#elif defined(FREERTOS_API_USING)

	max30102_sem = xSemaphoreCreateBinary();
	if(max30102_sem == NULL){
		uart_printf("[MAX30102] Failed to create Semaphores !\r\n");
		return HAL_ERROR;
	}

#endif // CMSIS_API_USING

	// Khoi tao cam bien voi cac thong so co ban
	max30102_init(&max30102_obj, i2c);

	// Scan dia chi I2C co tren bus
	Logger_i2c_scanner(&hi2c1);

	for(uint8_t retry = 0; retry < 3; retry++){
		max30102_reset(&max30102_obj);
		if(max30102_clear_fifo(&max30102_obj) == HAL_OK){
			uart_printf("[MAX30102] clear_fifo OK !\r\n");
			break;
		}
		uart_printf("[MAX30102] clear_fifo Failed!\r\n");
		ret |= HAL_ERROR;
	}

	//Sensor settings
	max30102_set_led_pulse_width(&max30102_obj, max30102_pw_118_us);
	max30102_set_adc_resolution(&max30102_obj, max30102_adc_18_bit);
	max30102_set_sampling_rate(&max30102_obj, max30102_sr_1000); // 1ms/sample => Fixed sample rate
	max30102_set_led_current_ir(&max30102_obj, 12.6f);
	max30102_set_led_current_red(&max30102_obj, 8.0f);
	max30102_set_led_mode(&max30102_obj, &record, max30102_spo2);

	max30102_set_a_full(&max30102_obj, 1);
	max30102_set_ppg_rdy(&max30102_obj, 1);

	// FIFO settings (Phai de sau cung khong se bi ghi de)
	max30102_set_fifo_config(&max30102_obj, max30102_smp_ave_1, 1, 0); // Gia tri mong muon: 0x10

	// Kiem tra trang thai thanh ghi cau hinh
	max30102_config_register_status_verbose(&max30102_obj);
	return ret;
}
/*-----------------------------------------------------------*/

void Max30102_task_ver2(void const *pvParameter){
	(void)(pvParameter);

	sensor_block_t block;
#ifdef sensor_config_SYNC_USING
	snapshot_sync_t snap;
#endif // sensor_config_SYNC_USING

	uart_printf("MAX30102 task started !\r\n");
	memset(&block, 0, sizeof(sensor_block_t));

	while(1){

#if defined(CMSIS_API_USING)

		if(osSemaphoreWait(max30102_semId, 100) == osOK){ //Take semaphore tu TIM3 sau du 32 sample (32ms) ung voi 32 counter_max

#ifdef sensor_config_SYNC_USING
			take_snapshotSYNC(&snap);
#endif // sensor_config_SYNC_USING

			uint8_t num_samples = (uint8_t)max30102_read_fifo_ver2_2(&max30102_obj, &record, ir_buffer, red_buffer, MAX_FIFO_SAMPLE);

			if(num_samples > 0){
				for(uint8_t i = 0; i < num_samples; i++){
					block.ppg.ir[i] = ir_buffer[i];
//					block.ppg.red[i] = red_buffer[i];
				}

				block.type = SENSOR_PPG;
				block.count = num_samples;

#ifdef sensor_config_SYNC_USING

				block.timestamp = snap.timestamp;
				block.sample_id = snap.sample_id; //Danh dau thoi diem -> dong bo

#else
				block.timestamp = global_snapshot.timestamp;
				block.sample_id = global_snapshot.sample_id; //Danh dau thoi diem -> dong bo

#endif // sensor_config_SYNC_USING

				MAIL_SEND_FROM_TASK(block); // Gui 32 sample vao Sync Task

			}else{ // Neu khong du 32 samples 1 lan
				uart_printf("[MAX30102] Warining: FIFO is not enoungh 32 samples !\r\n");
			}
		}else{
			uart_printf("[MAX30102] Can not take semaphore !\r\n");
		}

#elif defined(FREERTOS_API_USING)

		if(xSemaphoreTake(max30102_sem, pdMS_TO_TICKS(100)) == pdTRUE){ //Take semaphore tu TIM3 sau du 32 sample (32ms) ung voi 32 counter_max

#ifdef sensor_config_SYNC_USING
			take_snapshotSYNC(&snap);
#endif // sensor_config_SYNC_USING

			uint8_t num_samples = (uint8_t)max30102_read_fifo_ver2_2(&max30102_obj, &record, ir_buffer, red_buffer, MAX_FIFO_SAMPLE);

			if(num_samples > 0){
				for(uint8_t i = 0; i < num_samples; i++){
					block.ppg.ir[i] = ir_buffer[i];
					block.ppg.red[i] = red_buffer[i];
				}

				block.type = SENSOR_PPG;
				block.count = num_samples;

#ifdef sensor_config_SYNC_USING

				block.timestamp = snap.timestamp;
				block.sample_id = snap.sample_id; //Danh dau thoi diem -> dong bo

#else
				block.timestamp = global_snapshot.timestamp;
				block.sample_id = global_snapshot.sample_id; //Danh dau thoi diem -> dong bo

#endif // sensor_config_SYNC_USING

				QUEUE_SEND_FROM_TASK(&block); //Gui 32 sample vao queue

			}else{ // Neu khong du 32 samples 1 lan
				uart_printf("[MAX30102] Warining: FIFO is not enoungh 32 samples !\r\n");
			}
		}else{
			uart_printf("[MAX30102] Can not take semaphore !\r\n");
		}

#endif // CMSIS_API_USING
	}
}
/*-----------------------------------------------------------*/

/**
 * @note - Ham xu ly ngat (ISR) tu chan INT_Pin cua cam bien
 * \note - Ham callback duoc goi moi khi co ngat ngoài (EXTI) xay ra tren 1 chan GPIO
 * \note - Luc do chan GPIO ngat se duoc active-low de thuc thi ngat
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	if(GPIO_Pin == INT_Pin){

#if defined(FREERTOS_API_USING)

		vTaskNotifyGiveFromISR(max30102_task, &xHigherPriorityTaskWoken); //Gui thong bao cho task max30102

#elif defined(CMSIS_API_USING)

		vTaskNotifyGiveFromISR(max30102_taskId, &xHigherPriorityTaskWoken); //Gui thong bao cho task max30102

#endif
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

	}
}
/*-----------------------------------------------------------*/

#ifdef __cplusplus
}
#endif // __cplusplus
