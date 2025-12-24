/**
 * @author Luong Huu Phuc
 * @date 2025/07/19
 *
 * @note Thu vien dung de loc FIR lowpass cho PCG truoc khi downsample. Theo nguyen tac Nyquist-Shannon sampling theorem:
 * Truoc khi giam toc do lay mau, can loc thong thap tin hieu. Muc dich:
 * \note - Chong Aliasing (rang cua)
 * \note - Bo loc FIR la linear phase neu he so doi xung. Khi do, toan bo tin hieu duoc dich tre di dung bang (N - 1)/2 sample
 *
 * @remark Bo loc FIR, de tinh 1 output, can N samples qua khu, nen voi sample dau tien, khong co du lieu qua khu nen
 * khong the tinh duoc dau ra ngay lap tuc, the nen sinh ra do tre pha tuyen tinh (linear phase FIR)
 * => Can phai loai bo do tre pha do
 */

#ifndef FIR_FILTER_INC_FIR_FILTER_H_
#define FIR_FILTER_INC_FIR_FILTER_H_

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
#define EXPORT extern "C"
#else
#define EXPORT
#endif // __cplusplus

#define FIR_TAP_NUM 	32 //Bac cua bo loc FIR (He so coeffs - dap ung xung)
#define FIR_DELAY   	((FIR_TAP_NUM - 1) / 2) //Do tre pha cua bo loc FIR

typedef struct FIR_FILTER_t{
	float buffer[FIR_TAP_NUM]; //32 samples
	int index;
} FIRFilter;

extern FIRFilter fir;

/**
 * @brief Ham khoi tao bo loc so FIR
 *
 * @param filter Con tro tro den cau truc bo loc
 */
EXPORT void fir_init(FIRFilter *filter);

/**
 * @brief Ham thuc hien tinh toan gia tri cua so (hamming, hanning, hann, blackman, kaiser)
 *
 * @param filter Con tro tro den cau truc bo loc
 * @param windowType ten cua so can ap dung
 *
 * @return He so window sau khi da tinh toan
 */
EXPORT __attribute__((unused))float fir_window_process(FIRFilter *filter, char *windowType);

/**
 * @brief Ham trien khai bo loc so
 *
 * @param filter Con tro tro den cau truc bo loc
 * @param input Gia tri tin hieu dau vao x[n]
 *
 * @note Tinh toan bang cach nhan tich chap he so bo loc (dap ung xung da co) voi tin hieu dau vao
 * @note Cac he so cua bo loc FIR da duoc mo phong bang MATLAB bang cach nhan gia tri dap ung xung lowpass ly tuong (sinc)
 * voi cua so hamming (hamming window) de cho ra duoc he so bo loc tot nhat
 *
 * @note Tan so cat mong muon: 450Hz
 *
 * @return tin hieu dau ra y[n] sau khi da thuc hien tich chap
 */
EXPORT float FIR_process_convolution(FIRFilter *filter, float input);

#endif /* FIR_FILTER_INC_FIR_FILTER_H_ */
