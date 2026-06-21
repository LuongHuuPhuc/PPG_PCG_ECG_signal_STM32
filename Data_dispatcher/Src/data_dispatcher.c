/*
 * @file data_dispatcher.c
 *
 * @date Apr 23, 2026
 * @author LuongHuuPhuc
 */

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "data_dispatcher.h"
#include "stdint.h"

#include "Logger.h"				// Logger_dispatch()
#include "MicroSD_config.h"		// MicroSD_dispatch()
#include "sensor_pkt.h"			// PacketBuilder_dispatch()

static dispatch_target_t curr_target = DISPATCH_TO_NONE;
static dispatch_entry_t cb_map[MAX_DISPATCH_CB] = {0}; /* Khoi tao con tro den ham ban dau bang NULL */

/*-----------------------------------------------------------*/

static inline void DataDispatcher_SetTarget(DispatchTypeU32_t target){
	/* Gop co tich luy cho pheo duoc goi nhieu lan */
	curr_target |= (dispatch_target_t)target; // Cast DispatchType_t (uint32_t) ve dispatch_target_t
}

/*-----------------------------------------------------------*/

void DataDispatcher_RegisterCb(dispatch_target_t type, dispatch_callback_t cb){
	if(type == DISPATCH_TO_UART){
		cb_map[0].type = DISPATCH_TO_UART;
		cb_map[0].cb = cb;
	}

	if(type == DISPATCH_TO_SD){
		cb_map[1].type = DISPATCH_TO_SD;
		cb_map[1].cb = cb;
	}

	if(type == DISPATCH_TO_PACKET){
		cb_map[2].type = DISPATCH_TO_PACKET;
		cb_map[2].cb = cb;
	}
}

/*-----------------------------------------------------------*/

void DataDispatcher_UnregisterCb(void){
	cb_map[0].cb = NULL;
	cb_map[1].cb = NULL;
	cb_map[2].cb = NULL;
	curr_target = DISPATCH_TO_NONE;
}

/*-----------------------------------------------------------*/

__attribute__((weak)) void DataDispatcher_Init(DispatchTypeU32_t target){
	/* Nap tich luy co hieu cau hinh vao bien static */
	DataDispatcher_SetTarget(target);

	/* Dang ky callback tuong ung voi target */
	if(target & DISPATCH_TO_UART){
		DataDispatcher_RegisterCb(DISPATCH_TO_UART, Logger_dispatch);
	}

	if(target & DISPATCH_TO_SD){
		DataDispatcher_RegisterCb(DISPATCH_TO_SD, MicroSD_dispatch);
	}

	if(target & DISPATCH_TO_PACKET){
		DataDispatcher_RegisterCb(DISPATCH_TO_PACKET, PacketBuilder_dispatch);
	}
}

/*-----------------------------------------------------------*/

osStatus DataDispatcher_Send(sensor_sync_block_t *block){
	if(block == NULL) return osErrorParameter;

	/* UART */
	if(curr_target & DISPATCH_TO_UART)
		if(cb_map[0].cb) cb_map[0].cb(block);

	/* SD Card */
	if(curr_target & DISPATCH_TO_SD)
		if(cb_map[1].cb) cb_map[1].cb(block);

	/* Binary Packet */
	if(curr_target & DISPATCH_TO_PACKET)
		if(cb_map[2].cb) cb_map[2].cb(block);

	/* ALL: Binary Packet + SD Card */
	if(curr_target & DISPATCH_TO_ALL) return osOK;

	/* NONE */
	if(curr_target == DISPATCH_TO_NONE) return osOK;
	return osOK;
}

#ifdef __cplusplus
}
#endif // __cplusplus
