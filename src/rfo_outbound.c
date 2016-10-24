/***************************************************************************//**
 * @file rfo_outbound.c
 * @brief The purpose of this task is to allow a calling task to send an<br/>
 *  instruction, destined for the Nordic, and be assured that the message<br/>
 *  has a high probability of being delivered.  This task manages the retries.<br/>
 *  should a timeout or NAK occur.  In general, it provides a simple interface<br/>
 *  for initiating a message delivery and expects a simple interface for<br/>
 *  delivering the results.  As part of its state machine, it provides an easily<br/>
 *  configurable set of timing parameters for managing timeouts when expecting a<br/>
 *  response from the shade and for pacing the rate that shade messages are sent.
 *
 * @author Neal Shurmantine
 * @copyright (c) 2014 Hunter Douglas. All rights reserved.
 *
 * @version
 * 11/06/2014   Created.
 *
 *
 ******************************************************************************/

/* Includes
*******************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "rf_serial_api.h"
#include "util.h"
#include "os.h"
#include "config.h"

#include "rfo_outbound.h"
#include "rfu_uart.h"


/* Local Symbols
*******************************************************************************/
#define RFO_MSG_RCVD_EVENT              BIT0
#define RFO_SER_RESP_EVENT              BIT1


typedef enum
{
    RFO_SENDING_RF_MSG_STATE,
    RFO_SENDING_NORDIC_MSG_STATE,
    RFO_RETRY_RF_MSG_STATE,
    RFO_RETRY_NORDIC_MSG_STATE,
    RFO_IDLE_STATE
}RFO_STATE_T;


/* Local Functions
*******************************************************************************/
static void RFO_process_msg_request(void);
static void RFO_process_ser_response(void);
static void RFO_process_timeout_event(void);
//static void debug_print(const char * p_msg);


/* Local variables
*******************************************************************************/
/** Mailbox indicating a new shade message is ready to be transmitted. */
static uint16_t RFO_ReqMsgMbox;
/** Mailbox indicating a serial response from the Nordic has been received. */
static uint16_t RFO_ResponseMbox;
/** Mask containing the events to which a task may respond when it is<br/> 
 blocked waiting on events */
static uint16_t RFO_ExpectedEvents;
/** Local counter containing the number of retries that have occurred. */
static uint16_t RFO_MsgRetryCounter;
/** Maximum number of retry attempts that should be made for this message. */
static uint16_t RFO_MsgRetryLimit;
/** Holds that state of the message currently being transmitted. */
static RFO_STATE_T RFO_State;
/** A pointer to the byte array containing the current serial message. */
static uint8_t *RFO_SerMsg; //holds the active serial message info
/** A variable containing a timeout for waiting for an event.  This value<br/>
  is modified, based on the state of the message transmission. */
static uint32_t RFO_WaitTime;
static DESTINATION_DEVICE_TYPE RFO_DestinationType;

//static void debug_print(const char * p_msg)
//{
//#ifdef DEBUG_PRINT
//    printf(p_msg);
//#endif
//}

/*****************************************************************************//**
* @brief This function is called to indicate that a new serial command should<br/>
*  be sent to the Nordic for deivery to a shade.
*
* <b>Note:</b>  This function runs in the context of the calling task.  Also, <br/>
*   receiving task (rfo_outbound) is responsible for releasing the memory block<br/>
*   after processing.
* @param p_cfg_rec is a pointer to a shade configuration structure.
* @return nothing.
* @author Neal Shurmantine
* @since 2014-11-13
* @version Initial revision.
*******************************************************************************/
bool RFO_DeliverRequest(RNC_CONFIG_REC_PTR p_cfg_rec)
{
//FIX ME
// --- NOTE: block more than one message at a time
    if (RFO_DestinationType != DESTINATION_NONE) {
printf("Uh Oh!\n\r");
        return false;
    }
//printf("Got to here\n");
    RFO_DestinationType = p_cfg_rec->dest_dev_type;
    OS_MessageSend(RFO_ReqMsgMbox,p_cfg_rec);
    return true;
}

