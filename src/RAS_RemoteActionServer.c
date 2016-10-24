/***************************************************************************//**
 * @file   RAS_RemoteActionServer.c
 * @brief  This module contains functions to communicate with the Remote Connect
 *         server to get remote actions
 *
 * @author Neal Shurmantine
 * @copyright (c) 2015 Hunter Douglas. All rights reserved.
 *
 * @version
 * 02/24/2015   Created.
 * 05/14/2015   Renamed from REC_RemoteConnect.c
 ******************************************************************************/

/* Includes
*******************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "util.h"
#include "os.h"
#include "config.h"
#include "SCH_ScheduleTask.h"
#include "RMT_RemoteServers.h"
#include "LOG_DataLogger.h"
#include "stub.h"
#include "JSONParser_v2.h"

#ifdef USE_ME

#include "sceneProcessing.h"
#include "MultiSceneProcessing.h"
#include "LOG_DataLogger.h"
#include "IntegrationsProcessing.h"
#include "RDS_RemoteDataSync.h"
#endif

/* Global Variables
*******************************************************************************/

/* Local Constants and Definitions
*******************************************************************************/
#define ENABLE_JSON_PRINT

#define ENABLE_SSL_ON_RAS_SERVER            1

#define REMOTE_CONNECT_DEFAULT_CHECK_TIME   20
#define REMOTE_CONNECT_CHECK_TIME_NOW       1


#if ENABLE_SSL_ON_RAS_SERVER

// The location of the root certificate file
// This is only needed if you want to authenticate the server
// #define CA_FILE_RAS_SERVER  "cert:"              // to use a certificate in memory
// #define CA_FILE_RAS_SERVER  "c:\\hdcert.pem"     // to use a certificate on the SD card


// The parameters needed to initialize SSL to talk to the hunter douglas server
static const RTCS_SSL_PARAMS_STRUCT hunterDouglasRASServerSSLParameters =
{
    NULL,                   /* Client or Server Certificate file.*/
    NULL,                   /* Client or Server private key file.*/
#ifdef CA_FILE_RAS_SERVER
    CA_FILE_RAS_SERVER,     /* CA (Certificate Authority) certificate file.*/
#else
    NULL,
#endif
    RTCS_SSL_CLIENT,
    NULL,
#ifdef CA_FILE_RAS_SERVER
    false                       // do not disable server verification
#else
    true                        // disable server verification
#endif
};

#endif

/* Local Function Declarations
*******************************************************************************/
static bool parse_nest_object(JSON_PARSE_OBJECT *nestObject, HUBACTION_DATA_PTR hubactionData);
static void init_hubaction_data(HUBACTION_DATA_PTR hubactionData);
static bool parse_integration_object(JSON_PARSE_OBJECT *integrationsObject, HUBACTION_DATA_PTR hubactionData);
static bool parse_action_object(JSON_PARSE_OBJECT *actionObject, HUBACTION_DATA_PTR hubactionData);
static bool parse_next_update_object(JSON_PARSE_OBJECT *nextUpdateObject, HUBACTION_DATA_PTR hubactionData);
static bool parse_hubaction_update_data(char * serverResponse, HUBACTION_DATA_PTR hubactionData);
static bool parse_action_object_put_return(JSON_PARSE_OBJECT *actionObject, HUBACTION_DATA_PTR hubactionData);
static uint32_t put_action_response_return(REST_CLIENT_QUERY_STRUCT_PTR p_query);
static uint32_t action_update_get_return(REST_CLIENT_QUERY_STRUCT_PTR p_query);
static eActionStatus processAction(HUBACTION_DATA_PTR p_hubAction);
static void print_json_put_response_data(HUBACTION_DATA_PTR p_hub_action_data);
static void print_json_data(HUBACTION_DATA_PTR p_hub_action_data);
static void processNestAction(HUBACTION_DATA_PTR hubactionData);
static bool isCurrentTimeInWindow(struct tm * begin, struct tm * end);

