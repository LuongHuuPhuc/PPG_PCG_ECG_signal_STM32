/*
 * @file uart_dma_cfg.h
 *
 * @date May 14, 2026
 * @author LuongHuuPhuc
 *
 * @note
 * Tach rieng phan xu ly UART DMA ra thanh 1 layer de tien debug
 * Y tuong:
 * 	- Chuyen UART blocking (truyen thong) -> UART su dung DMA (non-blocking)
 * 	- Log (data, debug/error,...) tu cac task se duoc push vao queue/ring buffer chung
 * 	- Ho tro ham `uart_printf()` su dung UART DMA.
 *
 * 	Khi do ham `uart_printf()` moi khi duoc goi se:
 * 		-> format chuoi bang vsnprintf()
 * 		-> ghi data vao buffer tam
 * 		-> dua data vao queue/ring buffer DMA
 * 		-> neu UART dang ranh thi kick DMA transmit ngay
 * 		-> neu UART dang busy thi data se cho trong queue/ring buffer
 */

#ifndef INC_UART_DMA_CFG_H_
#define INC_UART_DMA_CFG_H_

#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "main.h"
#include "stdint.h"
#include "stdbool.h"
#include "string.h"

#include "cmsis_os.h"

/**
 * @note
 * Tinh nhanh: 32 samples x 20 bytes/sample
 * (ECG 4 chars + PPG 6 chars + PPG max 7 chars + 3 dau phay/newline)
 *  ~ 640 bytes/block
 */

/* Log queue config */
#define UART_DMA_TX_MAX_PACKET_SIZE		640 	// 256 -> 640 (du chua 1 block hoan chinh, tranh bi truncate - cat ngan)
#define UART_DMA_TX_QUEUE_LENGTH		7		// 7 x 640 = 4480 bytes heap
#define UART_PRINTF_BUFFER_SIZE      	256  	// Dung cho log/debug binh thuong
#define UART_STREAM_BUFFER_SIZE  		640		// Dung cho stream data

typedef struct {
	uint16_t len;  								/* Chieu dai packet */
	uint8_t data[UART_DMA_TX_MAX_PACKET_SIZE]; 	/* Mang data cua packet */
} uart_dma_packet_t;

extern osThreadId uart_dma_taskId;

/*-----------------------------------------------------------*/

/**
 * @brief Khoi tao queue buffer cho DMA
 * @note Can phai khoi tao dau tien moi co the log !
 */
HAL_StatusTypeDef uart_dma_init(UART_HandleTypeDef *huart);

/**
 * @brief Ham transmit data UART truyen thong (blocking)
 */
HAL_StatusTypeDef uart_tx_blocking(uint8_t *data, uint16_t len);

/**
 * @brief Ham enqueue data de transmit log/ASCII/Binary... qua chung DMA queue (non-blocking)
 * @note Dung cho uart_printf() hoac dung truc tiep (phai format truoc) -> Logger stream
 * Chi duoc 1 noi goi Transmit_DMA() !
 */
HAL_StatusTypeDef uart_tx_dma_enqueue(uint8_t *data, uint16_t len);

/**
 * @brief Task xu ly UART DMA Transmit
 *
 * @note Chi co nhiem vu duy nhat la truyen data UART tu DMA
 * sau khi enqueue tu cac thread khac.
 * \note Task hau nhu khong tieu ton CPU, stack tieu ton khong nhieu (set thap)
 * \note Uu tien binary packet truoc, sau do moi den log queue
 */
void uart_dma_task(void const *pvParameters);

/* Debug Counter */
uint32_t uart_dma_get_ok_count(void);   	/* Tra ve so lan UART transaction thanh cong */
uint32_t uart_dma_get_busy_count(void); 	/* Tra ve so lan UART transaction bi ban */
uint32_t uart_dma_get_error_count(void);	/* Tra ve so lan UART transaction bi loi */

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* INC_UART_DMA_CFG_H_ */
