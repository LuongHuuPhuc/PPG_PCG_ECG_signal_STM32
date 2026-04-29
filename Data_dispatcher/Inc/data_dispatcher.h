/*
 * @file data_dispatcher.h
 *
 * @note
 * Muc dich cua file nay la trung tam dieu huong cho Sync Task nen gui
 * block data da gom duoc den UART/SD Card hoac ca 2 cung luc
 *
 * @date Apr 22, 2026
 * @author LuongHuuPhuc
 */

#ifndef INC_DATA_DISPATCHER_H_
#define INC_DATA_DISPATCHER_H_

#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "cmsis_os.h"
#include "take_snapsync.h"

#define MAX_DISPATCH_CB			2

typedef enum {
	DISPATCH_TO_NONE = 0,
	DISPATCH_TO_UART = (1 << 0), 	          /* Cho phep data den task Logger de in ra qua UART */
	DISPATCH_TO_SD 	 = (1 << 1),		  	  /* Cho phep data den task cua SD Card */
	DISPATCH_TO_BOTH = (1 << 0) | (1 << 1)	  /* Dung ca 2 cung luc */
} dispatch_target_t;

/* Callback */
typedef void (*dispatch_callback_t)(sensor_sync_block_t *block);

typedef struct {
	dispatch_target_t type;
	dispatch_callback_t cb;
} dispatch_entry_t;

__attribute__((weak)) void DataDispatcher_Init(void);

/* Cho phep cac ham khac dang ky callback, mac dinh index 0 = UART, index 1 = SD */
void DataDispatcher_RegisterCb(dispatch_target_t type, dispatch_callback_t cb);

/* Huy dang ky callback */
void DataDispatcher_UnregisterCb(void);

/* Set huong muon gui data den (UART, SD) truoc khi Send (quan trong) */
void DataDispatcher_SetTarget(dispatch_target_t target);

/* Thuc hien callback */
osStatus DataDispatcher_Send(sensor_sync_block_t *block);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* INC_DATA_DISPATCHER_H_ */
