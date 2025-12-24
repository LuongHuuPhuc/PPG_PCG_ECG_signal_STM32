/*
 * Logger.h
 *
 *  Created on: Jun 17, 2025
 *  @Author Luong Huu Phuc
 */

#ifndef INC_LOGGER_H_
#define INC_LOGGER_H_

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "stdio.h"
#include "stdbool.h"
#include "stdarg.h"
#include "take_snapsync.h" // Lay struct block tu Sync de in ra man hinh
#include "Sensor_config.h"

//Kiem tra trang thai du lieu da san sang hay chua
typedef struct {
	bool ecg_ready;
	bool max_ready;
	bool mic_ready;
} __attribute__((unused)) sensor_check_t;

// Task & RTOS
#ifdef USING_UART_DMAPHORE

#if defined(FREERTOS_API_USING)
extern SemaphoreHandle_t loggerDMA_sem; // Binary de bao trang thai DMA cua UART (tranh ghi de buffer)
#elif defined(CMSIS_API_USING)
extern osSemaphoreId loggerDMA_semId;
#endif // CMSIS_API_USING

#endif // USING_UART_DMAPHORE

// Extern variables
extern UART_HandleTypeDef huart2; //Duoc dinh nghia cho khac, chi muon dung o day thoi ti dung extern !

#if defined(CMSIS_API_USING)

// CMSIS dung con tro de lay data, khong copy truc tiep data nhu FREERTOS API (Hieu nang + toc do cap hon)
// Data se nam yen trong RAM -> Tre thap hon
// Dung Mail vi Mail moi truyen duoc struct, Message thi khong
extern osMailQId logger_queueId; // Sync Task -> Logger task
extern osMutexId logger_mutexId; // Mutex cho UART printf
extern osThreadId logger_taskId; // Task Logger

#elif defined(FREERTOS_API_USING)

extern SemaphoreHandle_t logger_mutex; // Mutex de bao ve thao tac UART (UART khong bi tranh chap, chi 1 task dung UART)
extern QueueHandle_t logger_queue;
extern TaskHandle_t logger_task;

#endif // CMSIS_API_USING

// ==== MACROS　====
#define MIN(a, b) 				 ((a) < (b) ? (a) : (b))
#define MAX_COUNT 				 32
#define MAX_RETRY_SCANNER 		 2
#define LOGGER_QUEUE_LENGTH 	 40 // Tang chieu dai queue de chong tran
#define UART_TX_BUFFER_SIZE      1024

#if defined(FREERTOS_API_USING)

#define QUEUE_SEND_FROM_TASK(data_ptr) do { \
	if(xQueueSend(logger_queue, (data_ptr), portMAX_DELAY) != pdTRUE){ \
		uart_printf("[LOGGER] Logger queue full!\r\n"); \
	} \
} while(0)

#define QUEUE_SEND_FROM_ISR(data_ptr) do { \
	if(xQueueSendFromISR(logger_queue, (data_ptr), pxHigherPriorityTaskWoken) != pdTRUE){\
		uart_printf("[LOGGER] Logger queue full from ISR! \r\n "); \
	} \
} while(0) // Dung khi muon send queue trong cac ham HAL_callback

#elif defined(CMSIS_API_USING)

#ifdef SYNC_TO_LOGGER_MAIL_USING

static inline osStatus logger_mail_send(sensor_sync_block_t *block){
	sensor_sync_block_t *mail = osMailAlloc(logger_queueId, 0); // Cap phat dong
	if(mail == NULL){
		return osErrorNoMemory;
	}

	*mail = *block; // Copy
	osStatus ret = osMailPut(logger_queueId, mail);
	if(ret != osOK){
		uart_printf("[LOGGER] Logger queue full!\r\n");
		osMailFree(logger_queueId, mail);  /* Tranh memory leak neu khong gui duoc Mail */
	}
	return ret;

}

/* Macro de Sync Task gui den Logger Task */
#define MAIL_SEND_FROM_SYNC(block) do { \
	if(logger_mail_send(&(block)) != osOK){ \
		/* Neu Logger UART xu ly cham hoac tran heap thi se bi di vao day */ \
		uart_printf("[LOGGER] Mail sent from SYNC error !\r\n"); \
	} \
} while(0)

#else

static inline osStatus logger_mail_send(sensor_block_t *block){
	sensor_block_t *mail = osMailAlloc(logger_queueId, 0); // Cap phat dong
	if(mail == NULL){
		return osErrorNoMemory;
	}

	*mail = *block; // Copy
	osStatus ret = osMailPut(logger_queueId, mail);
	if(ret != osOK){
		uart_printf("[LOGGER] Logger queue full!\r\n");
		osMailFree(logger_queueId, mail);  /* Tranh memory leak neu khong gui duoc Mail */
	}
	return ret;
}

