/** @file
 *
 * @defgroup ipc_client_socket.c 
 * @{
 * @brief Code for creating socket for the hub core socket to use to communicate
 *     with other processes.
 *
 * @details This module contains the code for creating and using the socket
 *     required by the IPC client from hub core.
 *
 */

/* Includes
*******************************************************************************/
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "os.h"
#include "ipc_client_core.h"

typedef enum {errNone = 0, errMakeSocket, errConnectSocket} eErr;

static struct sockaddr_un addr;
static eErr error;
static bool made;


static struct sockaddr_un makeAddress(char *socket_path)
{
    struct sockaddr_un addr;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if(*socket_path == '\0') {
        *addr.sun_path = '\0';
        strncpy(addr.sun_path + 1, socket_path + 1, sizeof(addr.sun_path) - 2);
    }
    else {
        strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    }
    return addr;
}

static bool tryToConnect(char *socket_path, uint16_t *p_socket)
{
    bool success = true;

    // make path
    addr = makeAddress(socket_path);

    // try to make socket
    if(!made) {
        *p_socket = socket(AF_UNIX, SOCK_STREAM, 0);
        made = (*p_socket != -1);
        if(!made) {
            if(!error) error = errMakeSocket;
            success = false;
        }
    }

    // try to connect socket
    if(success) {
        if(connect(*p_socket, (struct sockaddr*) & addr, sizeof(addr)) == -1) {
            if(!error) error = errConnectSocket;
            success = false;
        }
        else printf("connected\n");
    }
    return success;
}

bool connectSocket(char *socket_path, uint16_t * p_socket)
{
    bool connected = false;
    uint16_t ctr = retries_;

    error = errNone;
    made = false;

    while(!connected && ctr) {
        connected = tryToConnect(socket_path, p_socket);
        if(!connected) {
            usleep(500);
            ctr--;
            printf(".");
        }
    }

    if(!connected) {
        printf("socket error: ");
        switch(error) {
            case errMakeSocket: printf("could not make\n"); break;
            case errConnectSocket: printf("could not connect\n"); break;
            default: printf("unknown\n");
        }
    }
    return connected;
}

int sendMessage(char *msg, uint16_t len, uint16_t socket)
{
    return write(socket, msg, len);
}

IPC_RECEIVE_MSG_PTR startListening(uint16_t socket)
{
    IPC_RECEIVE_MSG_PTR p_msg = (IPC_RECEIVE_MSG_PTR)OS_GetMemBlock(sizeof(IPC_RECEIVE_MSG));
    p_msg->p_buff = (char*)OS_GetMemBlock(socketBufferSize_);

    p_msg->len = read(socket, p_msg->p_buff, socketBufferSize_);
    close(socket);
    printf("disconnected\n");

    return p_msg;
}


/** @} */
