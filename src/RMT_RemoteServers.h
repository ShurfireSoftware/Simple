/***************************************************************************//**
 * @file RMT_RemoteServers.h
 * @brief Include file for RMT_RemoteServers module.
 *
 * @author Neal Shurmantine
 * @copyright (c) 2015 Hunter Douglas. All rights reserved.
 *
 * @version
 * 04/18/2015   Created.
 ******************************************************************************/

#ifndef _RMT_REMOTE_SERVERS_TASK_H_
#define _RMT_REMOTE_SERVERS_TASK_H_

#include "rest_client.h"

typedef enum {
    ACTION_STATUS_PENDING = 0,
    ACTION_STATUS_SUCCESS = 1,
    ACTION_STATUS_FAIL = 2,
    ACTION_STATUS_NULL = 3
} eActionStatus;

typedef enum {
    ACTION_MESSAGE_PENDING = 0,
    ACTION_MESSAGE_SUCCESS = 1,
    ACTION_MESSAGE_AUTHEN_FAIL = 2,
    ACTION_MESSAGE_TIMEOUT = 3,
    ACTION_MESSAGE_INVALID_TYPE = 4,
    ACTION_MESSAGE_INVALID_RESOURCE = 5,
    ACTION_MESSAGE_UNKNOWN = 6
} eActionMessageId;

typedef enum {
    ACTION_TYPE_NONE = 0,
    ACTION_TYPE_ACTIVATE_SCENE = 1,
    ACTION_TYPE_ACTIVATE_SCENE_COLLECTION = 2,
    ACTION_TYPE_ENABLE_SCHEDULES = 3,
    ACTION_TYPE_DISABLE_SCHEDULES = 4,
    ACTION_TYPE_CLEAR_NEST = 5
} eActionType;

typedef struct {
    bool                actionActive;
    uint32_t            nextUpdate;
    uint32_t            id;
    eActionType         type;
    eActionStatus       status;
    eActionMessageId    messageId;
    uint16_t            resourceId1;
    bool                nest_rhr_available;
    bool                nest_away;
    bool                schedule_modified;
    bool                nest_cleared;
    struct tm         nest_rhrStart;
    struct tm         nest_rhrEnd;
} HUBACTION_DATA, *HUBACTION_DATA_PTR;

#define MAX_VERSION_STRING_LENGTH       9

#define API_VERSION_ONE "/api/v1/"
#define API_VERSION_TWO "/api/v2/"
#define API_VERSION_THREE "/api/v3/"

#define REMOTE_SERVER_CONSUMER_DOMAIN "homeauto.hunterdouglas.com"
#define REMOTE_SERVER_QA_DOMAIN "betahome.hunterdouglas.com"

#define HW_VERSION              "19"

//http://homeauto.hunterdouglas.com/api/v1/times?tz=America%2FDenver
//http://homeauto.hunterdouglas.com/api/v1/times?tz=America%2FDenver&lat=40&lon=-105

#define HUB_FW_GET_RESOURCE                 "%sfirmware?revision=%d&hardware=%s"
#define HUB_TIME_GET_RESOURCE_NO_LAT_LON    "%stimes?tz=%s"
#define HUB_TIME_GET_RESOURCE               "%stimes?tz=%s&lat=%s&lon=%s"
#define HUB_REGISTER_POST_RESOURCE          "%shubRegistration/"
#define HUB_REGISTER_DELETE_RESOURCE        "%shubRegistration/%s"
#define HUB_SYNC_RESOURCE                   "%shubData/"
#define HUB_ACTION_PUT_RESOURCE             "%sactions/%d"
#define HUB_ACTION_GET_RESOURCE             "%shubActions"
#define HUB_FAULT_POST_RESOURCE             "%slowBatteryNotifications?count=%d"

#define HUB_ACTION_JSON_RESPONSE "{\"action\":{\"status\":%d,\"messageId\":%d}}"

void RMT_CheckTimeServerNow(uint16_t unused);
void RMT_InitRemoteServers(void);
void RMT_ScheduleTimeServerCheckInSeconds(uint32_t num_seconds);
void RMT_CheckRemoteActionNow(uint16_t unused);
void RMT_SendActionResponse(eActionStatus status, eActionMessageId msg, uint32_t id);
void RMT_FaultNotification(uint16_t unused);
void RMT_RefreshRemoteServerData(uint16_t unused);
void RMT_RegisterHub(char *p_hub_id, char *p_hub_name, char *p_pv_key);
void RMT_UnRegisterHub(void);
bool RMT_IsRegistrationBusy(void);
char * RMT_GetAPIVersion(void);
char * RMT_GetHWVersion(void);
char * RMT_GetDomainName(void);
void RMT_check_fw_update_now(uint16_t unused);

//in RAS_RemoteActionServer.c
uint32_t RAS_CheckActionUpdate(void);
uint32_t RAS_SendActionResponse(eActionStatus status, eActionMessageId msg, uint32_t action_id);
eRestClientStatus RAS_GetStatus(void);
bool RAS_IsNestActionsActive(void);
void RAS_ProcessActionResponseJSON(char *p_buff, HUBACTION_DATA *hubActionData,
                        eActionStatus *status,
                        eActionMessageId *msg);

//in FWU_FirmwareUpdate.c
uint32_t FWU_GetFirmwareVersion(void);
void FWU_FirmwareUpdateInit(void);
uint32_t FWU_CheckForUpdate(void);
eRestClientStatus FWU_GetStatus(void);

//in RTS_RemoteTimeServer
uint32_t RTS_CheckTimeUpdate(void);

//in NBT_NordicBootloadTask
uint32_t NBT_GetNordicFirmwareVersion(void);
void NBT_BeginNordicDownload(void);
bool NBT_VerifyNordicFiles(void);
bool NBT_IsNordicDownloadActive(void);

//in RFS_RemoteFaultServer
uint32_t RFS_ReportFaults(uint16_t total);

//in RCR_RemoteConnectRegister
bool RCR_RegisterWithRemoteConnect(char *hub_id, char *hub_name, char *pv_key);
bool RCR_UnregisterHub(void);
eRestClientStatus RCR_GetStatus(void);
char *RCR_GetErrorCode(void);

void RMT_check_fw_update_now(uint16_t unused);
bool verify_pin_another_time(char *p_pin);

#endif
