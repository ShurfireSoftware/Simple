/***************************************************************************//**
 * @file   RMT_RemoteServer.c
 * @brief  This module maintains the scheduling and handling of communicatioin
 *  with servers on the internet.  Included are the time server and remote
 *  connect server. The remote connect server includes firmware updates and
 *  remote actions from the app.
 * 
 * @author Neal Shurmantine
 * @copyright (c) 2015 Hunter Douglas. All rights reserved.
 *
 * @date Created: 04/18/2015
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
#include "RMT_RemoteServers.h"
#include "LOG_DataLogger.h"
#include "stub.h"

#ifdef USE_ME
#include <ipcfg.h>
#include "JSONParser.h"
#include "SPIFlashData.h"
#include "RDS_RemoteDataSync.h"
#include "mfs_sdcard.h"
#include <watchdog.h>
#include "Registration.h"
#include <mutex.h>
#include "UDPProcessing.h"
#endif

/* Global Variables
*******************************************************************************/
pthread_t RMT_RemoteServerTaskId;
extern char timeZoneName[ItsMaxTimeZoneNameLength_];
uint16_t RMT_RemoteActionToken = NULL_TOKEN;
uint16_t RMT_TimeServerTokenSeconds;
extern uint16_t LastPort;
extern MUTEX_STRUCT flashDeviceMutex;

/* Local Constants and Definitions
*******************************************************************************/
#define RMT_REMOTE_CONNECT_EVENT            BIT0
#define RMT_REMOTE_ACTION_EVENT             BIT1
#define RMT_FW_CHECK_EVENT                  BIT2
#define RMT_TIME_SERVER_EVENT               BIT3
#define RMT_FAULT_EVENT                     BIT4
#define RMT_REFRESH_REMOTE_DATA_EVENT       BIT5
#define RMT_REGISTER_EVENT                  BIT6
#define RMT_UNREGISTER_EVENT                BIT7
#define RMT_ACTION_RESPONSE_EVENT           BIT8

//check time server at:
#define RMT_FW_UPDATE_CHECK_HOUR            0
#define RMT_FW_UPDATE_CHECK_MINUTE          10
#define RMT_TIME_CHECK_HOUR                  2
#define RMT_TIME_CHECK_MINUTE               30
#define RMT_RANDOMIZE_MINUTES               30
#define RMT_CHECK_TIME_INTERVAL_FAIL                (5*MIN_IN_SEC)
#define RMT_CHECK_REMOTE_CONNECT_INTERVAL_START     (5*SEC)
#define RMT_CHECK_REMOTE_CONNECT_INTERVAL_FAIL      (1*MIN_IN_SEC)
#define RMT_CHECK_REMOTE_ACTION_INTERVAL_START      (10*SEC)
#define RMT_CHECK_REMOTE_ACTION_RETRY_INTERVAL      (10*SEC)
#define RMT_CHECK_FW_INTERVAL_START                 (1*SEC)
#define RMT_CHECK_FW_INTERVAL_FAIL                  (5*MIN_IN_SEC)
#define RMT_CHECK_FW_WAIT_NORDIC_DOWNLOAD           (2*SEC)
#define RMT_CHECK_TIME_START                        (2*SEC)
#define RMT_MAX_REMOTE_ACTION_CHECK_TIME        (30*SEC)
#define RMT_DEFAULT_REMOTE_ACTION_CHECK_TIME    (20*SEC)
#define RMT_REMOTE_ACTION_FAIL_CHECK_TIME           (5*MIN_IN_SEC)
#define RMT_WATCHDOG_INTERVAL                       (10*MIN_IN_MS)

typedef struct {
    char hub_id[ItsMaxHubIdLength_+1];
    char hub_name[ItsMaxNameLength_+1];
    char pv_key[ItsHubKeyLength_ +1];
} RMT_REG_DATA_STRUCT, *RMT_REG_DATA_STRUCT_PTR;

typedef struct {
    eActionStatus status;
    eActionMessageId msg;
    uint32_t id;
} RMT_ACTION_RESPONSE_STRUCT, *RMT_ACTION_RESPONSE_STRUCT_PTR;

/* Local Function Declarations
*******************************************************************************/
static void RMT_check_remote_connect_now(uint16_t unused);
static void RMT_check_time_server_daily(uint16_t unused);
static void RMT_handle_remote_connect(void);
static void RMT_handle_time_server(void);
static void RMT_handle_remote_action(void);
static void RMT_handle_action_response(uint16_t unused);
static void RMT_handle_fw_check(void);
static void RMT_report_fault(void);
static void RMT_refresh_remote_data(void);
static void RMT_register_hub(void);
static void RMT_unregister_hub(void);

