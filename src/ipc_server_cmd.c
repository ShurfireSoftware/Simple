/** @file
 *
 * @defgroup ipc_server_cmd IPC Server Command processor
 * @{
 * @brief This module is used to process the IPC commands received by
 *   the hub core server.
 *
 *
 */

/* Includes
*******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

#include "ipc.h"
#include "rf_serial_api.h"
#include "JSONReader.h"
#include "os.h"
#include "ipc_server_cmd.h"
#include "SCH_ScheduleTask.h"
#include "stub.h"
#include "RMT_RemoteServers.h"

/* Global Variables
*******************************************************************************/

/* Local Constants and Definitions
*******************************************************************************/
#define IPC_RESPONSE_ACK            "\"ack\""
#define IPC_RESPONSE_NACK           "\"nack\""
#define IPC_RESPONSE_ID             "{\"id\":%d}"
#define IPC_RESPONSE_LONG_LONG_ID   "{\"id\":%llu}"
#define IPC_RESPONSE_ACTIVE         "{\"active\":%s}"
//#define IPC_RESPONSE_LEVEL          "{\"level\":%d}"
#define IPC_RESPONSE_COARSE_LEVEL   "{\"level\":%d}"
#define IPC_RESPONSE_IS_BUSY        "{\"is_busy\":%s}"
#define IPC_RESPONSE_HAS_ATTEMPTED  "{\"has_attempted\":%s}"
#define IPC_RESPONSE_STATUS         "{\"status\":%d}"
#define IPC_RESPONSE_ERROR          "{\"error\":\"%s\"}"

#define IPC_RESPONSE_FULL           "{\"type\":\"%s\",\"data\":%s}\f"

//#define MAX_ACTION_TYPE_LENGTH  20
//#define MAX_TYPE_LENGTH  20
#define MAX_DATA_TYPE_LENGTH    100


#define PRINT_IPC_JSON

/* Local Function Declarations
*******************************************************************************/

/* Local variables
*******************************************************************************/


char * ipc_ack_response(void)
{
    char * p_resp = (char *)OS_GetMemBlock(IPC_CORE_MAX_RESPONSE_SIZE);
    memcpy(p_resp, IPC_RESPONSE_ACK,strlen(IPC_RESPONSE_ACK));
    return p_resp;
}

char * ipc_nack_response(void)
{
    char * p_resp = (char *)OS_GetMemBlock(IPC_CORE_MAX_RESPONSE_SIZE);
    memcpy(p_resp, IPC_RESPONSE_NACK,strlen(IPC_RESPONSE_NACK));
    return p_resp;
}

char * ipc_get_nordic_uuid(char * p_json)
{
    char * p_resp = (char*)OS_GetMemBlock(IPC_CORE_MAX_RESPONSE_SIZE);
    uint64_t id = RC_GetNordicUuid();
    sprintf(p_resp,IPC_RESPONSE_LONG_LONG_ID,id);
    return p_resp;
}

char * ipc_set_network_id(char * p_json)
{
    uint16_t id;
    if (findJSONuint16(p_json, "data\\id", &id) == true) {
        RC_AssignNewNetworkId(id);
        return ipc_ack_response();
    }
    else {
        return ipc_nack_response();
    }
}

char * ipc_get_network_id(char * p_json)
{
    char * p_resp = OS_GetMemBlock(IPC_CORE_MAX_RESPONSE_SIZE);
    uint16_t id = RC_GetNetworkId();
    sprintf(p_resp,IPC_RESPONSE_ID,id);
    return p_resp;
}

char * ipc_create_network_id(char * p_json)
{
    char * p_resp = OS_GetMemBlock(IPC_CORE_MAX_RESPONSE_SIZE);
    uint16_t id = RC_CreateRandomNetworkId();
    RC_AssignNewNetworkId(id);
    sprintf(p_resp,IPC_RESPONSE_ID,id);
    return p_resp;
}

char * ipc_jog_shade(char * p_json)
{
    P3_Address_Internal_Type address;
    address.Unique_Id = 0;

    if (findJSONuint16(p_json, "data\\shade_id", &address.Device_Id) == true) {
        SC_JogShade(P3_Address_Mode_Device_Id, &address);
        return ipc_ack_response();
    }
    else {
        return ipc_nack_response();
    }
}

