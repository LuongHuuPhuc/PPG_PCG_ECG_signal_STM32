/**
 * @file max30102_low_level.h
 *
 * @author LuongHuuPhuc
 * @date 2025/10/16
 */
#ifndef __MAX30102_LOW_LEVEL_H
#define __MAX30102_LOW_LEVEL_H

#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "stdio.h"
#include "main.h"

#define MAX30102_I2C_ADDR 					0x57 // Dia chi I2C 8-bit
#define MAX30102_I2C_TIMEOUT 				1000 // Blocking mode

#define MAX30102_BYTES_PER_LED 				3 	// Moi LED chiem 3 bytes
#define MAX30102_SAMPLE_LEN_MAX 			32 	// FIFO toi da 32 samples
#define MAX30102_STORAGE_SIZE 				4 	// Do dai moi sample lay tu Register: 32-bit (4 bytes)

/* Interrupt Status register */
#define MAX30102_INTERRUPT_STATUS_1 		0x00 // Thanh ghi chua trang thai ngat 1 ([3:1] khong dung)
#define MAX30102_INTERRUPT_STATUS_2 		0x01 // Thanh ghi chua trang thai ngat 2 (chi dung bit 1)
#define MAX30102_INTERRUPT_A_FULL 			7    // Bit thu 8 tai INTERRUPT_STATUS_1
#define MAX30102_INTERRUPT_PPG_RDY 			6	 // Bit thu 7 tai INTERRUPT_STATUS_1
#define MAX30102_INTERRUPT_ALC_OVF 			5    // Bit thu 6 tai INTERRUPT_STATUS_1
#define MAX30102_INTERRUPT_PWR_RDY			0 	 // Bit thu 1 tai INTERRUPT_STATUS_1
#define MAX30102_INTERRUPT_DIE_TEMP_RDY 	1	 // Bit thu 2 tai INTERRUPT_STATUS_2

/* Interrupt Enable register */
#define MAX30102_INTERRUPT_ENABLE_1 		0x02 // Thanh ghi bat-tat trang thai ngat 1
#define MAX30102_INTERRUPT_ENABLE_2 		0x03 // Thanh ghi bat tat trang thai ngat 2
#define MAX30102_INTERRUPT_A_FULL_EN		7	 // Bit thu 6 tai INTERRUPT_ENABLE_1
#define MAX30102_INTERRUPT_PPG_RDY_EN 		6	 // Bit thu 7 tai INTERRUPT_ENABLE_1
#define MAX30102_INTERRUPT_ALC_OVF_EN 		5    // Bit thu 6 tai INTERRUPT_ENABLE_1
#define MAX30102_INTERRUPT_DIE_TEMP_RDY_EN 	1    // Bit thu 2 tai INTERRUPT_ENABLE_2

/* FIFO register (0x04-0x07) */
#define MAX30102_FIFO_WR_PTR 				0x04 // 5-bits [4:0] trong FIFO Write Pointer
#define MAX30102_OVF_COUNTER 				0x05 // 5-bits [4:0] trong Over Flow Counter (0 -> 31)
#define MAX30102_FIFO_RD_PTR 				0x06 // 5-bits [4:0] trong FIFO Read Pointer
#define MAX30102_FIFO_DATA 					0x07 // 8-bits [7:0] trong FIFO Data Register

/* FIFO config register 0x08 */
#define MAX30102_FIFO_CONFIG 				0x08
#define MAX30102_FIFO_CONFIG_SMP_AVE		5 	// 3-bits bat dau tu bit5 [7:5]
#define MAX30102_FIFO_CONFIG_ROLL_OVER_EN 	4 	// 1-bit: bit4
#define MAX30102_FIFO_CONFIG_FIFO_A_FULL 	0 	// 4-bits bat dau tu bit0 [3:0]

/* MODE config register 0x09 */
#define MAX30102_MODE_CONFIG 				0x09 // Khong dung [5:3]
#define MAX30102_MODE_SHDN 					7	 // Bit 8 - Che do Power-save
#define MAX30102_MODE_RESET 				6	 // Bit 7 - Reset all config
#define MAX30102_MODE_MODE 					0	 // [2:0] - doc gia tri nay de xem che do doc du lieu (SpO2,HR) va active leds

/* Spo2 config register */
#define MAX30102_SPO2_CONFIG 				0x0a
#define MAX30102_SPO2_ADC_RGE 				5 // Bat dau tu bit6 -> bit7 [6:5]
#define MAX30102_SPO2_SR 					2 // Bat dau tu bit3 -> bit5 [4:2]
#define MAX30102_SPO2_LEW_PW 				0 // Bat dau tu bit1 -> bit2 [1:0]

/* Led Pulse Amplitude register */
#define MAX30102_LED_IR_PA1 				0x0c // [7:0]
#define MAX30102_LED_RED_PA2 				0x0d // [7:0]

/* Multi-LED Mode control register */
#define MAX30102_MULTI_LED_CTRL_1 			0x11
#define MAX30102_MULTI_LED_CTRL_SLOT2 		4
#define MAX30102_MULTI_LED_CTRL_SLOT1 		0

#define MAX30102_MULTI_LED_CTRL_2 			0x12
#define MAX30102_MULTI_LED_CTRL_SLOT4 		4
#define MAX30102_MULTI_LED_CTRL_SLOT3		0

/* Temperature Data register */
#define MAX30102_DIE_TINT 					0x1f	 // [7:0]
#define MAX30102_DIE_TFRAC 					0x20	 // [3:0]
#define MAX30102_DIE_TEMP_CONFIG 			0x21	 // Bit0
#define MAX30102_DIE_TFRAC_INCREMENT 		0.0625f
#define MAX30102_DIE_TEMP_EN 				1

/* Struct luu data cua MAX30102 va cac bien tuong tac voi I2C */
typedef struct max30102_t {
	/* Khong can set la volatile vi 2 bien nay khong co DMA, ISR,... can thiep */
    uint32_t _ir_samples[MAX30102_SAMPLE_LEN_MAX];  /* Mang chua data IR cua MAX30102 duoc CPU ghi den (khong load tu RAM) */
    uint32_t _red_samples[MAX30102_SAMPLE_LEN_MAX]; /* Mang chua data RED cua MAX30102 duoc CPU ghi den */
    float sensor_temp; /* Bien luu gia tri nhiet do noi bo cua chip */
    I2C_HandleTypeDef *_ui2c;
    uint8_t _interrupt_flag;
} max30102_t;

/**
 * @brief Write buffer of buflen bytes to a register of the MAX30102.
 *
 * @param obj Pointer to max30102_t object instance.
 * @param reg Register address to write to.
 * @param buf Pointer containing the bytes to write.
 * @param buflen Number of bytes to write.
 */
HAL_StatusTypeDef max30102_write(max30102_t *obj, uint8_t reg, uint8_t *buf, uint16_t buflen);

/**
 * @brief Read buflen bytes from a register of the MAX30102 and store to buffer.
 * (I2C blocking mode)
 *
 * @param obj Pointer to max30102_t object instance.
 * @param reg Register address to read from.
 * @param buf Pointer to the array to write to.
 * @param buflen Number of bytes to read.
 */
HAL_StatusTypeDef max30102_read(max30102_t *obj, uint8_t reg, uint8_t *buf, uint16_t buflen);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __MAX30102_LOW_LEVEL_H
