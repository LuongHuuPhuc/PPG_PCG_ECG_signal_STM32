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

#include "stdint.h"
#include "string.h"
#include "ff.h"

#include "cmsis_os.h"

/* Dung object do CubeMX sinh ra trong FATFS/App/fatfs.c */

extern FATFS USERFatFS;
extern char USERPath[4];
extern void uart_printf(const char *fmt,...);

//====== VARIABLES DEFINITION ======

static FIL file; // Doi tuong cho kieu cau truc File hien tai dang mo

/* Flag trang thai */
static bool is_Mounted = false;
static bool is_Opened = false;

char g_line_buffer[MICROSD_MAX_LINE_LEN];  /* Kich thuoc toi da cho 1 dong CSV duoc build trong RAM */

/* Them mutex de bao ve tranh truong hop race condition */
osMutexId microsd_mutexId = NULL;

/* ===================== HELPER FUCNTION DEFINITION ===================== */

/**
 * @brief Ham ho tro noi bo de them van ban da dinh dang mot cach an toan vao bo dem dong
 *
 * @params[in, out] line Line Buffer can ghi
 * @param[in] max_len Tong kich thuoc cua line buffer
 * @param[in] cur_len Chieu dai thuc te duoc su dung trong line buffer
 * @param[in] fmt printf-style format string
 * @param[in] value_ll Gia tri so can them vao
 */
static sd_status_t MicroSD_AppendFormattedLongLong(char *line, uint32_t max_len, uint32_t *cur_len, const char *fmt, long long value_ll){
	int ret;

	if((line == NULL) || (cur_len == NULL) || (fmt == NULL)) return SD_INVALID_ARG;

	// snprintf tra ve so ky tu da duoc ghi vao buffer `line`, khong bao gom `\0`
	ret = snprintf(&line[*cur_len], (size_t)(max_len - *cur_len), fmt, value_ll);
	if((ret < 0) || ((uint32_t)ret >= (max_len - *cur_len))){
		return SD_BUFFER_OVERFLOW;
	}

	*cur_len += (uint32_t)ret;
	return SD_OK;
}

/*-----------------------------------------------------------*/

static sd_status_t MicroSD_AppendFormattedString(char *line, uint32_t max_len, uint32_t *cur_len, const char *fmt, const char *str){
	int ret;

	if((line == NULL) || (cur_len == NULL) || (fmt == NULL) || (str == NULL)) return SD_INVALID_ARG;

	// snprintf tra ve so ky tu da duoc ghi vao buffer `line`, khong bao gom `\0`
	ret = snprintf(&line[*cur_len], (size_t)(max_len - *cur_len), fmt, str);
	if((ret < 0) || ((uint32_t)ret >= (max_len - *cur_len))){
		return SD_BUFFER_OVERFLOW;
	}

	*cur_len += (uint32_t)ret;
	return SD_OK;
}

/*-----------------------------------------------------------*/

sd_status_t MicroSD_MutexInit(void){
	if(microsd_mutexId != NULL) return SD_OK;

	/* Khoi tao mutex cho API cua Micro SD */
	osMutexDef(microSDMutexName);
	microsd_mutexId = osMutexCreate(osMutex(microSDMutexName));
	if(microsd_mutexId == NULL){
		uart_printf("[MICRO_SD] Failed to create Semaphores Mutex !\r\n");
		return SD_ERR;
	}
	return SD_OK;
}

/*-----------------------------------------------------------*/

static inline void MicroSD_MUTEXLOCK(void){
	if(microsd_mutexId != NULL){
		osMutexWait(microsd_mutexId, osWaitForever);
	}
}

/*-----------------------------------------------------------*/

static inline void MicroSD_MUTEXUNLOCK(void){
	if(microsd_mutexId != NULL){
		osMutexRelease(microsd_mutexId);
	}
}

/* ===================== FUCNTION DEFINITION ===================== */

/*-----------------------------------------------------------*/

