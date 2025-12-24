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
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include "cmsis_os.h"
#include "max30102_low_level.h"

// Macro de debug write va read low level
#define MAXLOWLEVELCHECKFUNC(__x__) do { \
	if((__x__) != HAL_OK) { \
		uart_printf("Function %s HAL error, stop !", #__x__); \
		configASSERT(0);\
	}\
} while(0)

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

typedef struct max30102_record {
	uint8_t activeLeds;
	uint8_t head; //Head of samples
	uint8_t tail; //Tail of samples
	uint32_t red_sample[MAX30102_STORAGE_SIZE];
	uint32_t ir_sample[MAX30102_STORAGE_SIZE]; //Do dai 1 samples data doc tu Register
	uint32_t green_sample[MAX30102_STORAGE_SIZE];
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
 * @brief Enable A_FULL interrupt.
 *
 * @param obj Pointer to max30102_t object instance.
 * @param enable Enable (1) or disable (0).
 */
void max30102_set_a_full(max30102_t *obj, uint8_t enable);

/**
 * @brief Enable PPG_RDY interrupt.
 *
 * @param obj Pointer to max30102_t object instance.
 * @param enable Enable (1) or disable (0).
 */
void max30102_set_ppg_rdy(max30102_t *obj, uint8_t enable);

/**
 * @brief Enable ALC_OVF interrupt.
 *
 * @param obj Pointer to max30102_t object instance.
 * @param enable Enable (1) or disable (0).
 */
void max30102_set_alc_ovf(max30102_t *obj, uint8_t enable);

/**
 * @brief Enable DIE_TEMP_RDY interrupt.
 *
 * @param obj Pointer to max30102_t object instance.
 * @param enable Enable (1) or disable (0).
 */
void max30102_set_die_temp_rdy(max30102_t *obj, uint8_t enable);

/**
 * @brief Enable temperature measurement.
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
 */
void max30102_interrupt_handler(max30102_t *obj);

/**
 * @brief Shutdown the sensor.
 *
 * @param obj Pointer to max30102_t object instance.
 * @param shdn Shutdown bit.
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
 * @param fifo_a_full So sample empty trong FIFO truoc khi interrupt sinh ra de doc du lieu
 *
 * Giam do tre (Chi lay 1 mau va khong lay trung binh) + push vao FIFO sau khi doc du 32 mau (32ms)
 *
 */
void max30102_set_fifo_config(max30102_t *obj, max30102_smp_ave_t smp_ave, uint8_t roll_over_en, uint8_t fifo_a_full);

/**
 * @brief Clear all FIFO pointers in the sensor.
 *
 * @param obj Pointer to max30102_t object instance.
 */
HAL_StatusTypeDef max30102_clear_fifo(max30102_t *obj);

/**
 * @brief Read FIFO content and store to buffer in max30102_t object instance.
 *
 * @param obj Pointer to max30102_t object instance.
 * @retval In ra data luon, khong tra ve gi ca
 */
void max30102_read_fifo_ver1(max30102_t *obj);

/**
 * @brief Ham nay doc gia tri FIFO roi tra ve so mau da doc (VER 2.2)
 * @retval `num_samples` - So mau doc duoc
 *
 * @warning Doc lien tuc toan bo du lieu trong FIFO 1 lan (Burst Read)
 * Roi moi vao vong for
 *
 * @param obj - Con tro tro toi doi tuong max30102_t
 * @param ir_buf - Buffer chua du lieu cua IR lay tu fifo
 * @param red_buf - Buffer chua du lieu cua RED lay tu fifo
 * @param max_samples - So mau toi da (Bang voi kich thuoc cua FIFO) - moi sample 6 byte (3 byte IR + 3 byte RED)
 *
 * @note Bit order co LSB luon ben phai, MSB luon ben trai va tuan theo Endianess la Little-Endian
 * \note Byte 1 [17:16], Byte 2[15:8], Byte 3[7:0]
 * \note Vi tri [23:18] khong su dung do do phan giai toi da cua max30102 la 18-bits nen khong the du 24-bits
 */
uint16_t max30102_read_fifo_ver2_2(max30102_t *obj, max30102_record *record, uint32_t *ir_buf,
									uint32_t *red_buf, uint16_t max_samples);

/**
 * @brief Ham nay doc gia tri FIFO roi tra ve so mau da doc (VER 2.1)
 * @retval `num_samples` - So mau doc duoc
 *
 * @warning Khong on dinh (van mat mau)
 * Doc 1 sample (6 bytes) moi vong for
 *
 * @param obj - Con tro tro toi doi tuong max30102_t
 * @param ir_buf - Buffer chua du lieu cua IR lay tu fifo
 * @param red_buf - Buffer chua du lieu cua RED lay tu fifo
 * @param max_samples - So mau toi da (Bang voi kich thuoc cua FIFO) - moi sample 6 byte (3 byte IR + 3 byte RED)
 *
 * @note Bit order co LSB luon ben phai, MSB luon ben trai va tuan theo Endianess la Little-Endian
 * \note Byte 1 [17:16], Byte 2[15:8], Byte 3[7:0]
 * \note Vi tri [23:18] khong su dung do do phan giai toi da cua max30102 la 18-bits nen khong the du 24-bits
 */
uint16_t __attribute__((unused))max30102_read_fifo_ver2_1(max30102_t *obj, max30102_record *record,
														  uint32_t *ir_buf, uint32_t *red_buf, uint16_t max_samples);

/**
 * @brief Ham kiem tra trang thai thanh ghi (DEBUG)
 *
 * @note In ra gia tri Hex cua cac thanh ghi:
 * MODE_CONFIG (0x09) - SPO2_CONFIG (0x0A) - LED1 (0x0C) - LED2 (0x0D) - FIFO_CONFIG(0x08)
 * Giup xac dinh cau hinh hien tai
 */
void max30102_config_register_status_verbose(max30102_t *obj);

/**
 * @brief Ham doc gia tri FIFO troi tra ve so mau da doc
 * @note Code toi uu byte cho dong vi dieu khien co thanh ghi han che nhu Arduino
 */
int16_t __attribute__((unused))max30102_read_fifo_ver3(max30102_t *obj, max30102_record *record, uint16_t max_samples);

/**
 * @brief Ham doc data RED tu FIFO
 */
uint32_t __attribute__((unused))max30102_ver3_getFIFORed(max30102_record *record);

/**
 * @brief Ham doc data IR tu FIFO
 */
uint32_t __attribute__((unused))max30102_ver3_getFIFOIR(max30102_record *record);

/**
 * @brief Ham kiem tra xem co bao nhieu sample dang ton tai
 *
 */
int __attribute__((unused))max30102_ver3_sample_available(max30102_record *record);

/**
 * @brief Ham doc tiep samples tu tail cua FIFO
 *
 */
void __attribute__((unused))max30102_ver3_next_sample(max30102_record *record);

#endif

#ifdef __cplusplus
}
#endif