/* Macro de Sensor Task gui truc tiep den Logger Task */
#define MAIL_SEND_FROM_TASK(block) do { \
	logger_mail_send(&(block)); \
} while(0)

#endif // SYNC_TO_LOGGER_MAIL_USING
#endif // CMSIS_API_USING

// ==== FUNCTION PROTOTYPE ====

/**
 * @brief Ham scan cac bus I2C de xem co dang ton tai dia chi nao khong
 *
 * @param hi2c Con tro den struct cau hinh I2C
 */
void Logger_i2c_scanner(I2C_HandleTypeDef *hi2c);

/**
 * @brief Ham khoi tao Queue de log du lieu theo hang doi.
 *
 * @note Do UART tai nguyen chung nen su dung Semaphore Mutex de uu tien 1 thread UART Logger chay
 * \note - Co che Lock/Unlock (Quyen so huu): chi 1 thread lock va unlock duoc no
 * \note - Co che Priority Inheritane (Uu tien): Chi 1 thread chay duoc trong 1 thoi diem
 * \note - Mutex khong dung duoc trong ISR
 * \note - Mutex tuong duong voi 1 Binary Semaphore
 */
HAL_StatusTypeDef Logger_init(void);

#ifdef SYNC_TO_LOGGER_MAIL_USING
/**
 * @brief Ham in ra 3 kenh du lieu PPG PCG ECG cung luc
 * Sau khi nhan Mail tu task Sync dong bo (giam ganh nang CPU cho Logger)
 * Thi moi in ra man hinh thong qua UART (chap nhan tre in)
 */
void Logger_three_task_ver2(void const *pvParameter);
#endif // SYNC_TO_LOGGER_MAIL_USING

/**
 * @brief Ham in ra 3 kenh du lieu PPG PCG ECG cung luc
 */
__attribute__((unused)) void Logger_three_task_ver1(void const *pvParameter); // Ham nhan data tu queue theo block 32 samples/lan

/**
 * @brief Ham in ra 2 kenh du lieu dong thoi
 */
__attribute__((unused)) void Logger_two_task(void const *pvParameter);

/**
 * @brief Ham in ra 1 kenh du lieu
 */
__attribute__((unused)) void Logger_one_task(void const *pvParameter); //Ham debug log de xem thuc te co bao nhieu sample moi task

#ifdef FREERTOS_API_USING
/**
 * @brief Ham kiem tra xem hang doi con free khong
 * @note Neu khong con trong -> Chuong trinh se dung lai
 */
void isQueueFree(const QueueHandle_t queue, const char *name);
#endif //CMSIS_API_USING

/**
* @brief Ham thay the cho printf su dung UART
*
* @note Su dung truyen UART theo Blocking Mode + Mutex de bao ve UART
*
* @warning Khong duoc goi ham nay ben trong cac ham ISR (Interrupt Service Routine) nhu may ham callback
* vi Semaphore o day khong co hau to `FromISR`
* => Nhu the no se duoc xem la ham non-safe ISR
* => Co nguy co gay block task
*
* Cac ham Non-ISR safe duoc thiet ke de goi tu Task Context (ngu canh cua 1 task thong thuong)
* boi vi cac ham nay thuong goi ngam cac ham nhu `taskENTER_CRITICAL()` hoac `vPortEnterCritical()`
* de bao ve du lieu Kernel khoi bi task khac gian doan
*
* Khi goi tu task context, cac ham do se hoat dong bang cach vo hieu hoa tat cac Interrurpt (co uu tien thap hon nguong an toan)
* bang cach goi cac ham Critical Section khong an toan. Neu goi no tu ISR, no se kich hoat loi Assert
* vi no vi pham quy tac: Khong duoc phep vo hieu hoa ngat khi CPU dang phuc vu 1 ngat
*/
__attribute__((weak)) void uart_printf(const char *fmt,...);

