/**
 * @author Luong Huu Phuc
 * @date 2025/07/19
 *
 * @note Thu vien dung de loc FIR lowpass cho PCG truoc khi downsample
 * \note - Chong Aliasing (rang cua)
 * \note - Dung theo nguyen tac Nyquist-Shannon sampling theorem: Truoc khi giam toc do lay mau,
 * can loc thong thap tin hieu
 * \note - Bo loc FIR la linear phase neu he so doi xung. Khi do, toan bo tin hieu duoc dich tre di dung bang (N - 1)/2 sample
 * \note - Bo loc FIR, de tinh 1 output, can N samples qua khu, nen voi sample dau tien, khong co du lieu qua khu nen
 * khong the tinh duoc dau ra ngay lap tuc, the nen sinh ra do tre pha tuyen tinh (linear phase FIR)
 *
 * => Can phai loai bo do tre pha do
 */

#ifndef INC_FIR_FILTER_H_
#define INC_FIR_FILTER_H_

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
#define EXPORT extern "C"
#else
#define EXPORT
#endif // __cplusplus

#define FIR_TAP_NUM 32 //Bac cua bo loc FIR
#define FIR_DELAY   ((FIR_TAP_NUM - 1) / 2) //Do tre pha cua bo loc FIR

typedef struct {
	float buffer[FIR_TAP_NUM]; //32 samples
	int index;
} FIRFilter;

extern const float fir_coeff[FIR_TAP_NUM];

EXPORT void fir_init(FIRFilter *filter);
EXPORT float FIR_Filter(FIRFilter *filter, float input);

#endif /* INC_FIR_FILTER_H_ */
