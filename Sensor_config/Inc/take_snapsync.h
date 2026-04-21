/*
 * TaskSnap.h
 *
 *  Created on: Oct 16, 2025
 *      Author: ADMIN
 */

#ifndef INC_TAKE_SNAPSYNC_H_
#define INC_TAKE_SNAPSYNC_H_

#pragma once

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include "stdio.h"
#include "stdbool.h"

#include "main.h"
#include "cmsis_os.h"
#include "Sensor_config.h" // Lay block data tu struct `sensor_block_t

// Snapshot cho dong bo (Dung neu define `sensor_config_SYNC_USING`)
typedef struct SNAPSHOT_SYNC_t {
	volatile TickType_t timestamp;
	volatile uint32_t sample_id;
} __attribute__((unused)) snapshot_sync_t;

#ifdef SYNC_TO_LOGGER_MAIL_USING /* Su dung sync task lam trung gian*/
#define SYNC_QUEUE_LENGTH		35

/* Macro de Sensor Task gui den Sync Task (duoc dinh nghia trong take_snapsync)
 * Cung ten Macro voi Macro o Logger (nhung Macro do cho phep Sensor Task gui truc tiep den Logger luon)
 * Dung khi khong muon dong bo voi sync task */
#define MAIL_SEND_FROM_TASK(block) do { \
	if(sync_mail_send(&(block)) != osOK){ \
		/* Neu Sync Task xu ly hoac tran heap cham (kha nang cao se bi cai nay) thi se bi di vao day */ \
		uart_printf("[SYNC] Mail sent from TASK error !\r\n"); \
	} \
} while(0)

#ifdef CMSIS_API_USING
extern osThreadId sync_taskId;	   // Sync task ID
extern osMailQId sync_queueId;     // Sensor task -> Sync task
#endif // CMSIS_API_USING

/* NOTE: Bien nay duoc dinh nghia ben trong ham main.c, extern ra de cac file sensor khac co the dung */
extern volatile snapshot_sync_t global_sync_snapshot;

/* Slot de gom & dong bo du lieu trong SYNC */
typedef struct {
	/* Block trung gian de luu data tu dau vao Sensor Task cua 3 cam bien */
	sensor_block_t ecg;
	sensor_block_t ppg;
	sensor_block_t pcg;

	/* Tach ra tu Logger.h */
	bool ecg_ready;
	bool ppg_ready;
	bool pcg_ready;
} sync_slot_t;

/* Struct cho data output de tu Sync Task gui den Logger Task (khong the dung sensor_block_t vi bien volatile) */
typedef struct {
	uint32_t sample_id_sync; 	// Dung de dong bo du lieu
	TickType_t timestamp_sync;	// Danh dau thoi gian dong bo
	uint16_t count_sync; 		// So luong mau trong mang

	uint32_t ppg_ir_sync[32];
	int32_t pcg_sync[32];
	int16_t ecg_sync[32];
} sensor_sync_block_t ;

static inline osStatus sync_mail_send(sensor_block_t *block){
	sensor_block_t *mail = osMailAlloc(sync_queueId, 0); // Cap phat dong
	if(mail == NULL){
		return osErrorNoMemory;
	}

	*mail = *block; // Copy
	osStatus ret = osMailPut(sync_queueId, mail);
	if(ret != osOK){
		osMailFree(sync_queueId, mail);  /* Tranh memory leak neu khong gui duoc Mail */
	}
	return ret;
}

// ==== FUNCTION PROTOTYPE ====

HAL_StatusTypeDef Sync_init(void);

/**
 * @brief
 * @Note Tach viec sync frame block va cac logic kiem tra
 * ra khoi Logger de giam tai CPU => Giam latency de dong bo hieu qua hon
 */
void SyncTask(void const *pvParameter);

#endif  // SYNC_TO_LOGGER_MAIL_USING

/**
 * @brief Ham de luu gia tri 2 bien toan cuc `global_sample_id` va `global_timestamp`
 * Gan gia tri cua 2 bien toan cuc vao noi bo de gui vao task
 *
 * @param snap Con tro tro vao doi tuong snapshot_sync_t
 * @note Tranh xung dot hay bi ghi de
 */
__attribute__((unused)) static inline void take_snapshotSYNC(snapshot_sync_t *snap){

#ifdef SYNC_TO_LOGGER_MAIL_USING
	/* Dong bo tham so */
	snap->sample_id = global_sync_snapshot.sample_id;
	snap->timestamp = global_sync_snapshot.timestamp;
#endif // SYNC_TO_LOGGER_MAIL_USING
}

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* INC_TAKE_SNAPSYNC_H_ */
