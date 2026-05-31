/*
 * @file uart_dma_cfg.c
 *
 * @date May 14, 2026
 * @author LuongHuuPhuc
 */

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "uart_dma_cfg.h"

/**
 * @note
 * Quy trinh DMA Transmit cua UART DMA (Truyen du lieu)
 * Chieu du lieu: RAM -> Ngoai vi
 *
 * -> Ban dau CPU co nvu ghi data vao RAM buffer (truoc khi DMA truyen)
 * sau do cau hinh DMA (bao cho DMA biet "truyen cai gi" va "truyen di dau"):
 * 		- source = dia chi RAM buffer chua data
 * 		- destination = thanh ghi ngoai vi UART->DR
 * 		- length = so byte
 *
 * Sau khi DMA duoc enable:
 * 		-> DMA tu dong doc tung byte data tu RAM buffer
 * 		-> DMA ghi data da doc vao thanh ghi UART->DR (tuy ngoai vi)
 * 		-> UART tu dong shift data ra chan TX
 *
 * Khi transmit hoan tat:
 * 		-> DMA phat sinh interupt gui den CPU (Can phai enable ca IRQ cho ca DMA va ngoai vi)
 * 		-> CPU vao callback (ISR) de thong bao DMA complete
 *
 *  @remark
 *  - IRQ (Interrupt Request): la tin hieu ngat gui den CPU (Hardware Signal) -> "Tieng chuong goi"
 *  - ISR (Interrupt Service Routine): la chuong trinh phan mem chay de xu ly tin hieu ngat -> "Nguoi giai quyet"
 */
/*-----------------------------------------------------------*/

static UART_HandleTypeDef *uart_dma_handle = NULL;

static osMailQId uart_dma_queueId = NULL;
osThreadId uart_dma_taskId = NULL;

/* Current DMA packet pointer - con tro tro den packet hien tai */
__attribute__((unused)) static uart_dma_packet_t *current_tx_packet = NULL;

/* DMA state */
__attribute__((unused)) static volatile bool uart_dma_busy = false;

/* Debug DMA state */
static volatile uint32_t uart_ok_count = 0;
static volatile uint32_t uart_busy_count = 0;
static volatile uint32_t uart_error_count = 0;

__attribute__((unused)) static void uart_dma_kick_tx(void);

/*-----------------------------------------------------------*/

HAL_StatusTypeDef uart_dma_init(UART_HandleTypeDef *huart){
	if(huart == NULL) return HAL_ERROR;
	uart_dma_handle = huart;

	/* Queue buffer de push data tu moi noi vao */
	osMailQDef(uartDMAQueueName, UART_DMA_TX_QUEUE_LENGTH, uart_dma_packet_t);
	uart_dma_queueId = osMailCreate(osMailQ(uartDMAQueueName), NULL);
	if(uart_dma_queueId == NULL) return HAL_ERROR;

	return HAL_OK;
}

/*-----------------------------------------------------------*/

/**
 * @note
 * Su dung cac ham Intrinsic (ham dac biet ho tro compiler) cho phep
 * goi truc tiep cac cau lenh Assembly dac thu cua phan cung duoi cac ham cua C
 * ma khong can viet ma hop ngu thu cong -> Dieu khien phan cung muc thap hieu qua
 *
 * @detail
 * Su dung co che bao ve de bien duoc atomic
 * __get_PRIMASK(): Doc trang thai interrupt hien tai
 * 		- primask = 0 -> interrupt dang enable
 * 		- primask = 1 -> interrupt dang disable
 *
 * __disable_irq(): Lenh nay set bit primask = 1. Khi primask = 1:
 * 		- Tat ca interrupt se bi chan
 * 		- CPU se khong nhay vao IRQ nua
 * 		- Dung de tao critical section cuc ngan
 *
 * __enable_irq(): Lenh nay se set bit primask = 0 (tro ve ban dau)
 */
__attribute__((unused)) static void uart_dma_kick_tx(void){
	uint32_t primask = __get_PRIMASK();
	__disable_irq(); // Set bit PRIMASK = 1

	/* Begin critical section */
	if(uart_dma_busy){
		if(!primask) __enable_irq();
		return;
	}

	osEvent evt = osMailGet(uart_dma_queueId, 0);
	if(evt.status != osEventMail){
		if(!primask) __enable_irq();
		return;
	}

	// Su dung ky thuat Zero-copy-ish de tranh copy nhieu lan, giam context switch
	current_tx_packet = (uart_dma_packet_t*)evt.value.p;
	if(current_tx_packet == NULL){
		if(!primask) __enable_irq();
		return;
	}

	uart_dma_busy = true;
	/* End critical section */

	if(!primask) __enable_irq();

	HAL_StatusTypeDef ret = HAL_UART_Transmit_DMA(uart_dma_handle, current_tx_packet->data, current_tx_packet->len);
	if(ret == HAL_OK) uart_ok_count++; // Truyen thanh cong data tu DMA se tang
	else if(ret == HAL_BUSY) uart_busy_count++;
	else{
		uart_dma_busy = false;
		uart_error_count++;
		osMailFree(uart_dma_queueId, current_tx_packet);
		current_tx_packet = NULL;
	}

	/* Chi free mail sau khi DMA callback complete ! */
}

/*-----------------------------------------------------------*/

