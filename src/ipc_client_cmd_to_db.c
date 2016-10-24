/** @file
 *
 * @defgroup ipc_client_cmd_to_db IPC Client Commands to Databases
 * @{
 * @brief Code for hub core client commands that are sent to the databases.
 *
 *
 */

/* Includes
*******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "JSONReader.h"
#include "ipc_client_cmd_to_db.h"
#include "os.h"

/* Global Variables
*******************************************************************************/
extern char clientJSON[MAX_JSON_LENGTH];

void IPC_Client_ReportShadePosition(SHADE_POSITION_PTR shade_pos)
{
/*
typedef struct
{
    uint16_t device_id;
    strPositions positions;
} SHADE_POSITION, *SHADE_POSITION_PTR;


typedef struct {
    uint8_t posCount;
    uint16_t position[ItsMaxPositionCount_];
    ePosKind posKind[ItsMaxPositionCount_];
} strPositions;

        {
            "type":"databases",
            "data": {
                "action":"report_shade_position",
                "shade_id":%d,
                "positions" : {
                    "position1":%d,
                    "posKind1":%d,
                    "position2":%d, (optional)
                    "posKind2":%d   (optional)
                }
            }
        }
*/
    if (shade_pos->positions.posCount == 1) {
        snprintf(clientJSON,MAX_JSON_LENGTH, REPORT_SHADE_POS_KIND1ONLY_CMD, shade_pos->device_id,
                                                        shade_pos->positions.posKind[0],
                                                        shade_pos->positions.position[0]);
    }
    else {
        snprintf(clientJSON,MAX_JSON_LENGTH, REPORT_SHADE_POS_KIND1AND2_CMD, shade_pos->device_id,
                                                        shade_pos->positions.posKind[0],
                                                        shade_pos->positions.position[0],
                                                        shade_pos->positions.posKind[1],
                                                        shade_pos->positions.position[1]);
    }
    ipc_expect_no_response(DATABASE_CLIENT_PATH);
}

void IPC_Client_ReportScenePosition(SHADE_POSITION_PTR p_shade_pos, uint8_t scene_num)
{
/*
    "type":"databases",
    "data": {
        "action":"report_scene_position",
        "scene_id":%d,
        "shade_id":%d,
        "positions" : {
            "position1":%d,
            "posKind1":%d,
            "position2":%d, (optional)
            "posKind2":%d   (optional)
        }
    }
}
*/
    if (p_shade_pos->positions.posCount == 1) {
        snprintf(clientJSON,MAX_JSON_LENGTH, REPORT_SCENE_POSITION_KIND1ONLY_CMD, scene_num,
                                                        p_shade_pos->device_id,
                                                        p_shade_pos->positions.posKind[0],
                                                        p_shade_pos->positions.position[0]);
    }
    else {
        snprintf(clientJSON,MAX_JSON_LENGTH, REPORT_SCENE_POSITION_KIND1AND2_CMD, scene_num,
                                                        p_shade_pos->device_id,
                                                        p_shade_pos->positions.posKind[0],
                                                        p_shade_pos->positions.position[0],
                                                        p_shade_pos->positions.posKind[1],
                                                        p_shade_pos->positions.position[1]);
    }
    ipc_expect_no_response(DATABASE_CLIENT_PATH);
}

void IPC_Client_RecordDiscovery(DISCOVERY_DATA_STRUCT_PTR p_disc_data)
{
    snprintf(clientJSON,MAX_JSON_LENGTH, RECORD_DISCOVERY_CMD, p_disc_data->uuid,
                                                    p_disc_data->network_id,
                                                    p_disc_data->device_id,
                                                    p_disc_data->shade_type);
    ipc_expect_no_response(DATABASE_CLIENT_PATH);
}

uint8_t IPC_Client_UpdateBatteryStatus(uint16_t shade_id, uint8_t measured)
{
    uint16_t socket;
    uint8_t level;
    snprintf(clientJSON,MAX_JSON_LENGTH, UPDATE_BATTERY_STATUS_CMD, shade_id, measured);
    if(connectSocket(DATABASE_CLIENT_PATH, &socket)) {
        sendMessage(clientJSON,strlen(clientJSON),socket); // make request
        IPC_RECEIVE_MSG_PTR p_msg = startListening(socket); // wait for response
        if (p_msg->len) {
            IPC_PrintClientJson(clientJSON, p_msg->p_buff);
            if (interpret_data(p_msg->p_buff,"databases") == true) {
                findJSONuint8(p_msg->p_buff, "data\\level", &level);
            }
        }
        free_msg_mem(p_msg);
    }
    return level;
}