char * ipc_set_shade_pos(char * p_json)
{
    P3_Address_Internal_Type address;
    address.Unique_Id = 0;
    strPositions pos;
    pos.posCount = 1;

    bool success = findJSONuint16(p_json, "data\\shade_id", &address.Device_Id);

printf("Size of %d\n",sizeof(pos.posKind[0]));

    success &= findJSONuint8(p_json, "data\\posKind", (uint8_t*)&pos.posKind[0]);
    success &= findJSONuint16(p_json, "data\\position", &pos.position[0]);
    if (success == true)  {
        SC_SetShadePosition(P3_Address_Mode_Device_Id, &address, &pos);
        return ipc_ack_response();
    }
    else {
        return ipc_nack_response();
    }
}

char * ipc_get_shade_pos(char * p_json)
{
    P3_Address_Internal_Type address;
    address.Unique_Id = 0;

    bool success = findJSONuint16(p_json, "data\\shade_id", &address.Device_Id);
    if (success == true)  {
        SC_GetShadePosition(P3_Address_Mode_Device_Id, &address);
        return ipc_ack_response();
    }
    else {
        return ipc_nack_response();
    }
}

char * ipc_enable_join(char * p_json)
{
    SC_EnableNetworkJoin();
    return ipc_ack_response();
}

char * ipc_is_join_active(char * p_json)
{
    char * p_resp = OS_GetMemBlock(IPC_CORE_MAX_RESPONSE_SIZE);
    if (SC_IsNetworkJoiningActive() == true) {
        sprintf(p_resp,IPC_RESPONSE_ACTIVE,"True");
    }
    else {
        sprintf(p_resp,IPC_RESPONSE_ACTIVE,"False");
    }
    return p_resp;
}

char * ipc_disable_join(char * p_json)
{
    SC_DisableNetworkJoin(0);
    return ipc_ack_response();
}

char * ipc_discover_shades(char * p_json)
{
    bool bool_val;
    uint8_t type;
    bool success = findJSONbool(p_json, "data\\is_absolute", &bool_val);
    success &= findJSONuint8(p_json, "data\\discover_type", &type);
    if (success == true) {
        SC_DiscoverShades(bool_val,type);
        return ipc_ack_response();
    }
    else {
        return ipc_nack_response();
    }
}

char * ipc_is_discovery_active(char * p_json)
{
    char * p_resp = OS_GetMemBlock(IPC_CORE_MAX_RESPONSE_SIZE);
    if (SC_IsDiscoveryActive() == true) {
        sprintf(p_resp,IPC_RESPONSE_ACTIVE,"True");
    }
    else {
        sprintf(p_resp,IPC_RESPONSE_ACTIVE,"False");
    }
    return p_resp;
}

char * ipc_send_beacon(char * p_json)
{
    SC_IssueBeacon();
    return ipc_ack_response();
}

char * ipc_group_assign(char * p_json)
{
    bool bool_val;
    uint8_t group_id;
    P3_Address_Internal_Type address;
    address.Unique_Id = 0;
    bool success = findJSONbool(p_json, "data\\is_assigned", &bool_val);
    success &= findJSONuint8(p_json, "data\\group_id", &group_id);
    success &= findJSONuint16(p_json, "data\\shade_id", &address.Device_Id);
    if (success == true) {
        SC_GroupAssign(
            P3_Address_Mode_Device_Id,
            &address,
            group_id,
            bool_val);
        return ipc_ack_response();
    }
    else {
        return ipc_nack_response();
    }
}

char * ipc_jog_group(char * p_json)
{
    P3_Address_Internal_Type address;
    address.Unique_Id = 0;
    bool success = findJSONuint8(p_json, "data\\group_id", &address.Group_Id[0]);
    if (success == true) {
        address.Group_Id[1] = 0;
        SC_JogShade(P3_Address_Mode_Group_Id, &address);
        return ipc_ack_response();
    }
    else {
        return ipc_nack_response();
    }
}

char * ipc_set_group_pos(char * p_json)
{
    P3_Address_Internal_Type address;
    address.Unique_Id = 0;
    strPositions pos;
    pos.posCount = 1;

    bool success = findJSONuint8(p_json, "data\\group_id", &address.Group_Id[0]);
    success &= findJSONuint8(p_json, "data\\posKind", (uint8_t*)&pos.posKind[0]);
    success &= findJSONuint16(p_json, "data\\position", &pos.position[0]);
    if (success == true) {
        address.Group_Id[1] = 0;
        SC_SetShadePosition(
            P3_Address_Mode_Group_Id,
            &address, &pos);
        return ipc_ack_response();
    }
    else {
        return ipc_nack_response();
    }
}