sd_status_t MicroSD_Init(void){
	FRESULT fr;
	BYTE work[_MAX_SS];

	if(MicroSD_MutexInit() != SD_OK) return SD_ERR;

    MicroSD_MUTEXLOCK();
	if(is_Mounted){
		MicroSD_MUTEXUNLOCK();
		return SD_OK;
	}

    uart_printf("[MICRO_SD] USERPath = %s\r\n", USERPath);

    fr = f_mount(&USERFatFS, (TCHAR const *)USERPath, 1);
    if(fr == FR_NO_FILESYSTEM){
        uart_printf("[MICRO_SD] No FAT filesystem, start f_mkfs...\r\n");

        // Khoi tao lai Fat/exFat khi the chua duoc format
        fr = f_mkfs((TCHAR const*)USERPath, FM_FAT32, 0, work, sizeof(work));
        if(fr != FR_OK){
    		uart_printf("[MICRO_SD] Create Fat volume failed (f_mkfs), fr = %d\r\n", fr);
    		MicroSD_MUTEXUNLOCK();
        	return SD_MOUNT_FAIL;
        }
        fr = f_mount(&USERFatFS, (TCHAR const *)USERPath, 1);

    }
	if(fr != FR_OK){
		uart_printf("[MICRO_SD] SD Card mount failed (f_mount), fr = %d\r\n", fr);
		/**
		 * Neu loi tra ve fr = 13 (FR_NO_FILESYSTEM) nghia la FatFS da doc duoc
		 * boot sector/partition area hop le nhung khong tim thay Fat12/16/32 hop le
		 * hoac the dang:
		 * 	- chua format
		 * 	- format kieu khong duoc ho tro
		 * 	- filesystem bi hong
		 * -> Can format lai bang may tinh + them code ho tro tu format bang FatFS
		 */
		MicroSD_MUTEXUNLOCK();
		return SD_MOUNT_FAIL;
	}
	MicroSD_MUTEXUNLOCK();

	is_Mounted = true;
	return SD_OK;
}

/*-----------------------------------------------------------*/


/**
 * @brief Ham xoa toan bo dung co trong file hien tai
 * @note
 * Dung truoc khi bat dau ghi data vao 1 file (tranh ghi tiep du lieu len data cu khong mong muon)
 * Rieng ham nay se khong co Mutex bao ve vi no duoc goi truc tiep trong `MicroSD_Init()`
 * de tranh Mutex bi goi lan nhau
 *
 * @params[in] loc_file con tro cua tham so can truyen vao (kieu FIL)
 */
// Phai truyen con tro vi chua biet size -> ko the truyen value
static sd_status_t MicroSD_ClearFile(FIL *loc_file){
	FRESULT fr;

	// Dua con tro ve dau file
	fr = f_lseek(loc_file, 0);
	if(fr != FR_OK){
		f_close(loc_file);
		return SD_ERR;
	}

	// Cat file ve 0 byte
	fr = f_truncate(loc_file);
	if(fr != FR_OK){
		f_close(loc_file);
		return SD_ERR;
	}

	// Sync de dam bao thuc su ghi xuong SD
	fr = f_sync(loc_file);
	if(fr != FR_OK) return SD_SYNC_FAIL;

	return SD_OK;
}

/*-----------------------------------------------------------*/

