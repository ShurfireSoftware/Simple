/***************************************************************************//**
 * @file   SC_ShadeConfig.c
 * @brief  This module provides an interface between the UI and RF message
 *          processing for shades.
 *
 * @author Neal Shurmantine
 * @copyright (c) 2014 Hunter Douglas. All rights reserved.
 *
 * @date Created: 11/03/2014
 * @date Last updated: 07/20/2015
 *
 * @version
 * 11/06/2014   Created.
 * 01/12/2015   Major refactoring
 * 07/20/2015        Modifications by Neal Schurmantine and James Baugh
 *                             - Filter out Beacons with 0 length payload
 *                             - Validate database version number coming from Scene
 *                                        Controller Update Packet Request
 * 08/10/2016   Converted to Hub2.0
 ******************************************************************************/

/* Includes
*******************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "util.h"
#include "config.h"
#include "rf_serial_api.h"
#include "os.h"
#include "rfo_outbound.h"
#include "stub.h"
#include "SCH_ScheduleTask.h"
#include "RMT_RemoteServers.h"
#include "LOG_DataLogger.h"
#include "ipc_client_cmd_to_db.h"

//
#ifdef USE_ME
#include "IO_InputOutput.h"
#include "SceneControllerCommunication.h"
#include "SPIFlashDataBuffer.h"
#include "SPIFlashIndexBuffer.h"
#include "DataCheck.h"
#include "rfNetworkProcessing.h"
#include "DataStruct.h"
#endif

/* Global Variables
*******************************************************************************/
extern bool flashDataNeedsWritten;
extern bool flashVectorNeedsWritten;

/* Local Constants and Definitions
*******************************************************************************/
#define SKIP_IF_NO_NETWORK  if (RC_IsNetworkIdAssigned() == false) return;
#define SC_NEW_CONFIG_EVENT          BIT0
#define SC_RF_TICK_EVENT             BIT1
#define SC_SERIAL_RESPONSE_EVENT     BIT2
#define SC_RF_INDICATION_EVENT       BIT3
//Read shade battery levels at:
#define SC_BATTERY_CHECK_HOUR                  4
#define SC_BATTERY_CHECK_MINUTE                0
#define SC_RANDOMIZE_MINUTES                   60
#define SC_RANDOMIZE_SECONDS                   60

#define SC_LOW_BATTERY_LEVEL                       110
#define SC_MODERATE_BATTERY_LEVEL                  120
#define SC_LOW_BATTERY_LEVEL_POWER_TILT            100
#define SC_MODERATE_BATTERY_LEVEL_POWER_TILT       110
#define SC_DISCOVERY_TIMEOUT                    3

#define SC_DISCOVERY_RETRY_NO_RESP  4
#define SC_SCENE_CONTROLLER_TYPE        201
typedef struct SHADE_DATA_REQ_TAG
{
    P3_Address_Mode_Type adr_mode;
    P3_Address_Internal_Type adr;
    uint8_t tx_opt;
    void(*p_callback)(void*);
    uint8_t len;
    uint8_t msg[MAX_CMD_PAYLOAD];
} SHADE_DATA_REQ, *SHADE_DATA_REQ_PTR;

#define BATTERY_CHECK_RETRY_MAX     7
#define SECONDS_BETWEEN_BATTERY_CHECKS  4

/* Local Function Declarations
*******************************************************************************/
static void sc_continue_discovery(void);
static void SC_discovery_timeout(uint16_t unused);
static void sc_build_shade_data_request(SHADE_DATA_REQ_PTR p_req);
static void SC_build_config_record(SHADE_DATA_REQ_MSG_STRUCT_PTR p_pay_rec);
static void sc_group_assign_exec(P3_Address_Mode_Type adr_mode,
                             P3_Address_Internal_Type * address, uint8_t group_id, bool isAssigned);
static void sc_issue_beacon_exec(void);
void sc_send_raw_payload(P3_Address_Mode_Type adr_mode,
                        P3_Address_Internal_Type * address,
                        uint8_t len, uint8_t * msg);
static RNC_CONFIG_REC_PTR SC_create_blank_record(void);
static void SC_clear_item_in_list(RNC_CONFIG_REC_PTR p_active_msg);

static void sc_shade_indication_received(PARSE_KEY_STRUCT_PTR p_rf_response);
static SC_SHADE_DISC_REC_PTR SC_create_discover_record(PARSE_KEY_STRUCT_PTR p_rf_response);
static void SC_remove_discovered_shade_from_list(void);

static void sc_group_set_indication_received(PARSE_KEY_STRUCT_PTR p_rf_response);
static void sc_cancel_configurations(void);
static void SC_discovery_callback(void *);
static bool SC_is_redundant_discovery_response(uint16_t id);
static void SC_batt_check_proc_continue(uint16_t token);
static void sc_parse_indication_payload(uint16_t id, uint8_t *p_payload);
static void sc_parse_scene_controller_payload(uint16_t id, uint8_t *p_data);
static void sc_indication_debug_metrics(uint16_t id, uint8_t* p_data);
static void sc_indication_shade_type(uint16_t id,uint8_t* p_data);
static void sc_indication_batt_level(uint16_t id, uint8_t* p_data);
static void sc_indication_cur_pos(uint16_t id, uint16_t val, ePosKind kind);
static void sc_indication_scene_pos(uint16_t id, uint16_t val, uint8_t scene_num, ePosKind kind);
static void sc_indication_nordic_fw(uint16_t id, uint8_t* p_data);
static void sc_indication_motor_fw(uint16_t id, uint8_t* p_data);
static void sc_indication_group(uint16_t id, uint8_t* p_data);
static void sc_indication_multi_packet(uint16_t id, uint16_t len, uint8_t* p_data);
static void sc_battery_measurement_callback(void *);
//DEBUG:
static void print_serial_msg(char *title, uint8_t * msg_str);

#define DEBUG_PRINT

/* Local variables
*******************************************************************************/
static SC_SHADE_DISC_REC_PTR SC_DiscoverHead;
static SC_SHADE_DISC_REC_PTR SC_DiscoverTail;
static bool SC_DiscoveryActive = false;
static bool SC_CapturingShades;
static uint16_t SC_DiscoverRetryCount;
static RNC_CONFIG_REC_PTR SC_HeadAddress;
static RNC_CONFIG_REC_PTR SC_TailAddress;
static bool SC_JoinEnabled = false;
static uint8_t SC_TxHandle = 0;
static bool SC_AbsoluteDiscoveryActive =  false;
static BATT_CHECK_STRUCT_PTR p_BattCheckData;
static uint16_t MaxShades;
static uint16_t ShadeIndex;
static uint16_t BattCheckRetryCount;
static SHADE_POSITION SC_Positions;
static bool SC_FinalPacketData;
static uint16_t SC_BatteryCheckToken = NULL_TOKEN;
uint16_t SC_RedundantChecksum = 0xffff;
static bool SC_SingleShadeBatteryCheck = false;
uint16_t SC_LowBatteryCount;
static bool SC_IsForceBatteryCheck = false;
static bool SC_MaintainFlash;
uint16_t SC_NetworkJoinScheduleToken;
eDiscoveryType SC_DiscoveryType;

bool SC_AvoidContinueDiscovery = false;

