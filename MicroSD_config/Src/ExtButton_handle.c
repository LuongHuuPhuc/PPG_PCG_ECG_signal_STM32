/*
 * @file ExtButton_handle.c
 *
 * @date Jun 20, 2026
 * @author LuongHuuPhuc
 */
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "ExtButton_handle.h"
#include "string.h"

#include "MicroSD_config.h"

#ifdef DEBUG_SWV_ITM
#include "SWV_debug.h"
#endif // DEBUG_SWV_ITM

/* Khoi tao bien co toan cuc he thong */
volatile bool g_sd_backup_allow_flag = false;

static osThreadId extButton_taskId = NULL;

extern void uart_printf(const char *fmt,...);

HAL_StatusTypeDef ExtButton_init(void){
	if(extButton_taskId != NULL) return HAL_OK;

	/* Khai bao va cau hinh luong FreeRTOS */
	osThreadDef(extButtonTaskName, ExtButton_process_task, osPriorityNormal, 0, 256);
	extButton_taskId = osThreadCreate(osThread(extButtonTaskName), NULL);
	configASSERT(extButton_taskId);

	return HAL_OK;
}

/*-----------------------------------------------------------*/

void ExtButton_process_task(void const *pvParameter){
	(void)pvParameter;

	uint32_t press_start_tick = 0;
	bool is_pressing = false;

	uart_printf("ExtButton Task started !");
	while(1){
		/* Doc trang thai vat ly chan PA10 (Active-Low: Nhan nut = RESET/0V) */
		if(HAL_GPIO_ReadPin(BUTTON_GPIO_Port, BUTTON_Pin) == GPIO_PIN_RESET){
			if(!is_pressing){
				// Nhip cham phim dau tien: Ghi nhan goc thoi gian tai phan cung
				is_pressing = true;
				press_start_tick = osKernelSysTick();
			}
			else{
				// Neu he thong chua ghi SD -> Kiem tra du 3s de bat che do ghi vao SD Card
				if(!g_sd_backup_allow_flag){
					if((osKernelSysTick() - press_start_tick) >= BUTTON_HOLD_DURATION_MS){
						/* Kich no trang thai cho phep ghi SD card */
						g_sd_backup_allow_flag = true;

#ifdef DEBUG_SWV_ITM
						SWV_LOG("[EXT_BUTTON] Held 3s ! Start write data to SD Card...");
#endif // DEBUG_SWV_ITM

						/* Wait USER buong tay de khong bi dinh vao logic tat */
						while(HAL_GPIO_ReadPin(BUTTON_GPIO_Port, BUTTON_Pin) == GPIO_PIN_RESET) osDelay(50);
						is_pressing = false;
					}
				}
			}
		}
		else{
			/* Quay lai trang thai gui data qua UART neu nhan lai */
			if(is_pressing){
				uint32_t press_duration = osKernelSysTick() - press_start_tick;

				/* Neu he thong dang ghi SD Card ma USER lai bam nha ngan (Click < 1.5s) */
				if(g_sd_backup_allow_flag && (press_duration < 1500U) && (press_duration > 40)){

					/* Tat ghi SD Card - Quay lai UART */
					g_sd_backup_allow_flag = false;
					MicroSD_Flush(); // Day not du lieu con ton du trong Sector Buffer xuong Flash

#ifdef DEBUG_SWV_ITM
						SWV_LOG("[EXT_BUTTON] Stopped writing to SD. Changing to UART...");
#endif // DEBUG_SWV_ITM

				}
				is_pressing = false;
			}
		}
		/* Chu ky lay mau 20ms */
		osDelay(20);
	}
}

/*-----------------------------------------------------------*/

#ifdef __cplusplus
}
#endif // __cplusplus

