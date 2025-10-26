/*
 * Sensor_config.c
 *
 *  Created on: Jun 15, 2025
 *  @Author Luong Huu Phuc
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdio.h>
#include <stdarg.h>
#include "cmsis_os.h"
#include "Sensor_config.h"
#include "Logger.h"

// ====== VARIABLES DEFINITION ======
volatile uint32_t global_sample_id = 0;
volatile TickType_t global_timestamp = 0;

// ==== FUNCTION DEFINITIONS ====

void SensorConfig_Init(void){
	SERROR_CHECK(Ad8232_init_ver1(&hadc1));
	uart_printf(">> AD8232 init OK !\r\n");

	SERROR_CHECK(Inmp441_init_ver1(&hi2s2));
	uart_printf(">> INMP441 init OK !\r\n");

	i2c_scanner(&hi2c1);

	SERROR_CHECK(Max30102_init_ver2(&hi2c1)); //Khoi tao cam bien PPG + no interrupt
	uart_printf(">> MAX30102 init OK !\r\n");

	sem_adc = xSemaphoreCreateBinary();
	sem_mic = xSemaphoreCreateBinary();
	sem_max = xSemaphoreCreateBinary();

	StackCheck();

	if(sem_mic == NULL || sem_max == NULL || sem_adc == NULL){
	  uart_printf("[ERROR] Failed to create semaphores !\r\n");
	  Error_Handler();
	}

	//Tao hang doi Logger
	logger_queue = xQueueCreate(LOGGER_QUEUE_LENGTH, sizeof(sensor_block_t));
	if(logger_queue == NULL){
	  uart_printf("[ERROR] Failed to create logger_queue !\r\n");
	  Error_Handler(); //Thay vi dung while(1) -> Dung quy chuan
	}

	HeapCheck(); //Check sau khi tao semaphore va queue
}

// Kiem tra so luong bo nho stack con lai
void __attribute__((unused))StackCheck(void){
	UBaseType_t stackleft = uxTaskGetStackHighWaterMark(NULL);
	uart_printf("[Stack] Stack left: %lu\r\n", (unsigned long)stackleft);
}


// Kiem tra dung luong bo nho heap con lai
void __attribute__((unused))HeapCheck(void){
	size_t heap_free = xPortGetFreeHeapSize(); //Dung luong bo nho con lai
	size_t heap_min_ever = xPortGetMinimumEverFreeHeapSize();

	uart_printf("[HEAP] Free Heap: %u bytes | Minimum Ever Free Heap: %u bytes\r\n",
				(unsigned int)heap_free,
				(unsigned int)heap_min_ever);
}

#ifdef __cplusplus
}
#endif //__cplusplus
