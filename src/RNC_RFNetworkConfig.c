/***************************************************************************//**
 * @file   RNC_RFNetworkConfig.c
 * @brief  This module provides the task responsible for delivery of shade<br/>
 *  and Nordic messages to the RF device and network.
 *
 * @author Neal Shurmantine
 * @copyright (c) 2014 Hunter Douglas. All rights reserved.
 *
 * @date Created: 11/24/2014
 * @date Last updated: 01/12/2015
 *
 * @version
 * 11/24/2014   Created.
 * 01/12/2015   Added Discovery
 ******************************************************************************/

/* Includes
*******************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "rf_serial_api.h"
#include "util.h"
#include "os.h"
#include "config.h"
#include "rfo_outbound.h"
#include "SCH_ScheduleTask.h"
#include <time.h>
#include "stub.h"
#include "sys/time.h"

#ifdef USE_ME
#include "UDPProcessing.h"
#include "DataStruct.h"
#endif

/* Global Variables
*******************************************************************************/
pthread_t RfRxTaskId;
pthread_t RfTxTaskId;
pthread_t RfInboundTaskId;
pthread_t RfOutboundTaskId;
pthread_t RNCRFNetworkConfigTaskId;
pthread_t NBTBootloadTaskId;
pthread_t IPCServerTaskId;

/* Local Constants and Definitions
*******************************************************************************/
#define RNC_RADIO_CONFIG_REQ_EVENT          BIT0
#define RNC_SHADE_CONFIG_REQ_EVENT          BIT1
#define RNC_RADIO_SERIAL_CONF_EVENT         BIT2
#define RNC_SHADE_SERIAL_CONF_EVENT         BIT3
#define RNC_SHADE_INDICATION_EVENT          BIT4
#define RNC_RF_TICK_EVENT                   BIT5
#define RNC_SYSTEM_INDICATION_EVENT         BIT6
#define RNC_DISCOVERY_PROCESS_EVENT         BIT7

/* Local Function Declarations
*******************************************************************************/
static void RNC_process_timeout(void);
static void RNC_process_radio_confg_request(void);
static void RNC_process_shade_confg_request(void);
static void RNC_process_radio_serial_confirmation(void);
static void RNC_process_shade_serial_confirmation(void);
static void RNC_process_shade_indication(void);
static void RNC_process_system_indication(void);

static uint8_t lastMessage[60];
static uint16_t lastMessageSize;
static struct timeval lastMessageTime = {0,0};

/* Global variables
*******************************************************************************/
extern uint16_t SC_NetworkJoinScheduleToken;

/* Local variables
*******************************************************************************/
static uint16_t RNC_RadioConfigRequestMbox;
static uint16_t RNC_ShadeConfigRequestMbox;
static uint16_t RNC_RadioSerialConfirmationMbox;
static uint16_t RNC_ShadeSerialConfirmationMbox;
static uint16_t RNC_ShadeIndicationMbox;
static uint16_t RNC_RFTickTimer;
static uint16_t RNC_SystemIndicationMbox;
static uint16_t RNC_ExpectedEvents;
static uint32_t RNC_WaitTime;
void *RNC_EventHandle;

/*****************************************************************************//**
* @brief This function initializes all of the tasks associated with the RF<br/>
*    shade network.
*
* <b>Note:</b>  This function runs in the context of the calling task.
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/25/2014    Created.
*******************************************************************************/
void RNC_InitRFNetworkCommunicationTasks(void)
{
    RfInboundTaskId = OS_TaskCreate(RFI_INBOUND_TASK_NUM, 0);
    RfOutboundTaskId = OS_TaskCreate(RFO_OUTBOUND_TASK_NUM, 0);
    RNCRFNetworkConfigTaskId = OS_TaskCreate(RNC_RF_CONFIG_TASK_NUM, 0);
    RfTxTaskId = OS_TaskCreate(RF_TX_TASK_NUM, 0);
OS_TaskSleep(2000);
    RfRxTaskId = OS_TaskCreate(RF_RX_TASK_NUM, 0);
//    NBTBootloadTaskId = OS_TaskCreate(NBT_BOOTLOAD_TASK_NUM, 0);
OS_TaskSleep(1000);
printf("RC_ResetRadio\n");
    RC_ResetRadio();
    IPCServerTaskId = OS_TaskCreate(IPC_SERVER_TASK_NUM, 0);

}

static void test_callback(uint16_t unused)
{
    printf("Callback executed\n");
}

void sch_test_scheduler(void)
{
    DAY_STRUCT day;
    day.hour = 15;
    day.minute = 3;
    day.second = 0;
    SCH_ScheduleEventPostSeconds(5,test_callback);
    SCH_ScheduleDaily(&day, test_callback);
    SCH_DebugRequestScheduleList();
}

