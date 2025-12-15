/*
 * inmp441_config.h
 *
 *  Created on: Oct 15, 2025
 *      Author: ADMIN
 */

#ifndef INC_INMP441_CONFIG_H_
#define INC_INMP441_CONFIG_H_

#pragma once

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include "main.h"
#include "stdio.h"
#include "stdbool.h"
#include "cmsis_os.h"
#include "FIR_Filter.h"

/* @note BIG NOTE BEFORE CODE (VAN DE & Y TUONG)
 * 		 IDEA: Do xung clock LRCLK (tao ra Sample Rate) cua phan mem gioi han toi thieu la 8000Hz nen ta can phai Downsample xuong 1000Hz
 * 		 PROBLEMS:
 * 			- Voi LRCLK (Sample Rate) setting tu phan mem = 8000Hz -> Group delay di khi qua DSP filter ben trong cam bien la 2.15ms (tinh theo cong thuc datasheet)
 * 			- Khi downsample -> sinh ra delay do bo loc decimation (filter ap dung truoc khi downsample) + Cong them delay he thong (DMA/Buffer/Task)
 * 			- Neu khong bu tru, PCG se bi lech so voi ECG/PPG
 * 			- Pass-band thay doi khi Fs giam (theo cong thuc datasheet): Neu Fs = 8kHz -> Passband = 3.384kHz
 * 			  -> Nghia la bo loc DSP se giu tot toi ~3.4kHz. Doi voi PCG (tan so < 1kHz) => OK !
 *
 * @remark Downsample bang cach loc Low Pass Filter + Ky thuat Decimation
 * 		   Low Pass Filter co chuc nang loai bo tan so cao truoc khi giam mau -> Tranh aliasing
 * 		   Decimation: Lay 1 mau trong N mau
 *
 *         1000Hz sample rate -> 1ms/sample -> Can xu ly 32 mau/lan (32ms) -> 32 x 4 bytes/sample = 128 bytes
 *         8000Hz sample rate (toi thieu cua cubeMX) -> 0.125ms/sample -> Gap 8 lan -> Mac dinh trong 32ms se thu duoc 256 samples -> 1024 bytes
 *
 *         Cam bien xuat du lieu dau ra khung 32-bit nhung chi co 24-bit co nghia + tre 1 xung SCK truoc moi bit MSB cua moi khung
 *         Trong truong hop mau 24-bit la so am thi (MSB = 1), se can phai tu mo rong dau (Sign Extension) tu 24-bit len 32-bit
*/

// Macros
#define PCG_SAMPLE_RATE	 			8000 // 8000Hz (Hardware)
#define TARGET_SAMPLE_RATE			1000 // 1000Hz (desired)
#define DOWNSAMPLE_FACTOR			(PCG_SAMPLE_RATE / TARGET_SAMPLE_RATE) //8

#define PCG_DMA_BUFFER   			1024 //bytes (8000Hz trong 32ms)
#define I2S_SAMPLE_COUNT 			(PCG_DMA_BUFFER / sizeof(int32_t)) // Desired 256 samples (Sample that)
#define I2S_DMA_FRAME_COUNT			(I2S_SAMPLE_COUNT * 2) // DMA data (dung de ghep 2 frame HALF-WORD)
#define I2S_HALF_SAMPLE_COUNT 		(I2S_SAMPLE_COUNT / 2)
#define DOWNSAMPLE_SAMPLE_COUNT		(I2S_SAMPLE_COUNT / DOWNSAMPLE_FACTOR) // 32 samples (mat mau do ep du lieu xuong 32)

// Extern protocol variable cho function trong .c
extern I2S_HandleTypeDef hi2s2;
extern DMA_HandleTypeDef hdma_spi2_rx;
extern FIRFilter fir;

// Task & RTOS
#if defined(FREERTOS_API_USING)

extern TaskHandle_t inmp441_task;
extern SemaphoreHandle_t sem_mic;

#elif defined(CMSIS_API_USING)

extern osSemaphoreId sem_micId;
extern osThreadId inmp441_taskId;

#endif // CMSIS_API_USING

