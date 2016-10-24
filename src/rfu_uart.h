/***************************************************************************//**
 * @file rfu_uart.h
 * @brief Include file for RF UART comm module.
 *
 * @author Neal Shurmantine
 * @copyright (c) 2014 Hunter Douglas. All rights reserved.
 *
 * @date Created: 09/19/2014
 * @date Last updated: 09/19/2014
 *
 * Change Log:
 * 09/19/2014
 * - Created.
 *
 ******************************************************************************/

#ifndef RFU_UART_H_
#define RFU_UART_H_

void RFU_SendMsg(unsigned char len, char *msg);
bool RFU_GetRxChar(unsigned char *rslt);
void RFU_SetBootloadActive(bool isBootloadActive);
void *RFU_Register_Inbound_Event(uint16_t event_group, uint16_t event_mask);
void *RFU_Register_Bootload_Event(uint16_t event_group, uint16_t event_mask);

#endif
