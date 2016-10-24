/***************************************************************************//**
 * @file   rfi_inbound.c
 * @brief  This module serves as the receiving end of the transport layer
 *   from the Nordic to the Freescale.
 * 
 * @author Neal Shurmantine
 * @copyright (c) 2014 Hunter Douglas. All rights reserved.
 *
 * @date Created: 09/23/2014
 * @date Last updated: 11/25/2014
 *
 * @version
 * 09/23/2014   Created.
 * 11/25/2014   Re-wrote to comply with the new protocol specification
 *
 ******************************************************************************/

/* Includes
*******************************************************************************/
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include "config.h"
#include "os.h"
#include "rf_serial_api.h"
#include "rfu_uart.h"

/* Local Constants and Definitions
*******************************************************************************/
#define RF_SERIAL_RX_EVENT_BIT     BIT0
#define INTER_CHARACTER_TIMEOUT     200

typedef struct S_INB_MSG_STRUCT
{
    uint8_t msg_hdr;
    uint8_t msg_len_min;
    uint8_t msg_len_max;
    void (*funct)(PARSE_KEY_STRUCT_PTR);
}sINBOUND_MESSAGE;

typedef enum INBOUND_STATE_TAG
{
    RFI_LOOKING_FOR_HEADER,
    RFI_LOOKING_FOR_LEN,
    RFI_LOOKING_FOR_PAYLOAD,
    RFI_LOOKING_FOR_CHECKSUM
} RFI_INBOUND_STATE_TYPE;

/* Local Function Declarations
*******************************************************************************/
static void process_char(uint8_t new_char);
static void process_message(void);
static void re_init(void);

/* Local variables
*******************************************************************************/
const sINBOUND_MESSAGE InboundMessage[] = {
    { MSG_TYPE_GET_ATTR_CONF, 4, 35, RNC_SendNordicConfigConfirmation},
    { MSG_TYPE_SET_ATTR_CONF, 3, 3, RNC_SendNordicConfigConfirmation},
    { MSG_TYPE_RESET_CONF, 2, 2, RNC_SendNordicConfigConfirmation},
    { MSG_TYPE_START_CONF, 2, 2, RNC_SendNordicConfigConfirmation},
    { MSG_TYPE_SEND_SHADE_DATA_CONF, 3, 3, RNC_SendShadeConfirmation},
    { MSG_TYPE_SEND_BEACON_CONF, 2, 2, RNC_SendShadeConfirmation},
    { MSG_TYPE_SEND_GROUP_SET_CONF, 2, 2, RNC_SendShadeConfirmation},
    { MSG_TYPE_SHADE_DATA_INDICATION, 22, 22 + MAX_INDICATION_PAYLOAD_SIZE, RNC_SendShadeIndication},
    { MSG_TYPE_BEACON_INDICATION, 24, 24 + MAX_BEACON_PAYLOAD_SIZE, RNC_SendShadeIndication},
    { MSG_TYPE_GROUP_SET_INDICATION, 12, 12, RNC_SendShadeIndication},
    { MSG_TYPE_SYSTEM_INDICATION, 2, MAX_SYSTEM_INDICATION_SERIAL_RESPONSE_SIZE, RNC_SendSystemIndication},
    {0, 0, 0, NULL}
};

static RFI_INBOUND_STATE_TYPE State;
static bool EscapeByteFound = false;
static uint16_t RxIndex;
static uint32_t WaitTime;
static PARSE_KEY_STRUCT_PTR pRxBuffer;
static uint8_t Checksum;

/*****************************************************************************//**
* @brief This function initializes the rf inbound task and holds the 
*   the main loop for the task.  If there is a gap between characters then the
*   state machine is restarted, otherwise, characters are processed as they
*   are received.
*
* @param temp is a unused variable.
* @return nothing.
* @author Neal Shurmantine
* @since 09/23/2014
* @version Initial revision.
*******************************************************************************/
void *rfi_inbound_task(void * temp)
{ 
    void *event_handle;
    uint16_t event_mask = RF_SERIAL_RX_EVENT_BIT;
    uint16_t event_active;
    uint8_t c;

    event_handle = RFU_Register_Inbound_Event(0, RF_SERIAL_RX_EVENT_BIT);
    re_init();
printf("rfi_inbound_task\n");
    while(1)
    {
        event_active = OS_TaskWaitEvents(event_handle, event_mask, WaitTime);
        if (event_active == 0) { //timeout event, re-initialize
            if (pRxBuffer != NULL) {
                OS_ReleaseMsgMemBlock((void*)pRxBuffer);
            }
            re_init();
//printf("!");
        }
        else {
            if (RFU_GetRxChar(&c) == true) {
//printf("`");
                process_char(c);
            }
        }
    }
}

