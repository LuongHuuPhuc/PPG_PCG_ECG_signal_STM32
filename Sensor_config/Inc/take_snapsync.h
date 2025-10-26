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
typedef struct {
	TickType_t timestamp;
	uint32_t sample_id;
} snapshot_sync_t;


extern volatile uint32_t global_sample_id;
extern TickType_t global_timstamp;

/**
 * @brief Ham de luu gia tri 2 bien toan cuc `global_sample_id` va `global_timestamp`
 * @note Tranh xung dot hay bi ghi de
 */
static inline void take_snapshotSYNC(snapshot_sync_t *snap){
	taskENTER_CRITICAL(); //Chan ISR tam thoi de tranh bi TIM ISR chen
	snap->sample_id = global_sample_id;
	snap->timestamp = global_timestamp;
	taskEXIT_CRITICAL();

}

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* INC_TAKE_SNAPSYNC_H_ */
