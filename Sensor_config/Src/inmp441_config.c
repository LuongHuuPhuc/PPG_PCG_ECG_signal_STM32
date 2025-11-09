/*
 * inmp441_config.c
 *
 *  Created on: Oct 15, 2025
 *      Author: ADMIN
 */

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include "stdio.h"
#include "stdlib.h"
#include "cmsis_os.h"
#include "Logger.h"
#include "inmp441_config.h" // Chua struct Forward Declaration
#include "Sensor_config.h"  // Dinh nghia struct cho inmp441_config.h
#include "take_snapsync.h"  // Dinh nghia struct cho inmp441_config.h

// ====== VARIABLES DEFINITION ======

TaskHandle_t inmp441_task = NULL;
SemaphoreHandle_t sem_mic = NULL;
osThreadId inmp441_taskId = NULL;
FIRFilter fir = {0};

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

// ====== FUCNTION DEFINITION ======
/*-----------------------------------------------------------*/

HAL_StatusTypeDef __attribute__((unused))Inmp441_init_ver1(I2S_HandleTypeDef *i2s){
	uart_printf("[INMP441] initializing....");
	HAL_StatusTypeDef ret = HAL_OK;
	ret |= ((i2s == &hi2s2) ? HAL_OK : HAL_ERROR);

	sem_mic = xSemaphoreCreateBinary();
	if(sem_mic == NULL){
		uart_printf("[INMP441] Failed to create Semaphores !\r\n");
		ret |= HAL_ERROR;
	}
	return ret;
}
/*-----------------------------------------------------------*/

//Covert tu buffer DMA 16-bit -> PCM 24-bit (sign-extended 32-bit)
static inline int32_t __attribute__((unused))Inmp441_rebuild_sample(uint16_t low, uint16_t high){
	int32_t sample = ((int32_t)high << 16) | low; //Ghep thanh 32-bit
	sample >>= 8; //Bo 8-bit padding, con 24-bit data
	return sample;
}
/*-----------------------------------------------------------*/

//==== VERSION 1: NORMAL BUFFER ====

void __attribute__((unused))Inmp441_task_ver1(void const *pvParameter){
	UNUSED(pvParameter);

	sensor_block_t block;
	int idx_out;
	int32_t sum;
	uart_printf("Inmp441 normal task started !\r\n");
	vTaskDelay(pdMS_TO_TICKS(5));

	//Bat dau DMA de thu 256 sample raw (do inmp441 set 8000Hz)
	if(HAL_I2S_Receive_DMA(&hi2s2, (uint16_t*)buffer32, I2S_SAMPLE_COUNT) != HAL_OK){
		uart_printf("[INMP441] HAL_I2S_Receive_DMA begin failed !\r\n");
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
					uart_printf("[INMP441] HAL_I2S_Receive_DMA end failed !\r\n");
				}

			}else {
				uart_printf("[INMP441] Timeout waiting for DMA done !\r\n");
			}
		}else{
			uart_printf("[INMP441] Can not take semaphore !\r\n");
		}
	}
}
/*-----------------------------------------------------------*/


//void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s){ //Khi DMA hoan tat (256 samples) -> Ham nay se duoc goi (Normal)
//	if(hi2s->Instance == SPI2){
//		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
//		vTaskNotifyGiveFromISR(inmp441_task, &xHigherPriorityTaskWoken); //Gui thong bao cho task inmp441 la DMA hoan tat
//		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
//	}
//}
/*-----------------------------------------------------------*/

// ==== VERSION 2: CIRCULAR BUFFER ====

void Inmp441_task_ver2(void const *pvParameter){
	UNUSED(pvParameter);

	sensor_block_t block;
	snapshot_sync_t snap;
	uart_printf("Inmp441 circular task started !\r\n");
	vTaskDelay(pdMS_TO_TICKS(5));
	memset(&block, 0, sizeof(block));

	//Bat dau DMA de thu 256 sample raw (do inmp441 set 8000Hz) - Do Circular nen chi can goi 1 lan
	//Data register I2S chi toi da 16-bit (gioi han phan cung), nen chi dung duoc half-word
	//Dung buffer 32-bit, cast ve 16-bit theo HAL -> Buffer 32-bit = 2 lan doc 16-bit (frame low + frame high)

	if(HAL_I2S_Receive_DMA(&hi2s2, (uint16_t*)buffer16, I2S_SAMPLE_COUNT) != HAL_OK){
		uart_printf("[INMP441] HAL_I2S_Receive_DMA failed !\r\n");
	}

	while(1){
		if(xSemaphoreTake(sem_mic, portMAX_DELAY) == pdTRUE){ //Trigger tu TIMER
			if(mic_full_ready){
				mic_full_ready = false;
				Inmp441_process_full_buffer(&block, &snap);
			}else{
				uart_printf("[INMP441] DMA is not ready !\r\n");
			}
		}else{
			uart_printf("[INMP441] Can not take semaphore !\r\n");
		}
	}
}
/*-----------------------------------------------------------*/