/*****************************************************************************//**
* @brief This function ...
*
* <b>Note:</b>  This function runs in the context of the calling task.
* @param p_status is a pointer to a structure of type RNC_CONFIG_REC_PTR.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/25/2014    Created.
*******************************************************************************/
void RNC_SendNordicConfigRequest(RNC_CONFIG_REC_PTR p_cfg_rec)
{
    OS_MessageSend(RNC_RadioConfigRequestMbox,p_cfg_rec);
//printf("5\n");
}

/*****************************************************************************//**
* @brief This function ...
*
* <b>Note:</b>  This function runs in the context of the calling task.
* @param p_cfg_rec is a pointer to a structure of type SHADE_COMMAND_INSTRUCTION_PTR.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/25/2014    Created.
* 11/19/2015	Modified by Henk
*******************************************************************************/
extern uint16_t SC_RedundantChecksum;
void RNC_SendShadeRequest(SHADE_COMMAND_INSTRUCTION_PTR p_cfg_rec)
{
    SC_RedundantChecksum = 0xffff;
    OS_MessageSend(RNC_ShadeConfigRequestMbox,p_cfg_rec);
}

/*****************************************************************************//**
* @brief This function ...
*
* <b>Note:</b>  This function runs in the context of the calling task.
* @param p_status is a pointer to a structure of type SC_SER_RESP_CODE_T.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/25/2014    Created.
*******************************************************************************/
void RNC_SendNordicConfigConfirmation(PARSE_KEY_STRUCT_PTR p_ser_msg)
{
    OS_MessageSend(RNC_RadioSerialConfirmationMbox,p_ser_msg);  //send to mailbox
}

/*****************************************************************************//**
* @brief This function ...
*
* <b>Note:</b>  This function runs in the context of the calling task.
* @param p_status is a pointer to a structure of type PARSE_KEY_STRUCT_PTR.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/25/2014    Created.
*******************************************************************************/
void RNC_SendShadeConfirmation(PARSE_KEY_STRUCT_PTR p_ser_msg)
{
    uint8_t * p_rslt_code;

    p_rslt_code = SC_ProcessShadeConfirmation(p_ser_msg);
    OS_MessageSend(RNC_ShadeSerialConfirmationMbox,p_rslt_code);
}

/*****************************************************************************//**
* @brief This function ...
*
* <b>Note:</b>  This function runs in the context of the calling task.
* @param
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/25/2014    Created.
*******************************************************************************/
void RNC_SendShadeIndication(PARSE_KEY_STRUCT_PTR p_ser_msg)
{
    OS_MessageSend(RNC_ShadeIndicationMbox,p_ser_msg);
}

/*****************************************************************************//**
* @brief Start up the RF tick timer and enable its event.
*
* @param none.
* @return none.
* @author Neal Shurmantine
* @since 11/25/2014
* @version Initial revision.
*******************************************************************************/
void RNC_StartTickTimer(void)
{
    if ((RNC_ExpectedEvents & RNC_RF_TICK_EVENT) == 0)
    {
        OS_TimerSetCyclicInterval(RNC_RFTickTimer,SC_WAIT_RF_TICK);
        RNC_ExpectedEvents |= RNC_RF_TICK_EVENT;
    }
}

/*****************************************************************************//**
* @brief Stop the RF tick timer and disable its event.
*
* @param none.
* @return none.
* @author Neal Shurmantine
* @version
* 11/25/2014    Created.
*******************************************************************************/
void RNC_StopTickTimer(void)
{
    OS_TimerStop(RNC_RFTickTimer);
    RNC_ExpectedEvents &= ~RNC_RF_TICK_EVENT;
}

/*****************************************************************************//**
* @brief
*
* @param none.
* @return
* @author Neal Shurmantine
* @version
* 11/25/2014    Created.
*******************************************************************************/
void RNC_NotifySerialTimeout(DESTINATION_DEVICE_TYPE type)
{
    if (type == DESTINATION_SHADE) {
        uint8_t * p_status =
                (uint8_t *)OS_GetMsgMemBlock(sizeof(uint8_t));
        *p_status = SC_RSLT_TIMEOUT;
        OS_MessageSend(RNC_ShadeSerialConfirmationMbox,p_status);
    }
//FIX ME
//  handle a nordic configuration timeout
}