/*****************************************************************************//**
* @brief This function is called when a serial inbound status message has been<br/>
*  received from the Nordic.
*
* <b>Note:</b>  This function runs in the context of the calling task.  Also, <br/>
*   receiving task (rfo_outbound) is responsible for releasing the memory block<br/>
*   after processing.
* @param p_status is a pointer to a status byte containing the result of the<br/>
*  last message send to the nordic.
* @return nothing.
* @author Neal Shurmantine
* @since 2014-11-13
* @version Initial revision.
*******************************************************************************/
void RFO_NotifySerialResponse(uint8_t status)
{
    uint8_t * p_status = 
            (uint8_t *)OS_GetMsgMemBlock(sizeof(uint8_t));
    *p_status = status;

    if ((RFO_State == RFO_SENDING_RF_MSG_STATE) 
            || (RFO_State == RFO_SENDING_NORDIC_MSG_STATE) ){
        OS_MessageSend(RFO_ResponseMbox,p_status);
    }
    else {
//printf("RFO_NotifySerialResponse OS_ReleaseMemBlock\n\r");
        OS_ReleaseMsgMemBlock((void *)p_status);
    }
}

/*****************************************************************************//**
* @brief This function initializes the rfo_outbound task and holds the<br/>
*  main loop of the task.
*
* @param temp is a unused variable.
* @return nothing.
* @author Neal Shurmantine
* @since 09/25/2014
* @version Initial revision.
*******************************************************************************/
void *rfo_outbound_task(void * temp)
{ 
    //holds the event bits that were set when the task wakes up
    uint16_t event_active;

    //create the event for this task and create a mailbox for a message received
    void *event_handle;
    event_handle = OS_EventCreate(0,false);

    RFO_ReqMsgMbox = OS_MboxCreate(event_handle,RFO_MSG_RCVD_EVENT); 
    RFO_ResponseMbox = OS_MboxCreate(event_handle,RFO_SER_RESP_EVENT);

    RFO_Reset();
printf("rfo_outbound_task\n");
    while(1) {
        event_active = OS_TaskWaitEvents(event_handle, RFO_ExpectedEvents, RFO_WaitTime);
        event_active &= RFO_ExpectedEvents;
        if (event_active == RFO_MSG_RCVD_EVENT) {
            RFO_process_msg_request();
        }
        if (event_active == RFO_SER_RESP_EVENT) {
            RFO_process_ser_response();
        }
        if (!event_active)
        {
            RFO_process_timeout_event();
        }
    }
}

/*****************************************************************************//**
* @brief This function ...
*
* <b>Note:</b>  This function runs in the context of the calling task.
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 12/04/2014    Created.
*******************************************************************************/
void RFO_Reset(void)
{
    RFO_WaitTime = WAIT_TIME_INFINITE;
    RFO_State = RFO_IDLE_STATE;
    RFO_ExpectedEvents = RFO_MSG_RCVD_EVENT;
    RFO_DestinationType = DESTINATION_NONE;
}

/*******************************************************************************
* Procedure:    RFO_process_msg_request
* Purpose:      xxxx
* Passed:       xxxx
*   
* Returned:     nothing
* Globals:      none
*
* Date:         Author:             Comments:
*   2014-11-13  Neal Shurmantine    initial revision
*******************************************************************************/
static void RFO_process_msg_request(void)
{
    RNC_CONFIG_REC_PTR p_cfg_rec = (RNC_CONFIG_REC_PTR)OS_MessageGet(RFO_ReqMsgMbox);

    if (RFO_DestinationType == DESTINATION_NORDIC) {
        //setup for retries
        RFO_MsgRetryCounter = 0;
        RFO_MsgRetryLimit = RFO_MAX_NORDIC_MSG_TRIES;
        //set next state and wait time
        RFO_State = RFO_SENDING_NORDIC_MSG_STATE;
        RFO_WaitTime = RFO_WAIT_NORDIC_SER_RESPONSE;
    }
    else {
        //setup for retries
        RFO_MsgRetryCounter = 0;
        RFO_MsgRetryLimit = RFO_MAX_RF_MSG_TRIES;
        //set next state and wait time
        RFO_State = RFO_SENDING_RF_MSG_STATE;
        RFO_WaitTime = RFO_WAIT_RF_SER_RESPONSE;
    }

    RFO_SerMsg = p_cfg_rec->ser_msg;
    RFO_ExpectedEvents = RFO_SER_RESP_EVENT;
    //Send message to UART
    RFU_SendMsg(RFO_SerMsg[1]+3,(char*)RFO_SerMsg);
}