/* Local variables
*******************************************************************************/
static uint32_t RAS_RemoteConnectCheckTime;
static eRestClientStatus RAS_Status = eFWU_OK;
static bool RAS_RushHourActive = false;
static bool RAS_IsAway = false;

/*****************************************************************************//**
* @brief This function parses the nest object withing the json response from
*       the remote server to the GET.
*
* @param nestObject.  Pointer to the json data with nest
* @param hubactionData.  Structure containing the relevant messages values.
* @return bool.  True if the correct format.
* @author Neal Shurmantine
* @version
* 02/01/2016    Created.
*******************************************************************************/
static bool parse_nest_object(JSON_PARSE_OBJECT *nestObject, HUBACTION_DATA_PTR hubactionData)
{
    bool dataComplete = false;
    JSON_PARSE_OBJECT valObject;

    if (jv2_isObjectNull(nestObject)==true) {
        RAS_RushHourActive = false;
        RAS_IsAway = false;
        return true;
    }
    if (jv2_findObject(nestObject,"away",&valObject)) {
        dataComplete = jv2_getObjectBool(&valObject,&hubactionData->nest_away);
    }
    if (dataComplete == true) {
        if (jv2_findObject(nestObject,"rhrEnabled",&valObject)) {
            dataComplete = jv2_getObjectBool(&valObject,&hubactionData->nest_rhr_available);
            if ((dataComplete == true) && (hubactionData->nest_rhr_available == true) ) {
                if (jv2_findObject(nestObject,"rhrStartTime",&valObject)) {
                    if (jv2_isObjectNull(&valObject)==true) {
                        hubactionData->nest_rhr_available = false;
                    }
                    else {
                        dataComplete &= jv2_getObjectUTC(&valObject,&hubactionData->nest_rhrStart);
                    }
                }
            }
            if ((dataComplete == true) && (hubactionData->nest_rhr_available == true) ) {
                if (jv2_findObject(nestObject,"rhrEndTime",&valObject)) {
                    if (jv2_isObjectNull(&valObject)==true) {
                        hubactionData->nest_rhr_available = false;
                    }
                    else {
                        dataComplete &= jv2_getObjectUTC(&valObject,&hubactionData->nest_rhrEnd);
                    }
                }
            }
        }
    }

    if (dataComplete == true) {
        processNestAction(hubactionData);
    }
    else {
        RAS_RushHourActive = false;
        RAS_IsAway = false;
    }
    return dataComplete;
}

/*****************************************************************************//**
* @brief Examine Nest object and execute scene if appropriate.
*
* @param hubactionData.  Pointer to hubactionData data
* @return nothing
* @author Neal Shurmantine
* @version
* 02/08/2016    Created.
*******************************************************************************/
static void processNestAction(HUBACTION_DATA_PTR hubactionData)
{
    bool rhr_time_in_window =  false;
    bool away_active = false;

    if ((isNestRushHourEnabled() == true) && (hubactionData->nest_rhr_available == true)) {
        rhr_time_in_window = isCurrentTimeInWindow(&hubactionData->nest_rhrStart, &hubactionData->nest_rhrEnd);
    }
    if ((isNestHomeAwayEnabled() == true) && (hubactionData->nest_away == true)) {
        away_active = true;
    }

    //if user changed the scene to be executed by Nest then
    //  execute the new scene even if away or rhr is already active
    if (RAS_IsAway == true) {
        if (isNewAwayScene() == true) {
            RAS_IsAway = false;
        }
    }
    else {
        isNewAwayScene();  //clear flag in integrations processing anyway
    }
    if (RAS_RushHourActive == true) {
        if (isNewRHRScene() == true) {
            RAS_RushHourActive = false;
        }
    }
    else {
        isNewRHRScene();  //clear flag in integrations processing anyway
    }

    if ((RAS_RushHourActive == false) && (RAS_IsAway == false)) {
        if (rhr_time_in_window == true) {
            RAS_RushHourActive = true;
            executeRHRScene();
        }
        else if (away_active == true) {
            RAS_IsAway = true;
            executeAwayScene();
        }
    }
    else if (RAS_RushHourActive == true) {
        if ((away_active == true) && (rhr_time_in_window == false) ) {
            RAS_RushHourActive = false;
            RAS_IsAway = true;
            executeAwayScene();
        }
        else if ((away_active == false) && (rhr_time_in_window == false) ) {
            RAS_RushHourActive = false;
        }
    }
    else if (RAS_IsAway == true) {
        if (rhr_time_in_window == true) {
            RAS_IsAway = false;
            RAS_RushHourActive = true;
            executeRHRScene();
        }
        else if (away_active == false) {
            RAS_IsAway = false;
        }
    }
}

