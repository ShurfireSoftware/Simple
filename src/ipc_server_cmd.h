/** @file
 *
 * @defgroup ipc_server_cmd.h  Hub Core IPC Server Command
 * @{
 * @brief Header file for Inter-Process Communication process 
 *    commands.  This module declares the functions that are
 *    executed when an appropriate command is received by the
 *    hub core.
 *
 * @details The structure IPC_PARSE_STRUCT_STRUCT contains
 *   the hub data action string and its associated function.
 *   An array of possible function is created and, then
 *   a call is received, this array is traversed to find
 *   the matching string. If the string is found then its
 *   associated function is called.
 *
 */

#ifndef IPC_SERVER_CORE_CMD_H__
#define IPC_SERVER_CORE_CMD_H__


char * ipc_get_nordic_uuid(char * p_json);
char * ipc_set_network_id(char * p_json);
char * ipc_get_network_id(char * p_json);
char * ipc_create_network_id(char * p_json);
char * ipc_enable_join(char * p_json);
char * ipc_is_join_active(char * p_json);
char * ipc_disable_join(char * p_json);
char * ipc_discover_shades(char * p_json);
char * ipc_is_discovery_active(char * p_json);
char * ipc_send_beacon(char * p_json);
char * ipc_group_assign(char * p_json);
char * ipc_jog_shade(char * p_json);
char * ipc_jog_group(char * p_json);
char * ipc_set_shade_pos(char * p_json);
char * ipc_set_group_pos(char * p_json);
char * ipc_clear_shade_groups(char * p_json);
char * ipc_calibrate_shade(char * p_json);
char * ipc_delete_shade(char * p_json);
char * ipc_clear_shade(char * p_json);
char * ipc_get_shade_pos(char * p_json);
char * ipc_set_shade_scene_to_current(char * p_json);
char * ipc_set_group_scene_to_current(char * p_json);
char * ipc_set_shade_scene_at_pos(char * p_json);
char * ipc_set_group_scene_at_pos(char * p_json);
char * ipc_execute_scene(char * p_json);
char * ipc_delete_scene(char * p_json);
char * ipc_request_scene_position(char * p_json);
char * ipc_request_shade_battery_level(char * p_json);
char * ipc_reset_all_shades(char * p_json);
char * ipc_request_coarse_battery_level(char * p_json);
char * ipc_scene_controller_clear_ack(char * p_json);
char * ipc_scene_controller_update_header(char * p_json);
char * ipc_scene_controller_update_packet(char * p_json);
char * ipc_scene_controller_trigger_ack(char * p_json);
char * ipc_set_time(char * p_json);
char * ipc_modify_scheduled_scenes(char * p_json);
char * ipc_trigger_remote_data_sync(char * p_json);
char * ipc_is_remote_data_sync_busy(char * p_json);
char * ipc_sync_remote_data_immediately(char * p_json);
char * ipc_get_remote_data_sync_error(char * p_json);
char * ipc_is_registration_busy(char * p_json);
char * ipc_has_unregister_been_attempted(char * p_json);
char * ipc_notify_remote_connect_status(char * p_json);
char * ipc_connect_to_aws(char * p_json);
char * ipc_unregister_hub(char * p_json);
char * ipc_register_hub(char * p_json);
char * ipc_get_registration_status(char * p_json);
char * ipc_get_registration_error(char * p_json);

typedef struct IPC_PARSE_STRUCT_STRUCT
{
    char * p_action;
    char * (*funct)(char*);
} IPC_PARSE_STRUCT;


const IPC_PARSE_STRUCT IPC_Functions[] = {
    { "get_nordic_uuid", ipc_get_nordic_uuid },
    { "set_network_id", ipc_set_network_id },
    { "get_network_id", ipc_get_network_id },
    { "create_network_id", ipc_create_network_id },
    { "enable_join", ipc_enable_join },
    { "is_join_active", ipc_is_join_active },
    { "disable_join", ipc_disable_join },
    { "discover_shades", ipc_discover_shades },
    { "is_discovery_active", ipc_is_discovery_active },
    { "send_beacon", ipc_send_beacon },
    { "group_assign", ipc_group_assign },
    { "jog_shade", ipc_jog_shade },
    { "jog_group", ipc_jog_group },
    { "set_shade_pos", ipc_set_shade_pos },
    { "set_group_pos", ipc_set_group_pos },
    { "clear_shade_groups", ipc_clear_shade_groups },
    { "calibrate_shade", ipc_calibrate_shade },
    { "delete_shade", ipc_delete_shade },
    { "clear_shade", ipc_clear_shade },
    { "get_shade_pos", ipc_get_shade_pos },
    { "set_shade_scene_to_current", ipc_set_shade_scene_to_current },
    { "set_group_scene_to_current", ipc_set_group_scene_to_current },
    { "set_shade_scene_at_pos", ipc_set_shade_scene_at_pos },
    { "set_group_scene_at_pos", ipc_set_group_scene_at_pos },
    { "execute_scene", ipc_execute_scene },
    { "delete_scene", ipc_delete_scene },
    { "request_scene_position", ipc_request_scene_position },
    { "request_shade_battery_level", ipc_request_shade_battery_level },
    { "reset_all_shades", ipc_reset_all_shades },
    { "request_coarse_battery_level", ipc_request_coarse_battery_level },
    { "scene_controller_clear_ack", ipc_scene_controller_clear_ack },
    { "scene_controller_update_header", ipc_scene_controller_update_header },
    { "scene_controller_update_packet", ipc_scene_controller_update_packet },
    { "scene_controller_trigger_ack", ipc_scene_controller_trigger_ack },
    { "set_time", ipc_set_time },
    { "modify_scheduled_scenes", ipc_modify_scheduled_scenes },
    { "trigger_remote_data_sync", ipc_trigger_remote_data_sync },
    { "is_remote_data_sync_busy", ipc_is_remote_data_sync_busy },
    { "sync_remote_data_immediately", ipc_sync_remote_data_immediately },
    { "get_remote_data_sync_error", ipc_get_remote_data_sync_error },
    { "is_registration_busy", ipc_is_registration_busy },
    { "has_unregister_been_attempted", ipc_has_unregister_been_attempted },
    { "notify_remote_connect_status", ipc_notify_remote_connect_status },
    { "connect_to_aws", ipc_connect_to_aws },
    { "unregister_hub", ipc_unregister_hub },
    { "register_hub", ipc_register_hub },
    { "get_registeration_status", ipc_get_registration_status },
    { "get_registration_error", ipc_get_registration_error },
    { "",NULL }
};

#endif

/** @} */

