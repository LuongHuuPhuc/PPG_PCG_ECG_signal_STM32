/*
 * @file max30102_low_level.c
 * @author LuongHuuPhuc
 *
 *  Created on: Oct 16, 2025
 */

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "main.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "max30102_low_level.h"

HAL_StatusTypeDef max30102_write(max30102_t *obj, uint8_t reg, uint8_t *buf, uint16_t buflen)
{
    uint8_t *payload = (uint8_t *)malloc((buflen + 1) * sizeof(uint8_t));
    *payload = reg;
    if (buf != NULL && buflen != 0)
        memcpy(payload + 1, buf, buflen);
    if(HAL_I2C_Master_Transmit(obj->_ui2c, MAX30102_I2C_ADDR << 1, payload, buflen + 1, MAX30102_I2C_TIMEOUT) != HAL_OK){
    	uart_printf("[ERROR] max30102_write failed at Transmit at reg 0x%02x, HAL error: 0x%lx!\r\n", *payload, obj->_ui2c->ErrorCode);
    	free(payload);
    	return HAL_ERROR;
    }
    free(payload);
    return HAL_OK;
}

HAL_StatusTypeDef max30102_read(max30102_t *obj, uint8_t reg, uint8_t *buf, uint16_t buflen)
{
    uint8_t reg_addr = reg; // Truyen vao 0x00
    HAL_StatusTypeDef status;

    status = HAL_I2C_Master_Transmit(obj->_ui2c, MAX30102_I2C_ADDR << 1, &reg_addr, 1, MAX30102_I2C_TIMEOUT);
    if(status != HAL_OK){
    	uart_printf("[ERROR] max30102_read failed at Transmit at reg 0x%02x, HAL error: 0x%lx!\r\n", reg, obj->_ui2c->ErrorCode);
    	return HAL_ERROR;
    }
    status = HAL_I2C_Master_Receive(obj->_ui2c, MAX30102_I2C_ADDR << 1, buf, buflen, MAX30102_I2C_TIMEOUT);
    if(status != HAL_OK){
    	uart_printf("[ERROR] max30102_read failed at Receive at reg 0x%02x, HAL error: 0x%lx!\r\n", reg, obj->_ui2c->ErrorCode);
    	return HAL_ERROR;
    }
    return HAL_OK;
}

#ifdef __cplusplus
}
#endif // __cplusplus