// INMP441
extern volatile uint16_t buffer16[I2S_DMA_FRAME_COUNT]; // Buffer cho DMA ghi data HALF-WORD vao (Khong chua sample hoan chinh)
extern volatile int32_t __attribute__((unused))buffer32[I2S_SAMPLE_COUNT]; // Buffer de luu sample that (da ghep framw LOW + HIGH)
extern volatile bool __attribute__((unused))mic_half_ready;
extern volatile bool __attribute__((unused))mic_full_ready;

// Flag do DMA set de bao buffer nao vua day VER3
extern volatile bool __attribute__((unused))mic_ping_ready;
extern volatile bool __attribute__((unused))mic_pong_ready;

// 2 buffer ping/pong (DMA se tu ghi luan phien)
extern volatile int16_t __attribute__((unused))mic_ping[I2S_SAMPLE_COUNT];
extern volatile int16_t __attribute__((unused))mic_pong[I2S_SAMPLE_COUNT];

// Forward Declaration (Khai bao tam thoi de compiler biet kieu du lieu nay se duoc dinh nghia o noi khac sau nay)
// Tranh include cheo nhau (A can B, B can A)
typedef struct SENSOR_BLOCK_t sensor_block_t; // From Sensor_config.h
typedef struct SNAPSHOT_SYNC_t snapshot_sync_t; // From take_snapsync.h

//==== FUNCTION PROTOTYPE ====

/**
 * @brief Ham khoi tao giao thuc I2S cho INMP441
 *
 * @param i2s Con tro de tro den struct cau hinh protocol I2S
 *
 * @note Khoi tao co che Semaphore Binary de quan ly tai nguyen chung giua cac cam bien
 * \note - Dong bo giua cac task khac nhau khi co nhieu thread truy cap chung tai nguyen (vd Memory)
 * \note - Semaphore khac voi Mutex o co che Lock/Unlock va Priority
 */
HAL_StatusTypeDef Inmp441_init(I2S_HandleTypeDef *i2s);

/**
 * @brief Ham thuc thi va xu ly `ver1`
 *
 * @note Task nay xu ly du lieu su dung DMA (Normal mode)
 *
 * @remark Ham nay su dung co che Normal DMA - tuc la sau khi thu duoc N sample thi can phai
 * goi lai ham. Su dung Timer de trigger Semaphore dong bo 32ms ket hop DMA callback
 * Data thuc hien dich bit  va lay trung binh mau. Rui ro van bi mat mau va do tre giua Timer va DMA callback
 */
__attribute__((unused)) void Inmp441_task_ver1(void const *pvParameter);

