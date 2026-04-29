/*
 * @file take_snapsync.h
 *
 * @note
 * File co chuc nang dong bo tham so cho cac Sensor Task
 * truoc khi thuc hien cong viec tiep theo
 *
 * @date Oct 16, 2025
 * @Author: LuongHuuPhuc
 */

#ifndef INC_TAKE_SNAPSYNC_H_
#define INC_TAKE_SNAPSYNC_H_

#pragma once

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include "main.h"
#include "cmsis_os.h"

#include "Sensor_config.h" // Lay block data tu struct `sensor_block_t

// Snapshot cho dong bo (Dung neu define `sensor_config_SYNC_USING`)
typedef struct SNAPSHOT_SYNC_t {
	volatile TickType_t timestamp;
	volatile uint32_t sample_id;
} __attribute__((unused)) snapshot_sync_t;

#ifdef SYNC_INTERMEDIARY_USING /* Su dung sync task lam trung gian*/
#define SYNC_QUEUE_LENGTH		35

extern osThreadId sync_taskId;	   // Sync task ID

/* NOTE: Bien nay duoc dinh nghia ben trong ham main.c, extern ra de cac file sensor khac co the dung */
extern volatile snapshot_sync_t global_sync_snapshot;

/* Struct cho data output de tu Sync Task gui den Logger Task (khong the dung sensor_block_t vi bien volatile) */
typedef struct __sensor_sync_block{
	uint32_t sample_id_sync; 	// Dung de dong bo du lieu
	TickType_t timestamp_sync;	// Danh dau thoi gian dong bo
	uint16_t count_sync; 		// So luong mau trong mang

	uint32_t ppg_ir_sync[32];
	int32_t pcg_sync[32];
	int16_t ecg_sync[32];
} sensor_sync_block_t;

// ==== FUNCTION PROTOTYPE ====

HAL_StatusTypeDef Sync_init(void);

/**
 * @Note Tach viec sync frame block va cac logic kiem tra
 * ra khoi Logger de giam tai CPU => Giam latency de dong bo hieu qua hon
 */
void Sync_Task(void const *pvParameter);
osStatus Sync_mail_send(sensor_block_t *block);

/* Macro ho tro Sensor Task de gui data block den Sync Task cho viec dong bo data block */
#define MAIL_SEND_FROM_TASK_TO_SYNC(block) do { \
	if(Sync_mail_send(&(block)) != osOK){ \
		/* Neu Sync Task xu ly hoac tran heap cham (kha nang cao se bi cai nay) thi se bi di vao day */ \
		uart_printf("[SYNC] Mail sent from TASK error !\r\n"); \
	} \
} while(0)

#endif  // SYNC_INTERMEDIARY_USING

#ifndef MAIL_SEND_FROM_TASK_TO_SYNC /* Neu chua enable gi ca */
#define MAIL_SEND_FROM_TASK_TO_SYNC(block) do {} while(0) /* Dummy macros function tu Sensor -> Logger de tranh bi loi */
#endif // MAIL_SEND_FROM_TASK_TO_SYNC

/**
 * @brief Ham de luu gia tri 2 bien toan cuc `global_sample_id` va `global_timestamp`
 * Gan gia tri cua 2 bien toan cuc vao noi bo de gui vao task
 *
 * @param snap Con tro tro vao doi tuong snapshot_sync_t
 * @note Tranh xung dot hay bi ghi de
 */
__attribute__((unused)) static inline void take_snapshotSYNC(snapshot_sync_t *snap){

#ifdef SYNC_INTERMEDIARY_USING
	/* Dong bo tham so */
	snap->sample_id = global_sync_snapshot.sample_id;
	snap->timestamp = global_sync_snapshot.timestamp;
#endif // SYNC_INTERMEDIARY_USING
}

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* INC_TAKE_SNAPSYNC_H_ */
