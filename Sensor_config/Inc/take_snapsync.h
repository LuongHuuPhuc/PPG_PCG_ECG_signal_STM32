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

#include "main.h"
#include "stdio.h"
#include "cmsis_os.h"
#include "Sensor_config.h"

// Slot de gom & dong bo du lieu trong SYNC
typedef struct {
	/* Block trung gian de luu data dau vao tu Sensor Task cua 3 cam bien */
	sensor_block_t ecg;
	sensor_block_t ppg;
	sensor_block_t pcg;

	/* Tach ra tu Logger.h */
	bool ecg_ready;
	bool max_ready;
	bool mic_ready;
} sync_slot_t;

// Struct cho data output de gui den LOGGER (khong the dung sensor_block_t vi bien volatile)
typedef struct {
	uint16_t count; 		// So luong mau trong mang
	uint32_t sample_id; 	// Dung de dong bo du lieu
	TickType_t timestamp;	// Danh dau thoi gian dong bo

	int16_t ecg[32];
	uint32_t ppg_ir[32];
	int32_t mic[32];
} sensor_sync_block_t ;

// Snapshot cho dong bo (Dung neu define `sensor_config_SYNC_USING`)
typedef struct SNAPSHOT_SYNC_t {
	volatile TickType_t timestamp;
	volatile uint32_t sample_id;
} __attribute__((unused)) snapshot_sync_t;

// Bien dong bo - SYNC PARAMETERS
#if defined(sensor_config_SYNC_USING)

extern volatile uint32_t global_sample_id;
extern volatile TickType_t global_timestamp;

#else
extern volatile snapshot_sync_t global_snapshot; // Bien nay duoc dinh nghia ben ngoai (External) trong ham main.c
#endif // sensor_config_SYNC_USING

#ifdef CMSIS_API_USING

extern osThreadId sync_taskId;	   // Sync task ID
extern osMailQId sync_queueId;     // Sensor task -> Sync task

#endif // CMSIS_API_USING

#ifdef SYNC_TO_LOGGER_MAIL_USING

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

#define SYNC_QUEUE_LENGTH		35

/* Macro de Sensor Task gui den Sync Task */
#define MAIL_SEND_FROM_TASK(block) do { \
	if(sync_mail_send(&(block)) != osOK){ \
		/* Neu Sync Task xu ly hoac tran heap cham thi se bi di vao day */ \
		uart_printf("[SYNC] Mail sent from TASK error !\r\n");\
	}\
} while(0)

#endif  // SYNC_TO_LOGGER_MAIL_USING

// ==== FUNCTION PROTOTYPE ====

/**
 * @brief
 * @retval
 */
HAL_StatusTypeDef Sync_init(void);

/**
 * @brief Ham de luu gia tri 2 bien toan cuc `global_sample_id` va `global_timestamp`
 * Gan gia tri cua 2 bien toan cuc vao noi bo de gui vao task
 *
 * @param snap Con tro tro vao doi tuong snapshot_sync_t
 * @note Tranh xung dot hay bi ghi de
 */
__attribute__((unused)) static inline void take_snapshotSYNC(snapshot_sync_t *snap){
	taskENTER_CRITICAL(); //Chan ISR tam thoi de tranh bi TIM ISR chen

#if defined(sensor_config_SYNC_USING)

	snap->sample_id = global_sample_id;
	snap->timestamp = global_timestamp;

#else
	/* Dong bo tham so */
	*snap = global_snapshot;

#endif // sensor_config_SYNC_USING

	taskEXIT_CRITICAL();
}

/**
 * @brief
 * @Note Tach viec sync frame block va cac logic kiem tra
 * ra khoi Logger de giam tai CPU => Giam latency de dong bo hieu qua hon
 */
void SyncTask(void const *pvParameter);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* INC_TAKE_SNAPSYNC_H_ */
