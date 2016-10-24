/** @file
 *
 * @defgroup ipc_client_cmd_to_db Header file for IPC Client Commands to Databases
 * @{
 * @brief Code for hub core client commands that are sent to the databases.
 *
 *
 */

#ifndef IPC_CLIENT_CORE_CMD_TO_DB_H__
#define IPC_CLIENT_CORE_CMD_TO_DB_H__

#include "ipc_client_core.h"
#include "rf_serial_api.h"
#include "stub.h"

#define REPORT_SHADE_POS_KIND1ONLY_CMD "{\"type\":\"databases\",\"data\":{\"action\":\"report_shade_position\",\"shade_id\":%d,\"positions\":{\"position1\":%d,\"posKind1\":%d}}}"
#define REPORT_SHADE_POS_KIND1AND2_CMD "{\"type\":\"databases\",\"data\":{\"action\":\"report_shade_position\",\"shade_id\":%d,\"positions\":{\"position1\":%d,\"posKind1\":%d,\"position2\":%d,\"posKind2\":%d}}}"
#define REPORT_SCENE_POSITION_KIND1ONLY_CMD "{\"type\":\"databases\",\"data\":{\"action\":\"report_scene_position\",\"scene_id\":%d,\"shade_id\":%d,\"positions\":{\"position1\":%d,\"posKind1\":%d}}}"
#define REPORT_SCENE_POSITION_KIND1AND2_CMD "{\"type\":\"databases\",\"data\":{\"action\":\"report_scene_position\",\"scene_id\":%d,\"shade_id\":%d,\"positions\":{\"position1\":%d,\"posKind1\":%d,\"position2\":%d,\"posKind2\":%d}}}"
#define RECORD_DISCOVERY_CMD "{\"type\":\"databases\",\"data\":{\"action\":\"record_discovery\",\"uuid\":%llu,\"network_id\":%d,\"device_id\":%d,\"shade_type\":%d}}"
#define UPDATE_BATTERY_STATUS_CMD "{\"type\":\"databases\",\"data\":{\"action\":\"update_battery_status\",\"shade_id\":%d,\"voltage\":%d}}"
#define SC_RECEIVED_CLEAR_ANNOUNCEMENT_CMD "{\"type\":\"databases\",\"data\":{\"action\":\"received_sc_clear_announcement\",\"scene_controller_id\":%d}}"
#define SC_DATABASE_UPDATE_REQUEST_CMD "{\"type\":\"databases\",\"data\":{\"action\":\"update_request_for_sc_with_id_and_ver\",\"scene_controller_id\":%d,\"version\":%d}}"
#define SC_UPDATE_REQUEST_FOR_MEMBER_CMD "{\"type\":\"databases\",\"data\":{\"action\":\"update_request_for_sc_member\",\"scene_controller_id\":%d,\"rec_num\":%d,\"version\":%d}}"
#define SC_TRIGGER_CMD "{\"type\":\"databases\",\"data\":{\"action\":\"received_sc_trigger\",\"scene_controller_id\":%d,\"scene_type\":%d,\"scene_id\":%d,\"version\":%d}}"
#define GET_SCHEDULED_EVENTS_CMD "{\"type\":\"databases\",\"data\":{\"action\":\"get_scheduled_events\"}}"
#define GET_SHADES_CMD "{\"type\":\"databases\",\"data\":{\"action\":\"get_shades\"}}"

void IPC_Client_ReportShadePosition(SHADE_POSITION_PTR shade_pos);
void IPC_Client_ReportScenePosition(SHADE_POSITION_PTR p_shade_pos, uint8_t scene_num);
void IPC_Client_RecordDiscovery(DISCOVERY_DATA_STRUCT_PTR p_disc_data);
uint8_t IPC_Client_UpdateBatteryStatus(uint16_t shade_id, uint8_t measured);
void IPC_Client_SceneControllerClearedAnnouncement(uint16_t scene_controller_id);
void IPC_Client_SceneControllerDatabaseUpdateRequest(uint16_t scene_controller_id, uint8_t version);
void IPC_Client_SceneControllerUpdatePacketRequest(uint16_t scene_controller_id, uint8_t rec_num, uint8_t version);
void IPC_Client_SceneControllerTrigger(uint16_t scene_controller, uint8_t scene_type, uint16_t scene_id, uint8_t version);
ALL_RAW_DB_STR_PTR IPC_Client_GetScheduledEvents(void);
ALL_RAW_DB_STR_PTR IPC_Client_GetShades(void);




#ifdef USE_ME

ExecuteSceneFromRemoteConnect(event->sceneOrMultiSceneID);
sendMultiSceneMsgToShades(event->sceneOrMultiSceneID);

//isEnableScheduledEvents()
//retrieveRegistrationData();
getRemoteConnectEnabled()
setEnableScheduledEvents(true);

----------------------------------------------------

    update_battery_status
get_shade_battery_data
    record_discovery
    report_shade_position
    report_scene_position

    received_sc_clear_announcement
    update_request_for_sc_with_id_and_ver
    update_request_for_sc_member
    received_sc_trigger

get_scene_index
get_multiscene_index
is_remote_connect_enabled
set_enabled_scheduled_events
reset_databases
is_ip_available
set_time_data
get_time_data
get_registration_data
get_integration_data
    get_scheduled_events
get_user_data
get_hub_data
get_shade_data
get_room_data
get_scene_data
get_scene_collection_data
get_scene_collection_members

clear_integration_data

-------------------------------------------
static void RC_nv_file_write(void)
static void RC_nv_file_read(void)
static void RC_nv_file_erase(void)

extern int32_t currentTimeOffset;
extern float latitude;
extern float longitude;
extern uint16_t sunriseTimeInMinutes, sunsetTimeInMinutes;
readRestartTimeFromFlash(&time);
writeRestartTimeToFlash(&time);
writeSunriseToFlash(&time);
writeSunsetToFlash(&time);
setLocalTimeOffset(currentTimeOffset);
writeRestartTimeToFlash(&now1);
#endif


#endif

/** @} */
