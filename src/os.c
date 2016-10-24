/***************************************************************************//**
 * @file OS.c
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
#include <sched.h>
#include "os.h"
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include "config.h"

/* Global Variables
*******************************************************************************/
extern int32_t currentTimeOffset;

/* Local Constants and Definitions
*******************************************************************************/
#define MILLION  1000000L
#define THOUSAND 1000L

#define OS_TIMER_RESOLUTION_MSEC    10
#define OS_TIMER_STACK_SIZE     1000

typedef struct MSG_HEADER_STRUCTURE_TAG
{
    uint16_t SIZE;
    void *p_next;
} MESSAGE_HEADER_STRUCT, * MESSAGE_HEADER_STRUCT_PTR;

typedef struct
{
    void *event_handle;
    uint16_t event_bit;
    uint16_t count;
    void *p_head;
    void *p_tail;
} MBOX_STRUCT, *MBOX_STRUCT_PTR;

typedef struct
{
    void *event_handle;
    uint16_t event_bit;
    bool enabled;
    uint32_t count_down;
} TIMER_STRUCT, *TIMER_STRUCT_PTR;

/* Local Functions
*******************************************************************************/
static void *timer_thread(void *);

/* Local variables
*******************************************************************************/
static uint16_t NumMailboxes = MAX_MAILBOXES;
static MBOX_STRUCT MboxParam[MAX_MAILBOXES];
static EVENT_STRUCT OS_Event[MAX_THREADS];

static TIMER_STRUCT TimerParam[MAX_TIMERS];
static uint16_t OS_NextAvailEvent = 0;
static pthread_mutex_t OS_MsgMutex;
static THREAD_TEMPLATE_STRUCT_PTR pThreadList;
static uint16_t NumTimers = 0;
static pthread_t OS_TimerThreadId = 0;


void OS_Init(THREAD_TEMPLATE_STRUCT_PTR p_list)
{
    pThreadList = p_list;
    pthread_mutex_init(&OS_MsgMutex, NULL);
}

pthread_t OS_TaskCreate(uint32_t template_index, void * parameter)
{
    pthread_t id;
    pthread_attr_t attr;
    struct sched_param param;
    THREAD_TEMPLATE_STRUCT_PTR p_template;
    p_template = pThreadList;
//printf("Create task %d\n",template_index);
    while (p_template->template_index != 0) {
        if (p_template->template_index == template_index) break;
        p_template++;
    }
    if (p_template->template_index == 0) {
        OS_Error(OS_ERR_THREAD_FAIL);
    }

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, p_template->stack_size);

    pthread_create(&id,&attr,p_template->task_func,NULL);
    param.sched_priority = p_template->priority;
    if (pthread_setschedparam(id, SCHED_FIFO, &param) != 0) {
        OS_Error(OS_ERR_THREAD_FAIL);
    }
//printf("Created task %d\n",id);
    return id;
}

void * OS_EventCreate(uint16_t event_group, bool clear)
{
    void * p_ev;
    pthread_mutex_lock(&OS_MsgMutex);
    if (OS_NextAvailEvent >= MAX_THREADS) {
        pthread_mutex_unlock(&OS_MsgMutex);
        OS_Error(OS_ERR_NO_EVENTS);
    }
    else {
        p_ev = (EVENT_STRUCT_PTR)&OS_Event[OS_NextAvailEvent];

        pthread_condattr_t con_attr;
        pthread_condattr_init(&con_attr);
        if (pthread_condattr_setclock(&con_attr, CLOCK_MONOTONIC)) {
            OS_Error(OS_ERR_TASK_CLOCK_FAIL);
        }
        pthread_cond_init(&OS_Event[OS_NextAvailEvent].trigger, &con_attr);

        pthread_mutex_init(&OS_Event[OS_NextAvailEvent].mutex, NULL);
        OS_NextAvailEvent++;
    }
    pthread_mutex_unlock(&OS_MsgMutex);
    return (void*)p_ev;
}

