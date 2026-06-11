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

uint8_t max30102_read_overflow_counter(max30102_t *obj){
	uint8_t ovf_out = 0; /* Bien check FIFO overflow (5-bits: 0 -> 31) */

	// Gia tri thanh ghi OVF_COUNTER la 5-bits (dem tu 0 -> 31 neu phat hien overflow)
	max30102_read(obj, MAX30102_OVF_COUNTER, &ovf_out, 1);
	return ovf_out;
}

/*-----------------------------------------------------------*/

void max30102_clear_fifo(max30102_t *obj){
    uint8_t val = 0x00;
    max30102_write(obj, MAX30102_FIFO_WR_PTR, &val, 1);
    max30102_write(obj, MAX30102_FIFO_RD_PTR, &val, 1);
    max30102_write(obj, MAX30102_OVF_COUNTER, &val, 1);
}

/*-----------------------------------------------------------*/

uint8_t max30102_get_write_pointer(max30102_t *obj){
	uint8_t data = 0;
	MAXLOWLEVELCHECKFUNC(max30102_read(obj, MAX30102_FIFO_WR_PTR, &data, 1));
	return data;
}

/*-----------------------------------------------------------*/

uint8_t max30102_get_read_pointer(max30102_t *obj){
	uint8_t data = 0; 	/* Bien doc - doc du lieu hien tai cua MCU */
	MAXLOWLEVELCHECKFUNC(max30102_read(obj, MAX30102_FIFO_RD_PTR, &data, 1));
	return data;
}

/*-----------------------------------------------------------*/

void max30102_burst_read_reg(max30102_t *obj, uint8_t from_reg, uint8_t n, uint8_t *out_buf){
	MAXLOWLEVELCHECKFUNC(max30102_read(obj, from_reg, out_buf, n));
}

/*-----------------------------------------------------------*/

uint16_t max30102_fifo_available(max30102_t *obj, uint8_t *wr, uint8_t *rd){
	// uint8_t wr_ptr = max30102_get_write_pointer(obj); /* Con tro ghi - ghi du lieu moi nhat cua cam bien vao FIFO */
	// uint8_t rd_ptr = max30102_get_read_pointer(obj); /* Con tro doc - doc du lieu hien tai cua MCU */

	/* Burst Read doc luon 1 lan 3 thanh ghi lien tiep tai tu WR_PTR 0x04 thay vi doc tung cai nhu tren */
	uint8_t fifo_reg[3] = {0};
	max30102_burst_read_reg(obj, MAX30102_FIFO_WR_PTR, 3, fifo_reg);

	uint8_t wr_ptr = fifo_reg[0];
	// uint8_t ovf_count = fifo_reg[1]; // Neu muon doc Overflow counter
	uint8_t rd_ptr = fifo_reg[2];

	if(wr != NULL) *wr = wr_ptr;
	if(rd != NULL) *rd = rd_ptr;

	/**
	 * wr_ptr == rd_ptr co the co 2 TH: 1 la FIFO empty, 2 la FIFO full
	 * Nen khong the mac dinh de num_samples doc duoc tu FIFO = 32
	 * Day chinh la van de cua circular buffer. Nhung he thong nay,
	 * neu luc 2 con tro doc-ghi bang nhau ma khong set no bang 32 ma
	 * lai tinh bang phep tru roi bit mask -> Loi ngay lap tuc
	 * -> Chung to moi lan wr_ptr = rd_ptr la luc FIFO day !
	 */
	return (wr_ptr == rd_ptr) ? 32 : ((int16_t)(wr_ptr - rd_ptr) & 0x1F);
	// return (uint16_t)((wr_ptr - rd_ptr) & 0x1F); // FIFO depth = 32 (dung bit mask nhanh hon)
}

/*-----------------------------------------------------------*/

