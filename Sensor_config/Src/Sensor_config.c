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
#include "stdarg.h"

// ====== FUNCTION DEFINITIONS ======
/*-----------------------------------------------------------*/

extern HAL_StatusTypeDef Ad8232_init(ADC_HandleTypeDef *adc);
extern HAL_StatusTypeDef Inmp441_init(I2S_HandleTypeDef *i2s);
extern HAL_StatusTypeDef Max30102_init_ver2(I2C_HandleTypeDef *i2c);

void SensorConfig_Init(void){
	SERROR_CHECK(Ad8232_init(&hadc1));
	uart_printf(">> [SENCONF] AD8232 init OK !\r\n");

	SERROR_CHECK(Inmp441_init(&hi2s2));
	uart_printf(">> [SENCONF] INMP441 init OK !\r\n");

	SERROR_CHECK(Max30102_init_ver2(&hi2c1)); //Khoi tao cam bien PPG + no interrupt
	uart_printf(">> [SENCONF] MAX30102 init OK !\r\n");

	//Check sau khi tao semaphore va queue
	HeapCheck();
	StackCheck();
}
/*-----------------------------------------------------------*/

// Kiem tra so luong bo nho stack con lai
void __attribute__((unused)) StackCheck(void){
	UBaseType_t stackleft = uxTaskGetStackHighWaterMark(NULL);
	uart_printf("[SENCONF] Stack left: %lu\r\n", (unsigned long)stackleft);
}

/*-----------------------------------------------------------*/

// Kiem tra dung luong bo nho heap con lai
void __attribute__((unused)) HeapCheck(void){
	size_t heap_free = xPortGetFreeHeapSize(); //Dung luong bo nho con lai
	size_t heap_min_ever = xPortGetMinimumEverFreeHeapSize();

	uart_printf("[SENCONF] Free Heap: %u bytes | Minimum Ever Free Heap: %u bytes\r\n",
				(unsigned int)heap_free,
				(unsigned int)heap_min_ever);
}
/*-----------------------------------------------------------*/

/**
 * Ham callback nay se duoc goi de canh bao phat hien StackOverFlow
 */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName){
	taskENTER_CRITICAL();
	uart_printf(" [SENCONF] Stack overflow in %s!\r\n", pcTaskName);
	Error_Handler();
	taskEXIT_CRITICAL();
}

/*-----------------------------------------------------------*/

/**
 * @brief Ham callback nay se duoc goi de canh bao phat hien loi cap phat dong
 * Callback nay se duoc goi neu pvPortMalloc() tra ve NULL
 * 	- Heap khong du lon
 *  - Heap bi phan manh
 *  - Heap size qua nho
 *
 * @warning Neu Heap rat nho(< 200 bytes) -> Heap sizing sai
 * 			Neu Heap lon ma van fail -> Heap bi phan manh
 */
void vApplicationMallocFailedHook(void){
	taskENABLE_INTERRUPTS(); // Ngan context switch
	/* Neu UART con song thi in ra debug */
	uart_printf("[SENCONF] Malloc failed ! Heap exhausted or fragmented !\r\n");

	size_t heap_free = xPortGetFreeHeapSize(); //Dung luong bo nho con lai
	size_t heap_min_ever = xPortGetMinimumEverFreeHeapSize();

	uart_printf("[SENCONF] Free Heap: %u bytes | Minimum Ever Free Heap: %u bytes\r\n",
				(unsigned int)heap_free,
				(unsigned int)heap_min_ever);
	Error_Handler();
}

/*-----------------------------------------------------------*/

#ifdef __cplusplus
}
#endif //__cplusplus
