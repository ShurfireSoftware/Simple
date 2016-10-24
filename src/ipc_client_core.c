/** @file
 *
 * @defgroup ipc_client_core IPC Client Core
 * @{
 * @brief Code for sending from hub core as a client
 *
 * @details This module contains the IPC client used by the
 *     hub core.
 *
 */

/* Includes
*******************************************************************************/
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <ctype.h>

#include "os.h"
#include "ipc_client_core.h"
#include "JSONReader.h"

/* Global Variables
*******************************************************************************/

/* Local Constants and Definitions
*******************************************************************************/

#define PRINT_IPC_JSON

/* Local Function Declarations
*******************************************************************************/

/* Local variables
*******************************************************************************/
char clientJSON[MAX_JSON_LENGTH];


void IPC_PrintClientJson(char * p_json, char * p_full_resp)
{
#ifdef PRINT_IPC_JSON
    char *p_dsp;
    printf("Sent:\n");
    p_dsp = p_json;
    while (*p_dsp) {
        if (*p_dsp != '\f')
            printf("%c",*p_dsp);
        ++p_dsp;
    }
    printf("\nReceived:\n");
    p_dsp = p_full_resp;
    while (*p_dsp) {
        if (*p_dsp != '\f')
            printf("%c",*p_dsp);
        ++p_dsp;
    }
    printf("\n");
#endif
}

IPC_RECEIVE_MSG_PTR ipc_client_communicate(char * p_path)
{
    uint16_t socket;
    if(connectSocket(p_path,&socket)) {
        sendMessage(clientJSON,strlen(clientJSON),socket); // make request
        IPC_RECEIVE_MSG_PTR p_msg = startListening(socket);             // wait for response
        if (p_msg->len) {
            IPC_PrintClientJson(clientJSON,p_msg->p_buff);
        }
        return p_msg;
    }
    return NULL;
}

bool interpret_data(char *p_json_resp, char * p_msg_type)
{
    char type[MAX_TYPE_STR_LENGTH];
    char action[MAX_ACTION_STR_LENGTH];
    uint16_t length;
    bool valid = false;

    length = strlen(p_json_resp);
    if(p_json_resp[length - 1] == '\f') {
        p_json_resp[length - 1] = 0;
        length--;
    }

    // first validate the data
    if(findJSONString(p_json_resp, "type", type)) {
        if(findJSONString(p_json_resp, "data\\action", action)) {
            if(strcmp(type, p_msg_type) == 0) {
                valid = true;
            }
        }
    }
    return valid;
}

void free_msg_mem(IPC_RECEIVE_MSG_PTR p_msg)
{
    OS_ReleaseMemBlock(p_msg->p_buff);
    OS_ReleaseMemBlock(p_msg);
}

void ipc_expect_no_response(char *p_socket_path)
{
    IPC_RECEIVE_MSG_PTR p_msg = ipc_client_communicate(p_socket_path);
    if (p_msg != NULL) {
        free_msg_mem(p_msg);
    }
}


/** @} */
