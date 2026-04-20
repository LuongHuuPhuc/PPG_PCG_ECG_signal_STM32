/*
 * @file sd_spi.h
 *
 * @date Mar 12, 2026
 * @author LuongHuuPhuc
 *
 * @brief Low-level SD Card driver giao tiep SD card qua SPI interface
 *
 * @details
 * Day la lop driver muc thap dung de:
 * 	- Khoi tao the SD o SPI mode
 * 	- Doc block du lieu 512 byte
 * 	- Ghi block du lieu 512 byte
 * 	- Dong bo trang thai san sang cua card
 *
 * Lop nay khong phai lop API cua FATFS
 * FATFS se di qua user_diskio.c roi moi goi xuong cac ham trong file nay (!)
 */

#ifndef TARGET_SD_SPI_H_
#define TARGET_SD_SPI_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/* Gia tri trang thai tra ve cua driver SD SPI */
typedef enum {
	SD_SPI_OK = 0,
	SD_SPI_ERR,
	SD_SPI_TIMEOUT,
	SD_SPI_NOINIT,
	SD_SPI_BADRESP
} sd_spi_status_t;


/* SD card type duoc nhan dien trong SPI mode */
typedef enum {
	SD_CARD_UNKNOWN = 0,
	SD_CARD_MMC,
	SD_CARD_SDSC,
	SD_CARD_SDHC
} sd_card_type_t;

/**
 * @brief Khoi tao the SD o SPI mode
 *
 * @details
 * Ham nay thuc hien chuoi khoi tao co ban:
 * - Cap xung dummy khi CS = 1
 * - Gui CMD0
 * - Thu CMD8
 * - Lap ACMD41 / CMD1 tuy theo loai the
 * - Doc OCR / CSD
 *
 * @retval SD_SPI_OK neu thanh cong
 * @retval SD_SPI_ERR neu co loi
 * @retval SD_SPI_TIMEOUT neu qua thoi gian cho
 *
 * Reset SPI bus neu Init fail
 */
sd_spi_status_t SD_SPI_InitCard(void);

/**
 * @brief Doc mot hoac nhieu sector 512 bytes tu the SD
 *
 * @param[out] buf Bo dem dich de chua du lieu doc ve
 * @param[in] sector Dia chi sector logic (LBA)
 * @param[in] count So Sector can doc
 */
sd_spi_status_t SD_SPI_ReadSectors(uint8_t *buf, uint32_t sector, uint32_t count);

/**
 * @brief Ghi 1 hoac nhieu Sector 512 bytes vao the SD
 *
 * @param[in] buf Bo dem nguon chua du lieu can ghi
 * @param[in] sector Dia chi sector logic (LBA)
 * @param[in] count So Sector can ghi
 */
sd_spi_status_t SD_SPI_WriteSectors(const uint8_t *buf, uint32_t sector, uint32_t count);

/**
 * @brief Dong bo va cho the SD san sang cho thao tac tiep theo
 *
 * @details
 * Ham nay thuong duoc FATFS goi qua CTRL_SYNC de dam bao
 * moi thao tac ghi truoc do da duoc hoan tat
 */
sd_spi_status_t SD_SPI_Sync(void);

/**
 * @brief Lay tong so Sector cua the
 *
 * @retval So Sector 512 bytes
 */
uint32_t SD_SPI_GetSectorCount(void);

/**
 * @brief Lay kich thuoc block erase (khac voi Cluster nhe !)
 *
 * @retval So Sector trong 1 erase block
 */
uint32_t SD_SPI_GetEraseBlockSize(void);

/**
 * @brief Kiem tra xem the da khoi tao chua
 *
 * @retval
 * true (1) - da khoi tao
 * false (0) - chua duoc khoi tao
 */
bool SD_SPI_IsInitialized(void);

/**
 * @brief Lay loai the SD da duoc nhan dien
 */
sd_card_type_t SD_SPI_GetCardType(void);

/**
 * @brief Ham kiem tra xem SD Card co dang duoc cam hay khong
 */
bool SD_SPI_IsCardPresent(void);

#ifdef __cplusplus
}
#endif

#endif /* TARGET_SD_SPI_H_ */