/*******************************************************************************
* Procedure:    RFO_process_ser_response
* Purpose:      xxxx
* Passed:       xxxx
*   
* Returned:     nothing
* Globals:      none
*
* Date:         Author:             Comments:
*   2014-11-13  Neal Shurmantine    initial revision
*******************************************************************************/
static void RFO_process_ser_response(void)
{
    uint8_t *p_stat;

    //Remove pointer to serial message response from NCO_NWCResponseMbox
    p_stat = (uint8_t*)OS_MessageGet(RFO_ResponseMbox);

    if (RFO_State == RFO_SENDING_RF_MSG_STATE) {
        if (*p_stat == SC_RSLT_SUCCESS)
        {
            //set events to wait on a timeout only, wait a
            //minimum time before receiving a new message request
            RFO_Reset();
        }
        else
        {
            //else this serial attempt failed, see if more retries available
            RFO_MsgRetryCounter++;
            if (RFO_MsgRetryCounter < RFO_MsgRetryLimit)
            {
                RFO_WaitTime = RFO_WAIT_RETRY_RF_OUTBOUND;
                RFO_ExpectedEvents = 0;
                RFO_State = RFO_RETRY_RF_MSG_STATE;
            }
            else
            {
                RFO_Reset();
            }
        }
    }
    else if (RFO_State == RFO_SENDING_NORDIC_MSG_STATE) {
        if (*p_stat == SC_RSLT_SUCCESS)
        {
            RFO_Reset();
        }
        else
        {
            //else this serial attempt failed, see if more retries available
            RFO_MsgRetryCounter++;
            if (RFO_MsgRetryCounter < RFO_MsgRetryLimit)
            {
                RFO_WaitTime = RFO_WAIT_RETRY_NORDIC_OUTBOUND;
                RFO_ExpectedEvents = 0;
                RFO_State = RFO_RETRY_NORDIC_MSG_STATE;
            }
            else
            {
                RFO_Reset();
            }
        }
    }
    // free memory used to hold the serial response
    OS_ReleaseMsgMemBlock((void *)p_stat);
}

/*******************************************************************************
* Procedure:    RFO_process_timeout_event
* Purpose:      xxxx
* Passed:       xxxx
*   
* Returned:     nothing
* Globals:      none
*
* Date:         Author:             Comments:
*   2014-11-13  Neal Shurmantine    initial revision
*******************************************************************************/
static void RFO_process_timeout_event(void)
{
    switch (RFO_State)
    {
        case RFO_SENDING_RF_MSG_STATE:
        case RFO_SENDING_NORDIC_MSG_STATE:
            RFO_MsgRetryCounter++;
            if (RFO_MsgRetryCounter < RFO_MsgRetryLimit)
            {
                if (RFO_DestinationType == DESTINATION_NORDIC) {
                    //last attempt to send an outbound message failed,retry
                    RFO_WaitTime = RFO_WAIT_NORDIC_SER_RESPONSE;
                }
                else {
                    //last attempt to send an outbound message failed,retry
                    RFO_WaitTime = RFO_WAIT_RF_SER_RESPONSE;
                }
                //send message to UART
                RFU_SendMsg(RFO_SerMsg[1]+3,(char*)RFO_SerMsg);
            }
            else
            {
                //no response was received to an rf outbound message, artificially
                // create a NAK with error code = timeout
                //get enough memory to hold a serial NAK header
                //memory must be obtained here because there was no response from NWC
                // which would have obtained memory and DC_NotifySerialComplete
                // may be called, which frees the response mem
                RNC_NotifySerialTimeout(RFO_DestinationType);

                //timeout has expired, no response received
                //enable task to receive a new outbound message
                RFO_Reset();
            }
            break;
        case RFO_RETRY_NORDIC_MSG_STATE:
        case RFO_RETRY_RF_MSG_STATE:
            //last attempt to send an outbound message failed,
            // it is time to retry
            if (RFO_DestinationType == DESTINATION_NORDIC) {
                RFO_State = RFO_SENDING_NORDIC_MSG_STATE;
                RFO_WaitTime = RFO_WAIT_NORDIC_SER_RESPONSE;
            }
            else {
                RFO_State = RFO_SENDING_RF_MSG_STATE;
                RFO_WaitTime = RFO_WAIT_RF_SER_RESPONSE;
            }

            RFO_ExpectedEvents = RFO_SER_RESP_EVENT;
            //send message to UART
            RFU_SendMsg(RFO_SerMsg[1]+3,(char*)RFO_SerMsg);
            break;
        default:
            break;
    }
}