/*****************************************************************************//**
* @brief The following functions provide an internal API between the UI/database<br/>
*   and the radio message management task.  All of these functions are called<br/>
*   in the context of the calling task.  A mailbox is used to pass data to the<br/>
*   RNC_RfNetworkConfig task which then buffers one or more shade commands in a<br/>
*   linked list and manages sending these commands to the Nordic one at a time.
*
*******************************************************************************/
void SC_ConditionalDiscovery( void )  //Deprecated
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_CONDITIONAL_DISCOVER;
    p_cmd->adr_mode = (uint8_t)P3_Address_Mode_Device_Id;
    p_cmd->address.Unique_Id = 0;
    p_cmd->address.Device_Id = ALL_DEVICES_ADDRESS;
    p_cmd->p_callback = SC_discovery_callback;
    RNC_SendShadeRequest(p_cmd);
}
void SC_AbsoluteDiscovery( void )
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_ABSOLUTE_DISCOVER;
    p_cmd->adr_mode = (uint8_t)P3_Address_Mode_Device_Id;
    p_cmd->address.Unique_Id = 0;
    p_cmd->address.Device_Id = ALL_DEVICES_ADDRESS;
    p_cmd->p_callback = SC_discovery_callback;
    RNC_SendShadeRequest(p_cmd);
}
void SC_SetDiscoveredFlag( P3_Address_Mode_Type adr_mode,
                            P3_Address_Internal_Type * address )
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_SET_DISCOVERED_FLAG;
    p_cmd->adr_mode = (uint8_t)adr_mode;
    p_cmd->address.Unique_Id = address->Unique_Id;
    RNC_SendShadeRequest(p_cmd);
}
void SC_SetDiscoveredFlagProc( P3_Address_Mode_Type adr_mode,
                            P3_Address_Internal_Type * address )
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_SET_DISCOVERED_FLAG;
    p_cmd->adr_mode = (uint8_t)adr_mode;
    p_cmd->address.Unique_Id = address->Unique_Id;
    p_cmd->p_callback = SC_discovery_callback;
    RNC_SendShadeRequest(p_cmd);
}
void SC_GetShadePosition( P3_Address_Mode_Type adr_mode,
                            P3_Address_Internal_Type * address )
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_GET_SHADE_POSITION;
    p_cmd->adr_mode = (uint8_t)adr_mode;
    p_cmd->address.Unique_Id = address->Unique_Id;
    RNC_SendShadeRequest(p_cmd);
}
void SC_SetShadePosition( P3_Address_Mode_Type adr_mode,
                            P3_Address_Internal_Type * address,
                            strPositions * pos )
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_SET_SHADE_POSITION;
    p_cmd->adr_mode = (uint8_t)adr_mode;
    p_cmd->address.Unique_Id = address->Unique_Id;
    p_cmd->data.pos_data.posCount = pos->posCount;
    p_cmd->data.pos_data.posKind[0] = pos->posKind[0];
    p_cmd->data.pos_data.position[0] = pos->position[0];
    p_cmd->data.pos_data.posKind[1] = pos->posKind[1];
    p_cmd->data.pos_data.position[1] = pos->position[1];
    RNC_SendShadeRequest(p_cmd);
}
void SC_MoveShade( P3_Address_Mode_Type adr_mode,
                            P3_Address_Internal_Type * address,
                            MOVE_DIRECTION_T dir)
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_MOVE_SHADE;
    p_cmd->adr_mode = (uint8_t)adr_mode;
    p_cmd->address.Unique_Id = address->Unique_Id;
    p_cmd->data.move_data.dir = dir;
    RNC_SendShadeRequest(p_cmd);
}
void SC_RequestShadeStatus(P3_Address_Mode_Type adr_mode,
                             P3_Address_Internal_Type * address)
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_REQUEST_SHADE_STATUS;
    p_cmd->adr_mode = (uint8_t)adr_mode;
    p_cmd->address.Unique_Id = address->Unique_Id;
    RNC_SendShadeRequest(p_cmd);
}
void SC_RequestShadeType(P3_Address_Mode_Type adr_mode,
                             P3_Address_Internal_Type * address)
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_REQUEST_SHADE_TYPE;
    p_cmd->adr_mode = (uint8_t)adr_mode;
    p_cmd->address.Unique_Id = address->Unique_Id;
    RNC_SendShadeRequest(p_cmd);
}
void SC_RequestBatteryLevel(P3_Address_Mode_Type adr_mode,
                             P3_Address_Internal_Type * address,
                             void(*p_callback)(void*))
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_REQUEST_BATTERY_LEVEL;
    p_cmd->adr_mode = (uint8_t)adr_mode;
    p_cmd->address.Unique_Id = address->Unique_Id;
    p_cmd->p_callback = p_callback;
    RNC_SendShadeRequest(p_cmd);
}
void SC_CheckShadeBattery(P3_Address_Mode_Type adr_mode,
                             P3_Address_Internal_Type * p_address)
{
    SC_SingleShadeBatteryCheck = true;
    SC_RequestBatteryLevel(adr_mode, p_address, sc_battery_measurement_callback);
}
void SC_SetSceneAtPosition( P3_Address_Mode_Type adr_mode,
                            P3_Address_Internal_Type * address,
                            uint8_t scene_id,
                            strPositions * pos )
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_SET_SCENE_POSITION;
    p_cmd->adr_mode = (uint8_t)adr_mode;
    p_cmd->address.Unique_Id = address->Unique_Id;
    p_cmd->data.scene_data.count = 1;
    p_cmd->data.scene_data.id_list[0] = scene_id;
    p_cmd->data.scene_data.posCount = pos->posCount;
    p_cmd->data.scene_data.posKind[0] = pos->posKind[0];
    p_cmd->data.scene_data.position[0] = pos->position[0];
    p_cmd->data.scene_data.posKind[1] = pos->posKind[1];
    p_cmd->data.scene_data.position[1] = pos->position[1];
    RNC_SendShadeRequest(p_cmd);
	OS_TaskSleep(1000);
}
void SC_SetSceneToCurrent( P3_Address_Mode_Type adr_mode,
                            P3_Address_Internal_Type * address,
                            uint8_t scene_id)
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_SET_SCENE;
    p_cmd->adr_mode = (uint8_t)adr_mode;
    p_cmd->address.Unique_Id = address->Unique_Id;
    p_cmd->data.scene_data.count = 1;
    p_cmd->data.scene_data.id_list[0] = scene_id;
    RNC_SendShadeRequest(p_cmd);
	OS_TaskSleep(1000);
}
void SC_ExecuteSceneProc(uint8_t scene_count, uint8_t * scene_id, void(*p_callback)(void*))
{
    int i;
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_EXECUTE_SCENE;
    p_cmd->adr_mode = (uint8_t)P3_Address_Mode_Device_Id;
    p_cmd->address.Unique_Id = 0;
    p_cmd->address.Device_Id = ALL_DEVICES_ADDRESS;
    p_cmd->data.scene_data.count = scene_count;
    for(i = 0; i<scene_count; ++i) {
        p_cmd->data.scene_data.id_list[i] = scene_id[i];
    }
    p_cmd->p_callback = p_callback;
    RNC_SendShadeRequest(p_cmd);
}
void SC_DeleteScene( P3_Address_Mode_Type adr_mode,
                        P3_Address_Internal_Type * address, 
                        uint8_t scene_id)
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_DELETE_SCENE;
    p_cmd->adr_mode = (uint8_t)adr_mode;
    p_cmd->address.Unique_Id = address->Unique_Id;
    p_cmd->data.scene_data.count = 1;
    p_cmd->data.scene_data.id_list[0] = scene_id;
    RNC_SendShadeRequest(p_cmd);
}
void SC_RequestScenePosition( P3_Address_Mode_Type adr_mode,
                        P3_Address_Internal_Type * address, 
                        uint8_t scene_id)
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_REQUEST_SCENE_POSITION;
    p_cmd->adr_mode = (uint8_t)adr_mode;
    p_cmd->address.Unique_Id = address->Unique_Id;
    p_cmd->data.scene_data.count = 1;
    p_cmd->data.scene_data.id_list[0] = scene_id;
    RNC_SendShadeRequest(p_cmd);
}
void SC_JogShade(P3_Address_Mode_Type adr_mode,
                             P3_Address_Internal_Type * address)
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_JOG_SHADE;
    p_cmd->adr_mode = (uint8_t)adr_mode;
    p_cmd->address.Unique_Id = address->Unique_Id;
    RNC_SendShadeRequest(p_cmd);
}
void SC_RequestReceiverFW(P3_Address_Mode_Type adr_mode,
                             P3_Address_Internal_Type * address)
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_REQUEST_RECEIVER_FW;
    p_cmd->adr_mode = (uint8_t)adr_mode;
    p_cmd->address.Unique_Id = address->Unique_Id;
    RNC_SendShadeRequest(p_cmd);
}
void SC_RequestMotorFW(P3_Address_Mode_Type adr_mode,
                             P3_Address_Internal_Type * address)
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_REQUEST_MOTOR_FW;
    p_cmd->adr_mode = (uint8_t)adr_mode;
    p_cmd->address.Unique_Id = address->Unique_Id;
    RNC_SendShadeRequest(p_cmd);
}
void SC_RequestGroup(P3_Address_Mode_Type adr_mode,
                        P3_Address_Internal_Type * address)
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_REQUEST_GROUP;
    p_cmd->adr_mode = (uint8_t)adr_mode;
    p_cmd->address.Unique_Id = address->Unique_Id;
    RNC_SendShadeRequest(p_cmd);
}
void SC_GroupAssign(P3_Address_Mode_Type adr_mode,
                        P3_Address_Internal_Type * address,
                        uint8_t group_id,
                        bool isAssigned)
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_GROUP_ASSIGN;
    p_cmd->adr_mode = (uint8_t)adr_mode;
    p_cmd->address.Unique_Id = address->Unique_Id;
    p_cmd->data.group_data.id = group_id;
    p_cmd->data.group_data.is_assigned = isAssigned;
    RNC_SendShadeRequest(p_cmd);
	OS_TaskSleep(1000);
}
void SC_ResetShade(P3_Address_Mode_Type adr_mode,
                        P3_Address_Internal_Type * address,
                        uint16_t cfg)
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_RESET_SHADE;
    p_cmd->adr_mode = (uint8_t)adr_mode;
    p_cmd->address.Unique_Id = address->Unique_Id;
    p_cmd->data.reset_data = cfg;
    RNC_SendShadeRequest(p_cmd);
	OS_TaskSleep(1000);
}
void sc_send_raw_payload(P3_Address_Mode_Type adr_mode,
                        P3_Address_Internal_Type * address,
                        uint8_t len, uint8_t * msg)
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_RAW_PAYLOAD;
    p_cmd->adr_mode = (uint8_t)adr_mode;
    p_cmd->address.Unique_Id = address->Unique_Id;
    p_cmd->data.raw_data.len = len;
    memcpy(p_cmd->data.raw_data.msg, msg, len);
    RNC_SendShadeRequest(p_cmd);
}
void SC_IssueBeacon(void)
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_ISSUE_BEACON;
    RNC_SendShadeRequest(p_cmd);
}
void SC_GetDebugStatus(P3_Address_Mode_Type adr_mode,
                            P3_Address_Internal_Type * address )
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_REQUEST_DEBUG_STATUS;
    p_cmd->adr_mode = (uint8_t)adr_mode;
    p_cmd->address.Unique_Id = address->Unique_Id;
    RNC_SendShadeRequest(p_cmd);
}
void SC_SceneControllerClearedAck(uint16_t controller_id)
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_SCENE_CTL_CLEARED_ACK;
    p_cmd->adr_mode = (uint8_t)P3_Address_Mode_Device_Id;
    p_cmd->address.Unique_Id = 0;
    p_cmd->address.Device_Id = controller_id;
    RNC_SendShadeRequest(p_cmd);
}
void SC_SceneControllerUpdateHeader(uint16_t controller_id, SC_SCENE_CTL_UPDATE_HDR_STR_PTR p_update_hdr)
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_SCENE_CTL_UPDATE_HEADER;
    p_cmd->adr_mode = (uint8_t)P3_Address_Mode_Device_Id;
    p_cmd->address.Unique_Id = 0;
    p_cmd->address.Device_Id = controller_id;
    memcpy(&p_cmd->data.scene_ctl_update_header, p_update_hdr, sizeof(SC_SCENE_CTL_UPDATE_HDR_STR));
    RNC_SendShadeRequest(p_cmd);
}
void SC_SceneControllerUpdatePacket(uint16_t controller_id, SC_SCENE_CTL_UPDATE_PACKET_STR_PTR p_update_packet)
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_SCENE_CTL_UPDATE_PACKET;
    p_cmd->adr_mode = (uint8_t)P3_Address_Mode_Device_Id;
    p_cmd->address.Unique_Id = 0;
    p_cmd->address.Device_Id = controller_id;
    memcpy(&p_cmd->data.scene_ctl_update_packet, p_update_packet, sizeof(SC_SCENE_CTL_UPDATE_PACKET_STR));
    RNC_SendShadeRequest(p_cmd);
}
void SC_SceneControllerTriggerAck(uint16_t controller_id, SC_SCENE_CTL_TRIGGER_ACK_STR_PTR p_trigger_ack)
{
    SKIP_IF_NO_NETWORK;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_SCENE_CTL_TRIGGER_ACK;
    p_cmd->adr_mode = (uint8_t)P3_Address_Mode_Device_Id;
    p_cmd->address.Unique_Id = 0;
    p_cmd->address.Device_Id = controller_id;
    memcpy(&p_cmd->data.scene_ctl_trigger_ack, p_trigger_ack, sizeof(SC_SCENE_CTL_TRIGGER_ACK_STR));
    RNC_SendShadeRequest(p_cmd);
}


/*****************************************************************************//**
* @brief A single raw command is now within the context of RNC_RfNetworkConfig task.
*******************************************************************************/
void SC_LoadNewCommand(SHADE_COMMAND_INSTRUCTION_PTR p_cmd)
{
    bool shade_data_req = true;
    char n;
    int i;
    SHADE_DATA_REQ req;
    req.p_callback = p_cmd->p_callback;
    req.adr_mode = p_cmd->adr_mode;
    req.adr.Unique_Id = p_cmd->address.Unique_Id;
    SC_RedundantChecksum = 0xffff;

    switch (p_cmd->cmd_type) {

        case SC_CONDITIONAL_DISCOVER:
            req.len = 2;
            req.msg[0] = 'C';
            req.msg[1] = 'F';
            break;
        case SC_ABSOLUTE_DISCOVER:
            req.len = 2;
            req.msg[0] = 'C';
            req.msg[1] = 'A';
            break;
        case SC_GET_SHADE_POSITION:
            req.len = 11;
            req.msg[0] = '?';
            req.msg[1] = 'Z';
            req.msg[2] = 2;
            req.msg[3] = '?';
            req.msg[4] = 'P';
            req.msg[5] = 2;
            req.msg[6] = '?';
            req.msg[7] = 'M';
            req.msg[8] = 2;
            req.msg[9] = '?';
            req.msg[10] = 'T';
            break;
        case SC_SET_DISCOVERED_FLAG:
            req.len = 2;
            req.msg[0] = 'C';
            req.msg[1] = 'D';
            break;
        case SC_SET_SHADE_POSITION:
            req.len = (p_cmd->data.pos_data.posCount * 5) + 2;
            req.msg[0] = '?';
            req.msg[1] = 'Z';
            for (i = 0; i < p_cmd->data.pos_data.posCount; ++i) {
                req.msg[(i * 5) + 2] = 4;
                req.msg[(i * 5) + 3] = '@';
                if (p_cmd->data.pos_data.posKind[i] == pkPrimaryRail) {
                    req.msg[(i * 5) + 4] = 'P';
                }
                else if (p_cmd->data.pos_data.posKind[i] == pkSecondaryRail) {
                    req.msg[(i * 5) + 4] = 'M';
                }
                else { //kind == pkVaneTilt
                    req.msg[(i * 5) + 4] = 'T';
                }
                req.msg[(i * 5) + 5] = (uint8_t)(p_cmd->data.pos_data.position[i] & 0xff);
                req.msg[(i * 5) + 6] = (uint8_t)((p_cmd->data.pos_data.position[i] & 0xff00)>>8);
            }

            break;
        case SC_MOVE_SHADE:
            req.len = 2;
            req.msg[0] = 'R';
            if ( p_cmd->data.move_data.dir == DIRECTION_UP) {
                req.msg[1] = 'U';
            }
            else if (p_cmd->data.move_data.dir == DIRECTION_DOWN) {
                req.msg[1] = 'D';
            }
            else { //stop
                req.msg[1] = 'S';
            }
            break;
        case SC_REQUEST_BATTERY_LEVEL:
            req.len = 5;
            req.msg[0] = '?';
            req.msg[1] = 'Z';
            req.msg[2] = 2;
            req.msg[3] = '?';
            req.msg[4] = 'B';
            break;
        case SC_REQUEST_RECEIVER_FW:
            req.len = 3;
            req.msg[0] = '?';
            req.msg[1] = 'F';
            req.msg[2] = 'N';
            break;
        case SC_REQUEST_MOTOR_FW:
            req.len = 3;
            req.msg[0] = '?';
            req.msg[1] = 'F';
            req.msg[2] = 'C';
            break;
        case SC_REQUEST_SHADE_TYPE:
            req.len = 3;
            req.msg[0] = '?';
            req.msg[1] = 'D';
            req.msg[2] = 'S';
            break;
        case SC_SET_SCENE_POSITION:
            req.len = (p_cmd->data.scene_data.posCount * 6) + 2;
            req.msg[0] = '?';
            req.msg[1] = 'Z';
            for (i = 0; i < p_cmd->data.scene_data.posCount; ++i) {
                req.msg[(i * 6) + 2] = 5;
                req.msg[(i * 6) + 3] = 'S';
                if (p_cmd->data.scene_data.posKind[i] == pkPrimaryRail) {
                    req.msg[(i * 6) + 4] = 'P';
                }
                else if (p_cmd->data.scene_data.posKind[i] == pkSecondaryRail) {
                    req.msg[(i * 6) + 4] = 'M';
                }
                else { //kind == pkVaneTilt
                    req.msg[(i * 6) + 4] = 'T';
                }
                req.msg[(i * 6) + 5] = p_cmd->data.scene_data.id_list[0];
                req.msg[(i * 6) + 6] = (uint8_t)(p_cmd->data.scene_data.position[i] & 0xff);
                req.msg[(i * 6) + 7] = (uint8_t)((p_cmd->data.scene_data.position[i] & 0xff00)>>8);
            }
            break;
        case SC_SET_SCENE:
            req.len = 3;
            req.msg[0] = 'S';
            req.msg[1] = 'C';
            req.msg[2] = p_cmd->data.scene_data.id_list[0];
            break;
        case SC_EXECUTE_SCENE:
            req.msg[0] = 'S';
            req.msg[1] = 'G';
            for (i = 0; i < p_cmd->data.scene_data.count; ++i) {
                req.msg[i+2] = p_cmd->data.scene_data.id_list[i];
            }
            req.len = 2 + p_cmd->data.scene_data.count;
            break;
        case SC_DELETE_SCENE:
            req.len = 3;
            req.msg[0] = 'S';
            req.msg[1] = 'D';
            req.msg[2] = p_cmd->data.scene_data.id_list[0];
            break;
        case SC_REQUEST_SCENE_POSITION:
            req.len = 17;
            req.msg[0] = '?';
            req.msg[1] = 'Z';
            req.msg[2] = 4;
            req.msg[3] = '?';
            req.msg[4] = 'S';
            req.msg[5] = 'P';
            req.msg[6] = p_cmd->data.scene_data.id_list[0];
            req.msg[7] = 4;
            req.msg[8] = '?';
            req.msg[9] = 'S';
            req.msg[10] = 'M';
            req.msg[11] = p_cmd->data.scene_data.id_list[0];
            req.msg[12] = 4;
            req.msg[13] = '?';
            req.msg[14] = 'S';
            req.msg[15] = 'T';
            req.msg[16] = p_cmd->data.scene_data.id_list[0];
            break;
        case SC_JOG_SHADE:
            req.len = 3;
            req.msg[0] = 'c';
            req.msg[1] = 'j';
            req.msg[2] = 1;
            break;
        case SC_REQUEST_DEBUG_STATUS:
            req.len = 2;
            req.msg[0] = '?';
            req.msg[1] = 'X';
            break;
        case SC_REQUEST_GROUP:
            req.len = 2;
            req.msg[0] = '?';
            req.msg[1] = 'g';
            break;
        case SC_RESET_SHADE:
            req.len = 4;
            req.msg[0] = '@';
            req.msg[1] = 'r';
            req.msg[2] = (uint8_t)(p_cmd->data.reset_data & 0xff);
            req.msg[3] = (uint8_t)((p_cmd->data.reset_data & 0xff00)>>8);
            break;
        case SC_RAW_PAYLOAD:
            req.len = p_cmd->data.raw_data.len;
            memcpy(req.msg, p_cmd->data.raw_data.msg, req.len);
            break;
        case SC_SCENE_CTL_CLEARED_ACK:
            req.len = 3;
            req.msg[0] = '!';
            req.msg[1] = 'K';
            req.msg[2] = 'X';
            break;
        case SC_SCENE_CTL_UPDATE_HEADER:
            req.len = sizeof(SC_SCENE_CTL_UPDATE_HDR_STR)+3;
            req.msg[0] = '!';
            req.msg[1] = 'K';
            req.msg[2] = 'U';
            memcpy(&req.msg[3],
                    &p_cmd->data.scene_ctl_update_header.rec_count,
                    sizeof(SC_SCENE_CTL_UPDATE_HDR_STR));
            break;
        case SC_SCENE_CTL_UPDATE_PACKET:
            req.len = sizeof(SC_SCENE_CTL_UPDATE_PACKET_STR)+3;
            req.msg[0] = '!';
            req.msg[1] = 'K';
            req.msg[2] = 'u';
            memcpy(&req.msg[3],
                    &p_cmd->data.scene_ctl_update_packet.rec_count,
                    sizeof(SC_SCENE_CTL_UPDATE_PACKET_STR));
            break;
        case SC_SCENE_CTL_TRIGGER_ACK:
            req.len = sizeof(SC_SCENE_CTL_TRIGGER_ACK_STR) + 3;
            req.msg[0] = '!';
            req.msg[1] = 'K';
            req.msg[2] = 't';
            req.msg[3] = p_cmd->data.scene_ctl_trigger_ack.scene_type;

            req.msg[4] = (uint8_t)(p_cmd->data.scene_ctl_trigger_ack.scene_id & 0xff);
            req.msg[5] = (uint8_t)((p_cmd->data.scene_ctl_trigger_ack.scene_id & 0xff00)>>8);
            req.msg[6] = p_cmd->data.scene_ctl_trigger_ack.version;
            break;
        case SC_GROUP_ASSIGN:
            shade_data_req = false;
            sc_group_assign_exec(p_cmd->adr_mode,
                            &p_cmd->address,
                            p_cmd->data.group_data.id,
                            p_cmd->data.group_data.is_assigned);
            break;
        case SC_ISSUE_BEACON:
			
			// marker 06/22/2016
			sendTextMessageToSlaveHubs("issue beacon");
		
            shade_data_req = false;
            sc_issue_beacon_exec();
            break;

        default:
            shade_data_req = false;
            break;
    }
    OS_ReleaseMsgMemBlock((void *)p_cmd);
    if (shade_data_req == true) {
        req.tx_opt = TX_OPTION;
        sc_build_shade_data_request(&req);
    }
    SC_GetNextMessageToSend();
}

