/*
 * take_snapsync.c
 *
 *  Created on: Dec 24, 2025
 *      Author: ADMIN
 */

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include "take_snapsync.h"
#include "stdio.h"
#include "string.h"
#include "Logger.h" // Lay macro de gui block cam bien

#ifdef CMSIS_API_USING

osThreadId sync_taskId = NULL;
osMailQId sync_queueId = NULL;

#endif // CMSIS_API_USING

/*-----------------------------------------------------------*/

/* Ham se reset trang thai slot */
__attribute__((always_inline)) static inline void sync_slot_reset(sync_slot_t *slot){
	slot->ecg_ready = false;
	slot->max_ready = false;
	slot->mic_ready = false;
}

/*-----------------------------------------------------------*/

/**
 * Kha nang cao 3 task se xay ra viec lech nhip cho task con lai xu ly cham hon
 * Nhung neu xay ra mismatch thi khong the reset het flag duoc ma nen drop cac flag cu nhat
 * (sample_id/timestamp be nhat), giu lai 2 cai con lai de cho block tuong ung den
 * Lam the thay vi bo luon 3 block -> Reset -> task tiep tuc gui -> lai lech -> mismatch lien hoan
 * Thi lam cach nay se lam mat mau it hon va he thong se lai tu hoi phuc lai dung phase ban dau
 * => Su dung sample_id de lam chuan can cu theo de giu phase
 */
__attribute__((always_inline)) static inline void sync_drop_oldest(sync_slot_t *slot){
	uint32_t id_ecg = slot->ecg.sample_id;
	uint32_t id_ppg = slot->ppg.sample_id;
	uint32_t id_pcg = slot->pcg.sample_id;

	// Tim sample be nhat
	uint32_t min_sampleId = MIN(MIN(id_ecg, id_ppg), id_pcg);

	// So sanh de xem nen drop block nao
	if(slot->ecg_ready && id_ecg == min_sampleId) slot->ecg_ready = false;  // Drop
	if(slot->max_ready && id_ppg == min_sampleId) slot->max_ready = false;  // Drop
	if(slot->mic_ready && id_pcg == min_sampleId) slot->mic_ready = false;  // Drop
}

/*-----------------------------------------------------------*/

/* Ham kiem tra du 3 block hay khong */
__attribute__((always_inline)) static inline bool sync_slot_ready(sync_slot_t *slot){
	return (slot->ecg_ready && slot->max_ready && slot->mic_ready);
}

/*-----------------------------------------------------------*/

/* Ham kiem tra dong bo */
static inline bool sync_match(sync_slot_t *slot){
	// Ngan rui ro so sanh logic truc tiep thi se co task bi overwrite boi block moi (Context Switch)
	// Gan ra 1 bien chung de dam bao tinh nguyen tu (atomic) - Nhat quan du lieu
	// Chon ECG vi Timer + ADC -> On dinh nhat
	uint32_t cmm_sample_id = slot->ecg.sample_id;
	TickType_t cmm_timestamp = slot->ecg.timestamp;

	return (slot->ppg.sample_id == cmm_sample_id &&
			slot->pcg.sample_id == cmm_sample_id &&
			slot->ppg.timestamp == cmm_timestamp &&
			slot->pcg.timestamp == cmm_timestamp);
}

/*-----------------------------------------------------------*/

HAL_StatusTypeDef Sync_init(void){
	HAL_StatusTypeDef ret = HAL_OK;

	// Tao Queue cho Sync task
	osMailQDef(syncQueueName, SYNC_QUEUE_LENGTH, sensor_block_t);
	sync_queueId = osMailCreate(osMailQ(syncQueueName), NULL);
	if(sync_queueId == NULL){
		ret |= HAL_ERROR;
	}
	return ret;
}

/*-----------------------------------------------------------*/

void SyncTask(void const *pvParameter){
	(void)(pvParameter);

	osEvent evt;
	sensor_sync_block_t out; // Bien luu gia tri dau ra tu Sync Task
	sensor_block_t *in; // Bien luu gia tri dau vao tu Sensor Task
	sync_slot_t slot;   // Bien de dong bo tham so

	sync_slot_reset(&slot);

	while(1){
		// Nhan Mail tu Sensor task
		evt = osMailGet(sync_queueId, osWaitForever);
		if(evt.status != osEventMail){
			continue;
		}

		// Copy pointer
		in = evt.value.p;

		// Gom theo Type
		switch(in->type){
		case SENSOR_ECG:
			slot.ecg = *in;
			slot.ecg_ready = true;
			break;
		case SENSOR_PCG:
			slot.pcg = *in;
			slot.mic_ready = true;
			break;
		case SENSOR_PPG:
			slot.ppg = *in;
			slot.max_ready = true;
			break;
		}

		osMailFree(sync_queueId, in); // Giai phong bo nho heap ngay de cho block khac gui

		// Neu du 3 block
		if(sync_slot_ready(&slot)){

			// Neu timestamp va sampleId match nhau
			if(sync_match(&slot)){

				out.sample_id = slot.ecg.sample_id;
				out.timestamp = slot.ecg.timestamp;
				out.count = MIN(MIN(slot.ecg.count, slot.ppg.count), slot.pcg.count);

				// !!! Gan truc tiep chi cho bien co cung kieu !!!
				// Ep sang const cho dung voi memcpy (data gio da duoc snapshot roi)
				memcpy(out.ecg, (const int16_t*)slot.ecg.ecg, out.count * sizeof(int16_t));
				memcpy(out.ppg_ir, (const uint32_t*)slot.ppg.ppg.ir, out.count * sizeof(uint32_t));
				memcpy(out.mic, (const uint32_t*)slot.pcg.mic, out.count * sizeof(int32_t));

				// Thuc hien gui Mail cho Logger Task de in ra man hinh
				MAIL_SEND_FROM_SYNC(out);

				// Tiep tuc reset 3 frame neu moi thu deu on
				sync_slot_reset(&slot);

			}else{
//				uart_printf("[SYNC] Mismatch drop oldest ! ECG: %lu PPG: %lu PCG: %lu\r\n",
//							slot.ecg.sample_id, slot.ppg.sample_id, slot.pcg.sample_id);

				// Khong reset het - chi drop cai cu nhat roi cho cai moi
				sync_drop_oldest(&slot);
			}
		}
	}
}

#ifdef __cplusplus
}
#endif // __cplusplus
