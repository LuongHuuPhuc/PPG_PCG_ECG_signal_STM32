/*
 * @file data_dispatcher.h
 *
 * @note
 * Muc dich cua file nay la trung tam dieu huong cho Sync Task nen gui
 * block data da gom duoc den UART/SD Card hoac ca 2 cung luc (Routing)
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

#include "dispatch_target_def.h"
#include "cmsis_os.h"
#include "take_snapsync.h"

#define MAX_DISPATCH_CB			3
#define DISPATCH_CB_UART		0
#define DISPATCH_CB_SD			1
#define DISPATCH_CB_PACKET		2

typedef enum __dispatch_target_type{
	DISPATCH_TO_NONE 	= DISPATCH_NONE,
	DISPATCH_TO_UART 	= DISPATCH_UART, 		/* 0x01 Cho phep data den task Logger de in ra qua UART */
	DISPATCH_TO_SD 	 	= DISPATCH_SD,			/* 0x02 Cho phep data den task cua SD Card */
	DISPATCH_TO_PACKET	= DISPATCH_PACKET,		/* 0x03 Cho phep data dong goi thanh Binary Packet roi di qua UART */
	DISPATCH_TO_ALL 	= DISPATCH_ALL		 	/* 0x06 Binary Packet + SD Card */
} dispatch_target_t;

/* Callback */
typedef void (*dispatch_callback_t)(sensor_sync_block_t *block);

typedef struct {
	dispatch_target_t type;
	dispatch_callback_t cb;
} dispatch_entry_t;

/**
 * @brief Ham set target muon dung va dang ky callback tuong ung voi target
 * @param[in] target Dich den muon gui data den (UART, SD Card, Binary Packet, All)
 * @note Co the khoi tao o noi khac (weak function)
 */
__attribute__((weak)) void DataDispatcher_Init(DispatchTypeU32_t target);

/* Cho phep cac ham khac dang ky callback, mac dinh index 0 = UART, index 1 = SD */
void DataDispatcher_RegisterCb(dispatch_target_t type, dispatch_callback_t cb);

/* Huy dang ky callback */
void DataDispatcher_UnregisterCb(void);

/* Thuc hien callback　(chay trong context cua Sync Task) */
osStatus DataDispatcher_Send(sensor_sync_block_t *block);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* INC_DATA_DISPATCHER_H_ */