/*****************************************************************************//**
* @brief A serial message was received indicating the Nordic sent a system message
*     either a reset or version..
*
* @param p_ser_msg.  Pointer to a union PARSE_KEY_STRUCT
* @return none
* @author Neal Shurmantine
* @version
* 05/05/2015    Created.
*******************************************************************************/
void RNC_SendSystemIndication(PARSE_KEY_STRUCT_PTR p_ser_msg)
{
    OS_MessageSend(RNC_SystemIndicationMbox,p_ser_msg);
}

/*****************************************************************************//**
* @brief This function was created to allow shade processing during discovery
*    to occur in this task, not the scheduler task.
*
* @param nothing.
* @return none
* @author Neal Shurmantine
* @version
* 05/23/2015    Created.
*******************************************************************************/
void RNC_BeginDiscoveryProcessing(void)
{
    OS_EventSet(RNC_EventHandle, RNC_DISCOVERY_PROCESS_EVENT);
}

/*****************************************************************************//**
* @brief This function ...
*
* @param temp is a unused variable.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/25/2014    Created.
*******************************************************************************/
void *rnc_rf_network_config_task(void * temp)
{
    //holds the event bits that were set when the task wakes up
    uint16_t event_active;

    //the task will wake up on this timeout if no events have occurred.
    RNC_WaitTime = WAIT_TIME_INFINITE;

    //create the event for this task plus mailboxes and timer
    RNC_EventHandle = OS_EventCreate(0,false);
    RNC_RadioConfigRequestMbox = OS_MboxCreate(RNC_EventHandle,RNC_RADIO_CONFIG_REQ_EVENT);
    RNC_ShadeConfigRequestMbox = OS_MboxCreate(RNC_EventHandle,RNC_SHADE_CONFIG_REQ_EVENT);
    RNC_RadioSerialConfirmationMbox = OS_MboxCreate(RNC_EventHandle,RNC_RADIO_SERIAL_CONF_EVENT);
    RNC_ShadeSerialConfirmationMbox = OS_MboxCreate(RNC_EventHandle,RNC_SHADE_SERIAL_CONF_EVENT);
    RNC_ShadeIndicationMbox = OS_MboxCreate(RNC_EventHandle,RNC_SHADE_INDICATION_EVENT);
    RNC_SystemIndicationMbox = OS_MboxCreate(RNC_EventHandle,RNC_SYSTEM_INDICATION_EVENT);
    RNC_RFTickTimer = OS_TimerCreate(RNC_EventHandle, RNC_RF_TICK_EVENT);

    OS_TimerSetCyclicInterval(RNC_RFTickTimer,RNC_TICK_INTERVAL);

    RNC_ExpectedEvents = RNC_RADIO_CONFIG_REQ_EVENT
                        | RNC_SHADE_CONFIG_REQ_EVENT
                        | RNC_RADIO_SERIAL_CONF_EVENT
                        | RNC_SHADE_SERIAL_CONF_EVENT
                        | RNC_SHADE_INDICATION_EVENT
                        | RNC_SYSTEM_INDICATION_EVENT
                        | RNC_RF_TICK_EVENT
                        | RNC_DISCOVERY_PROCESS_EVENT;
printf("rnc_rf_network_config_task\n");
    while(1) {
        event_active = OS_TaskWaitEvents(RNC_EventHandle, RNC_ExpectedEvents, RNC_WaitTime);
        event_active &= RNC_ExpectedEvents;
        if (event_active & RNC_RADIO_CONFIG_REQ_EVENT)
        {
            RNC_process_radio_confg_request();
        }
        if (event_active & RNC_DISCOVERY_PROCESS_EVENT) {
            OS_EventClear(RNC_EventHandle,RNC_DISCOVERY_PROCESS_EVENT);
            SC_ProcessDiscoveredList();
        }
        if (event_active & RNC_SHADE_CONFIG_REQ_EVENT)
        {
            RNC_process_shade_confg_request();
        }
        if (event_active & RNC_RADIO_SERIAL_CONF_EVENT)
        {
            RNC_process_radio_serial_confirmation();
        }
        if (event_active & RNC_SHADE_SERIAL_CONF_EVENT)
        {
            RNC_process_shade_serial_confirmation();
        }
        if (event_active & RNC_SHADE_INDICATION_EVENT)
        {
            RNC_process_shade_indication();
        }
        if (event_active & RNC_SYSTEM_INDICATION_EVENT)
        {
            RNC_process_system_indication();
        }
        if (event_active & RNC_RF_TICK_EVENT)
        {
            OS_EventClear(RNC_EventHandle,RNC_RF_TICK_EVENT);
            OS_TimerSetCyclicInterval(RNC_RFTickTimer,RNC_TICK_INTERVAL);
            RNC_process_timeout();
//printf("tick\n");
        }
    }
}

