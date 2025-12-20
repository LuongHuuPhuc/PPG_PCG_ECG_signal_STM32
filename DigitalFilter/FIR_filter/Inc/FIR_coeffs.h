/*
 * @file fir_coeffs.h
 *
 * @date Dec 5, 2025
 * @author LuongHuuPhuc
 *
 * @remark He so bo loc su dung MATLAB mo phong
 */

#ifndef FIR_FILTER_INC_FIR_COEFFS_H_
#define FIR_FILTER_INC_FIR_COEFFS_H_

#include "stdio.h"

/**
 * @brief FIR coeffs: 32 taps (dap ung xung cua bo loc)
 *
 * @note gia tri dap ung xung h[x] sau khi da thuc hien mo phong bang MATLAB
 * bang cach nhan gia tri dap ung xung ly tuong lowpass (sinc) voi cua so truot hamming
 * cho ra duoc gia tri cac gia tri dap xung xung tot nhat
 */
const float fir_coeffs_lowpass[32] = {
       -0.001185691f,
       -0.001800525f,
       -0.002763760f,
       -0.003963307f,
       -0.004928931f,
       -0.004854678f,
       -0.002733636f,
       0.002416139f,
       0.011283148f,
       0.024016473f,
       0.040067988f,
       0.058171995f,
       0.076481553f,
       0.092844137f,
       0.105165065f,
       0.111784030f,
       0.111784030f,
       0.105165065f,
       0.092844137f,
       0.076481553f,
       0.058171995f,
       0.040067988f,
       0.024016473f,
       0.011283148f,
       0.002416139f,
       -0.002733636f,
       -0.004854678f,
       -0.004928931f,
       -0.003963307f,
       -0.002763760f,
       -0.001800525f,
       -0.001185691f
};

#endif /* FIR_FILTER_INC_FIR_COEFFS_H_ */