/*****************************************************************************//**
* @brief Determine if Nest Rush Hour Reward time interval includes the current
*      time.
*
* @param  begin. DATE_STRUCT_PTR
* @param  end. DATE_STRUCT_PTR
* @return true if Nest RHR is currently active
* @author Neal Shurmantine
* @version
* 02/08/2016    Created.
*******************************************************************************/
static bool isCurrentTimeInWindow(struct tm * p_begin, struct tm * p_end)
{
    time_t now;
    time_t upper;
    time_t lower;
    time(&now);
    lower = mktime(p_begin);
    upper = mktime(p_end);
    if ((now >= lower) && (now < upper))
        return true;
    else
        return false;
}

/*****************************************************************************//**
* @brief Determine if Nest action is currently active.  Used by scene scheduler
*     to determine if scheduled scene should activate.
*
* @param none.
* @return true if Nest action is currently active
* @author Neal Shurmantine
* @version
* 02/08/2016    Created.
*******************************************************************************/
bool RAS_IsNestActionsActive(void)
{
    bool rslt;
    OS_SchedLock();
    if (isNestHomeAwayEnabled() == false) RAS_IsAway = false;
    if (isNestRushHourEnabled() == false) RAS_RushHourActive = false;
    rslt = ((RAS_IsAway == true) || (RAS_RushHourActive == true));
    OS_SchedUnlock();
    return rslt;
}

/*****************************************************************************//**
* @brief This function initializes the hubactionData structure before a GET is sent.
*
* @param hubactionData.  Pointer to hubactionData object.
* @return nothing
* @author Neal Shurmantine
* @version
* 02/01/2016    Created.
*******************************************************************************/
static void init_hubaction_data(HUBACTION_DATA_PTR hubactionData)
{
    hubactionData->nest_rhr_available = false;
    hubactionData->actionActive = false;
    hubactionData->schedule_modified = false;
    hubactionData->nest_cleared = false;
    hubactionData->id = 0;
    hubactionData->status = ACTION_STATUS_NULL;
    hubactionData->type = ACTION_TYPE_NONE;
    hubactionData->resourceId1 = 0;
    hubactionData->messageId = ACTION_MESSAGE_UNKNOWN;
}

/*****************************************************************************//**
* @brief This function parses the integration object withing the json response from
*       the remote server to the GET.  Currently only nest is used
*
* @param integrationsObject.  Pointer to the json data with integration
* @param hubactionData.  Structure containing the relevant messages values.
* @return bool.  True if the correct format.
* @author Neal Shurmantine
* @version
* 02/01/2016    Created.
*******************************************************************************/
static bool parse_integration_object(JSON_PARSE_OBJECT *integrationsObject, HUBACTION_DATA_PTR hubactionData)
{
    bool dataComplete = false;
    JSON_PARSE_OBJECT nestObject;

    if(jv2_findObject(integrationsObject,"nest",&nestObject)) {
        dataComplete = parse_nest_object(&nestObject, hubactionData);
    }
    return dataComplete;
}

