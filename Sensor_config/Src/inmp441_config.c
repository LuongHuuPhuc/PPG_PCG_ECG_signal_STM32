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

#include "string.h"
#include "stdlib.h"
#include "math.h"

#include "FIR_Filter.h" // Bo loc LPF cho downsample
#include "Sensor_config.h" // Su dung struct `sensor_block_t` va `sensor_type_t`
#include "take_snapsync.h" // Sensor task -> Sync task (ho tro macros & global_sync_snapshot)

#ifdef SENSOR_SEND_DIRECT_USING
#include "Logger.h" // Truong hop su dung MAIL_SEND_FROM_TASK_DIRECT_LOGGER cho Sensor Task -> Logger Task (khong dung sync_task)
#endif // SENSOR_SEND_DIRECT_USING

// ====== VARIABLES DEFINITION ======

osSemaphoreId inmp441_semId = NULL;
osThreadId inmp441_taskId = NULL;

volatile uint16_t buffer16[I2S_DMA_FRAME_COUNT] = {0};
__attribute__((unused)) volatile int32_t buffer32[I2S_SAMPLE_COUNT] = {0};

// Extern protocol variable cho function trong .c
extern I2S_HandleTypeDef hi2s2;
extern void uart_printf(const char *fmt,...); // Logger.h - muon dung ham do thi khai bao extern

// ====== FUCNTION DEFINITION ======
/*-----------------------------------------------------------*/

HAL_StatusTypeDef Inmp441_init(I2S_HandleTypeDef *i2s){
	uart_printf("[INMP441] initializing....");
	HAL_StatusTypeDef ret = HAL_OK;
	ret |= ((i2s == &hi2s2) ? HAL_OK : HAL_ERROR);

	// Khoi tao bo loc FIR
	fir_init(&fir);

	osSemaphoreDef(inmp441SemaphoreName);
	inmp441_semId = osSemaphoreCreate(osSemaphore(inmp441SemaphoreName), 1);
	if(inmp441_semId == NULL){
		uart_printf("[INMP441] Failed to create Semaphores !\r\n");
		return HAL_ERROR;
	}
	return ret;
}

/*-----------------------------------------------------------*/
//==== VERSION 1: NORMAL MODE ====

__attribute__((unused)) void Inmp441_task_ver1(void const *pvParameter){
	UNUSED(pvParameter);

	sensor_block_t block;
	int idx_out = 0;
	int32_t sum;

	uart_printf("INMP441 task started !\r\n");
	memset(&block, 0, sizeof(sensor_block_t));

	//Bat dau DMA de thu 256 sample raw (do inmp441 set 8000Hz)
	if(HAL_I2S_Receive_DMA(&hi2s2, (uint16_t*)buffer32, I2S_SAMPLE_COUNT * 2) != HAL_OK){
		uart_printf("[INMP441] HAL_I2S_Receive_DMA begin failed !\r\n");
	}

	while(1){
		if(osSemaphoreWait(inmp441_semId, 100) == osOK){ //Duoc Trigger boi TIMER (1000Hz) sau 32ms
			if(ulTaskNotifyTake(pdTRUE, portMAX_DELAY) == pdTRUE){ //Block task cho den khi DMA hoan tat (bao ve tu RxCpltCallback)

				for(uint16_t i = 0; i < I2S_SAMPLE_COUNT; i += DOWNSAMPLE_FACTOR){ //Thuc hien downsample 256 samples raw
					sum = 0; //Reset sum sau moi vong

					for(uint8_t j = 0; j < DOWNSAMPLE_FACTOR; j++){ //Thuc hien xu ly voi 8 mau 1 lan
						sum += (buffer32[i + j] >> 8); //Dich ve 24-bit co nghia
					}
					block.pcg[idx_out++] = (int16_t)(sum / DOWNSAMPLE_FACTOR); //Lay trung binh 8 samples roi day vao buffer
				} //Moi vong lap i lai tang them 8 (8 sample 1 lan)

				block.count = idx_out; //So luong mau sau khi da downsample (Nen la so mau thuc te)
				block.type = SENSOR_PCG;

#ifdef SYNC_INTERMEDIARY_USING
				block.timestamp = global_sync_snapshot.timestamp;
				block.sample_id = global_sync_snapshot.sample_id; //Danh dau thoi diem -> dong bo

				MAIL_SEND_FROM_TASK_TO_SYNC(block);
#elif defined(SENSOR_SEND_DIRECT_USING) /* Gui truc tiep */
				MAIL_SEND_FROM_TASK_DIRECT_LOGGER(block);
#endif // SYNC_INTERMEDIARY_USING

				// DMA che do normal nen can goi lai ham nay sau moi lan doc DMA
				if(HAL_I2S_Receive_DMA(&hi2s2, (void*)buffer32, I2S_SAMPLE_COUNT) != HAL_OK){
					uart_printf("[INMP441] HAL_I2S_Receive_DMA end failed !\r\n");
				}

			}
			else uart_printf("[INMP441] Timeout waiting for DMA done !\r\n");
		}
		else uart_printf("[INMP441] Can not take semaphore !\r\n");
	}
}