uint16_t max30102_read_fifo(max30102_t *obj, max30102_record *record, uint16_t max_samples){
	int16_t num_samples = 0;	/* Bien luu tong so mau trong FIFO */

#ifdef DEBUG_SWV_ITM
	int16_t actual_samples = 0;
	uint8_t rd_ptr_before = 0;
	uint8_t wr_ptr_before = 0;
	num_samples = max30102_fifo_available(obj, &wr_ptr_before, &rd_ptr_before);
#else
	num_samples = max30102_fifo_available(obj, NULL, NULL);
#endif // DEBUG_SWV_ITM

	/* Neu khong doc duoc sample nao -> return 0 luon */
	if(num_samples == 0) return 0;

	/* Wrap condition (mac du dung bit mask 0x1F thi khong am) */
	if(num_samples < 0) num_samples += 32;

	/* Neu samples doc duoc lon hon 32 */
	if(num_samples > max_samples) num_samples = max_samples;

	/* Neu sample doc duoc gia tri co nghia (khoang 0->31) */
	if(num_samples > 0){
		uint16_t bytesToRead = (uint16_t)(record->activeLeds * MAX30102_BYTES_PER_LED * num_samples); // max 192 bytes

		if(bytesToRead > 0){
			uint8_t data_temp[bytesToRead];

			// Doc 1 lan toan bo du lieu trong FIFO thay vi doc tung lan
			MAXLOWLEVELCHECKFUNC(max30102_read(obj, MAX30102_FIFO_DATA, data_temp, bytesToRead));

#ifdef DEBUG_SWV_ITM
			/* Doc lai rd_ptr sau khi I2C read xong FIFO de biet thuc su da doc duoc bao nhieu */
			uint8_t rd_ptr_after = max30102_get_read_pointer(obj);

			/* Tinh so samples thuc su da doc duoc */
			actual_samples = ((int16_t)(rd_ptr_after - rd_ptr_before) & 0x1F);
			if(actual_samples == 0) actual_samples = 32;
			if(actual_samples > max_samples) actual_samples = max_samples;

			uint8_t ovf = max30102_read_overflow_counter(obj); 	/* Doc thanh ghi OVF_COUNTER 0x05 */

			/* Debug su dung SWV thay cho UART khi truyen Binary Packet */
			SWV_LOG("[DEBUG] wr_ptr_before=%u, rd_ptr_before=%u, rd_ptr_after=%u, ovf=%u, calc=%d, actual=%d\r\n",
					wr_ptr_before, rd_ptr_before, rd_ptr_after, ovf, num_samples, actual_samples);
#endif // DEBUG_SWV_ITM

			/**
			 * DEBUG NOTE:
			 * He thong chay muọt, binh thuong nhung dot nhien con tro doc nhay loan:
			 * [DEBUG] wr_ptr_before=26, rd_ptr_before=25, rd_ptr_after=26, ovf=0, calc=1, actual=1
			 * [DEBUG] wr_ptr_before=25, rd_ptr_before=26, rd_ptr_after=25, ovf=0, calc=31, actual=31
			 * [DEBUG] wr_ptr_before=25, rd_ptr_before=25, rd_ptr_after=30, ovf=0, calc=32, actual=5
			 * [DEBUG] wr_ptr_before=25, rd_ptr_before=30, rd_ptr_after=25, ovf=0, calc=27, actual=27
			 * -> Kha nang phan cung bi Clock drift cho MCU dung clock tu thach anh con MAX dung internall oscillator (RC)
			 * nen co sai so lon. O tan so 1000Hz (1ms/sample), su lech pha giua 2 clock tich luy dan sau moi chu ky 32ms
			 * Cu sau 1 thoi gian, pha cua MCU va cam bien trung khit (Race Condition). Cam bien nap mau moi de len len
			 * con tro ngay luc MCU chuan bi doc I2C, gay va cham va nhay loan xa con tro.
			 * -> Ket qua: Phan cung bao hut mau dot ngot (tut xuong num_sample = 1, sau do lai vot len 27 de xa bo dem)
			 *
			 * Giai quyet: Software Padding samples
			 * - Tuan thu phan cung: I2C chi doc dung so byte chip dang bao (khong ep/ghi de thanh ghi bay ba)
			 * - Cuu luong Sync: Ep vong lap giai ma luon chay du 32 samples. Neu phan cung bi hut do lech pha
			 * phan mem se tu dong kich hoat co che Padding (sao chep mau lien truoc vao cho trong)
			 * - Ham luon return 32 de giu block size co dinh, triet tieu hoan toan loi Drop sample
			 */

			// Chuyen tung mau raw data thanh mau IR va RED
			for(int8_t i = 0; i < MAX30102_SAMPLE_LEN_MAX; i++){
				if(i < num_samples){
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

				/* Neu bi thieu mau (i >= num_sample): Copy cac mau lien truoc de dap vao o trong (Padding) */
				else{
					if(i > 0){
						obj->_red_samples[i] = obj->_red_samples[i - 1];
						obj->_ir_samples[i] = obj->_ir_samples[i - 1];
					}
				}
			}
		}
	}
	return MAX30102_SAMPLE_LEN_MAX; //Tra ve so mau da doc
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