char * ipc_clear_shade_groups(char * p_json)
{
    P3_Address_Internal_Type address;
    address.Unique_Id = 0;
    bool success = findJSONuint16(p_json, "data\\shade_id", &address.Device_Id);
    if (success == true) {
        SC_ResetShade(P3_Address_Mode_Device_Id, &address, SR_DEL_GROUP_7_TO_255);
        return ipc_ack_response();
    }
    else {
        return ipc_nack_response();
    }
}

char * ipc_calibrate_shade(char * p_json)
{
    P3_Address_Internal_Type address;
    address.Unique_Id = 0;
    bool success = findJSONuint16(p_json, "data\\shade_id", &address.Device_Id);
    if (success == true) {
        SC_ResetShade(P3_Address_Mode_Device_Id, &address, SR_RECAL_NEXT_RUN);
        return ipc_ack_response();
    }
    else {
        return ipc_nack_response();
    }
}

char * ipc_delete_shade(char * p_json)
{
    P3_Address_Internal_Type address;
    address.Unique_Id = 0;
    bool success = findJSONuint16(p_json, "data\\shade_id", &address.Device_Id);
    if (success == true) {
        SC_ResetShade(P3_Address_Mode_Device_Id, &address, 
            SR_CLEAR_DISCOVERED_FLAG | SR_DEL_GROUP_7_TO_255 | SR_DELETE_SCENES);
        return ipc_ack_response();
    }
    else {
        return ipc_nack_response();
    }
}

char * ipc_clear_shade(char * p_json)
{
    P3_Address_Internal_Type address;
    address.Unique_Id = 0;
    bool success = findJSONuint16(p_json, "data\\shade_id", &address.Device_Id);
    if (success == true) {
        SC_ResetShade(P3_Address_Mode_Device_Id, &address, 
            SR_DEL_GROUP_7_TO_255 | SR_DELETE_SCENES);
        return ipc_ack_response();
    }
    else {
        return ipc_nack_response();
    }
}

char * ipc_set_shade_scene_to_current(char * p_json)
{
    uint8_t scene_id;
    P3_Address_Internal_Type address;

    address.Unique_Id = 0;
    bool success = findJSONuint16(p_json, "data\\shade_id", &address.Device_Id);
    success &= findJSONuint8(p_json, "data\\scene_id", &scene_id);
    if (success == true) {
        SC_SetSceneToCurrent(
            P3_Address_Mode_Device_Id,
            &address,
            scene_id);
        return ipc_ack_response();
    }
    else {
        return ipc_nack_response();
    }
}

char * ipc_set_group_scene_to_current(char * p_json)
{
    uint8_t scene_id;
    P3_Address_Internal_Type address;

    address.Unique_Id = 0;
    bool success = findJSONuint8(p_json, "data\\group_id", &address.Group_Id[0]);
    success &= findJSONuint8(p_json, "data\\scene_id", &scene_id);
    if (success == true) {
        address.Group_Id[1] = 0;
        SC_SetSceneToCurrent(
            P3_Address_Mode_Group_Id,
            &address, 
            scene_id);
        return ipc_ack_response();
    }
    else {
        return ipc_nack_response();
    }
}

char * ipc_set_shade_scene_at_pos(char * p_json)
{
    uint8_t scene_id;
    P3_Address_Internal_Type address;
    address.Unique_Id = 0;
    strPositions pos;
    pos.posCount = 1;

    bool success = findJSONuint16(p_json, "data\\shade_id", &address.Device_Id);
    success &= findJSONuint8(p_json, "data\\posKind", (uint8_t*)&pos.posKind[0]);
    success &= findJSONuint16(p_json, "data\\position", &pos.position[0]);
    success &= findJSONuint8(p_json, "data\\scene_id", &scene_id);
    if (success == true)  {
        SC_SetSceneAtPosition(
            P3_Address_Mode_Device_Id,
            &address, scene_id, &pos);
        return ipc_ack_response();
    }
    else {
        return ipc_nack_response();
    }
}