/*****************************************************************************//**
* @brief This function a shade data request message.
*
* @param p_req. A pointer to SHADE_DATA_REQ
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/06/2014    Created.
*******************************************************************************/
static void sc_build_shade_data_request(SHADE_DATA_REQ_PTR p_req)
{
    int i;
    SHADE_DATA_REQ_MSG_STRUCT payload_rec;
    payload_rec.p_callback = p_req->p_callback;
    payload_rec.hdr.length = SHADE_DATA_REQUEST_HDR_SIZE;
    payload_rec.hdr.request_type = MSG_TYPE_SEND_SHADE_DATA_REQ;
    payload_rec.hdr.source_mode = (uint8_t)P3_Address_Mode_Device_Id;
    payload_rec.hdr.dest_mode = p_req->adr_mode;
    payload_rec.hdr.dest_adr.Unique_Id = p_req->adr.Unique_Id; //copies memory

    for (i=0; i<p_req->len; ++i) {
        payload_rec.msg_payload[i] = p_req->msg[i];
    }
    payload_rec.hdr.length = payload_rec.hdr.length + p_req->len;

    payload_rec.hdr.tx_options = p_req->tx_opt;
    payload_rec.hdr.tx_handle = SC_TxHandle++;

    SC_build_config_record(&payload_rec);
}

/*****************************************************************************//**
* @brief This function builds a RNC_CONFIG_REC for a shade configuration message
*  that is read for transport.
*
* @param p_pay_rec. A pointer to SHADE_DATA_REQ_MSG_STRUCT_PTR
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/06/2014    Created.
*******************************************************************************/
static void SC_build_config_record(SHADE_DATA_REQ_MSG_STRUCT_PTR p_pay_rec)
{
    RNC_CONFIG_REC_PTR p_cfg_rec;

    //create a blank RNC_CONFIG_REC for a shade configuration
    p_cfg_rec = SC_create_blank_record();
    p_cfg_rec->p_callback = p_pay_rec->p_callback;
    p_cfg_rec->dest_dev_type = DESTINATION_SHADE;
    p_cfg_rec->dest_mode = p_pay_rec->hdr.dest_mode;
    p_cfg_rec->id.Unique_Id = p_pay_rec->hdr.dest_adr.Unique_Id;

    RNC_AddTransportLayer(&p_cfg_rec->ser_msg[0], (uint8_t*)&p_pay_rec->hdr.length);
print_serial_msg("config record", &p_cfg_rec->ser_msg[0]);

    //indicate that this message is ready to be sent
    p_cfg_rec->state = WAITING_TO_SEND_STATE;
}

/*****************************************************************************//**
* @brief This function builds a command to assign or remove a shade from a group.
*
* @param adr_mode.  A P3_Address_Mode eumeration representing the address type.
* @param address.  Pointer to the address of the shade.
* @param group_id.  ID of group to add shade to or remove shade from.
* @param isAssigned.  True to add to group, false to remove from group.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/06/2014    Created.
*******************************************************************************/
static void sc_group_assign_exec(P3_Address_Mode_Type adr_mode,
                             P3_Address_Internal_Type * address, uint8_t group_id, bool isAssigned)
{
    GROUP_SET_REQUEST_HEADER_STRUCT p_payload_rec;
    RNC_CONFIG_REC_PTR p_cfg_rec;
    p_payload_rec.length = GROUP_SET_REQUEST_HDR_SIZE;
    p_payload_rec.request_type = MSG_TYPE_SEND_GROUP_SET_REQ;
    p_payload_rec.dest_mode = (uint8_t)adr_mode;
    //Note: setting to UUID forces compiler to copy memory
    p_payload_rec.dest_adr.Unique_Id = address->Unique_Id;
    p_payload_rec.group_id = group_id;
    p_payload_rec.is_assigned = (uint8_t)isAssigned;
    p_payload_rec.tx_options = TX_OPTION | TX_OPTION_NO_NET_HEADER;

    //create a blank RNC_CONFIG_REC
    p_cfg_rec = SC_create_blank_record();
    p_cfg_rec->dest_dev_type = DESTINATION_SHADE;
    p_cfg_rec->dest_mode = p_payload_rec.dest_mode;
    p_cfg_rec->id.Unique_Id = p_payload_rec.dest_adr.Unique_Id;

    RNC_AddTransportLayer(&p_cfg_rec->ser_msg[0], (uint8_t*)&p_payload_rec);
print_serial_msg("group assign", &p_cfg_rec->ser_msg[0]);

    //indicate that this message is ready to be sent
    p_cfg_rec->state = WAITING_TO_SEND_STATE;
}

/*****************************************************************************//**
* @brief This function builds a beacon message.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/06/2014    Created.
*******************************************************************************/
static void sc_issue_beacon_exec(void)
{
    BEACON_REQUEST_HEADER_STRUCT p_payload_rec;
    RNC_CONFIG_REC_PTR p_cfg_rec;

    p_payload_rec.length = BEACON_REQUEST_HDR_SIZE;
    p_payload_rec.request_type = MSG_TYPE_SEND_BEACON_REQ;
    p_payload_rec.tx_options = TX_OPTION | TX_OPTION_NO_NET_HEADER;

    //create a blank RNC_CONFIG_REC
    p_cfg_rec = SC_create_blank_record();
    p_cfg_rec->dest_dev_type = DESTINATION_SHADE;
    //the following two parameters are not used for beacon
    p_cfg_rec->dest_mode = (uint8_t)P3_Address_Mode_None;
    p_cfg_rec->id.Unique_Id = 0;


    RNC_AddTransportLayer(&p_cfg_rec->ser_msg[0], (uint8_t*)&p_payload_rec);
print_serial_msg("beacon", &p_cfg_rec->ser_msg[0]);

    //indicate that this message is ready to be sent
    p_cfg_rec->state = WAITING_TO_SEND_STATE;
}

/*****************************************************************************//**
* @brief Debug function to print a serial message.
*
* @param pointer to message string. Note: second byte is length.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/06/2014    Created.
*******************************************************************************/
static void print_serial_msg(char *title, uint8_t * msg_str)
{
    uint8_t c;
    printf(title);
    printf("s: ");
    for (c = 0; c < msg_str[1]+3; ++c)
    {
        printf("%02X ",msg_str[c]);
    }
    printf("\r\n");
}

/*****************************************************************************//**
* @brief This function acquires a memory block to hold a structure of type<br/>
*  RNC_CONFIG_REC.  This structure is part of a linked list and this function adds
*  the new record to the list.
*
* @param nothing.
* @return a pointer to a new memory block containing a RNC_CONFIG_REC.
* @author Neal Shurmantine
* @version
* 11/06/2014    Created.
*******************************************************************************/
static RNC_CONFIG_REC_PTR SC_create_blank_record(void)
{
    RNC_CONFIG_REC_PTR p_cfg_rec;
    RNC_CONFIG_REC_PTR p_last_rec;

    //get memory for a new record (freed in SC_clear_item_in_list)

    p_cfg_rec = (RNC_CONFIG_REC_PTR)OS_GetMsgMemBlock(sizeof (RNC_CONFIG_REC));

    p_cfg_rec->rf_retry_count = 0;
    p_cfg_rec->rf_retry_max = SC_MAX_MSG_TRIES;

    //if no records in the list
    if (SC_HeadAddress == NULL)
    {
        //create list with this record only
        SC_HeadAddress = p_cfg_rec;
        SC_TailAddress = p_cfg_rec;
        p_cfg_rec->p_prev_rec = NULL;
        p_cfg_rec->p_next_rec = NULL;
    }
    else
    {
        //add this record to the end of the list
        p_last_rec = SC_TailAddress;
        p_last_rec->p_next_rec = p_cfg_rec;
        SC_TailAddress = p_cfg_rec;
        p_cfg_rec->p_prev_rec = p_last_rec;
        p_cfg_rec->p_next_rec = NULL;
    }
    return p_cfg_rec;
}

/*****************************************************************************//**
* @brief This function searches through the linked list of config records to see
*  if any are waiting for serial transmission.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/06/2014    Created.
*******************************************************************************/
void SC_GetNextMessageToSend(void)
{
    RNC_CONFIG_REC_PTR p_cfg_rec;
    RNC_CONFIG_REC_PTR p_next_send = NULL;

    //There are only three states that a message may be in:
    //  waiting to be sent, waiting for a confirmation from nordic
    //  or message just sent and waiting for a timeout before
    //  clearing message and sending the next.
    //Search through the list and make sure all are waiting to
    //  be sent, if true, send the first one on list.
    p_cfg_rec = SC_HeadAddress;
    while (p_cfg_rec != NULL)
    {
        if (p_cfg_rec->state == WAITING_TO_SEND_STATE)
        {
            if (p_next_send == NULL) {
                //this is the first available message in the list
                p_next_send = p_cfg_rec;
            }
            p_cfg_rec = p_cfg_rec->p_next_rec;
        }
        else
        {
            //This message was just sent, wait for confirmation
            //or timeout before checking again
            //escape while loop instead
            p_next_send = NULL;
            p_cfg_rec = NULL;
        }
    }
    if (p_next_send != NULL)
    {
        p_next_send->state = WAITING_FOR_SER_ACK_STATE;
        RFO_DeliverRequest(p_next_send);
        LED_Flicker(true);
    }
}

/*****************************************************************************//**
* @brief This function searches through the linked list of config records and<br/>
*  removes the record matching the passed in value.
*
* @param p_active_msg is a pointer to a config record to be removed.  The spot<br/>
*   in the linked list is removed and the memory block freed.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/06/2014    Created.
*******************************************************************************/
static void SC_clear_item_in_list(RNC_CONFIG_REC_PTR p_active_msg)
{
    RNC_CONFIG_REC_PTR p_prev_list;
    RNC_CONFIG_REC_PTR p_next_list;

    //remove from list of pending msgs
    if ((SC_HeadAddress != p_active_msg) && (SC_TailAddress != p_active_msg))
    {
        p_prev_list = p_active_msg->p_prev_rec;
        p_next_list = p_active_msg->p_next_rec;
        p_prev_list->p_next_rec = p_next_list;
        p_next_list->p_prev_rec = p_prev_list;
    }
    else
    {
        if (SC_HeadAddress == p_active_msg)
        {
            SC_HeadAddress = p_active_msg->p_next_rec;
            p_next_list = p_active_msg->p_next_rec;
            if (p_next_list != NULL)
            {
                p_next_list->p_prev_rec = NULL;
            }
        }
        if (SC_TailAddress == p_active_msg)
        {
            SC_TailAddress = p_active_msg->p_prev_rec;
            p_prev_list = p_active_msg->p_prev_rec;
            if (p_prev_list != NULL)
            {
                p_prev_list->p_next_rec = NULL;
            }
        }
    }
    //free memory
    OS_ReleaseMsgMemBlock((void *)p_active_msg);
}

