/*
 * @file SWV_debug.h
 *
 * @date Jun 2, 2026
 * @author LuongHuuPhuc
 */

#ifndef SWV_INC_SWV_DEBUG_H_
#define SWV_INC_SWV_DEBUG_H_

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#pragma once

#include "main.h"

/**
 * SWV - Serial Wire Viewer
 */
void SWV_Init(void);

/**
 *  DWT - Data Watchdog & Trace (dung de do thoi gian thuc thi cua thuat toan)
 *  DWT Cycle Counter (thanh ghi CYCCNT nam trong khoi DWT) la mot bo dem thoi gian
 *  phan cung dem chinh xac so xung nhip clock cua bo xu ly
 */
void DWT_Init(void);

#define SWV_LOG(...) printf(__VA_ARGS__)

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* SWV_INC_SWV_DEBUG_H_ */
