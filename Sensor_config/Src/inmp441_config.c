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

/* Su dung thu vien CMSIS-DSP ho tro xu ly tin hieu so cho ARM-Cortex */
#include "arm_math.h"
#include "arm_const_structs.h"

#include "FIR_Filter.h" // Bo loc LPF cho downsample (tu lam)
#include "Sensor_config.h" // Su dung struct `sensor_block_t` va `sensor_type_t`
#include "take_snapsync.h" // Sensor task -> Sync task (ho tro macros & global_sync_snapshot)

#ifdef SENSOR_SEND_DIRECT_USING
#include "Logger.h" // Truong hop su dung MAIL_SEND_FROM_TASK_DIRECT_LOGGER cho Sensor Task -> Logger Task (khong dung sync_task)
#endif // SENSOR_SEND_DIRECT_USING

/**
 * @note
 * Quy trinh DMA Receive tu Peripheral (Nhan du lieu)
 * Chieu du lieu: Ngoai vi -> RAM
 *
 * Quy trinh hoat dong:
 * 		- CPU chuan bi san sang 1 vung nho trong trong RAM (Rx buffer) de chua du lieu
 * 		- CPU cau hinh DMA:
 * 			- Tro dia chi nguon (Source) den thanh ghi du lieu cua ngoai vi
 * 			- Tro dia chi dich (Destination) den vung nho trong trong DMA
 * 		- CPU bat kich hoat qua trinh nhan DMA va thiet bi ngoai vi
 * 		- Khi ngoai vi nhan duoc du lieu tu ben ngoai (vi du co byte du lieu bay den cong I2S), ngoai vi se bao cho DNA
 * 		- DMA tu dong lay du lieu tu thanh ghi ngoai vi roi de thang vao RAM ma khong can CPU can thiep
 * 		- Khi vung nho nhan day hoac dat so luong byte da cau hinh, DMA se phat ra 1 interrupt de CPU
 * 		den lay data di xu ly
 */
// ====== VARIABLES DEFINITION ======

osSemaphoreId inmp441_semId = NULL;
osThreadId inmp441_taskId = NULL;

volatile uint16_t buffer16[I2S_DMA_FRAME_COUNT] = {0};
__attribute__((unused)) volatile int32_t buffer32[I2S_SAMPLE_COUNT] = {0};

static int32_t pcg_temp[DOWNSAMPLE_SAMPLE_COUNT]; /* Khong can ghi/doc truc tiep tu RAM nen khong can volatile */
static bool pcg_block_ready = false; /* note: Khong co task access, khong ISR -> khong can volatile */

extern I2S_HandleTypeDef hi2s2;
extern void uart_printf(const char *fmt,...); // Logger.h - muon dung ham do thi khai bao extern

/* Buffer index */
typedef enum{
	PCG_BUFFER_HALF,
	PCG_BUFFER_FULL,
} pcg_dma_part_t;

typedef struct {

#ifdef DWT_DEBUG
	uint32_t cycle; /* Debug do tre ISR->Done */
#endif // DWT_DEBUG

	TickType_t timestamp;
	uint32_t sample_id;
	pcg_dma_part_t part;
} pcg_dma_event_t;
static volatile pcg_dma_event_t pcg_dma_event;

// ====== FUCNTION DEFINITION ======

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
__attribute__((always_inline))
static inline void Inmp441_downsample_process(uint16_t out_offset, pcg_dma_part_t part){
    static bool decimation_synced = false; /* Them vao de tranh loi alignment tiem an khi startup */
	static uint16_t decimation_index = 0; // Chi so decimation de dem xem da du 8 mau de thuc hien decimate chua
	uint16_t idx_out = out_offset; // Duy tri gia tri cho bien
	uint16_t start_sample, end_sample;

	/* Dong bo phase decimation dung 1 lan tai block dau tien, ep sample dau tien cua stream luon co phase = 0 */
    if(!decimation_synced && part == PCG_BUFFER_HALF){
        decimation_index  = 0;
        decimation_synced = true;
    }

	/* Dung Half-buffer thi so mau output chi con 1 nua (16 samples) nhung van can phai du 32 samples thi moi duoc */
	if(part == PCG_BUFFER_HALF){
		/* 0 -> 127 */
		start_sample = 0;
		end_sample = I2S_SAMPLE_COUNT / 2;
	}
	else{
		/* 128 -> 256 */
		start_sample = I2S_SAMPLE_COUNT / 2;
		end_sample = I2S_SAMPLE_COUNT;
	}

	// Lay dung 32 sample sau khi downsample
	// Nhung van can FIR chay qua tat 256 sample de state dung
	for(uint16_t i = start_sample; i < end_sample; i++){

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
		if(decimation_index == 0){ // Cu moi 8 mau thi chi luu 1 mau
			if(idx_out < DOWNSAMPLE_SAMPLE_COUNT){
				// Toi dang de mic la int32_t
				pcg_temp[idx_out++] = (int32_t)filtered; //32 samples sau khi downsample (khong dung lroundf())
			}
		}
		// Tang chi so Decimation va quay vong (Modulo) bat ke co luu sample hay khong
//		decimation_index = (decimation_index + 1) % DOWNSAMPLE_FACTOR; // Dung modulo cost cao nen can toi uu
		if(++decimation_index >= DOWNSAMPLE_FACTOR) decimation_index = 0;
	}

	/* Chi reset ready khi xong FULL, dam bao du 32 samples */
	if(part == PCG_BUFFER_FULL) pcg_block_ready = true;
}

