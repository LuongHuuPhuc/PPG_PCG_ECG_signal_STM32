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

#ifdef DEBUG_SWV_ITM
/**
 * @brief SWV - Serial Wire Viewer - tinh nang debug real-time trong STM32
 * - Hoat dong bang cach truyen tai du lieu trang thai, bien so, log duoc luu
 * tren RAM cua MCU qua may tinh bang chan SWO voi toc do cao ma khong gian doan
 * chuong trinh.
 * - Khong luu tru, khong ngon RAM, hoat dong doc lap qua chan SWO
 * - SWV su dung khoi ITM (Instrumentation Trace Macrocell) de tiep nhan
 * du lieu. Phan mem cau hinh ITM de nhan cac luong du lieu (vd: printf())
 * va dong goi chung thanh cac luong du lieu
 */
HAL_StatusTypeDef SWV_Init(void);
#define SWV_LOG(...) printf(__VA_ARGS__)

#elif DEBUG_DWT
/**
 *  @brief DWT - Data Watchdog & Trace (dung de do thoi gian thuc thi cua thuat toan)
 *  DWT Cycle Counter (thanh ghi CYCCNT nam trong khoi DWT) la mot bo dem thoi gian
 *  phan cung dem chinh xac so xung nhip clock cua bo xu ly
 */
void DWT_Init(void);
#endif // DebugHelper

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* SWV_INC_SWV_DEBUG_H_ */