/*****************************************************************************//**
* @brief This function parses the action object withing the json response from
*       the remote server to the GET.
*
* @param actionObject.  Pointer to the json data with actions
* @param hubactionData.  Structure containing the relevant messages values.
* @return bool.  True if the correct format.
* @author Neal Shurmantine
* @version
* 02/01/2016    Created.
*******************************************************************************/
static bool parse_action_object(JSON_PARSE_OBJECT *actionObject, HUBACTION_DATA_PTR hubactionData)
{
	JSON_PARSE_OBJECT   idObject, typeObject, resourceId1Object, statusObject;
	JSON_PARSE_OBJECT   messageIdObject;
    bool                dataComplete = false;
    bool itsGood;

    if (jv2_isObjectNull(actionObject)==true) {
        return true;
    }
    if(jv2_findObject(actionObject,"id",&idObject)) {
        dataComplete = jv2_getObjectUint32(&idObject,&hubactionData->id);
    }

    if (dataComplete == true) {
        if(jv2_findObject(actionObject,"type",&typeObject)) { //1,2,3,4
            itsGood = jv2_getObjectInteger(&typeObject,(uint16_t*)&hubactionData->type);
            dataComplete &= itsGood;
            if ((itsGood == true)
                    && ((hubactionData->type < ACTION_TYPE_ACTIVATE_SCENE)
                    || (hubactionData->type > ACTION_TYPE_CLEAR_NEST))) {
//printf("Fail1\n");
                dataComplete=false;
            }
        }
        else {
//printf("Fail2\n");
            dataComplete=false;
        }
    }
//    else {
//printf("Fail3\n");
//    }

    if ((dataComplete == true) &&
            ((hubactionData->type == ACTION_TYPE_ACTIVATE_SCENE)
            || (hubactionData->type == ACTION_TYPE_ACTIVATE_SCENE_COLLECTION))) {
        if(jv2_findObject(actionObject,"resourceId1",&resourceId1Object)) {
                dataComplete &= jv2_getObjectInteger(&resourceId1Object,&hubactionData->resourceId1);
        }
        else {
            dataComplete=false;
//printf("Fail4\n");
        }
    }
//    else {
//printf("Fail5\n");
//    }

    if (dataComplete == true) {
        if(jv2_findObject(actionObject,"status",&statusObject)) {
            itsGood = jv2_getObjectInteger(&statusObject,(uint16_t*)&hubactionData->status);
            dataComplete &= itsGood;
            if ((itsGood == true) && (hubactionData->status > ACTION_STATUS_PENDING)) {
                dataComplete=false;
//printf("Fail6\n");
            }
        }
        else {
            dataComplete=false;
//printf("Fail7\n");
        }
    }
//    else {
//printf("Fail8\n");
//    }

    if (dataComplete == true) {
        if(jv2_findObject(actionObject,"messageId",&messageIdObject)) {
            itsGood = jv2_getObjectInteger(&messageIdObject,(uint16_t*)&hubactionData->messageId);
            dataComplete &= itsGood;
//if (dataComplete == false) {
//printf("Fail9a\n");
//}
//printf("messageId = %d\n",hubactionData->messageId);
            if ((itsGood == true) && (hubactionData->messageId > ACTION_MESSAGE_PENDING)) {
                dataComplete = false;
//printf("Fail9\n");
            }
        }
        else {
            dataComplete=false;
//printf("Fail10\n");
        }
    }

    if (dataComplete == true) {
        hubactionData->actionActive = true;
    }
//    else {
//printf("Fail11\n");
//    }

    return dataComplete;
}

/*****************************************************************************//**
* @brief This function parses the nextUpdate object withing the json response from
*       the remote server to the GET.
*
* @param nextUpdateObject.  Pointer to the json data with next update interval.
* @param hubactionData.  Structure containing the relevant messages values.
* @return bool.  True if the correct format.
* @author Neal Shurmantine
* @version
* 02/01/2016    Created.
*******************************************************************************/
static bool parse_next_update_object(JSON_PARSE_OBJECT *nextUpdateObject, HUBACTION_DATA_PTR hubactionData)
{
    bool dataComplete = false;
    dataComplete = jv2_getObjectUint32(nextUpdateObject,&hubactionData->nextUpdate);
    dataComplete &= hubactionData->nextUpdate != 0;
    return dataComplete;
}

