/*
 * @file ExtButton_handle.h
 *
 * @date Jun 20, 2026
 * @author LuongHuuPhuc
 */

#ifndef INC_EXTBUTTON_HANDLE_H_
#define INC_EXTBUTTON_HANDLE_H_

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#pragma once

#include "main.h"		// O day dinh nghia BUTTON can dung External Interrupt
#include "stdbool.h"
#include "cmsis_os.h"

#define BUTTON_HOLD_DURATION_MS		3000U		/* Thoi gian giu nut toi thieu de kich no su kien (3s) */

extern volatile bool g_sd_backup_allow_flag; 	/* Co hieu toan cuc thong bao trang thai cho phep ghi the SD */

/**
 * @brief Khoi tao Task FreeRTOS quan ly giam sat nut bam
 */
HAL_StatusTypeDef ExtButton_init(void);

/**
 * @brief Tac vu chay ngam dinh ky quet trang thai va tinh toan giu nut trong 3s
 */
void ExtButton_process_task(void const *pvParameter);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* INC_EXTBUTTON_HANDLE_H_ */
