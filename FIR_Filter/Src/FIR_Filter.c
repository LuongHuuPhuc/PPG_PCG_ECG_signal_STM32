/**
 * @author Luong Huu Phuc
 * @date 2025/07/19
 *
 * @note Thu vien dung de loc FIR lowpass cho PCG truoc khi downsample
 * \note - Chong Aliasing (rang cua)
 * \note - Dung theo nguyen tac Nyquist-Shannon sampling theorem: Truoc khi downsample tan so lay mau,
 * can loc thong thap tin hieu de loai bo tan so lon hon tan so Nyquist
 * \note - Khi downsample tu 8000Hz -> 1000Hz (giam 8 lan), dai tan giu lai nen nho hon 500Hz (1 nua cua 1000Hz)
 * va can loc bot tin hieu cao hon 500Hz truoc khi decimate de tranh Aliasing (Theo dinh ly Nyquist)
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "FIR_Filter.h"

const float fir_coeff[FIR_TAP_NUM] = {
		-0.001186, -0.001801, -0.002764, -0.003963,
		-0.004929, -0.004855, -0.002734, 0.002416,
		0.011283, 0.024016, 0.040068, 0.058172, 0.076482,
		0.092844, 0.105165, 0.111784, 0.111784, 0.105165,
		0.092844, 0.076482, 0.058172, 0.040068, 0.024016,
		0.011283, 0.002416, -0.002734, -0.004855, -0.004929,
		-0.003963, -0.002764, -0.001801, -0.001186 };


void fir_init(FIRFilter *filter){
	for(uint16_t i = 0; i < FIR_TAP_NUM; i++){
		filter->buffer[i] = 0.0f;
	}
	filter->index = 0;
}


float FIR_Filter(FIRFilter *filter, float input){
	filter->buffer[filter->index] = input; //Ghi vao buffer truoc, xu ly index sau
	float output = 0.0f;
	int32_t buf_idx = filter->index;

	if(++filter->index >= FIR_TAP_NUM){ //Moi lan goi ham, filter->index se tang them 1 dvi
		filter->index = 0; //Reset index buffer ve 0
	}


	//Can toi thieu 32 samples　input thi moi tao ra 1 output FIR chinh xac => Chap nhan mot vai mau dau bi sai
	for(uint16_t i = 0; i < FIR_TAP_NUM; i++){
		if(buf_idx-- == 0){
			buf_idx = FIR_TAP_NUM - 1;
		}
		//Mau moi nhat se nhan voi he so h[0], mau cu nhat nhan voi he so cuoi cung h[N-1]
		output += filter->buffer[buf_idx] * fir_coeff[i];
	}
	return output;
}

#ifdef __cplusplus
}
#endif