/* Local variables
*******************************************************************************/
static uint16_t RMT_RegisterEventMbox;
static uint16_t RMT_ActionResponseMbox;
static uint32_t RMT_WaitTime;
static uint16_t RMT_ExpectedEvents;
static void *RMT_EventHandle;
static uint16_t RMT_TimeServerTokenDaily;
static bool RMT_RemoteConnectEnabled = false;
//static DAY_STRUCT RMT_DailyFWCheckTime;
static DAY_STRUCT RMT_DailyTimeCheckTime;
static bool RMT_RegistrationBusy = false;
static bool RMT_UseQAServer;
static char RMT_DomainAddress[MAX_DOMAIN_NAME_LENGTH];

/*****************************************************************************//**
* @brief Initialize the Task that schedules and handles communication with 
*    Servers on the internet.  
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 04/18/2015    Created
*******************************************************************************/
void RMT_InitRemoteServers(void)
{
    strcpy(RMT_DomainAddress,REMOTE_SERVER_CONSUMER_DOMAIN);
    retrieveRegistrationData();
    RMT_RemoteServerTaskId = OS_TaskCreate(RMT_REMOTE_SERVER_TASK_NUM, 0);
    SCH_ScheduleEventPostSeconds(RMT_CHECK_REMOTE_CONNECT_INTERVAL_START, RMT_check_remote_connect_now);
    RMT_TimeServerTokenSeconds = NULL_TOKEN;
    RMT_TimeServerTokenDaily = NULL_TOKEN;

//    RMT_DailyFWCheckTime.hour = RMT_FW_UPDATE_CHECK_HOUR;
//    RMT_DailyFWCheckTime.minute = RMT_FW_UPDATE_CHECK_MINUTE + SCH_Randomize(RMT_RANDOMIZE_MINUTES);
//    RMT_DailyFWCheckTime.second = SCH_Randomize(60);

    RMT_DailyTimeCheckTime.hour = RMT_TIME_CHECK_HOUR;
    RMT_DailyTimeCheckTime.minute = RMT_TIME_CHECK_MINUTE + SCH_Randomize(RMT_RANDOMIZE_MINUTES);
    RMT_DailyTimeCheckTime.second = SCH_Randomize(60);

    SCH_InitTimeVariables();
    FWU_FirmwareUpdateInit();
}

/*****************************************************************************//**
* @brief Return the api version used by the firmware.  This can be modified
*        with a file on the SD card. 
*
* @param pointer to string containing the resource version.
* @return nothing
* @author Neal Shurmantine
* @version
* 11/20/2015    Created
*******************************************************************************/
char * RMT_GetAPIVersion(void)
{
    return API_VERSION_TWO;
}

/*****************************************************************************//**
* @brief Returns the string representing the hardware version (19 or 20).
*
* @param char * points to hardware version string (20 if V2)
* @return none
* @author Neal Shurmantine
* @version
* 11/20/2015    Created
*******************************************************************************/
char * RMT_GetHWVersion(void)
{
    return "19";
}

/*****************************************************************************//**
* @brief Returns the string representing the host domain.
*
* @param none
* @return char * domain
* @author Neal Shurmantine
* @version
* 11/20/2015    Created
*******************************************************************************/
char * RMT_GetDomainName(void)
{
    return RMT_DomainAddress;
}

/*****************************************************************************//**
* @brief Callback function from the scheduler that kicks off a check for 
*    remote connect that runs within the RMT_Server task.
*
* @param unused.  Required parameter of callback from scheduler.
* @return nothing.
* @author Neal Shurmantine
* @version
* 04/18/2015    Created
*******************************************************************************/
static void RMT_check_remote_connect_now(uint16_t unused)
{
    OS_EventSet(RMT_EventHandle, RMT_REMOTE_CONNECT_EVENT);
}

/*****************************************************************************//**
* @brief Callback function from the scheduler that kicks off a check for 
*    a firmware update that runs within the RMT_Server task.
*
* @param unused.  Required parameter of callback from scheduler.
* @return nothing.
* @author Neal Shurmantine
* @version
* 04/18/2015    Created
*******************************************************************************/
void RMT_check_fw_update_now(uint16_t unused)
{
    if (SCH_IsHTTPActive() == true) {
        SCH_ScheduleEventPostSeconds(10, RMT_check_fw_update_now);
    }
    else {
        OS_EventSet(RMT_EventHandle, RMT_FW_CHECK_EVENT);
    }
}