/*****************************************************************************//**
* @brief This function removes all pending shade configuration messages and<br/>
*  frees the memory.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 12/04/2014    Created.
*******************************************************************************/
static void sc_cancel_configurations(void)
{
    RNC_CONFIG_REC_PTR p_cfg_rec;
    RNC_CONFIG_REC_PTR p_del_rec;

    p_cfg_rec = SC_HeadAddress;
    while (p_cfg_rec != NULL)
    {
        p_del_rec = p_cfg_rec;
        p_cfg_rec = p_cfg_rec->p_next_rec;
        OS_ReleaseMsgMemBlock((void *)p_del_rec);
    }
    SC_HeadAddress = NULL;
    SC_TailAddress = NULL;
}

/*****************************************************************************//**
* @brief This function is called when an event is received indicating that a<br/>
*  serial response has been received from the Nordic to a shade data config request.
*
* @param p_ser_resp is a pointer to the result code of the serial message.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/06/2014    Created.
*******************************************************************************/
uint8_t * SC_ProcessShadeConfirmation(PARSE_KEY_STRUCT_PTR p_ser_msg)
{
    uint8_t*  p_rslt;
    p_rslt = (uint8_t *)OS_GetMsgMemBlock(sizeof(uint8_t));
    uint8_t res_type = p_ser_msg->generic_conf.confirmation_type;
    switch (res_type) {
        case MSG_TYPE_SEND_SHADE_DATA_CONF:
            *p_rslt = (uint8_t)p_ser_msg->shade_conf.status;
            break;
        case MSG_TYPE_SEND_BEACON_CONF:
            *p_rslt = (uint8_t)p_ser_msg->beacon_conf.status;
            break;
        case MSG_TYPE_SEND_GROUP_SET_CONF:
            *p_rslt = (uint8_t)p_ser_msg->group_conf.status;
            break;
        default:
            break;
    }
    OS_ReleaseMsgMemBlock((void *)p_ser_msg);
    return p_rslt;
}

/*****************************************************************************//**
* @brief This function is called when an event is received indicating that a<br/>
*  serial response has been received from the Nordic to a shade data config request.
*
* @param p_ser_resp is a pointer to the result code of the serial message.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/06/2014    Created.
*******************************************************************************/
void SC_HandleShadeConfirmationResult(uint8_t* p_ser_resp)
{
    RNC_CONFIG_REC_PTR p_msg_list;
    RNC_CONFIG_REC_PTR p_active_msg = NULL;

    if (*p_ser_resp != SC_RSLT_TIMEOUT) {
        //if the result is a serial timeout then rfo_outbound task already knows
        RFO_NotifySerialResponse(*p_ser_resp);
        if (*p_ser_resp == SC_RSLT_SUCCESS) {
            printf("ACK\n");
        }
        else {
            printf("NACK(%02X)\n", *p_ser_resp);
        }
    }
    else {
        printf("TIMEOUT\n");

        // marker 02/10/2016 - timeout temporarely disabled for testing
        RC_ForceNordicReset();
    }

    //Search thru list of pending messages to find the one that is waiting
    // for a serial response
    p_msg_list = SC_HeadAddress;
    while (p_msg_list != NULL)
    {
        //if message found then
        if (p_msg_list->state == WAITING_FOR_SER_ACK_STATE)
        {
            //point to the config record
            p_active_msg = p_msg_list;
            p_msg_list = NULL;//set to NULL to exit while loop
        }
        else
        {
            p_msg_list = p_msg_list->p_next_rec;
        }
    }

    //if config record found that was waiting for a serial response
    if (p_active_msg != NULL)
    {
        p_active_msg->state = WAITING_TO_SEND_NEXT;
        if (SC_SingleShadeBatteryCheck == true) {
            p_active_msg->rf_ack_tick_count = SC_SHADE_BATT_REQ_RF_TICK_MAX;
        }
        else
        {
            p_active_msg->rf_ack_tick_count = SC_SHADE_RF_TICK_MAX;
        }
        RNC_StartTickTimer();
    }
    //free serial response memory
    OS_ReleaseMsgMemBlock((uint8_t *)p_ser_resp);
}

/*****************************************************************************//**
* @brief This function is called when an event occurs indicating that a shade<br/>
*  indication message was received.
*
* @param p_rf_response is a data structure of type PARSE_KEY_STRUCT_PTR.
* @return none.
* @author Neal Shurmantine
* @version
* 11/25/2014    Created.
*******************************************************************************/
void SC_HandleShadeIndication(PARSE_KEY_STRUCT_PTR p_rf_response)
{
    uint16_t cksm;
    uint16_t len;
    uint8_t c;

    if (p_rf_response->generic_ind.indication_type == MSG_TYPE_SHADE_DATA_INDICATION) {

        // printf("marker1\n");

        if (RC_IsNetworkIdAssigned() == true) {

            // printf("marker2\n");

            cksm = (uint8_t)(p_rf_response->shade_data_ind.source_adr.Device_Id>>8);
            cksm += (uint8_t)(p_rf_response->shade_data_ind.source_adr.Device_Id & 0xff);
            len = p_rf_response->shade_data_ind.payload_len - SHADE_INDICATION_HEADER_SIZE;
            for (c = 0; c < len; ++c)
            {
                cksm += p_rf_response->shade_data_ind.msg_payload[c];
            }
            if (cksm != SC_RedundantChecksum) {

                // printf("marker3\n");
                sc_shade_indication_received(p_rf_response);
            }
            SC_RedundantChecksum = cksm;
        }
    }
    else if (p_rf_response->generic_ind.indication_type == MSG_TYPE_BEACON_INDICATION) {
        sc_beacon_indication_received(p_rf_response);
    }
    else {
        if (RC_IsNetworkIdAssigned() == true) {
            sc_group_set_indication_received(p_rf_response);
        }
    }
    OS_ReleaseMsgMemBlock((uint8_t *)p_rf_response);
}

/******************************************************************************
* @brief This function is called when shade indication message was received
*  over UDP from a slave
*
* @param p_rf_response is a data structure of type PARSE_KEY_STRUCT_PTR.
* @return none.
* @author Henk Meewis
* @version
* 12/11/2015    Created.
*******************************************************************************/

/* marker 12/16/2015 - for debugging
static void displayShadeIndicationMessagePayload(char *title, PARSE_KEY_STRUCT_PTR shadeMessagePtr)
{
    uint16_t len, n;
    SHADE_DATA_INDICATION_STRUCT_PTR shadeIndicationPtr = &shadeMessagePtr->shade_data_ind;

    if(shadeIndicationPtr->indication_type == MSG_TYPE_SHADE_DATA_INDICATION) {

        len = shadeIndicationPtr->payload_len - SHADE_INDICATION_HEADER_SIZE;
        printf("%s(%d):\n", title, len);
        for(n = 0; n < len; n++) printf("%02x ", shadeIndicationPtr->msg_payload[n]);
        printf("\n\n");
    }
}
*/

void SC_HandleShadeIndicationFromSlave(PARSE_KEY_STRUCT_PTR p_rf_response, uint16_t size)
{
	// displayShadeIndicationMessagePayload("before", p_rf_response);

    PARSE_KEY_STRUCT_PTR p_sig = (PARSE_KEY_STRUCT_PTR)OS_GetMsgMemBlock(size);
    memcpy(p_sig, p_rf_response, size);

	// displayShadeIndicationMessagePayload("after", p_sig);

    //force processing to occur in context of RNC_RFNetworkConfig task
    RNC_SendShadeIndication(p_sig);

    //will run in context of UDPProcessing task which may execute concurrently with
    //same function in context of RNC_RFNetworkConfig task
    //        SC_HandleShadeIndication(p_sig);
    OS_ReleaseMsgMemBlock((uint8_t *)p_rf_response);
}

// marker 02/25/2016 - temporary procedure to display data
/*
static void displaySystemIndication(SYSTEM_INDICATION_STRUCT_PTR sysIndPtr)
{
    uint8_t length = sysIndPtr->payload_len, i;

    printf("\nSystem Indication as received from master\n");
    printf("=========================================\n");
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
*/
/******************************************************************************
* @brief This function uses the system indication message as a payload of
*    a new shade request message to be sent to the Nordic as a message with
*    no network header.
*
* @param p_udp_msg is a udp message of type SYSTEM_INDICATION_STRUCT_PTR.
* @return none.
* @author Neal Shurmantine
* @version
* 02/26/2016    Created.
******************************************************************************
static void SC_load_slave_data_request(SYSTEM_INDICATION_STRUCT_PTR p_udp_msg)
{
    SHADE_DATA_REQ req;
    req.p_callback = 0;
    req.adr_mode = (uint8_t)P3_Address_Mode_Device_Id;
    req.adr.Unique_Id = 0;
    req.adr.Device_Id = ALL_DEVICES_ADDRESS;
    req.tx_opt = TX_OPTION | TX_OPTION_NO_NET_HEADER;
    SC_RedundantChecksum = 0xffff;

    req.len = p_udp_msg->payload_len-2;
    memcpy(req.msg,p_udp_msg->payload,req.len);

    sc_build_shade_data_request(&req);
    SC_GetNextMessageToSend();
}
*/
/******************************************************************************
* @brief This function is called when system indication message was received
*  over UDP from a master
*
* @param p_rf_response is a data structure of type PARSE_KEY_STRUCT_PTR.
* @return none.
* @author Henk Meewis
* @version
* 12/11/2015    Created.
******************************************************************************
void SC_HandleSystemIndicationFromMaster(SYSTEM_INDICATION_STRUCT_PTR p_sys_ind)
{
    // marker 02/25/2016 - for debugging
    displaySystemIndication(p_sys_ind);
    SC_load_slave_data_request(p_sys_ind);

    OS_ReleaseMsgMemBlock((uint8_t *)p_sys_ind);
}
*/


//---------------------------SHADE INDICATION PROCESSING-----------------------------


/*****************************************************************************//**
* @brief This function is called when an rf message is received and it is determined
*  to be a shade indication message.
*
* @param p_rf_response is a data structure of type PARSE_KEY_STRUCT_PTR.
* @return none.
* @author Neal Shurmantine
* @version
* 11/25/2014    Created.
*******************************************************************************/
static void sc_shade_indication_received(PARSE_KEY_STRUCT_PTR p_rf_response)
{
    uint16_t id = p_rf_response->shade_data_ind.source_adr.Device_Id;
    uint8_t c;
    uint8_t first_char = p_rf_response->shade_data_ind.msg_payload[0];
    uint8_t second_char = p_rf_response->shade_data_ind.msg_payload[1];
    SC_Positions.positions.posCount = 0;

    uint16_t len = p_rf_response->shade_data_ind.payload_len - SHADE_INDICATION_HEADER_SIZE;
    printf("Shade Response Payload (ID=%04X):  ",p_rf_response->shade_data_ind.source_adr.Device_Id);
    for (c = 0; c < len; ++c)
    {
        printf("%02X ",p_rf_response->shade_data_ind.msg_payload[c]);
    }
    printf("\r\n");

    if ((first_char == '!') && (second_char == 'Z')) {
        // printf("marker4\n");

        uint16_t len = p_rf_response->shade_data_ind.payload_len - SHADE_INDICATION_HEADER_SIZE;
        sc_indication_multi_packet(id, len - 2, &p_rf_response->shade_data_ind.msg_payload[2]);
    }
    else if ((second_char == 'K') && ( (first_char=='?') || (first_char == '!') || (first_char == '@') ) ) {
        // printf("marker5\n");

        sc_parse_scene_controller_payload(id, &p_rf_response->shade_data_ind.msg_payload[2]);
    }
    else if (first_char == '!') {
        // printf("marker6\n");

        SC_FinalPacketData = true;
        sc_parse_indication_payload(id, &p_rf_response->shade_data_ind.msg_payload[1]);
    }
}

/*****************************************************************************//**
* @brief This function is called when a shade indication message is received and
*  it is determined to contain a multi-data packet.
* sample of multi-data packet with generic position response and battery measurement
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |  '!'  |  'Z'  |  0x05 |  '!'  |  'G'  | 0x00  | 0x00  | 0x00  |  0x03 |  '!'  |  'B'  | 0xB8  |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*
* @param id.  Device ID of shade that sent packet.
* @param len.  Total length of packet payload.
* @param p_data.  Pointer to the packet data.
* @return none.
* @author Neal Shurmantine
* @version
* 11/25/2014    Created.
*******************************************************************************/
static void sc_indication_multi_packet(uint16_t id, uint16_t len, uint8_t* p_data)
{
    uint8_t *p_next;
    uint8_t *p_end;
    p_end = p_data + len;
    SC_FinalPacketData = false;
    while (SC_FinalPacketData == false) {
        p_next = p_data + *p_data + 1;
        len -= (*p_data + 1);
        p_data += 2;
        if ((len==0) || (p_next >= p_end)) {
            SC_FinalPacketData = true;
        }
        sc_parse_indication_payload(id, p_data);
        p_data = p_next;
    }
//Note: update HubRFInterfaceSpec
}

