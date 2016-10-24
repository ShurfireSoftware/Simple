#ifndef _OS_H_
#define _OS_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <sched.h>

typedef struct EVENT_STRUCT_TAG
{
    pthread_cond_t trigger;
    pthread_mutex_t mutex;
    uint16_t spec_event;
} EVENT_STRUCT, *EVENT_STRUCT_PTR;

typedef void *(* THREAD_FUNCT)(void *);
typedef pthread_t _task_id;
typedef struct thread_template_struct
{
    uint32_t template_index;
    THREAD_FUNCT task_func;
    uint32_t stack_size;
    uint32_t priority;
    char * task_name;
} THREAD_TEMPLATE_STRUCT, * THREAD_TEMPLATE_STRUCT_PTR;

#define WAIT_TIME_INFINITE      0
#define BILLION  1000000000L

#define OS_ERR_MEMPOOL_LOW      1
#define OS_ERR_NO_MEMPOOL       2
#define OS_ERR_NO_MAILBOXES     3
#define OS_ERR_NO_EVENTS        4
#define OS_ERR_MSG_QUEUE_FAIL   5
#define OS_ERR_EVENT_SET_FAIL   6
#define OS_ERR_SEM_CREATE       7
#define OS_ERR_SEM_OPEN         8
#define OS_ERR_SEM_NOT_CREATED  9
#define OS_ERR_NO_TIMERS        10
#define OS_ERR_TIMER_FAIL       11
#define OS_ERR_MEMORY_FREE      12
#define OS_ERR_MEM_ALLOC_FAIL   13
#define OS_ERR_MUTEX_FAIL       14
#define OS_ERR_MSG_ALLOC_FAIL   15
#define OS_ERR_THREAD_FAIL      16
#define OS_ERR_TASK_CLOCK_FAIL  17
#define OS_ERR_MSG_QUEUE_MSG_FAIL  18
#define OS_ERR_SERIAL_PORT      19

void OS_Init(THREAD_TEMPLATE_STRUCT_PTR p_list);
pthread_t OS_TaskCreate(uint32_t template_index, void * parameter);
void * OS_EventCreate(uint16_t event_group, bool auto_clear);

uint16_t OS_TaskWaitEvents(void* handle, uint16_t mask, int32_t delay);

void OS_EventClear(void * event_handle, uint16_t mask);
void OS_EventSet(void * event_handle, uint16_t mask);
uint16_t OS_MboxCreate(void *event_handle, uint16_t event_bit );
void OS_MessageSend(uint16_t mailbox_index, void * p_envelope);
void *OS_MessageGet(uint16_t mailbox_index);
void *OS_GetMsgMemBlock(uint16_t size);
void OS_ReleaseMsgMemBlock(void *p_msg);

void *OS_GetMemBlock(uint16_t size);
void OS_ReleaseMemBlock(void *p_msg);

uint16_t OS_TimerCreate(void *event_handle, uint16_t event_bit);
void OS_TimerStop(uint16_t timer);
void OS_TimerSetCyclicInterval(uint16_t timer, uint32_t interval);

void OS_GetTimeLocal(time_t*);
void OS_SetTime(time_t *time);

void OS_Error(uint16_t err_code);

#define MESSAGE_HEADER_SIZE sizeof(MESSAGE_HEADER_STRUCT)

#define MAX_THREADS 20
#define MAX_MAILBOXES   40
#define MAX_TIMERS  10

#define OS_TaskYield()     sched_yield();
//#define OS_TaskSleep(val_msec)  nanosleep((const struct timespec[]){{val_msec/1000, (1000000L)*(val_msec%1000)}}, NULL);
void OS_TaskSleep(uint32_t val_msec);

#ifdef USE_ME

#define OS_SchedLock()        _task_stop_preemption()
#define OS_SchedUnlock()      _task_start_preemption()


#endif

#endif