char * ipc_set_group_scene_at_pos(char * p_json)
{
    uint8_t scene_id;
    P3_Address_Internal_Type address;
    address.Unique_Id = 0;
    strPositions pos;
    pos.posCount = 1;

    bool success = findJSONuint8(p_json, "data\\group_id", &address.Group_Id[0]);
    success &= findJSONuint8(p_json, "data\\posKind", (uint8_t*)&pos.posKind[0]);
    success &= findJSONuint16(p_json, "data\\position", &pos.position[0]);
    success &= findJSONuint8(p_json, "data\\scene_id", &scene_id);
    if (success == true) {
        address.Group_Id[1] = 0;
        SC_SetSceneAtPosition(
                P3_Address_Mode_Group_Id,
                &address, scene_id, &pos);
        return ipc_ack_response();
    }
    else {
        return ipc_nack_response();
    }
}

char * ipc_execute_scene(char * p_json)
{
    uint8_t scene_count;
    uint8_t scene_list[MAX_SCENE_EXECUTE_SIZE];
    char key[32];
    bool success = findJSONuint8(p_json, "data\\scene_count", &scene_count);
    if ((success == true) && (scene_count <= MAX_SCENE_EXECUTE_SIZE) ){
        int n;
        for (n=0; (n < scene_count) && (success == true); ++n) {
            sprintf(key, "data\\scene_list[%d]\\scene_id", n);
            success &= findJSONuint8(p_json, key, &scene_list[n]);
        }
        if (success == true) {
            SC_ExecuteScene(scene_count,scene_list);
            return ipc_ack_response();
        }
        else {
            return ipc_nack_response();
        }
    }
    else {
        return ipc_nack_response();
    }
}

char * ipc_delete_scene(char * p_json)
{
    uint8_t scene_id;
    P3_Address_Internal_Type address;
    address.Unique_Id = 0;
    bool success = findJSONuint16(p_json, "data\\shade_id", &address.Device_Id);
    success &= findJSONuint8(p_json, "data\\scene_id", &scene_id);
    if (success == true)  {
        SC_DeleteScene(
            P3_Address_Mode_Device_Id,
            &address,scene_id);
        return ipc_ack_response();
    }
    else {
        return ipc_nack_response();
    }
}

char * ipc_request_scene_position(char * p_json)
{
    uint8_t scene_id;
    P3_Address_Internal_Type address;
    address.Unique_Id = 0;
    bool success = findJSONuint16(p_json, "data\\shade_id", &address.Device_Id);
    success &= findJSONuint8(p_json, "data\\scene_id", &scene_id);
    if (success == true)  {
        SC_RequestScenePosition( P3_Address_Mode_Device_Id,
            &address,scene_id);
        return ipc_ack_response();
    }
    else {
        return ipc_nack_response();
    }
}

char * ipc_request_shade_battery_level(char * p_json)
{
    P3_Address_Internal_Type address;
    address.Unique_Id = 0;
    bool success = findJSONuint16(p_json, "data\\shade_id", &address.Device_Id);
    if (success == true)  {
        SC_CheckShadeBattery(P3_Address_Mode_Device_Id, &address);
        return ipc_ack_response();
    }
    else {
        return ipc_nack_response();
    }
}

char * ipc_reset_all_shades(char * p_json)
{
    SC_ResetAllShades();
    return ipc_ack_response();
}

char * ipc_scene_controller_clear_ack(char * p_json)
{
    uint16_t controller_id;
    bool success = findJSONuint16(p_json, "data\\controller_id", &controller_id);
    if (success == true)  {
        SC_SceneControllerClearedAck(controller_id);
        return ipc_ack_response();
    }
    else {
        return ipc_nack_response();
    }
}

char * ipc_scene_controller_update_header(char * p_json)
{
    uint16_t controller_id;
    SC_SCENE_CTL_UPDATE_HDR_STR update_hdr;
    bool success = findJSONuint16(p_json, "data\\controller_id", &controller_id);
    success &= findJSONuint8(p_json, "data\\rec_count", &update_hdr.rec_count);
    success &= findJSONuint8(p_json, "data\\version", &update_hdr.version);
    success &= findJSONString(p_json, "data\\name", update_hdr.name);
    if (success == true)  {
        SC_SceneControllerUpdateHeader(controller_id, &update_hdr);
        return ipc_ack_response();
    }
    else {
        return ipc_nack_response();
    }
}

