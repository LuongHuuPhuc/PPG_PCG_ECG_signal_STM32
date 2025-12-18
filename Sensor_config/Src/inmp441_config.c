/*
 * inmp441_config.c
 *
 *  Created on: Oct 15, 2025
 *      Author: ADMIN
 */

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include "inmp441_config.h" // Chua struct Forward Declaration
#include "stdio.h"
#include "stdlib.h"
#include "math.h"
#include "cmsis_os.h"
#include "Logger.h"
#include "Sensor_config.h"  // Dinh nghia struct cho inmp441_config.h
#include "take_snapsync.h"  // Dinh nghia struct cho inmp441_config.h

// ====== VARIABLES DEFINITION ======

FIRFilter fir = {0};

#if defined(CMSIS_API_USING)

osSemaphoreId sem_micId = NULL;
osThreadId inmp441_taskId = NULL;

#elif defined(FREERTOS_API_USING)

TaskHandle_t inmp441_task = NULL;
SemaphoreHandle_t sem_mic = NULL;

#endif // CMSIS_USING_API

// INMP441
volatile uint16_t buffer16[I2S_DMA_FRAME_COUNT] = {0};
volatile int32_t __attribute__((unused))buffer32[I2S_SAMPLE_COUNT] = {0};
volatile bool __attribute__((unused))mic_full_ready = false;
volatile bool __attribute__((unused))mic_half_ready = false;

// doubleBuffer Ping-pong
volatile int16_t __attribute__((unused))mic_ping[I2S_SAMPLE_COUNT] = {0};
volatile int16_t __attribute__((unused))mic_pong[I2S_SAMPLE_COUNT] = {0};
volatile bool __attribute__((unused))mic_ping_ready = false;
volatile bool __attribute__((unused))mic_pong_ready = false;

// ====== FUCNTION DEFINITION ======
/*-----------------------------------------------------------*/

HAL_StatusTypeDef Inmp441_init(I2S_HandleTypeDef *i2s){
	uart_printf("[INMP441] initializing....");
	HAL_StatusTypeDef ret = HAL_OK;
	ret |= ((i2s == &hi2s2) ? HAL_OK : HAL_ERROR);

	// Khoi tao bo loc FIR
	fir_init(&fir);

#if defined(CMSIS_API_USING)

	osSemaphoreDef(inmp441SemaphoreName);
	sem_micId = osSemaphoreCreate(osSemaphore(inmp441SemaphoreName), 1);
	if(sem_micId == NULL){
		uart_printf("[INMP441] Failed to create Semaphores !\r\n");
		return HAL_ERROR;
	}

#elif defined(FREERTOS_API_USING)

	sem_mic = xSemaphoreCreateBinary();
	if(sem_mic == NULL){
		uart_printf("[INMP441] Failed to create Semaphores !\r\n");
		return HAL_ERROR;
	}

#endif // CMSIS_API_USING

	return ret;
}

/*-----------------------------------------------------------*/
//==== VERSION 1: NORMAL BUFFER ====

