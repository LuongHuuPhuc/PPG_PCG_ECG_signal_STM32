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
#include "stdio.h"
#include "stdbool.h"
#include "stdarg.h"

/**
 * @Flow thuc thi thuc te tu tang cao xuong tang thap:
 *
 * main.c:
 * -> MX_FATFS_Init() trong `App/fatfs.c` de link bien USER_Driver do nguoi dung viet (trong user_diskio) voi FatFS (trong `ff_gen_drv.c`)
 * 	  - File `ff_gen_drv.c` dinh nghia doi tuong `disk` voi kieu Disk_drvTypeDef co chuc nang la bang mapping driver
 * 	  - Bien `USER_Driver` (duoc user dinh nghia trong `user_diskio`) duoc truyen vao ham FATFS_LinkDriver() trong MX_FATFS_Init().
 * 	  - Trong `ff_gen_drv.c`, `disk` dung bien thanh vien co kieu Diskio_drvTypeDef `drv` de copy tham so cua USER_Driver da duoc truyen vao
 * 	  	dung de khoi tao drive
 * 	  - `disk` sau khi duoc gan thanh cong -> chuyen sang API low-level `diskio.c/.h` de san sang thuc hien thao tac giao tiep
 *
 * task/API:
 * -> Ung dung goi MicroSD_xxx() trong `MicroSD_config.c`
 * -> MicroSD_config.c goi cac API f_xxx() cua FatFS (`ff.c/ff.h`)
 *
 * -> FatFS lai goi xuong low-level API disk_xxx() trong `diskio.c/.h`
 *    - Day la lop trung gian cua FatFS, khi FatFS muon read/write sector, diskio.c can nhin vao bang `disk`.
 *    - Trong file nay, co extern object `disk` (dinh nghia truoc trong `ff_gen_drv.c`) de su dung.
 *    - diskio.c tra bang `disk` de tim USER_Driver
 *
 * -> diskio chuyen xuong bien `USER_Driver` duoc dinh nghia trong `user_diskio.c/.h`
 *
 * -> user_diskio lai goi driver muc thap SD_SPI_XXX() trong `sd_spi.c`
 *
 * -> sd_spi su dung HAL_SPI_xxx()
 *
 * -> Giao tiep voi module SD / SD card
 */

#ifndef MICROSD_MAX_LINE_LEN
#define MICROSD_MAX_LINE_LEN    256U /* Kich thuoc toi da cho 1 dong CSV duoc build trong RAM */
#endif // MICRO_MAX_LINE_LEN

typedef enum __SD_STATUS_t {
	SD_OK = 0,
	SD_ERR,
	SD_NOT_MOUNTED,
	SD_ALREADY_MOUNTED,
	SD_MOUNT_FAIL,
	SD_OPEN_FAIL,
	SD_WRITE_FAIL,
	SD_SYNC_FAIL,
	SD_CLOSE_FAIL,
	SD_UNMOUNT_FAIL,
	SD_INVALID_ARG,
	SD_BUFFER_OVERFLOW
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
 * @param wanna_clearfile co muon xoa noi dung file truoc khi ghi hay khong
 *
 * @retval Trang thai cua the SD
 */
sd_status_t MicroSD_Open(const char *filename, bool wanna_clearfile);

/**
 * @brief Ham co chuc nang day data tu Sector buffer (RAM)
 * xuong SD card (Xa/lam trong buffer = Flush)
 * Giai phong bo nho dem va dam bao du lieu duoc ghi ngay lap tuc thay vi cho bo dem day
 * Giong voi viec dong bo data (sync)
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

/**
 * @brief Ham thao/ngat ket noi thao the an toan
 * Ngat toan bo he thong the (phan cung)
 */
sd_status_t MicroSD_Unmount(void);

/**
 * @brief Kiem tra xem Filesystem co dang duoc gan vao khong
 * (SD Card co dang chay)
 */
bool MicroSD_IsMounted(void);

/**
 * @brief Kiem tra xem File co dang duoc mo khong
 */
bool MicroSD_IsOpened(void);

/**
 * @brief Ham ghi data theo 1 dong vao file hien tai
 *
 * @details
 * Ham nay se ghi chuoi dau vao dung nhu duoc cung cap
 * Neu muon xuong 1 dong moi, phai bao gom '\\r\\n'
 *
 * @param[in] line Chuoi ket thuc bang ky tu Null (Null-Terminated)
 *
 * @retval Trang thai cua the SD
 */
sd_status_t MicroSD_WriteLine(const char *fmt,...);

/**
 * @brief Ham thuc hien ghi data theo dang CSV tu chuoi gia tri unsigned 32-bit
 *
 * @details
 * Dinh dang tao ra theo kieu:
 * `value0,value1,value2,value3,...\\r\\n'
 *
 * @param[in] data Con tro den chuoi gia tri dau vao
 * @param[in] num_cols So cot mong muon ghi vao
 */
sd_status_t MicroSD_WriteCSV_U32(const uint32_t *data, uint16_t num_cols);

/**
 * @brief Ham thuc hien ghi data theo dang CSV tu chuoi gia tri signed 32-bit
 *
 * @details
 * Dinh dang tao ra theo kieu:
 * `value0,value1,value2,value3,...\\r\\n'
 *
 * @param[in] data Con tro den chuoi gia tri dau vao
 * @param[in] num_cols So cot mong muon ghi vao
 */
sd_status_t MicroSD_WriteCSV_I32(const int32_t *data, uint16_t num_cols);

/**
 * @brief Viet 1 dong CSV tu 1 mang chuoi
 *
 * @details
 * Ding dang tao ra theo kieu:
 * `col0,col1,col2,...\\r\\n`
 *
 * @param[in] cols Con tro cua mang chuoi dau vao
 * @param[in] num_cols So cot muon ghi
 */
sd_status_t MicroSD_WriteCSV_Str(const char *cols[], uint16_t num_cols);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* INC_MICROSD_CONFIG_H_ */
