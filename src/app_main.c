/***************************************************************************//**
 * @file app_main.c
 * @brief Wrapper functions.
 ******************************************************************************/

/* Includes
*******************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <pthread.h>
#include <time.h>
#include "os.h"
#include "config.h"
#include "rf_serial_api.h"
#include "util.h"
#include "SCH_ScheduleTask.h"
#include "RMT_RemoteServers.h"

/* Global Variables
*******************************************************************************/
pthread_t TestMainTaskId;
pthread_t ShellTaskId;

/* Constants
*******************************************************************************/

/* Local Functions
*******************************************************************************/

void *main_task(void *);


/* Local variables
*******************************************************************************/
THREAD_TEMPLATE_STRUCT  ThreadList[] = 
{
    { SHELL_TASK_NUM, shell_task, 3000, SHELL_TASK_PRI, "Shell_task"},
    { MAIN_THREAD_NUM, main_task, 3000, MAIN_TASK_PRI, "MAIN_THREAD"},
    { RF_RX_TASK_NUM, rx_task, 3000,   RF_RX_TASK_PRI, "RF_rx_task"},
    { RF_TX_TASK_NUM, tx_task, 3000,   RF_TX_TASK_PRI, "RF_tx_task"},
    { RFI_INBOUND_TASK_NUM, rfi_inbound_task, 3000, RFI_INBOUND_TASK_PRI, "RFI_inbound_task"},
    { RFO_OUTBOUND_TASK_NUM, rfo_outbound_task, 3000, RFO_OUTBOUND_TASK_PRI, "RFO_outbound_task"},
    { RNC_RF_CONFIG_TASK_NUM,rnc_rf_network_config_task, 4000, RNC_RF_CONFIG_TASK_PRI, "RNC_rf_network_config_task"},
    { SCH_SCHEDULE_TASK_NUM, SCH_schedule_task, 6000, SCH_SCHEDULE_TASK_PRI, "SCH_ScheduleTask"},
    { NBT_BOOTLOAD_TASK_NUM, nbt_nordic_download_task, 6000, NBT_BOOTLOAD_TASK_PRI, "NBT_BootloadTask"},
    { RMT_REMOTE_SERVER_TASK_NUM, RMT_remote_server_task, 20000, RMT_REMOTE_SERVER_TASK_PRI, "RMT_RemoteServerTask"},
    { IPC_SERVER_TASK_NUM, ipc_server_task, 10000, IPC_SERVER_TASK_PRI, "ipc_server_task"},
    { 0 }
};

int main(void)
{
    OS_Init(ThreadList);
    TestMainTaskId = OS_TaskCreate(MAIN_THREAD_NUM, 0);
    while(1) {
        OS_TaskSleep(5000);
    }
    return 0;
}

void test_schedule_callback(uint16_t unused)
{
    printf("Schedule Callback\n\r");
    SCH_DisplayCurrentTime();
}

void test_schedule(void)
{
    DAY_STRUCT day;
    day.hour = 10;
    day.minute = 5;
    day.second = 0;
    SCH_ScheduleDaily(&day, test_schedule_callback);
}

void *main_task(void * temp)
{
printf("\n");
    ShellTaskId = OS_TaskCreate(SHELL_TASK_NUM, 0);

    RC_InitRadio();
printf("main_task\n");
    RNC_InitRFNetworkCommunicationTasks();
    OS_TaskSleep(1000);
    SCH_ScheduleTaskInit();
    RMT_InitRemoteServers();

printf("Task setup complete\n");
test_schedule();
#ifdef USE_ME
    uint16_t id = RC_GetNetworkId();
    PKT_STRUCT_ADDRESS adr_pkt;
    printf("NID = %04x\n",id);
    OS_TaskSleep(8000);
    printf("Set Nid\n");
    id = 0x0390;
    RC_AssignNewNetworkId(id);
    OS_TaskSleep(2000);
    adr_pkt.address.Unique_Id = 0;
    adr_pkt.address.Device_Id = 0xffff;
    adr_pkt.adr_mode = P3_Address_Mode_Device_Id;
    SC_JogShade(&adr_pkt);
//    P3_Address_Internal_Type addr;
//    addr.Unique_Id = 0;
//    addr.Device_Id = 0xffff;
//    SC_JogShade(P3_Address_Mode_Device_Id,&addr);
#endif

    while(1) {
        OS_TaskSleep(5000);
    }
    return 0;
}

