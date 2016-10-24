/***************************************************************************//**
 * @file rfu_uart.c
 * @brief Provide send and receive for Nordic communication.
 *
 * @author Neal Shurmantine
 * @copyright (c) 2014 Hunter Douglas. All rights reserved.
 *
 * @date Created: 09/19/2014
 * @date Last updated: 09/19/2014
 *
 * Change Log:
 * 09/19/2014
 * - Created.
 *
 ******************************************************************************/

/* Includes
*******************************************************************************/
#include <errno.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include "rf_serial_api.h"
#include "util.h"

#include "config.h"
#include "os.h"
#include "rfu_uart.h"
#include "que.h"

/* Local Constants and Definitions
*******************************************************************************/
//#define SERIAL_PORT_NAME  "/dev/ttyUSB0"
//raspberrypi: /dev/ttyAMA0
//imx6 board: /dev/ttymxc1

#define SERIAL_PORT_NAME  "/dev/ttymxc6"
#define RF_SERIAL_TX_EVENT_BIT     BIT0
#define RECEIVE_SIZE              511
#define NORMAL_RECEIVE_WAIT_TIME        20
#define BOOTLOAD_RECEIVE_WAIT_TIME       5
#define BAUDRATE B115200

/* Local Function Declarations
*******************************************************************************/
static void tx_msg_to_uart(void);

/* Local variables
*******************************************************************************/
static void *InboundEventHandle;
static uint16_t InboundEventBit;
static void *BootloadEventHandle;
static uint16_t BootloadEventBit;
static void *ActiveEventHandle;
static uint16_t ActiveEventBit;

static uint16_t TxMailbox;
static int nordic_fp = 0;
static uint8_t RxData[RECEIVE_SIZE+1];
static S_QUEUE    RxQueue;
static uint32_t URX_WaitTime;

static int set_interface_attribs(void)
{
    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    tty.c_iflag = IGNPAR | IGNBRK;
    tty.c_oflag = 0;
    tty.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD; 
    tty.c_lflag = 0;
    tty.c_cc[VMIN]=0;
    tty.c_cc[VTIME]=0;
    if (tcsetattr(nordic_fp, TCSANOW, &tty) != 0) {
        return false;
    }
    return true;
}

