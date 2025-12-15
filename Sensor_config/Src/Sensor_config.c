/*
 * Sensor_config.c
 *
 *  Created on: Jun 15, 2025
 *  @Author Luong Huu Phuc
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "Sensor_config.h"
#include "main.h"
#include <stdio.h>
#include <stdarg.h>
#include "cmsis_os.h"
#include "Logger.h"

// ====== VARIABLES DEFINITION ======
#if defined(sensor_config_SYNC_USING)

__attribute__((unused))volatile uint32_t global_sample_id = 0;
__attribute__((unused))volatile TickType_t global_timestamp = 0;

#endif // sensor_config_SYNC_USING

// ==== FUNCTION DEFINITIONS ====
/*-----------------------------------------------------------*/

void SensorConfig_Init(void){
	SERROR_CHECK(Ad8232_init(&hadc1));
	uart_printf(">> AD8232 init OK !\r\n");

	SERROR_CHECK(Inmp441_init(&hi2s2));
	uart_printf(">> INMP441 init OK !\r\n");

	SERROR_CHECK(Max30102_init_ver2(&hi2c1)); //Khoi tao cam bien PPG + no interrupt
	uart_printf(">> MAX30102 init OK !\r\n");

#ifdef USING_UART_DMAPHORE
	SERROR_CHECK(Logger_init_ver2());
	uart_printf(">> LOGGER init OK !\r\n");
#else // USING_UART_DMAPHORE

	SERROR_CHECK(Logger_init());
	uart_printf(">> LOGGER init OK !\r\n");
#endif

	//Check sau khi tao semaphore va queue
	HeapCheck();
	StackCheck();
}
/*-----------------------------------------------------------*/

// Kiem tra so luong bo nho stack con lai
void __attribute__((unused))StackCheck(void){
	UBaseType_t stackleft = uxTaskGetStackHighWaterMark(NULL);
	uart_printf("[STACK] Stack left: %lu\r\n", (unsigned long)stackleft);
}

/*-----------------------------------------------------------*/

// Kiem tra dung luong bo nho heap con lai
void __attribute__((unused))HeapCheck(void){
	size_t heap_free = xPortGetFreeHeapSize(); //Dung luong bo nho con lai
	size_t heap_min_ever = xPortGetMinimumEverFreeHeapSize();

	uart_printf("[HEAP] Free Heap: %u bytes | Minimum Ever Free Heap: %u bytes\r\n",
				(unsigned int)heap_free,
				(unsigned int)heap_min_ever);
}
/*-----------------------------------------------------------*/

/**
 * Ham callback nay se duoc goi de canh bao phat hien StackOverFlow
 */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName){
	taskENTER_CRITICAL();

	uart_printf_fromISR("Stack overflow in %s!\r\n", pcTaskName);
	Error_Handler();

	taskEXIT_CRITICAL();
}

#ifdef __cplusplus
}
#endif //__cplusplus