/*****************************************************************************//**
* @brief This function parses a single data packet in the payload and calls the
*  relavant function to handle the message.
*
* @param id.  Device ID of shade that sent packet.
* @param p_data.  Pointer to the packet data.
* @return none.
* @author Neal Shurmantine
* @version
* 11/25/2014    Created.
*******************************************************************************/
static void sc_parse_indication_payload(uint16_t id, uint8_t *p_data)
{
    uint16_t val;
    uint8_t num;
    switch (p_data[0]) {
        case 'D':
            if (p_data[1] == 'S') {
                sc_indication_shade_type(id, p_data+2);
            }
            break;
        case 'B':
            sc_indication_batt_level(id, p_data+1);
            break;
        case 'G':
            val = (uint16_t)p_data[1] + (256 * (uint16_t)p_data[2]);
            if (p_data[3] == 0) {
                sc_indication_cur_pos(id, val, pkPrimaryRail);
            }
            else if (p_data[3] == 1) {
                sc_indication_cur_pos(id, val, pkSecondaryRail);
            }
            else if (p_data[3] == 2) {
                sc_indication_cur_pos(id, val, pkVaneTilt);
            }
            else {
                sc_indication_cur_pos(id, val, pkNone);
            }
            break;
        case 'P':
            val = (uint16_t)p_data[1] + (256 * (uint16_t)p_data[2]);
            sc_indication_cur_pos(id, val, pkPrimaryRail);
            break;
        case 'M':
            val = (uint16_t)p_data[1] + (256 * (uint16_t)p_data[2]);
            sc_indication_cur_pos(id, val, pkSecondaryRail);
            break;
        case 'T':
            val = (uint16_t)p_data[1] + (256 * (uint16_t)p_data[2]);
            sc_indication_cur_pos(id, val, pkVaneTilt);
            break;
        case 'S':
            val = (uint16_t)p_data[3] + (256 * (uint16_t)p_data[4]);
            num = p_data[2];
            if (p_data[1] == 'P') {
                sc_indication_scene_pos(id, val, num, pkPrimaryRail);
            }
            else if (p_data[1] == 'M') {
                sc_indication_scene_pos(id, val, num, pkSecondaryRail);
            }
            else if (p_data[1] == 'T') {
                sc_indication_scene_pos(id, val, num, pkVaneTilt);
            }
            else if (p_data[1] == 'E') {

                // The scene is not set in the shade, so send an error response
                sc_indication_scene_pos(id, val, num, pkError);
            }
            break;
        case 'F':
            if (p_data[1] == 'N') {
                sc_indication_nordic_fw(id, p_data+2);
            }
            else if (p_data[1] == 'C') {
                sc_indication_motor_fw(id, p_data+2);
            }
            break;
        case 'g':
            sc_indication_group(id, p_data+1);
            break;
        case 'X':
            sc_indication_debug_metrics(id, p_data+1);
            break;
        default:
            break;
    }
}

/*****************************************************************************//**
* @brief This function handles processing of a data packet that indicates shade type.
*
* @param id.  Device ID of shade that sent packet.
* @param p_data.  Pointer to the packet data.
* @return none.
* @author Neal Shurmantine
* @version
* 03/18/2015    Created.
*******************************************************************************/
static void sc_indication_shade_type(uint16_t id, uint8_t* p_data)
{
    printf("Shade Type: %02X\n", p_data[0]);
}

/*****************************************************************************//**
* @brief This function handles processing of a data packet that indicates
*  shade position.
*
* @param id.  Device ID of shade that sent packet.
* @param val.  Value of shade position.
* @param kind.  Which position kind is being reported
* @return none.
* @author Neal Shurmantine
* @version
* 03/18/2015    Created.
*******************************************************************************/
static void sc_indication_cur_pos(uint16_t id, uint16_t val, ePosKind kind)
{
    SC_Positions.positions.position[SC_Positions.positions.posCount] = val;
    SC_Positions.positions.posKind[SC_Positions.positions.posCount] = kind;
    SC_Positions.positions.posCount++;
    SC_Positions.device_id = id;

    if (SC_FinalPacketData == true) {
        IPC_Client_ReportShadePosition(&SC_Positions);

        /*
        printf("ID = %04x:\n",SC_Positions.device_id);
        for (int i = 0; i < SC_Positions.positions.posCount; ++i) {
            printf("  Kind = %d Value = %d\n",
                    SC_Positions.positions.posKind[i],
                    SC_Positions.positions.position[i]);
        }
        */
    }
}

/*****************************************************************************//**
* @brief This function handles processing of a data packet that indicates
*  position set by a shade for a particular scene.
*
* @param id.  Device ID of shade that sent packet.
* @param val.  Value of shade position.
* @param kind.  Which position kind is being reported
* @return none.
* @author Neal Shurmantine
* @version
* 03/18/2015    Created.
*******************************************************************************/
static void sc_indication_scene_pos(uint16_t id, uint16_t val, uint8_t scene_num, ePosKind kind)
{
    int i;
    SC_Positions.positions.position[SC_Positions.positions.posCount] = val;
    SC_Positions.positions.posKind[SC_Positions.positions.posCount] = kind;
    SC_Positions.positions.posCount++;
    SC_Positions.device_id = id;

    if (SC_FinalPacketData == true) {
        IPC_Client_ReportScenePosition(&SC_Positions,scene_num);
        printf("ID = %04x\n",SC_Positions.device_id);
        printf("Scene Num = %02x\n",scene_num);
        for (i = 0; i < SC_Positions.positions.posCount; ++i) {
            printf("  Kind = %d Value = %d\n",
            SC_Positions.positions.posKind[i],
            SC_Positions.positions.position[i]);
        }
    }
}

/*****************************************************************************//**
* @brief This function handles processing of a data packet that indicates
*  receiver firmware revision.
*
* @param id.  Device ID of shade that sent packet.
* @param p_data.  Pointer to the packet data.
* @return none.
* @author Neal Shurmantine
* @version
* 03/18/2015    Created.
*******************************************************************************/
static void sc_indication_nordic_fw(uint16_t id, uint8_t* p_data)
{
printf("ID=%04X\n",id);
    printf("Nordic FW Resp: %02X %02X %02X %02X\n", p_data[0], p_data[1], p_data[2], p_data[3]);
}

/*****************************************************************************//**
* @brief This function handles processing of a data packet that indicates
*  motor firmware revision.
*
* @param id.  Device ID of shade that sent packet.
* @param p_data.  Pointer to the packet data.
* @return none.
* @author Neal Shurmantine
* @version
* 03/18/2015    Created.
*******************************************************************************/
static void sc_indication_motor_fw(uint16_t id, uint8_t* p_data)
{
printf("ID=%04X\n",id);
    printf("Motor FW Resp:  %02X %02X %02X %02X\n", p_data[0], p_data[1], p_data[2], p_data[3]);
}

/*****************************************************************************//**
* @brief This function handles processing of a data packet that indicates
*  group assignment.
*
* @param id.  Device ID of shade that sent packet.
* @param p_data.  Pointer to the packet data.
* @return none.
* @author Neal Shurmantine
* @version
* 03/30/2015    Created.
*******************************************************************************/
static void sc_indication_group(uint16_t id, uint8_t* p_data)
{
    *p_data &= 0x80;
    uint8_t g = 0;
    uint8_t i, j, mask;
    for( i=0; i < 32; ++i, ++p_data ) {
        if (*p_data != 0) {
            for( mask=1, j = 0; mask; mask<<=1, ++j) {
                if (mask & *p_data) {
                    g += j;
                    break;
                }
            }
            break;
        }
        else {
            g += 8;
        }
    }
    printf("ID = %04X Group = %d\n",id, g);
}

/*****************************************************************************//**
* @brief This debug function handles processing of a data packet that indicates
*  shade metrics.
*
* @param id.  Device ID of shade that sent packet.
* @param p_data.  Pointer to the packet data.
* @return none.
* @author Neal Shurmantine
* @version
* 03/18/2015    Created.
*******************************************************************************/
static void sc_indication_debug_metrics(uint16_t id, uint8_t* p_data)
{
#ifdef DEBUG_PRINT
    uint16_t val;
    printf("\n\rMetrics:\n\r");
    printf("ID: %04x\n",id);
    val = (uint16_t)p_data[0] + (256 * (uint16_t)p_data[1]);
    printf("  Rcvd =%d\n",val);
    val = (uint16_t)p_data[2] + (256 * (uint16_t)p_data[3]);
    printf("  No Motor Ack =%d\n",val);
    val = (uint16_t)p_data[4] + (256 * (uint16_t)p_data[5]);
    printf("  CSMA_Attempts =%d\n",val);
    val = (uint16_t)p_data[6] + (256 * (uint16_t)p_data[7]);
    printf("  CSMA_Retries =%d\n",val);
    val = (uint16_t)p_data[8] + (256 * (uint16_t)p_data[9]);
    printf("  CSMA_Failures =%d\n",val);
#endif
}



//----------------------------SCENE EXECUTION-----------------------------------


/*****************************************************************************//**
* @brief This function starts the process of sending a message to execute a
*   scene.  A callback is registered to be executed after the message has been
*   sent.  The number of scenes in the payload and the list of scenes is saved
*   for the retry round that is subsequently sent with an identical sequence
*   number.
*
* @param scene_count.  The number of scenes in the payload.
* @param scene_id.  A pointer to list of scene IDs to be included in the payload.
* @return nothing.
* @author Neal Shurmantine
* @version
* 04/01/2015    Created.
*******************************************************************************/
void SC_ExecuteScene(uint8_t scene_count, uint8_t * scene_id)
{
    SC_ExecuteSceneProc(scene_count, scene_id, NULL);
    SC_ExecuteSceneProc(scene_count, scene_id, NULL);
}


//----------------------------NETWORK JOIN PROCESS-------------------------------


/*****************************************************************************//**
* @brief This function is called when it is required to put the Hub into a mode<br/>
*  where it waits for a beacon message from the remote in order to join the<br/>
*  network ID of the remote.
*
* <b>Note:</b>  This function runs in the context of the calling task.
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/06/2014    Created.
*******************************************************************************/
void SC_EnableNetworkJoin(void)
{
    RC_RestoreRadioDefault();
    SC_JoinEnabled = true;

    LED_Flicker(true);
    SC_NetworkJoinScheduleToken = SCH_ScheduleEventPostSeconds(12,SC_DisableNetworkJoin);
}

/*****************************************************************************//**
* @brief This function is called when it is required to end the mode mode where<br/>
*  the hub waits for a beacon message from the remote
*
* <b>Note:</b>  This function runs in the context of the calling task.
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/06/2014    Created.
*******************************************************************************/
void SC_DisableNetworkJoin(uint16_t token)
{
    if (SC_NetworkJoinScheduleToken != token) {
        SCH_RemoveScheduledEvent(SC_NetworkJoinScheduleToken);
    }
    LED_NetworkID(RC_IsNetworkIdAssigned());
    SC_JoinEnabled = false;
}

/*****************************************************************************//**
* @brief This function is called to determine if hub is still actively attempting
*   to join to a remote.
*
* <b>Note:</b>  This function runs in the context of the calling task.
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/06/2014    Created.
*******************************************************************************/
bool SC_IsNetworkJoiningActive(void)
{
    return SC_JoinEnabled;
}


//-------------------------BATTERY CHECK PROCESSING------------------------------


/*****************************************************************************//**
* @brief This function handles processing of a data packet that indicates
*  battery level.
*
* @param id.  Device ID of shade that sent packet.
* @param p_data.  Pointer to the packet data.
* @return none.
* @author Neal Shurmantine
* @version
* 03/18/2015    Created.
*******************************************************************************/
static void sc_indication_batt_level(uint16_t id, uint8_t* p_data)
{
    if (SC_SingleShadeBatteryCheck == true) {
#ifdef USE_ME
        IPC_Client_UpdateBatteryStatus(id, *p_data);
#else
        char lvl_str[7];
        COARSE_BATTERY_LEVEL lvl = IPC_Client_UpdateBatteryStatus(id, *p_data);
        if (lvl == RED) {
            strcpy(lvl_str,"RED");
        }
        else if (lvl == YELLOW) {
            strcpy(lvl_str,"YELLOW");
        }
        else if (lvl == GREEN) {
            strcpy(lvl_str,"GREEN");
        }
        else {
            strcpy(lvl_str,"NONE");
        }
        printf("Level = %s\n",lvl_str);
#endif
    }
    else {
        uint16_t n;
        for (n=0; n<MaxShades; ++n) {
            if (p_BattCheckData[n].shade_id == id) {
                p_BattCheckData[n].replicate_level[p_BattCheckData[n].replicate_counter] = *p_data;
                ++p_BattCheckData[n].replicate_counter;
                break;
            }
        }
    }
}

/*****************************************************************************//**
* @brief Analyze data from periodic battery check.  Up to 7 attempts are made to
*    determine the battery level of each shade.  If the shade has responded 5 times
*    then the highest measured value is determined from the 5 readings.  This value
*    is considered the true value and is used to determine if the battery
*    is low.
*
* @param nothing.
* @return none.
* @author Neal Shurmantine
* @version
* 09/19/2016    Created.
*******************************************************************************/
static void SC_store_weekly_battery_levels(void)
{
    uint16_t n;
    uint16_t j;
    uint8_t max_level;
    char lvl_str[7];
    for (n=0; n < MaxShades; ++n) {
        max_level = 0;
        if (p_BattCheckData[n].replicate_counter >= MAX_BATTERY_MEASUREMENTS_PER_SHADE) {
            for (j=0; j < MAX_BATTERY_MEASUREMENTS_PER_SHADE; ++j) {
                if (max_level < p_BattCheckData[n].replicate_level[j]) {
                    max_level = p_BattCheckData[n].replicate_level[j];
                }
            }
        }
        p_BattCheckData[n].coarse_lvl = IPC_Client_UpdateBatteryStatus(p_BattCheckData[n].shade_id,max_level);
        if (p_BattCheckData[n].coarse_lvl == RED) {
            SC_LowBatteryCount++;
            strcpy(lvl_str,"RED");
        }
        else if (p_BattCheckData[n].coarse_lvl == YELLOW) {
            SC_LowBatteryCount++;
            strcpy(lvl_str,"YELLOW");
        }
        else if (p_BattCheckData[n].coarse_lvl == GREEN) {
            strcpy(lvl_str,"GREEN");
        }
        else {
            strcpy(lvl_str,"NONE");
        }
        printf("ID = %04x, voltage = %d coarse_lvl=%s\n", p_BattCheckData[n].shade_id, max_level, lvl_str);
        if (flashDataNeedsWritten == true) {
            writeDataBufferToCurrentSector();
            SC_MaintainFlash = true;
        }
        if (flashVectorNeedsWritten == true) {
            writeVectorBufferToCurrentSector();
            SC_MaintainFlash = true;
        }
    }
}