sd_status_t MicroSD_Open(const char *filename, bool wanna_clearfile){
	FRESULT fr;

	if(!is_Mounted) return SD_NOT_MOUNTED;
	if(filename == NULL) return SD_ERR;

	MicroSD_MUTEXLOCK();
	if(is_Opened){ // Neu dang mo file khac thi dong lai
		if(f_sync(&file) != FR_OK){
			MicroSD_MUTEXUNLOCK();
			return SD_SYNC_FAIL;
		}
		if(f_close(&file) != FR_OK){
			MicroSD_MUTEXUNLOCK();
			return SD_CLOSE_FAIL;
		}
		is_Opened = false;
	}

	// Mo file
	fr = f_open(&file, filename, FA_WRITE | FA_OPEN_ALWAYS); // ghi + noi tiep cai cu
	if(fr != FR_OK){
		uart_printf("[MICRO_SD] Failed to open file (f_open): %d\r\n", fr);
		MicroSD_MUTEXUNLOCK();
		return SD_OPEN_FAIL;
	}

	if(wanna_clearfile){
		if(MicroSD_ClearFile(&file) != SD_OK){
			f_close(&file);
			MicroSD_MUTEXUNLOCK();
			return SD_ERR;
		}
		uart_printf("[MICRO_SD] Clear file %s success!\r\n", filename);
	}
	else{
		// Di chuyen con tro ve cuoi file
		// f_size(&file) - Vi tri byte muon chuyen toi (tinh tu dau tep) ~ cuoi file
		fr = f_lseek(&file, f_size(&file));
		if(fr != FR_OK){
			uart_printf("[MICRO_SD] Failed to move pointer to the end of file (f_lseek): %d\r\n", fr);
			(void)f_close(&file);

			MicroSD_MUTEXUNLOCK();
			return SD_OPEN_FAIL;
		}
	}
	MicroSD_MUTEXUNLOCK();

	is_Opened = true;
	return SD_OK;
}

/*-----------------------------------------------------------*/

sd_status_t MicroSD_WriteLine(const char *fmt,...){
	UINT bw = 0;
	UINT len;
	FRESULT fr;

	if(!is_Opened) return SD_ERR;
	if(fmt == NULL) return SD_ERR;

	va_list args;
	va_start(args, fmt);

	// format vao buffer
	int ret = vsnprintf(g_line_buffer, MICROSD_MAX_LINE_LEN, fmt, args);
	va_end(args);

	if(ret < 0 || ret >= MICROSD_MAX_LINE_LEN){
		return SD_BUFFER_OVERFLOW;
	}
	len = (UINT)ret;

	MicroSD_MUTEXLOCK();
	fr = f_write(&file, g_line_buffer, len, &bw);

	if(fr != FR_OK || bw != len){
		// Neu bi loi o SD_SPI_WriteSectors() hay USER_write hoac loi khac
		uart_printf("[MICRO_SD] Write data failed (f_write): %d bw=%u\r\n", fr, bw);
		MicroSD_MUTEXUNLOCK();
		return SD_WRITE_FAIL;
	}
	MicroSD_MUTEXUNLOCK();
	return SD_OK;
}

/*-----------------------------------------------------------*/

sd_status_t MicroSD_Flush(void){
	FRESULT fr;
	if(!is_Opened) return SD_ERR;

	MicroSD_MUTEXLOCK();
	fr = f_sync(&file);

	if (fr != FR_OK){
		uart_printf("[MICRO_SD] Sync data failed (f_sync): %d\r\n", fr);
		MicroSD_MUTEXUNLOCK();
		return SD_SYNC_FAIL;
	}
	uart_printf("[MICRO_SD] Sync data OK (f_sync): %d\r\n", fr);
	MicroSD_MUTEXUNLOCK();
	return SD_OK;
}

/*-----------------------------------------------------------*/

sd_status_t MicroSD_Close(void){
	FRESULT fr;
	if(!is_Opened) return SD_OK;

	MicroSD_MUTEXLOCK();
	fr = f_sync(&file);
	if (fr != FR_OK){
		uart_printf("[MICRO_SD] Sync data failed (f_sync): %d\r\n", fr);
		MicroSD_MUTEXUNLOCK();
		return SD_SYNC_FAIL;
	}

	fr = f_close(&file);
	if (fr != FR_OK){
		uart_printf("[MICRO_SD] Close file failed (f_close): %d\r\n", fr);
		MicroSD_MUTEXUNLOCK();
		return SD_CLOSE_FAIL;
	}
	MicroSD_MUTEXUNLOCK();

	is_Opened = false;
	return SD_OK;
}

/*-----------------------------------------------------------*/