void OS_EventSet(void * handle, uint16_t event_bit)
{
    EVENT_STRUCT * p_ev = (EVENT_STRUCT*)handle;
    pthread_mutex_lock(&p_ev->mutex);
    p_ev->spec_event |= event_bit;
    pthread_cond_signal(&p_ev->trigger);
    pthread_mutex_unlock(&p_ev->mutex);
}

void OS_EventClear(void * handle, uint16_t event_bit)
{
    EVENT_STRUCT * p_ev = (EVENT_STRUCT*)handle;
    pthread_mutex_lock(&p_ev->mutex);
    p_ev->spec_event &= ~event_bit;
    pthread_mutex_unlock(&p_ev->mutex);
}

uint16_t OS_TaskWaitEvents(void * handle, uint16_t mask, int32_t delay)
{
    struct timespec loc_delay; //{time_t  tv_sec seconds; long tv_nsec nanoseconds}
//printf("Start Wait for thread %d\n",(uint32_t)handle);
    if (delay != WAIT_TIME_INFINITE) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC,&now);
        loc_delay.tv_sec = (delay/1000) + now.tv_sec;
        loc_delay.tv_nsec = (delay % 1000) * 1000000 + now.tv_nsec;
        if (loc_delay.tv_nsec > BILLION) {
            loc_delay.tv_nsec -= BILLION;
            loc_delay.tv_sec++;
        }
    }
    EVENT_STRUCT * p_ev = (EVENT_STRUCT*)handle;
    pthread_mutex_lock(&p_ev->mutex);
    while((p_ev->spec_event & mask) == 0) {
        if (delay != WAIT_TIME_INFINITE) {
            int err = pthread_cond_timedwait(&p_ev->trigger, &p_ev->mutex, &loc_delay);
            if (ETIMEDOUT == err) {
                break;
            }
        }
        else {
            pthread_cond_wait(&p_ev->trigger, &p_ev->mutex);
        }
    }
//printf("Finished Wait for thread %d\n",(uint32_t)handle);
    pthread_mutex_unlock(&p_ev->mutex);
    return p_ev->spec_event; //??
}


/**@brief Create a mailbox for a task
 *
 * @param[in]   event handle for the task   
 * @param[in]   event bit to be set when a new message is present   
 *
 * @returned  A mailbox handle (index of this mailbox in OS mailbox array. 
 *
 * @details This function creates a mailbox and initializes it.
 *     
 */
uint16_t OS_MboxCreate(void *event_handle, uint16_t event_bit )
{
    if( NumMailboxes == 0) {
        OS_Error(OS_ERR_NO_MAILBOXES);
    }
    else {
        NumMailboxes--;
        MboxParam[NumMailboxes].event_handle = event_handle;
        MboxParam[NumMailboxes].event_bit = event_bit;
        MboxParam[NumMailboxes].p_head = NULL;
        MboxParam[NumMailboxes].p_tail = NULL;
        MboxParam[NumMailboxes].count = 0;
    }
//printf("Mailbox %d\n",NumMailboxes);
    return NumMailboxes;
}

/**@brief Get a memory block for a message. *//********
 *
 * @param[in]   number of bytes required for the message.  
 *
 * @returned  A pointer to memory where the message can be
 *    stored.
 *
 * @details This function is used to obtain a block of
 *   memory to hold a message to pass to a thread.  The
 *   allocated memory also reserves some space for a
 *   header that allows the message to be included
 *   in a linked list to form a message queue.  Note
 *   that the message will be passed to only a single
 *   thread.  After use, the message memory must
 *   be freed. 
 *     
 *******************************************************/
