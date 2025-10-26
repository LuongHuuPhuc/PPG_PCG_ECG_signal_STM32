/*
 * max30102_config.c
 *
 *  Created on: Oct 15, 2025
 *      Author: ADMIN
 */

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "stdio.h"
#include "cmsis_os.h"
#include "Logger.h"
#include "Sensor_config.h"
#include "take_snapsync.h"
#include "max30102_config.h"

//====== VARIABLES DEFINITION ======

TaskHandle_t max30102_task = NULL;
SemaphoreHandle_t sem_max = NULL;
osThreadId max30102_taskId = NULL;

max30102_t max30102_obj;
max30102_record record;
uint32_t ir_buffer[MAX_FIFO_SAMPLE] = {0};
uint32_t red_buffer[MAX_FIFO_SAMPLE] = {0};

//====== FUCNTION DEFINITION ======

// === VERSION 1 ===

HAL_StatusTypeDef __attribute__((unused))Max30102_init_ver1(I2C_HandleTypeDef *i2c){
	max30102_init(&max30102_obj, i2c);
	max30102_reset(&max30102_obj);
	vTaskDelay(pdMS_TO_TICKS(100)); //Delay 1 luc sau reset

	max30102_clear_fifo(&max30102_obj);

	/**
	 * @note - Sample Average = n -> Tinh trung binh n mau sau do moi day vao fifo
	 * \note - fifo_a_full = n -> So mau con lai trong FIFO truoc khi sinh ngat -> Khi du (32 - n) -> Sinh ra interrupt
	 * \note - roll_over_en = 1 -> Cho phep ghi de neu fifo day
	 */
	max30102_set_fifo_config(&max30102_obj, max30102_smp_ave_4, 1, 0); //0 mau con lai tuc la du 32 sample -> interrupt

	//Sensor settings
	max30102_set_led_pulse_width(&max30102_obj, max30102_pw_18_bit);
	max30102_set_adc_resolution(&max30102_obj, max30102_adc_8192);
	max30102_set_sampling_rate(&max30102_obj, max30102_sr_100); //10ms/sample
	max30102_set_led_current_1(&max30102_obj, 6.2);
	max30102_set_led_current_2(&max30102_obj, 6.2);
	max30102_set_led_mode(&max30102_obj, &record, max30102_spo2);

	//Interrupt
	max30102_set_a_full(&max30102_obj, 1);
	max30102_set_ppg_rdy(&max30102_obj, 1);

	//Initialize 1 temperature measurement
	max30102_set_die_temp_en(&max30102_obj, 1);
	max30102_set_die_temp_rdy(&max30102_obj, 1);

	/**
	 * Doc thanh ghi ngat (interrupt register) 0x00 & 0x01 thi interrupt flag se tu dong xoa (trong datasheet)
	 * Tat cac cac interrupt flag se duoc clear ve 0
	 * Khi nao can doc du lieu thi se reset interrupt flag
	 */
	uint8_t en_reg[2] = {0};
	if(max30102_read(&max30102_obj, 0x00, en_reg, 1) != HAL_OK){ //Clear flag
		uart_printf("[MAX30102] init Failed !\r\n");
		return HAL_ERROR;
	}
	uart_printf("[MAX30102] initializing...!");
	return HAL_OK;
}


