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
volatile TickType_t global_timestamp = 0;

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
volatile int16_t __attribute__((unused))buffer16[I2S_SAMPLE_COUNT] = {0};
volatile int32_t __attribute__((unused))buffer32[I2S_SAMPLE_COUNT] = {0};
volatile bool __attribute__((unused))mic_full_ready = false;
volatile bool __attribute__((unused))mic_half_ready = false;

//doubleBuffer Ping-pong
volatile bool __attribute__((unused))mic_ping_ready = false;
volatile bool __attribute__((unused))mic_pong_ready = false;
volatile int16_t __attribute__((unused))mic_ping[I2S_SAMPLE_COUNT] = {0};
volatile int16_t __attribute__((unused))mic_pong[I2S_SAMPLE_COUNT] = {0};

// AD8232
volatile int16_t ecg_buffer[ECG_DMA_BUFFER] = {0};

// MAX30102
max30102_t max30102_obj;
max30102_record record;
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


// SNAPSHOT for SYNC
static inline void take_snapshotSYNC(snapshot_sync_t *snap){
	taskENTER_CRITICAL(); //Chan ISR tam thoi de tranh bi TIM ISR chen
	snap->sample_id = global_sample_id;
	snap->timestamp = global_timestamp;
	taskEXIT_CRITICAL();
}


//====== MAX30102 =======

// === INTERRUPT VERSION ===

HAL_StatusTypeDef __attribute__((unused))Max30102_init_int(I2C_HandleTypeDef *i2c){
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


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){ //
	if(GPIO_Pin == INT_Pin){
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		vTaskNotifyGiveFromISR(max30102_task, &xHigherPriorityTaskWoken); //Gui thong bao cho task max30102
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}


// === NO-INTERRUPT VERSION ===

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
	max30102_set_led_mode(&max30102_obj, &record, max30102_spo2);

	max30102_set_a_full(&max30102_obj, 1);
	max30102_set_ppg_rdy(&max30102_obj, 1);

	uint8_t dummy_reg ;
	max30102_read(&max30102_obj, 0x00, &dummy_reg, 1); //Khong can kiem tra loi
	uart_printf("[MAX30102] initializing...!");
	return HAL_OK;
}

//VER 3
static inline __attribute__((unused)) uint32_t Max30102_getFIFORed(max30102_record *record){
	return (record->red_sample[record->tail]);
}

//VER 3
static inline __attribute__((unused)) uint32_t Max30102_getFIFOIR(max30102_record *record){
	return (record->ir_sample[record->tail]);
}