/*-----------------------------------------------------------*/
// ==== VERSION 2: CIRCULAR MODE ====

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
				block->pcg[idx_out++] = (int32_t)lroundf(filtered); //32 samples sau khi downsample
			}
		}
		// Tang chi so Decimation va quay vong (Modulo) bat ke co luu sample hay khong
		decimation_index = (decimation_index + 1) % DOWNSAMPLE_FACTOR;
	}

	block->count = idx_out; //Ky vong 32 sample sau khi downsample tu 256
	block->type = SENSOR_PCG;

#ifdef SYNC_INTERMEDIARY_USING
	UNUSED(snap); // Con tro snap khong can dung vi da luu truc tiep bien dong bo
	block->timestamp = global_sync_snapshot.timestamp;
	block->sample_id = global_sync_snapshot.sample_id; //Danh dau thoi diem -> dong bo

	/* Gui block vao sync task */
	MAIL_SEND_FROM_TASK_TO_SYNC(*block);
#elif defined(SENSOR_SEND_DIRECT_USING) /* Gui truc tiep */
	MAIL_SEND_FROM_TASK_DIRECT_LOGGER(block);
#endif // SYNC_INTERMEDIARY_USING

}

/*-----------------------------------------------------------*/

void Inmp441_task_ver2(void const *pvParameter){
	UNUSED(pvParameter);

	sensor_block_t block;
	__attribute__((unused)) snapshot_sync_t snap;

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
		if(ulTaskNotifyTake(pdTRUE, portMAX_DELAY) == pdTRUE){

//			Inmp441_frame_debug(buffer16); // Debug frame order
			Inmp441_downsample_process(&block, &snap);
		}
		else uart_printf("[INMP441] Can not take DMA callback !\r\n");
	}
}

/*-----------------------------------------------------------*/

/**
 * @brief Ham debug xem frame nao duoc truyen truoc den thanh ghi
 * tu cam bien INMP441 bang cach in ra 10 sample dau
 *
 * Thu tu nao dung thi se thay (s_..._first & 0xFF) thuong xuyen ~ 0x00 (hoac rat gan 0 voi nhieu)
 * Thu tu nao sai thi low byte se nhay loan len
 *
 * @note Trong datashett, INMP441 co bit order la left-justified (MSB se sinh ra truoc)
 * va truyen data theo frame 32-bit (chi co 24-bit co nghia, 8-bit cuoi la padding)
 * Frame HIGH tuong duong voi phan chua bit MSB, frame LOW tuong duong voi phan chua bit LSB
 *
 * Data duoc gui vao thanh ghi voi Frame Order la HIGH frame first (MSB truoc) roi sau do moi den LOW
 */
 /* De thanh ham staic de tranh ton bo nho RAM */
__attribute__((unused)) static void Inmp441_frame_debug(volatile uint16_t *inputBuffer){
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

		// Van de xay ra neu buffer trong ham chua xu ly xong ma DMA Callback cu ban Semaphore Give lien tuc thi Semaphore Take se bi tre !
		// Khong duoc phep Give Semaphore neu trang thai Semaphore = 1 (non-available)
		// Kha nang dung Semaphore de trigger khong on lam => Su dung TaskNotify
//		osSemaphoreRelease(inmp441_semId);
		vTaskNotifyGiveFromISR(inmp441_taskId, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

/*-----------------------------------------------------------*/

#ifdef __cplusplus
}
#endif // __cplusplus
