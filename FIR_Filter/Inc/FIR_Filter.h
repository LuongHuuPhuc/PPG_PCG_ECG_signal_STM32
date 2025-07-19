/**
 * @author Luong Huu Phuc
 * @date 2025/07/19
 *
 * @note Thu vien dung de loc FIR lowpass cho PCG truoc khi downsample
 * \note - Chong Aliasing (rang cua)
 * \note - Dung theo nguyen tac Nyquist-Shannon sampling theorem: Truoc khi giam toc do lay mau,
 * can loc thong thap tin hieu
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

#define FIR_TAP_NUM 64 //Bac cua bo loc FIR

extern const float fir_coeff[FIR_TAP_NUM];

EXPORT float FIR_Filter(float input);

#endif /* INC_FIR_FILTER_H_ */
