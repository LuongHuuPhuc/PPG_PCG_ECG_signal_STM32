/**
 * @author Luong Huu Phuc
 * @date 2025/07/19
 *
 * @note Thu vien dung de loc FIR lowpass cho PCG truoc khi downsample
 * \note - Chong Aliasing (rang cua)
 * \note - Dung theo nguyen tac Nyquist-Shannon sampling theorem: Truoc khi downsample tan so lay mau,
 * can loc thong thap tin hieu de loai bo tan so lon hon tan so Nyquist
 * \note - Khi downsample tu 8000Hz -> 1000Hz (giam 8 lan), dai tan giu lai nen nho hon 500Hz (1 nua cua 1000Hz)
 * va can loc bot tin hieu cao honw 500Hz truoc khi decimate de tranh Aliasing (Theo dinh ly Nyquist)
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "FIR_Filter.h"

const float fir_coeff[FIR_TAP_NUM] = {
		-0.000802, -0.000840, -0.000811, -0.000678, -0.000391, 0.000096, 0.000801, 0.001692, 0.002670,
		0.003569, 0.004166, 0.004211, 0.003476, 0.001807, -0.000821, -0.004262, -0.008174, -0.012027,
		-0.015139, -0.016750, -0.016113, -0.012601, -0.005813, 0.004338, 0.017573, 0.033233, 0.050321,
		0.067587, 0.083650, 0.097148, 0.106890, 0.111995, 0.111995, 0.106890, 0.097148, 0.083650, 0.067587,
		0.050321, 0.033233, 0.017573, 0.004338, -0.005813, -0.012601, -0.016113, -0.016750, -0.015139,
		-0.012027, -0.008174, -0.004262, -0.000821, 0.001807, 0.003476, 0.004211, 0.004166, 0.003569,
		0.002670, 0.001692, 0.000801, 0.000096, -0.000391, -0.000678, -0.000811, -0.000840, -0.000802 };


static float fir_buffer[FIR_TAP_NUM] = {0};
static uint16_t fir_idx = 0;

float FIR_Filter(float input){
	fir_buffer[fir_idx++] = input; //Ghi vao buffer truoc, xu ly index sau
	if(fir_idx >= FIR_TAP_NUM){
		fir_idx = 0;
	}

	float output = 0.0f;
	uint16_t buf_idx = fir_idx;

	for(uint16_t i = 0; i < FIR_TAP_NUM; i++){
		if(buf_idx == 0){
			buf_idx = FIR_TAP_NUM;
		}
		buf_idx--;
		output += fir_buffer[buf_idx] * fir_coeff[i];
	}
	return output;
}

#ifdef __cplusplus
}
#endif

