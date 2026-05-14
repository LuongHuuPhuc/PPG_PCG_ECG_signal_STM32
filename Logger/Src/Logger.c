/*
 * @file Logger.c
 *
 *  Created on: Jun 17, 2025
 *  @Author Luong Huu Phuc
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "Logger.h"

#include "string.h"
#include "stdarg.h" // va_list
#include "limits.h" // INT_MAX

#include "take_snapsync.h" // Lay struct `sensor_sync_block_t`
#include "uart_dma_cfg.h"	// Su dung UART qua DMA de log

static osMailQId logger_queueId = NULL;
osThreadId logger_taskId = NULL;

// Extern variables
extern UART_HandleTypeDef huart2;

/*-----------------------------------------------------------*/

HAL_StatusTypeDef Logger_init(void){
	HAL_StatusTypeDef ret = HAL_OK;

	/* Khoi tao tai nguyen cho UART DMA dau tien ! */
	ret = uart_dma_init(&huart2);
	if(ret != HAL_OK) ret |= HAL_ERROR;

#ifdef SYNC_INTERMEDIARY_USING
	// Tao Queue cho Logger de nhan block tu Sync
	osMailQDef(loggerQueueName, LOGGER_QUEUE_LENGTH, sensor_sync_block_t);
	logger_queueId = osMailCreate(osMailQ(loggerQueueName), NULL);
	if(logger_queueId == NULL){
		uart_printf("[LOGGER] Failed to create logger_queueId !\r\n");
		ret |= HAL_ERROR;
	}

#elif SENSOR_SEND_DIRECT_USING
	// Tao Queue cho Logger de nhan block truc tiep tu Sensor task
	osMailQDef(loggerQueueName, LOGGER_QUEUE_LENGTH, sensor_block_t);
	logger_queueId = osMailCreate(osMailQ(loggerQueueName), NULL);
	if(logger_queueId == NULL){
		uart_printf("[LOGGER] Failed to create logger_queueId !\r\n");
		ret |= HAL_ERROR;
	}
#else
	/* Do nothing */
#endif // LOGGER_MAIL_USING

	return ret;
}

/*-----------------------------------------------------------*/

void Logger_i2c_scanner(I2C_HandleTypeDef *hi2c){
	uart_printf("[LOGGER] Starting I2C Scanner...\r\n");

	for(uint8_t address = 0x08; address <= 0x77; address++){
		HAL_StatusTypeDef result = HAL_I2C_IsDeviceReady(hi2c, address << 1, MAX_RETRY_SCANNER, 50); //Dich 7-bit to 8-bit addr

		// Ham tren se di qua tung dia chi mot de xem xem co dia chi nao tra ve HAL_OK khong
		// Moi vong for gia tri cua result se thay doi lien tuc tuy theo address -> Nen dat lam bien cuc bo
		// De result thanh global thi gia tri cua no se bi ghi de lien tuc -> Anh huong neu co nhieu task goi ham nay
		if(result == HAL_OK){
			uart_printf("[LOGGER] I2C device found at address: 0x%02X\r\n", address); // Chi luu gia tri address phat hien duoc
		}
	}
	uart_printf("[LOGGER] I2C Scanner complete !\r\n");
}

/*-----------------------------------------------------------*/

