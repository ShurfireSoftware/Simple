/***************************************************************************//**
 * @file rfo_outbound.h
 * @brief Include file for rfo_outbound module.
 *
 * @author Neal Shurmantine
 * @copyright (c) 2014 Hunter Douglas. All rights reserved.
 *
 * @date Created: 09/25/2014
 * @date Last updated: 09/25/2014
 *
 * Change Log:
 * 09/25/2014
 * - Created.
 *
 ******************************************************************************/

#ifndef RFO_OUTBOUND_H_
#define RFO_OUTBOUND_H_

bool RFO_DeliverRequest(RNC_CONFIG_REC_PTR p_ser_msg);
void RFO_NotifySerialResponse(uint8_t status);
void RFO_Reset(void);

#endif
