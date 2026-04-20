/*
 * sd_spi_port.h
 *
 *  Created on: Mar 12, 2026
 *      Author: ADMIN
 */

#ifndef TARGET_SD_SPI_PORT_H_
#define TARGET_SD_SPI_PORT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

extern SPI_HandleTypeDef hspi1;

/* SPI handle duoc dung boi sd_spi.c */
#define SD_SPI_HANDLE    hspi1

/* Moi lenh SPI voi SD card deu phai boc bang CS (Chip Select) */
#define SD_CS_LOW()		HAL_GPIO_WritePin(CS_PIN_GPIO_Port, CS_PIN_Pin, GPIO_PIN_RESET)  // SD card duoc chon (Select) - Bat dau frame data
#define SD_CS_HIGH()	HAL_GPIO_WritePin(CS_PIN_GPIO_Port, CS_PIN_Pin, GPIO_PIN_SET)    // SD card bi bo chon (Deselect) - Ket thuc frame data

/* SPI Speed Config */
#define SD_SPI_PRESCALER_SLOW 			SPI_BAUDRATEPRESCALER_256 /* Khuyen nghi, giai doan init nen de clock cham (100 - 400 kHz) */
#define SD_SPI_PRESCALER_FAST		    SPI_BAUDRATEPRESCALER_8  /* Sau khi the SD da khoi tao thanh cong */

#ifdef __cplusplus
}
#endif

#endif /* TARGET_SD_SPI_PORT_H_ */