void Max30102_task_no_int(void *pvParameter){
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


//====== INMP441 ======

HAL_StatusTypeDef __attribute__((unused))Inmp441_init(I2S_HandleTypeDef *i2s){
	uart_printf("[INMP441] initializing....");
	return (i2s == &hi2s2) ? HAL_OK : HAL_ERROR;
}


//Covert tu buffer DMA 16-bit -> PCM 24-bit (sign-extended 32-bit)
static inline int32_t Inmp441_rebuild_sample(uint16_t low, uint16_t high){
	int32_t sample = ((int32_t)high << 16) | low; //Ghep thanh 32-bit
	sample >>= 8; //Bo 8-bit padding, con 24-bit data
	return sample;
}


//==== NORMAL VERSION ====

void __attribute__((unused))Inmp441_dma_normal_task(void *pvParameter){
	sensor_block_t block;
	int idx_out;
	int32_t sum;
	uart_printf("Inmp441 normal task started !\r\n");
	vTaskDelay(pdMS_TO_TICKS(5));

	//Bat dau DMA de thu 256 sample raw (do inmp441 set 8000Hz)
	if(HAL_I2S_Receive_DMA(&hi2s2, (uint16_t*)buffer32, I2S_SAMPLE_COUNT) != HAL_OK){
		uart_printf("[ERROR] HAL_I2S_Receive_DMA failed !\r\n");
	}

	while(1){
		if(xSemaphoreTake(sem_mic, portMAX_DELAY) == pdTRUE){ //Duoc Trigger boi TIMER (1000Hz) sau 32ms
			if(ulTaskNotifyTake(pdTRUE, portMAX_DELAY) == pdTRUE){ //Block task cho den khi DMA hoan tat (bao ve tu RxCpltCallback)

				memset(&block, 0, sizeof(sensor_block_t));
				block.type = SENSOR_PCG;
				block.count = DOWNSAMPLE_COUNT;//So luong mau sau khi da downsample (32 samples)
				block.sample_id = global_sample_id; //Danh dau dong bo
				block.timestamp = xTaskGetTickCount();

//				uart_printf("[DEBUG] PCG sample_id = %lu\r\n", block.sample_id);

				idx_out = 0;
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


// ==== CIRCULAR VERSION ====

void Inmp441_dma_circular_task(void *pvParameter){
	uart_printf("Inmp441 circular task started !\r\n");
	vTaskDelay(pdMS_TO_TICKS(5));

	//Bat dau DMA de thu 256 sample raw (do inmp441 set 8000Hz) - Do Circular nen chi can goi 1 lan
	//Data register I2S chi toi da 16-bit (gioi han phan cung), nen chi dung duoc half-word
	//Dung buffer 32-bit, cast ve 16-bit theo HAL -> Buffer 32-bit = 2 lan doc 16-bit (frame low + frame high)

	if(HAL_I2S_Receive_DMA(&hi2s2, (uint16_t*)buffer16, I2S_SAMPLE_COUNT) != HAL_OK){
		uart_printf("[ERROR] HAL_I2S_Receive_DMA failed !\r\n");
	}

	while(1){
		if(xSemaphoreTake(sem_mic, portMAX_DELAY) == pdTRUE){ //Trigger tu TIMER
			if(mic_full_ready){
				mic_full_ready = false;
				Inmp441_process_full_buffer();
			}else{
				uart_printf("[MIC] DMA is not ready !\r\n");
			}
		}else{
			uart_printf("[MIC] Can not take semaphore !\r\n");
		}
	}
}


void Inmp441_process_full_buffer(void){
	int idx_out;
	sensor_block_t block;
	snapshot_sync_t snap;

	take_snapshotSYNC(&snap);

	memset(&block, 0, sizeof(block));
	block.type = SENSOR_PCG;
	block.timestamp = snap.timestamp;
	block.sample_id = snap.sample_id; //Danh dau dong bo
//	block.timestamp = xTaskGetTickCount(); //Lay tick moi task duoc goi

//	uart_printf("[DEBUG] PCG sample_id = %lu\r\n", block.sample_id);

	idx_out = 0;

	//Downsample bang cach loc lowwpass FIR + decimation
	for(uint16_t i = 0; i < I2S_SAMPLE_COUNT; i++){
		float filtered = FIR_Filter(&fir, (float)buffer16[i]); //Data 32-bit nen dich lay 24-bit co nghia
		if(i >= (FIR_TAP_NUM - 1)){
			if((i - (FIR_TAP_NUM - 1)) % DOWNSAMPLE_FACTOR == 0){ //Cu moi 8 mau thi chi luu 1 mau (decimation)
				block.mic[idx_out++] = (int16_t)filtered; //32 samples sau khi downsample
			}
		}
	}

	block.count = idx_out; //Ky vong 32 sample sau khi downsample tu 256
	QUEUE_SEND_FROM_TASK(&block); //Gui 32 sample da downsample vao queue
}


void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s){ //Khi DMA hoan tat (256 samples) -> Ham nay se duoc goi (Circular)
	if(hi2s->Instance == SPI2){
		if(mic_full_ready){ //Neu data cu chua xu ly xong ma DMA da hoan tat (neu task xu ly cham)
			uart_printf("[WARN] mic_full_ready OVERWRITTEN ! Previous data not processed!\r\n");
		}
		mic_full_ready = true; //Danh dau buffer DMA da san sang -> Mo flag cho task xu ly DMA
	}
}


// === PING-PONG VERSION ===

void Inmp441_dma_pingpong_task(void *pvParameter){
	sensor_block_t block;
	snapshot_sync_t snap;
	int idx_out;

	uart_printf("Inmp441 ping_pong task started !\r\n");
	vTaskDelay(pdMS_TO_TICKS(5));

	//Chuc nang double buffer + DMA (giong HAL_I2S_Receive_DMA)
	if(Inmp441_init_dma_doubleBuffer(&hi2s2, (uint16_t*)mic_ping, (uint16_t*)mic_pong, I2S_SAMPLE_COUNT) != HAL_OK){
		uart_printf("[ERROR] Inmp441_init_dma_doubleBuffer failed ! \r\n");
	}

	while(1){
		//Cho semaphore tu TIM3 (moi 32ms) de dong bo voi PPG/ECG
		if(xSemaphoreTake(sem_mic, portMAX_DELAY) == pdTRUE){

			//Snapshot sync tu TIM ISR (tranh truong hop counter tang len ma task chua xu ly xong, gay mismatch)
			//Dam bao timestamp va sample_id duoc lay cung thoi diem
			take_snapshotSYNC(&snap);

			//Uu tien xu ly dung 1 block/32ms
			//Truong hop ca ping va pong cung true (xu ly cham), xu ly 1 cai va clear cai con lai de khong backlog vo han
			const volatile int16_t *src = NULL;

			if(mic_ping_ready){
				mic_ping_ready = false;

				//Neu ca pong cung ready, co the quyet dinh drop pong de theo kip real-time
				if(mic_pong_ready) mic_pong_ready = false; // Drop 1 nua de bat kip
				src = mic_ping;
			}else if(mic_pong_ready){
				mic_pong_ready = false;
				src = mic_pong;
			}else{
				uart_printf("[MIC] Ping-Pong buffer not ready ! \r\n");
				continue;
			}

			//Downsample 256 -> 32
			memset(&block, 0, sizeof(block));
			block.type = SENSOR_PCG;
			block.sample_id = snap.sample_id;
			block.timestamp = snap.timestamp;

			idx_out = 0;
			for(uint16_t i = 0; i < I2S_SAMPLE_COUNT; i++){
				float filtered = FIR_Filter(&fir, (float)(src[i])); //Data 32-bit nen dich lay 24-bits co nghia
				if(i >= (FIR_TAP_NUM - 1)){
					if((i - FIR_TAP_NUM - 1) % DOWNSAMPLE_FACTOR == 0){
						block.mic[idx_out++] = (int16_t)filtered;
					}
				}
			}

			block.count = idx_out; // Mong doi gia tri bang I2S_SAMPLE_COUNT (32)
			QUEUE_SEND_FROM_TASK(&block);

		}else{
			uart_printf("[MIC] Can not take Semaphore !\r\n");
			continue;
		}
	}
}


// ===== DMA Callback cho Double-buffer ping-pong

//Callback khi DMA buffer ping full
static void Inmp441_DMA_M0CpltCallback(DMA_HandleTypeDef *hdma){
	if(mic_ping_ready){
		uart_printf("[WARN] mic_ping_ready OVERWRITTEN ! \r\n");
	}
	mic_ping_ready = true;
}


//Callback khi DMA buffer pong full
static void Inmp441_DMA_M1CpltCallback(DMA_HandleTypeDef *hdma){
	if(mic_pong_ready){
		uart_printf("[WARN] mic_pong_ready OVERWRITTEN ! \r\n");
	}
	mic_pong_ready = true;
}


//Callback khi co loi DMA
static void Inmp441_DMA_ErrorCallback(DMA_HandleTypeDef *hdma){
	uart_printf("[MIC] DMA callback error !\r\n");
}


HAL_StatusTypeDef __attribute__((unused))Inmp441_init_dma_doubleBuffer(I2S_HandleTypeDef *hi2s, uint16_t *ping, uint16_t *pong, uint16_t Size){
	//Bat double-buffer DMA cho I2S Rx
	//DR Adress co the khac nhau tuy dong MCU; voi F4: &SPi2->DR

	uint32_t tmpreg_cfgr;

	if(hi2s->State != HAL_I2S_STATE_READY){
		return HAL_BUSY;
	}

	// Process locked
	__HAL_LOCK(hi2s);

	hi2s->State = HAL_I2S_STATE_BUSY_RX;
	hi2s->ErrorCode = HAL_I2S_ERROR_NONE;

	tmpreg_cfgr = hi2s->Instance->I2SCFGR & (SPI_I2SCFGR_DATLEN | SPI_I2SCFGR_CHLEN);

	if((tmpreg_cfgr == I2S_DATAFORMAT_24B) || (tmpreg_cfgr == I2S_DATAFORMAT_32B)){
		//Vi set buffer mic_ping va mic_pong 16-bit & Data width = Half-word (16-bit) nen Size << 1U
		hi2s->RxXferSize = Size << 1U;
		hi2s->RxXferCount = Size << 1U;
	}else{
		hi2s->RxXferCount = Size;
		hi2s->RxXferSize = Size;
	}

	//Gan callback cua inmp441 double buffer thay vi call back mac dinh cua HAL_I2S_Receive_DMA
	hi2s->hdmarx->XferCpltCallback = Inmp441_DMA_M0CpltCallback;
	hi2s->hdmarx->XferM1CpltCallback = Inmp441_DMA_M1CpltCallback;
	hi2s->hdmarx->XferErrorCallback = Inmp441_DMA_ErrorCallback;


	//Kiem tra neu Master Rx thi clear overrun flag
	if((hi2s->Instance->I2SCFGR & SPI_I2SCFGR_I2SCFG) == I2S_MODE_MASTER_RX){
		__HAL_I2S_CLEAR_OVRFLAG(hi2s);
	}

	//Start DMA o che do Multibuffer
	if(HAL_DMAEx_MultiBufferStart_IT(hi2s->hdmarx,
			(uint32_t)&hi2s->Instance->DR,
			(uint32_t)ping,
			(uint32_t)pong,
			hi2s->RxXferSize) != HAL_OK)
	{
		uart_printf("DMAEx not initialized ! \r\n");

		SET_BIT(hi2s->ErrorCode, HAL_I2S_ERROR_DMA);
		hi2s->State = HAL_I2S_STATE_READY;

		__HAL_UNLOCK(hi2s);
		return HAL_ERROR;
	}

	__HAL_UNLOCK(hi2s);

	//Bat DMA request cho I2S Rx
	if(HAL_IS_BIT_CLR(hi2s->Instance->CR2, SPI_CR2_RXDMAEN)){
		SET_BIT(hi2s->Instance->CR2, SPI_CR2_RXDMAEN);
	}

	//Bat I2S Peripheral neu chua bat
	if(HAL_IS_BIT_CLR(hi2s->Instance->I2SCFGR, SPI_I2SCFGR_I2SE)){
		__HAL_I2S_ENABLE(hi2s);
	}

	uart_printf("DMAEx initialized OK ! \r\n");
	return HAL_OK;
}


//====== AD8232 ======

HAL_StatusTypeDef __attribute__((unused))Ad8232_init(ADC_HandleTypeDef *adc){
	uart_printf("[AD8232] initializing...");
	return (adc == &hadc1) ? HAL_OK : HAL_ERROR;
}


// === SEMAPHORE VERSION ===

void Ad8232_dma_sema_task(void *pvParameter){
	sensor_block_t block;
	snapshot_sync_t snap;
	uart_printf("Ad8232 dma task started !\r\n");
	vTaskDelay(pdMS_TO_TICKS(5));

	//Ghi du lieu DMA vao ECG_DMA_BUFFER nay den khi du 32 mau, trigger moi mau 1ms (sample rate = 1000Hz) la 1 lan doc ADC
	if(HAL_ADC_Start_DMA(&hadc1, (uint32_t*)&ecg_buffer, ECG_DMA_BUFFER) != HAL_OK){
		uart_printf("[ERROR] HAL_ADC_Start_DMA failed!\r\n");
	}

	while(1){
		if(xSemaphoreTake(sem_adc, portMAX_DELAY) == pdTRUE){ //Sau 32ms thi TIMER nha semaphore
			take_snapshotSYNC(&snap);

			if(ulTaskNotifyTake(pdTRUE, portMAX_DELAY) == pdTRUE){ //Doi notify DMA hoan tat tu ham ConvCpltCallback

				memset(&block, 0, sizeof(sensor_block_t)); //Set ve 0 de tranh byte rac
				block.type = SENSOR_ECG;
				block.timestamp = snap.timestamp;
				block.sample_id = snap.sample_id; //Danh dau thoi diem -> dong bo hoa
//				block.timestamp = xTaskGetTickCount(); //Lay tick moi task duoc goi

//				uart_printf("[DEBUG] ECG sample_id = %lu\r\n", block.sample_id);

				for(uint8_t i = 0; i < ECG_DMA_BUFFER; i++){
					block.ecg[i] = ecg_buffer[i]; //Copy gia tri sang bien ecg trong sensor_block_t
				}

				block.count = ECG_DMA_BUFFER;
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
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		vTaskNotifyGiveFromISR(ad8232_task, &xHigherPriorityTaskWoken); //gui thong bao cho task la DMA hoan tat
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}


// === NO SEMAPHORE VERSION ===

void __attribute__((unused))Ad8232_dma_nosema_task(void *pvParameter){
	sensor_block_t block;
	uart_printf("Ad8232 dma task started !\r\n");
	vTaskDelay(pdMS_TO_TICKS(5));

	//Ghi du lieu DMA vao ECG_DMA_BUFFER nay den khi du 32 mau, trigger moi mau 1ms (sample rate = 1000Hz) la 1 lan doc ADC
	if(HAL_ADC_Start_DMA(&hadc1, (uint32_t*)&ecg_buffer, ECG_DMA_BUFFER) != HAL_OK){
		uart_printf("[ERROR] HAL_ADC_Start_DMA failed!\r\n");
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
			uart_printf("[ADC] Timeout waiting for DMA done !\r\n");
		}
	}
}


//void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc){
//	if(hadc->Instance == ADC1){
//		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
//		vTaskNotifyGiveFromISR(ad8232_task, &xHigherPriorityTaskWoken);
//		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
//	}
//}


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