/**
 * @brief Ham dung de log debug neu muon su dung tu ISR
 *
 * @remark Ham nay tao ra de tranh viec log debug nhung lai bi loi Assert tu FreeRTOS
 * khi goi cac API function FreeRTOS ben trong cac ham ISR
 *
 * @note Assert (Khang dinh) la loi nghiem trong xay ra khi Kernel FreeRTOS phat hien ra rang
 * cac quy tac cot loi bi vi pham, dac biet la cac quy tac an toan ve ngat (Interrupt Safety)
 * Loi nay thuong duoc kich hoat bang macro `configASSERT()`. Khi do toan bo he thong se bi dung lai
 * FreeRTOS phai dam bao rang cac ham cot loi cua Kernel (nhu context switch, quan ly Semaphore/Queue,...)
 * khong bao gio bi gian doan boi 1 Interrupt co the goi lai chinh cac ham do 1 cach khong an toan
 *
 * @attention Cac API FreeRTOS phai duoc goi tu cac Ngat (ISR) co quyen uu tien thap hon mot nguong an toan
 * do ban thiet lap (`configMAX_SYSCALL_INTERRUPT_PRIORITY`).
 *
 * \attention Khong duoc goi bat ky ham API FreeRTOS an toan ngat (FromISR) tu 1 ISR co quyen uu tien cao hon nguong toi
 * da an toan cua Kernel (vi du nhu cac ham can phan hoi nhanh nhu Hard Fault, WatchDog hoac Timing tuyet doi..)
 * Thu tu uu tien:
 * 1. Cac ngat can phan hoi nhanh (Hard Fault, WatchDog,...) -> Cao hon nguong an toan -> Khong duoc goi bat ky API nao
 * 2. Cac ngat phan cung thong thuong (DMA, UART, Timer, I2S,...) Thap hon nguong an toan -> Chi duoc goi cac API FreeRTOS an toan (`FromISR`)
 * 3. Kernel Core (Systick, PendSV) -> Priority thap nhat. Day la noi Scheduler chay. Kernel phai co uu tien thap nhat
 * de dam bao tat ca ca Interrupt khac (bao gom ca DMA/UART,...) deu co the chay xong truoc khi Kernel thuc hien context switch
 *
 * \attention Muc dich la de lam ranh gioi giua ngat "Uu tien cao/Khong an toan" va "Uu tien thap/an toan"
 *
 * @example Vi du nhu ban goi 1 ham hay API (...FromISR) hay non-ISR safe tu ben trong ISR co uu tien (Preempty Priority)
 * cao hon nguong an toan cua Kernel, dieu nay kich hoat mot Assert nghiem trong cua Kernel, lam dong bang toan bo he thong
 * Day la co che phong thu de ngan ngua viec Kernel bi hong neu du lieu Ngat do co gang Ngat cac thao tac nhay cam
 *
 * => Vi the ISR cua ban phai co Preempty Priority thap hon nguong an toan de FreeRTOS co the quan ly no 1 cach tin cay
 * => Tom lai phai dam bao rang cac ISR cua ban luon luon nam trong vung uu tien thap de chung khong lam gian doan co che
 * bao ve cua Kernel khi chung goi cac API RTOS
 */
__attribute__((weak)) void uart_printf_safe(const char *fmt,...);

/* FIXME Doan code nay dang can duoc fix loi de su dung UART theo DMA (Hien tai chua dung duoc) */
#ifdef USING_UART_DMAPHORE
/**
 * @brief Ham khoi tao Queue de log du lieu theo hang doi.
 * Su dung DMA UART de tang toc do doc (non-blocking mode tranh chan CPU) thay cho UART thong thuong (blocking mode)
 *
 * @note Do UART tai nguyen chung nen su dung Semaphore Mutex de uu tien 1 thread UART Logger chay
 * \note - Co che Lock/Unlock (Quyen so huu): chi 1 thread lock va unlock duoc no
 * \note - Co che Priority Inheritane (Uu tien): Chi 1 thread chay duoc trong 1 thoi diem
 * \note - Mutex khong dung duoc trong ISR
 * \note - Mutex tuong duong voi 1 Binary Semaphore
 */
HAL_StatusTypeDef Logger_init_ver2(void);

/**
 * @brief Ham in ra 2 kenh du lieu dong thoi su dung DMA + Semaphore (Mutex, Binary)
 * @warning Loi khong chay duoc data nhu y muon (cac gia tri lap lai) => Khong dung duoc
 */
__attribute__((unused)) void Logger_two_task_dmaphore(void const *pvParameter);

/**
 * @brief Ham thay the cho printf su dung UART
 * @note Su dung DMA + Semaphore Mutex de quan ly tai nguyen UART tranh du lieu buffer bi ghi de
 */
__attribute__((weak, unused)) void uart_printf_dmaphore(const char *buffer, uint16_t buflen);

#endif // USING_UART_DMAPHORE

#ifdef __cplusplus
}
#endif //__cplusplus

#endif /* INC_LOGGER_H_ */