sd_status_t MicroSD_Unmount(void){
	FRESULT fr;

	MicroSD_MUTEXLOCK();
	if(is_Opened){
		/* Cung 1 API MicroSD goi lan nhau -> Deadlock -> Khong goi MicroSD_Close */
		// Inline code MicroSD_Close() vao day

		fr = f_sync(&file);
		if (fr != FR_OK){
			uart_printf("[MICRO_SD] Sync data failed (f_sync): %d\r\n", fr);
			MicroSD_MUTEXUNLOCK();
			return SD_SYNC_FAIL;
		}

		fr = f_close(&file);
		if (fr != FR_OK){
			uart_printf("[MICRO_SD] Close file failed (f_close): %d\r\n", fr);
			MicroSD_MUTEXUNLOCK();
			return SD_CLOSE_FAIL;
		}
		is_Opened = false;
	}

	if(is_Mounted){
		if(f_mount(NULL, (TCHAR const *)USERPath, 1) != FR_OK){
			MicroSD_MUTEXUNLOCK();
			return SD_UNMOUNT_FAIL;
		}
		is_Mounted = false;
	}
	MicroSD_MUTEXUNLOCK();
	return SD_OK;
}

/*-----------------------------------------------------------*/

bool MicroSD_IsMounted(void){
	bool ret;
	MicroSD_MUTEXLOCK();
	ret = is_Mounted;
	MicroSD_MUTEXUNLOCK();
	return ret;
}

/*-----------------------------------------------------------*/

bool MicroSD_IsOpened(void){
	bool ret;
	MicroSD_MUTEXLOCK();
	ret = is_Opened;
	MicroSD_MUTEXUNLOCK();
	return ret;
}

/*-----------------------------------------------------------*/

sd_status_t MicroSD_WriteCSV_U32(const uint32_t *data, uint16_t num_cols){
	uint32_t len = 0U;
	sd_status_t st;

	if((data == NULL) || (num_cols == 0U)) return SD_INVALID_ARG;

	g_line_buffer[0]= '\0';

	for(uint16_t i = 0U; i < num_cols; i++){
		st = MicroSD_AppendFormattedLongLong(g_line_buffer,
											 MICROSD_MAX_LINE_LEN,
											 &len,
											 (i < (num_cols - 1U)) ? "%llu," : "%llu\r\n", // Xem khi nao can xuong dong
										     (unsigned long long)data[i]);
		if(st != SD_OK) return st;
	}
	return MicroSD_WriteLine(g_line_buffer);
}

/*-----------------------------------------------------------*/

sd_status_t MicroSD_WriteCSV_I32(const int32_t *data, uint16_t num_cols){
	uint32_t len = 0U;
	sd_status_t st;

	if((data == NULL) || (num_cols == 0U)) return SD_INVALID_ARG;

	g_line_buffer[0]= '\0';

	for(uint16_t i = 0U; i < num_cols; i++){
		st = MicroSD_AppendFormattedLongLong(g_line_buffer,
											 MICROSD_MAX_LINE_LEN,
											 &len,
											 (i < (num_cols - 1U)) ? "%lld," : "%lld\r\n", // Xem khi nao can xuong dong
											 (long long)data[i]);
		if(st != SD_OK) return st;
	}
	return MicroSD_WriteLine(g_line_buffer);
}

/*-----------------------------------------------------------*/

sd_status_t MicroSD_WriteCSV_Str(const char *cols[], uint16_t num_cols){
	uint32_t len = 0U;
	sd_status_t st;

	if((cols == NULL) || (num_cols == 0U)) return SD_INVALID_ARG;

	g_line_buffer[0]= '\0';

	for(uint16_t i = 0U; i < num_cols; i++){
		st = MicroSD_AppendFormattedString(g_line_buffer,
										   MICROSD_MAX_LINE_LEN,
										   &len,
										   (i < (num_cols - 1U)) ? "%s," : "%s\r\n", // Xem khi nao can xuong dong
										   cols[i]);
		if(st != SD_OK) return st;
	}
	return MicroSD_WriteLine(g_line_buffer);
}

#ifdef __cplusplus
}
#endif // __cplusplus