void *OS_GetMsgMemBlock(uint16_t size)
{
    MESSAGE_HEADER_STRUCT * p_mem;

    size += MESSAGE_HEADER_SIZE;   //add room for block tag
    pthread_mutex_lock(&OS_MsgMutex);
    p_mem = (MESSAGE_HEADER_STRUCT * )calloc(size,sizeof(uint8_t));  //Get pointer to fixed block
    if (p_mem == NULL) {
        OS_Error(OS_ERR_NO_MEMPOOL);
    }
    p_mem->SIZE = size;
    p_mem->p_next = NULL;
    pthread_mutex_unlock(&OS_MsgMutex);

//printf("Get:%08x\n\r",(uint32_t)p_mem);
    return ( ((uint8_t *)p_mem)+MESSAGE_HEADER_SIZE);  //return pointer to usable memory space
}

/**@brief Send a message to a thread mailbox. *//********
 *
 * @param[in]   Mailbox index.  Obtained when mailbox was
 *              created.  
 * @param[in]   pointer to message to put in mailbox.  
 *
 * @details The message is added to the linked list and
 *      an evevt is triggered for the thread that owns
 *      the mailbox.
 *     
 *******************************************************/
void OS_MessageSend(uint16_t mailbox_index, void * p_envelope)
{
//printf("Sending to Mailbox %d\n",mailbox_index);
    pthread_mutex_lock(&OS_MsgMutex);
    MESSAGE_HEADER_STRUCT_PTR p_msg = (MESSAGE_HEADER_STRUCT_PTR)((uint8_t*)p_envelope - MESSAGE_HEADER_SIZE);
    if (MboxParam[mailbox_index].p_head == NULL) {
        MboxParam[mailbox_index].p_head = p_msg;
        MboxParam[mailbox_index].count = 1;
    }
    else {
        MESSAGE_HEADER_STRUCT_PTR p_mb_msg;
        p_mb_msg = (MESSAGE_HEADER_STRUCT_PTR)MboxParam[mailbox_index].p_tail;
        p_mb_msg->p_next = p_msg;
        MboxParam[mailbox_index].count++;
    }
    MboxParam[mailbox_index].p_tail = p_msg;
    p_msg->p_next = NULL;
    OS_EventSet((EVENT_STRUCT*)MboxParam[mailbox_index].event_handle, MboxParam[mailbox_index].event_bit);
    pthread_mutex_unlock(&OS_MsgMutex);
//printf("Sent to Mailbox %d\n",mailbox_index);
}

void *OS_MessageGet(uint16_t mailbox_index)
{
    MESSAGE_HEADER_STRUCT_PTR p_msg;
    pthread_mutex_lock(&OS_MsgMutex);

    if (MboxParam[mailbox_index].count == 0) {
        OS_Error(OS_ERR_MSG_QUEUE_FAIL);
    }
    p_msg = (MESSAGE_HEADER_STRUCT_PTR)MboxParam[mailbox_index].p_head;
    if (p_msg == NULL) {
//printf("Error in mailbox %d\n",mailbox_index);
        OS_Error(OS_ERR_MSG_QUEUE_MSG_FAIL);
    }
    --MboxParam[mailbox_index].count;
    if ( MboxParam[mailbox_index].count == 0) {
        OS_EventClear(MboxParam[mailbox_index].event_handle,MboxParam[mailbox_index].event_bit);
        MboxParam[mailbox_index].p_head = NULL;
        MboxParam[mailbox_index].p_tail = NULL;
    }
    else {
        MboxParam[mailbox_index].p_head = p_msg->p_next;
    }
//printf("Mailbox (%d) Messages Left = %d\n",mailbox_index, MboxParam[mailbox_index].count);
    pthread_mutex_unlock(&OS_MsgMutex);
    return (uint8_t*)p_msg + MESSAGE_HEADER_SIZE;
}

//uint16_t msg_mem_count = 0;
void OS_ReleaseMsgMemBlock(void *p_msg)
{
    MESSAGE_HEADER_STRUCT *p_env = (MESSAGE_HEADER_STRUCT *)((uint8_t*)p_msg - MESSAGE_HEADER_SIZE);
//++msg_mem_count;
//printf("Rel:%08x\n\r",(uint32_t)p_env);
    free(p_env);
//printf("MsgCnt:%d\r\n",msg_mem_count);
}