void Inmp441_process_full_buffer(sensor_block_t *block, snapshot_sync_t *snap){
	static int idx_out = 0; // Duy tri gia tri cho bien
	take_snapshotSYNC(snap);

	block->type = SENSOR_PCG;
	block->timestamp = snap->timestamp;
	block->sample_id = snap->sample_id; //Danh dau dong bo
//	block.timestamp = xTaskGetTickCount(); //Lay tick moi task duoc goi

//	uart_printf("[DEBUG] PCG sample_id = %lu\r\n", block.sample_id);

	//Downsample bang cach loc lowwpass FIR + decimation
	for(uint16_t i = 0; i < I2S_SAMPLE_COUNT; i++){
		float filtered = FIR_Filter(&fir, (float)buffer16[i]); //Data 32-bit nen dich lay 24-bit co nghia
		if(i >= (FIR_TAP_NUM - 1)){
			if((i - (FIR_TAP_NUM - 1)) % DOWNSAMPLE_FACTOR == 0){ //Cu moi 8 mau thi chi luu 1 mau (decimation)
				block->mic[idx_out++] = (int16_t)filtered; //32 samples sau khi downsample
			}
		}
	}

	block->count = idx_out; //Ky vong 32 sample sau khi downsample tu 256
	idx_out = 0; // Reset ve 0

	taskENTER_CRITICAL();
	QUEUE_SEND_FROM_TASK(&block); //Gui 32 sample da downsample vao queue
	taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s){ //Khi DMA hoan tat (256 samples) -> Ham nay se duoc goi (Circular)
	if(hi2s->Instance == SPI2){
		if(mic_full_ready){ //Neu data cu chua xu ly xong ma DMA da hoan tat (neu task xu ly cham)
			uart_printf("[INMP441] mic_full_ready OVERWRITTEN ! Previous data not processed!\r\n");
		}
		mic_full_ready = true; //Danh dau buffer DMA da san sang -> Mo flag cho task xu ly DMA
	}
}
/*-----------------------------------------------------------*/

// === VERSION 3: DOUBLE BUFFER PING-PONG ===

void __attribute__((unused))Inmp441_task_ver3(void const *pvParameter){
	UNUSED(pvParameter);

	sensor_block_t block;
	snapshot_sync_t snap;
	int idx_out;

	uart_printf("Inmp441 ping_pong task started !\r\n");
	vTaskDelay(pdMS_TO_TICKS(5));

	//Chuc nang double buffer + DMA (giong HAL_I2S_Receive_DMA)
	if(Inmp441_init_ver2(&hi2s2, (uint16_t*)mic_ping, (uint16_t*)mic_pong, I2S_SAMPLE_COUNT) != HAL_OK){
		uart_printf("[INMP441] Inmp441_init_dma_doubleBuffer failed ! \r\n");
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
				uart_printf("[INMP441] Ping-Pong buffer not ready ! \r\n");
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
			uart_printf("[INMP441] Can not take Semaphore !\r\n");
			continue;
		}
	}
}
/*-----------------------------------------------------------*/

//Callback khi DMA buffer ping full
static void __attribute__((unused))Inmp441_DMA_M0CpltCallback(DMA_HandleTypeDef *hdma){
	if(mic_ping_ready){
		uart_printf("[INMP441] mic_ping_ready OVERWRITTEN ! \r\n");
	}
	mic_ping_ready = true;
}
/*-----------------------------------------------------------*/

//Callback khi DMA buffer pong full
static void __attribute__((unused))Inmp441_DMA_M1CpltCallback(DMA_HandleTypeDef *hdma){
	if(mic_pong_ready){
		uart_printf("[INMP441] mic_pong_ready OVERWRITTEN ! \r\n");
	}
	mic_pong_ready = true;
}
/*-----------------------------------------------------------*/

//Callback khi co loi DMA
static void __attribute__((unused))Inmp441_DMA_ErrorCallback(DMA_HandleTypeDef *hdma){
	uart_printf("[INMP441] DMA callback error !\r\n");
}
/*-----------------------------------------------------------*/

HAL_StatusTypeDef __attribute__((unused))Inmp441_init_ver2(I2S_HandleTypeDef *hi2s, uint16_t *ping, uint16_t *pong, uint16_t Size){
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
/*-----------------------------------------------------------*/

#ifdef __cplusplus
}
#endif // __cplusplus
