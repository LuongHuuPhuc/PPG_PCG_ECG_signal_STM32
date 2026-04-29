/*
 * @file data_dispatcher.c
 *
 *  Created on: Apr 23, 2026
 *      Author: ADMIN
 */

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "data_dispatcher.h"
#include "stdint.h"

#include "Logger.h"
#include "MicroSD_config.h"

static dispatch_target_t curr_target = DISPATCH_TO_NONE;
static dispatch_entry_t cb_map[MAX_DISPATCH_CB] = {0}; /* Khoi tao con tro den ham ban dau bang NULL */

/*-----------------------------------------------------------*/

void DataDispatcher_RegisterCb(dispatch_target_t type, dispatch_callback_t cb){
	if(type == DISPATCH_TO_UART) cb_map[0].cb = cb;
	if(type == DISPATCH_TO_SD) cb_map[1].cb = cb;
}

/*-----------------------------------------------------------*/

void DataDispatcher_UnregisterCb(void){
	cb_map[0].cb = NULL;
	cb_map[1].cb  = NULL;
}

/*-----------------------------------------------------------*/

void DataDispatcher_SetTarget(dispatch_target_t target){
	curr_target = target;
}

/*-----------------------------------------------------------*/

__attribute__((weak)) void DataDispatcher_Init(void){
	/* Co the khoi tao o noi khac */
	DataDispatcher_RegisterCb(DISPATCH_TO_UART, Logger_dispatch);
	DataDispatcher_RegisterCb(DISPATCH_TO_SD, SD_dispatch);

	DataDispatcher_SetTarget(DISPATCH_TO_UART);
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

	/* BOTH: tu dong goi ca 2 */

	/* NONE */
	if(curr_target == DISPATCH_TO_NONE) return osOK;
	return osOK;
}

#ifdef __cplusplus
}
#endif // __cplusplus
