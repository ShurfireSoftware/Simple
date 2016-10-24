/** @file
 *
 * @defgroup ipc_client_core Inter-Process Communication client.
 * @{
 * @brief Header file for inter process communication functions used
 *     by the hub core.
 *
 * @details 
 *
 */

#ifndef IPC_CLIENT_CORE_H__
#define IPC_CLIENT_CORE_H__

#include <stdbool.h>
#include <stdint.h>
#include "ipc.h"

#define MAX_JSON_LENGTH 8192
#define TRUE_STR    "True"
#define FALSE_STR   "False"

#define socketBufferSize_ 8192
#define retries_ 1500

typedef struct {
    uint16_t len;
    char * p_buff;
}IPC_RECEIVE_MSG, *IPC_RECEIVE_MSG_PTR;

// for client
bool connectSocket(char *p_path,uint16_t *p_socket);
int sendMessage(char* msg, uint16_t len, uint16_t socket);
IPC_RECEIVE_MSG_PTR startListening(uint16_t socket);
bool interpret_data(char *p_json_resp, char * p_msg_type);
void ipc_expect_no_response(char *);
void free_msg_mem(IPC_RECEIVE_MSG_PTR p_msg);
void IPC_PrintClientJson(char * p_json, char * p_full_resp);

#endif

/** @} */