HAL_StatusTypeDef uart_tx_blocking(uint8_t *data, uint16_t len){
	if(data == NULL || len == 0) return HAL_ERROR;
	return HAL_UART_Transmit(uart_dma_handle, data, len, 1000);
}

/*-----------------------------------------------------------*/

HAL_StatusTypeDef uart_tx_dma(uint8_t *data, uint16_t len){
	if(data == NULL || len == 0) return HAL_ERROR;

	if(len > UART_DMA_TX_MAX_PACKET_SIZE) len = UART_DMA_TX_MAX_PACKET_SIZE;

	uart_dma_packet_t *mail;
	mail = osMailAlloc(uart_dma_queueId, 0); // Cap phat heap

	if(mail == NULL){
		uart_busy_count++;
		return HAL_BUSY;
	}
	memcpy(mail->data, data, len); // Copy data lan 1
	mail->len = len;

	// Gui data vao queue buffer cho UART DMA task in ra
	if(osMailPut(uart_dma_queueId, mail) != osOK){
		osMailFree(uart_dma_queueId, mail);
		uart_error_count++;
		return HAL_ERROR;
	}

	// Chuan bi transmit DMA
//	uart_dma_kick_tx();

	/* Khong can kick_tx() nhu truoc vi da co task xu ly */
	return HAL_OK;
}

/*-----------------------------------------------------------*/

void uart_dma_task(void const *pvParameters){
	(void)(pvParameters);

	osEvent evt;
	uart_dma_packet_t *packet;

	while(1){
		// Khi nhan duoc Mail gui den queue buffer
		evt = osMailGet(uart_dma_queueId, osWaitForever);
		if(evt.status != osEventMail) continue;

		packet = (uart_dma_packet_t*)evt.value.p;
		if(packet == NULL) continue;

		HAL_StatusTypeDef ret;

		// Retry neu BUSY, toi da 3 lan
		for(uint8_t retry = 0; retry < 3; retry++){
			ret = HAL_UART_Transmit_DMA(uart_dma_handle, packet->data, packet->len);

			if(ret == HAL_OK) break;
			else if(ret == HAL_BUSY){
				uart_busy_count++;
				osDelay(1); // Nhuong CPU, doi UART ranh
			}
			else{
				uart_error_count++;
				break;
			}
		}

		if(ret == HAL_OK){

			/**
			 * Wait DMA complete dung TaskNotify nhe & cho toc do cao hon 45% so voi semaphore
			 * Neu de portMAX_DELAY thi khi DMA stuck se khong reset WDT (de cho no dem ve 0) -> Reset he thong
			 * -> pdMS_TO_TICK() de biet loi
			 * -> DMA stuck -> Abort de reset
			 */
			if(ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)) == 0){
				HAL_UART_Abort(uart_dma_handle);
				uart_error_count++;
			}
			else uart_ok_count++;
		}
		osMailFree(uart_dma_queueId, packet); /* Callback Data DMA transmit complete thi free luon (du OK hay ERROR) */
	}
}

/*-----------------------------------------------------------*/

/**
 * Ham chuyen trang thai flag khi DMA da duoc transmit xong
 * @note
 * Goi ham nay trong ham `HAL_UART_TxCpltCallback()`
 * IRQ context -> KHONG goi API tai day
 */
__attribute__((unused)) void uart_dma_tx_complete_callback_ver1(UART_HandleTypeDef *huart){
	if(huart != uart_dma_handle) return;

	// Free ngay cho task khac vao
	if(current_tx_packet != NULL){
		osMailFree(uart_dma_queueId, current_tx_packet);
		current_tx_packet = NULL;
	}
	uart_dma_busy = false; // Chuyen sang trang thai DMA ranh

	/**
	 * BUG NOTE: Sau khi DMA hoan tat queue van con packet nhung khong ai kick packet tiep theo
	 * cho den khi 1 task khac log tiep va `uart_tx_dma()` -> goi `uart_dma_kick_tx()
	 * -> Packet cu + Packet moi bi timing chong nhau -> Terminal in ra dong bi dinh vao nhau
	 *
	 * -> Phai goi tiep`uart_dma_kick_tx()` de kick packet tiep theo trong buffer
	 * (Luu y khong duoc goi ham non-reentrant trong ISR de tranh race-condition)
	 */
	uart_dma_kick_tx();
}

/*-----------------------------------------------------------*/

/**
 * @note
 * - Khong xu ly queue, malloc, kick tx,... nang ne nhu ver1
 * - Sieu sach & nhe
 */
void uart_dma_tx_complete_callback_ver2(UART_HandleTypeDef *huart){
	if(huart != uart_dma_handle) return;

	BaseType_t xHigherPriorityTaskWoken = pdFALSE;

	// 1 ISR -> 1 task thi dung TaskNotify ngon luon
	vTaskNotifyGiveFromISR(uart_dma_taskId, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/*-----------------------------------------------------------*/

/* Callback de thong bao neu doc data tu DMA hoan tat  */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart){
	uart_dma_tx_complete_callback_ver2(huart);
}

/*-----------------------------------------------------------*/

uint32_t uart_dma_get_ok_count(void){
	return uart_ok_count;
}

/*-----------------------------------------------------------*/

uint32_t uart_dma_get_busy_count(void){
	return uart_busy_count;
}

/*-----------------------------------------------------------*/

uint32_t uart_dma_get_error_count(void){
	return uart_error_count;
}

#ifdef __cplusplus
}
#endif // __cplusplus
