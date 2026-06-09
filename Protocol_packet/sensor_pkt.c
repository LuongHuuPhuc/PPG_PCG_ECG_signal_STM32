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
#elif defined(DEBUG_SWV_ITM) || defined(DEBUG_DWT)
#include "SWV_debug.h"
#endif // DEBUG_SEGGER_RTT

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

	/* Build Audio packet (PCG) voi sync data lay tu Sync Task */
	uint16_t audio_pkt_len = pkt_build_audio(&s_audio_pkt, s_audio_seq++, block->pcg_sync, block->count_sync);

#ifdef DEBUG_SWV_ITM
	/* Doi PKT_AUDIO_SIZE -> audio_pkt_len de lay kich thuoc that vi count_sync khong phai luc nao cung 32 samples */
	HAL_StatusTypeDef ret1 = uart_tx_dma_enqueue((uint8_t*)&s_audio_pkt, audio_pkt_len);
	SWV_LOG("[PKT] Audio seq=%u count_sync= %u pkt_len=%u ret=%d\r\n",
			(uint8_t)(s_audio_seq - 1),
			block->count_sync,
			audio_pkt_len,
			ret1); // ret = 0 (khong loi) -> OK
#else
	uart_tx_dma_enqueue((uint8_t*)&s_audio_pkt, audio_pkt_len);
#endif // DEBUG_SWV_ITM

	/* Build Bio packet (ECG + PPG) voi sync data lay tu Sync Task */
	uint16_t bio_pkt_len = pkt_build_bio(&s_bio_pkt, s_bio_seq++, block->ppg_ir_sync, block->ecg_sync, block->count_sync);

#ifdef DEBUG_SWV_ITM
	/* Doi PKT_BIO_SIZE -> bio_pkt_len de lay kich thuoc that vi count_sync khong phai luc nao cung 32 samples */
	HAL_StatusTypeDef ret2 = uart_tx_dma_enqueue((uint8_t*)&s_bio_pkt, bio_pkt_len);
	SWV_LOG("[PKT] Bio seq=%u count_sync= %u pkt_len=%u ret=%d\r\n",
			(uint8_t)(s_bio_seq - 1),
			block->count_sync,
			bio_pkt_len,
			ret2); // ret = 0 (khong loi) -> OK
#else
	uart_tx_dma_enqueue((uint8_t*)&s_bio_pkt, bio_pkt_len);
#endif // DEBUG_SWV_ITM

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
#elif DEBUG_SWV_ITM /* Co the se khong di den day !*/
	if(ret1 != HAL_OK) SWV_LOG("[PKT] AUDIO drop seq=%u ret=%d\r\n", s_audio_seq - 1, ret1);
	if(ret2 != HAL_OK) SWV_LOG("[PKT] BIO drop seq=%u ret=%d\r\n", s_bio_seq - 1, ret2);
#endif // DebugHelper
}

#endif // SYNC_INTERMEDIARY_USING

#ifdef __cplusplus
}
#endif // __cplusplus