void * OS_GetMemBlock (uint16_t size)
{
    void *value = calloc(size,sizeof(uint8_t));
    if (value == 0) {
        OS_Error(OS_ERR_MEM_ALLOC_FAIL);
    }
    return value;
}

//uint16_t mem_count = 0;
void OS_ReleaseMemBlock(void *p_msg)
{
//++mem_count;
//printf("MemCnt:%d\r\n",mem_count);
    free(p_msg);
}

uint16_t OS_TimerCreate(void *event_handle, uint16_t event_bit)
{
    uint16_t num_timers;
    pthread_mutex_lock(&OS_MsgMutex);
    if( NumTimers == MAX_TIMERS) {
        OS_Error(OS_ERR_NO_TIMERS);
    }
    else {
        TimerParam[NumTimers].event_handle = event_handle;
        TimerParam[NumTimers].event_bit = event_bit;
        TimerParam[NumTimers].enabled = false;
    }
    num_timers = NumTimers;
    NumTimers++;
    pthread_mutex_unlock(&OS_MsgMutex);
    return num_timers;
}

void OS_TimerStop(uint16_t timer)
{
    TimerParam[timer].enabled = false;
}

void OS_TimerSetCyclicInterval(uint16_t timer, uint32_t msec)
{
    pthread_mutex_lock(&OS_MsgMutex);

    if (OS_TimerThreadId == 0) {
        pthread_attr_t attr;
        struct sched_param param;
        pthread_t id;

        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, OS_TIMER_STACK_SIZE);
    
        pthread_create(&id,&attr,timer_thread,NULL);
        param.sched_priority = TIMER_PRI;
        if (pthread_setschedparam(id, SCHED_FIFO, &param) != 0) {
            OS_Error(OS_ERR_THREAD_FAIL);
        }
//printf("Started Timer %d\n",timer);

        OS_TimerThreadId = id;
    }

    TimerParam[timer].enabled = true;
    TimerParam[timer].count_down = msec/OS_TIMER_RESOLUTION_MSEC;
    pthread_mutex_unlock(&OS_MsgMutex);
}

static void *timer_thread(void *temp)
{
    uint16_t idx;


    while(1) {
        OS_TaskSleep(OS_TIMER_RESOLUTION_MSEC);
//printf(".");
        for (idx = 0; idx < NumTimers; ++idx) {
//printf("-");
            if (TimerParam[idx].enabled == true) {
//printf("+");
                TimerParam[idx].count_down--;
                if (TimerParam[idx].count_down == 0) {
                    TimerParam[idx].enabled = false;
//printf("BOOM\n");
                    OS_EventSet(TimerParam[idx].event_handle, TimerParam[idx].event_bit);
                }
            }
        }
    }
    return 0;
}


void OS_GetTimeLocal(time_t *p_time)
{
    time(p_time);
//    uint32_t offset = (uint32_t)currentTimeOffset;
    *p_time += currentTimeOffset;
}

void OS_TaskSleep(uint32_t val_msec)
{
	struct timespec a_time;
	a_time.tv_sec = val_msec/1000;
	a_time.tv_nsec = 1000000L*(val_msec%1000);
	nanosleep(&a_time,NULL);
//	nanosleep((const struct timespec[]){{val_msec/1000, (1000000L)*(val_msec%1000)}}, NULL);
}

void OS_Error(uint16_t err_code)
{
    printf("OS Error = %d\n",err_code);
    while(1);
#ifdef USE_ME
    //TODO: React, based on error type
    printf("OS Error = %d\n",err_code);
    char s[MAX_TAG_LABEL_SIZE];
    sprintf(s,"OS Error = %d",err_code);
    LOG_LogEvent(s);
    RESET_HUB();
#endif
}

void OS_SetTime(time_t *p_time)
{
    stime(p_time);
}