/*****************************************************************************//**
* @brief This function parses the response to the GET to the remote server.
*
* @param serverResponse.  Pointer to the response data.
* @param hubactionData.  Pointer to the action data structure.
* @return boolean.  True if the response data is complete.
* @author Neal Shurmantine
* @version
* 02/24/2015    Created.
*******************************************************************************/
static bool parse_hubaction_update_data(char * serverResponse, HUBACTION_DATA_PTR hubactionData)
{
	JSON_PARSE_OBJECT   rootObject, hubActionObject, actionObject, nextUpdateObject;
	JSON_PARSE_OBJECT   integrationObject;
    bool                dataComplete = false;
    bool                itsGood;

    jv2_makeObjectFromString(&rootObject,serverResponse);

    hubactionData->nextUpdate = REMOTE_CONNECT_DEFAULT_CHECK_TIME;
    // find the hubAction node
    if(jv2_findObject(&rootObject,"hubAction",&hubActionObject)) {
        dataComplete = true;

        if(jv2_findObject(&hubActionObject,"action",&actionObject)) {
            itsGood = parse_action_object(&actionObject, hubactionData);
            dataComplete = itsGood;
        }

       // get next update
        if(jv2_findObject(&hubActionObject,"nextUpdate",&nextUpdateObject)) {
            itsGood = parse_next_update_object(&nextUpdateObject, hubactionData);
            dataComplete &= itsGood;
        }
        else {
            dataComplete=false;
        }

        //optional integration data
        if(jv2_findObject(&hubActionObject,"integration",&integrationObject)) {
            itsGood = parse_integration_object(&integrationObject,hubactionData);
            dataComplete &= itsGood;
        }
     }

    return dataComplete;
}

/*****************************************************************************//**
* @brief Determines if a scene must be activated based on the response to the GET.
*    Execute scene or multiroom scene if this action is requested.
*
* @param p_hubAction.  Pointer to the action structure.
* @return status.  Scene execution successful, failed or no action required.
* @author Neal Shurmantine
* @version
* 02/24/2015    Created.
*******************************************************************************/
static eActionStatus processAction(HUBACTION_DATA_PTR p_hubAction)
{
    eActionStatus scene_executed = ACTION_STATUS_FAIL;
    uint16_t scene_id = p_hubAction->resourceId1;
    if (p_hubAction->actionActive == false) {
        scene_executed = ACTION_STATUS_NULL;
    }
    else {
        if (p_hubAction->type == ACTION_TYPE_ACTIVATE_SCENE) {
            if (ExecuteSceneFromRemoteConnect(scene_id)==true) {
                scene_executed = ACTION_STATUS_SUCCESS;
            }
        }
        else if (p_hubAction->type == ACTION_TYPE_ACTIVATE_SCENE_COLLECTION) {
            if (sendMultiSceneMsgToShades(scene_id)==true) {
                scene_executed = ACTION_STATUS_SUCCESS;
            }
        }
        else if (p_hubAction->type == ACTION_TYPE_ENABLE_SCHEDULES) {
            setEnableScheduledEvents(true);
            p_hubAction->schedule_modified = true;
            scene_executed = ACTION_STATUS_SUCCESS;
        }
        else if (p_hubAction->type == ACTION_TYPE_DISABLE_SCHEDULES) {
            setEnableScheduledEvents(false);
            p_hubAction->schedule_modified = true;
            scene_executed = ACTION_STATUS_SUCCESS;
        }
        else if (p_hubAction->type == ACTION_TYPE_CLEAR_NEST) {
            p_hubAction->nest_cleared = true;
            scene_executed = ACTION_STATUS_SUCCESS;
        }
    }
    return scene_executed;
}

