/* USER CODE BEGIN Header */
/**
 ******************************************************************************
  * @file    user_diskio.c
  * @brief   This file includes a diskio driver skeleton to be completed by the user.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
 /* USER CODE END Header */

#ifdef USE_OBSOLETE_USER_CODE_SECTION_0
/*
 * Warning: the user section 0 is no more in use (starting from CubeMx version 4.16.0)
 * To be suppressed in the future.
 * Kept to ensure backward compatibility with previous CubeMx versions when
 * migrating projects.
 * User code previously added there should be copied in the new user sections before
 * the section contents can be deleted.
 */
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */
#endif

/* USER CODE BEGIN DECL */

/**
 * @author Luong Huu Phuc
 * @date 2026/03/11
 *
 * @details
 * - Day la phan de user implement logic va chuong trinh driver phan cung giao tiep giua SPI va SD Card (sd_spi)
 * (Cau noi giua FATFS va Driver SD SPI low-level driver)
 * - Trong day dinh nghia USER_Driver, bien ma doi tuong `disk` trong diskio.c chuyen tiep loi goi den Driver duoc dang ky trong kieu du lieu cua no.
 *
 * @note
 * Cac ham nay khong truc tiep giao tiep voi the SD ma se goi xuong driver muc thap sd_spi.c
 * roi tu do se xu ly cac logic phan cung qua ham HAL_SPI_xxx()
 */

/* Includes ------------------------------------------------------------------*/

#include "user_diskio.h"
#include "diskio.h"

#include "sd_spi_internal_macros.h"
#include "sd_spi.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
/* Disk status */

static volatile DSTATUS Stat = STA_NOINIT;
extern void uart_printf(const char *fmt,...);

/* USER CODE END DECL */

/* Private function prototypes -----------------------------------------------*/
DSTATUS USER_initialize (BYTE pdrv);
DSTATUS USER_status (BYTE pdrv);
DRESULT USER_read (BYTE pdrv, BYTE *buff, DWORD sector, UINT count);
#if _USE_WRITE == 1
  DRESULT USER_write (BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);
#endif /* _USE_WRITE == 1 */
#if _USE_IOCTL == 1
  DRESULT USER_ioctl (BYTE pdrv, BYTE cmd, void *buff);
#endif /* _USE_IOCTL == 1 */

Diskio_drvTypeDef  USER_Driver =
{
  USER_initialize,
  USER_status,
  USER_read,
#if  _USE_WRITE
  USER_write,
#endif  /* _USE_WRITE == 1 */
#if  _USE_IOCTL == 1
  USER_ioctl,
#endif /* _USE_IOCTL == 1 */
};

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initializes a Drive
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_initialize (
	BYTE pdrv           /* Physical drive nmuber to identify the drive */
)
{
  /* USER CODE BEGIN INIT */
	if(pdrv != 0){
		return STA_NOINIT;
	}

	if(SD_SPI_InitCard() == SD_SPI_OK){
		Stat &= (DSTATUS)(~STA_NOINIT);
	}
	else{
		Stat = STA_NOINIT;
	}
	return Stat;
  /* USER CODE END INIT */
}

/**
  * @brief  Gets Disk Status
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_status (
	BYTE pdrv       /* Physical drive number to identify the drive */
)
{
  /* USER CODE BEGIN STATUS */
	if(pdrv != 0){
		return STA_NOINIT;
	}

	// Ham nay co the duoc goi lai nhieu lan trong f_xxx() API nen
	// se lien tuc in ra log neu ban dung ham in ra man hinh
	if(SD_SPI_IsInitialized()){
		Stat &= (DSTATUS)(~STA_NOINIT);
	}
	else{
	    Stat |= STA_NOINIT;
	}
    return Stat;
  /* USER CODE END STATUS */
}

/**
  * @brief  Reads Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */
DRESULT USER_read (
	BYTE pdrv,      /* Physical drive nmuber to identify the drive */
	BYTE *buff,     /* Data buffer to store read data */
	DWORD sector,   /* Sector address in LBA */
	UINT count      /* Number of sectors to read */
)
{
  /* USER CODE BEGIN READ */
	if((pdrv != 0U) || (buff == NULL) || (count == 0U)) return RES_PARERR;

	if(Stat & STA_NOINIT) return RES_NOTRDY;

	SD_SPI_ERROR_CHECK(SD_SPI_ReadSectors(buff, sector, count), USER_DISKIO, RES_ERROR);
	return RES_OK;

  /* USER CODE END READ */
}

/**
  * @brief  Writes Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
#if _USE_WRITE == 1
DRESULT USER_write (
	BYTE pdrv,          /* Physical drive nmuber to identify the drive */
	const BYTE *buff,   /* Data to be written */
	DWORD sector,       /* Sector address in LBA */
	UINT count          /* Number of sectors to write */
)
{
  /* USER CODE BEGIN WRITE */
  /* USER CODE HERE */
	if((pdrv != 0U) || (buff == NULL) || (count == 0U)){
		return RES_PARERR;
	}

	if(Stat & STA_NOINIT){
		return RES_NOTRDY;
	}

	if(Stat & STA_PROTECT){
		return RES_WRPRT;
	}

	SD_SPI_ERROR_CHECK(SD_SPI_WriteSectors(buff, sector, count), USER_DISKIO, RES_ERROR);
	return RES_OK;

  /* USER CODE END WRITE */
}
#endif /* _USE_WRITE == 1 */

/**
  * @brief  I/O control operation
  * @param  pdrv: Physical drive number (0..)
  * @param  cmd: Control code
  * @param  *buff: Buffer to send/receive control data
  * @retval DRESULT: Operation result
  */
#if _USE_IOCTL == 1
DRESULT USER_ioctl (
	BYTE pdrv,      /* Physical drive nmuber (0..) */
	BYTE cmd,       /* Control code */
	void *buff      /* Buffer to send/receive control data */
)
{
  /* USER CODE BEGIN IOCTL */

	/**
	 * Cac lenh quan trong duoc FatFS goi:
	 * 	- CTRL_SYNC
	 * 	- GET_SECTOR_COUNT
	 * 	- GET_SECTOR_SIZE
	 * 	- GET_BLOCK_SIZE
	 */
	if(pdrv != 0U) return RES_PARERR;

	/* Note: rieng CTRL_SYNC khong bat buoc can buff */
	if((cmd != CTRL_SYNC) && (buff == NULL)) return RES_PARERR;

	if(Stat & STA_NOINIT) return RES_NOTRDY;

	switch(cmd){
		case CTRL_SYNC:{
			return (SD_SPI_Sync() == SD_SPI_OK) ? RES_OK : RES_ERROR;
		}
		case GET_SECTOR_COUNT:{
			*(DWORD*)buff = (DWORD)SD_SPI_GetSectorCount();
			return RES_OK;
		}
		case GET_SECTOR_SIZE:{
			*(WORD*)buff = SD_SECTOR_SIZE;
			return RES_OK;
		}
		case GET_BLOCK_SIZE:{
			*(DWORD*)buff = (DWORD)SD_SPI_GetEraseBlockSize();
			return RES_OK;
		}
		default:{
			return RES_PARERR;
		}
	}
  /* USER CODE END IOCTL */
}
#endif /* _USE_IOCTL == 1 */

