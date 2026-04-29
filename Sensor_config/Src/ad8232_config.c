/*
 * ad8232_config.h
 *
 *  Created on: Oct 15, 2025
 *      Author: ADMIN
 */

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include "ad8232_config.h"

#include "string.h"

#include "Sensor_config.h" // Su dung struct `sensor_block_t` va `sensor_type_t`
#include "take_snapsync.h" // Sensor task -> Sync task with macros & global_sync_snapshot
#include "Logger.h" // Truong hop su dung MAIL_SEND_FROM_TASK mode Sensor Task -> Logger Task (khong dung sync_task)

// ====== VARIABLES DEFINITION ======

int16_t ecg_buffer[ECG_DMA_BUFFER] = {0};

osSemaphoreId ad8232_semId = NULL;
osThreadId ad8232_taskId = NULL;

// Extern protocol variable cho function trong .c
extern ADC_HandleTypeDef hadc1;
extern void uart_printf(const char *fmt,...); // Logger.h - muon dung ham do thi khai bao extern

// ====== FUCNTION DEFINITION ======
/*-----------------------------------------------------------*/

HAL_StatusTypeDef Ad8232_init(ADC_HandleTypeDef *adc){
	uart_printf("[AD8232] initializing...");
	HAL_StatusTypeDef ret = HAL_OK;
	ret |= ((adc == &hadc1) ? HAL_OK : HAL_ERROR);

	osSemaphoreDef(ad8232SemaphoreName);
	ad8232_semId = osSemaphoreCreate(osSemaphore(ad8232SemaphoreName), 1);
	if(ad8232_semId == NULL){
		uart_printf("[AD8232] Failed to create Semaphores !\r\n");
		return HAL_ERROR;
	}
	return ret;
}
/*-----------------------------------------------------------*/

// === VERSION 3 ===

void Ad8232_task_ver3(void const *pvParameter){
	(void)(pvParameter);
	sensor_block_t block;

	uart_printf("AD8232 task started !\r\n");
	memset(&block, 0, sizeof(sensor_block_t)); //Set ve 0 de tranh byte rac

	// Ghi du lieu tu DMA vao buffer den khi du 32 mau
	// Trigger tu TIMER (sample rate = 1000Hz) moi mau 1ms la 1 lan doc ADC
	if(HAL_ADC_Start_DMA(&hadc1, (uint32_t*)ecg_buffer, ECG_DMA_BUFFER) != HAL_OK) uart_printf("[AD8232] HAL_ADC_Start_DMA failed!\r\n");

	while(1){
		if(osSemaphoreWait(ad8232_semId, 100) == osOK){ // Sau 32ms thi TIMER nha semaphore

			// Take notify khi DMA ghi data vao buffer hoan tat tu ham ConvCpltCallback
			if(ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)) == pdTRUE){

				block.type = SENSOR_ECG;
				block.count = ECG_DMA_BUFFER;

				for(uint8_t i = 0; i < ECG_DMA_BUFFER; i++) block.ecg[i] = ecg_buffer[i]; // Copy gia tri sang bien ecg trong sensor_block_t

#ifdef SYNC_INTERMEDIARY_USING /* Khong dung bien snap local ben ngoai de tranh cap nhat bi cham do phai dung inline function */
				block.timestamp = global_sync_snapshot.timestamp;
				block.sample_id = global_sync_snapshot.sample_id; //Danh dau thoi diem -> dong bo

				MAIL_SEND_FROM_TASK_TO_SYNC(block);
#elif defined(SENSOR_SEND_DIRECT_USING) /* Gui truc tiep */
				MAIL_SEND_FROM_TASK_DIRECT_LOGGER(block);
#endif
			}
			else uart_printf("[AD8232] Timeout waiting for DMA done !\r\n");
		}
		else uart_printf("[AD8232] Can not take semaphore !\r\n");
	}
}

/*-----------------------------------------------------------*/

// === VERSION 2 ===

__attribute__((unused)) void Ad8232_task_ver2(void const *pvParameter){
	(void)(pvParameter);
	sensor_block_t block;

	uart_printf("AD8232 task started !\r\n");
	memset(&block, 0, sizeof(sensor_block_t)); //Set ve 0 de tranh byte rac

	//Ghi du lieu DMA vao ECG_DMA_BUFFER nay den khi du 32 mau, trigger moi mau 1ms (sample rate = 1000Hz) la 1 lan doc ADC
	if(HAL_ADC_Start_DMA(&hadc1, (uint32_t*)ecg_buffer, ECG_DMA_BUFFER) != HAL_OK){
		uart_printf("[AD8232] HAL_ADC_Start_DMA failed!\r\n");
	}

	while(1){
		if(ulTaskNotifyTake(pdTRUE, portMAX_DELAY) == pdTRUE){ //Doi notify DMA hoan tat tu ham ConvCpltCallback

			block.type = SENSOR_ECG;
			block.count = ECG_DMA_BUFFER;

#ifdef SYNC_INTERMEDIARY_USING
			block.timestamp = global_sync_snapshot.timestamp;
			block.sample_id = global_sync_snapshot.sample_id; //Danh dau thoi diem -> dong bo
#endif // SYNC_INTERMEDIARY_USING

			for(uint8_t i = 0; i < ECG_DMA_BUFFER; i++) block.ecg[i] = ecg_buffer[i]; //Copy gia tri sang bien ecg trong sensor_block_t

#ifdef SYNC_INTERMEDIARY_USING
				MAIL_SEND_FROM_TASK_TO_SYNC(block); // Gui 32 sample vao den task tiep theo
#elif defined(SENSOR_SEND_DIRECT_USING)
				MAIL_SEND_FROM_TASK_DIRECT_LOGGER(block);
#endif
		}
		else uart_printf("[ADC8232] Timeout waiting for DMA done !\r\n");
	}
}