char * ipc_scene_controller_update_packet(char * p_json)
{
    SC_SCENE_CTL_UPDATE_PACKET_STR update_packet;
    uint16_t controller_id;
    bool success = findJSONuint16(p_json, "data\\controller_id", &controller_id);
    success &= findJSONuint8(p_json, "data\\rec_count", &update_packet.rec_count);
    success &= findJSONuint8(p_json, "data\\version", &update_packet.version);
    success &= findJSONuint8(p_json, "data\\scene_type", &update_packet.scene_type);
    success &= findJSONuint16(p_json, "data\\scene_id", &update_packet.scene_id);
    success &= findJSONString(p_json, "data\\name", update_packet.name);
    if (success == true)  {
        SC_SceneControllerUpdatePacket(controller_id, &update_packet);
        return ipc_ack_response();
    }
    else {
        return ipc_nack_response();
    }
}

char * ipc_scene_controller_trigger_ack(char * p_json)
{
    uint16_t controller_id;
    SC_SCENE_CTL_TRIGGER_ACK_STR trigger_ack;
    bool success = findJSONuint16(p_json, "data\\controller_id", &controller_id);
    success &= findJSONuint8(p_json, "data\\version", &trigger_ack.version);
    success &= findJSONuint8(p_json, "data\\scene_type", &trigger_ack.scene_type);
    success &= findJSONuint16(p_json, "data\\scene_id", &trigger_ack.scene_id);
    if (success == true)  {
        SC_SceneControllerTriggerAck(controller_id, &trigger_ack);
        return ipc_ack_response();
    }
    else {
        return ipc_nack_response();
    }
}

char * ipc_request_coarse_battery_level(char * p_json)
{
    uint8_t level;
    uint8_t shade_type;
    uint8_t voltage;
 
    bool success = findJSONuint8(p_json, "data\\shade_type", &shade_type);
    success &= findJSONuint8(p_json, "data\\voltage", &voltage);

    if (success == true)  {
        char * p_resp = (char*)OS_GetMemBlock(IPC_CORE_MAX_RESPONSE_SIZE);
        level = (uint8_t)SC_GetCoarseBatteryLevel(shade_type, voltage);
        sprintf(p_resp,IPC_RESPONSE_COARSE_LEVEL,level);
        return p_resp;
    }
    else {
        return ipc_nack_response();
    }
}

char * ipc_set_time(char * p_json)
{
/*
    void SCH_SetTime(TIME_STRUCT_PTR p_new_time,int32_t timezone_offset);
                "time_utc":String, ("2016-09-15 02:22:15.000")
                "offset":%d
*/
    return ipc_nack_response();
}

char * ipc_modify_scheduled_scenes(char * p_json)
{
    SCH_ModifyScheduledScenes();
    return ipc_ack_response();
}

char * ipc_trigger_remote_data_sync(char * p_json)
{
    RDS_TriggerRemoteSync(NULL_TOKEN);
    return ipc_ack_response();
}

char * ipc_is_remote_data_sync_busy(char * p_json)
{
/*    char * p_resp = OS_GetMemBlock(IPC_CORE_MAX_RESPONSE_SIZE);
    if (RDS_IsSynchronizationBusy() == true) {
        sprintf(p_resp,IPC_RESPONSE_IS_BUSY,"True");
    }
    else {
        sprintf(p_resp,IPC_RESPONSE_IS_BUSY,"False");
    }
    return p_resp;
*/
    return ipc_nack_response();
}

char * ipc_sync_remote_data_immediately(char * p_json)
{
    RDS_SyncDataImmediately(NULL_TOKEN);
    return ipc_ack_response();
}

char * ipc_get_remote_data_sync_error(char * p_json)
{
/*
    char *RDS_GetErrorCode(void);
#define IPC_RESPONSE_ERROR          "{\"error\":\"%s\"}"
*/
    return ipc_nack_response();
}

char * ipc_is_registration_busy(char * p_json)
{
    char * p_resp = OS_GetMemBlock(IPC_CORE_MAX_RESPONSE_SIZE);
    if (RMT_IsRegistrationBusy() == true) {
        sprintf(p_resp,IPC_RESPONSE_IS_BUSY,"True");
    }
    else {
        sprintf(p_resp,IPC_RESPONSE_IS_BUSY,"False");
    }
    return p_resp;
}

