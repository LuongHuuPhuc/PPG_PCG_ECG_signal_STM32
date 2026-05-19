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

#include "stdbool.h"
#include "string.h"
#include "data_dispatcher.h" // Gui len Dispatcher de xu ly tiep block data da dong goi

#ifdef SYNC_INTERMEDIARY_USING

osThreadId sync_taskId = NULL;
static osMailQId sync_queueId = NULL;

#ifdef SYNC_BLOCK_COUNT_DEBUG
static volatile uint32_t g_synced_block = 0;
static volatile uint32_t g_mismatched_block_count = 0;	/* Bien luu so lan xay ra mismatch */
#endif // SYNC_BLOCK_COUNT_DEBUG

extern void uart_printf(const char *fmt,...);

/* Slot de gom data & dong bo du lieu trong SYNC (chi dung noi bo trong file nay) */
typedef struct __sync_slot {
	/* Block trung gian de luu data tu dau vao Sensor Task cua 3 cam bien */
	sensor_block_t ecg;
	sensor_block_t ppg;
	sensor_block_t pcg;

	/* Bien luu trang thai data san sang de dong bo */
	bool ecg_ready;
	bool ppg_ready;
	bool pcg_ready;
} sync_slot_t;

/*-----------------------------------------------------------*/

/* Ham se reset trang thai slot */
__attribute__((always_inline)) static inline void sync_slot_reset(sync_slot_t *slot){
	slot->ecg_ready = false;
	slot->ppg_ready = false;
	slot->pcg_ready = false;
}

/*-----------------------------------------------------------*/

/**
 * Kha nang cao 3 task se xay ra viec lech nhip cho task con lai xu ly cham hon
 * Nhung neu xay ra mismatch thi khong the reset het flag duoc ma nen drop cac flag cu nhat
 * (sample_id/timestamp be nhat), giu lai 2 cai con lai de cho block tuong ung den
 * Lam the thay vi bo luon 3 block -> Reset -> task tiep tuc gui -> lai lech -> mismatch lien hoan
 * Thi lam cach nay se lam mat mau it hon va he thong se lai TU HOI PHUC lai dung phase ban dau
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
	if(slot->ppg_ready && id_ppg == min_sampleId) slot->ppg_ready = false;  // Drop
	if(slot->pcg_ready && id_pcg == min_sampleId) slot->pcg_ready = false;  // Drop
}

/*-----------------------------------------------------------*/

/* Ham kiem tra du 3 block hay khong */
__attribute__((always_inline)) static inline bool sync_slot_ready(sync_slot_t *slot){
	return (slot->ecg_ready && slot->ppg_ready && slot->pcg_ready);
}

/*-----------------------------------------------------------*/

/* Ham thuc hien dong bo */
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
	// Tao Queue cho Sync task
	osMailQDef(syncQueueName, SYNC_QUEUE_LENGTH, sensor_block_t);
	sync_queueId = osMailCreate(osMailQ(syncQueueName), NULL);
	if(sync_queueId == NULL){
		return HAL_ERROR;
	}
	return HAL_OK;
}

/*-----------------------------------------------------------*/

osStatus Sync_mail_send(sensor_block_t *block){
	sensor_block_t *mail = osMailAlloc(sync_queueId, 0); // Cap phat dong
	if(mail == NULL){
		return osErrorNoMemory;
	}

	*mail = *block; // Copy
	osStatus ret = osMailPut(sync_queueId, mail); // Send mail
	if(ret != osOK){
		osMailFree(sync_queueId, mail);  /* Tranh memory leak neu khong gui duoc Mail */
	}

	/* Chi free sau khi block duoc xu ly xong tai Sync task */
	return ret;
}

/*-----------------------------------------------------------*/