/*****************************************************************************//**
* @brief Determine, based on voltage, what the coarse battery level is.
*
* @param shade_type.  (Power tilt shades are powered with a 12 volt pack)
* @param voltage.  byte value representing the voltage in 100 millivolt units
* @return COARSE_BATTERY_LEVEL. RED for low, YELLOW for marginal, GREEN for good
* @author Neal Shurmantine
* @version
* 09/08/2015    Created.
*******************************************************************************/
COARSE_BATTERY_LEVEL SC_GetCoarseBatteryLevel(uint8_t shade_type, uint8_t voltage)
{
    COARSE_BATTERY_LEVEL lvl;
    uint8_t low_voltage;
    uint8_t moderate_voltage;
    switch (shade_type) {
        case 18: //Pirouette
        case 39: //Parkland
        case 40: //Everwood
        case 41: //Modern Precious Metals
        case 244: //power tilt generic
            low_voltage = SC_LOW_BATTERY_LEVEL_POWER_TILT;
            moderate_voltage = SC_MODERATE_BATTERY_LEVEL_POWER_TILT;
            break;
        default:
            low_voltage = SC_LOW_BATTERY_LEVEL;
            moderate_voltage = SC_MODERATE_BATTERY_LEVEL;
            break;
    }

    if (voltage <= low_voltage) {
        lvl = RED;
    }
    else if ( (voltage > low_voltage) && ( voltage <= moderate_voltage) ) {
        lvl = YELLOW;
    }
    else {
        lvl = GREEN;
    }
    return lvl;
}

/*****************************************************************************//**
* @brief Initialize the daily battery checking.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 03/09/2015    Created.
*******************************************************************************/
void SC_ScheduleBatteryCheck(void)
{
    DAY_STRUCT day;
    uint16_t frac = (uint16_t)RC_GetNordicUuid();
    day.hour = SC_BATTERY_CHECK_HOUR;
    day.minute = SC_BATTERY_CHECK_MINUTE + (frac % 60);
    frac>>=8;
    day.second = frac % 60;
    if (SC_BatteryCheckToken != NULL_TOKEN) {
        SCH_RemoveScheduledEvent(SC_BatteryCheckToken);
    }
    SC_BatteryCheckToken = SCH_ScheduleDaily(&day, SC_BeginBatteryCheckProcess);
}

/*****************************************************************************//**
* @brief Allows an immediate check of all shade batteries rather than waiting
*      until weekly scheduled time.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 09/09/2015    Created.
*******************************************************************************/
void SC_DebugForceBatteryCheck(void)
{
    time_t now_time;
    struct tm now_date;
    DAY_STRUCT day;
    OS_GetTimeLocal(&now_time);
    now_time += 5;
    localtime_r(&now_time, &now_date);

    day.hour = now_date.tm_hour;
    day.minute = now_date.tm_min;
    day.second = now_date.tm_sec;
    SC_IsForceBatteryCheck = true;
    if (SC_BatteryCheckToken != NULL_TOKEN) {
        SCH_RemoveScheduledEvent(SC_BatteryCheckToken);
    }
    SC_BatteryCheckToken = SCH_ScheduleDaily(&day, SC_BeginBatteryCheckProcess);
}

/*****************************************************************************//**
* @brief This function is called from the scheduler and begins the daily process
*        of checking the battery level of each shade. This function is called
*        first and collects the ID of each shade.  It schedules itself to be
*        called the next day.
*
* @param token. Unused but required as part of the scheduler callback.
* @return none.
* @author Neal Shurmantine
* @version
* 03/09/2015    Created.
*******************************************************************************/
void SC_BeginBatteryCheckProcess(uint16_t token)
{
    time_t now;
    struct tm date;
    OS_GetTimeLocal(&now);
    localtime_r(&now, &date);
    if ((date.tm_wday == 0) || (SC_IsForceBatteryCheck == true)) {  //Sunday=0
        SC_IsForceBatteryCheck = false;
        SC_LowBatteryCount = 0;
        //get total number of shades and reserve memory
        MaxShades = SI_GetShadeCount();
        if (MaxShades) {
            p_BattCheckData = (BATT_CHECK_STRUCT_PTR)OS_GetMemBlock(MaxShades*sizeof(BATT_CHECK_STRUCT));

            //fill out list of BATT_CHECK_STRUCT with all non-plugged shade data
            MaxShades = SI_GetShadeBattData(p_BattCheckData);
            if (MaxShades) {
                LOG_LogEvent("Check Shade Batteries");
                ShadeIndex = 0;
                BattCheckRetryCount = 0;
                SC_MaintainFlash = false;
                SC_batt_check_proc_continue(0);
            }
            else {
                OS_ReleaseMemBlock((void *)p_BattCheckData);
                LOG_LogEvent("No Batt Powered Shades");
            }
        }
    }
    else {
        LOG_LogEvent("Skip Battery Check");
    }

    SC_ScheduleBatteryCheck();
}

/*****************************************************************************//**
* @brief This function is called from the scheduler as part of the battery checking
*        process. Each shade battery request is scheduled with a 4 second interval
*        between messages.  The call is made up to 7 times (BATTERY_CHECK_RETRY_MAX)
*        for each shade or until the shade responds with its battery level 
*        5 times (MAX_BATTERY_MEASUREMENTS_PER_SHADE).
*
* @param token. Unused but required as part of the scheduler callback.
* @return none.
* @author Neal Shurmantine
* @version
* 03/09/2015    Created.
*******************************************************************************/
static void SC_batt_check_proc_continue(uint16_t token)
{
    P3_Address_Internal_Type address;

    while(ShadeIndex < MaxShades) {
        if ((p_BattCheckData[ShadeIndex].replicate_counter < MAX_BATTERY_MEASUREMENTS_PER_SHADE) && 
                (BattCheckRetryCount < BATTERY_CHECK_RETRY_MAX)) {
            break;
    }
        ++ShadeIndex;
        BattCheckRetryCount = 0;
            }
    if (ShadeIndex < MaxShades) {
            ++BattCheckRetryCount;
        address.Unique_Id = 0;
        address.Device_Id = p_BattCheckData[ShadeIndex].shade_id;
        SC_RequestBatteryLevel(P3_Address_Mode_Device_Id, &address, NULL);
        SCH_ScheduleEventPostSeconds(SECONDS_BETWEEN_BATTERY_CHECKS, SC_batt_check_proc_continue);
        }
        else {
        SC_store_weekly_battery_levels();
        MaxShades = 0;
        OS_ReleaseMemBlock((void *)p_BattCheckData);
        if (SC_MaintainFlash == true) {
            maintainFlash(1);
        }
        RDS_TriggerRemoteSync(NULL_TOKEN);
        if (SC_LowBatteryCount != 0) {
            RMT_FaultNotification(0);
        }
    }
}

/*****************************************************************************//**
* @brief Returns the number of shades with low batteries from the last scheduled
*      measurement cycle.  Note that this must be called soon after the batteries
*      were read, before the hub has had a chance to reset, since it uses the
*      value in RAM.
*
* @param p_unused.  Unused parameter, required for this type of callback.
* @return uint16_t, number of shades with low battery.
* @author Neal Shurmantine
* @version
* 09/11/2015    Created.
*******************************************************************************/
uint16_t SC_GetLowBatteryCount(void)
{
    return SC_LowBatteryCount;
}

/*****************************************************************************//**
* @brief This callback function is called if a single battery measurement
*    has been requested by the app, the RF message was sent and enough time
*    has elapsed for the shade to respond if it is going to.
*
* @param p_unused.  Unused parameter, required for this type of callback.
* @return nothing.
* @author Neal Shurmantine
* @version
* 09/08/2015    Created.
*******************************************************************************/
static void sc_battery_measurement_callback(void *p_unused)
{
    SC_SingleShadeBatteryCheck = false;
}

//----------------------------DISCOVERY PROCESSING----------------------------------

// marker 06/30/2015 - study
/*****************************************************************************//**
* @brief This function is called to start the discovery process.
*
* <b>Note:</b>  This function runs in the context of the calling task.
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/06/2014    Created.
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |     0x43      |     0x46      |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*******************************************************************************/
void SC_DiscoverShades(bool is_absolute,uint16_t type)
{
    SC_AvoidContinueDiscovery = false;
    SC_DiscoveryType = (eDiscoveryType)type;
    SKIP_IF_NO_NETWORK;
    SC_AbsoluteDiscoveryActive = is_absolute;
    SC_DiscoverRetryCount = SC_DISCOVERY_RETRY_NO_RESP;
    LED_Flicker(true);
    SCH_ScheduleEventPostSeconds(SC_DISCOVERY_TIMEOUT, SC_discovery_timeout);
    sc_continue_discovery();
}

/*****************************************************************************//**
* @brief This function is called to continue the discovery process.
*
* <b>Note:</b>  This function runs in the context of the calling task.
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/06/2014    Created.
*******************************************************************************/
static void sc_continue_discovery(void)
{
    SC_DiscoverHead = NULL;
    SC_DiscoverTail = NULL;
    SC_DiscoveryActive = true;
    SC_CapturingShades = false;
    if (SC_AbsoluteDiscoveryActive == true) {
        SC_AbsoluteDiscovery();
    }
    else {
        SC_IssueBeacon();
    }
}

/*****************************************************************************//**
* @brief This function is used to determine if discovery is occurring.
*
* <b>Note:</b>  This function runs in the context of the calling task.
* @param none.
* @return boolean, true if discovery is occurring.
* @author Neal Shurmantine
* @version
* 11/06/2014    Created.
*******************************************************************************/
bool SC_IsDiscoveryActive(void)
{
    return SC_DiscoveryActive;
}

/*****************************************************************************//**
* @brief This function processes a received beacon.
*
* @param p_rf_response is a data structure of type PARSE_KEY_STRUCT_PTR.
* @return none.
* @author Neal Shurmantine
* @version
* 11/25/2014    Created.
*******************************************************************************/
void sc_beacon_indication_received(PARSE_KEY_STRUCT_PTR p_rf_response)
{
    uint8_t dev_type;
    if ((SC_JoinEnabled == true) && (p_rf_response->beacon_ind.source_network_id != ALL_NETWORK_ID)
                && (p_rf_response->beacon_ind.source_network_id != FACTORY_DEFAULT_NETWORK_ID)) {
        SC_DisableNetworkJoin(0);
        sc_cancel_configurations();
        RC_AssignNewNetworkId(p_rf_response->beacon_ind.source_network_id);
    }
    else if (SC_DiscoveryActive == true) {
        if ((p_rf_response->beacon_ind.source_device_id != 0)  //ignore echoed beacon from hub
            && (p_rf_response->beacon_ind.payload_len == (BEACON_INDICATION_HEADER_SIZE + 1)) ) {
            if (SC_is_redundant_discovery_response(p_rf_response->beacon_ind.source_device_id ) == false) {
                printf("NID=%04X ID=%04X TYPE=%d\n", p_rf_response->beacon_ind.source_network_id,
                                    p_rf_response->beacon_ind.source_device_id,
                                    p_rf_response->beacon_ind.msg_payload[0]);
                dev_type = p_rf_response->beacon_ind.msg_payload[0];
                if ((SC_DiscoveryType == dtAny)
                            || ((SC_DiscoveryType == dtSceneController) && (dev_type == SC_SCENE_CONTROLLER_TYPE))
                            || ((SC_DiscoveryType == dtShades) && (dev_type != SC_SCENE_CONTROLLER_TYPE) )) {
                    SC_create_discover_record(p_rf_response);
                }
            }
        }
    }
//#ifdef DEBUG_PRINT
//    if (RC_GetNetworkId() == p_rf_response->beacon_ind.source_network_id) {
//        printf("NID=%04X ID=%04X\n", p_rf_response->beacon_ind.source_network_id,
//            p_rf_response->beacon_ind.source_device_id);
//    }
//#endif
}

/*****************************************************************************//**
* @brief Scan through the list of shade discovery responses to see if this
*       shade ID is alreay present.
*
* @param id. Id of shade to check
* @return none.
* @author Neal Shurmantine
* @version
* 08/19/2015    Created.
*******************************************************************************/
static bool SC_is_redundant_discovery_response(uint16_t id)
{
    bool redund = false;
    SC_SHADE_DISC_REC_PTR p_cfg_rec = SC_DiscoverHead;

    while(p_cfg_rec != NULL) {
        if (p_cfg_rec->shade_rsp.device_id == id) {
            redund = true;
            break;
        }
        p_cfg_rec = p_cfg_rec->p_next_rec;
    }
    return redund;
}