__attribute__((unused)) void Inmp441_task_ver1(void const *pvParameter){
	UNUSED(pvParameter);

	sensor_block_t block;
	__attribute__((unused))snapshot_sync_t snap;
	int idx_out = 0;
	int32_t sum;

	uart_printf("INMP441 task started !\r\n");
	memset(&block, 0, sizeof(sensor_block_t));

	//Bat dau DMA de thu 256 sample raw (do inmp441 set 8000Hz)
	if(HAL_I2S_Receive_DMA(&hi2s2, (uint16_t*)buffer32, I2S_SAMPLE_COUNT * 2) != HAL_OK){
		uart_printf("[INMP441] HAL_I2S_Receive_DMA begin failed !\r\n");
	}

	while(1){

#if defined(CMSIS_API_USING)

		if(osSemaphoreWait(sem_micId, 100) == osOK){ //Duoc Trigger boi TIMER (1000Hz) sau 32ms

#ifdef sensor_config_SYNC_USING
			take_snapshotSYNC(&snap);
#endif // sensor_config_SYNC_USING

			if(ulTaskNotifyTake(pdTRUE, portMAX_DELAY) == pdTRUE){ //Block task cho den khi DMA hoan tat (bao ve tu RxCpltCallback)

				block.type = SENSOR_PCG;

#ifdef sensor_config_SYNC_USING

				block.timestamp = snap.timestamp;
				block.sample_id = snap.sample_id; //Danh dau thoi diem -> dong bo

#else
				block.timestamp = global_snapshot.timestamp;
				block.sample_id = global_snapshot.sample_id; //Danh dau thoi diem -> dong bo

#endif // sensor_config_SYNC_USING

				for(uint16_t i = 0; i < I2S_SAMPLE_COUNT; i += DOWNSAMPLE_FACTOR){ //Thuc hien downsample 256 samples raw
					sum = 0; //Reset sum sau moi vong
					for(uint8_t j = 0; j < DOWNSAMPLE_FACTOR; j++){ //Thuc hien xu ly voi 8 mau 1 lan
						sum += (buffer32[i + j] >> 8); //Dich ve 24-bit co nghia
					}
					block.mic[idx_out++] = (int16_t)(sum / DOWNSAMPLE_FACTOR); //Lay trung binh 8 samples roi day vao buffer
				} //Moi vong lap i lai tang them 8 (8 sample 1 lan)

				block.count = idx_out; //So luong mau sau khi da downsample (Nen la so mau thuc te)

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

#elif defined(FREERTOS_API_USING)

		if(xSemaphoreTake(sem_mic, pdMS_TO_TICKS(100)) == pdTRUE){ //Duoc Trigger boi TIMER (1000Hz) sau 32ms

#ifdef sensor_config_SYNC_USING
			take_snapshotSYNC(&snap);
#endif // sensor_config_SYNC_USING

			if(ulTaskNotifyTake(pdTRUE, portMAX_DELAY) == pdTRUE){ //Block task cho den khi DMA hoan tat (bao ve tu RxCpltCallback)

				memset(&block, 0, sizeof(sensor_block_t));
				block.type = SENSOR_PCG;

#ifdef sensor_config_SYNC_USING

				block.timestamp = snap.timestamp;
				block.sample_id = snap.sample_id; //Danh dau thoi diem -> dong bo

#else
				block.timestamp = global_snapshot.timestamp;
				block.sample_id = global_snapshot.sample_id; //Danh dau thoi diem -> dong bo

#endif // sensor_config_SYNC_USING

				for(uint16_t i = 0; i < I2S_SAMPLE_COUNT; i += DOWNSAMPLE_FACTOR){ //Thuc hien downsample 256 samples raw
					sum = 0; //Reset sum sau moi vong
					for(uint8_t j = 0; j < DOWNSAMPLE_FACTOR; j++){ //Thuc hien xu ly voi 8 mau 1 lan
						sum += (buffer32[i + j] >> 8); //Dich ve 24-bit co nghia
					}
					block.mic[idx_out++] = (int16_t)(sum / DOWNSAMPLE_FACTOR); //Lay trung binh 8 samples roi day vao buffer
				} //Moi vong lap i lai tang them 8 (8 sample 1 lan)

				block.count = idx_out; //So luong mau sau khi da downsample (Nen la so mau thuc te)

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

#endif // CMSIS_API_USING
	}
}

/*-----------------------------------------------------------*/
// ==== VERSION 2: CIRCULAR BUFFER ====

/**
 * @brief Ham xu ly va downsample du lieu khi full buffer
 *
 * @param block con tro tro den cau truc du lieu cua cam bine
 * @param snap con tro tro den cau truc du lieu dong bo
 *
 * @note Ham con cua `Inmp441_task_ver2`.
 * Sau khi DMA ghi du `I2S_SAMPLE_COUNT = 256` mau thi downsample ve 32 mau bang FIR Filter + Decimation
 *
 * INMP441: 24-bit data + 1 bit data tre ban dau do HighZ (SCK1) trong khung 32-bits [1-bit tre|24-bit data|7-bit 0]
 * 24-bits do nam o cac chu ky tu SCK2 -> SCK25 (theo datasheet) va co bit-order tu trai -> phai (MSB cao nhat)
 * Vi tri SCK26 -> SCK32 khong duoc su dung. Data 24-bit nay se duoc dong goi vao frame 32-bit roi gui di qua chan SD
 *
 * Frame nay ban dau se duoc luu vao thanh ghi SPI->DR cua Peripheral I2S
 * Khi thanh ghi day 16-bit -> Copy truc tiep 16-bit do vao Memory cua MCU thong qua DMA (luu trong bien buffer[])
 * Du 2 lan DMA -> co duoc 1 sample frame 32-bit. No duoc can chinh vao 24-bit cao (bit[31:8] cua thanh ghi Memory do (32-bits).
 * De dam bao du lieu 24-bit xuat ra la so co dau, ta can mo rong dau (sign extension) sample tu 24-bit len 32-bit
 */
__attribute__((always_inline)) static inline void Inmp441_downsample_process(sensor_block_t *block, snapshot_sync_t *snap){
	uint16_t idx_out = 0; // Duy tri gia tri cho bien
	uint16_t decimation_index = 0; // Chi so decimation de dem xem da du 8 mau de thuc hien decimate chua

	block->type = SENSOR_PCG;

#ifdef sensor_config_SYNC_USING

	block->timestamp = snap->timestamp;
	block->sample_id = snap->sample_id; //Danh dau thoi diem -> dong bo

#else
	UNUSED(snap); // Con tro snap khong can dung vi da luu truc tiep bien dong bo
	block->timestamp = global_snapshot.timestamp;
	block->sample_id = global_snapshot.sample_id; //Danh dau thoi diem -> dong bo

#endif // sensor_config_SYNC_USING

	// Lay dung 32 sample sau khi downsample
	// Nhung van can FIR chay qua tat 256 sample de state dung
	for(uint16_t i = 0; i < I2S_SAMPLE_COUNT; i++){

		// 1. Doc buffer16[] (buffer luu data DMA) tu Memory de ghep 2 frame LOW va HIGH lai thanh 1 sample 32-bit dung nghia
		// Hai gia tri lien tiep tu buffer16[] se ra duoc sample co nghia -> Lam den khi nao du I2S_SAMPLE_COUNT sample
		// DMA day HIGH half-word ra truoc, LOW half-word ra sau
		uint16_t FRAME_HIGH = buffer16[2u * i + 0u]; // MSB Truoc
		uint16_t FRAME_LOW = buffer16[2u * i + 1u];  // Sau

		// 2. Dich 16-bit frame HIGH sang trai roi them vao 16-bit frame LOW de cho duoc gia tri 32-bit (Bit order la left-justified)
		// 32-bit nay se nam o [31:0] nhung vi tri [7:0] khong co y nghia vi data chi nam o [31:8]
		uint32_t sample32bit = ((uint32_t)FRAME_HIGH << 16) | (uint32_t)FRAME_LOW;

		// 3. Xu ly dau cua sample32bit (noi dung cua thanh ghi 32-bit) ve 24-bit co y nghia va can chinh dau
		// Dich phai 8-bit de thuc hien mo rong dau neu co so am cho 24-bit
		int32_t sample24bit = (int32_t)(sample32bit) >> 8; // Gio sample24 la signed 24-bit trong int32_t

		// 4. Loc du lieu 32-bit da can chinh de chuan bi cho decimation (anti-alias)
		float filtered = FIR_process_convolution(&fir, (float)sample24bit);

		// 5. Decimation (Giam mau 8:1)
		if(decimation_index == 0){ //Cu moi 8 mau thi chi luu 1 mau
			if(idx_out < DOWNSAMPLE_SAMPLE_COUNT){
				// Toi dang de mic la int32_t
				block->mic[idx_out++] = (int32_t)lroundf(filtered); //32 samples sau khi downsample
			}
		}
		// Tang chi so Decimation va quay vong (Modulo) bat ke co luu sample hay khong
		decimation_index = (decimation_index + 1) % DOWNSAMPLE_FACTOR;
	}

	block->count = idx_out; //Ky vong 32 sample sau khi downsample tu 256

	QUEUE_SEND_FROM_TASK(block); //Gui 32 sample da downsample vao queue
}

/*-----------------------------------------------------------*/

void Inmp441_task_ver2(void const *pvParameter){
	UNUSED(pvParameter);

	sensor_block_t block;
	__attribute__((unused))snapshot_sync_t snap;

	uart_printf("INMP441 task started !\r\n");
	memset(&block, 0, sizeof(block));

	// buffer16: Buffer chua frame data HALF-WORD tu register SPI->DR truyen vao Memory (kich thuoc phai bang so lan DMA transfer)
	// I2S_DMA_FRAME_COUNT: So lan DMA transfer tu register
	// Tuy nhien trong ham HAL_I2S_Receive_DMA da xu ly luon viec nhan doi Size (hi2s->RxXferSize = Size << 1U)
	// neu frame > 16-bit (24/32-bit) roi nen chi can truyen vao Size = I2S_SAMPLE_COUNT la duoc roi (so sample count)
	if(HAL_I2S_Receive_DMA(&hi2s2, (uint16_t*)buffer16, I2S_SAMPLE_COUNT) != HAL_OK){ // 256 sample 24-bit
		uart_printf("[INMP441] HAL_I2S_Receive_DMA failed !\r\n");
	}

	while(1){

#if defined(CMSIS_API_USING)

//		if(osSemaphoreWait(sem_micId, osWaitForever)) {
		if(ulTaskNotifyTake(pdTRUE, portMAX_DELAY) == pdTRUE){

#ifdef sensor_config_SYNC_USING
			take_snapshotSYNC(&snap);
#endif // sensor_config_SYNC_USING

//			Inmp441_frame_debug(buffer16); // Debug frame order
			Inmp441_downsample_process(&block, &snap);
		}
		else{
			uart_printf("[INMP441] Can not take DMA callback !\r\n");
//			uart_printf("[INMP441] Can not take semaphore !\r\n");
		}

#elif defined(FREERTOS_API_USING)

		if(xSemaphoreTake(sem_mic, pdMS_TO_TICKS(100)) == pdTRUE){

#ifdef sensor_config_SYNC_USING
			take_snapshotSYNC(&snap);
#endif // sensor_config_SYNC_USING

			Inmp441_downsample_process(&block, &snap);
		}else{
			uart_printf("[INMP441] Can not take semaphore !\r\n");
		}

#endif // CMSIS_API_USING
	}
}

/*-----------------------------------------------------------*/

// === VERSION 3: DOUBLE BUFFER PING-PONG ===

// Callback khi DMA buffer ping full
__attribute__((unused)) static void Inmp441_DMA_M0CpltCallback(DMA_HandleTypeDef *hdma){
	if(mic_ping_ready){
		uart_printf("[INMP441] mic_ping_ready OVERWRITTEN ! \r\n");
	}
	mic_ping_ready = true;
}

/*-----------------------------------------------------------*/

// Callback khi DMA buffer pong full
__attribute__((unused)) static void Inmp441_DMA_M1CpltCallback(DMA_HandleTypeDef *hdma){
	if(mic_pong_ready){
		uart_printf("[INMP441] mic_pong_ready OVERWRITTEN ! \r\n");
	}
	mic_pong_ready = true;
}

/*-----------------------------------------------------------*/

// Callback khi co loi DMA
__attribute__((unused)) static void Inmp441_DMA_ErrorCallback(DMA_HandleTypeDef *hdma){
	uart_printf("[INMP441] DMA callback error !\r\n");
}

/*-----------------------------------------------------------*/

/**
 * @brief Ham doc giu lieu DMA su dung double buffer ping-pong
 * \brief chuc nang double buffer + DMA (giong HAL_I2S_Receive_DMA)
 *
 * @param hi2s Con tro tro den struct cau hinh protocol I2S
 * @param ping Con tro tro den mang chua data buffer ping (buffer 1)
 * @param pong Con tro tro den mang chua data buffer pong (buffer 2)
 * @param Size Kich thuoc so mau cua moi buffer
 *
 * @return HAL_status
 */
__attribute__((unused)) static HAL_StatusTypeDef Inmp441_receive_DMA_pingpong(I2S_HandleTypeDef *hi2s, uint16_t *ping, uint16_t *pong, uint16_t Size){
	// Bat double-buffer DMA cho I2S Rx
	// DR Adress co the khac nhau tuy dong MCU; voi F4: &SPi2->DR

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

__attribute__((unused)) void Inmp441_task_ver3(void const *pvParameter){
	UNUSED(pvParameter);

	sensor_block_t block;
	__attribute__((unused))snapshot_sync_t snap;
	int idx_out = 0;

	uart_printf("INMP441 task started !\r\n");
	memset(&block, 0, sizeof(block));

	if(Inmp441_receive_DMA_pingpong(&hi2s2, (uint16_t*)mic_ping, (uint16_t*)mic_pong, I2S_SAMPLE_COUNT) != HAL_OK){
		uart_printf("[INMP441] Inmp441_init_dma_doubleBuffer failed ! \r\n");
	}

	while(1){

#if defined(CMSIS_API_USING)

		// Cho semaphore tu TIM3 (moi 32ms) de dong bo voi PPG/ECG
		if(osSemaphoreWait(sem_micId, 100) == osOK){

			// Snapshot sync tu TIM ISR (tranh truong hop counter tang len ma task chua xu ly xong, gay mismatch)
			// Dam bao timestamp va sample_id duoc lay cung thoi diem
#ifdef sensor_config_SYNC_USING
			take_snapshotSYNC(&snap);
#endif // sensor_config_SYNC_USING

			// Uu tien xu ly dung 1 block/32ms
			// Truong hop ca ping va pong cung true (xu ly cham), xu ly 1 cai va clear cai con lai de khong backlog vo han
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

			// Downsample 256 -> 32
			block.type = SENSOR_PCG;

#ifdef sensor_config_SYNC_USING

			block.timestamp = snap.timestamp;
			block.sample_id = snap.sample_id; //Danh dau thoi diem -> dong bo

#else
			block.timestamp = global_snapshot.timestamp;
			block.sample_id = global_snapshot.sample_id; //Danh dau thoi diem -> dong bo

#endif // sensor_config_SYNC_USING

			for(uint16_t i = 0; i < I2S_SAMPLE_COUNT; i++){
				float filtered = FIR_process_convolution(&fir, (float)(src[i])); //Data 32-bit nen dich lay 24-bits co nghia
				if(i >= (FIR_TAP_NUM - 1)){
					if((i - FIR_TAP_NUM - 1) % DOWNSAMPLE_FACTOR == 0){
						block.mic[idx_out++] = (int16_t)filtered;
					}
				}
			}
			block.count = idx_out; // Mong doi gia tri bang I2S_SAMPLE_COUNT (32)

			QUEUE_SEND_FROM_TASK(&block);

		}else{
			uart_printf("[INMP441] Can not take semaphore !\r\n");
			continue;
		}

#elif defined(FREERTOS_API_USING)

		// Cho semaphore tu TIM3 (moi 32ms) de dong bo voi PPG/ECG
		if(xSemaphoreTake(sem_mic, portMAX_DELAY) == pdTRUE){

			// Snapshot sync tu TIM ISR (tranh truong hop counter tang len ma task chua xu ly xong, gay mismatch)
			// Dam bao timestamp va sample_id duoc lay cung thoi diem
#ifdef sensor_config_SYNC_USING
			take_snapshotSYNC(&snap);
#endif // sensor_config_SYNC_USING

			// Uu tien xu ly dung 1 block/32ms
			// Truong hop ca ping va pong cung true (xu ly cham), xu ly 1 cai va clear cai con lai de khong backlog vo han
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

			// Downsample 256 -> 32
			memset(&block, 0, sizeof(block));
			block.type = SENSOR_PCG;

#ifdef sensor_config_SYNC_USING

			block.timestamp = snap.timestamp;
			block.sample_id = snap.sample_id; //Danh dau thoi diem -> dong bo

#else
			block.timestamp = global_snapshot.timestamp;
			block.sample_id = global_snapshot.sample_id; //Danh dau thoi diem -> dong bo

#endif // sensor_config_SYNC_USING

			idx_out = 0;
			for(uint16_t i = 0; i < I2S_SAMPLE_COUNT; i++){
				float filtered = FIR_process_convolution(&fir, (float)(src[i])); //Data 32-bit nen dich lay 24-bits co nghia
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

#endif // CMSIS_API_USING
	}
}
/*-----------------------------------------------------------*/

__attribute__((unused)) void Inmp441_frame_debug(volatile uint16_t *inputBuffer){
	for(uint8_t i = 0; i < 10; i++){
		uint16_t a = inputBuffer[2u * i + 0u];
		uint16_t b = inputBuffer[2u * i + 1u];

		uint32_t s_high_first = ((uint32_t)a << 16) | b;
		uint32_t s_low_first = ((uint32_t)b << 16) | a;

		// In ra 24-bit dau va 8-bit cuoi (padding thi se la 0)
		uart_printf("i = %d: HF: 0x%08lX low8=0x%02lX | LF: 0x%08lX low8=0x%02lX\r\n", i,
				s_high_first & 0xFF,
				s_low_first & 0xFF);
	}
}
/*-----------------------------------------------------------*/

/**
 * @brief Khi ghi DMA hoan tat (256 samples) -> Ngat (ISR) se sinh ra va ham nay se duoc goi (Circular)
 * Ly do khong dung Timer trigger moi 32ms ma dung DMA callback:
 * Vi khi dung Timer, task se chay theo lich trinh co dinh, doc lap voi DMA
 * Cho du data DMA van chua xu ly nhung Timer van cu chay task thi se gap loi "Overwritten"
 * Dung DMA callback, task se duoc danh thuc ngay lap tuc khi buffer day, dam bao
 * xu ly du lieu moi nhat ma khong bao gio bi mat mau do cho Timer
 *
 * Doi voi cac cam bien co toc do lay mau co dinh va lien tuc nhu INMP441 qua I2S DMA
 * thi nen de DMA cplt lam trigger chinh de xu ly du lieu cua chinh no
 *
 * @warning Muc uu tien ngat (Interrupt Priority) cua bat ky ISR cung phai phai lon hon
 * (nghia la uu tien logic thap hon) so voi nguong cau hinh toi thieu `configMAX_SYSCALL_INTERRUPT_PRIORITY`
 */
void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s){
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	if(hi2s->Instance == SPI2){


#if defined(CMSIS_API_USING)

		// Van de xay ra neu buffer trong ham chua xu ly xong ma DMA Callback cu ban Semaphore Give lien tuc thi Semaphore Take se bi tre !
		// Khong duoc phep Give Semaphore neu trang thai Semaphore = 1 (non-available)
		// Kha nang dung Semaphore de trigger khong on lam => Su dung TaskNotify
//		osSemaphoreRelease(sem_micId);

		vTaskNotifyGiveFromISR(inmp441_taskId, &xHigherPriorityTaskWoken);

#elif defined(FREERTOS_API_USING)

		// Phat hien: Loi treo CPU khi dat ham nay o trong ISR callback cua DMA -> Cac API FreeRTOS ke tu luc ham nay
		// duoc goi khong the chay duoc nua -> Kernel dong bang
		xSemaphoreGiveFromISR(sem_mic, &xHigherPriorityTaskWoken); //Give semaphore cho INMP441 sau moi 32ms

		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

#endif // CMSIS_API_USING

//		if(mic_full_ready){ //Neu data truoc do chua xu ly xong ma DMA da hoan tat (neu task xu ly cham)
//			uart_printf_fromISR("[INMP441] mic_full_ready OVERWRITTEN ! Previous data not processed!\r\n");
//		}
//		mic_full_ready = true; //Danh dau buffer DMA da san sang -> Mo flag cho task xu ly DMA
	}
}

/*-----------------------------------------------------------*/

#ifdef __cplusplus
}
#endif // __cplusplus
