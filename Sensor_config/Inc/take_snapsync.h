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
#include "FreeRTOS.h"
#include "task.h"
#include "Sensor_config.h"

//Snapshot cho dong bo
typedef struct SNAPSHOT_SYNC_t {
	volatile TickType_t timestamp;
	volatile uint32_t sample_id;
} snapshot_sync_t;

#if defined(sensor_config_SYNC_USING)

extern volatile uint32_t global_sample_id;
extern volatile TickType_t global_timestamp;

#else
extern volatile snapshot_sync_t global_snapshot; // Bien nay duoc dinh nghia ben ngoai (External) trong ham main.c
#endif // sensor_config_SYNC_USING

/**
 * @brief Ham de luu gia tri 2 bien toan cuc `global_sample_id` va `global_timestamp`
 * Gan gia tri cua 2 bien toan cuc vao noi bo de gui vao task
 *
 * @param snap Con tro tro vao doi tuong snapshot_sync_t
 * @note Tranh xung dot hay bi ghi de
 */
__attribute__((unused)) __STATIC_INLINE void take_snapshotSYNC(snapshot_sync_t *snap){
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

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* INC_TAKE_SNAPSYNC_H_ */