char * ipc_has_unregister_been_attempted(char * p_json)
{
/*
    bool RMT_HasUnregisterAttempted(void);
#define IPC_RESPONSE_HAS_ATTEMPTED  "{\"has_attempted\":%s}"
*/
    return ipc_nack_response();
}

char * ipc_notify_remote_connect_status(char * p_json)
{
/*
            "data": {
                "action":"notify_remote_connect_status",
                "is_enabled":bool
            }
    void RMT_NotifyRemoteConnectStatus(void);
*/
    return ipc_nack_response();
}

char * ipc_connect_to_aws(char * p_json)
{
/*
    void RMT_ConnectAWS(uint32_t wait_time_sec);
*/
    return ipc_nack_response();
}

char * ipc_unregister_hub(char * p_json)
{
/*
    void RMT_UnRegisterHub(void);
*/
    return ipc_nack_response();
}

char * ipc_register_hub(char * p_json)
{
/*
    void RMT_RegisterHub(char *p_hub_id, char *p_hub_name, char *p_pv_key);
            "data":{
                "action":"register_hub",
                "hub_id":String,
                "hub_name":String,
                "pv_key":String
            }
*/
    return ipc_nack_response();
}

char * ipc_get_registration_status(char * p_json)
{
/*
    eRestClientStatus RCR_GetStatus(void);
#define IPC_RESPONSE_STATUS         "{\"status\":%d}"
*/
    return ipc_nack_response();
}

char * ipc_get_registration_error(char * p_json)
{
/*
    char *RCR_GetErrorCode(void);
#define IPC_RESPONSE_ERROR          "{\"error\":\"%s\"}"
*/
    return ipc_nack_response();
}

void ipc_print_server_json(char * p_json, char * p_full_resp)
{
    char *p_dsp;
    printf("Received:\n");
    p_dsp = p_json;
    while (*p_dsp) {
        if (*p_dsp != '\f')
            printf("%c",*p_dsp);
        ++p_dsp;
    }
    printf("\nSent:\n");
    p_dsp = p_full_resp;
    while (*p_dsp) {
        if (*p_dsp != '\f')
            printf("%c",*p_dsp);
        ++p_dsp;
    }
    printf("\n");
}

static char * process_action(char * p_type, char * p_action, char * p_json)
{
    uint16_t n = 0; // length;
    char * p_resp = NULL;

    // printf("*** process_message(%d) ***\n", pRxBuffer->generic.length);

    while (IPC_Functions[n].p_action[0]) {
        if (strcmp(IPC_Functions[n].p_action,p_action) == 0) {
            p_resp = (*IPC_Functions[n].funct)(p_json);
            break;
        }
        ++n;
    }

    return p_resp;
}

/**@brief Process a TCP packet and route it to appropriate function
 *
 * @details Compare the first 3 bytes of the packet with the contents
 *    of the IPC_CORE_PARSE_STRUCT array to look for a match. If a
 *    match is found, send the packet payload to the appropriate
 *    function.
 */
char * IPC_ProcessPacket(char * p_json)
{
    char * p_resp = NULL;
    char type[MAX_TYPE_STR_LENGTH];
    char action[MAX_DATA_TYPE_LENGTH];
    char * p_full_resp = OS_GetMemBlock(IPC_CORE_MAX_RESPONSE_SIZE);

    uint16_t length;

    length = strlen(p_json);
    if(p_json[length - 1] == '\f') {
        p_json[length - 1] = 0;
        length--;
    }

    // first validate the data
    if(findJSONString(p_json, "type", type)) {
        if(findJSONString(p_json, "data\\action", action)) {
            if(strcmp(type, "hub_core") == 0) {
                p_resp = process_action(type, action, p_json);
            }
        }
    }

    if (p_resp == NULL) {
        p_resp = ipc_nack_response();
        strcpy(action,"unknown");
    }

    sprintf(p_full_resp, IPC_RESPONSE_FULL, type, p_resp);
    OS_ReleaseMemBlock(p_resp);

#ifdef PRINT_IPC_JSON
    ipc_print_server_json(p_json, p_full_resp);
#endif

    return p_full_resp;
}



/** @} */