/*-----------------------------------------------------------*/

void Inmp441_task(void const *pvParameter){
	UNUSED(pvParameter);
	sensor_block_t block;

	uart_printf("INMP441 task started !\r\n");
	memset(&block, 0, sizeof(block));

	/**
	 * NOTE:
	 * - buffer16[I2S_DMA_FRAME_COUNT]: Buffer chua frame data HALF-WORD tu register SPI->DR truyen vao Memory (kich thuoc phai bang so lan DMA transfer)
	 * - I2S_DMA_FRAME_COUNT: So lan DMA transfer tu register (thanh ghi SPI->DR chi dai 16-bit)
	 * - I2S_SAMPLE_COUNT: So sample that
	 * Tuy nhien trong ham HAL_I2S_Receive_DMA da xu ly luon viec nhan doi Size (hi2s->RxXferSize = Size << 1U)
	 * neu frame > 16-bit (24/32-bit) roi nen chi can truyen vao Size = I2S_SAMPLE_COUNT la duoc roi (so sample count)
	 * Day la so lan DMA halfword tranfer (transaction) thuc te de thu duoc 1 sample 32-bit hoan chinh tuc la voi sample mong muon
	 * la 256 thi thuc te no se phai transaction 256 * 2 = 512 lan
	 * Khi chay, no se tu dong ghi lien tiep 2 sample HIGH-LOW vao buffer16 co kich thuoc I2S_SAMPLE_COUNT * 2
	 * luc do can parse data de thu duoc 32-bit data hoan chinh
	 * Tuy voi 512 lan DMA halfword cho 256 samples voi Fs = 8000Hz -> Ton 32ms de buffer day (Day la timing phan cung)
	 * -> Khong ton CPU trong qua trinh nay
	 */
	if(HAL_I2S_Receive_DMA(&hi2s2, (uint16_t*)buffer16, I2S_SAMPLE_COUNT) != HAL_OK){ // 256 sample 24-bit
		uart_printf("[INMP441] HAL_I2S_Receive_DMA failed !\r\n");
	}

	while(1){
		if(ulTaskNotifyTake(pdTRUE, portMAX_DELAY) == pdTRUE){

			/* Snapshot local ngay khi task wakeup - tranh doc lai volatile trong memory nhieu lan */
			pcg_dma_event_t event = pcg_dma_event;

#ifdef DWT_DEBUG

			/**
			 * DEBUG NOTE:
			 * Thoi gian CPU downsample data: 1.3ms ~ 2.4ms (du nhanh, khong bi overload)
			 * Thoi gian tu luc ISR -> Done (co ca downsample):  1.38ms ~ 3.4ms
			 * Tuy DMA ghi vao RAM khong can CPU can thiep nhung ban chat CPU van phai doi
			 * DMA fill xong buffer thi moi xu ly duoc. Nen khi DMA complete va trigger ISR (256/8000 = 32ms) thi CPU moi
			 * bat dau den downsample_process -> mat them ~2ms -> Output ~34ms -> Lech frame
			 * Voi them ly do la DMA cua I2S khong cung goc thoi gian voi TIM3 so voi ecg/ppg
			 * -> De xuat thu chuyen sang Half-buffer (moi buffer 16ms) & de cho pcg start DMA cung phase voi TIM3
			 * ngay tu dau he thong (trong main) de PCG di truoc ECG/PPG (?)
			 */

			if(event.part == PCG_BUFFER_HALF){
				uint32_t start = DWT->CYCCNT;

				Inmp441_downsample_process(0, PCG_BUFFER_HALF);

				uint32_t end = DWT->CYCCNT;
				uart_printf("[INMP441] Half: DSP = %lu cycles | ISR->Done = %lu cycles\r\n", end - start, end - event.cycle);
				/* ISR->Done: 0.68ms ~ 0.72ms */
			}
			else{
				uint32_t start = DWT->CYCCNT;

				Inmp441_downsample_process(DOWNSAMPLE_SAMPLE_COUNT / 2, PCG_BUFFER_FULL);
				if(pcg_block_ready){
					memcpy(block.pcg, pcg_temp, sizeof(pcg_temp));
					block.type = SENSOR_PCG;
					block.count = DOWNSAMPLE_SAMPLE_COUNT;

#ifdef SYNC_INTERMEDIARY_USING
					block.timestamp = event.timestamp;
					block.sample_id = event.sample_id;

					MAIL_SEND_FROM_TASK_TO_SYNC(block); /* Gui block vao sync task */
#endif // SYNC_INTERMEDIARY_USING

					pcg_block_ready = false;
				}
				uint32_t end = DWT->CYCCNT;
				uart_printf("[INMP441] Full: DSP = %lu cycles | ISR->Done = %lu cycles\r\n", end - start, end - event.cycle);
				/* ISR->Done: 0.68ms ~ 0.72ms */
			}
			/* Total: ~ 1.45ms */

#else
			if(event.part == PCG_BUFFER_HALF) Inmp441_downsample_process(0, PCG_BUFFER_HALF);
			else{
				Inmp441_downsample_process(DOWNSAMPLE_SAMPLE_COUNT / 2, PCG_BUFFER_FULL);
				if(pcg_block_ready){
					memcpy(block.pcg, pcg_temp, sizeof(pcg_temp));
					block.type = SENSOR_PCG;
					block.count = DOWNSAMPLE_SAMPLE_COUNT;

#ifdef SYNC_INTERMEDIARY_USING
					block.timestamp = event.timestamp;
					block.sample_id = event.sample_id;

					MAIL_SEND_FROM_TASK_TO_SYNC(block); /* Gui block vao sync task */
#endif // SYNC_INTERMEDIARY_USING

					pcg_block_ready = false;
				}
			}
#endif // DWT_DEBUG
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

/* Half callback chi xu ly du lieu tam thoi, chi khi RxCplt xong thi moi gui 1 block hoan chinh 32 samples */
void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s){
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;

	if(hi2s->Instance == SPI2){

#ifdef DWT_DEBUG
		pcg_dma_event.cycle = DWT->CYCCNT;
#endif // DWT_DEBUG
		pcg_dma_event.part = PCG_BUFFER_HALF;
		pcg_dma_event.timestamp = global_sync_snapshot.timestamp;
		pcg_dma_event.sample_id = global_sync_snapshot.sample_id;

		/**
		 * Moi 2 half-buffer moi cap nhat frame sync 1 lan
		 * HALF dau -> lay snapshot moi
		 * HALF sau -> giu nguyen sample_id/timestamp
		 */

		vTaskNotifyGiveFromISR(inmp441_taskId, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
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

#ifdef DWT_DEBUG
		pcg_dma_event.cycle = DWT->CYCCNT;
#endif // DWT_DEBUG
		pcg_dma_event.part = PCG_BUFFER_FULL;

		/**
		 * Van de xay ra neu buffer trong ham chua xu ly xong ma DMA Callback cu ban Semaphore Give lien tuc thi Semaphore Take se bi tre !
		 * Khong duoc phep Give Semaphore neu trang thai Semaphore = 1 (non-available)
		 * Voi ca TaskNotify co toc do nhanh hon 45% so voi Semaphore Binary vi chi can thong bao cho
		 * 1 task duy nhat de xu ly du lieu chu khong can xu ly nhieu task nhu Semaphore
		 * Kha nang dung Semaphore de trigger khong on lam => Su dung TaskNotify
		 */
//		osSemaphoreRelease(inmp441_semId);
		vTaskNotifyGiveFromISR(inmp441_taskId, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

/*-----------------------------------------------------------*/

#ifdef __cplusplus
}
#endif // __cplusplus
