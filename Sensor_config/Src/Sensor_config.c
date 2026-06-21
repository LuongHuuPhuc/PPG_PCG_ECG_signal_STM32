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
#include "dispatch_target_def.h"

// ====== FUNCTION DEFINITIONS ======
/*-----------------------------------------------------------*/

extern I2C_HandleTypeDef hi2c1;
extern ADC_HandleTypeDef hadc1;
extern I2S_HandleTypeDef hi2s2;

extern HAL_StatusTypeDef Ad8232_init(ADC_HandleTypeDef *adc);
extern HAL_StatusTypeDef Inmp441_init(I2S_HandleTypeDef *i2s);
extern HAL_StatusTypeDef Max30102_init(I2C_HandleTypeDef *i2c);

void SensorConfig_init(void){
	SERROR_CHECK(Ad8232_init(&hadc1));
	uart_printf(">> [SENCONF] AD8232 init OK !\r\n");

	SERROR_CHECK(Inmp441_init(&hi2s2));
	uart_printf(">> [SENCONF] INMP441 init OK !\r\n");

	SERROR_CHECK(Max30102_init(&hi2c1)); //Khoi tao cam bien PPG + no interrupt
	uart_printf(">> [SENCONF] MAX30102 init OK !\r\n");

	//Check sau khi tao semaphore va queue
	HeapCheck();
	StackCheck();
}

/*-----------------------------------------------------------*/

extern void DataDispatcher_Init(DispatchTypeU32_t target);
extern HAL_StatusTypeDef Logger_init(void);
extern HAL_StatusTypeDef MicroSD_init(void);
extern HAL_StatusTypeDef ExtButton_init(void);

void SensorOutput_init(void){
#ifdef SENSOR_BINARY_PACKET /* Neu dung UART de gui binary packet thi khong dung Logger nua */
  /* Dang ky callback */
  DataDispatcher_Init(DISPATCH_PACKET);

#elif defined(SENSOR_LOGGER_USING) && !defined(SENSOR_BINARY_PACKET) /* Neu dung Logger thi khong dung Binary packet */
  /* Dang ky callback */
  DataDispatcher_Init(DISPATCH_UART);

  SERROR_CHECK(Logger_init());
  uart_printf(">> [SENCONF] LOGGER init OK !\r\n");
#endif // SENSOR_BINARY_PACKET

#ifdef SENSOR_SD_CARD_USING
  /* Dang ky callback */
  DataDispatcher_Init(DISPATCH_SD);

  SERROR_CHECK(MicroSD_init());
  uart_printf("[SENCONF] MicroSD init OK !\r\n");

  SERROR_CHECK(ExtButton_init());
  uart_printf("[SENCONF] ExtButton init OK !\r\n");
#endif // SENSOR_SD_CARD_USING

  uart_printf(">> [SENCONF] Dispatcher init OK !\r\n");
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

#ifdef QUEUE_FREE_CHECK
void __attribute__((unused)) QueueCheck(const QueueHandle_t queue, const char *name){
	UBaseType_t used = uxQueueMessagesWaiting(queue);
	UBaseType_t free = uxQueueSpacesAvailable(queue);

	if(free == 0){
		uart_printf("[ERROR] Queue %s is FULL ! Used: %lu\r\n", name, used);
	}else if(free < 5){ // Canh bao khi queue sap day
		uart_printf("[WARN] Queue %s almost FULL ! Used: %lu, Free: %lu", name, used, free);
	}
}
#endif // QUEUE_FREE_CHECK

/*-----------------------------------------------------------*/

#ifdef __cplusplus
}
#endif //__cplusplus
