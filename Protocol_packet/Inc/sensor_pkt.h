/*
 * @file sensor_pkt.h
 *
 * @date May 29, 2026
 * @author LuongHuuPhuc
 */

#ifndef INC_SENSOR_PKT_H_
#define INC_SENSOR_PKT_H_

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#pragma once

#include "main.h"
#include "stdint.h"
#include "string.h"

/**
 * @note
 * Thay vi gui data dang ASCII di qua UART, STM32 se dong goi thanh chuoi BINARY
 * de tang toc do va giam kich thuoc du lieu -> Gui qua UART toi ESP32
 * Tai ESP32 se checksum, check CRC va gui thang BLE (Khong parse tai day).
 * Packet se gui den App va parse sau do ve waveform.
 *
 * @details
 * Tinh toan size 1 packet (1 block)
 * - INMP441 (PCG): 32 samples x 4 bytes (int32, 24-bit data) = 128 bytes
 * - MAX30102 (PPG): 32 samples x 4 bytes (uint32 IR) = 128 bytes
 * - AD8232 (ECG): 32 samples x 2 bytes (int16) = 64 bytes
 * -> Payload tong: 320 bytes
 * -----
 * - Header: 6 bytes
 * - CRC16: 2 bytes
 * - Footer: 1 byte
 * -> Tong 1 packet = 329 bytes
 * Nhung BLE co gioi han kich thuoc toi da cua 1 packet: 
 * BLE 5.0 cua ESP32S3 co MTU (Maximum Transmission Unit): 247 bytes
 * BLE 4.0 - 4.2: MTU mac dinh la 23 bytes (3 bytes header + 20 payload)
 * -> Data vuot qua gioi han cua MTU phai chia thanh nhieu goi nho, gay fragment
 * -----
 * Tinh toan ban dau: Voi Fs = 1000Hz (1ms/sample) voi 1 block = 32 samples (32ms) 
 * -> 1s = ~31.25 blocks (~1024 samples)
 * -> 32 packets/s (1 packet = 329 bytes) -> 10.5KB/s -> Baudrate 460800 OK ! (~22.7% bandwidth UART)
 * Tuy nhien bandwidth thuc te cua BLE 5.x co the len toi 1.4Mbps (~175KB/s),
 * toc do cao thi cung khong the ganh duoc suc nang cua 1 Packet vuot qua MTU !
 * -> Tach thanh 2 packet: Audio rieng, Bio rieng
 * -----
 * - Packet 1 - INMP441 (audio): 32 samples x 4 bytes = 128 + header/CRC/footer = 137 bytes
 *		+ 1 frame UART = 11bit (8-bit payload + 2 bit Start/Stop + 1 Parity) -> (137 x 11) / 460800 = 3.27ms 
 * - Packet 2 - MAX30102 + AD8232 (bio): 32 samples x (4 + 2) bytes = 192 + header/CRC/footer = 201 bytes
 * 		+ 1 frame UART = 11bit -> (192 x 11) / 460800 = 4.58ms
 * -> Tong 1 block (2 packet) = 338 bytes/32ms 
 * -> Tong transmit 2 packet = 7.85ms -> 24.15ms idle
 * -> 1s = ~32 blocks (Can gui 64 lan lien = 64 packets) -> 10.8KB/s -> Baudrate 460800 OK ! (~23.5% bandwidth UART)
 */

/* ============ Hang so & cau truc cua packet ============ */

#define PKT_HEADER			0xAA 	/* Header */
#define PKT_FOOTER			0x55 	/* Footer */
#define PKT_VERSION			0x01 	/* Version 1 */
#define PKT_SAMPLES			32	 	/* 32ms = 1 block = 32 samples */

#define PKT_TYPE_AUDIO		0x01 /* INMP441 (1)*/
#define PKT_TYPE_BIO		0x02 /* MAX30102 + AD8232 (2) */

#pragma pack(1) // Ep cau truc theo don vi 1 byte

/* --- Common header (dung chung cho ca 2 packet) --- */
typedef struct {
	uint8_t header;         /* 0xAA */
	uint8_t version;		/* PKT_VERSION */
	uint8_t type;			/* PKT_TYPE_xxx */
	uint8_t seq;			/* Sequence number 0...255 rollover (ghi de neu tran so) */
	uint16_t payload_len;   /* So bytes payload theo sau */
} ss_pkt_header_t; // 6 bytes

/* --- Common footer (bao gom ca CRC16) --- */
typedef struct {
	uint16_t crc16;  /* CRC16-CCITT kiem tra phan du theo chu ky 16-bit */
	uint8_t footer;  /* 0x55 */
} ss_pkt_footer_t; // 3 bytes

/* --- Packet 1: INMP441 Audio (tong: 137 bytes) --- */
typedef struct {
	ss_pkt_header_t hdr;		// 6 bytes
	int32_t pcg[PKT_SAMPLES]; 	// 32 x 4 = 128 bytes (24-bit data trong int32)
	ss_pkt_footer_t ftr;		// 3 bytes
} pkt_audio_t; // 137 bytes

/* ---- Packet 2: MAX30102 + AD8232 (tong: 201 bytes) ---- */
typedef struct {
	ss_pkt_header_t hdr;		// 6 bytes
	uint32_t ir[PKT_SAMPLES];	// MAX30102 IR: 32 x 4 = 128 bytes
	int16_t ecg[PKT_SAMPLES];	// AD8232 ECG: 32 x 2 = 64 bytes
	ss_pkt_footer_t ftr;		// 3 bytes
} pkt_bio_t; // 201 bytes

#pragma pack()

