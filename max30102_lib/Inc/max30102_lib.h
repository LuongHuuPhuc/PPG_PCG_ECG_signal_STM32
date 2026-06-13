/*
 * @file max30102_lib.h
 * @author LuongHuuPhuc
 *
 *  Created on: Feb 10, 2025
 *
 */

#ifndef MAX30102_LIB_H
#define MAX30102_LIB_H

#ifdef __cpluslplus
extern "C"{
#endif

#include <stdint.h>
#include <stdlib.h>
#include "cmsis_os.h"
#include "max30102_low_level.h"

// Macro de debug write va read low level
#define MAXLOWLEVELCHECKFUNC(__x__) do { \
	if((__x__) != HAL_OK) { \
		uart_printf("Function %s HAL error, stop !", #__x__); \
		configASSERT(0);\
	}\
} while(0)

#define MAX_UNUSED(X) 			__attribute__((unsued)) x

typedef enum max30102_mode_t
{
    max30102_heart_rate = 0x02,
    max30102_spo2 = 0x03,
    max30102_multi_led = 0x07
} max30102_mode_t;

typedef enum max30102_smp_ave_t
{
    max30102_smp_ave_1,
    max30102_smp_ave_2,
    max30102_smp_ave_4,
    max30102_smp_ave_8,
    max30102_smp_ave_16,
    max30102_smp_ave_32,
} max30102_smp_ave_t;

typedef enum max30102_sr_t
{
    max30102_sr_50,
    max30102_sr_100,
    max30102_sr_200,
    max30102_sr_400,
    max30102_sr_800,
    max30102_sr_1000,
    max30102_sr_1600,
    max30102_sr_3200
} max30102_sr_t;

typedef enum max30102_led_pw_t
{
    max30102_pw_69_us,
    max30102_pw_118_us,
    max30102_pw_215_us,
    max30102_pw_411_us
} max30102_led_pw_t;

typedef enum max30102_adc_t
{
    max30102_adc_15_bit, // (0 - 2047) tuong ung voi pw 69us
    max30102_adc_16_bit, // (0 - 4095) tuong ung voi pw 118us
    max30102_adc_17_bit, // (0 - 8191) tuong ung voi pw 215us
    max30102_adc_18_bit  // (0 - 16383) tuong ung voi pw 411us
} max30102_adc_t;

typedef enum max30102_multi_led_ctrl_t
{
    max30102_led_off,
    max30102_led_red,
    max30102_led_ir
} max30102_multi_led_ctrl_t;

/* Cau truc du lieu cho data cua MAX30102 */
typedef struct max30102_record {
	uint8_t activeLeds; // So led hoat dong
	uint8_t head; 		// Head of samples
	uint8_t tail; 		// Tail of samples
} max30102_record;

extern void uart_printf(const char *fmt,...); // Logger.h - muon dung ham do thi khai bao extern

// === MAX30102 CONFIG ====

/**
 * @brief Built-in plotting function. Called during an interrupt to print/plot the current sample.
 * @note Override this in your main.c if you do not use printf() for printing.
 * @param ir_sample
 * @param red_sample
 */
void max30102_plot(uint32_t ir_sample, uint32_t red_sample);

/**
 * @brief MAX30102 initiation function.
 *
 * @param obj Pointer to max30102_t object instance.
 * @param hi2c Pointer to I2C object handle
 */
void max30102_init(max30102_t *obj, I2C_HandleTypeDef *hi2c);

/**
 * @brief Reset the sensor.
 *
 * @param obj Pointer to max30102_t object instance.
 */
void max30102_reset(max30102_t *obj);

/**
 * @brief Bat tat A_FULL_EN interrupt.
 * Thanh ghi cho phep tao ngat neu con tro ghi thay FIFO co mot khoang trong co dinh
 * truoc ghi day (phu thuoc vao gia tri 5bits duoc set tai FIFO_A_FULL tai thanh ghi FIFO_CONFIG 0x08)
 * Khi do gia tri A_FULL trong thanh ghi Interrupt Status 1 (0x00) se duoc phep thay doi
 *
 * @param obj Pointer to max30102_t object instance.
 * @param enable Enable (1) or disable (0).
 */
void max30102_set_a_full_itrp(max30102_t *obj, uint8_t enable);

/**
 * @brief Bat-tat PPG_RDY_EN interrupt.
 * Thanh ghi cho phep tao ngat neu dang co data moi trong FIFO
 * Khi do gia tri PPG_RDY trong thanh ghi Interrupt Status 1 (0x00) se duoc phep thay doi
 *
 * @param obj Pointer to max30102_t object instance.
 * @param enable Enable (1) or disable (0).
 */
void max30102_set_ppg_rdy_itrp(max30102_t *obj, uint8_t enable);

/**
 * @brief Bat-tat ALC_OVF_EN interrupt.
 * Thanh ghi nay cho phep tao ngat neu Ambient light (anh sang moi truong) qua lon
 * den gioi han nhat dinh, gay anh huong den tin hieu thu duoc
 * Khi do gia tri ALC_OVF trong thanh ghi Interrupt Status 1 (0x00) se duoc phep thay doi
 *
 * @param obj Pointer to max30102_t object instance.
 * @param enable Enable (1) or disable (0).
 */
void max30102_set_alc_ovf_itrp(max30102_t *obj, uint8_t enable);

/**
 * @brief Enable power ready flag interrupt
 * Neu phat hien cam bien co su co sut ap do khoi dong hoac do nguyen nhan nao do
 * khien dien ap duoi muc nguong (brownout), roi lai tang len tren muc nguong, hanh ghi
 * cho phep tao ngat de bao hieu rang cam bien da duoc cap dien va san sang thu thap data
 *
 * @param obj Pointer to max30102_t object instance.
 * @param enable Enable (1) or disable (0).
 */
void max30102_set_power_ready(max30102_t *obj, uint8_t enable);

/**
 * @brief Bat-tat DIE_TEMP_RDY_EN (Internal Temperature Ready Flag) interrupt.
 * Thanh ghi cho phep tao ngat khi cam bien chuyen doi nhiet do noi bo hoan tat
 * tu tuong tu sang so, giup theo doi nhiet do noi bo cua chip cung nhu hieu chinh
 * cac phep do SpO2 va bu tru cac sai so do su thay doi cua moi truong gay ra
 * Khi do gia tri DIE_TEMP_RDY trong thanh ghi Interrupt Status 2 (0x01) se duoc phep thay doi
 *
 * @param obj Pointer to max30102_t object instance.
 * @param enable Enable (1) or disable (0).
 */
void max30102_set_die_temp_rdy_itrp(max30102_t *obj, uint8_t enable);

/**
 * @brief Enable temperature measurement.
 * Thanh ghi cho phep thuc hien cac phep do nhiet do noi bo cua cam bien
 *
 * @param obj Pointer to max30102_t object instance.
 * @param enable Enable (1) or disable (0).
 */
void max30102_set_die_temp_en(max30102_t *obj, uint8_t enable);

/**
 * @brief Set interrupt flag on interrupt. To be called in the corresponding external interrupt handler.
 *
 * @param obj Pointer to max30102_t object instance.
 */
void max30102_on_interrupt(max30102_t *obj);

/**
 * @brief Check whether the interrupt flag is active.
 *
 * @param obj Pointer to max30102_t object instance.
 * @return uint8_t Active (1) or inactive (0).
 */
uint8_t max30102_has_interrupt(max30102_t *obj);

/**
 * @brief Read interrupt status registers (0x00 and 0x01) and perform corresponding tasks.
 *
 * @param obj Pointer to max30102_t object instance.
 * @param record Pointer to max30102_record object instance
 */
void max30102_interrupt_handler(max30102_t *obj, max30102_record *record);

/**
 * @brief Shutdown the sensor.
 * Dua cam bien vao trang thai tiet kiem nang luong. Khi do tat ca thanh ghi
 * se giu nguyen gia tri, khi do moi ngat cung se bi clear ve 0
 *
 * @param obj Pointer to max30102_t object instance.
 * @param shdn Shutdown bit (1 - enable, 0 - disable).
 */
void max30102_shutdown(max30102_t *obj, uint8_t shdn);

/**
 * @brief Set measurement mode.
 *
 * @param obj Pointer to max30102_t object instance.
 * @param mode Measurement mode enum (max30102_mode_t).
 */
void max30102_set_led_mode(max30102_t *obj, max30102_record *record, max30102_mode_t mode);

/**
 * @brief Set sampling rate.
 *
 * @param obj Pointer to max30102_t object instance.
 * @param sr Sampling rate enum (max30102_spo2_st_t).
 *
 * @note Sample Rate va Pulse Width co lien quan voi nhau o cho SR dat mot gioi han tren cho PW
 * \note Neu user chon SR qua cao so voi PW da chon thi SR se bi gioi han boi thanh ghi
 */
void max30102_set_sampling_rate(max30102_t *obj, max30102_sr_t sr);

/**
 * @brief Set led pulse width.
 *
 * @param obj Pointer to max30102_t object instance.
 * @param pw Pulse width enum (max30102_led_pw_t).
 *
 * @warning Pulse Width va Sample Rate co anh huong den nhau.
 * \warning Neu voi pw 215us va sr 1000Hz ma set:
 * \warning - ADC res 18-bit thi sr se chi gioi han 400Hz (0x6E)
 * \warning - ADC res 17-bit/16-bit thi sr se chi gioi han 800Hz (0x52 voi 17-bit va 0x32 voi 16-bit)
 * \warning - The nen khong the co dinh sr 1000Hz voi pw 215us -> Giam pw (1) hoac giu nguyen pw roi giam ADC res (2)
 *
 * @if (1) Neu muon cai thien ADC_res: giam Pulse Width (Recommend)
 * - Pulse Width 118us (Vua du)
 * - ADC res 18-bit (Bu lai co the set toi da gia tri ADC)
 * - Khi do thanh ghi SPO2_CONFIG se co gia tri mong muon => `0x75`
 *
 * @if (2) Neu muon cai thien PW, chap nhan ADC res thap: Giu nguyen Pulse Width 215us roi giam ADC res
 * - Pulse Width 215us (giam nhieu tot hon)
 * - ADC res 15-bit (Gia tri be nhat co the)
 * - Khi do thanh ghi SPO2_CONFIG se co gia tri mong muon => `0x13`
 * => Thuc te voi Pulse Width 215us thi cach tren van khong ra duoc ket qua mong muon (thuc te 0x12)
 *
 * @example De co dinh duoc Sample Rate 1000Hz tot nhat: Cach (1)
 */
void max30102_set_led_pulse_width(max30102_t *obj, max30102_led_pw_t pw);

/**
 * @brief Set ADC resolution.
 *
 * @param obj Pointer to max30102_t object instance.
 * @param adc ADC resolution enum (max30102_adc_t).
 *
 * @warning Pulse Width va Sample Rate co anh huong den nhau.
 * \warning Neu voi pw 215us va sr 1000Hz ma set:
 * \warning - ADC res 18-bit thi sr se chi gioi han 400Hz (0x6E)
 * \warning - ADC res 17-bit/16-bit thi sr se chi gioi han 800Hz (0x52 voi 17-bit va 0x32 voi 16-bit)
 * \warning - The nen khong the co dinh sr 1000Hz voi pw 215us -> Giam pw (1) hoac giu nguyen pw roi giam ADC res (2)
 *
 * @if (1) Neu muon cai thien ADC_res: giam Pulse Width (Recommend)
 * - Pulse Width 118us (Vua du)
 * - ADC res 18-bit (Bu lai co the set toi da gia tri ADC)
 * - Khi do thanh ghi SPO2_CONFIG se co gia tri mong muon => `0x75`
 *
 * @if (2) Neu muon cai thien PW, chap nhan ADC res thap: Giu nguyen Pulse Width 215us roi giam ADC res
 * - Pulse Width 215us (giam nhieu tot hon)
 * - ADC res 15-bit (Gia tri be nhat co the)
 * - Khi do thanh ghi SPO2_CONFIG se co gia tri mong muon => `0x13`
 * => Thuc te voi Pulse Width 215us thi cach tren van khong ra duoc ket qua mong muon (thuc te 0x12)
 *
 * @example De co dinh duoc Sample Rate 1000Hz tot nhat: Cach (1)
 *
 */
void max30102_set_adc_resolution(max30102_t *obj, max30102_adc_t adc);

/**
 * @brief Set IR LED current.
 *
 * @param obj Pointer to max30102_t object instance.
 * @param ma LED current float (0 < ma < 51.0).
 */
void max30102_set_led_current_ir(max30102_t *obj, float ma);

/**
 * @brief Set RED LED current.
 *
 * @param obj Pointer to max30102_t object instance.
 * @param ma LED current float (0 < ma < 51.0).
 */
void max30102_set_led_current_red(max30102_t *obj, float ma);

/**
 * @brief Set slot mode when in multi-LED mode.
 *
 * @param obj Pointer to max30102_t object instance.
 * @param slot1 Slot 1 mode enum (max30102_multi_led_ctrl_t).
 * @param slot2 Slot 2 mode enum (max30102_multi_led_ctrl_t).
 */
void max30102_set_multi_led_slot_1_2(max30102_t *obj, max30102_multi_led_ctrl_t slot1, max30102_multi_led_ctrl_t slot2);

/**
 * @brief Set slot mode when in multi-LED mode.
 *
 * @param obj Pointer to max30102_t object instance.
 * @param slot1 Slot 1 mode enum (max30102_multi_led_ctrl_t).
 * @param slot2 Slot 2 mode enum (max30102_multi_led_ctrl_t).
 */
void max30102_set_multi_led_slot_3_4(max30102_t *obj, max30102_multi_led_ctrl_t slot3, max30102_multi_led_ctrl_t slot4);

/**
 * @brief Read die temperature.
 *
 * @param obj Pointer to max30102_t object instance.
 * @param temp_int Pointer to store the integer part of temperature. Stored in 2's complement format.
 * @param temp_frac Pointer to store the fractional part of temperature. Increments of 0.0625 deg C.
 */
void max30102_read_temp(max30102_t *obj, int8_t *temp_int, uint8_t *temp_frac);

/**
 * @brief Ham cau hinh thanh ghi FIFO
 *
 * @param obj Con tro toi doi tuong cua struct max30102_t
 * @param smp_ave So mau trung binh sau do moi sinh interrupt de push vao FIFO
 * @param roll_over_en Cho phep ghi de FIFO neu FIFO day (0 - enable, 1 - disable)
 * @param fifo_a_full So sample empty trong FIFO truoc khi interrupt sinh ra de doc du lieu (0->15)
 * (0 -> FIFO gan nhu full moi interrupt). Can phai enable interrupt thi moi dung duoc.
 *
 * -> Giam do tre (Chi lay 1 mau va khong lay trung binh) + push vao FIFO sau khi doc du 32 mau (32ms)
 */
void max30102_set_fifo_config(max30102_t *obj, max30102_smp_ave_t smp_ave, uint8_t roll_over_en, uint8_t fifo_a_full);

/**
 * @brief Ham doc so lan FIFO bi oveflow data
 *
 * @param[in] obj Con tro toi doi tuong struct max30102_t
 * @retval Gia tri doc duoc tai OVF_COUNTER
 *
 * @note
 * Khi FIFO full, samples se khong duoc day them vao FIFO ma bi mat di
 * Thanh ghi OVF_COUNTER se dem so lan samples bi mat.
 * Neu MCU chua kip doc data ma cam bien tiep tuc day mau moi vao, cac mau cu se bi ghi de -> ovf se tang len (tu 0 -> 31)
 */
uint8_t max30102_read_overflow_counter(max30102_t *obj);

/**
 * @brief Clear all FIFO pointers in the sensor.
 * @param obj Pointer to max30102_t object instance.
 */
void max30102_clear_fifo(max30102_t *obj);

/**
 *
 */
uint8_t max30102_get_write_pointer(max30102_t *obj);

/**
 *
 */
uint8_t max30102_get_read_pointer(max30102_t *obj);

/**
 * @brief Ham doc gia tri tai n thanh ghi lien tiep (burst read)
 * Giup doc 1 lan duy nhat, giam transaction cua I2C
 *
 * @param obj Con tro tro toi doi tuong co kieu max30102_t
 * @param from_reg Vi tri thanh ghi dau tien can doc
 * @param n so thanh ghi ke tiep can doc
 * @param out_buf buffer luu gia tri doc duoc (output)
 */
void max30102_burst_read_reg(max30102_t *obj, uint8_t from_reg, uint8_t n, uint8_t *out_buf);

/**
 * @brief Ham kiem tra so sample trong FIFO
 *
 * @param obj Con tro tro toi doi tuong co kieu max30102_t
 * @param wr Con tro luu gia tri wr_ptr doc duoc trong ham (neu muon doc o noi khac). NULL neu khong dung
 * @param rd Con tro luu gia tri rd_ptr doc duoc trong ham (neu muon doc o noi khac). NULL neu khong dung
 *
 * + Moi lan cam bien ghi data vao FIFO, con tro ghi lai tang len -> Dan dau
 * + Moi lan doc xong data tai FIFO, con tro doc lai tang len -> Di sau
 *
 * FIFO: | 0 | 1 | 2 | ... | 29 | 30 | 31 |
 *
 * 1. De doc duoc FIFO nhu mong muon thi con tro ghi luc nao cung phai cach con tro doc 1 chu ky FIFO
 * -> Con tro ghi luon phai di truoc con tro doc 1 vong tinh tien (trai->phai) de duoi kip con tro doc tu phia sau
 * -> Con tro ghi gio nam ben trai con tro doc va cach nhau 1 khoang be nhat co the (2 con tro cang gan cang tot)
 * (Neu khoang cach nay lon -> Do MCU doc nhanh het 1 chu ky FIFO, bo xa con tro ghi phia sau/FIFO ghi khong kip)
 * Neu 2 con tro bang nhau trong TH nay -> doc duoc 1 vong -> FULL FIFO
 *
 * 2. Nguoc lai, neu MCU doc nhanh -> con tro doc di 1 het 1 vong duoi kip con tro ghi tu phia sau, FIFO ghi khong kip
 * -> Con tro ghi gio nam ben phai con tro doc -> Doc duoc it mau (drop)
 * Neu 2 con tro bang nhau trong TH nay -> MCU doc het du lieu -> EMPTY FIFO -> Luc nay neu MCU van gui xung clock I2C
 * de co hut them data, sensor se khong tang con tro doc. Thay vao do no dong bang (Freeze) va lien tuc day gia tri cua
 * sample cu nhat ma no co (NSX lam the tranh con tro doc VUOT QUA con tro ghi -> FIFO se la sample rac !)
 *
 * 3. Hien tuong nghen FIFO: xay ra khi con tro ghi chay qua nhanh (do tan so lay mau cao hoac clock drift)
 * trong khi con tro doc khong kip doc het m sample con lai -> no vo tinh VUOT QUA con tro doc n ô.
 * -> Luc MCU tinh toan con tro de doc FIFO -> Vo tinh tinh sai lech la FIFO moi chi co n samples,
 * gay nghen m sample cu trong chip
 *
 * Khi con tro ghi (wr_ptr) = con tro doc (rd_ptr), co 2 truong hop:
 * -> FIFO empty (0): Khi do MCU da doc sach toan bo du lieu truoc do, lam con tro doc duoi kip con tro ghi (Doc nhanh, Ghi khong kip)
 * -> FIFO full (32): Khi do con tro ghi quay vong duoi kip con tro doc (Ghi nhanh, Doc khong kip)
 * Neu cau hinh FIFO, set fifo_a_full = 0 -> FIFO empty = 0 moi interrupt doc FIFO.
 *
 * @retval 0: FIFO empty, !=0: FIFO co data
 */
uint16_t max30102_fifo_available(max30102_t *obj, uint8_t *wr, uint8_t *rd);

/**
 * @brief Ham nay doc gia tri FIFO roi tra ve so mau da doc
 * Doc lien tuc toan bo du lieu trong FIFO 1 lan (Burst Read) roi moi vao vong for
 *
 * @param obj - Con tro tro toi doi tuong max30102_t
 * @param record Con tro toi doi tuong kieu max30102_record
 * @param max_samples - So mau toi da (Bang voi kich thuoc cua FIFO) - moi sample 6 byte (3 byte IR + 3 byte RED)
 *
 * @note
 * Bit order co LSB luon ben phai, MSB luon ben trai va tuan theo Endianess la Little-Endian
 * Byte 1 [17:16], Byte 2[15:8], Byte 3[7:0]
 * Vi tri [23:18] khong su dung do do phan giai toi da cua max30102 la 18-bits nen khong the du 24-bits
 *
 * @retval `num_samples` - So mau doc duoc
 */
uint16_t max30102_read_fifo(max30102_t *obj, max30102_record *record, uint16_t max_samples);

/**
 * @brief Ham nay doc gia tri FIFO roi tra ve so mau da doc (with external interrupt)
 */
__attribute__((unused))
uint16_t max30102_read_fifo_on_interrupt(max30102_t *obj, max30102_record *record);

/**
 * @brief Ham kiem tra trang thai thanh ghi (DEBUG)
 *
 * @note In ra gia tri Hex cua cac thanh ghi:
 * MODE_CONFIG (0x09) - SPO2_CONFIG (0x0A) - LED1 (0x0C) - LED2 (0x0D) - FIFO_CONFIG(0x08)
 * Giup xac dinh cau hinh hien tai
 */
void max30102_config_register_status_verbose(max30102_t *obj);

#endif

#ifdef __cplusplus
}
#endif