void Sync_Task(void const *pvParameter){
	(void)(pvParameter);
	uart_printf("SYNC task started !\r\n");

	osEvent evt;
	sensor_block_t *in; // Bien luu gia tri dau vao tu Sensor Task
	sensor_sync_block_t out; // Bien luu gia tri dau ra tu Sync Task
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
		if(in == NULL) continue;

		// Gom theo Type roi copy data vao sensor tuong ung vao slot
		switch(in->type){
		case SENSOR_ECG:
			slot.ecg = *in;
			slot.ecg_ready = true;
			break;
		case SENSOR_PCG:
			slot.pcg = *in;
			slot.pcg_ready = true;
			break;
		case SENSOR_PPG:
			slot.ppg = *in;
			slot.ppg_ready = true;
			break;
		}

		osMailFree(sync_queueId, in); // Giai phong bo nho heap ngay de cho block khac gui den

		// Neu ca 3 block data ready
		if(sync_slot_ready(&slot)){

			// Neu timestamp va sampleId match nhau
			if(sync_match(&slot)){

				out.sample_id_sync = slot.ecg.sample_id;
				out.timestamp_sync = slot.ecg.timestamp;
				out.count_sync = MIN(MIN(slot.ecg.count, slot.ppg.count), slot.pcg.count);

				memcpy(out.ecg_sync, slot.ecg.ecg, out.count_sync * sizeof(int16_t));
				memcpy(out.ppg_ir_sync, slot.ppg.ppg.ir, out.count_sync * sizeof(uint32_t));
				memcpy(out.pcg_sync, slot.pcg.pcg, out.count_sync * sizeof(int32_t));

				/* NOTE: Gui mail block data da sync den trung tam xu ly de DataDispatcher lo het phan con lai */
				DataDispatcher_Send(&out);

#ifdef SYNC_BLOCK_COUNT_DEBUG
				g_synced_block++; /* Dem so blocks duoc gui di */
#endif // SYNC_BLOCK_COUNT_DEBUG

				sync_slot_reset(&slot); // Tiep tuc reset 3 frame neu moi thu deu on
			}

			// Neu ready nhung mismatch
			else{
#ifdef SYNC_BLOCK_COUNT_DEBUG
				/* Thay vi in log thi chi dung 1 bien de ghi lai so lan xay ra mismatch */
				g_mismatched_block_count++;
#else
				/**
				 * DEBUG NOTE:
				 * Sau khi debug, pcg luon bi lech frame cham hon so 2 sensor con lai (luon di sau 1 nhip)
				 * nhung moi giay van nhan du 32 blocks, khong cham throughput nhung toan bo frame bi
				 * lech phase 1 nhip. Ly do la vi PCG tu luc DMA tranfer xong -> CPU co data de xu ly thi mat hon 32ms !
				 * (Da ghi trong task) The nen neu cu lech phase 1 block thi task lai bi roi vao day de drop va doi
				 * block moi den, lien tuc nhu the voi cac block tiep theo se deu bi tre frame so voi ecg va pcg
				 * Sai so nay se tich luy theo thoi gian qua nhieu frame nen luc pcg lay duoc id moi thi ecg/ppg block
				 * da nhay sang id khac
				 * -> The neu 1s tuy co du 32 blocks nhung van xay ra 32 lan mismatch den tu pcg !
				 */
				uart_printf("[SYNC] Mismatch ! ECG_id= %lu, PPG_id= %lu, PCG_id= %lu\r\n",
						slot.ecg.sample_id,
						slot.ppg.sample_id,
						slot.pcg.sample_id);
#endif // SYNC_BLOCK_COUNT_DEBUG

				/**
				 * Khong reset het - chi drop sensor tre nhat roi doi cai moi
				 * Drop roi doi den khi nao 3 block ready thi moi gui di
				 * The nen ve ly thuyet se gui du 31-32 blocks/s cho du co bi mismatch
				 * nhung neu bi lech thi se bo luon block do
				 */
				sync_drop_oldest(&slot);
			}
		}
	}
}

/*-----------------------------------------------------------*/

#ifdef SYNC_BLOCK_COUNT_DEBUG
uint32_t Sync_block_ok_count(void){
	return g_synced_block;
}

/*-----------------------------------------------------------*/

uint32_t Sync_mismatch_count(void){
    return g_mismatched_block_count;
}
#endif // SYNC_BLOCK_COUNT_DEBUG

/*-----------------------------------------------------------*/

#endif  // SYNC_INTERMEDIARY_USING

#ifdef __cplusplus
}
#endif // __cplusplus
