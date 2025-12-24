/*
 * @file MicroSD_config.c
 *
 * Created on: Dec 17, 2025
 * @author LuongHuuPhuc
 */

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "MicroSD_config.h"
#include "ff.h"
#include "string.h"

//====== VARIABLES DEFINITION ======

static FATFS fs; // Doi tuong cho cau truc File System
static FIL file; // Doi tuong cho cau truc file
static bool is_Mounted = false;
static bool is_Opened = false;

//====== FUCNTION DEFINITION ======

/*-----------------------------------------------------------*/

sd_status_t MicroSD_Init(void){
	if(f_mount(&fs, "", 1) != FR_OK){
		return SD_MOUNT_FAIL;
	}

	is_Mounted = true;
	return SD_OK;
}
/*-----------------------------------------------------------*/

sd_status_t MicroSD_Open(const char *filename){
	if(!is_Mounted) return SD_ERR;

	if(f_open(&file, filename, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK){
		return SD_OPEN_FAIL;
	}

	is_Opened = true;
	return SD_OK;
}
/*-----------------------------------------------------------*/

sd_status_t MicroSD_WriteLine(const char *line){
	if(!is_Opened) return SD_ERR;

	UINT bw;
	if(f_write(&file, line, strlen(line), &bw) != FR_OK){
		return SD_WRITE_FAIL;
	}
	return SD_OK;
}
/*-----------------------------------------------------------*/

sd_status_t MicroSD_Flush(void){
	if(!is_Opened) return SD_ERR;

	if(f_sync(&file) != FR_OK){
		return SD_SYNC_FAIL;
	}
	return SD_OK;
}
/*-----------------------------------------------------------*/

sd_status_t MicroSD_Close(void){
	if(is_Opened){
		f_close(&file);
		is_Opened = false;
	}
	return SD_OK;
}
/*-----------------------------------------------------------*/

#ifdef __cplusplus
}
#endif // __cplusplus