void IPC_Client_SceneControllerClearedAnnouncement(uint16_t scene_controller_id)
{
/*
replaces: receivedSCClearAnnouncement()
{
    "type":"databases",
    "data": {
        "action":"received_sc_clear_announcement",
        "scene_controller_id":uin16_t
    }
}
*/

    snprintf(clientJSON,MAX_JSON_LENGTH, SC_RECEIVED_CLEAR_ANNOUNCEMENT_CMD, scene_controller_id);
    ipc_expect_no_response(DATABASE_CLIENT_PATH);
}

void IPC_Client_SceneControllerDatabaseUpdateRequest(uint16_t scene_controller_id, uint8_t version)
{
/*
replaces: updateRequestForSceneControllerWithIDAndVersion()
{
    "type":"databases",
    "data": {
        "action":"update_request_for_sc_with_id_and_ver",
        "scene_controller_id":uint16_t,
        "version":uint8_t
    }
}
*/

    snprintf(clientJSON,MAX_JSON_LENGTH, SC_DATABASE_UPDATE_REQUEST_CMD, 
                                            scene_controller_id,
                                            version);
    ipc_expect_no_response(DATABASE_CLIENT_PATH);
}

void IPC_Client_SceneControllerUpdatePacketRequest(uint16_t scene_controller_id, uint8_t rec_num, uint8_t version)
{
/*
replaces: updateRequestForSceneControllerMember()
{
    "type":"databases",
    "data": {
        "action":"update_request_for_sc_member",
        "scene_controller_id":uint16_t,
        "rec_num":uint8_t,
        "version":uint8_t
    }
}
*/
    snprintf(clientJSON,MAX_JSON_LENGTH, SC_UPDATE_REQUEST_FOR_MEMBER_CMD, 
                                            scene_controller_id,
                                            rec_num,
                                            version);
    ipc_expect_no_response(DATABASE_CLIENT_PATH);
}

void IPC_Client_SceneControllerTrigger(uint16_t scene_controller_id, uint8_t scene_type, uint16_t scene_id, uint8_t version)
{
/*
replaces: receivedSCTriggerForSceneWithID()
{
    "type":"databases",
    "data": {
        "action":"received_sc_trigger",
        "scene_controller_id":uint16_t,
        "scene_type":uint8_t,
        "scene_id":uint16_t,
        "version":uint8_t
    }
}
*/

    snprintf(clientJSON,MAX_JSON_LENGTH, SC_TRIGGER_CMD, 
                                            scene_controller_id,
                                            scene_type,
                                            scene_id,
                                            version);
    ipc_expect_no_response(DATABASE_CLIENT_PATH);
}