uint8_t __attribute__((unused))Max30102_interrupt_process(max30102_t *obj){
	uint8_t reg[2] = {0x00};
	uint8_t samples = 0;

	//Doc thanh ghi 0x00 va 0x01 de clear interrupt va xu ly viet doc FIFO
	max30102_read(obj, MAX30102_INTERRUPT_STATUS_1, reg, 2);

	if ((reg[0] >> MAX30102_INTERRUPT_A_FULL) & 0x01){ //FIFO chuan bi day thi gui lenh doc
		samples = max30102_read_fifo_ver2(obj, &record, ir_buffer, red_buffer, MAX_FIFO_SAMPLE);
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


void __attribute__((unused))Max30102_task_ver1(void *pvParameter){
	sensor_data_t __attribute__((unused))sensor_data;
	sensor_block_t block;
	uint8_t num_samples = 0;

	uart_printf("MAX30102 task started !\r\n");
	vTaskDelay(pdMS_TO_TICKS(5));

	while(1){
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY); //Cho callback ngat INT thuc su tu sensor

		if(max30102_has_interrupt(&max30102_obj)){ //Neu flag interrupt dang la 1
			num_samples = Max30102_interrupt_process(&max30102_obj);

			if(num_samples > 0 && num_samples <= MAX_FIFO_SAMPLE){
				memset(&block, 0, sizeof(sensor_block_t)); //Set ve 0 de tranh byte rac
				block.type = SENSOR_PPG;
				block.count = num_samples;
				block.sample_id = global_sample_id; //Danh dau thoi diem gui -> dong bo hoa
				block.timestamp = xTaskGetTickCount();

				for(uint8_t i = 0; i < num_samples; i++){
					block.ppg.ir[i] = ir_buffer[i];
					block.ppg.red[i] = red_buffer[i];
				}
				QUEUE_SEND_FROM_TASK(&block); //Moi lan du 32 mau se gui 1 lan len queue
			}
		}
	}
}


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
	if(GPIO_Pin == INT_Pin){
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		vTaskNotifyGiveFromISR(max30102_task, &xHigherPriorityTaskWoken); //Gui thong bao cho task max30102
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}


// === VERSION 2 ===

HAL_StatusTypeDef Max30102_init_ver2(I2C_HandleTypeDef *i2c){
	max30102_init(&max30102_obj, i2c);

	for(uint8_t retry = 0; retry < 3; retry++){
		max30102_reset(&max30102_obj);
		vTaskDelay(pdMS_TO_TICKS(200));
		if(max30102_clear_fifo(&max30102_obj) == HAL_OK){
			break;
		}
		uart_printf("[MAX30102] clear_fifo Failed!\r\n");
		return HAL_ERROR;
	}

	/**
	 * @note - Sample Average = n -> Tinh trung binh n mau sau do moi sinh interrupt + push vao fifo
	 * \note - fifo_a_full = n -> So mau con lai trong FIFO truoc khi sinh ngat -> Khi du (32 - n) -> Sinh ra interrupt
	 * \note - roll_over_en = 1 -> Cho phep ghi de neu fifo day
	 */

	//Giam do tre (Chi lay 1 mau va khong lay trung binh) + push vao FIFO sau khi doc du 32 mau (32ms)
	max30102_set_fifo_config(&max30102_obj, max30102_smp_ave_1, 1, 1);

	//Sensor settings
	max30102_set_led_pulse_width(&max30102_obj, max30102_pw_18_bit);
	max30102_set_adc_resolution(&max30102_obj, max30102_adc_8192);
	max30102_set_sampling_rate(&max30102_obj, max30102_sr_1000); // 1ms/sample
	max30102_set_led_current_1(&max30102_obj, 6.2);
	max30102_set_led_current_2(&max30102_obj, 6.2);
	max30102_set_led_mode(&max30102_obj, &record, max30102_spo2);

	max30102_set_a_full(&max30102_obj, 1);
	max30102_set_ppg_rdy(&max30102_obj, 1);

	uint8_t dummy_reg ;
	max30102_read(&max30102_obj, 0x00, &dummy_reg, 1); //Khong can kiem tra loi
	uart_printf("[MAX30102] initializing...!");
	return HAL_OK;
}


void Max30102_task_ver2(void *pvParameter){
	sensor_block_t block;
	snapshot_sync_t snap;
	uart_printf("Max30102 task started !\r\n");
	vTaskDelay(pdMS_TO_TICKS(5));

	while(1){
		if(xSemaphoreTake(sem_max, portMAX_DELAY) == pdTRUE){ //Take semaphore tu TIM3 sau du 32 sample (32ms) ung voi 32 counter_max

			take_snapshotSYNC(&snap); //Chup snapshot
			uint16_t num_samples = max30102_read_fifo_ver2(&max30102_obj, &record, ir_buffer, red_buffer, MAX_FIFO_SAMPLE); //Doc data tu FIFO

			if(num_samples > 0 && num_samples <= MAX_FIFO_SAMPLE){
				memset(&block, 0, sizeof(sensor_block_t));
				block.type = SENSOR_PPG;
				block.timestamp = snap.timestamp;
				block.sample_id = snap.sample_id; //Danh dau thoi diem -> dong bo
//				block.timestamp = xTaskGetTickCount(); //Lay tick moi lan task duoc goi

//				uart_printf("[DEBUG] PPG sample_id = %lu\r\n", block.sample_id);

				for(uint8_t i = 0; i < num_samples; i++){
					block.ppg.ir[i] = ir_buffer[i];
					block.ppg.red[i] = red_buffer[i];
				}

				block.count = num_samples;
				QUEUE_SEND_FROM_TASK(&block); //Gui 32 sample vao queue

			}else {
				uart_printf("[MAX] Warining: FIFO is not enoungh 32 samples !\r\n");
			}
		}else{
			uart_printf("[MAX] Can not take semaphore !\r\n");
		}
	}
}

#ifdef __cplusplus
}
#endif // __cplusplus
