/*
 * @file SWV_debug.c
 *
 * @date Jun 2, 2026
 * @author LuongHuuPhuc
 */

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "SWV_debug.h"
#include "core_cm4.h"	// ITM_SendChar()
#include "main.h"

#ifdef DEBUG_SWV_ITM
void SWV_Init(void){
	/* Enable trace subsystem */
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

	/* Enable ITM */
	ITM->TCR = ITM_TCR_ITMENA_Msk | ITM_TCR_TSENA_Msk | ITM_TCR_SYNCENA_Msk;

	/* Enable stimulus port 0 */
	ITM->TER = 1;

	TPI->SPPR = 2;
	TPI->ACPR = 39; // Gia tri cua thanh ghi chia tan so xung bat dong bo
}

/*-----------------------------------------------------------*/

static inline void SWV_SendChar(uint8_t ch){
	/* Trace disabled */
	if((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) == 0UL) return;

	/* ITM disabled */
	if((ITM->TCR & ITM_TCR_ITMENA_Msk) == 0UL) return;

	/* Port 0 disabled */
	if((ITM->TER & 1U) == 0UL) return;

	/* Kiem tra FIFO ready */
	while(ITM->PORT[0].u32 == 0UL); // <- Block tai day trong RTOS context

	ITM->PORT[0].u8 = ch;
}

/*-----------------------------------------------------------*/

int _write(int file, char *ptr, int len){
	(void)file;

	for(int dataIdx = 0; dataIdx < len; dataIdx++){
//		SWV_SendChar((uint8_t)ptr[dataIdx]);
		ITM_SendChar((uint8_t)ptr[dataIdx]);
	}
	return len;
}

/*-----------------------------------------------------------*/

#elif DEBUG_DWT
void DWT_Init(void){
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
#endif // DebugHelper

/*-----------------------------------------------------------*/

#ifdef __cplusplus
}
#endif // __cplusplus