#ifdef QUEUE_FREE_CHECK
void isQueueFree(const QueueHandle_t queue, const char *name){
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

/* Check neu ham co duoc goi trong ISR (return true -> Handler Mode (dang trong ISR))*/
static inline bool in_isr(void){
	return (__get_IPSR() != 0U);
}

/*-----------------------------------------------------------*/

void uart_printf(const char *fmt,...){
	// Buffer local
	char uart_buf[UART_PRINTF_BUFFER_SIZE];

	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(uart_buf, sizeof(uart_buf), fmt, args);
	va_end(args);

	if(len <= 0) return;

	// Clamp len ve dung so byte thuc co trong buffer
	if(len >= (int)sizeof(uart_buf)) len = (int)sizeof(uart_buf) - 1; // vi vnsprintf da dam bao co '\0`

	/* RTOS chua chay -> dung blocking UART thay the */
	if(osKernelRunning() == 0){
		if(uart_tx_blocking((uint8_t*)uart_buf, (uint16_t)len) != HAL_OK) return;
		return;
	}
	else if(in_isr()){ return; /* Drop va khong log (han che toi da) de ISR nhanh nhat co the */}
	else{
		/* RTOS chay -> chuyen hoan toan qua non-blocking UART de giam tai CPU */
		uart_tx_dma((uint8_t*)uart_buf, (uint16_t)len);
	}
}

/*-----------------------------------------------------------*/

#ifdef SYNC_INTERMEDIARY_USING /* NOTE: Ham cho viec nhan block tu Sync task -> Logger task nay (trung gian) va in ra man hinh qua UART */
static inline osStatus Logger_mail_send(sensor_sync_block_t *block){
	sensor_sync_block_t *mail = osMailAlloc(logger_queueId, 0); // Cap phat dong
	if(mail == NULL){
		/* Kha nang bi Bottleneck do UART printf cham */
		uart_printf("[LOGGER] Logger queue memory alloc failed!\r\n");
		return osErrorNoMemory;
	}

	*mail = *block; // Copy
	osStatus ret = osMailPut(logger_queueId, mail);
	if(ret != osOK){
		uart_printf("[LOGGER] Logger queue full!\r\n");
		osMailFree(logger_queueId, mail);  /* Tranh memory leak neu khong gui duoc Mail */
	}

	/* Chi free sau khi block duoc xu ly xong tai Logger task */
	return ret;
}

/*-----------------------------------------------------------*/

void Logger_dispatch(sensor_sync_block_t *block){
	if(Logger_mail_send(block) != osOK){
		/* Neu Logger UART xu ly cham hoac tran heap (kha nang cao se bi cai nay) thi se bi di vao day */
		uart_printf("[LOGGER] Mail sent from SYNC error !\r\n");
	}
}

/*-----------------------------------------------------------*/

/*
 * itoa() co toc do nhanh hon snprintf do snprintf can phai format
 * va phan tich chuoi dinh dang tai thoi diem runtime gay ra chi phi cao
 * con itoa() chi thuc hien 1 nhiem vu duy nhat la chuyen so nguyen thanh
 * chuoi
 * 	-> Log stream data dung cai nay thay cho uart_printf
 */
static int fast_itoa(int value, char *buf){
	if(value == INT_MIN){
		memcpy(buf, "-2147483648", 11);
		buf[11] = '\0';
		return 11;
	}

	char tmp[12];
	int i = 0, j = 0;
	bool neg = (value < 0);

	if(value == 0){
		buf[0] = '0';
		buf[1] = '\0';
		return 1;
	}

	if(neg) value = -value;

	while(value > 0){
		tmp[i++] = (value % 10) + '0';
		value /= 10;
	}

	if(neg) tmp[i++] = '-';

	// reverse
	for(j = 0; j < i; j++) buf[j] = tmp[i - j - 1];
	buf[i] = '\0';

	return i; // So ky tu da ghi
}

/*-----------------------------------------------------------*/

void Logger_three_task_ver2(void const *pvParameter){
	(void)(pvParameter);
	uart_printf("LOGGER task started !\r\n");

	osEvent evt;
	sensor_sync_block_t *data;

	// Buffer local
	static char stream_buf[UART_STREAM_BUFFER_SIZE]; // buffer dem de gom block data roi in 1 lan, giam blocking

#ifdef SYNC_BLOCK_COUNT_DEBUG
	static uint32_t last_count = 0;
	static TickType_t last_tick = 0;
#endif // SYNC_BLOCK_COUNT_DEBUG

	while(1){
		evt = osMailGet(logger_queueId, osWaitForever);
		if(evt.status != osEventMail){
			uart_printf("[LOGGER] Mail error: %d\r\n", evt.status);
			continue; // Di tiep de FreeMail
		}

		// Lay con tro Mail
		data = evt.value.p;
		if(data == NULL) continue;

//		uart_printf("[LOGGER] ID: %lu | COUNT: %u | TIME: %lu\r\n", data->sample_id_sync, data->count_sync, data->timestamp_sync);

		char *p = stream_buf; // Pointer hien tai
		char *buf_end = stream_buf + UART_STREAM_BUFFER_SIZE; // Con tro den vi tri cuoi cua buffer

		for(uint16_t i = 0; i < data->count_sync; i++){ // In theo so luong sample nho nhat trong 3 data

			/* Neu khoang cach giua Pointer hien tai va con tro vi tri cuoi be hon 32 */
			if((buf_end - p) < 32) break;

			/* ECG 16-bit data */
			p += fast_itoa(data->ecg_sync[i], p);
			*p++ = ',';

			/* PPG 32-bit data */
			p += fast_itoa(data->ppg_ir_sync[i], p);
			*p++ = ',';

			/* PCG 32-bit data */
			p += fast_itoa(data->pcg_sync[i], p);
			*p++ = '\r';
			*p++ = '\n';
		}

		/* Chieu dai chuoi = con tro hien tai so voi con tro dau tien */
		uint16_t len = (uint16_t)(p - stream_buf);
		if(len > 0){
			/* Dung truc tiep luon cho toc do nhanh thay vi `uart_printf()` */
			uart_tx_dma((uint8_t*)stream_buf, len);
		}

		osMailFree(logger_queueId, data); // Giai phong Mail

#ifdef SYNC_BLOCK_COUNT_DEBUG
		// Debug so block nhan duoc tu SyncTask de biet bottleneck xay ra the nao
		TickType_t now = osKernelSysTick();
		if(now - last_tick >= 1000){
			uint32_t block_get = g_sync_block_per_sec;
			uint32_t diff = block_get - last_count;
			uart_printf("[LOGGER] Blocks get from SyncTask in 1s: %lu\r\n", diff);
			last_count = block_get;
			last_tick = now;
		}
#endif // SYNC_BLOCK_COUNT_DEBUG
	}
}
#endif // SYNC_INTERMEDIARY_USING

/*-----------------------------------------------------------*/

#ifdef SENSOR_SEND_DIRECT_USING /* NOTE: Cac ham cho viec gui block truc tiep tu Sensor task -> Logger task (khong trung gian) */

osStatus Logger_mail_send(sensor_block_t *block){
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

/*-----------------------------------------------------------*/

// Tinh chenh lech thoi gian so voi time hien tai cua Logger
__attribute__((unused)) static inline void debugSYNC(sensor_block_t *ecg_block, sensor_block_t *ppg_block, sensor_block_t *pcg_block){
	TickType_t logger_timestamp = xTaskGetTickCount(); //Lay tick de so sanh do tre voi cac task khac

	uart_printf("[SYNC] ECG_tick: %lu, PPG_tick: %lu, PCG_tick: %lu, Logger_tick: %lu | delECG: %ld, delPPG: %ld, delPCG: %ld\r\n",
			ecg_block->timestamp,
			ppg_block->timestamp,
			pcg_block->timestamp,
			logger_timestamp,
			(int32_t)(logger_timestamp - ecg_block->timestamp),
			(int32_t)(logger_timestamp - ppg_block->timestamp),
			(int32_t)(logger_timestamp - pcg_block->timestamp));
}

/*-----------------------------------------------------------*/

//Reset trang thai flag ready cua sensor
__attribute__((unused)) static inline void ResetSensor_flag(sensor_check_t *check){
	check->ecg_ready = false;
	check->pcg_ready = false;
	check->ppg_ready = false;
}

/*-----------------------------------------------------------*/

__attribute__((unused)) void Logger_three_task_ver1(void const *pvParameter){
	(void)(pvParameter);
	uart_printf("LOGGER task started !\r\n");

	sensor_block_t *block;
	osEvent evt;

	sensor_block_t ecg_block = {0}, ppg_block = {0}, pcg_block = {0};
	sensor_check_t sensor_check = {
			.ecg_ready = false,
			.ppg_ready = false,
			.pcg_ready = false
	};

	while(1){
		evt = osMailGet(logger_queueId, 200);
		if(evt.status == osEventMail){
			block = evt.value.p;

			switch(block->type){
			case SENSOR_ECG:
				ecg_block = *block; // Copy du lieu tu block sang ecg_block neu la type ECG
				sensor_check.ecg_ready = true;
				break;
			case SENSOR_PPG:
				ppg_block = *block; //Copy du lieu tu block sang ppg_block neu la type PPG
				sensor_check.ppg_ready = true;
				break;
			case SENSOR_PCG:
				pcg_block = *block; //Copy du lieu tu block sang pcg_block neu la type PCG
				sensor_check.pcg_ready = true;
				break;
			}

			osMailFree(logger_queueId, block); // Giai phong bo nho heap ngay de cho block khac gui

			// Neu 3 data da ready
			if(sensor_check.ecg_ready && sensor_check.ppg_ready && sensor_check.pcg_ready){

				// Neu timestamp match nhau
				if(ecg_block.timestamp == ppg_block.timestamp && ecg_block.timestamp == pcg_block.timestamp){

//					debugSYNC(&ecg_block, &ppg_block, &pcg_block);

					// Neu ID cua cac sample match nhau
					if(ecg_block.sample_id == ppg_block.sample_id && ecg_block.sample_id == pcg_block.sample_id){

						// Lay so sample be nhat trong 3 kenh thu duoc
						uint16_t min_count = MIN(MIN(ecg_block.count, ppg_block.count), pcg_block.count);

						uart_printf("[LOGGER] ECG count - ID: %d - %lu, PPG count - ID: %d - %lu, PCG count - ID: %d - %lu\r\n",
								ecg_block.count, ecg_block.sample_id,
								ppg_block.count, ppg_block.sample_id,
								pcg_block.count, pcg_block.sample_id);

						for(uint16_t i = 0; i < min_count; i++){ //In theo so luong sample nho nhat trong 3 data
							uart_printf("%d,%lu,%ld\n",
									ecg_block.ecg[i],
									ppg_block.ppg.ir[i],
									pcg_block.pcg[i]);
						}

						//Reset flag
						ResetSensor_flag(&sensor_check);

					}
					else{
						uart_printf("[LOGGER] Sample_id mismatch ! ECG: %lu, PPG: %lu, PCG: %lu\r\n",
								ecg_block.sample_id, ppg_block.sample_id, pcg_block.sample_id);

						//Reset de sync lai vong sau
						ResetSensor_flag(&sensor_check);
					}
				}
				else{
					uart_printf("[LOGGER] Timestamp mismatch ! ECG: %lu, PPG: %lu, PCG: %lu\r\n",
							ecg_block.timestamp, ppg_block.timestamp, pcg_block.timestamp);

					ResetSensor_flag(&sensor_check);
				}

			}
			else{
//				uart_printf("[LOGGER] Some data is not ready ! ECG: %d, PPG: %d, PCG: %d\r\n",
//						sensor_check.ecg_ready, sensor_check.ppg_ready, sensor_check.pcg_ready);
			}

		}
		else if(evt.status != osEventTimeout) uart_printf("[LOGGER] Mail error: %d\r\n", evt.status); // Neu ticks != 0 thi se tra ve timeout
	}
}

/*-----------------------------------------------------------*/

__attribute__((unused)) void Logger_two_task(void const *pvParameter){
	(void)(pvParameter);
	uart_printf("LOGGER task started !\r\n");

	sensor_block_t *block;
	osEvent evt;

	sensor_block_t __attribute__((unused))ppg_block = {0};
	sensor_block_t __attribute__((unused))pcg_block = {0};
	sensor_block_t __attribute__((unused))ecg_block = {0};

	sensor_check_t sensor_check = {
			.ecg_ready = false,
			.ppg_ready = false,
			.pcg_ready = false
	};

	while(1){
		evt = osMailGet(logger_queueId, 150);
		if(evt.status == osEventMail){
			block = evt.value.p;

			switch(block->type){
			case SENSOR_ECG:
				ecg_block = *block; // Copy du lieu tu block sang ecg_block neu la type ECG
				sensor_check.ecg_ready = true;
				break;
			case SENSOR_PPG:
				ppg_block = *block; //Copy du lieu tu block sang ppg_block neu la type PPG
				sensor_check.ppg_ready = true;
				break;
			case SENSOR_PCG:
				pcg_block = *block; //Copy du lieu tu block sang pcg_block neu la type PCG
				sensor_check.pcg_ready = true;
				break;
			}

			osMailFree(logger_queueId, block); // Giai phong bo nho heap ngay de cho block khac gui

			// Neu 3 data da ready
			if((sensor_check.ecg_ready && sensor_check.ppg_ready) || (sensor_check.ppg_ready && sensor_check.pcg_ready)
																|| (sensor_check.ecg_ready && sensor_check.pcg_ready)){
				// Neu timestamp match nhau
				if((ecg_block.timestamp == ppg_block.timestamp) || (ecg_block.timestamp == pcg_block.timestamp)
															  || (ppg_block.timestamp == pcg_block.timestamp)){

//					debugSYNC(&ecg_block, &ppg_block, &pcg_block);

					// Neu ID cua cac sample match nhau
					if((ecg_block.sample_id == ppg_block.sample_id) || (ecg_block.sample_id == pcg_block.sample_id)
																  || (ppg_block.sample_id == pcg_block.sample_id)){

#if defined(AD8232_ONLY_LOGGER) && defined(MAX30102_ONLY_LOGGER)
						uint16_t min_count = MIN(ecg_block.count, ppg_block.count);

#elif defined(AD8232_ONLY_LOGGER) && defined(INMP441_ONLY_LOGGER)
						uint16_t min_count = MIN(ecg_block.count, pcg_block.count);

#elif defined(MAX30102_ONLY_LOGGER) && defined(INMP441_ONLY_LOGGER)
						uint16_t min_count = MIN(ppg_block.count, pcg_block.count);
#else
						uint16_t min_count = MAX_COUNT; // default
#endif // SENSOR_COUNT
						uart_printf("[LOGGER] ECG count - ID: %d - %lu, PPG count - ID: %d - %lu, PCG count - ID: %d - %lu\r\n",
								ecg_block.count, ecg_block.sample_id,
								ppg_block.count, ppg_block.sample_id,
								pcg_block.count, pcg_block.sample_id);

						for(uint16_t i = 0; i < min_count; i++){ //In theo so luong sample nho nhat trong 3 data
							uart_printf("%d,%lu,%ld\n",
									ecg_block.ecg[i],
									ppg_block.ppg.ir[i],
									pcg_block.pcg[i]);
						}

						//Reset flag
						ResetSensor_flag(&sensor_check);

					}
					else{
						uart_printf("[LOGGER] Sample_id mismatch ! ECG: %lu, PPG: %lu, PCG: %lu\r\n",
								ecg_block.sample_id, ppg_block.sample_id, pcg_block.sample_id);

						//Reset de sync lai vong sau
						ResetSensor_flag(&sensor_check);
					}
				}
				else{
					uart_printf("[LOGGER] Timestamp mismatch ! ECG: %lu, PPG: %lu, PCG: %lu\r\n",
							ecg_block.timestamp, ppg_block.timestamp, pcg_block.timestamp);

					ResetSensor_flag(&sensor_check);
				}

			}
			else{
//				uart_printf("[LOGGER] Some data is not ready ! ECG: %d, PPG: %d, PCG: %d\r\n",
//						sensor_check.ecg_ready, sensor_check.ppg_ready, sensor_check.pcg_ready);
			}
		}
		else if(evt.status != osEventTimeout) uart_printf("[LOGGER] Mail error: %d\r\n", evt.status);  // Neu ticks != 0 thi se tra ve timeout
	}
}

/*-----------------------------------------------------------*/

__attribute__((unused)) void Logger_one_task(void const *pvParameter){
	UNUSED(pvParameter);
	uart_printf("LOGGER task started !\r\n");

	sensor_block_t *block;
	osEvent evt;

	sensor_block_t __attribute__((unused))ppg_block = {0};
	sensor_block_t __attribute__((unused))pcg_block = {0};
	sensor_block_t __attribute__((unused))ecg_block = {0};

	sensor_check_t sensor_check = {
			.ecg_ready = false,
			.ppg_ready = false,
			.pcg_ready = false
	};

	while(1){
		evt = osMailGet(logger_queueId, 100);
		if(evt.status == osEventMail){
			block = evt.value.p;  // Data o day

			switch(block->type){
			case SENSOR_PCG:
				pcg_block = *block;
				sensor_check.pcg_ready = true;
				break;
			case SENSOR_PPG:
				ppg_block = *block;
				sensor_check.ppg_ready = true;
				break;
			case SENSOR_ECG:
				ecg_block = *block;
				sensor_check.ecg_ready = true;
				break;
			}

			osMailFree(logger_queueId, block); // Giai phong bo nho heap ngay de cho block khac gui

			if(sensor_check.ecg_ready || sensor_check.ppg_ready || sensor_check.pcg_ready){

#if defined(MAX30102_ONLY_LOGGER) /* Chi muon log moi PPG */
//				uart_printf("[LOGGER] ID: %lu | COUNT: %u | TIME: %lu\r\n", ppg_block.sample_id, ppg_block.count, ppg_block.timestamp);

				for(uint16_t i = 0; i < ppg_block.count; i++){
					uart_printf("%lu\r\n", ppg_block.ppg.ir[i]);
				}

				ResetSensor_flag(&sensor_check);
#elif defined(AD8232_ONLY_LOGGER) /* Chi muon log moi ECG */
//				uart_printf("[LOGGER] ID: %lu | COUNT: %u | TIME: %ld\r\n", ecg_block.sample_id, ecg_block.count, ecg_block.timestamp);

				for(uint16_t i = 0; i < ecg_block.count; i++){
					uart_printf("%d\r\n", ecg_block.ecg[i]);
				}

				ResetSensor_flag(&sensor_check);
#elif defined(INMP441_ONLY_LOGGER) /* Chi muon log moi PCG */
//				uart_printf("[LOGGER] ID: %lu | COUNT: %u | TIME: %lu\r\n", pcg_block.sample_id, pcg_block.count, pcg_block.timestamp);

				for(uint16_t i = 0; i < pcg_block.count; i++){
					uart_printf("%ld\r\n", pcg_block.pcg[i]);
				}

				ResetSensor_flag(&sensor_check);
#endif // SENSOR_MACROS
			}
		}
		else if(evt.status != osEventTimeout) uart_printf("[LOGGER] Mail error: %d\r\n", evt.status); // Neu ticks != 0 thi se tra ve timeout
	}
}
#endif // SENSOR_SEND_DIRECT_USING

#ifdef __cplusplus
}
#endif