/*****************************************************************************//**
* @brief Callback function from the scheduler that kicks off a check for 
*    the time from the time server that runs within the RMT_Server task.
*
* @param unused.  Required parameter of callback from scheduler.
* @return nothing.
* @author Neal Shurmantine
* @version
* 04/18/2015    Created
*******************************************************************************/
void RMT_CheckTimeServerNow(uint16_t unused)
{
    RMT_TimeServerTokenSeconds = NULL_TOKEN;
    OS_EventSet(RMT_EventHandle, RMT_TIME_SERVER_EVENT);
}

/*****************************************************************************//**
* @brief Callback function from the scheduler that kicks off a check for 
*    the time from the time server that runs within the RMT_Server task.
*
* @param unused.  Required parameter of callback from scheduler.
* @return nothing.
* @author Neal Shurmantine
* @version
* 04/18/2015    Created
*******************************************************************************/
static void RMT_check_time_server_daily(uint16_t unused)
{
    RMT_TimeServerTokenDaily = NULL_TOKEN;
    OS_EventSet(RMT_EventHandle, RMT_TIME_SERVER_EVENT);
}

/*****************************************************************************//**
* @brief Callback function from the scheduler that kicks off a check for 
*    a remote action that runs within the RMT_Server task.
*
* @param unused.  Required parameter of callback from scheduler.
* @return nothing.
* @author Neal Shurmantine
* @version
* 04/18/2015    Created
*******************************************************************************/
void RMT_CheckRemoteActionNow(uint16_t unused)
{
    OS_EventSet(RMT_EventHandle, RMT_REMOTE_ACTION_EVENT);
}

/*****************************************************************************//**
* @brief Call this function when an action was receive from remote connect that
*    requires a separate PUT response.
*
* @param status.  id of message status.
* @param msg.  Id of message to send.
* @param id.  Id of action response
* @return nothing.
* @author Neal Shurmantine
* @version
* 02/09/2016    Created
*******************************************************************************/
void RMT_SendActionResponse(eActionStatus status, eActionMessageId msg, uint32_t id)
{
    RMT_ACTION_RESPONSE_STRUCT_PTR p_rsp_data = (RMT_ACTION_RESPONSE_STRUCT_PTR)OS_GetMsgMemBlock(sizeof(RMT_ACTION_RESPONSE_STRUCT));
    p_rsp_data->status = status;
    p_rsp_data->msg = msg;
    p_rsp_data->id = id;
    OS_MessageSend(RMT_ActionResponseMbox,p_rsp_data);
}

/*****************************************************************************//**
* @brief One or more batteries have been detected as low, report to remote server.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 09/08/2015    Created.
*******************************************************************************/
void RMT_FaultNotification(uint16_t unused)
{
    OS_EventSet(RMT_EventHandle, RMT_FAULT_EVENT);
}

/*****************************************************************************//**
* @brief One or more databases have been modified so inform remote connect.
*
* @param uint16_t unused. Required for a schedule callback
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/04/2015    Created.
*******************************************************************************/
void RMT_RefreshRemoteServerData(uint16_t unused)
{
    if (getHubKey()[0] && getEmail()[0]) {
        OS_EventSet(RMT_EventHandle, RMT_REFRESH_REMOTE_DATA_EVENT);
    }
    else {
        printf("Skip sync, hub not registered\n");
    }
}

/*****************************************************************************//**
* @brief Initiate registration of the hub with remote connect.
*
* @param hub_id.  Pointer to string containing Hub ID
* @param hub_name.  Pointer to string containing Hub name
* @param pv_key.  Pointer to string containing PowerView Key.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/16/2015    Created.
*******************************************************************************/
void RMT_RegisterHub(char *p_hub_id, char *p_hub_name, char *p_pv_key)
{
    RMT_RegistrationBusy = true;
    RMT_REG_DATA_STRUCT_PTR p_reg_data = (RMT_REG_DATA_STRUCT_PTR)OS_GetMsgMemBlock(sizeof(RMT_REG_DATA_STRUCT));
    strcpy(p_reg_data->hub_id, p_hub_id);
    strcpy(p_reg_data->hub_name, p_hub_name);
    strcpy(p_reg_data->pv_key, p_pv_key);
    OS_MessageSend(RMT_RegisterEventMbox,p_reg_data);
}