/*****************************************************************************//**
* @brief The function holds the state machine that processes each character
*  in the transport layer as the character is received.
*
* @param new_char is the byte to be processed.
* @return nothing.
* @author Neal Shurmantine
* @version
* 09/23/2014    Created.
* 11/25/2014    Updated for new protocol
*
*******************************************************************************/
static void process_char(uint8_t new_char)
{
    WaitTime = INTER_CHARACTER_TIMEOUT;

    switch (State)
    {
        case RFI_LOOKING_FOR_HEADER:
            if (new_char == START_OF_HEADER) {
                State = RFI_LOOKING_FOR_LEN;
            }
            break;
        case RFI_LOOKING_FOR_LEN:
            //Note: this memory may be freed from this task or the RNC_RFNetworkConfig task
            pRxBuffer = (PARSE_KEY_STRUCT_PTR)OS_GetMsgMemBlock(new_char+2); //add 2 for length itself
            pRxBuffer->generic.length = new_char;
            RxIndex = 0;
            State = RFI_LOOKING_FOR_PAYLOAD;
            break;
        case RFI_LOOKING_FOR_PAYLOAD:
            Checksum += new_char;
            if (new_char == START_OF_HEADER) {
                re_init();
                State = RFI_LOOKING_FOR_LEN;
            }
            else if (new_char == ESCAPE_TOKEN) {
                EscapeByteFound = true;
                --pRxBuffer->generic.length;
            }
            else {
                pRxBuffer->generic.msg[RxIndex] = new_char;
                if (EscapeByteFound == true) {
                    pRxBuffer->generic.msg[RxIndex] |= 0x40;
                    EscapeByteFound = false;
                }
                ++RxIndex;
                if (RxIndex >= pRxBuffer->generic.length) {
                    State = RFI_LOOKING_FOR_CHECKSUM;
                }
            }
            break;
        case RFI_LOOKING_FOR_CHECKSUM:
            if (Checksum == new_char) {
                process_message();
            }
            else {
                OS_ReleaseMsgMemBlock((void*)pRxBuffer);
            }
            re_init();
           break;
        default:
            re_init();
            break;
    }
}

/*****************************************************************************//**
* @brief The message was received correctly via the transport layer and this
*   function confirms that it looks like a recognizable message.
*
* @param none
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/25/2014    Created.
*******************************************************************************/
static void process_message(void)
{
    sINBOUND_MESSAGE const *pMessages;
    uint16_t n = 0; // length;
    pMessages = InboundMessage;
    
    // printf("*** process_message(%d) ***\n", pRxBuffer->generic.length);
    
    while ( pMessages->msg_hdr != 0 ) {
        if (pRxBuffer->generic.msg[0] == pMessages->msg_hdr)
        {
            if ((pRxBuffer->generic.length >= (uint16_t)InboundMessage[n].msg_len_min) 
                && (pRxBuffer->generic.length <= (uint16_t)InboundMessage[n].msg_len_max) )
            {
//printf("Valid message\n");
                (*InboundMessage[n].funct)(pRxBuffer);
                
                //The called function should release memory
                return;
            }
        }
        ++pMessages;
        ++n;
    }
    OS_ReleaseMsgMemBlock((void*)pRxBuffer);
}

/*****************************************************************************//**
* @brief Reinitialize the Receiver state machine to prepare for a new serial message.
*
* @param nothing
* @return nothing.
* @author Neal Shurmantine
* @version
* 09/26/2014    Created.
*******************************************************************************/
static void re_init(void)
{
    pRxBuffer = NULL;
    RxIndex = 0;
    WaitTime = WAIT_TIME_INFINITE;
    EscapeByteFound = false;
    State = RFI_LOOKING_FOR_HEADER;
    Checksum = 0;
}