/*****************************************************************************//**
* @brief This callback function executes after a message has been sent to set
*     the discovered flag of a shade.  If all messages on the list have been
*     sent then determines if the discovery process is done.
*
* @param p_unused. Parameter required by callback function
* @return none.
* @author Neal Shurmantine
* @version
* 11/06/2014    Created(date approximate).
*******************************************************************************/
static void SC_discovery_callback(void * p_unused)
{
    if ((SC_HeadAddress == NULL)
            && (SC_TailAddress == NULL)
            &&  (SC_DiscoveryActive == true)
            &&  (SC_CapturingShades == true) )
    {
        if (SC_AbsoluteDiscoveryActive || SC_AvoidContinueDiscovery) {
            SC_AbsoluteDiscoveryActive = false;
            SC_DiscoveryActive = false;
            SC_CapturingShades = false;
            LED_Flicker(false);
        }
        else {
            SCH_ScheduleEventPostSeconds(SC_DISCOVERY_TIMEOUT, SC_discovery_timeout);
            sc_continue_discovery();
        }
     }
}

/*****************************************************************************//**
* @brief This function allocates memory and adds an entry on the linked list of
*     devices responding to discovery.
*
* @param p_rf_response. Raw serial message data from Nordic
* @return SC_SHADE_DISC_REC_PTR.  Pointer to this record.
* @author Neal Shurmantine
* @version
* 11/06/2014    Created(date approximate).
*******************************************************************************/
static SC_SHADE_DISC_REC_PTR SC_create_discover_record(PARSE_KEY_STRUCT_PTR p_rf_response)
{
    SC_SHADE_DISC_REC_PTR p_cfg_rec;
    SC_SHADE_DISC_REC_PTR p_last_rec;
    uint8_t type;

    p_cfg_rec = (SC_SHADE_DISC_REC_PTR)OS_GetMemBlock(sizeof(SC_SHADE_DISC_REC));

    //if no records in the list
    if (SC_DiscoverHead == NULL)
    {
        //create list with this record only
        SC_DiscoverHead = p_cfg_rec;
        SC_DiscoverTail = p_cfg_rec;
    }
    else
    {
        //add this record to the end of the list
        p_last_rec = SC_DiscoverTail;
        p_last_rec->p_next_rec = p_cfg_rec;
        SC_DiscoverTail = p_cfg_rec;
    }
    p_cfg_rec->p_next_rec = NULL;
    p_cfg_rec->shade_rsp.device_id = p_rf_response->beacon_ind.source_device_id;
    p_cfg_rec->shade_rsp.uuid = p_rf_response->beacon_ind.source_adr.Unique_Id;
    p_cfg_rec->shade_rsp.network_id = p_rf_response->beacon_ind.source_network_id;

    type = p_rf_response->beacon_ind.msg_payload[0];
#ifdef SPOOF_SHADE_TYPES
    switch (p_cfg_rec->shade_rsp.device_id) {
        case 0x4873:
            type = 14;
            break;
        case 0x658A:
            type = 20;
            break;
        case 0x3C75:
            type = 26;
            break;
        case 0x34C5:
            type = 4;
            break;
        case 0x3519:
            type = 4;
            break;
        case 0x7D7B:
            type = 4;
            break;
        default:
            break;
    }
#endif

    p_cfg_rec->shade_rsp.shade_type = type;

    SC_AvoidContinueDiscovery = (type == SC_SCENE_CONTROLLER_TYPE);

    return p_cfg_rec;
}

/*****************************************************************************//**
* @brief This is a callback function from the scheduler that executes after a
*     beacon message has been sent for discovery and enough time has elapsed
*     for all shades to send their responses.
* Note: this function is called in the context of the SCH_ScheduleTask.
*
* @param token. Unused, required for callback from scheduler
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/06/2014    Created(date approximate).
*******************************************************************************/
void SC_discovery_timeout(uint16_t token)
{
    if (SC_DiscoverHead == NULL) {
        if (SC_AbsoluteDiscoveryActive || SC_AvoidContinueDiscovery) {
            SC_AbsoluteDiscoveryActive = false;
            SC_DiscoveryActive = false;
            SC_CapturingShades = false;
            LED_Flicker(false);
        }
        else {
            --SC_DiscoverRetryCount;
            if (SC_DiscoverRetryCount == 0) {
                SC_DiscoveryActive = false;
                LED_Flicker(false);
            }
            else {
                SCH_ScheduleEventPostSeconds(SC_DISCOVERY_TIMEOUT, SC_discovery_timeout);
                sc_continue_discovery();
            }
        }
    }
    else {
        SC_DiscoverRetryCount = SC_DISCOVERY_RETRY_NO_RESP;
        SC_CapturingShades = true;
        RNC_BeginDiscoveryProcessing();
    }
}

/*****************************************************************************//**
* @brief This function goes through the linked list of devices that responed to
*   the discovery beacon and queues up a set-discovered flag message to be
*   sent to each.  The HTTP task is also notified of a new responder.
*
*   Note: This function is called in the context of the RNC task.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/06/2014    Created(date approximate).
*******************************************************************************/
void SC_ProcessDiscoveredList(void)
{
    P3_Address_Internal_Type address;

    while(SC_DiscoverHead != NULL) {
        DISCOVERY_DATA_STRUCT_PTR p_disc_data = &SC_DiscoverHead->shade_rsp;
        address.Unique_Id = 0;
        address.Device_Id = p_disc_data->device_id;
        SC_SetDiscoveredFlagProc(P3_Address_Mode_Device_Id, &address);
        IPC_Client_RecordDiscovery(p_disc_data);
        SC_remove_discovered_shade_from_list();
    }
}

/*****************************************************************************//**
* @brief Remove an entry from the linked list of devices that responded to
*    the beacon discovery message and free memory.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/06/2014    Created(date approximate).
*******************************************************************************/
static void SC_remove_discovered_shade_from_list(void)
{
    SC_SHADE_DISC_REC_PTR p_cfg_rec;
    p_cfg_rec = SC_DiscoverHead;
    SC_DiscoverHead = p_cfg_rec->p_next_rec;
    OS_ReleaseMemBlock((void *)p_cfg_rec);
}


//----------------------------SCENE CONTROLLER-----------------------------------


/*****************************************************************************//**
* @brief This function parses a single data packet in the payload from a scene
*  controller and calls the relavant function to handle the message.
*
* @param id.  Device ID of scene controller that sent packet.
* @param p_data.  Pointer to the packet data.
* @return none.
* @author Neal Shurmantine
* @version
* 06/09/2015    Created.
*******************************************************************************/
static void sc_parse_scene_controller_payload(uint16_t id, uint8_t *p_data)
{
    uint16_t val;
    switch (p_data[0]) {
        case 'X':
            IPC_Client_SceneControllerClearedAnnouncement(id);
            break;
        case 'U' :
            IPC_Client_SceneControllerDatabaseUpdateRequest(id, p_data[1]);
            break;
        case 'u' :
            IPC_Client_SceneControllerUpdatePacketRequest(id, p_data[1], p_data[2]);
            break;
        case 't' :
            val = (uint16_t)p_data[2] + (256 * (uint16_t)p_data[3]);
            IPC_Client_SceneControllerTrigger(id, p_data[1], val, p_data[4]);
            break;
    }
}

//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>


static void sc_group_set_indication_received(PARSE_KEY_STRUCT_PTR p_rf_response)
{
}

/*****************************************************************************//**
* @brief This function is called after a timeout expires following the transmission
*   of a message to a shade.  If a callback has been assigned for this message
*   then it is executed.  If more messages are queued up then the next one is
*   sent.
*
* @param none.
* @return none.
* @author Neal Shurmantine
* @version
* 11/06/2014    Created.
*******************************************************************************/
void SC_ProcessRFTimeout(void)
{
    RNC_CONFIG_REC * p_msg_list;

    p_msg_list = SC_HeadAddress;
    while (p_msg_list != NULL)
    {
        if (p_msg_list->state == WAITING_TO_SEND_NEXT)
        {
            //if wait has expired then
            --p_msg_list->rf_ack_tick_count;
            if (p_msg_list->rf_ack_tick_count == 0)
            {
                if (SC_DiscoveryActive == false) {
                    LED_Flicker(false);
                }
                SC_clear_item_in_list(p_msg_list);
                if (p_msg_list->p_callback != NULL) {
                    (*p_msg_list->p_callback)(NULL);
                }
                SC_GetNextMessageToSend();
            }
            //exit loop, only one message will have this state
            p_msg_list = NULL;
        }
        else {
            p_msg_list = p_msg_list->p_next_rec;
        }
    }
}

//----------------------------RESET ALL SHADES ----------------------------------


/*****************************************************************************//**
* @brief This is a callback function that is used after all shades are reset
*   and it starts the reset of the network ID in the Hub.
*
* @param p_unused.  Variable not used, required for callback function
* @return none.
* @author Neal Shurmantine
* @version
* May 2015    Created.
*******************************************************************************/
void SC_reset_all_shades_return(void * p_unused)
{
    RC_RestoreRadioDefault();
}

/*****************************************************************************//**
* @brief This function creates the message to reset the shades when all
*   shades are to be reset.
*
* @param p_callback.  Pointer to callback function after a message is sent, either
*     set to NULL or pointer to function (for last message in list).
* @return none.
* @author Neal Shurmantine
* @version
* May 2015    Created.
*******************************************************************************/
void SC_reset_all_shades(void(*p_callback)(void*))
{
    uint16_t cfg = SR_CLEAR_DISCOVERED_FLAG | SR_DEL_GROUP_7_TO_255 | SR_DELETE_SCENES;
    SHADE_COMMAND_INSTRUCTION_PTR p_cmd = (SHADE_COMMAND_INSTRUCTION_PTR)OS_GetMsgMemBlock(sizeof (SHADE_COMMAND_INSTRUCTION));
    p_cmd->cmd_type = SC_RESET_SHADE;
    p_cmd->adr_mode = (uint8_t)P3_Address_Mode_Device_Id;
    p_cmd->address.Unique_Id = 0;
    p_cmd->address.Device_Id = ALL_DEVICES_ADDRESS;
    p_cmd->data.reset_data = cfg;
    p_cmd->p_callback = p_callback;
    RNC_SendShadeRequest(p_cmd);
}

/*****************************************************************************//**
* @brief This function starts the process of sending 3 messages to shades on the
*    "ALL" for the shades to reset their discovered flag and clear scenes and
*    groups.  After the third message is completed, the nordic network ID is
*    restored.
*
* @param none.
* @return none.
* @author Neal Shurmantine
* @version
* May 2015    Created.
*******************************************************************************/
bool SC_ResetAllShades(void)
{
    bool rslt = false;
    if (RC_IsNetworkIdAssigned() == true) {
        rslt = true;
        SC_reset_all_shades(NULL);
        SC_reset_all_shades(NULL);
        SC_reset_all_shades(SC_reset_all_shades_return);
    }
    return rslt;
}

uint16_t TestShadeIDs[] = {
    0x4873,
    0x658A,
    0x3C75,
    0x34C5,
    0x3519,
    0x7D7B
};
const uint16_t MAX_TEST_SHADES = sizeof(TestShadeIDs)/sizeof(uint16_t);

//const uint8_t MULTI_PACKET[] = {'?', 'Z', 2, '?', 'P', 2, '?', 'M', 2, '?', 'T', 2, '?', 'B' };
const uint8_t MULTI_PACKET[] = {'?', 'Z', 2, '?', 'B' };

void SC_Test_DeleteAllShades(void)
{
    uint8_t len;
    uint8_t msg[16];
    int n;
    P3_Address_Internal_Type address;
    strPositions pos;
    pos.posCount = 1;
    pos.posKind[0] = pkPrimaryRail;
    len = sizeof(MULTI_PACKET);
    memcpy(msg, MULTI_PACKET,len);

    address.Unique_Id = 0;
    printf("Set Scene to position 0x4000\n");
    for (n = 0; n < MAX_TEST_SHADES; ++n) {
        address.Device_Id = TestShadeIDs[n];
        pos.position[0] = 0x4000;
        SC_SetSceneAtPosition(P3_Address_Mode_Device_Id,&address,n,&pos);
        OS_TaskSleep(2000);
    }
    OS_TaskSleep(5000);

    printf("Read Battery\n");
    for (n = 0; n < MAX_TEST_SHADES; ++n) {
        address.Device_Id = TestShadeIDs[n];
        SC_RequestBatteryLevel(P3_Address_Mode_Device_Id,&address,NULL);
//        sc_send_raw_payload(P3_Address_Mode_Device_Id,&address,len,msg);
        OS_TaskSleep(2000);
    }
//    OS_TaskSleep(2000);
}

