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

#include "main.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include "max30102_low_level.h"

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
    max30102_pw_15_bit,
    max30102_pw_16_bit,
    max30102_pw_17_bit,
    max30102_pw_18_bit
} max30102_led_pw_t;

typedef enum max30102_adc_t
{
    max30102_adc_2048,
    max30102_adc_4096,
    max30102_adc_8192,
    max30102_adc_16384
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
 */
void max30102_set_sampling_rate(max30102_t *obj, max30102_sr_t sr);

/**
 * @brief Set led pulse width.
 *
 * @param obj Pointer to max30102_t object instance.
 * @param pw Pulse width enum (max30102_led_pw_t).
 */
void max30102_set_led_pulse_width(max30102_t *obj, max30102_led_pw_t pw);

/**
 * @brief Set ADC resolution.
 *
 * @param obj Pointer to max30102_t object instance.
 * @param adc ADC resolution enum (max30102_adc_t).
 */
void max30102_set_adc_resolution(max30102_t *obj, max30102_adc_t adc);

/**
 * @brief Set LED current.
 *
 * @param obj Pointer to max30102_t object instance.
 * @param ma LED current float (0 < ma < 51.0).
 */
void max30102_set_led_current_1(max30102_t *obj, float ma);

/**
 * @brief Set LED current.
 *
 * @param obj Pointer to max30102_t object instance.
 * @param ma LED current float (0 < ma < 51.0).
 */
void max30102_set_led_current_2(max30102_t *obj, float ma);

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
 * @brief
 *
 * @param obj Pointer to max30102_t object instance.
 * @param smp_ave
 * @param roll_over_en Roll over enabled(1) or disabled(0).
 * @param fifo_a_full Number of empty samples when A_FULL interrupt issued (0 < fifo_a_full < 15).
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
 */
void max30102_read_fifo_ver1(max30102_t *obj); //In ra data luon, khong tra ve gi ca

/**
 * @brief Ham nay doc gia tri FIFO roi tra ve so mau da doc
 * @retval `num_samples` - So mau doc duoc
 *
 * @param obj - Con tro tro toi doi tuong max30102_t
 * @param ir_buf - Buffer chua du lieu cua IR lay tu fifo
 * @param red_buf - Buffer chua du lieu cua RED lay tu fifo
 * @param max_samples - So mau toi da (Bang voi kich thuoc cua FIFO)
 */
uint16_t max30102_read_fifo_ver2(max30102_t *obj, max30102_record *record, uint32_t *ir_buf, uint32_t *red_buf, uint16_t max_samples);

/**
 * @brief
 */
int16_t max30102_read_fifo_ver3(max30102_t *obj, max30102_record *record, uint16_t max_samples);

/**
 * @brief
 */
uint32_t __attribute__((unused))Max30102_getFIFORed(max30102_record *record);

/**
 * @brief
 */
uint32_t __attribute__((unused))Max30102_getFIFOIR(max30102_record *record);

/**
 * @brief Ham kiem tra xem co bao nhieu sample dang ton tai

 */
int max30102_sample_available(max30102_record *record);

/**
 * @brief Ham doc tiep samples tu tail cua FIFO
 *
 */
void max30102_next_sample(max30102_record *record);

/**
 * @brief Read die temperature.
 *
 * @param obj Pointer to max30102_t object instance.
 * @param temp_int Pointer to store the integer part of temperature. Stored in 2's complement format.
 * @param temp_frac Pointer to store the fractional part of temperature. Increments of 0.0625 deg C.
 */
void max30102_read_temp(max30102_t *obj, int8_t *temp_int, uint8_t *temp_frac);

#endif

#ifdef __cplusplus
}
#endif
