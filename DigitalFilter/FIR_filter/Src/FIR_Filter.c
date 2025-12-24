/**
 * @author Luong Huu Phuc
 * @date 2025/07/19
 *
 * @note Thu vien dung de loc FIR lowpass cho PCG truoc khi downsample
 * \note - Chong Aliasing (rang cua)
 * \note - Dung theo nguyen tac Nyquist-Shannon sampling theorem: Truoc khi downsample tan so lay mau, can loc thong thap
 * tin hieu de loai bo tan so lon hon tan so Nyquist
 * \note - Khi downsample tu 8000Hz -> 1000Hz (giam 8 lan), dai tan giu lai nen nho hon 450Hz va can loc bot tin hieu
 *  cao hon 450Hz truoc khi decimate de tranh Aliasing (Theo dinh ly Nyquist)
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "FIR_Filter.h"
#include "FIR_coeffs.h"

FIRFilter fir = {0};

void fir_init(FIRFilter *filter){
	for(uint16_t i = 0; i < FIR_TAP_NUM; i++){
		filter->buffer[i] = 0.0f;
	}
	filter->index = 0;
}

float FIR_process_convolution(FIRFilter *filter, float input){
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
		output += filter->buffer[buf_idx] * fir_coeffs_lowpass[i];
	}
	return output;
}

#ifdef __cplusplus
}
#endif

