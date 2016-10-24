#ifndef __OS_CONFIG_H
#define __OS_CONFIG_H

#include <pthread.h>
#include <stdint.h>

/* TASK NUMBERS */
#define MAIN_THREAD_NUM           1
#define THREAD_1_NUM              2
#define THREAD_2_NUM              3
#define THREAD_3_NUM              4
#define RF_RX_TASK_NUM            5
#define RF_TX_TASK_NUM            6
#define RFI_INBOUND_TASK_NUM      7
#define RFO_OUTBOUND_TASK_NUM     8
#define RNC_RF_CONFIG_TASK_NUM    9
#define SCH_SCHEDULE_TASK_NUM     10
#define NBT_BOOTLOAD_TASK_NUM     11
#define RMT_REMOTE_SERVER_TASK_NUM  12
#define AWS_IOT_TASK_NUM            13
#define IPC_SERVER_TASK_NUM         14
#define SHELL_TASK_NUM            15

/* TASK PRIORITIES 1-99 (larger num = higher priority) */
#define MAIN_TASK_PRI               2
#define THREAD_1_PRI                5
#define THREAD_2_PRI                5
#define THREAD_3_PRI                5
#define TIMER_PRI                   20
#define RF_RX_TASK_PRI              12
#define RF_TX_TASK_PRI              12
#define RFI_INBOUND_TASK_PRI        12
#define NBT_BOOTLOAD_TASK_PRI       12
#define RFO_OUTBOUND_TASK_PRI       11
#define RNC_RF_CONFIG_TASK_PRI      10
#define SCH_SCHEDULE_TASK_PRI       13
#define RMT_REMOTE_SERVER_TASK_PRI  10
#define AWS_IOT_TASK_PRI            9
#define IPC_SERVER_TASK_PRI         11
#define SHELL_TASK_PRI              9

/* TASK FUNCTIONS */
extern void *main_task(void *);
extern void *thread_1(void *);
extern void *thread_2(void *);
extern void *thread_3(void *);
extern void *rx_task(void *);
extern void *tx_task(void *);
extern void *rfi_inbound_task(void *);
extern void *rfo_outbound_task(void *);
extern void *rnc_rf_network_config_task(void *);
extern void *SCH_schedule_task(void *);
extern void *nbt_nordic_download_task(void *);
extern void *RMT_remote_server_task(void * temp);
extern void *aws_iot_task(void * temp);
extern void *ipc_server_task(void * temp);
extern void *shell_task(void * temp);

/* TASK IDs */
extern pthread_t Thread1TaskId;
extern pthread_t Thread2TaskId;
extern pthread_t Thread3TaskId;
extern pthread_t TestMainTaskId;
extern pthread_t RfRxTaskId;
extern pthread_t RfTxTaskId;
extern pthread_t RfInboundTaskId;
extern pthread_t RfOutboundTaskId;
extern pthread_t RNCRFNetworkConfigTaskId;
extern pthread_t SCH_ScheduleTaskId;
extern pthread_t NBTBootloadTaskId;
extern pthread_t RMT_RemoteServerTaskId;
extern pthread_t AwsIotTaskId;
extern pthread_t IPCServerTaskId;
extern pthread_t ShellTaskId;

typedef struct
{
    uint16_t len;
    uint16_t count;
    uint8_t msg[];
} sSIMPLE_MESSAGE, *sSIMPLE_MESSAGE_PTR;

typedef void (*FUNC_PTR_PARAM)(void *);
typedef void (*FUNC_PTR)(void);

#define BIT0                            (0x0001)
#define BIT1                            (0x0002)
#define BIT2                            (0x0004)
#define BIT3                            (0x0008)
#define BIT4                            (0x0010)
#define BIT5                            (0x0020)
#define BIT6                            (0x0040)
#define BIT7                            (0x0080)
#define BIT8                            (0x0100)
#define BIT9                            (0x0200)
#define BITA                            (0x0400)
#define BITB                            (0x0800)
#define BITC                            (0x1000)
#define BITD                            (0x2000)
#define BITE                            (0x4000)
#define BITF                            (0x8000)
#define BIT10                           (0x0400)
#define BIT11                           (0x0800)
#define BIT12                           (0x1000)
#define BIT13                           (0x2000)
#define BIT14                           (0x4000)
#define BIT15                           (0x8000)

#endif