/*****************************************************************************//**
* @brief This function initializes the shade configuration task and holds the<br/>
*  main loop of the task.
*
* @param temp is a unused variable.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/25/2014    Created.
*******************************************************************************/
static void RNC_process_radio_confg_request(void)
{
    RNC_CONFIG_REC_PTR p_cfg_rec = (RNC_CONFIG_REC_PTR)OS_MessageGet(RNC_RadioConfigRequestMbox);
    RFO_DeliverRequest(p_cfg_rec);
}


/*****************************************************************************//**
* @brief This function initializes the shade configuration task and holds the<br/>
*  main loop of the task.
*
* @param temp is a unused variable.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/25/2014    Created.
*******************************************************************************/
static void RNC_process_shade_confg_request(void)
{
    SHADE_COMMAND_INSTRUCTION_PTR p_cfg_rec = (SHADE_COMMAND_INSTRUCTION_PTR)OS_MessageGet(RNC_ShadeConfigRequestMbox);
    
    // printf("*** RNC_process_shade_confg_request ***\n");
    if (p_cfg_rec->cmd_type == SC_GROUP_ASSIGN) {
        sendShadeCommandInstructionToSlaveHubs(p_cfg_rec);
    }
    
    SC_LoadNewCommand(p_cfg_rec);
}

/*****************************************************************************//**
* @brief This function initializes the shade configuration task and holds the<br/>
*  main loop of the task.
*
* @param temp is a unused variable.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/25/2014    Created.
*******************************************************************************/
static void RNC_process_radio_serial_confirmation(void)
{
    PARSE_KEY_STRUCT_PTR p_ser_msg = (PARSE_KEY_STRUCT_PTR)OS_MessageGet(RNC_RadioSerialConfirmationMbox);
//printf("Process confirmation\n");
    RC_HandleRadioConfirmation(p_ser_msg);
}

/*****************************************************************************//**
* @brief This function ...
*
* @param temp is a unused variable.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/25/2014    Created.
*******************************************************************************/
static void RNC_process_shade_serial_confirmation(void)
{
    uint8_t * p_ser_stat = (uint8_t *)OS_MessageGet(RNC_ShadeSerialConfirmationMbox);
    SC_HandleShadeConfirmationResult(p_ser_stat);
}

static bool isDuplicate(uint8_t *message, uint16_t messageSize)
{
    bool duplicate;
    struct timeval currentTime;
    uint32_t elapsedMilliSeconds;
    uint16_t n = 0;



    gettimeofday(&currentTime, 0);
    elapsedMilliSeconds = (currentTime.tv_sec * 1000 + currentTime.tv_usec/1000)
        - (lastMessageTime.tv_sec * 1000 + lastMessageTime.tv_usec/1000);
    
    duplicate = (messageSize == lastMessageSize) && (elapsedMilliSeconds <= 500);
    while(duplicate && (n < messageSize)) {
        duplicate = (message[n] = lastMessage[n]);
        n++;
    }
    
    if(!duplicate) {
        lastMessageSize = messageSize;
        for(n = 0; n < messageSize; n++) lastMessage[n] = message[n];
    }
    
    gettimeofday(&lastMessageTime, 0);
    
    return duplicate;
}

/*****************************************************************************//**
* @brief This function ...
*
* @param temp is a unused variable.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/25/2014    Created.
*******************************************************************************/
static void RNC_process_shade_indication(void)
{
    // marker 12/10/2015 - catch the signal from the shade here
    PARSE_KEY_STRUCT_PTR p_rf_response = (PARSE_KEY_STRUCT_PTR)OS_MessageGet(RNC_ShadeIndicationMbox);


    uint16_t messageSize = p_rf_response->generic.length + 1; //+1 for length itself
    uint8_t *bytes;

    // check if the message "p_ser_msg" has been sent before
    bytes = (uint8_t *)p_rf_response;
    if(isDuplicate(bytes, messageSize)) {
        printf("found *duplicate* message\n");
        OS_ReleaseMsgMemBlock((uint8_t *)p_rf_response);
    }
    else {
    SC_HandleShadeIndication(p_rf_response);
}
}

/*****************************************************************************//**
* @brief This function initializes the shade configuration task and holds the<br/>
*  main loop of the task.
*
* @param temp is a unused variable.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/25/2014    Created.
*******************************************************************************/
static void RNC_process_timeout(void)
{
    SC_ProcessRFTimeout();
}