/*****************************************************************************//**
* @brief Initiate the unregistration of the hub with remote connect.
*
* @param hub_id.  Pointer to string containing Hub ID
* @return nothing.
* @author Neal Shurmantine
* @version
* 12/08/2015    Created.
*******************************************************************************/
void RMT_UnRegisterHub(void)
{
    RMT_RegistrationBusy = true;
    OS_EventSet(RMT_EventHandle, RMT_UNREGISTER_EVENT);
}

/*****************************************************************************//**
* @brief Returns whether registration with remote connect is actively occurring.
*
* @param none
* @return True or false.
* @author Neal Shurmantine
* @version
* 11/16/2015    Created.
*******************************************************************************/
bool RMT_IsRegistrationBusy(void)
{
    return RMT_RegistrationBusy;
}

/*****************************************************************************//**
* @brief This function is the task that handles the communication with the
*  remote servers.  Each communication may take several seconds so this
*  tasks allows the communication to occur in its own thread so it doesn't
*  block the scheduler. 
*
* @param temp is a unused variable.
* @return nothing.
* @author Neal Shurmantine
* @version
* 04/19/2015    Created.
*******************************************************************************/
void *RMT_remote_server_task(void * temp)
{
    //holds the event bits that were set when the task wakes up
    uint16_t event_active;
    
    //the task will wake up on this timeout if no events have occurred.
    RMT_WaitTime = WAIT_TIME_INFINITE;

    //create the event for this task plus mailboxes and timer
    RMT_EventHandle = OS_EventCreate(0,false);

    RMT_RegisterEventMbox = OS_MboxCreate(RMT_EventHandle,RMT_REGISTER_EVENT); 
    RMT_ActionResponseMbox = OS_MboxCreate(RMT_EventHandle,RMT_ACTION_RESPONSE_EVENT); 
    RMT_ExpectedEvents = RMT_REMOTE_CONNECT_EVENT
                        | RMT_REMOTE_ACTION_EVENT
                        | RMT_FW_CHECK_EVENT
                        | RMT_TIME_SERVER_EVENT
                        | RMT_FAULT_EVENT
                        | RMT_REFRESH_REMOTE_DATA_EVENT
                        | RMT_REGISTER_EVENT
                        | RMT_UNREGISTER_EVENT
                        | RMT_ACTION_RESPONSE_EVENT;

printf("RMT_remote_server_task\n");
    while(1) {
        _watchdog_stop();
        event_active = OS_TaskWaitEvents(RMT_EventHandle, RMT_ExpectedEvents, RMT_WaitTime);
        event_active &= RMT_ExpectedEvents;
        _watchdog_start(RMT_WATCHDOG_INTERVAL);
        if (event_active & RMT_TIME_SERVER_EVENT) {
            OS_EventClear(RMT_EventHandle,RMT_TIME_SERVER_EVENT);
            RMT_handle_time_server();
        }
        if (event_active & RMT_REMOTE_CONNECT_EVENT) {
            OS_EventClear(RMT_EventHandle,RMT_REMOTE_CONNECT_EVENT);
            RMT_handle_remote_connect();
        }
        if (event_active & RMT_FW_CHECK_EVENT) {
            OS_EventClear(RMT_EventHandle,RMT_FW_CHECK_EVENT);
            
            // marker 05/03/2016 - send firmware check request to slave hubs (access points)
            RMT_handle_fw_check();
            sendTextMessageToSlaveHubs("check firmware");
        }
        if (event_active & RMT_REMOTE_ACTION_EVENT) {
            OS_EventClear(RMT_EventHandle,RMT_REMOTE_ACTION_EVENT);
            RMT_handle_remote_action();
        }
        if (event_active & RMT_FAULT_EVENT) {
            OS_EventClear(RMT_EventHandle,RMT_FAULT_EVENT);
            RMT_report_fault();
        }
        if (event_active & RMT_REFRESH_REMOTE_DATA_EVENT) {
            OS_EventClear(RMT_EventHandle,RMT_REFRESH_REMOTE_DATA_EVENT);
            RMT_refresh_remote_data();
        }
        if (event_active & RMT_REGISTER_EVENT) {
            RMT_register_hub();
        }
        if (event_active & RMT_UNREGISTER_EVENT) {
            OS_EventClear(RMT_EventHandle,RMT_UNREGISTER_EVENT);
            RMT_unregister_hub();
        }
        if (event_active & RMT_ACTION_RESPONSE_EVENT) {
            RMT_handle_action_response(NULL_TOKEN);
        }
    }
}

