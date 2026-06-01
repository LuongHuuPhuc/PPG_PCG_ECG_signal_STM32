/*
 * @file sensor_pkt.c
 *
 * @date May 29, 2026
 * @author LuongHuuPhuc
 */

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "sensor_pkt.h"

#include "take_snapsync.h" 	// Lay struct `sensor_sync_block_t`
#include "uart_dma_cfg.h"	// Su dung UART qua DMA de log

/* Su dung Debugger khac thay cho uart_printf() vi dung chung 1 port UART */
#ifdef DEBUG_SEGGER_RTT
#include "SEGGER_RTT.h"
#elif defined(DEBUG_SWV_ITM ) || defined(DEBUG_DWT)
#include "SWV_debug.h"
#endif // DEBUG_SEGGER_RTT

#ifdef SENSOR_BINARY_PACKET
#ifdef SYNC_INTERMEDIARY_USING

/**
 * Static buffers - tranh cap phat tren stack Sync task
 * @warning
 * s_audio_pkt va s_bio_pkt la static buffer, chi an toan khi PacketBuilder_dispatch
 * duoc goi tu 1 task duy nhat (Sync Task). Neu sau nay co nhieu task goi thi can
 * them mutex hoac chuyen sang local buffer de tranh race condition
 */
static pkt_audio_t s_audio_pkt;
static pkt_bio_t s_bio_pkt;

static uint8_t s_audio_seq = 0;
static uint8_t s_bio_seq = 0;

/*-----------------------------------------------------------*/

void PacketBuilder_reset_seq(void){
	s_audio_seq = 0;
	s_bio_seq = 0;
}

/*-----------------------------------------------------------*/

void PacketBuilder_dispatch(sensor_sync_block_t *block){
	if(block == NULL) return;

	/* Build Audio packet (PCG) */
	pkt_build_audio(&s_audio_pkt, s_audio_seq++, block->pcg_sync, block->count_sync);
	HAL_StatusTypeDef ret1 = uart_tx_dma_enqueue((uint8_t*)&s_audio_pkt, PKT_SIZE_AUDIO);

	/* Build Bio packet (ECG + PPG) */
	pkt_build_bio(&s_bio_pkt, s_bio_seq++, block->ppg_ir_sync, block->ecg_sync, block->count_sync);
	HAL_StatusTypeDef ret2 = uart_tx_dma_enqueue((uint8_t*)&s_bio_pkt, PKT_SIZE_BIO);

	/**
	 * @warning
	 * Khong duoc log de kiem tra packet nhu `uart_printf() trong khi dang gui
	 * Binary Packet vi `uart_printf` va `uart_tx_dma` dung chung 1 UART queue
	 * nen text log se bi sen ke vao Binary packet tren cung 1 day TX
	 * -> ESP32 nhan duoc stream hon hop, parse se bi lech
	 * -> Tach thanh 2 queue khac nhau neu muon log: `uart_tx_dma()` va `uart_tx_dma_packet()`
	 */
#ifdef DEBUG_SEGGER_RTT
	if(ret1 != HAL_OK) SEGGER_RTT_printf(0, "[PKT] AUDIO drop seq=%u ret=%d\r\n", s_audio_seq - 1, ret1);
	if(ret2 != HAL_OK) SEGGER_RTT_printf(0, "[PKT] BIO drop seq=%u ret=%d\r\n", s_bio_seq - 1, ret2);
#elif DEBUG_SWV_ITM
	if(ret1 != HAL_OK) SWV_LOG("[PKT] AUDIO drop seq=%u ret=%d\r\n", s_audio_seq - 1, ret1);
	if(ret2 != HAL_OK) SWV_LOG("[PKT] BIO drop seq=%u ret=%d\r\n", s_bio_seq - 1, ret2);
#endif // DebugHelper
}

#endif // SYNC_INTERMEDIARY_USING
#endif // SENSOR_BINARY_PACKET

#ifdef __cplusplus
}
#endif // __cplusplus