/* Max size de cap phat buffer chung neu can */
#define PKT_SIZE_AUDIO		sizeof(pkt_audio_t) // 137
#define PKT_SIZE_BIO		sizeof(pkt_bio_t)   // 201
#define PKT_SIZE_MAX		PKT_SIZE_BIO

/*-----------------------------------------------------------*/

/**
 * @brief Thuc hien tinh toan CRC16 cho binary packet gui di tu STM32
 *
 * @param[in] data Con tro tro den mang gia tri dau vao
 * @param[in] len Chieu dai cua mang du lieu
 *
 * @details
 * Nguyen ly:
 * - Chuoi data dau vao duoc xem nhu la 1 so nhi phan khong lo
 * - Thuat toan se lay chuoi du lieu nay chia cho 1 da thuc sinh.
 * (Da thuc nay co gia tri Hex tuong duong: 0x1021)
 * - Thuc hien chia bang cach dung lien tiep cac phep tru XOR
 * - Phan du cua phep chia chinh la ma CRC duoc sinh ra.
 *
 * Truyen nhan:
 * 	- Ben gui: Ap dung thuat toan chia da thuc len chuoi du lieu goc
 * 	Phan du (thuong la 16-bit) thu duoc se gan vao cuoi ban tin (footer)
 * 	- Ben nhan: Thuc hien lai phep tinh tuong tu tren toan bo goi tin
 * 	nhan duoc (bao gom ca chuoi CRC di kem).
 * 	Neu ket qua phep chia bang 0, du lieu duoc xem la toan ven va khong co loi
 */
__attribute__((always_inline))
static inline uint16_t pkt_crc16(const uint8_t *data, uint16_t len){
	uint16_t crc = 0xFFFF; // Gia tri khoi tao, mot so co the dung 0x0000

	for(uint16_t i = 0; i < len; i++){
		crc ^= (uint16_t)data[i] << 8; // Thuc hien XOR data va crc hien tai (ban dau la 0xFFFF)

		for(uint8_t j = 0; j < 8; j ++){ // Lap 8 lan cho 8 bit cua byte
			crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
		}
	}
	return crc;
}

/*-----------------------------------------------------------*/

/**
 * @brief Ham dong goi packet audio va tra ve size
 */
static inline uint16_t pkt_build_audio(pkt_audio_t *pkt, uint8_t seq, const int32_t *pcg_buf, uint16_t n_samples){
	pkt->hdr.header = PKT_HEADER;
	pkt->hdr.version = PKT_VERSION;
	pkt->hdr.type = PKT_TYPE_AUDIO;
	pkt->hdr.seq = seq;
	pkt->hdr.payload_len = n_samples * sizeof(int32_t);

	// Chi la memcpy nen toc do rat nhanh (STM32 toi uu mempcy tot hon for)
	memcpy(pkt->pcg, pcg_buf, n_samples * sizeof(int32_t));

	/* CRC tinh tren header + payload */
	uint16_t crc_len = sizeof(ss_pkt_header_t) + pkt->hdr.payload_len;
	pkt->ftr.crc16 = pkt_crc16((uint8_t*)pkt, crc_len);
	pkt->ftr.footer = PKT_FOOTER;

	/* Tra ve dung kich thuoc packet gui di */
	return (sizeof(ss_pkt_header_t) + pkt->hdr.payload_len + sizeof(ss_pkt_footer_t));
}

/*-----------------------------------------------------------*/

/**
 * @brief Ham dong goi packet bio va tra ve size
 */
static inline uint16_t pkt_build_bio(pkt_bio_t *pkt, uint8_t seq, const uint32_t *ir_buf, const int16_t *ecg_buf, uint16_t n_samples){
	pkt->hdr.header = PKT_HEADER;
	pkt->hdr.version = PKT_VERSION;
	pkt->hdr.type = PKT_TYPE_BIO;
	pkt->hdr.seq = seq;
	pkt->hdr.payload_len = n_samples * (sizeof(uint32_t) + sizeof(int16_t));

	// Chi la memcpy nen toc do rat nhanh
	memcpy(pkt->ir, ir_buf, n_samples * sizeof(uint32_t));
	memcpy(pkt->ecg, ecg_buf, n_samples * sizeof(int16_t));

	/* CRC tinh tren header + payload */
	uint16_t crc_len = sizeof(ss_pkt_header_t) + pkt->hdr.payload_len;
	pkt->ftr.crc16 = pkt_crc16((uint8_t*)pkt, crc_len);
	pkt->ftr.footer = PKT_FOOTER;

	/* Tra ve dung kich thuoc packet gui di */
	return (sizeof(ss_pkt_header_t) + pkt->hdr.payload_len + sizeof(ss_pkt_footer_t));
}

#ifdef SYNC_INTERMEDIARY_USING

void PacketBuilder_reset_seq(void);

typedef struct __sensor_sync_block sensor_sync_block_t;
/**
 * Callback function goi boi Sync Task de gui data den MicroSD Task thong qua data dispatcher
 * @note
 * Khong can dung queue giong nhu Logger vi Logger xu ly cham va nang - convert 32 samples
 * sang ASCII, gom buffer, roi moi UART DMA. Neu khong co queue thi Sync Task bi block
 * cho Logger xu ly xong -> mat dong bo. Con pkt_build chi la copy data nen rat nhanh.
 * Ban than goi `uart_tx_dma()` da co queue ben trong roi
 */
void PacketBuilder_dispatch(sensor_sync_block_t *block);

#endif // SYNC_INTERMEDIARY_USING

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* INC_SENSOR_PKT_H_ */