// marker 02/25/2016 - temporary procedure to display data
static void displaySystemIndication(SYSTEM_INDICATION_STRUCT_PTR sysIndPtr)
{
    uint8_t length = sysIndPtr->payload_len, i;

    printf("\nSystem Indication before sending it to slaves\n");
    printf("=============================================\n");
    printf("  payload length = %d\n", length);
    printf("  indication type = %d\n", sysIndPtr->indication_type);
    printf("  id = %d\n", sysIndPtr->id);
    printf("  payload = 0x");
    for(i = 0; i < length; i++) printf("%02x ", sysIndPtr->payload[i]);
    printf("\n      that is ");
    for(i = 0; i < length; i++) printf("%d ", sysIndPtr->payload[i]);
    printf("\n\n");
}
//-----------------------------------------------------------------------------

/*****************************************************************************//**
* @brief This function handles a nordic system message, reset or version.
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 05/05/2015    Created.
*******************************************************************************/
static void RNC_process_system_indication(void)
{
    SYSTEM_INDICATION_STRUCT_PTR p_cfg_rec = (SYSTEM_INDICATION_STRUCT_PTR)OS_MessageGet(RNC_SystemIndicationMbox);

    if (p_cfg_rec->id == 0x01) {
        RC_HandleResetIndication(p_cfg_rec);
    }
    else if (p_cfg_rec->id == 0x03) {
        RC_HandleVersionIndication(p_cfg_rec);
    }
    else if (p_cfg_rec->id == 5) {
        displaySystemIndication(p_cfg_rec);
        sendSystemIndicationToSlaveHubs(p_cfg_rec);
        OS_ReleaseMsgMemBlock(p_cfg_rec);
    }
    else {
        OS_ReleaseMsgMemBlock(p_cfg_rec);
    }
}

// marker 02/25/2016 - add process system indication
/*****************************************************************************//**
* @brief This function ...
*
* @param
* @return nothing.
* @author Henk Meewis
* @version
* 02/25/2016    Created.
*******************************************************************************/
/*
static void RNC_process_slave_system_indication(void)
{
    SYSTEM_INDICATION_STRUCT_PTR p_sys_ind = (SYSTEM_INDICATION_STRUCT_PTR)OS_MessageGet(RNC_SystemIndicationMbox);
    SC_HandleSystemIndicationFromMaster(p_sys_ind);
}
*/

/*****************************************************************************//**
* @brief This function ...
*
* <b>Note:</b>  This function runs in the context of the calling task.
* @return nothing.
* @author Neal Shurmantine
* @since 12-02-2014
* @version Initial revision.
*******************************************************************************/
void RNC_AddTransportLayer(uint8_t * p_msg_send, uint8_t * p_msg_raw)
{
//p_msg_raw = {14 0C 01 01 84 18 00 00 00 00 00 00 01 01 3F 5A 04 40 50 D5 5F}
    uint8_t len = p_msg_raw[0];
    uint16_t n;
    bool need_escape = false;
    uint8_t *p_source;
    uint8_t *p_dest;
    uint8_t chksum = 0;

    p_msg_send[0] = START_OF_HEADER;
    p_source = &p_msg_raw[1];
    p_dest = &p_msg_send[2];

    for (n = 0; n < len; ++n) {
        if (need_escape == true) {
            need_escape = false;
            *p_dest = *p_source & 0xbf;
            p_source++;
        }
        else if ((*p_source == START_OF_HEADER) || (*p_source == ESCAPE_TOKEN)) {
            ++len;
            need_escape = true;
            *p_dest = ESCAPE_TOKEN;
        }
        else {
            *p_dest = *p_source;
            p_source++;
        }
        chksum += *p_dest;
        p_dest++;
    }
    *p_dest = chksum;
    p_msg_send[1] = len;
//p_msg_send = {7E 14 0C 01 01 84 18 00 00 00 00 00 00 01 01 3F 5A 04 40 50 D5 5F 0D}
//checksum = SUM{0C 01 ... D5 5F}
}
//-----------------------------------------------------------------------------

// marker 05/02/2016 - new
void RNC_sendNetworkIDToSlaves(void)
{
    uint16_t networkId = RC_GetNetworkId();
    sendNetworkIDToSlaveHubs(networkId);
}
//-----------------------------------------------------------------------------

static void RNC_sendNetworkIDToSlavesWithToken(uint16_t token)
{
   if (SC_NetworkJoinScheduleToken != token) {
        SCH_RemoveScheduledEvent(SC_NetworkJoinScheduleToken);
    }
    RNC_sendNetworkIDToSlaves();
}
//-----------------------------------------------------------------------------

void scheduleToSendNetworkIdToSlaves(void)
{
    SCH_ScheduleEventPostSeconds(0, RNC_sendNetworkIDToSlavesWithToken);
}
//-----------------------------------------------------------------------------
