/**
 * @file max30102_lib.c
 * @author LuongHuuPhuc
 */

#include "max30102_lib.h"
#include "stdio.h"
#include "max30102_low_level.h"

#ifdef __cplusplus
extern "C"
{
#endif

__attribute__((weak)) void max30102_plot(uint32_t ir_sample, uint32_t red_sample)
{
    UNUSED(ir_sample);
    UNUSED(red_sample);
}


void max30102_init(max30102_t *obj, I2C_HandleTypeDef *hi2c)
{
    obj->_ui2c = hi2c;
    obj->_interrupt_flag = 0;
    memset(obj->_ir_samples, 0, MAX30102_SAMPLE_LEN_MAX * sizeof(uint32_t));
    memset(obj->_red_samples, 0, MAX30102_SAMPLE_LEN_MAX * sizeof(uint32_t));
}


void max30102_reset(max30102_t *obj)
{
    uint8_t val = 0x40;
    max30102_write(obj, MAX30102_MODE_CONFIG, &val, 1);
}

void max30102_set_a_full(max30102_t *obj, uint8_t enable)
{
    uint8_t reg = 0;
    max30102_read(obj, MAX30102_INTERRUPT_ENABLE_1, &reg, 1);
    reg &= ~(0x01 << MAX30102_INTERRUPT_A_FULL);
    reg |= ((enable & 0x01) << MAX30102_INTERRUPT_A_FULL);
    max30102_write(obj, MAX30102_INTERRUPT_ENABLE_1, &reg, 1);
}

void max30102_set_ppg_rdy(max30102_t *obj, uint8_t enable)
{
    uint8_t reg = 0;
    max30102_read(obj, MAX30102_INTERRUPT_ENABLE_1, &reg, 1);
    reg &= ~(0x01 << MAX30102_INTERRUPT_PPG_RDY);
    reg |= ((enable & 0x01) << MAX30102_INTERRUPT_PPG_RDY);
    max30102_write(obj, MAX30102_INTERRUPT_ENABLE_1, &reg, 1);
}

void max30102_set_alc_ovf(max30102_t *obj, uint8_t enable)
{
    uint8_t reg = 0;
    max30102_read(obj, MAX30102_INTERRUPT_ENABLE_1, &reg, 1);
    reg &= ~(0x01 << MAX30102_INTERRUPT_ALC_OVF);
    reg |= ((enable & 0x01) << MAX30102_INTERRUPT_ALC_OVF);
    max30102_write(obj, MAX30102_INTERRUPT_ENABLE_1, &reg, 1);
}

void max30102_set_die_temp_rdy(max30102_t *obj, uint8_t enable)
{
    uint8_t reg = (enable & 0x01) << MAX30102_INTERRUPT_DIE_TEMP_RDY;
    max30102_write(obj, MAX30102_INTERRUPT_ENABLE_2, &reg, 1);
}


void max30102_set_die_temp_en(max30102_t *obj, uint8_t enable)
{
    uint8_t reg = (enable & 0x01) << MAX30102_DIE_TEMP_EN;
    max30102_write(obj, MAX30102_DIE_TEMP_CONFIG, &reg, 1);
}


void max30102_on_interrupt(max30102_t *obj)
{
    obj->_interrupt_flag = 1;
}


uint8_t max30102_has_interrupt(max30102_t *obj)
{
    return obj->_interrupt_flag;
}


void max30102_interrupt_handler(max30102_t *obj)
{
    uint8_t reg[2] = {0x00};
    // Interrupt flag in registers 0x00 and 0x01 are cleared on read
    max30102_read(obj, MAX30102_INTERRUPT_STATUS_1, reg, 2);

    if ((reg[0] >> MAX30102_INTERRUPT_A_FULL) & 0x01)
    {
        // FIFO almost full
        max30102_read_fifo_ver1(obj);
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

void max30102_shutdown(max30102_t *obj, uint8_t shdn)
{
    uint8_t config;
    max30102_read(obj, MAX30102_MODE_CONFIG, &config, 1);
    config = (config & 0x7f) | (shdn << MAX30102_MODE_SHDN);
    max30102_write(obj, MAX30102_MODE_CONFIG, &config, 1);
}


void max30102_set_led_mode(max30102_t *obj, max30102_record *record, max30102_mode_t mode)
{
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

void max30102_set_sampling_rate(max30102_t *obj, max30102_sr_t sr)
{
    uint8_t config;
    max30102_read(obj, MAX30102_SPO2_CONFIG, &config, 1);

    // Clear [4:2] bits
    config &= ~(0x7 << MAX30102_SPO2_SR);

    // Ghi gia tri moi
    config |= (sr << MAX30102_SPO2_SR);
    max30102_write(obj, MAX30102_SPO2_CONFIG, &config, 1);
}


void max30102_set_led_pulse_width(max30102_t *obj, max30102_led_pw_t pw)
{
    uint8_t config;
    max30102_read(obj, MAX30102_SPO2_CONFIG, &config, 1);

    // Clear bit [1:0]
    config &= ~(0x3 << MAX30102_SPO2_LEW_PW);

    // Ghi lai gia tri moi
    config |= (pw << MAX30102_SPO2_LEW_PW);
    max30102_write(obj, MAX30102_SPO2_CONFIG, &config, 1);
}


void max30102_set_adc_resolution(max30102_t *obj, max30102_adc_t adc)
{
    uint8_t config;
    max30102_read(obj, MAX30102_SPO2_CONFIG, &config, 1);

    // Clear bit [6:5]
    config &= ~(0x3 << MAX30102_SPO2_ADC_RGE);

    // Ghi gia tri moi vao thanh ghi
    config |= (adc << MAX30102_SPO2_ADC_RGE);
    max30102_write(obj, MAX30102_SPO2_CONFIG, &config, 1);
}


void max30102_set_led_current_ir(max30102_t *obj, float ma)
{
	if(ma < 0) ma = 0;
	if(ma > 51.0f) ma = 51.0f;

    uint8_t pa = (uint8_t)(ma / 0.2f); // Chuyen sang gia tri thanh ghi (0 - 255)
    max30102_write(obj, MAX30102_LED_IR_PA1, &pa, 1);
}


void max30102_set_led_current_red(max30102_t *obj, float ma)
{
	if(ma < 0) ma = 0;
	if(ma > 51.0f) ma = 51.0f;

	uint8_t pa = (uint8_t)(ma / 0.2f); // Chuyen sang gia tri thanh ghi (0 - 255)
	max30102_write(obj, MAX30102_LED_RED_PA2, &pa, 1);
}


void max30102_set_multi_led_slot_1_2(max30102_t *obj, max30102_multi_led_ctrl_t slot1, max30102_multi_led_ctrl_t slot2)
{
    uint8_t val = 0;
    val |= ((slot1 << MAX30102_MULTI_LED_CTRL_SLOT1) | (slot2 << MAX30102_MULTI_LED_CTRL_SLOT2));
    max30102_write(obj, MAX30102_MULTI_LED_CTRL_1, &val, 1);
}


void max30102_set_multi_led_slot_3_4(max30102_t *obj, max30102_multi_led_ctrl_t slot3, max30102_multi_led_ctrl_t slot4)
{
    uint8_t val = 0;
    val |= ((slot3 << MAX30102_MULTI_LED_CTRL_SLOT3) | (slot4 << MAX30102_MULTI_LED_CTRL_SLOT4));
    max30102_write(obj, MAX30102_MULTI_LED_CTRL_2, &val, 1);
}

void max30102_read_temp(max30102_t *obj, int8_t *temp_int, uint8_t *temp_frac)
{
    max30102_read(obj, MAX30102_DIE_TINT, (uint8_t *)temp_int, 1);
    max30102_read(obj, MAX30102_DIE_TFRAC, temp_frac, 1);
}

void max30102_set_fifo_config(max30102_t *obj, max30102_smp_ave_t smp_ave, uint8_t roll_over_en, uint8_t fifo_a_full)
{
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


HAL_StatusTypeDef max30102_clear_fifo(max30102_t *obj)
{
    uint8_t val = 0x00;
    max30102_write(obj, MAX30102_FIFO_WR_PTR, &val, 3);
    max30102_write(obj, MAX30102_FIFO_RD_PTR, &val, 3);
    max30102_write(obj, MAX30102_OVF_COUNTER, &val, 3);

    return HAL_OK;
}


void max30102_read_fifo_ver1(max30102_t *obj)
{
    // First transaction: Get the FIFO_WR_PTR
    uint8_t wr_ptr = 0, rd_ptr = 0;
    max30102_read(obj, MAX30102_FIFO_WR_PTR, &wr_ptr, 1);
    max30102_read(obj, MAX30102_FIFO_RD_PTR, &rd_ptr, 1);

    int8_t num_samples = (wr_ptr - rd_ptr) & 0x1F; // Bit mask

    if (num_samples < 1)
    {
        num_samples += 32;
    }

    // Second transaction: Read NUM_SAMPLES_TO_READ samples from the FIFO
    for (int8_t i = 0; i < num_samples; i++)
    {
        uint8_t sample[6];
        max30102_read(obj, MAX30102_FIFO_DATA, sample, 6);
        uint32_t ir_sample = ((uint32_t)(sample[0] << 16) | (uint32_t)(sample[1] << 8) | (uint32_t)(sample[2])) & 0x3ffff;
        uint32_t red_sample = ((uint32_t)(sample[3] << 16) | (uint32_t)(sample[4] << 8) | (uint32_t)(sample[5])) & 0x3ffff;
        obj->_ir_samples[i] = ir_sample;
        obj->_red_samples[i] = red_sample;
    }
}


uint16_t __attribute__((unused))max30102_read_fifo_ver2_1(max30102_t *obj, max30102_record *record, uint32_t *ir_buf, uint32_t *red_buf, uint16_t max_samples){
	uint8_t wr_ptr = 0; // Vi tri ghi tiep theo - khi tang den 31 roi ghi tiep, no se vong ve 0
	uint8_t rd_ptr = 0; // Vi tri doc tiep theo -> Khong tang thi khong doc duoc data

	MAXLOWLEVELCHECKFUNC(max30102_read(obj, MAX30102_FIFO_WR_PTR, &wr_ptr, 1));
	MAXLOWLEVELCHECKFUNC(max30102_read(obj, MAX30102_FIFO_RD_PTR, &rd_ptr, 1));

	// Neu wr_ptr == rd_ptr thi num_sample = 0
	uart_printf("[DEBUG] wr_ptr = %u, rd_ptr = %u\r\n", wr_ptr, rd_ptr);

	// Tinh so mau trong FIFO - Doc 32 sample tu FIFO can 5-bits (2^5 = 32 samples)
	uint8_t num_samples;
	if(wr_ptr >= rd_ptr){
		num_samples = (uint8_t)(wr_ptr - rd_ptr);
	}else{
		num_samples = 32 + wr_ptr - rd_ptr;
	}

	if(num_samples > max_samples) num_samples = max_samples;
	if(num_samples == 0) return 0;

	const uint8_t bytesPerSample = record->activeLeds * MAX30102_BYTES_PER_LED;

	// Tong so byte can doc cho 2 kenh IR va RED (FIFO co the chua toi 192 bytes cho 32 sample, moi sample 6 bytes)
//	uint16_t TotalBytesToRead = (uint16_t)(bytesPerSample * num_samples);

	uint8_t dataRead[bytesPerSample]; // Array luu gia tri 1 sample (6 bytes)
	for(uint8_t i = 0; i < num_samples; i++){

		// Doc 6 byte 1 lan
		MAXLOWLEVELCHECKFUNC(max30102_read(obj, MAX30102_FIFO_DATA, dataRead, bytesPerSample));

		// Tach tung byte raw data sample thanh mau 3 byte cho IR va 3 byte cho RED
		if(record->activeLeds > 0){ // RED (3 bytes = 24-bits -> Res 18-bits nen bo di 6 bit)
			red_buf[i] = ((uint32_t)(dataRead[0] << 16) |   // Byte 1
						 (uint32_t)(dataRead[1] << 8) | 	// Byte 2
						 (uint32_t)(dataRead[2]))       	// Byte 3
						 & 0x3FFFF; // bit mask 18-bit
		}

		if(record->activeLeds > 1){ // IR (3 bytes = 24-bits -> Res 18-bits nen bo di 6 bit)
			ir_buf[i] = ((uint32_t)(dataRead[3] << 16) | // Byte 1
						(uint32_t)(dataRead[4] << 8) |   // Byte 2
						(uint32_t)(dataRead[5])) 		 // Byte 3
						& 0x3FFFF; // bit mask 18-bit
		}
	}

	return num_samples; //Tra ve so mau da doc
}

uint16_t max30102_read_fifo_ver2_2(max30102_t *obj, max30102_record *record, uint32_t *ir_buf, uint32_t *red_buf, uint16_t max_samples){
	uint8_t wr_ptr = 0, rd_ptr = 0;
	MAXLOWLEVELCHECKFUNC(max30102_read(obj, MAX30102_FIFO_WR_PTR, &wr_ptr, 1));
	MAXLOWLEVELCHECKFUNC(max30102_read(obj, MAX30102_FIFO_RD_PTR, &rd_ptr, 1));

//	uart_printf("[DEBUG] wr_ptr = %u, rd_ptr = %u\r\n", wr_ptr, rd_ptr);

	//Tinh so mau trong FIFO
	int16_t num_samples = (int16_t)(wr_ptr - rd_ptr) & 0x1F; //FIFO depth = 32

	if(num_samples < 1) num_samples += 32;
	if(num_samples > max_samples) num_samples = max_samples;
	if(num_samples == 0) return 0;
	if(num_samples > 0){
		uint16_t bytesToRead = (uint16_t)(record->activeLeds * MAX30102_BYTES_PER_LED * num_samples);

		if(bytesToRead > 0){
			uint8_t data_temp[bytesToRead];

			//Doc 1 lan toan bo du lieu trong FIFO thay vi doc tung lan
			MAXLOWLEVELCHECKFUNC(max30102_read(obj, MAX30102_FIFO_DATA, data_temp, bytesToRead));

			//Chuyen tung mau raw data thanh mau IR va RED
			for(int8_t i = 0; i < num_samples; i++){
				int index = i * record->activeLeds * MAX30102_BYTES_PER_LED;

				//Nhay qua tung mau (voi i = 0, 1,...,31)
				if(record->activeLeds >= 1){ //RED
					red_buf[i] = ((uint32_t)(data_temp[index] << 16) |
								(uint32_t)(data_temp[index + 1] << 8) |
								(uint32_t)(data_temp[index + 2])) & 0x3FFFF;

				}
				if(record->activeLeds >= 2){ //RED + IR
					ir_buf[i] = ((uint32_t)(data_temp[index + 3] << 16) |
								(uint32_t)(data_temp[index + 4] << 8) |
								(uint32_t)(data_temp[index + 5])) & 0x3FFFF; //18-bit data
				}
			}
		}
	}
	return num_samples; //Tra ve so mau da doc
}

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

int16_t __attribute__((unused))max30102_read_fifo_ver3(max30102_t *obj, max30102_record *record, uint16_t max_samples){
	uint8_t wr_ptr = 0, rd_ptr = 0;
	max30102_read(obj, MAX30102_FIFO_WR_PTR, &wr_ptr, 1);
	max30102_read(obj, MAX30102_FIFO_RD_PTR, &rd_ptr, 1);

	int16_t num_samples = (int16_t)(wr_ptr - rd_ptr) & 0x1F; //So sample muon doc tu FIFO

	if(num_samples < 0) num_samples += 32;
	if(num_samples > max_samples) num_samples = max_samples;

	if(num_samples > 0){
		//Doc so byte cua IR va RED (3 bytes moi LED)
		int bytesLeftToRead = (int)(num_samples * record->activeLeds * MAX30102_BYTES_PER_LED);

		//FIFO co the chua toi da 32 samples
		//Neu 3 led -> 32 * 3 * 3 = 288 bytes
		//Nhung I2C tren nhieu MCU chi cho phep doc toi da 32 bytes moi lan -> Chi nho thanh nhieu block
		while(bytesLeftToRead > 0){
			int toGet = bytesLeftToRead;
			if(toGet > 32){
				//Neu toGet > 32 bytes thi voi RED + IR (6 bytes 1 lan) -> 32 % 6 = thua 2 bytes
				//Neu toGet > 32 bytes thi voi RED + IR + GREEN (9 bytes 1 lan) -> 32 % 9 = thua 5 bytes
				//Vi vay phai giam xuong boi so gan nhat cua 3 * so LED -> 2 led = 30 bytes, 3 led = 27 bytes
				toGet = 32  - (32 % (MAX30102_BYTES_PER_LED * record->activeLeds)); //So bytes se lay (30 bytes not 32 bytes)
			}

			bytesLeftToRead -= toGet; //So bytes thua (2 bytes)
			uint8_t data_temp[toGet]; //Mang dung de convert thanh Red va IR

			//Read bytes
			max30102_read(obj, MAX30102_FIFO_DATA, data_temp, toGet);

			uint8_t read_count = 0;

			while(toGet > 0){
				record->head++;
				record->head %= MAX30102_STORAGE_SIZE;

				uint8_t temp[sizeof(uint32_t)]; //Mang 4 bytes
				uint32_t tempLong;

				temp[3] = 0; //Bytes cuoi khong co nghia
				temp[2] = data_temp[read_count++];
				temp[1] = data_temp[read_count++];
				temp[0] = data_temp[read_count++];

				//Convert mang thanh long
				memcpy(&tempLong, temp, sizeof(tempLong));

				tempLong &= 0x3FFFF; //Padding chi chua lai 18-bit (bit-mask)

				record->red_sample[record->head] = tempLong; //Ghi data vao head, lay data tu tail (FIFO)

				//Neu co tu 1 led tro len (red)
				if(record->activeLeds > 1){
					temp[3] = 0;
					temp[2] = data_temp[read_count++];
					temp[1] = data_temp[read_count++];
					temp[0] = data_temp[read_count++];

					memcpy(&tempLong, temp, sizeof(tempLong));

					tempLong &= 0x3FFFF;

					record->ir_sample[record->head] = tempLong;//Ghi data vao head, lay data tu tail (FIFO)
				}

				//Neu co hon 2 led (red + ir)
				if(record->activeLeds > 2){
					temp[3] = 0;
					temp[2] = data_temp[read_count++];
					temp[1] = data_temp[read_count++];
					temp[0] = data_temp[read_count++];

					memcpy(&tempLong, temp, sizeof(tempLong));

					tempLong &= 0x3FFFF;

					record->green_sample[record->head] = tempLong;//Ghi data vao head, lay data tu tail (FIFO)
				}

				toGet -= record->activeLeds * MAX30102_BYTES_PER_LED; //Tru di so byte da doc
			}
		}
	}

	return num_samples;
}

int __attribute__((unused))max30102_ver3_sample_available(max30102_record *record){
	int numberOfSamples = record->head - record->tail;
	if(numberOfSamples < 0){
		numberOfSamples += MAX30102_STORAGE_SIZE;
	}
	return numberOfSamples;
}


void __attribute__((unused))max30102_ver3_next_sample(max30102_record *record){
	if(max30102_ver3_sample_available(record)){
		record->tail++;
		record->tail %= MAX30102_STORAGE_SIZE;
	}
}

uint32_t __attribute__((unused))max30102_ver3_getFIFORed(max30102_record *record){
	return (record->red_sample[record->tail]);
}

uint32_t __attribute__((unused))Max30102_ver3_getFIFOIR(max30102_record *record){
	return (record->ir_sample[record->tail]);
}

#ifdef __cplusplus
}
#endif