/*****************************************************************************//**
* @brief This is a callback function that executes if a successful GET reponse
*   is received from the remote server.
*
* @param pointer to the query structure
* @return unused uint32_t.
* @author Neal Shurmantine
* @version
* 02/24/2015    Created.
*******************************************************************************/
static uint32_t action_update_get_return(REST_CLIENT_QUERY_STRUCT_PTR p_query)
{
    HUBACTION_DATA hubActionData;
    eActionStatus status;
    eActionMessageId msg = ACTION_MESSAGE_INVALID_RESOURCE;

    RAS_ProcessActionResponseJSON( p_query->buffer, &hubActionData, &status, &msg);
    RAS_RemoteConnectCheckTime = hubActionData.nextUpdate;
    if (status != (uint32_t)ACTION_STATUS_NULL) {
        RMT_SendActionResponse(status, msg, hubActionData.id);
    }

    return 1;
}

/*****************************************************************************//**
* @brief Process json data from remote action
*
* @param pointer to the json data
* @param pointer to HUBACTION_DATA
* @param pointer to action status
* @param pointer to response message
* @return the function returns result in parameters
* @author Neal Shurmantine
* @version
* 08/08/2016    Created.
*******************************************************************************/
void RAS_ProcessActionResponseJSON(char *p_buff, HUBACTION_DATA *hubActionData,
                        eActionStatus *status,
                        eActionMessageId *msg)
{
    init_hubaction_data(hubActionData);
    parse_hubaction_update_data(p_buff, hubActionData);

    print_json_data(hubActionData);

    *status = processAction(hubActionData);
    if (*status == ACTION_STATUS_SUCCESS) {
        if ((hubActionData->schedule_modified==false) && (hubActionData->nest_cleared==false)) {
            LOG_LogEvent("Remote Action Received");
            printf("Scene %d Executed\n\n",hubActionData->resourceId1);
            RAS_RemoteConnectCheckTime = REMOTE_CONNECT_CHECK_TIME_NOW;
        }
        else if (hubActionData->schedule_modified==true) {
            LOG_LogEvent("Schedule Enable Modified");
            printf("Schedule Enable was modified\n");
            RDS_SyncDataImmediately(NULL_TOKEN);
        }
        else {
            LOG_LogEvent("Nest Struct Cleared");
            printf("Nest Struct Cleared\n");
            clearIntegrations();
            RDS_SyncDataImmediately(NULL_TOKEN);
        }
        *msg = ACTION_MESSAGE_SUCCESS;
    }
    else if (*status == ACTION_STATUS_NULL) {
#ifdef DEBUG_REMOTE_EVENTS
        printf("No Action Required\n\n");
#endif
    }
    else {
        printf("Scene %d Not Found\n\n",hubActionData->resourceId1);
    }
}

static void print_json_data(HUBACTION_DATA_PTR p_hub_action_data)
{
#ifdef ENABLE_JSON_PRINT
    printf("JSON DATA: \n\r");
    printf("nextUpdate %d\n",p_hub_action_data->nextUpdate);
    printf("id %d\n",p_hub_action_data->id);
    printf("type %d\n",p_hub_action_data->type);
//    printf("resourceId2 %d\n",p_hub_action_data->resourceId2);
    printf("status %d\n",p_hub_action_data->status);
    printf("messageId %d\n",p_hub_action_data->messageId);
    printf("resourceId1 %d\n",p_hub_action_data->resourceId1);
#endif
}