#define SCHEDULE_EVENTS_KEY_STRING  "data\\scheduledEvents[%d]\\%s"
ALL_RAW_DB_STR_PTR ipc_get_schedules_from_json(char * p_json)
{
    ALL_RAW_DB_STR_PTR p_sched_list_str;
    strScheduledEvent *p_single_schedule;
    bool success;
    uint16_t dummy_int;
    int16_t count = 0;
    char key_str[48];
    uint16_t n;
    bool dummy_bool;
    int32_t dummy_int32;
    bool is_resource_set = false;
    bool temp_bool;

    do {
        sprintf(key_str,SCHEDULE_EVENTS_KEY_STRING,count,"id");
        success = findJSONuint16(p_json, key_str, &dummy_int);
        if (success == true) ++count;
    } while (success == true);

    p_sched_list_str = (ALL_RAW_DB_STR_PTR)OS_GetMemBlock(2 + count * sizeof(strScheduledEvent));
    p_sched_list_str->count = count;

    p_single_schedule = (strScheduledEvent*)&p_sched_list_str->db_list;
    success = true;
    for (n=0; (n<count) && (success == true); ++n) {
        is_resource_set = false;
        sprintf(key_str,SCHEDULE_EVENTS_KEY_STRING,n,"id");
        success = findJSONuint16(p_json, key_str, &p_single_schedule->uID);

        sprintf(key_str,SCHEDULE_EVENTS_KEY_STRING,n,"hour");
        success &= findJSONuint8(p_json, key_str, &p_single_schedule->hours);

        sprintf(key_str,SCHEDULE_EVENTS_KEY_STRING,n,"minute");
        success &= findJSONint32(p_json, key_str, &dummy_int32);
        p_single_schedule->minutes = (int16_t)dummy_int32;

        sprintf(key_str,SCHEDULE_EVENTS_KEY_STRING,n,"enabled");
        success &= findJSONbool(p_json, key_str, &dummy_bool);
        p_single_schedule->enabledFlags.flags.isEnabled = dummy_bool;

        sprintf(key_str,SCHEDULE_EVENTS_KEY_STRING,n,"daySunday");
        success &= findJSONbool(p_json, key_str, &dummy_bool);
        p_single_schedule->enabledFlags.flags.daySunday = dummy_bool;

        sprintf(key_str,SCHEDULE_EVENTS_KEY_STRING,n,"dayMonday");
        success &= findJSONbool(p_json, key_str, &dummy_bool);
        p_single_schedule->enabledFlags.flags.dayMonday = dummy_bool;

        sprintf(key_str,SCHEDULE_EVENTS_KEY_STRING,n,"dayTuesday");
        success &= findJSONbool(p_json, key_str, &dummy_bool);
        p_single_schedule->enabledFlags.flags.dayTuesday = dummy_bool;

        sprintf(key_str,SCHEDULE_EVENTS_KEY_STRING,n,"dayWednesday");
        success &= findJSONbool(p_json, key_str, &dummy_bool);
        p_single_schedule->enabledFlags.flags.dayWednesday = dummy_bool;

        sprintf(key_str,SCHEDULE_EVENTS_KEY_STRING,n,"dayThursday");
        success &= findJSONbool(p_json, key_str, &dummy_bool);
        p_single_schedule->enabledFlags.flags.dayThursday = dummy_bool;

        sprintf(key_str,SCHEDULE_EVENTS_KEY_STRING,n,"dayFriday");
        success &= findJSONbool(p_json, key_str, &dummy_bool);
        p_single_schedule->enabledFlags.flags.dayFriday = dummy_bool;

        sprintf(key_str,SCHEDULE_EVENTS_KEY_STRING,n,"daySaturday");
        success &= findJSONbool(p_json, key_str, &dummy_bool);
        p_single_schedule->enabledFlags.flags.daySaturday = dummy_bool;

        sprintf(key_str,SCHEDULE_EVENTS_KEY_STRING,n,"eventType");
        success &= findJSONuint16(p_json, key_str, &dummy_int);
        p_single_schedule->typeFlags.flags.isClock = (dummy_int == 0);
        p_single_schedule->typeFlags.flags.isSunrise = (dummy_int == 1);

        sprintf(key_str,SCHEDULE_EVENTS_KEY_STRING,n,"sceneId");
        temp_bool = findJSONuint16(p_json, key_str, &dummy_int);
        if (temp_bool == true) {
            p_single_schedule->typeFlags.flags.isMultiSceneID = false;
            p_single_schedule->sceneOrMultiSceneID = dummy_int;
            is_resource_set = true;
        }

        sprintf(key_str,SCHEDULE_EVENTS_KEY_STRING,n,"sceneCollectionId");
        temp_bool = findJSONuint16(p_json, key_str, &dummy_int);
        if (temp_bool == true) {
            p_single_schedule->typeFlags.flags.isMultiSceneID = true;
            p_single_schedule->sceneOrMultiSceneID = dummy_int;
            if (is_resource_set == true) {
                is_resource_set = false;
            }
            else {
                is_resource_set = true;
            }
        }
        success &= is_resource_set;
        ++p_single_schedule;
    }
    if (count && (success == false)) {
        p_sched_list_str->count = -1;
    }
    return p_sched_list_str;
}

ALL_RAW_DB_STR_PTR IPC_Client_GetScheduledEvents(void)
{
    uint16_t socket;
    ALL_RAW_DB_STR_PTR p_sched_list_str;
    p_sched_list_str = (ALL_RAW_DB_STR_PTR)OS_GetMemBlock(2);
    p_sched_list_str->count = -1;

    snprintf(clientJSON,MAX_JSON_LENGTH, GET_SCHEDULED_EVENTS_CMD);
    if(connectSocket(DATABASE_CLIENT_PATH, &socket)) {
        sendMessage(clientJSON,strlen(clientJSON),socket); // make request
        IPC_RECEIVE_MSG_PTR p_msg = startListening(socket); // wait for response
        if (p_msg->len) {
            IPC_PrintClientJson(clientJSON, p_msg->p_buff);
            if (interpret_data(p_msg->p_buff,"databases") == true) {
                OS_ReleaseMemBlock((void *)p_sched_list_str);
                p_sched_list_str = ipc_get_schedules_from_json(p_msg->p_buff);
            }
        }
        free_msg_mem(p_msg);
    }
    return p_sched_list_str;
}


/** @} */