/*****************************************************************************//**
* @brief Start the process of registering the hub with remote connect within
*        the context of RMT_RemoteServers task..
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/16/2015    Created.
*******************************************************************************/
static void RMT_register_hub(void)
{
    RMT_REG_DATA_STRUCT_PTR p_reg_data = (RMT_REG_DATA_STRUCT_PTR)OS_MessageGet(RMT_RegisterEventMbox);
    RCR_RegisterWithRemoteConnect(p_reg_data->hub_id, p_reg_data->hub_name, p_reg_data->pv_key);
    OS_ReleaseMsgMemBlock((void *)p_reg_data);
    RMT_RegistrationBusy = false;
}

/*****************************************************************************//**
* @brief Start the process of unregistering the hub with remote connect within
*        the context of RMT_RemoteServers task..
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 12/08/2015    Created.
*******************************************************************************/
static void RMT_unregister_hub(void)
{
    RCR_UnregisterHub();
    RMT_RegistrationBusy = false;
}

/*****************************************************************************//**
* @brief Start the process of synchronizing the hub database with remote connect
*        from within the RMT_RemoteServers  task.
*
* @param none
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/04/2015    Created.
*******************************************************************************/
static void RMT_refresh_remote_data(void)
{
    if (isRegistrationActive() == true) {
        RDS_SyncDataWithRemoteConnect();
    }
    else {
        if (_mutex_try_lock(&flashDeviceMutex) != MQX_EOK) {
            RDS_SyncDataImmediately(NULL_TOKEN);
//            printf("RMT Sync could not get mutex\n");
        }
        else {
            RDS_SyncDataWithRemoteConnect();
            _mutex_unlock(&flashDeviceMutex);
        }
    }
}

/*****************************************************************************//**
* @brief One or more batteries have been detected as low, report to remote server.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 09/08/2015    Created.
*******************************************************************************/
static void RMT_report_fault(void)
{
    LOG_LogEvent("Low Batt Reported");
    uint16_t count = SC_GetLowBatteryCount();
    RFS_ReportFaults(count);
}

/*****************************************************************************//**
* @brief Timer has expired to check remote connect.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 02/17/2015    Created.
*******************************************************************************/
static void RMT_handle_remote_connect(void)
{
    if (ResolveIpAddress(RMT_DomainAddress) == true) {
        SCH_ScheduleEventPostSeconds(RMT_CHECK_FW_INTERVAL_START,
                                            RMT_check_fw_update_now);
        RMT_RemoteActionToken = SCH_ScheduleEventPostSeconds(RMT_CHECK_REMOTE_ACTION_INTERVAL_START,
                                            RMT_CheckRemoteActionNow);
        if (strlen(timeZoneName) > 0) {
            RMT_ScheduleTimeServerCheckInSeconds(RMT_CHECK_TIME_START);
        }
        RDS_TriggerRemoteSync(NULL_TOKEN);
    }
    else {
        printf("could NOT resolve Hunter Douglas server %s\n",RMT_DomainAddress);
        SCH_ScheduleEventPostSeconds(RMT_CHECK_REMOTE_CONNECT_INTERVAL_FAIL, 
                                            RMT_check_remote_connect_now);
    }
}

