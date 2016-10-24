/** @file
 *
 * @defgroup ipc Inter-process communication
 * @{
 * @brief Header file for inter process communication with hub core.
 *
 * @details 
 *
 */

#ifndef _IPC_CORE_H_
#define _IPC_CORE_H_

#define SERVER_SOCKET_PATH "/tmp/app.core"
#define DATABASE_CLIENT_PATH "/tmp/app.db"

#define MAX_ACTION_STR_LENGTH   32
#define MAX_TYPE_STR_LENGTH 20
#define IPC_CORE_MAX_REQUEST_SIZE  8192
#define IPC_CORE_MAX_RESPONSE_SIZE  8192

char * IPC_ProcessPacket(char * p_json);

#endif

/** @} */
