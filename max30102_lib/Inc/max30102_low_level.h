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
#define MAX30102_I2C_TIMEOUT 				1000

#define MAX30102_BYTES_PER_LED 				3 //Moi LED chiem 3 bytes
#define MAX30102_SAMPLE_LEN_MAX 			32
#define MAX30102_STORAGE_SIZE 				4 //Do dai moi sample lay tu Register: 32-bit (4 bytes)

#define MAX30102_INTERRUPT_STATUS_1 		0x00
#define MAX30102_INTERRUPT_STATUS_2 		0x01
#define MAX30102_INTERRUPT_ENABLE_1 		0x02
#define MAX30102_INTERRUPT_ENABLE_2 		0x03
#define MAX30102_INTERRUPT_A_FULL 			7
#define MAX30102_INTERRUPT_A_FULL_MASK 		0x80
#define MAX30102_INTERRUPT_PPG_RDY 			6
#define MAX30102_INTERRUPT_ALC_OVF 			5
#define MAX30102_INTERRUPT_DIE_TEMP_RDY 	1

#define MAX30102_FIFO_WR_PTR 				0x04 // 5-bits [4:0] trong FIFO Write Pointer
#define MAX30102_OVF_COUNTER 				0x05
#define MAX30102_FIFO_RD_PTR 				0x06 // 5-bits [4:0] trong FIFO Read Pointer

#define MAX30102_FIFO_DATA 					0x07

#define MAX30102_FIFO_CONFIG 				0x08
#define MAX30102_FIFO_CONFIG_SMP_AVE		5 // 3-bits bat dau tu bit5 [7:5]
#define MAX30102_FIFO_CONFIG_ROLL_OVER_EN 	4 // 1-bit: bit4
#define MAX30102_FIFO_CONFIG_FIFO_A_FULL 	0 // 4-bits bat dau tu bit0 [3:0]

#define MAX30102_MODE_CONFIG 				0x09
#define MAX30102_MODE_SHDN 					7
#define MAX30102_MODE_RESET 				6
#define MAX30102_MODE_MODE 					0

#define MAX30102_SPO2_CONFIG 				0x0a
#define MAX30102_SPO2_ADC_RGE 				5 // Bat dau bang bit5 -> bit7 [7:5]
#define MAX30102_SPO2_SR 					2 // Bat dau bang bit2 -> bit4 [4:2]
#define MAX30102_SPO2_LEW_PW 				0 // Bat dau bang bit0 -> bit1 [1:0]

#define MAX30102_LED_IR_PA1 				0x0c // [7:0]
#define MAX30102_LED_RED_PA2 				0x0d // [7:0]

#define MAX30102_MULTI_LED_CTRL_1 			0x11
#define MAX30102_MULTI_LED_CTRL_SLOT2 		4
#define MAX30102_MULTI_LED_CTRL_SLOT1 		0
#define MAX30102_MULTI_LED_CTRL_2 			0x12
#define MAX30102_MULTI_LED_CTRL_SLOT4 		4
#define MAX30102_MULTI_LED_CTRL_SLOT3		0

#define MAX30102_DIE_TINT 					0x1f
#define MAX30102_DIE_TFRAC 					0x20
#define MAX30102_DIE_TFRAC_INCREMENT 		0.0625f
#define MAX30102_DIE_TEMP_CONFIG 			0x21
#define MAX30102_DIE_TEMP_EN 				1

typedef struct max30102_t {
    I2C_HandleTypeDef *_ui2c;
    uint32_t _ir_samples[32]; //Fifo toi da 32 samples
    uint32_t _red_samples[32];
    uint8_t _interrupt_flag;
} max30102_t;

extern void uart_printf(const char *fmt,...); // Logger.h - muon dung ham do thi khai bao extern

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