uint8_t GROUP_TEST_1[] = {
0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t GROUP_TEST_2[] = {
0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t GROUP_TEST_3[] = {
0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t GROUP_TEST_4[] = {
0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
uint8_t GROUP_TEST_5[] = {
0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80 };
uint8_t GROUP_TEST_6[] = {
0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
//request shade position and battery voltage
//const uint8_t MULTI_PACKET[] = {'?', 'v' };
const uint8_t INFORCE_PACKET[] = { '?', '?' };
const uint8_t REQUEST_GROUP[] = { '?', 'g' };

#define SHADE_ID        0x7977
#define TEST_SHADE_MOVE
#define TEST_SHADE_POSITION
#define TEST_READ_FW
#define TEST_MULTI_DATA_PACKET
#define TEST_BATTERY_REQUEST
#define TEST_SCENE
#define TEST_REQUEST_SHADE_TYPE

void SC_Test(void)
{
    uint8_t len;
    uint8_t msg[10];

    uint8_t id_list[8];
    P3_Address_Internal_Type address;
    strPositions pos;
    pos.posCount = 1;
    pos.posKind[0] = pkPrimaryRail;
    address.Unique_Id = 0;
    address.Device_Id = SHADE_ID;

    len = sizeof(REQUEST_GROUP);
    memcpy(msg, REQUEST_GROUP,len);

//sc_indication_group(7, GROUP_TEST_1);
//sc_indication_group(71, GROUP_TEST_2);
//sc_indication_group(64, GROUP_TEST_3);
//sc_indication_group(248, GROUP_TEST_4);
//sc_indication_group(255, GROUP_TEST_5);
//sc_indication_group(0, GROUP_TEST_6);
//    sc_send_raw_payload(P3_Address_Mode_Device_Id,&address,len,msg);

    SC_JogShade(P3_Address_Mode_Device_Id,&address );
    SC_JogShade(P3_Address_Mode_Device_Id,&address );
    SC_JogShade(P3_Address_Mode_Device_Id,&address );
    SC_JogShade(P3_Address_Mode_Device_Id,&address );
    SC_JogShade(P3_Address_Mode_Device_Id,&address );
    SC_JogShade(P3_Address_Mode_Device_Id,&address );
    SC_JogShade(P3_Address_Mode_Device_Id,&address );
    SC_JogShade(P3_Address_Mode_Device_Id,&address );
    SC_JogShade(P3_Address_Mode_Device_Id,&address );
    SC_JogShade(P3_Address_Mode_Device_Id,&address );
    SC_JogShade(P3_Address_Mode_Device_Id,&address );
    SC_JogShade(P3_Address_Mode_Device_Id,&address );
    SC_JogShade(P3_Address_Mode_Device_Id,&address );
    SC_JogShade(P3_Address_Mode_Device_Id,&address );
    SC_JogShade(P3_Address_Mode_Device_Id,&address );
    SC_JogShade(P3_Address_Mode_Device_Id,&address );

    if (len < 12)
        return;

    printf("\n");
    printf("Sending Beacon\n");
    SC_IssueBeacon();
    OS_TaskSleep(2000);

    printf("Absolute Discovery\n");
    SC_AbsoluteDiscovery();
    OS_TaskSleep(2000);

    printf("Jog Shade\n");
    SC_JogShade(P3_Address_Mode_Device_Id,&address );
    OS_TaskSleep(2000);

    printf("Assign Group 8\n");
    SC_GroupAssign(P3_Address_Mode_Device_Id,&address,8,true);
    OS_TaskSleep(2000);

    printf("Set Primary Rail of Single Shade to 0xffff\n");
    pos.position[0] = 0xffff;
    SC_SetShadePosition(P3_Address_Mode_Device_Id,&address,&pos);
    OS_TaskSleep(5000);

    printf("Reading Shade Position\n");
    SC_GetShadePosition(P3_Address_Mode_Device_Id,&address);
    OS_TaskSleep(1000);

    printf("Setting Primary Rail of Group to 0x8000\n");
    address.Group_Id[0] = 8;
    address.Group_Id[1] = 0;
    pos.position[0] = 0x8000;
    SC_SetShadePosition(P3_Address_Mode_Group_Id,&address,&pos);
    OS_TaskSleep(5000);

    printf("Reading Shade Position as Group\n");
    SC_GetShadePosition(P3_Address_Mode_Group_Id,&address);
    OS_TaskSleep(1000);

    printf("Set Primary Rail to 0x0000\n");
    address.Device_Id = SHADE_ID;
    pos.position[0] = 0x0000;
    SC_SetShadePosition(P3_Address_Mode_Device_Id,&address,&pos);
    OS_TaskSleep(5000);

    printf("Reading Motor Firmware\n");
    SC_RequestMotorFW(P3_Address_Mode_Device_Id,&address);
    OS_TaskSleep(1000);

    printf("Reading Radio Firmware\n");
    SC_RequestReceiverFW(P3_Address_Mode_Device_Id,&address);
    OS_TaskSleep(1000);

    printf("Request Battery Voltage\n");
    SC_RequestBatteryLevel(P3_Address_Mode_Device_Id,&address,NULL);
    OS_TaskSleep(1000);

    printf("Requesting Shade Type\n");
    SC_RequestShadeType(P3_Address_Mode_Device_Id,&address);
    OS_TaskSleep(5000);

    printf("Setting Primary Rail to 0x8000\n");
    pos.position[0] = 0x8000;
    SC_SetShadePosition(P3_Address_Mode_Device_Id,&address,&pos);
    OS_TaskSleep(5000);

    printf("Setting Scene 1 to Current\n");
    SC_SetSceneToCurrent(P3_Address_Mode_Device_Id,&address,1);
    OS_TaskSleep(5000);

    printf("Set Scene 2 to position 0x4000\n");
    pos.position[0] = 0x4000;
    SC_SetSceneAtPosition(P3_Address_Mode_Device_Id,&address,2,&pos);
    OS_TaskSleep(5000);

    printf("Execute Scene 2\n");
    id_list[0] = 2;
    SC_ExecuteScene(1,id_list);
    OS_TaskSleep(5000);

    printf("Execute Scene 1\n");
    id_list[0] = 2;
    SC_ExecuteScene(1,id_list);
    OS_TaskSleep(5000);

    printf("Request Shade Scene Position 1\n");
    SC_RequestScenePosition(P3_Address_Mode_Device_Id,&address,1);
    OS_TaskSleep(2000);

    printf("Request Shade Scene Position 2\n");
    SC_RequestScenePosition(P3_Address_Mode_Device_Id,&address,2);
    OS_TaskSleep(2000);

    printf("Delete Scene 2\n");
    SC_DeleteScene(P3_Address_Mode_Device_Id,&address,2);
    OS_TaskSleep(2000);

    printf("Execute Scene 2\n");
    id_list[0] = 2;
    SC_ExecuteScene(1,id_list);
    OS_TaskSleep(5000);

    printf("Request Shade Scene Position 2\n");
    SC_RequestScenePosition(P3_Address_Mode_Device_Id,&address,2);
    OS_TaskSleep(2000);

    printf("\nGroup Move Shade Up\n");
    address.Group_Id[0] = 8;
    address.Group_Id[1] = 0;
    SC_MoveShade(P3_Address_Mode_Group_Id,&address,DIRECTION_UP);
    OS_TaskSleep(4000);

    printf("\nGroup Stop\n");
    SC_MoveShade(P3_Address_Mode_Group_Id,&address,DIRECTION_STOP);
    OS_TaskSleep(2000);

    printf("\nGroup Move Shade Down\n");
    SC_MoveShade(P3_Address_Mode_Group_Id,&address,DIRECTION_DOWN);
    OS_TaskSleep(4000);

    printf("Unassign Group\n");
    address.Device_Id = SHADE_ID;
    SC_GroupAssign(P3_Address_Mode_Device_Id,&address,8,false);
    OS_TaskSleep(2000);

    printf("\nGroup Move Shade Up\n");
    address.Group_Id[0] = 8;
    address.Group_Id[1] = 0;
    SC_MoveShade(P3_Address_Mode_Group_Id,&address,DIRECTION_UP);
    OS_TaskSleep(4000);




//    P3_Address_Mode_Type adr_mode = P3_Address_Mode_Device_Id;
//    P3_Address_Internal_Type address;
////    uint16_t val = 0x5678;
////    uint16_t val_open = 0xffff;
////    uint16_t val_close = 0x0;
//    uint8_t id_list[8];
////    uint8_t group_id = 0x56;
//    uint8_t scene_id = 0x67;
////    uint16_t cfg;
//    uint8_t len;
//    uint8_t msg[10];
//    uint8_t enf_len;
//    uint8_t enf_msg[10];
//    address.Unique_Id = 0;
//    address.Device_Id = SHADE_ID;

//    len = sizeof(MULTI_PACKET);
//    memcpy(msg, MULTI_PACKET,len);

//    enf_len = sizeof(INFORCE_PACKET);
//    memcpy(enf_msg, INFORCE_PACKET,enf_len);

//    id_list[0] = scene_id;
//    id_list[1] = 0x22;
//    id_list[2] = 0x33;
//    id_list[3] = 0x44;
//    id_list[4] = 0x55;
//    id_list[5] = 0x66;
//    id_list[6] = 0x77;
//    id_list[7] = 0x88;


//////SC_EnableNetworkJoin();
//////SC_DiscoverShades();
////-----------------
//#ifdef TEST_SHADE_MOVE
//    printf("\nMoving Shade Up\n");
//    SC_MoveShade(adr_mode,&address,DIRECTION_UP);
//    OS_TaskSleep(2000);
//    printf("Moving Shade Down\n");
//    SC_MoveShade(adr_mode,&address,DIRECTION_DOWN);
//    OS_TaskSleep(2000);
//    printf("Stopping Shade\n");
//    SC_MoveShade(adr_mode,&address,DIRECTION_STOP);
//    OS_TaskSleep(2000);
//#endif

//#ifdef TEST_SHADE_POSITION
////    printf("Moving Primary Rail to 0xffff\n");
////    SC_SetShadePosition(adr_mode,&address,pkPrimaryRail,0xffff);
////    OS_TaskSleep(5000);
////    printf("Reading Shade Position\n");
////    SC_GetShadePosition(adr_mode,&address);
////    OS_TaskSleep(1000);
////    printf("Moving Primary Rail to 0x5678\n");
////    SC_SetShadePosition(adr_mode,&address,pkPrimaryRail,val);
////    OS_TaskSleep(5000);
////    printf("Reading Shade Position\n");
////    SC_GetShadePosition(adr_mode,&address);
////    OS_TaskSleep(1000);
//////    printf("Reading Shade Primary Position\n");
//////    SC_GetShadeRail(adr_mode,&address,pkPrimaryRail);
//////    OS_TaskSleep(1000);
//////    printf("Reading Shade Secondary Position\n");
//////    SC_GetShadeRail(adr_mode,&address,pkSecondaryRail);
//////    OS_TaskSleep(1000);
//////    printf("Reading Shade Tilt Position\n");
//////    SC_GetShadeRail(adr_mode,&address,pkVaneTilt);
//////    OS_TaskSleep(1000);
////    printf("Moving Primary Rail to 0x0000\n");
////    SC_SetShadePosition(adr_mode,&address,pkPrimaryRail,0x0000);
////    OS_TaskSleep(5000);
////    printf("Reading Shade Position\n");
////    SC_GetShadePosition(adr_mode,&address);
////    OS_TaskSleep(1000);
////    printf("Jog Shade\n");
////    SC_JogShade(adr_mode,&address );
////    OS_TaskSleep(2000);
//#endif

//#ifdef TEST_READ_FW
//    printf("Reading Motor Firmware\n");
//    SC_RequestMotorFW(adr_mode,&address);
//    OS_TaskSleep(1000);
//    printf("Reading Radio Firmware\n");
//    SC_RequestReceiverFW(adr_mode,&address);
//    OS_TaskSleep(1000);
//#endif

//#ifdef TEST_MULTI_DATA_PACKET
//    printf("Request Shade Position and Battery Voltage\n");
//    sc_send_raw_payload(adr_mode,&address,len,msg);
//    OS_TaskSleep(2000);
//    sc_send_raw_payload(adr_mode,&address,enf_len,enf_msg);
//    OS_TaskSleep(2000);
//#endif


//#ifdef TEST_BATTERY_REQUEST
//    printf("Request Battery Voltage\n");
//    SC_RequestBatteryLevel(adr_mode,&address,NULL);
//#endif

//#ifdef TEST_SCENE
////    printf("Set Shade to Primary Rail Position 0x5678\n");
////    SC_SetShadePosition(adr_mode,&address,pkPrimaryRail,val);
////    OS_TaskSleep(5000);
////    printf("Set Shade to Scene at Current Position\n");
////    SC_SetSceneToCurrent(adr_mode,&address,scene_id);
////    OS_TaskSleep(2000);
////    printf("Request Shade Scene Position\n");
////    SC_RequestScenePosition(adr_mode,&address,scene_id);
////    OS_TaskSleep(2000);
////    printf("Set Shade to Primary Rail Position 0xFFFF\n");
////    SC_SetShadePosition(adr_mode,&address,pkPrimaryRail,val_close);
////    OS_TaskSleep(5000);
////    printf("Execute Scene\n");
////    SC_ExecuteScene(1,id_list);
////    OS_TaskSleep(5000);
////    printf("Set Shade to Primary Rail Position 0xFFFF\n");
////    SC_SetShadePosition(adr_mode,&address,pkPrimaryRail,val_close);
////    OS_TaskSleep(5000);
////    printf("Delete Scene\n");
////    SC_DeleteScene(adr_mode,&address,scene_id);
////    OS_TaskSleep(2000);
////    printf("Execute Scene\n");
//    SC_ExecuteScene(1,id_list);
////    OS_TaskSleep(1000);
//#endif

//#ifdef TEST_REQUEST_SHADE_TYPE
//    printf("Requesting Shade Type\n");
//    SC_RequestShadeType(adr_mode,&address);
//    OS_TaskSleep(5000);
//#endif

//    OS_TaskSleep(1000);


//////#define SR_RESET_NETWORK_ID             BIT0
//////#define SR_RANDOMIZE_DEVICE_ID          BIT1
//////#define SR_DEL_APPROVED_CONTROLLERS     BIT2
//////#define SR_DEL_GROUP_1_TO_6             BIT3
//////#define SR_DEL_GROUP_7_TO_255           BIT4
//////#define SR_CLEAR_DISCOVERED_FLAG        BIT5
//////#define SR_DELETE_SCENES                BIT8
//////#define SR_RECAL_NEXT_RUN               BIT9
//////#define SR_CLEAR_BOTTOM_LIMIT           BIT10
//////#define SR_CLEAR_TOP_LIMIT              BIT11
//////#define SR_REVERT_SHADE_TYPE_TO_DEFAULT BIT12
////    cfg = SR_REVERT_SHADE_TYPE_TO_DEFAULT | SR_RESET_NETWORK_ID;
////    SC_ResetShade(P3_Address_Mode_Device_Id, &address, cfg);
}