/*****************************************************************************//**
* @brief This function is called to start the whole process of contacting the
*        remote connect server and getting an action update
*
* @param none
* @return next time to check for an update in seconds.
* @author Neal Shurmantine
* @version
* 02/24/2015    Created.
*******************************************************************************/
uint32_t RAS_CheckActionUpdate(void)
{
    char pin[5];
    int n;
    getRemoteConnectPin(pin);
    pin[4] = 0;
    for (n = 0; n < 4; ++n) {
        if ( (pin[n] < '0') || (pin[n] > '9') ) {
            return REMOTE_CONNECT_DEFAULT_CHECK_TIME;
        }
    }

    RAS_RemoteConnectCheckTime = REMOTE_CONNECT_DEFAULT_CHECK_TIME;
    REST_CLIENT_QUERY_STRUCT_PTR p_query = (REST_CLIENT_QUERY_STRUCT_PTR)OS_GetMemBlock(sizeof(REST_CLIENT_QUERY_STRUCT));

    LoadDefaultClientData(p_query, &hunterDouglasRASServerSSLParameters,HTTPSRV_REQ_GET,action_update_get_return);


    p_query->socket_options.connection_timeout = 18 * SEC_IN_MS;
/*
    p_query->socket_options.send_timeout = 5 * SEC_IN_MS;
    p_query->socket_options.rto = 2 * SEC_IN_MS;
    p_query->socket_options.maxrto = 5 * SEC_IN_MS;
*/
    MakeAuthorizationString(p_query->authorize,false);
    snprintf(p_query->resource,MAX_RESOURCE_NAME_LENGTH, HUB_ACTION_GET_RESOURCE, RMT_GetAPIVersion());

    ConnectToServer(p_query);
    if (p_query->connection.socket) {
        if (GetResource(p_query)) {
            p_query->callback(p_query);
        }

        if (GetResponseStatus()->code == 401) {
            printf("Dissociate this hub!\n");
            clearRegistrationData();
        }
        DisconnectFromServer(p_query);
    }
    RAS_Status = p_query->status;
    OS_ReleaseMemBlock((void*)p_query);
    return RAS_RemoteConnectCheckTime;
}

/*****************************************************************************//**
* @brief Returns the status of the remote action process.
*
* @param none.
* @return nothing.
* @version
*******************************************************************************/
eRestClientStatus RAS_GetStatus(void)
{
    return RAS_Status;
}

/*****************************************************************************//**
* @brief This function parses the response to the PUT to the remote server.
*
* @param actionObject.  Pointer to the action object data.
* @param hubactionData.  Pointer to the action data structure.
* @return boolean.  True if the response data is complete.
* @author Neal Shurmantine
* @version
* 02/02/2016    Created.
*******************************************************************************/
static bool parse_action_object_put_return(JSON_PARSE_OBJECT *actionObject, HUBACTION_DATA_PTR hubactionData)
{
	JSON_PARSE_OBJECT  statusObject;
	JSON_PARSE_OBJECT   messageIdObject;
    bool dataComplete = false;
    bool itsGood;

    if (jv2_isObjectNull(actionObject)==true) {
        return true;
    }
//TODO:  this response should include all fields
    if(jv2_findObject(actionObject,"status",&statusObject)) { //0,1,2
        itsGood = jv2_getObjectInteger(&statusObject,(uint16_t*)&hubactionData->status);
        dataComplete = itsGood;
        if ((itsGood == true) && (hubactionData->status > ACTION_STATUS_NULL)) {
            dataComplete = false;
        }
    }
    else {
        dataComplete=false;
    }

    if (dataComplete == true) {
        if(jv2_findObject(actionObject,"messageId",&messageIdObject)) { //0,1,2,3
            itsGood = jv2_getObjectInteger(&messageIdObject,(uint16_t*)&hubactionData->messageId);
            dataComplete &= itsGood;
            if ((itsGood == true) && (hubactionData->messageId > ACTION_MESSAGE_INVALID_RESOURCE)) {
                dataComplete = false;
            }
        }
        else {
            dataComplete=false;
        }
    }
    if (dataComplete == true) hubactionData->actionActive = true;
    return dataComplete;
}

