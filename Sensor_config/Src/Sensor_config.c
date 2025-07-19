/*
 * Sensor_config.c
 *
 *  Created on: Jun 15, 2025
 *  @Author Luong Huu Phuc
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdio.h>
#include <stdarg.h>
#include "FreeRTOS.h"
#include "Task.h"
#include "semphr.h"
#include "queue.h"
#include "Sensor_config.h"
#include "max30102_for_stm32_hal.h"
#include "Logger.h"
#include <FIR_Filter.h>

volatile uint32_t global_sample_id = 0;

//Task
TaskHandle_t inmp441_task = NULL;
TaskHandle_t max30102_task = NULL;
TaskHandle_t ad8232_task = NULL;
TaskHandle_t logger_task = NULL;
TaskHandle_t test_task = NULL;

//Semaphore
SemaphoreHandle_t sem_adc, sem_mic, sem_max;

//Logger queue
QueueHandle_t logger_queue;

// INMP441
volatile int32_t buffer32[I2S_SAMPLE_COUNT] = {0};
volatile int16_t buffer16[I2S_SAMPLE_COUNT] = {0};
volatile bool __attribute__((unused))mic_full_ready = false;

// AD8232
volatile int16_t ecg_buffer[ECG_DMA_BUFFER] = {0};

// MAX30102
max30102_t max30102_obj;
uint32_t ir_buffer[MAX_FIFO_SAMPLE] = {0};
uint32_t red_buffer[MAX_FIFO_SAMPLE] = {0};

// Kiem tra so luong bo nho stack con lai
void __attribute__((unused))StackCheck(void){
	UBaseType_t stackleft = uxTaskGetStackHighWaterMark(NULL);
	uart_printf("[Stack] Stack left: %lu\r\n", (unsigned long)stackleft);
}

// Kiem tra dung luong bo nho heap con lai
void __attribute__((unused))HeapCheck(void){
	size_t heap_free = xPortGetFreeHeapSize(); //Dung luong bo nho con lai
	size_t heap_min_ever = xPortGetMinimumEverFreeHeapSize();

	uart_printf("[HEAP] Free Heap: %u bytes | Minimum Ever Free Heap: %u bytes\r\n",
				(unsigned int)heap_free,
				(unsigned int)heap_min_ever);
}


//====== MAX30102 =======

HAL_StatusTypeDef Max30102_init_int(I2C_HandleTypeDef *i2c){
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
	max30102_set_mode(&max30102_obj, max30102_spo2);

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

HAL_StatusTypeDef Max30102_init_no_int(I2C_HandleTypeDef *i2c){
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
	max30102_set_mode(&max30102_obj, max30102_spo2);

	uint8_t dummy_reg ;
	max30102_read(&max30102_obj, 0x00, &dummy_reg, 1); //Khong can kiem tra loi
	uart_printf("[MAX30102] initializing...!");
	return HAL_OK;
}

uint8_t __attribute__((unused))Max30102_interrupt_process(max30102_t *obj){
	uint8_t reg[2] = {0x00};
	uint8_t samples = 0;

	//Doc thanh ghi 0x00 va 0x01 de clear interrupt va xu ly viet doc FIFO
	max30102_read(obj, MAX30102_INTERRUPT_STATUS_1, reg, 2);

	if ((reg[0] >> MAX30102_INTERRUPT_A_FULL) & 0x01){ //FIFO chuan bi day thi gui lenh doc
		samples = max30102_read_fifo_values(obj, ir_buffer, red_buffer, MAX_FIFO_SAMPLE);
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

void __attribute__((unused))Max30102_task_int(void *pvParameter){
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

				for(uint8_t i = 0; i < num_samples; i++){
					block.ppg.ir[i] = ir_buffer[i];
					block.ppg.red[i] = red_buffer[i];
				}
				QUEUE_SEND_FROM_TASK(&block); //Moi lan du 32 mau se gui 1 lan len queue
				global_sample_id++; //Sau khi da gui cung loai id, tang id
			}
		}
	}
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){ //
	if(GPIO_Pin == INT_Pin){
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		vTaskNotifyGiveFromISR(max30102_task, &xHigherPriorityTaskWoken); //Gui thong bao cho task max30102
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

void Max30102_task_timer(void *pvParameter){
	sensor_block_t block;
	uart_printf("Max30102 task started !\r\n");
	vTaskDelay(pdMS_TO_TICKS(5));

	while(1){
		if(xSemaphoreTake(sem_max, portMAX_DELAY) == pdTRUE){ //Take semaphore tu TIM3 sau du 32 sample (32ms) ung voi 32 counter_max

			uint8_t num_samples = max30102_read_fifo_values(&max30102_obj, ir_buffer, red_buffer, MAX_FIFO_SAMPLE); //Doc data tu FIFO

			if(num_samples > 0 && num_samples <= MAX_FIFO_SAMPLE){
				memset(&block, 0, sizeof(sensor_block_t));
				block.type = SENSOR_PPG;
				block.count = num_samples;
				block.sample_id = global_sample_id; //Danh dau thoi diem -> dong bo
//				uart_printf("[DEBUG] PPG sample_id = %lu\r\n", block.sample_id);

				for(uint8_t i = 0; i < num_samples; i++){
					block.ppg.ir[i] = ir_buffer[i];
					block.ppg.red[i] = red_buffer[i];
				}
				QUEUE_SEND_FROM_TASK(&block); //Gui 32 sample vao queue
			}else {
				uart_printf("[MAX] Warining: FIFO is not enoungh 32 samples !\r\n");
			}
		}else{
			uart_printf("[MAX] Can not take semaphore !\r\n");
		}
	}
}

//====== INMP441 ======

HAL_StatusTypeDef __attribute__((unused))Inmp441_init(I2S_HandleTypeDef *i2s){
	uart_printf("[INMP441] initializing....");
	return (i2s == &hi2s2) ? HAL_OK : HAL_ERROR;
}

void Inmp441_dma_normal_task(void *pvParameter){
	sensor_block_t block;
	uart_printf("Inmp441 normal task started !\r\n");
	vTaskDelay(pdMS_TO_TICKS(5));

	//Bat dau DMA de thu 256 sample raw (do inmp441 set 8000Hz)
	if(HAL_I2S_Receive_DMA(&hi2s2, (uint16_t*)buffer32, I2S_SAMPLE_COUNT) != HAL_OK){
		uart_printf("[ERROR] HAL_I2S_Receive_DMA failed !\r\n");
	}

	while(1){
		if(xSemaphoreTake(sem_mic, portMAX_DELAY) == pdTRUE){ //Duoc Trigger boi TIMER (1000Hz) sau 32ms
			if(ulTaskNotifyTake(pdTRUE, portMAX_DELAY) == pdTRUE){ //Block task cho den khi DMA hoan tat (bao ve tu RxCpltCallback)
				int idx_out = 0;
				int32_t sum;
				memset(&block, 0, sizeof(sensor_block_t));
				block.type = SENSOR_PCG;
				block.count = DOWNSAMPLE_COUNT;//So luong mau sau khi da downsample (32 samples)
				block.sample_id = global_sample_id; //Danh dau dong bo
//				uart_printf("[DEBUG] PCG sample_id = %lu\r\n", block.sample_id);

				for(uint16_t i = 0; i < I2S_SAMPLE_COUNT; i += DOWNSAMPLE_FACTOR){ //Thuc hien downsample 256 samples raw
					sum = 0; //Reset sum sau moi vong
					for(uint8_t j = 0; j < DOWNSAMPLE_FACTOR; j++){ //Thuc hien xu ly voi 8 mau 1 lan
						sum += (buffer32[i + j] >> 8); //Dich ve 24-bit co nghia
					}
					block.mic[idx_out++] = (int16_t)(sum / DOWNSAMPLE_FACTOR); //Lay trung binh 8 samples roi day vao buffer
				} //Moi vong lap i lai tang them 8 (8 sample 1 lan)
				QUEUE_SEND_FROM_TASK(&block); //Gui 32 sample da downsample vao queue

				//DMA che do normal nen can goi lai ham nay sau moi lan doc DMA
				if(HAL_I2S_Receive_DMA(&hi2s2, (void*)buffer32, I2S_SAMPLE_COUNT) != HAL_OK){
					uart_printf("[ERROR] HAL_I2S_Receive_DMA failed !\r\n");
				}

			}else {
				uart_printf("[MIC] Timeout waiting for DMA done !\r\n");
			}
		}else{
			uart_printf("[MIC] Can not take semaphore !\r\n");
		}
	}
}

//void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s){ //Khi DMA hoan tat (256 samples) -> Ham nay se duoc goi (Normal)
//	if(hi2s->Instance == SPI2){
//		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
//		vTaskNotifyGiveFromISR(inmp441_task, &xHigherPriorityTaskWoken); //Gui thong bao cho task inmp441 la DMA hoan tat
//		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
//	}
//}

void Inmp441_dma_circular_task(void *pvParameter){
	uart_printf("Inmp441 circular task started !\r\n");
	vTaskDelay(pdMS_TO_TICKS(5));

	//Bat dau DMA de thu 256 sample raw (do inmp441 set 8000Hz) - Do Circular nen chi can goi 1 lan
	if(HAL_I2S_Receive_DMA(&hi2s2, (uint16_t*)buffer16, I2S_SAMPLE_COUNT) != HAL_OK){
		uart_printf("[ERROR] HAL_I2S_Receive_DMA failed !\r\n");
	}

	while(1){
		if(xSemaphoreTake(sem_mic, portMAX_DELAY) == pdTRUE){ //Trigger tu TIMER
			if(mic_full_ready){
				mic_full_ready = false;
				Inmp441_process_full_buffer(); //Xu ly khi ca DMA buffer da day
			}else{
				uart_printf("[MIC] DMA is not ready !\r\n");
			}
		}else{
			uart_printf("[MIC] Can not take semaphore !\r\n");
		}
	}
}

void Inmp441_process_full_buffer(void){
	int idx_out = 0;
	int32_t __attribute__((unused))sum;
	sensor_block_t block;
	memset(&block, 0, sizeof(sensor_block_t));

	block.type = SENSOR_PCG;
	block.count = DOWNSAMPLE_COUNT; //32 sample sau khi downsample tu 256
	block.sample_id = global_sample_id; //Danh dau dong bo

	//Downsample bang cach loc lowwpass FIR + decimation
	for(uint16_t i = 0; i < I2S_SAMPLE_COUNT; i++){
		float filtered = FIR_Filter((float)buffer16[i]);

		if(i % DOWNSAMPLE_FACTOR == 0){ //Cu moi 8 mau thi chi luu 1 mau
			block.mic[idx_out++] = (int16_t)filtered;
		}
	}

	//Downsample bang cach lay trung binh
//	for(uint16_t i = 0; i < I2S_SAMPLE_COUNT; i+= DOWNSAMPLE_FACTOR){
//		sum = 0;
//		for(uint8_t j = 0; j < DOWNSAMPLE_FACTOR; j++){
//			sum += (buffer16[i + j]);
//		}
//		block.mic[idx_out++] = (int16_t)(sum / DOWNSAMPLE_FACTOR);
//	}
	QUEUE_SEND_FROM_TASK(&block); //Gui 32 sample da downsample vao queue
}


void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s){ //Khi DMA hoan tat (256 samples) -> Ham nay se duoc goi (Circular)
	if(hi2s->Instance == SPI2){
		mic_full_ready = true; //Danh dau buffer DMA da san sang
	}
}


//====== AD8232 ======

HAL_StatusTypeDef __attribute__((unused))Ad8232_init(ADC_HandleTypeDef *adc){
	uart_printf("[AD8232] initializing...");
	return (adc == &hadc1) ? HAL_OK : HAL_ERROR;
}

void Ad8232_dma_task(void *pvParameter){
	sensor_block_t block;
	uart_printf("Ad8232 dma task started !\r\n");
	vTaskDelay(pdMS_TO_TICKS(5));

	//Ghi du lieu DMA vao ECG_DMA_BUFFER nay den khi du 32 mau, trigger moi mau 1ms (sample rate = 1000Hz) la 1 lan doc ADC
	if(HAL_ADC_Start_DMA(&hadc1, (uint32_t*)&ecg_buffer, ECG_DMA_BUFFER) != HAL_OK){
		uart_printf("[ERROR] HAL_ADC_Start_DMA failed!\r\n");
	}

	while(1){
		if(xSemaphoreTake(sem_adc, portMAX_DELAY) == pdTRUE){ //Sau 32ms thi TIMER nha semaphore

			if(ulTaskNotifyTake(pdTRUE, portMAX_DELAY)== pdTRUE){ //Doi notify DMA hoan tat tu ham ConvCpltCallback
				memset(&block, 0, sizeof(sensor_block_t)); //Set ve 0 de tranh byte rac
				block.type = SENSOR_ECG;
				block.count = ECG_DMA_BUFFER;
				block.sample_id = global_sample_id; //Danh dau thoi diem -> dong bo hoa
//				uart_printf("[DEBUG] ECG sample_id = %lu\r\n", block.sample_id);

				for(uint8_t i = 0; i < ECG_DMA_BUFFER; i++){
					block.ecg[i] = ecg_buffer[i]; //Copy gia tri sang bien ecg trong sensor_block_t
				}
				QUEUE_SEND_FROM_TASK(&block); //Gui 32 sample vao queue
			}else{
				uart_printf("[ADC] Timeout waiting for DMA done !\r\n");
			}
		}else{
			uart_printf("[ADC] Can not take semaphore !r\n");
		}
	}
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *adc){ //Khi DMA hoan tat (du 32 sample ~ 32ms) ham nay se duoc goi
	if(adc == &hadc1){
//		uart_printf("[ADC] DMA complete callback ! Time = %lu ms\n", HAL_GetTick()); //Debug thoi gian tung lan goi
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		vTaskNotifyGiveFromISR(ad8232_task, &xHigherPriorityTaskWoken); //gui thong bao cho task la DMA hoan tat
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

void __attribute__((unused))Ad8232_task(void *pvParameter){
	sensor_data_t sensor_data;
	uart_printf("Ad8232 task started !\r\n");
	while(1){
		if(xSemaphoreTake(sem_adc, portMAX_DELAY) == pdTRUE){ //Trigger tu ham Callback de lay semaphore
			uart_printf("[ADC] got Semaphore !\r\n");
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


#ifdef __cplusplus
}
#endif //__cplusplus