/**
 * @brief Ham thuc thi va xu ly `ver2`
 * Task nay xu ly du lieu su dung DMA (Circular mode)
 *
 * @remark Bat dau doc data duoc ghi vao DMA va luu vao buffer16 voi chieu dai I2S_DMA_FRAME_COUNT (do inmp441 set 8000Hz)
 * 		   Va sau do moi 2 frame HALF_WORD de cho dau ra la data 32-bit
 * 		   Do Circular nen chi can goi `HAL_I2S_Receive_DMA(I2S_HandleTypeDef *hi2s, uint16_t *pData, uint16_t Size)` 1 lan
 *
 * @note Khi cau hinh I2S chon khung du lieu 16/24/32-bit thi tham so `Size` bieu thi so lan DMA transfer du lieu 16/24/32-bit tu Register->RAM
 * 		 Data register I2S Peripheral (SPI->DR) chi toi da 16-bit (gioi han phan cung), nen moi lan DMA chi doc duoc HALF-WORD (16-bit) tu Peripheral
 *       DMA khong biet ve khai niem "Sample", DMA chi biet: Moi request -> copy 1 don vi = Peripheral Data Width (1 lan DMA Transfer)
 *       Neu mong muon 1 sample 32-bit, truoc tien cu cast buffer32 ve 16-bit data `pData` theo API cua HAL (Co ly do moi lam the)
 *       hoac truyen truc tiep buffer16 vao la tot nhat
 *
 *       Sau khi thanh ghi `SPI->DR` du 16-bit -> copy truc tiep vao Memory (tai buffer[] ma USER truyen vao ham)
 *       Ung voi 2 lan lien tiep thanh ghi `SPI->DR` copy "HALF-WORD data" vao ghi vao Memory tai buffer[] ung voi 2 frame LOW va HIGH
 *       De ghep thanh data 32-bit, USER can ghep data 16-bit cua buffer[] thanh dang frame LOW + frame HIGH
 *
 * @example Sample #k:
 * 			DMA read SPI->DR  (16-bit) → frame LOW
 *          DMA read SPI->DR  (16-bit) → frame HIGH
 *          DMA ghi 2 HALF-WORD lien tiep vao buffer:
 * 				+ Transfer #1: buffer[0].low
 * 				+ Transfer #2: buffer[0].high
 * 				+ ...
 * 		    USER code co nhiem vu ghep (frame LOW + frame HIGH) → Ra duoc 1 sample 32-bit
 *
 * ##### I2S DMA FLOW #####
 * + B1: INMP441 thu thap tin hieu tu moi truong -> ADC -> Ouput 24-bit -> Dong goi theo chuan I2S vao Frame 32-bit -> Day bit frame vao chan SD
 * 		 Frame I2S 32-bit: [ b31 ... b16 ][ b15 ... b0 ]
 *
 * + B2: Theo xung clock SCK, I2S shift register nhan bit. Khi du 16-bit (HALF-WORD) -> ghi dung 16-bit vao thanh ghi `SPI->DR`, cu lien tiep nhu the...
 *  	 (SPIx->DR chi chua duoc 1 lan 16-bit tai 1 thoi diem)
 *
 * + B3: Khi thanh ghi SPI->DR full -> Thuc hien copy 16-bit da co trong thanh ghi SPI->DR thang toi RAM (Memory) (chinh la dia chi cua buffer[] da truyen vao)
 * 		 (day chinh la ban chat cua DMA - truy cap truc tiep Memory ma khong can qua CPU)
 *
 * 		 !!! DMA phai cau hinh Memory Data Width = HALF-WORD (16-bit) va Peripheral Data Width luon = HALF-WORD (16-bit)
 *       Khong duoc cau hinh Peripheral Data Width thanh WORD vi khong dung voi do rong thanh ghi phan cung -> Ghi sai alignment
 *       Memory cung phai cau hinh co Data Width = Peripheral Data Width (16-bit) de khong lam lech data alignment
 *       Neu de Memory la WORD (32-bit) thi moi lan register SPI->DR copy 16-bit qua -> 16-bit data + 16-bit rac -> ghi lech buffer -> Memory corruption
 *
 * 		 HAL (API layer cao hon DMA) chi dong vai tro xu ly buffer DMA de tao ra sample dung nghia
 * 		 - Cau hinh I2S (clock, standard, data format 16/24/32 bit) va khoi dong DMA
 * 		 - Quan ly viec nhan du lieu theo kich thuoc Size va goi callback
 * 		 - Khong co chuc nang ghep 2 frame DMA thanh 1 sample 32-bit co nghia. Viec day la do USER dam nhiem
 *
 * + B4: USER code can xu ly buffer DMA da co de ra sample 32-bit dung nghia
 * 		 - Vi I2S frame 32-bit duoc nhan bang 2 lan DMA lien tiep (2 lan 16-bit)
 * 		 - Moi sample logic se tuong ung voi 2 phan tu lien tiep trong buffer 16-bit:
 * 		 	+ low_frame = buffer16[2 * i];
 * 		 	+ high_frame = buffer16[2 * i + 1];
 *
 * 		 - Sau do USER tu ghep thanh 32-bit va (neu can) trich xuat 24-bit signed cho INMP441:
 * 		 	+ sample32 = ((uint32_t)high_frame << 16) | low_frame;
 * 		 	+ sample24 = sign_extened((sample32 >> 8 ) 0x00FFFFFF);
 *
 * 		 - Sau do sample nay se duoc dua vao pipeline xu ly (downsample,...)
 */
void Inmp441_task_ver2(void const *pvParameter);

/**
 * @brief Ham thuc thi va xu ly `ver3`
 *
 * @note Do toc do lay mau nhanh va de tranh bi overwritten du lieu cu khi chua xu ly xong
 * => Su dung Double Buffer Ping-Pong
 * @remark Ham prototype, chua hoat dong duoc
 */
__attribute__((unused)) void Inmp441_task_ver3(void const *pvParameter);


#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* INC_INMP441_CONFIG_H_ */