/*-----------------------------------------------------------*/

// ===== VERSION 1 =====

__attribute__((unused)) void Ad8232_task_ver1(void const *pvParameter){
	(void)(pvParameter);
	__attribute__((unused)) sensor_block_t block;
	uart_printf("AD8232 task started !\r\n");

	while(1){
		if(osSemaphoreWait(ad8232_semId, osWaitForever) == osOK){ //Trigger tu ham Callback de lay semaphore
			HAL_ADC_Start(&hadc1);

			if(HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK){
				block.type = SENSOR_ECG;
//				block.ecg = (int16_t)HAL_ADC_GetValue(&hadc1);

#ifdef SYNC_INTERMEDIARY_USING
				MAIL_SEND_FROM_TASK_TO_SYNC(block); // Gui 32 sample vao den task tiep theo
#elif defined(SENSOR_SEND_DIRECT_USING)
				MAIL_SEND_FROM_TASK_DIRECT_LOGGER(block);
#endif
			}
			HAL_ADC_Stop(&hadc1);
		}
	}
}
/*-----------------------------------------------------------*/

/**
 * @brief Ham de chuyen doi giua don vi ADC (digital) sang don vi dien ap mV (Analog)j
 *
 * @remark Phan biet ro 2 loai dien ap
 * 		   - Dien ap tai chan OUT (sau khuech dai): Dao dong quanh REFOUT ~ Vref / 2. Day la dien ap ADC dang do
 * 		   - Dien ap ECG that (o dien cuc): la tin hieu da chia nguoc gain, tru di gia tri vitural ground, tin hieu giao dong quanh 0V, don vi mV (hoac uV)
 *  Cong thuc tinh ADC -> Dien ap Out (mV): Vout(mV) = ADC_value x Vref / (so muc ADC: 2^N - 1)
 *  VD: STM32 co ADC 12-bit, Vref = 3.3V => Vout (mV) = ADC_value x 3300 / 4095 (Dien ap dau ra cua chan OUTPUT sau khi duoc khuech dai)
 *  Voi ADC_value = 2200 => Vout ~ 1773 mV
 *
 *  - De tinh gia tri ECG thuc te, tru di REFOUT (de dua ve tinh hieu AC) vi OUT dau ra giao dong quanh REFOUT ~ Vref / 2
 *   + Vac_out (mV) = Vout - (Vref / 2) = 1773 - 1650 = 123mV (De tin hieu quay quanh truc 0V)\
 *   + Chia gain tong de ra ECG that: V_ecg(mV) = Vac_out / Gain_total = 123 / 1100 = 0.112mV = 112uV
 *
 * @param input_adc_value Gia tri ADC dau vao
 * @param gain He so khuech dai cua phan cung (100 - 1100)
 *
 * @retval Gia tri dien ap theo don vi mV
 * 		   - Gain = 100 -> Gia tri dau ra la dien ap da khuech dai
 * 		   - Gain != 100 -> Gia tri dien ap that cua ECG
 */
__attribute__((unused)) static inline float AD8323_adc_to_mv(volatile int16_t input_adc_value, float gain){
	float v_adc_mv = ((input_adc_value * ADC_REF_VOL * 1000.0f) / ADC_VOL_LEVEL_12B) - VIRTUAL_GND_VOL; // Tru dien dien ap GND ao de dich truc ve 0V
	return (v_adc_mv / gain);
}

/*-----------------------------------------------------------*/

/**
 * @brief Ham conversion callback
 *
 * @note Khi buffer DMA hoan tat thi ham nay se duoc goi
 * Khi DMA day (du 32 sample ~ 32ms) ham nay se duoc goi
 * \note `vTaskNotifyGiveFromISR()` se duoc truyen con tro cua ham task vao de callback moi lan
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *adc){
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	if(adc == &hadc1){
		// Gui thong bao cho task khi DMA ghi vao buffer hoan tat
		vTaskNotifyGiveFromISR(ad8232_taskId, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

	}
}
/*-----------------------------------------------------------*/

#ifdef __cplusplus
}
#endif