static bool setup_serial_port(void)
{
    bool rtn = false;
    char *portname = SERIAL_PORT_NAME;
    nordic_fp = open(portname, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (nordic_fp != -1) {
        return set_interface_attribs();
    }
    return rtn;
}


/*******************************************************************************
* Procedure:    tx_task
* Purpose:      Task that processes serial output for Nordic RF chip.
* Passed:
*   
* Returned:     nothing
* Globals:      none
*
* Date:         Author:             Comments:
*   2014-09-19  Neal Shurmantine    initial revision
*******************************************************************************/
void *tx_task(void * param)
{
    void *event_handle;
    uint32_t wait_time = WAIT_TIME_INFINITE;
    uint16_t event_mask = RF_SERIAL_TX_EVENT_BIT;
    uint16_t event_active;

//#define SERIAL_PORT_NAME  "/dev/ttyAMA0"
    if (setup_serial_port() == false) {
        OS_Error(OS_ERR_SERIAL_PORT);
    }
    
    event_handle = OS_EventCreate(0,false);
    TxMailbox = OS_MboxCreate(event_handle,RF_SERIAL_TX_EVENT_BIT);
printf("tx_task\n");
    while(1) {
        event_active = OS_TaskWaitEvents(event_handle, event_mask, wait_time);
        if (event_active == RF_SERIAL_TX_EVENT_BIT) {
            tx_msg_to_uart();
            //give a little space between stacked up messages
            event_mask = 0;
            wait_time = 20;
        }
        else {
            event_mask = RF_SERIAL_TX_EVENT_BIT;
            wait_time = WAIT_TIME_INFINITE;
       }
   }
}

/*******************************************************************************
* Procedure:    tx_msg_to_uart
* Purpose:      Task that processes serial input from Nordic RF chip.
* Passed:
*   
* Returned:     nothing
* Globals:      none
*
* Date:         Author:             Comments:
*   2014-09-19  Neal Shurmantine    initial revision
*******************************************************************************/
static void tx_msg_to_uart(void)
{
    sSIMPLE_MESSAGE_PTR p_content;
    uint16_t idx;
    p_content = (sSIMPLE_MESSAGE_PTR)OS_MessageGet(TxMailbox);
    write(nordic_fp, p_content->msg, p_content->len);

printf("<");
int i;
for (i=0; i<p_content->len; ++i) printf("%02x ",p_content->msg[i]);
printf("\n");

    OS_ReleaseMsgMemBlock((void*)p_content);
}

/*******************************************************************************
* Procedure:    rx_task
* Purpose:      Task that processes serial input from Nordic RF chip.
* Passed:
*   
* Returned:     nothing
* Globals:      none
*
* Date:         Author:             Comments:
*   2014-09-19  Neal Shurmantine    initial revision
*******************************************************************************/
void *rx_task(void * param)
{
    int numbytes;
    bool process;
    char ser_rx_buff[1];
    QInit(RECEIVE_SIZE,&RxQueue, RxData);
    URX_WaitTime = NORMAL_RECEIVE_WAIT_TIME;

uint16_t tick_count = 0;
printf("rx_task\n");
    while(1)
    {    
        OS_TaskSleep(URX_WaitTime);
++tick_count;

#ifdef USE_ME
if ((tick_count % 100) == 1) printf(".");
if ((tick_count % 500) == 1) printf("\n");
#endif

        process = false;
        do {
            numbytes = read(nordic_fp, ser_rx_buff, 1);
            if (numbytes != 0) {
printf("%02x ",(uint8_t)ser_rx_buff[0]);
                process = true;
                QInsert(ser_rx_buff[0], &RxQueue);
            }
        } while (numbytes != 0);
        if (process == true) {
            OS_EventSet(ActiveEventHandle,ActiveEventBit);
        }
    }
}

/*******************************************************************************
* Procedure:    RFU_SetBootloadActive
* Purpose:      xxxx
* Passed:       xxxx
*   
* Returned:     nothing
* Globals:      none
*
* Date:         Author:             Comments:
*   2014-03-13  Neal Shurmantine    initial revision
*******************************************************************************/
void RFU_SetBootloadActive(bool isBootloadActive)
{
    int numbytes;
    char ser_rx_buff[1];

//    OS_SchedLock();<<
    OS_EventClear(ActiveEventHandle,ActiveEventBit);
    QFlush(&RxQueue);
    if (isBootloadActive == true) {
        URX_WaitTime = BOOTLOAD_RECEIVE_WAIT_TIME;
        ActiveEventHandle = BootloadEventHandle;
        ActiveEventBit = BootloadEventBit;
    }
    else {
        URX_WaitTime = NORMAL_RECEIVE_WAIT_TIME;
        ActiveEventHandle = InboundEventHandle;
        ActiveEventBit = InboundEventBit;
    }

    do {
        numbytes = read(nordic_fp, ser_rx_buff, 1);
    } while (numbytes != 0);
//    OS_SchedUnlock();<<

}

/*******************************************************************************
* Procedure:    RFU_Register_Bootload_Event
* Purpose:      xxxx
* Passed:       xxxx
*   
* Returned:     nothing
* Globals:      none
*
* Date:         Author:             Comments:
*   2014-03-13  Neal Shurmantine    initial revision
*******************************************************************************/
void *RFU_Register_Bootload_Event(uint16_t event_group, uint16_t event_mask)
{
    BootloadEventBit = event_mask;
    BootloadEventHandle = OS_EventCreate(event_group,false);
    return BootloadEventHandle;
}


/*******************************************************************************
* Procedure:    RFU_Register_Inbound_Event
* Purpose:      xxxx
* Passed:       xxxx
*   
* Returned:     nothing
* Globals:      none
*
* Date:         Author:             Comments:
*   2014-09-19  Neal Shurmantine    initial revision
*******************************************************************************/
void *RFU_Register_Inbound_Event(uint16_t event_group, uint16_t event_mask)
{
    InboundEventBit = event_mask;
    InboundEventHandle = OS_EventCreate(event_group,false);
    ActiveEventHandle = InboundEventHandle;
    ActiveEventBit = InboundEventBit;
    return InboundEventHandle;
}

/*******************************************************************************
* Procedure:    RFU_SendMsg
* Purpose:      xxxx
* Passed:       xxxx
*   
* Returned:     nothing
* Globals:      none
*
* Date:         Author:             Comments:
*   2014-09-19  Neal Shurmantine    initial revision
*******************************************************************************/
void RFU_SendMsg(unsigned char len, char *msg)
{
    sSIMPLE_MESSAGE_PTR p_block = (sSIMPLE_MESSAGE_PTR)OS_GetMsgMemBlock(len+sizeof(sSIMPLE_MESSAGE));
    p_block->len = len;
    memcpy(p_block->msg,msg,len);
    OS_MessageSend(TxMailbox,p_block);
}

/*******************************************************************************
* Procedure:    RFU_GetRxChar
* Purpose:      xxxx
* Passed:       xxxx
*   
* Returned:     nothing
* Globals:      none
*
* Date:         Author:             Comments:
*   2014-09-19  Neal Shurmantine    initial revision
*******************************************************************************/
bool RFU_GetRxChar(unsigned char *rslt)
{
    if (QEmpty(&RxQueue) == false ) {
        QRemove(rslt,&RxQueue);
        if (QEmpty(&RxQueue) == true) {
            OS_EventClear(ActiveEventHandle,ActiveEventBit);
        }
        return true;
    }
    return false;
}

