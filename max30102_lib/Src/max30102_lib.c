/**
 * @file max30102_lib.c
 * @author LuongHuuPhuc
 */

#include "max30102_lib.h"

#include "stdio.h"
#include <string.h>
#include "max30102_low_level.h"

#ifdef DEBUG_SWV_ITM
#include "SWV_debug.h"
#endif // DEBUG_SWV_ITM

#ifdef __cplusplus
extern "C"
{
#endif

/*-----------------------------------------------------------*/

__attribute__((weak)) void max30102_plot(uint32_t ir_sample, uint32_t red_sample){
    UNUSED(ir_sample);
    UNUSED(red_sample);
}

/*-----------------------------------------------------------*/

void max30102_init(max30102_t *obj, I2C_HandleTypeDef *hi2c){
    obj->_ui2c = hi2c;
    obj->_interrupt_flag = 0;
    memset(obj->_ir_samples, 0, MAX30102_SAMPLE_LEN_MAX * sizeof(uint32_t));
    memset(obj->_red_samples, 0, MAX30102_SAMPLE_LEN_MAX * sizeof(uint32_t));
}

/*-----------------------------------------------------------*/

void max30102_reset(max30102_t *obj){
    uint8_t val = 0x40;
    max30102_write(obj, MAX30102_MODE_CONFIG, &val, 1);
}

/*-----------------------------------------------------------*/

void max30102_set_a_full(max30102_t *obj, uint8_t enable){
    uint8_t reg = 0;
    max30102_read(obj, MAX30102_INTERRUPT_ENABLE_1, &reg, 1);
    reg &= ~(0x01 << MAX30102_INTERRUPT_A_FULL);
    reg |= ((enable & 0x01) << MAX30102_INTERRUPT_A_FULL);
    max30102_write(obj, MAX30102_INTERRUPT_ENABLE_1, &reg, 1);
}

/*-----------------------------------------------------------*/

void max30102_set_ppg_rdy(max30102_t *obj, uint8_t enable){
    uint8_t reg = 0;
    max30102_read(obj, MAX30102_INTERRUPT_ENABLE_1, &reg, 1);
    reg &= ~(0x01 << MAX30102_INTERRUPT_PPG_RDY);
    reg |= ((enable & 0x01) << MAX30102_INTERRUPT_PPG_RDY);
    max30102_write(obj, MAX30102_INTERRUPT_ENABLE_1, &reg, 1);
}

/*-----------------------------------------------------------*/

void max30102_set_alc_ovf(max30102_t *obj, uint8_t enable){
    uint8_t reg = 0;
    max30102_read(obj, MAX30102_INTERRUPT_ENABLE_1, &reg, 1);
    reg &= ~(0x01 << MAX30102_INTERRUPT_ALC_OVF);
    reg |= ((enable & 0x01) << MAX30102_INTERRUPT_ALC_OVF);
    max30102_write(obj, MAX30102_INTERRUPT_ENABLE_1, &reg, 1);
}

/*-----------------------------------------------------------*/

void max30102_set_die_temp_rdy(max30102_t *obj, uint8_t enable){
    uint8_t reg = (enable & 0x01) << MAX30102_INTERRUPT_DIE_TEMP_RDY;
    max30102_write(obj, MAX30102_INTERRUPT_ENABLE_2, &reg, 1);
}

/*-----------------------------------------------------------*/

void max30102_set_die_temp_en(max30102_t *obj, uint8_t enable){
    uint8_t reg = (enable & 0x01) << MAX30102_DIE_TEMP_EN;
    max30102_write(obj, MAX30102_DIE_TEMP_CONFIG, &reg, 1);
}

/*-----------------------------------------------------------*/

void max30102_on_interrupt(max30102_t *obj){
    obj->_interrupt_flag = 1;
}

/*-----------------------------------------------------------*/

uint8_t max30102_has_interrupt(max30102_t *obj){
    return obj->_interrupt_flag;
}

/*-----------------------------------------------------------*/

void max30102_interrupt_handler(max30102_t *obj, max30102_record *record){
    uint8_t reg[2] = {0x00};
    // Interrupt flag in registers 0x00 and 0x01 are cleared on read
    max30102_read(obj, MAX30102_INTERRUPT_STATUS_1, reg, 2);

    if ((reg[0] >> MAX30102_INTERRUPT_A_FULL) & 0x01)
    {
        // FIFO almost full
    	max30102_read_fifo_on_interrupt(obj, record);
    }

    if ((reg[0] >> MAX30102_INTERRUPT_PPG_RDY) & 0x01)
    {
        // New FIFO data ready
    }

    if ((reg[0] >> MAX30102_INTERRUPT_ALC_OVF) & 0x01)
    {
        // Ambient light overflow
    }

    if ((reg[1] >> MAX30102_INTERRUPT_DIE_TEMP_RDY) & 0x01)
    {
        // Temperature data ready
        int8_t temp_int;
        uint8_t temp_frac;
        max30102_read_temp(obj, &temp_int, &temp_frac);
        // float temp = temp_int + 0.0625f * temp_frac;
    }

    // Reset interrupt flag
    obj->_interrupt_flag = 0;
}

/*-----------------------------------------------------------*/

void max30102_shutdown(max30102_t *obj, uint8_t shdn){
    uint8_t config;
    max30102_read(obj, MAX30102_MODE_CONFIG, &config, 1);
    config = (config & 0x7f) | (shdn << MAX30102_MODE_SHDN);
    max30102_write(obj, MAX30102_MODE_CONFIG, &config, 1);
}

/*-----------------------------------------------------------*/

void max30102_set_led_mode(max30102_t *obj, max30102_record *record, max30102_mode_t mode){
    uint8_t config;
    if(mode == max30102_spo2){
    	record->activeLeds = 2; //Chi dung RED va IR
    }else if(mode == max30102_multi_led){
    	record->activeLeds = 3; //Dung RED + IR + GREEN
    }else if(mode == max30102_heart_rate){
    	record->activeLeds = 1; //Chi dung RED
    }

	max30102_read(obj, MAX30102_MODE_CONFIG, &config, 1);
	config = (config & 0xf8) | mode;
	max30102_write(obj, MAX30102_MODE_CONFIG, &config, 1);
	max30102_clear_fifo(obj);
}

/*-----------------------------------------------------------*/

void max30102_set_sampling_rate(max30102_t *obj, max30102_sr_t sr){
    uint8_t config;
    max30102_read(obj, MAX30102_SPO2_CONFIG, &config, 1);

    // Clear [4:2] bits
    config &= ~(0x7 << MAX30102_SPO2_SR);

    // Ghi gia tri moi
    config |= (sr << MAX30102_SPO2_SR);
    max30102_write(obj, MAX30102_SPO2_CONFIG, &config, 1);
}

/*-----------------------------------------------------------*/

void max30102_set_led_pulse_width(max30102_t *obj, max30102_led_pw_t pw){
    uint8_t config;
    max30102_read(obj, MAX30102_SPO2_CONFIG, &config, 1);

    // Clear bit [1:0]
    config &= ~(0x3 << MAX30102_SPO2_LEW_PW);

    // Ghi lai gia tri moi
    config |= (pw << MAX30102_SPO2_LEW_PW);
    max30102_write(obj, MAX30102_SPO2_CONFIG, &config, 1);
}

/*-----------------------------------------------------------*/

void max30102_set_adc_resolution(max30102_t *obj, max30102_adc_t adc){
    uint8_t config;
    max30102_read(obj, MAX30102_SPO2_CONFIG, &config, 1);

    // Clear bit [6:5]
    config &= ~(0x3 << MAX30102_SPO2_ADC_RGE);

    // Ghi gia tri moi vao thanh ghi
    config |= (adc << MAX30102_SPO2_ADC_RGE);
    max30102_write(obj, MAX30102_SPO2_CONFIG, &config, 1);
}

/*-----------------------------------------------------------*/

void max30102_set_led_current_ir(max30102_t *obj, float ma){
	if(ma < 0) ma = 0;
	if(ma > 51.0f) ma = 51.0f;

    uint8_t pa = (uint8_t)(ma / 0.2f); // Chuyen sang gia tri thanh ghi (0 - 255)
    max30102_write(obj, MAX30102_LED_IR_PA1, &pa, 1);
}

/*-----------------------------------------------------------*/

void max30102_set_led_current_red(max30102_t *obj, float ma){
	if(ma < 0) ma = 0;
	if(ma > 51.0f) ma = 51.0f;

	uint8_t pa = (uint8_t)(ma / 0.2f); // Chuyen sang gia tri thanh ghi (0 - 255)
	max30102_write(obj, MAX30102_LED_RED_PA2, &pa, 1);
}

/*-----------------------------------------------------------*/

void max30102_set_multi_led_slot_1_2(max30102_t *obj, max30102_multi_led_ctrl_t slot1, max30102_multi_led_ctrl_t slot2){
    uint8_t val = 0;
    val |= ((slot1 << MAX30102_MULTI_LED_CTRL_SLOT1) | (slot2 << MAX30102_MULTI_LED_CTRL_SLOT2));
    max30102_write(obj, MAX30102_MULTI_LED_CTRL_1, &val, 1);
}

/*-----------------------------------------------------------*/

void max30102_set_multi_led_slot_3_4(max30102_t *obj, max30102_multi_led_ctrl_t slot3, max30102_multi_led_ctrl_t slot4){
    uint8_t val = 0;
    val |= ((slot3 << MAX30102_MULTI_LED_CTRL_SLOT3) | (slot4 << MAX30102_MULTI_LED_CTRL_SLOT4));
    max30102_write(obj, MAX30102_MULTI_LED_CTRL_2, &val, 1);
}

/*-----------------------------------------------------------*/

void max30102_read_temp(max30102_t *obj, int8_t *temp_int, uint8_t *temp_frac){
    max30102_read(obj, MAX30102_DIE_TINT, (uint8_t *)temp_int, 1);
    max30102_read(obj, MAX30102_DIE_TFRAC, temp_frac, 1);
}

/*-----------------------------------------------------------*/

void max30102_set_fifo_config(max30102_t *obj, max30102_smp_ave_t smp_ave, uint8_t roll_over_en, uint8_t fifo_a_full){
    uint8_t config = 0x00;

    // Ghi gia tri moi vao
    config |= (smp_ave & 0x07) << MAX30102_FIFO_CONFIG_SMP_AVE;
    config |= ((roll_over_en & 0x01) << MAX30102_FIFO_CONFIG_ROLL_OVER_EN);
    config |= ((fifo_a_full & 0x0f) << MAX30102_FIFO_CONFIG_FIFO_A_FULL);

    uart_printf("[SET FIFO CONFIG] FIFO before write: 0x%02X\r\n", config);
    max30102_write(obj, MAX30102_FIFO_CONFIG, &config, 1);

    max30102_read(obj, MAX30102_FIFO_CONFIG, &config, 1);
    uart_printf("[SET FIFO CONFIG] FIFO after write: 0x%02X\r\n", config);
}

/*-----------------------------------------------------------*/

void max30102_read_overflow_counter(max30102_t *obj, uint8_t ovf_out){
	// Gia tri thanh ghi OVF_COUNTER la 5-bits (dem tu 0 -> 31 neu phat hien overflow)
	max30102_read(obj, MAX30102_OVF_COUNTER, &ovf_out, 1);
}

/*-----------------------------------------------------------*/

HAL_StatusTypeDef max30102_clear_fifo(max30102_t *obj){
    uint8_t val = 0x00;
    max30102_write(obj, MAX30102_FIFO_WR_PTR, &val, 3);
    max30102_write(obj, MAX30102_FIFO_RD_PTR, &val, 3);
    max30102_write(obj, MAX30102_OVF_COUNTER, &val, 3);

    return HAL_OK;
}

/*-----------------------------------------------------------*/

uint16_t max30102_read_fifo(max30102_t *obj, max30102_record *record, uint16_t max_samples){
	uint8_t wr_ptr = 0; 	/* Bien ghi - ghi du lieu moi nhat cua cam bien vao FIFO */
	uint8_t rd_ptr = 0; 	/* Bien doc - doc du lieu hien tai cua MCU */
	int16_t num_samples;	/* Bien luu tong so mau trong FIFO */

	MAXLOWLEVELCHECKFUNC(max30102_read(obj, MAX30102_FIFO_WR_PTR, &wr_ptr, 1));
	MAXLOWLEVELCHECKFUNC(max30102_read(obj, MAX30102_FIFO_RD_PTR, &rd_ptr, 1));

#ifdef DEBUG_SWV_ITM
	uint8_t ovf = 0; /* Bien check FIFO overflow (5-bits: 0 -> 31) */
	max30102_read_overflow_counter(obj, ovf); 	/* Doc thanh ghi OVF_COUNTER 0x05 */

	/* Debug su dung SWV thay cho UART khi truyen Binary Packet */
	 SWV_LOG("[DEBUG] wr_ptr =%u, rd_ptr =%u, ovf=%u\r\n", wr_ptr, rd_ptr, ovf);
#endif

	/**
	 * Khi con tro ghi = con tro doc, co 2 truong hop:
	 * -> FIFO empty (0): Khi do MCU da doc sach toan bo du lieu truoc do, lam con tro doc duoi kip con tro ghi (Doc nhanh, Ghi khong kip)
	 * -> FIFO full (32): Khi do con tro ghi quay vong duoi kip con tro doc (Ghi nhanh, Doc khong kip)
	 * Neu cau hinh FIFO, set fifo_a_full = 0 -> FIFO empty = 0 moi interrupt doc FIFO.
	 * Neu MCU chua kip doc data ma cam bien tiep tuc day mau moi vao, cac mau cu se bi ghi de -> ovf se tang len (tu 0 -> 31)
	 * Trong he thong hien tai, bien nay chi co 2 gia tri "0" hoac "1" chu khong chay tu 0 -> 31.
	 * Tuc la khi ovf = 1 (bi overflow 1 sample) thi ngay lap tuc tro ve 0 -> Khong overflow qua nhieu
	 */
	 num_samples = (wr_ptr == rd_ptr) ? 32 : ((int16_t)(wr_ptr - rd_ptr) & 0x1F); // FIFO depth = 32 (dung bit mask nhanh hon)
	 // num_samples = (wr_ptr == rd_ptr) ? 32 : ((int16_t)(wr_ptr - rd_ptr + 32) % 32);

	/* Neu khong doc duoc sample nao -> return 0 luon */
	if(num_samples == 0) return 0;

	/* Neu samples doc duoc lon hon 32 */
	if(num_samples > max_samples) num_samples = max_samples;

	/* Neu sample doc duoc gia tri co nghia (khoang 0->31) */
	if(num_samples > 0){
		uint16_t bytesToRead = (uint16_t)(record->activeLeds * MAX30102_BYTES_PER_LED * num_samples);

		if(bytesToRead > 0){
			uint8_t data_temp[bytesToRead];

			// Doc 1 lan toan bo du lieu trong FIFO thay vi doc tung lan
			MAXLOWLEVELCHECKFUNC(max30102_read(obj, MAX30102_FIFO_DATA, data_temp, bytesToRead));

			// Chuyen tung mau raw data thanh mau IR va RED
			for(int8_t i = 0; i < num_samples; i++){
				int index = i * record->activeLeds * MAX30102_BYTES_PER_LED;

				// Nhay qua tung mau (voi i = 0, 1,...,31)
				if(record->activeLeds >= 1){ // RED
					obj->_red_samples[i] = ((uint32_t)(data_temp[index] << 16) |
								(uint32_t)(data_temp[index + 1] << 8) |
								(uint32_t)(data_temp[index + 2])) & 0x3FFFF;

				}
				if(record->activeLeds >= 2){ // RED + IR
					obj->_ir_samples[i] = ((uint32_t)(data_temp[index + 3] << 16) |
								(uint32_t)(data_temp[index + 4] << 8) |
								(uint32_t)(data_temp[index + 5])) & 0x3FFFF; //18-bit data
				}
			}
		}
	}
	return num_samples; //Tra ve so mau da doc
}

/*-----------------------------------------------------------*/

void max30102_config_register_status_verbose(max30102_t *obj){
	uint8_t mode_reg, spo2_reg, irled_reg, redled_reg, fifo_reg;
	max30102_read(obj, MAX30102_MODE_CONFIG, &mode_reg, 1); //Khong can kiem tra loi
	max30102_read(obj, MAX30102_SPO2_CONFIG, &spo2_reg, 1);
	max30102_read(obj, MAX30102_LED_IR_PA1, &irled_reg, 1);
	max30102_read(obj, MAX30102_LED_RED_PA2, &redled_reg, 1);
	max30102_read(obj, MAX30102_FIFO_CONFIG, &fifo_reg, 1);

	uart_printf("[DEBUG] MODE=0x%02X SPO2=0x%02X IRLED=0x%02X REDLED=0x%02X FIFO=0x%02X\r\n",
				mode_reg, spo2_reg, irled_reg, redled_reg, fifo_reg);
}

/*-----------------------------------------------------------*/

#ifdef __cplusplus
}
#endif
