/***************************************************************************//**
 * @file ipc_server.c
 * @brief Wrapper functions.
 ******************************************************************************/
/** @file
 *
 * @defgroup ipc_server IPC Server
 * @{
 * @brief Code containing IPC server task used by hub core for receiving messages.
 *
 * @details This task opens a socket to listen for messages from other
 *    processes.  Processing of messages occurs using the ipc_server_cmd module.
 *
 */

/* Includes
*******************************************************************************/
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#include "ipc.h"
#include "os.h"
//#include "rf_serial_api.h"
#include "JSONReader.h"

/* Global Variables
*******************************************************************************/

/* Local Constants and Definitions
*******************************************************************************/
#define MAX_BACKLOG             10

/* Local Function Declarations
*******************************************************************************/

/* Local variables
*******************************************************************************/

/**@brief IPC server task
 *
 * @details Set up a socket and monitor for incoming packets
 *  used as interprocess communication.  When a packet is received,
 *  call a parser to process the packet.
 */
void *ipc_server_task(void * temp)
{
    int connfd = 0;
    int listenfd = 0;
    int pkt_size;
    bool success = true;
    struct sockaddr_un addr;

    listenfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(listenfd == -1) {
        printf("server socket error\n");
        success = false;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if(*SERVER_SOCKET_PATH == '\0') {
        *addr.sun_path = '\0';
        strncpy(addr.sun_path + 1, SERVER_SOCKET_PATH + 1, sizeof(addr.sun_path) - 2);
    }
    else {
        strncpy(addr.sun_path, SERVER_SOCKET_PATH, sizeof(addr.sun_path) - 1);
    }

    unlink(SERVER_SOCKET_PATH);

    if (bind(listenfd, (struct sockaddr*)&addr,sizeof(addr))== -1) {
        printf("server bind error\n");
        success = false;
    }

    if (listen(listenfd, MAX_BACKLOG) == -1) {
        printf("Failed to listen\n");
        return false;
    }

    while(success) {
        char * p_json;
        connfd = accept(listenfd, (struct sockaddr*)NULL ,NULL); // accept awaiting request

        if(connfd == -1) {
            printf("server accept error\n");
            success = false;
            continue;
        }
        else {
            printf("\nserver accept\n");
            p_json = OS_GetMemBlock(IPC_CORE_MAX_REQUEST_SIZE);
        }

        pkt_size = read(connfd, p_json, IPC_CORE_MAX_REQUEST_SIZE);
        if (pkt_size > 0) {
            char * p_resp = IPC_ProcessPacket(p_json);
            write(connfd, p_resp, strlen(p_resp));
            OS_ReleaseMemBlock(p_resp);
        }
        OS_ReleaseMemBlock(p_json);
        close(connfd);
    }
    return 0;
}

/** @} */
