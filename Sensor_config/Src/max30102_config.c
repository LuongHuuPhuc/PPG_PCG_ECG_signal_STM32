/*
 * @file max30102_config.c
 *
 * @date Oct 15, 2025
 * @author LuongHuuPhuc
 */

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "max30102_config.h"
#include "max30102_low_level.h" // max30102_t
#include "max30102_lib.h" 		// max30102_record
#include "string.h"

#include "Sensor_config.h" // Su dung struct `sensor_block_t` va `sensor_type_t`
#include "take_snapsync.h" // Sensor task -> Sync task (ho tro macros & global_sync_snapshot)

#ifdef DEBUG_SWV_ITM
#include "SWV_debug.h"
#endif // DEBUG_SWV_ITM

//====== VARIABLES DEFINITION ======

max30102_t max30102_obj;
max30102_record record;

osSemaphoreId max30102_semId = NULL;
osThreadId max30102_taskId = NULL;

// Extern protocol variables
extern void Logger_i2c_scanner(I2C_HandleTypeDef *hi2c);
extern void uart_printf(const char *fmt,...);

// ====== FUCNTION DEFINITION ======

/*-----------------------------------------------------------*/

HAL_StatusTypeDef Max30102_init(I2C_HandleTypeDef *i2c){
	uart_printf("[MAX30102] initializing...!\r\n");
	HAL_StatusTypeDef ret = HAL_OK;

	osSemaphoreDef(max30102SemaphoreName);
	max30102_semId = osSemaphoreCreate(osSemaphore(max30102SemaphoreName), 1);
	if(max30102_semId == NULL){
		uart_printf("[MAX30102] Failed to create Semaphores !\r\n");
		return HAL_ERROR;
	}

	// Khoi tao cam bien voi cac thong so co ban
	max30102_init(&max30102_obj, i2c);

	// Scan dia chi I2C co tren bus
	Logger_i2c_scanner(i2c);

	for(uint8_t retry = 0; retry < 3; retry++){
		max30102_reset(&max30102_obj);
		if(max30102_clear_fifo(&max30102_obj) == HAL_OK){
			uart_printf("[MAX30102] clear_fifo OK !\r\n");
			break;
		}
		uart_printf("[MAX30102] clear_fifo Failed!\r\n");
		ret |= HAL_ERROR;
	}

	// Sensor config settings
	max30102_set_led_pulse_width(&max30102_obj, max30102_pw_118_us);
	max30102_set_adc_resolution(&max30102_obj, max30102_adc_18_bit);
	max30102_set_sampling_rate(&max30102_obj, max30102_sr_1000); // 1ms/sample => Fixed sample rate
	max30102_set_led_current_ir(&max30102_obj, 12.6f);
	max30102_set_led_current_red(&max30102_obj, 8.0f);
	max30102_set_led_mode(&max30102_obj, &record, max30102_spo2);

	// Interrupt settings (disable)
	max30102_set_a_full(&max30102_obj, 0);
	max30102_set_ppg_rdy(&max30102_obj, 0);

	// FIFO settings (Phai de sau cung khong se bi ghi de)
	max30102_set_fifo_config(&max30102_obj, max30102_smp_ave_1, 1, 0); // Gia tri mong muon: 0x10

	// Kiem tra trang thai thanh ghi cau hinh
	max30102_config_register_status_verbose(&max30102_obj);
	return ret;
}

/*-----------------------------------------------------------*/

void Max30102_task(void const *pvParameter){
	(void)(pvParameter);
	sensor_block_t block;

	uart_printf("MAX30102 task started !\r\n");
	memset(&block, 0, sizeof(sensor_block_t));

	while(1){
		if(osSemaphoreWait(max30102_semId, 100) == osOK){ // Take semaphore tu TIM3 sau du 32 sample (32ms) ung voi 32 counter_max

			/* Doc data tu FIFO roi dua no vao buffer qua I2C */
			uint8_t num_samples = (uint8_t)max30102_read_fifo(&max30102_obj, &record, MAX_FIFO_SAMPLE);

			/* Copy vao block buffer */
			if(num_samples > 0){

#ifdef DEBUG_SWV_ITM
				/* Neu co block nao be hon 31 samples */
				if(num_samples < 31){
					SWV_LOG("[MAX30102] Tick=%lu sid=%lu num_samples=%u\r\n", osKernelSysTick(), global_sync_snapshot.sample_id, num_samples);
				}
#endif // DEBUG_SWV_ITM

				for(uint8_t i = 0; i < num_samples; i++){
					block.ppg.ir[i] = max30102_obj._ir_samples[i];
//					block.ppg.red[i] = max30102_obj._red_samples[i];
				}

				block.type = SENSOR_PPG;
				block.count = num_samples;

#ifdef SYNC_INTERMEDIARY_USING
				block.timestamp = global_sync_snapshot.timestamp;
				block.sample_id = global_sync_snapshot.sample_id; //Danh dau thoi diem -> dong bo

//		        uart_printf("[MAX30102] sending sample_id = %lu\r\n", block.sample_id);

				MAIL_SEND_FROM_TASK_TO_SYNC(block);
#endif // SYNC_INTERMEDIARY_USING

			}
			else uart_printf("[MAX30102] Warining: FIFO is not enoungh 32 samples !\r\n"); // Neu khong du 32 samples 1 lan
		}
		else uart_printf("[MAX30102] Can not take semaphore !\r\n");
	}
}

/*-----------------------------------------------------------*/

#ifdef __cplusplus
}
#endif // __cplusplus
