/*
 * @file MicroSD_config.h
 *
 * Created on: Dec 17, 2025
 * @author: LuongHuuPhuc
 *
 * @remark Day la file thuoc application layer cho cac file khac goi
 * nham muc dich luu data vao file SD, USER khong can dung truc tiep den SPI/FATFS
 * Chi can goi cac ham trong nay ra la duoc
 */

#ifndef INC_MICROSD_CONFIG_H_
#define INC_MICROSD_CONFIG_H_

#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "main.h"
#include "stdint.h"
#include "stdbool.h"

/*
 * Luong API duoc su dung :
 *  -> MicroSD_xxx() (MicroSD_config.h)
 *  -> f_xxx() (ff.h) (FATFS core)
 *  -> disk_xxx() (diskio.h) (dispatcher)
 *  -> user_diskio.c (CS + SPI o day, noi USER viet driver)
 *  -> HAL SPI
 *  -> SD Card
 * Thu vien nay la Layer cao, USER chi can goi la dung duoc luon
 */

typedef enum __SD_STATUS_t {
	SD_OK = 0,
	SD_ERR,
	SD_MOUNT_FAIL,
	SD_OPEN_FAIL,
	SD_WRITE_FAIL,
	SD_SYNC_FAIL
} sd_status_t;

// ==== FUCNTION PROTOTYPE ====

/**
 * @brief Ham khoi tao SD card
 * Chuc nang la Mount (Gan ket) toi the SD de nhan dien, ket noi va kich hoạt the SD
 * de nguoi dung co the truy cap, doc va ghi du lieu
 * @retval Trang thai cua the SD
 */
sd_status_t MicroSD_Init(void);

/**
 * @brìef Ham mo file co trong SD Card
 * Neu khong ton tai file, no se tao moi
 *
 * @param filename Ten file can luu data
 * @retval Trang thai cua the SD
 */
sd_status_t MicroSD_Open(const char *filename);

/**
 * @brief Ham ghi data
 * @retval Trang thai cua the SD
 */
sd_status_t MicroSD_WriteLine(const char *line);

/**
 * @brief Ham co chuc nang day data tu Sector buffer (RAM)
 * xuong SD card (Xa/lam trong buffer = Flush)
 * Giai phong bo nho dem va dam bao du lieu duoc ghi ngay lap tuc
 *
 * @note He dieu hanh va ngon ngu lap trinh can su dung bo nho dem de
 * gom nhieu thao tac ghi nho thanh 1 lan ghi lon, giup tang toc do
 * @retval Trang thai cua the SD
 */
sd_status_t MicroSD_Flush(void);

/**
 * @brief Ham dong file da ghi
 * @retval Trang thai cua the SD
 */
sd_status_t MicroSD_Close(void);


#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* INC_MICROSD_CONFIG_H_ */