/*****************************************************************************//**
* @brief This is a callback function that executes after the PUT is sent
*      to the remote server.
*
* @param p_query.  Pointer to query structure.
* @return boolean.  Indicates success if the json body is correct.
* @author Neal Shurmantine
* @version
* 02/24/2015    Created.
*******************************************************************************/
static uint32_t put_action_response_return(REST_CLIENT_QUERY_STRUCT_PTR p_query)
{
    HUBACTION_DATA hubActionData;
    JSON_PARSE_OBJECT   rootObject, actionObject;
    bool                dataComplete = false;

    jv2_makeObjectFromString(&rootObject,p_query->buffer);

    if(jv2_findObject(&rootObject,"action",&actionObject)) {

        dataComplete = parse_action_object_put_return(&actionObject, &hubActionData);
    }
    print_json_put_response_data(&hubActionData);
    return dataComplete;
}

static void print_json_put_response_data(HUBACTION_DATA_PTR p_hub_action_data)
{
#ifdef ENABLE_JSON_PRINT
    printf("JSON PUT RESPONSE DATA: \n\r");
    printf("id %d\n",p_hub_action_data->id);
    printf("type %d\n",p_hub_action_data->type);
//    printf("resourceId2 %d\n",p_hub_action_data->resourceId2);
    printf("status %d\n",p_hub_action_data->status);
    printf("messageId %d\n",p_hub_action_data->messageId);
    printf("resourceId1 %d\n",p_hub_action_data->resourceId1);
#endif
}

/*****************************************************************************//**
* @brief This function sets up the PUT response to the remote server after
*    the GET response is returned.
*
* @param status.  The status of the query.
* @param msg.  Result code to be sent back to the remote server.
* @param id.  id of action
* @return nothing.
* @author Neal Shurmantine
* @version
* 02/09/2016    Created.
*******************************************************************************/
uint32_t RAS_SendActionResponse(eActionStatus status, eActionMessageId msg, uint32_t action_id)
{
    REST_CLIENT_QUERY_STRUCT_PTR p_query = (REST_CLIENT_QUERY_STRUCT_PTR)OS_GetMemBlock(sizeof(REST_CLIENT_QUERY_STRUCT));
    char *clientJSON = (char*)OS_GetMemBlock(MAX_JSON_LENGTH);

    LoadDefaultClientData(p_query, &hunterDouglasRASServerSSLParameters,HTTPSRV_REQ_PUT,put_action_response_return);

    snprintf(clientJSON,MAX_JSON_LENGTH, HUB_ACTION_JSON_RESPONSE, status, msg);
    snprintf(p_query->resource,MAX_RESOURCE_NAME_LENGTH, HUB_ACTION_PUT_RESOURCE, RMT_GetAPIVersion(), action_id);

    p_query->json = clientJSON;

    p_query->socket_options.connection_timeout = 18 * SEC_IN_MS;

    MakeAuthorizationString(p_query->authorize,false);

    ConnectToServer(p_query);
    if (p_query->connection.socket) {
        if (PutResource(p_query,false)) {
            p_query->callback(p_query);
        }
        DisconnectFromServer(p_query);
    }
    OS_ReleaseMemBlock((char*)clientJSON);
    OS_ReleaseMemBlock((void*)p_query);
    return 1;
}


/*
17. When the Hub then polls Remote Connect, it will look into the NestStructureData object.
    a. If the Away feature is enabled, the hub's current state is "Home" and the
    NestStructureData object contains a "HomeAway" property that is set to "Away", the
    Hub will:
        i. Temporarily disable all schedules.
        ii. Activate the scene that is associated to the Away feature.
    b. If the Rush Hour feature is enabled, the hub is not currently in Rush Hour Mode, and the
    NestStructureData object contains a "RushHourStart" property whos timestamp is <=
    the current time and "RushHourEnd" property whos timestamp is >= the current time,
    the hub will:
        i. Temporarily disable all schedules.
        ii. Activate the scene that is associated to the Rush Hour feature.
*/