/*****************************************************************************//**
* @brief Timer has expired to check for remote action.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 02/17/2015    Created.
*******************************************************************************/
static void RMT_handle_remote_action(void)
{
    static uint16_t error_count = 0;
    uint32_t next_check;
    char pin[5];
    bool is_pin;

    is_pin = true;

    if ( is_pin == true ) {
        if (isHubRegistered() == false) {
            is_pin = false;
        }
    }

    if ( is_pin == true ) {
        RMT_SetPin(pin);
        if (RMT_RemoteConnectEnabled == false) {
            RMT_RemoteConnectEnabled = true;
            printf("Remote Actions Enabled\n");
            LOG_LogEvent("Remote Actions Enabled");
        }
        if (isRegistrationActive() == false) {

            while (_mutex_try_lock(&flashDeviceMutex) != MQX_EOK) {
                OS_TaskSleep(100);
            }
            next_check = RAS_CheckActionUpdate();
            _mutex_unlock(&flashDeviceMutex);
            eRestClientStatus err = RAS_GetStatus();
            if (err == eFWU_OK) {
                error_count = 0;
                //temporarily increase the check-in time for V2
                uint32_t max;
                uint32_t default_time;
    
                max = RMT_MAX_REMOTE_ACTION_CHECK_TIME;
                default_time = RMT_DEFAULT_REMOTE_ACTION_CHECK_TIME;
                if (next_check > max) {
                    next_check = default_time;
                }
            }
            else {
                ++error_count;
                if (error_count > 5) {
                    next_check = RMT_REMOTE_ACTION_FAIL_CHECK_TIME;
                }
                else {
                    next_check = RMT_DEFAULT_REMOTE_ACTION_CHECK_TIME;
                }
                char s[MAX_TAG_LABEL_SIZE];
                sprintf(s,"Remote action error=%d",err);
                LOG_LogEvent(s);
                printf("%s\n",s);
            }
        }
        else {
            printf("Postponed Remote Action call\n");
            next_check = 1;
        }
    }
    else {
        if (RMT_RemoteConnectEnabled == true) {
            RMT_RemoteConnectEnabled = false;
            RDS_TriggerRemoteSync(NULL_TOKEN);
            LOG_LogEvent("Remote Actions Disabled");
            printf("Remote Actions Disabled\n");
        }
        next_check = RMT_DEFAULT_REMOTE_ACTION_CHECK_TIME;
    }
    RMT_RemoteActionToken = SCH_ScheduleEventPostSeconds(next_check,
                                            RMT_CheckRemoteActionNow);
}

/*****************************************************************************//**
* @brief Begin a PUT response to a remote action.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 02/09/2016    Created.
*******************************************************************************/
static void RMT_handle_action_response(uint16_t unused)
{
    while (_mutex_try_lock(&flashDeviceMutex) != MQX_EOK) {
//        printf("Action Response waiting for mutex\n");
        OS_TaskSleep(100);
    }
    RMT_ACTION_RESPONSE_STRUCT_PTR p_rsp_data = (RMT_ACTION_RESPONSE_STRUCT_PTR)OS_MessageGet(RMT_ActionResponseMbox);
    RAS_SendActionResponse(p_rsp_data->status, p_rsp_data->msg, p_rsp_data->id);
    OS_ReleaseMsgMemBlock((void *)p_rsp_data);
    _mutex_unlock(&flashDeviceMutex);
}

/*****************************************************************************//**
* @brief Timer has expired to check for firmware update.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 02/17/2015    Created.
*******************************************************************************/
static void RMT_handle_fw_check(void)
{
    printf("No firmware update\n");
}

/*****************************************************************************//**
* @brief Schedule a call to the remote time server to occur in a specified number
*      of seconds.  If there is already a pending schedule then remove it first.
*
* @param num_seconds.  Number of seconds from now to check server.
* @return nothing.
* @author Neal Shurmantine
* @version
* 05/19/2015    Created.
*******************************************************************************/
void RMT_ScheduleTimeServerCheckInSeconds(uint32_t num_seconds)
{
    RMT_TimeServerTokenSeconds = SCH_ScheduleEventPostSeconds(num_seconds, 
                                            RMT_CheckTimeServerNow);
}

/*****************************************************************************//**
* @brief Check the time server.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 05/11/2015    Created.
*******************************************************************************/
static void RMT_handle_time_server(void)
{
    TIME_UPDATE_DATA_PTR p_time_data;
    while (_mutex_try_lock(&flashDeviceMutex) != MQX_EOK) {
//        printf("Remote Time waiting for mutex\n");
        OS_TaskSleep(100);
    }
    p_time_data = (TIME_UPDATE_DATA_PTR)RTS_CheckTimeUpdate();
    if ( p_time_data != NULL ) {
        SCH_ProcessNewTime(p_time_data);
        //remove a previously scheduled check if it exists
        if (RMT_TimeServerTokenDaily != NULL_TOKEN) {
            SCH_RemoveScheduledEvent(RMT_TimeServerTokenDaily);
        }
        RMT_TimeServerTokenDaily = SCH_ScheduleDaily(&RMT_DailyTimeCheckTime, 
                RMT_check_time_server_daily);
    }
    else {
        RMT_ScheduleTimeServerCheckInSeconds(RMT_CHECK_TIME_INTERVAL_FAIL);
    }
    _mutex_unlock(&flashDeviceMutex);
}

