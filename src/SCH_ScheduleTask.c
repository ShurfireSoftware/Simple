/***************************************************************************//**
 * @file   SCH_ScheduleTask.c
 * @brief  This task allows events to be scheduled for execution. 
 * Scheduled events exist in a linked list.  The scheduler searches thru the 
 * linked list at 1 second increments and executes the entry's callback 
 * function when its time matches the current time. The entry is 
 * removed from the list.
 *
 * For scenes, the callback function executes the scene and reloads
 * itself in the scheduled events.
 * 
 * This task also is responsible for setting the system time.
 *
 * @author Neal Shurmantine
 * @copyright (c) 2015 Hunter Douglas. All rights reserved.
 *
 * @date Created: 02/13/2015
 * @date Last updated: 04/18/2015
 *
 * @version
 * 02/13/2015   Created.
 * 04/18/2015   Significant changes.
 ******************************************************************************/

/* Includes
*******************************************************************************/
#define _XOPEN_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "util.h"
#include "config.h"
#include "os.h"
#include "rf_serial_api.h"
#include "SCH_ScheduleTask.h"
#include "LOG_DataLogger.h"
#include "stub.h"
#include "RMT_RemoteServers.h"

#ifdef USE_ME
#include <mutex.h>
#include "ScheduledEventDataStorage.h"
#include "SceneProcessing.h"
#include "MultiSceneProcessing.h"
#include "SPIFlashData.h"
#include "FF_FlashFileTask.h"
#include "IO_InputOutput.h"
#include "datacheck.h"
#include "SPIFlashDataBuffer.h"
#include "SPIFlashIndexBuffer.h"
#include <watchdog.h>
#endif

/* Global Variables
*******************************************************************************/
pthread_t SCH_ScheduleTaskId;
extern int32_t currentTimeOffset;
extern float latitude;
extern float longitude;
//extern uint16_t RMT_RemoteActionToken;
extern MUTEX_STRUCT flashDeviceMutex;
extern uint16_t RMT_TimeServerTokenSeconds;
extern bool flashDataNeedsWritten;
extern bool flashVectorNeedsWritten;
extern uint16_t sunriseTimeInMinutes, sunsetTimeInMinutes;

/* Local Constants and Definitions
*******************************************************************************/
#define SCH_TICK_EVENT                          BIT0
#define SCH_NEW_SCHEDULE_EVENT                  BIT1
#define SCH_MODIFY_SCHEDULED_SCENES_EVENT       BIT2
#define SCH_TIME_CHANGE_EVENT                   BIT3
#define SCH_REMOVE_SCHEDULED_EVENT              BIT4

#define SCH_DEBUG_REQ_DAILY_LIST                BIT5

#define SCH_TICK_INTERVAL                       1000
#define SCH_SIGNIFICANT_TIME_CHANGE  60
#define SCH_NO_TIME                             0xffffffff
#ifdef ENABLE_DEBUG
#define HIGH_WATER_CHECK_TIME     (10*SEC_IN_MS)
#else
#define HIGH_WATER_CHECK_TIME     (10*MIN_IN_MS)
#endif

typedef struct SCH_EVENT_STRUCT_TAG
{
    time_t time;
    uint16_t event_id;
    bool isSceneEvent;
    bool isDailyEvent;
    int32_t count_down;
    DAY_STRUCT day;
    void(*p_callback)(uint16_t);
    struct SCH_EVENT_STRUCT_TAG *p_next;
    struct SCH_EVENT_STRUCT_TAG *p_previous;
} SCH_EVENT_STRUCT, *SCH_EVENT_STRUCT_PTR;

typedef struct SCH_EVENT_REQUEST_TAG
{
    uint16_t event_id;
    bool isSceneEvent;
    bool isDailyEvent;
    int32_t count_down;
    DAY_STRUCT day;
    void(*p_callback)(uint16_t);
} SCH_EVENT_REQUEST, *SCH_EVENT_REQUEST_PTR;

#define SCH_WATCHDOG_INTERVAL                       (20*SEC_IN_MS)

/* Local Function Declarations
*******************************************************************************/
static void SCH_set_default_time(void);
static void SCH_handle_time_change(void);
static void SCH_handle_new_schedule_request(void);
static void SCH_new_time_set(time_t * p_new_time);
static uint16_t SCH_send_new_schedule_event(SCH_EVENT_REQUEST_PTR p_req, 
                                    void(*p_callback)(uint16_t));
static void SCH_monitor_schedule_list(void);
static void SCH_refresh_scene_events(void);
static strScheduledEvent * SCH_find_scheduled_event_data(uint16_t event_id);
static void SCH_remove_all_scene_events(void);
static void SCH_remove_event(SCH_EVENT_STRUCT_PTR p_rec);
static void SCH_add_record_to_list(SCH_EVENT_STRUCT_PTR p_event_rec);
static bool SCH_compute_scene_event(strScheduledEvent *sched_event, time_t * p_when);
static void SCH_execute_scene_now(uint16_t event_id);
static bool SCH_is_happening_today(DAY_STRUCT_PTR p_day, time_t * p_when);
static bool SCH_check_sunrise_sunset(bool is_sunrise, 
                                    int16_t minute, 
                                    time_t * p_when);
static bool SCH_is_event_enabled_today(strScheduledEvent *p_event);
static void SCH_handle_midnight(uint16_t unused);
static void SCH_schedule_midnight(void);
static void SCH_remove_event_at_token(uint16_t token);
static void SCH_handle_remove_event_at_token(void);
static void SCH_debug_handle_event_request(void);
static void SCH_re_schedule_midnight(void);
static void SCH_schedule_scene_refresh(bool remove, uint32_t countdown);
static bool SCH_is_time_change_significant(void);
static void SCH_commit_to_flash_if_necessary(void);

/* Local variables
*******************************************************************************/
static uint16_t SCH_NewScheduleMbox;
static uint16_t SCH_RemoveEventMbox;
static uint32_t SCH_WaitTime;
static uint16_t SCH_TickTimer;
static uint16_t SCH_ExpectedEvents;
static void *SCH_EventHandle;
static uint16_t SCH_MidnightHandle;
static uint16_t SCH_RefreshHandle=NULL_TOKEN;
static uint16_t SCH_TokenCounter = 1;
static SCH_EVENT_STRUCT_PTR p_HeadEvent;
static SCH_EVENT_STRUCT_PTR p_TailEvent;
static bool sunrise_sunset_found;
static int32_t SCH_TimeZoneOffset;
static bool SCH_IsTimeSet;
static ALL_RAW_DB_STR_PTR SCH_pScheduleEventList = NULL;
static uint32_t SCH_HTTPActiveCount = SCH_HTTP_ACTIVE_MAX_COUNT;
static bool SCH_AppTimeChange;
static time_t SCH_OldTime;
static time_t SCH_NewTime;

//>>>>>>>>>>>>>>>>>TEST CODE>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

#ifdef DEBUG_SCHEDULE_TASK
static char test_log[MAX_TAG_LABEL_SIZE];
void sch_test_log(char * p_tag, uint16_t token)
{
    char t[MAX_TAG_LABEL_SIZE];
    strcpy(t,p_tag);
    strcat(t, " :%d");
    sprintf(test_log,t,token);
LOG_LogEvent(test_log);
}

void sch_test_func_return(uint16_t token)
{
    sch_test_log("Expired", token);
}

void sch_test_func(uint16_t unused)
{
    uint16_t token;
    token = SCH_ScheduleEventPostSeconds(60, sch_test_func_return);
    sch_test_log("60 sec", token);
printf("sch_test_func - ");
SCH_DisplayCurrentTime();
    token = SCH_ScheduleEventPostSeconds(120, sch_test_func_return);
    sch_test_log("120 sec", token);
    token = SCH_ScheduleDaily(5,30,sch_test_func_return);
    sch_test_log("5:30", token);
    token = SCH_ScheduleDaily(7,30,sch_test_func_return);
    sch_test_log("7:30", token);
    token = SCH_ScheduleDaily(13,30,sch_test_func_return);
    sch_test_log("13:30", token);
    token = SCH_ScheduleDaily(23,30,sch_test_func_return);
    sch_test_log("23:30", token);
}

void SCH_ScheduleTaskTest(void)
{
    uint16_t token = SCH_ScheduleEventPostSeconds(2, sch_test_func);
    printf("scheduletaskinit - %d -",token);
    SCH_DisplayCurrentTime();
}
#endif

//<<<<<<<<<<<<<<<<End TEST CODE<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

/*****************************************************************************//**
* @brief Initialize the Scheduler task.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 02/13/2015    Created.
*******************************************************************************/
void SCH_ScheduleTaskInit(void)
{
    p_HeadEvent = NULL;
    p_TailEvent = NULL;
    SCH_ScheduleTaskId = OS_TaskCreate(SCH_SCHEDULE_TASK_NUM, 0);
    SCH_set_default_time();
    if (IO_IsSelfTestActive() == false) {
        SCH_schedule_midnight();
        SC_ScheduleBatteryCheck();
#ifdef USE_ME
        _task_create(0, HIGHWATER_TASK_NUM, 0);
#endif
    }
}

#ifdef USE_ME
void DIA_highwater_task(uint32_t temp)
{
    uint32_t * p_highwater;
    uint32_t * p_last_highwater = 0;
    LOG_LogEvent("Start Log");
    char t[MAX_TAG_LABEL_SIZE];

    while(1) {
        OS_TaskSleep(HIGH_WATER_CHECK_TIME);
        OS_SchedLock();
        p_highwater = _mem_get_highwater();
        if (p_highwater > p_last_highwater) {
            sprintf(t,"New highwater: %08x",p_highwater);
            SCH_DisplayCurrentTime();
            printf("%s\n",t);
            LOG_LogEvent(t);
            p_last_highwater = p_highwater;
        }
        OS_SchedUnlock();
        if (p_highwater > (uint32_t*)0x2002e000) {
            RESET_HUB();
        }
    }
}
#endif

/*****************************************************************************//**
* @brief Called at initialization. Keeps a local copy of current time offset
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 05/16/2015    Created.
*******************************************************************************/
void SCH_InitTimeVariables(void)
{
    SCH_TimeZoneOffset = currentTimeOffset;
}

/*****************************************************************************//**
* @brief Set default time to Jan 1, 2015 at midnight.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 04/18/2015    Created.
* 09/24/2015   Restore time from flash if present.
*******************************************************************************/
static void SCH_set_default_time(void)
{
    struct tm date;
    time_t time;
    uint16_t hour;
    uint16_t minute;
    bool clear_flash_time = false;
    readRestartTimeFromFlash(&time);
    printf("\nLast Time = %08X\n",(uint32_t)time);
    printf("Latitude= %1.3f\n",latitude);
    printf("Longitude= %1.3f\n",longitude);

    sunrise_sunset_found = true;
    hour = sunriseTimeInMinutes / 60;
    minute = sunriseTimeInMinutes % 60;
    if ((hour < 23) && (hour+minute>0)) {
        printf("Sunrise=%02d:%02d\n",hour,minute);
    }
    else {
        printf("Sunrise not set\n");
        sunrise_sunset_found = false;
    }

    hour = sunsetTimeInMinutes / 60;
    minute = sunsetTimeInMinutes % 60;
    if ((hour < 23) && (hour+minute>0)) {
        printf("Sunset=%02d:%02d\n\n",hour,minute);
    }
    else {
        printf("Sunset not set\n");
        sunrise_sunset_found = false;
    }

    SCH_IsTimeSet = false;
    clear_flash_time = true;
    if ((time != SCH_NO_TIME) && time) {
        SCH_new_time_set(&time);
    }
    else {
        printf("Not Daily Reset\n");
        strptime("1 Jan 2016 12:00:00", "%d %b %Y %H:%M:%S", &date);
        date.tm_isdst = -1; //tells mktime() to determine if DST
        time = mktime(&date);
        OS_SetTime(&time);
    }

    SCH_DisplayCurrentTime();

    if (clear_flash_time == true) {
        //clear the flash time
        //  it is only set on scheduled reset
        time = SCH_NO_TIME;
        writeRestartTimeToFlash(&time);
        SCH_commit_to_flash_if_necessary();
    }
}

/*****************************************************************************//**
* @brief A PUT has been received from the App to set time.  Schedule a check of
*   the remote time server to refresh sunrise/sunset.  Change real time clock.
*   If time changed more than one minute then refresh scheduled events.
*NOTE:  If time offset is changed along with the UTC time then the offset should
*    be changed first.
*
* @param p_new_time. UTC Time
* @return nothing.
* @author Neal Shurmantine
* @version
* 04/26/2015    Created.
* 05/15/2015   Major changes
*******************************************************************************/
void SCH_SetTime(time_t * p_new_time,int32_t timezone_offset)
{
    SCH_AppTimeChange = true;
    OS_GetTimeLocal(&SCH_OldTime);
    SCH_new_time_set(p_new_time);
    if (RMT_TimeServerTokenSeconds != NULL_TOKEN) {
        SCH_RemoveScheduledEvent(RMT_TimeServerTokenSeconds);
    }
    RMT_ScheduleTimeServerCheckInSeconds(SCH_HTTP_ACTIVE_MAX_COUNT);
    LOG_LogEvent("Time From App");
}

/*****************************************************************************//**
* @brief A valid response was received from the remote time server. Recover
*   sunrise and sunset times.  Refresh scheduler.
*NOTE:  If time offset is changed along with the UTC time then the offset should
*    be changed first.
*
* @param p_new_time. UTC Time
* @return nothing.
* @author Neal Shurmantine
* @version
* 05/15/2015    Created.
*******************************************************************************/
void SCH_ProcessNewTime(TIME_UPDATE_DATA_PTR p_time_data)
{
    time_t time;
    int32_t offset;
    bool offset_changed;
    OS_GetTimeLocal(&SCH_OldTime);
    offset = p_time_data->raw_offset + p_time_data->dst_offset;
    offset /= 1000;
    currentTimeOffset = offset;
    offset_changed = (currentTimeOffset != SCH_TimeZoneOffset);
    if ( (latitude !=0) && (longitude != 0) ) {
        sunrise_sunset_found = true;
        time = mktime(&p_time_data->sunrise);
        time += currentTimeOffset;
		writeSunriseToFlash(&time);
        printf("Sunrise = ");
        SCH_DisplayTime(&time);

        time = mktime(&p_time_data->sunset);
        time += currentTimeOffset;
		writeSunsetToFlash(&time);
        printf("Sunset = ");
        SCH_DisplayTime(&time);
    }

    time = mktime(&p_time_data->cur_time);
    SCH_AppTimeChange = false;
    SCH_new_time_set(&time);
    LOG_LogEvent("Time From Remote");
    if (offset_changed == true) {
        setLocalTimeOffset(currentTimeOffset);
    }
    SCH_DisplayCurrentTime();
    SCH_commit_to_flash_if_necessary();
}

/*****************************************************************************//**
* @brief Checks to see if there is a pending change in the database that must
*     be written to flash.  If yse then write to flash and verify.
*
* @param none
* @return nothing.
* @author Neal Shurmantine
* @version
* 09/24/2015    Created.
*******************************************************************************/
static void SCH_commit_to_flash_if_necessary(void)
{
    bool maintain_flash = false;
    if (flashDataNeedsWritten == true) {
        writeDataBufferToCurrentSector();
        maintain_flash = true;
    }
    if (flashVectorNeedsWritten == true) {
        writeVectorBufferToCurrentSector();
        maintain_flash = true;
    }
    if (maintain_flash==true) {
        maintainFlash(1);
    }
}

/*****************************************************************************//**
* @brief The real time clock has been changed.  
*
* Note: This function runs in the context of the calling task.
* @param p_new_time. time_t * to UTC time that was just set
* @return nothing.
* @author Neal Shurmantine
* @version
* 04/18/2015    Created.
*******************************************************************************/
void SCH_new_time_set(time_t * p_new_time)
{
	OS_SetTime(p_new_time);
    OS_EventSet(SCH_EventHandle, SCH_TIME_CHANGE_EVENT);
}

/*****************************************************************************//**
* @brief Schedule an event to occur after a given number of seconds.
*
* Note: This function runs in the context of the calling task.
* @param sec Number of seconds from now before the callback function is executed.
* @param p_callback is a pointer to a function that is called when event occurs.
* @return uint16_t event_id that will be passed as a parameter to the callback function.
* @author Neal Shurmantine
* @version
* 03/04/2015    Created.
*******************************************************************************/
uint16_t SCH_ScheduleEventPostSeconds(uint32_t sec, void(*p_callback)(uint16_t))
{
    SCH_EVENT_REQUEST_PTR p_req = (SCH_EVENT_REQUEST_PTR)OS_GetMsgMemBlock(sizeof(SCH_EVENT_REQUEST));
    p_req->isSceneEvent = false;
    p_req->isDailyEvent = false;
    p_req->count_down =(int32_t)sec;

    return SCH_send_new_schedule_event(p_req, p_callback);
}

/*****************************************************************************//**
* @brief Schedule an event to occur at a set hour and minute every day.
*
* Note: This function runs in the context of the calling task.
* @param p_day, pointer to a DAY_STRUCT (time of day for event to occur).
* @param p_callback is a pointer to a function that is called when event occurs.
* @return uint16_t event_id that will be passed as a parameter to the callback function.
* @author Neal Shurmantine
* @version
* 03/04/2015    Created.
*******************************************************************************/
uint16_t SCH_ScheduleDaily(DAY_STRUCT_PTR p_day, void(*p_callback)(uint16_t))
{
    SCH_EVENT_REQUEST_PTR p_req = (SCH_EVENT_REQUEST_PTR)OS_GetMsgMemBlock(sizeof(SCH_EVENT_REQUEST));
    p_req->isDailyEvent = true;
    p_req->isSceneEvent = false;
    memcpy(&p_req->day, p_day, sizeof(DAY_STRUCT));
    return SCH_send_new_schedule_event(p_req, p_callback);
}

/*****************************************************************************//**
* @brief Takes a partially filled SCH_EVENT_REQUEST structure, completes it
*    and sends message to schedule task.
*
* @param p_req, pointer to SCH_EVENT_REQUEST.
* @param p_callback is a pointer to a function that is called when event occurs.
* @return uint16_t event_id that will be passed as a parameter to the callback function.
* @author Neal Shurmantine
* @version
* 06/04/2015    Created.
*******************************************************************************/
static uint16_t SCH_send_new_schedule_event(SCH_EVENT_REQUEST_PTR p_req, 
                                    void(*p_callback)(uint16_t))
{
    OS_SchedLock();
    p_req->event_id = SCH_TokenCounter++;
    if (SCH_TokenCounter == NULL_TOKEN) {
        SCH_TokenCounter = 1;
    }
    OS_SchedUnlock();
    p_req->p_callback = p_callback;
    uint16_t id = p_req->event_id;
    OS_MessageSend(SCH_NewScheduleMbox,p_req);
    return id;
}

/*****************************************************************************//**
* @brief This function is called whenever there is a change to the scene database.
*
* Note: This function runs in the context of the calling task.
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 04/19/2015    Created.
*******************************************************************************/
void SCH_ModifyScheduledScenes(void)
{
    OS_EventSet(SCH_EventHandle, SCH_MODIFY_SCHEDULED_SCENES_EVENT);
}

/*****************************************************************************//**
* @brief This is a debug function that prints the expiration time of events
*    on the scheduler.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 06/04/2015    Created.
*******************************************************************************/
void SCH_DebugRequestScheduleList(void)
{
    OS_EventSet(SCH_EventHandle, SCH_DEBUG_REQ_DAILY_LIST);
}

/*****************************************************************************//**
* @brief This function is called to remove an event, identified by the token,
*  for the scheduled events list.
*
* Note: This function runs in the context of the calling task.
* @param token.
* @return nothing.
* @author Neal Shurmantine
* @version
* 04/19/2015    Created.
*******************************************************************************/
void SCH_RemoveScheduledEvent(uint16_t token)
{
    uint16_t * p_event_token = (uint16_t*)OS_GetMsgMemBlock(sizeof(uint16_t));
    *p_event_token = token;
    OS_MessageSend(SCH_RemoveEventMbox,p_event_token);
}

/*****************************************************************************//**
* @brief Display the current time on the terminal.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 04/19/2015    Created.
*******************************************************************************/
void SCH_DisplayCurrentTime(void)
{
    time_t now;
    OS_GetTimeLocal(&now);
    printf("Current Local Time:  ");
    SCH_DisplayTime(&now);
}

/*****************************************************************************//**
* @brief Display the passed-in time on the terminal.
*
* @param p_time. time_t * containing time to display
* @return nothing.
* @author Neal Shurmantine
* @version
* 05/01/2015    Created.
*******************************************************************************/
void SCH_DisplayTime(time_t * p_time)
{
    char time_str[TIME_STRING_MAX_LENGTH];
    SCH_MakeTimeString(time_str,p_time);
    printf("%s\n",time_str);
}

/*****************************************************************************//**
* @brief Create a time string.
*
* @param p_time_str. pointer to character array to be filled.  Size must be >= 20.
* @param p_time. time_t * containing time to convert.
* @return nothing.
* @author Neal Shurmantine
* @version
* 05/01/2015    Created.
*******************************************************************************/
void SCH_MakeTimeString(char * p_time_str, time_t * p_time)
{
    struct tm date_time;
    localtime_r(p_time, &date_time);
    sprintf(p_time_str, "%04d/%02d/%02d %02d:%02d:%02d",
            date_time.tm_year+1900, date_time.tm_mon+1, date_time.tm_mday,
            date_time.tm_hour, date_time.tm_min, date_time.tm_sec);
}

/*****************************************************************************//**
* @brief This function is used to determine if enough time has elapsed since
*    the last REST request was received from the app to do potentially
*    conflicting operations with the HTTP task such as contact with remote
*    server or scene scheduling.
*
* Note: This function may run in the context of another task.
* @param none.
* @return bool. True if there has not been much time since last user interaction
*               via the app.
* @author Neal Shurmantine
* @version
* 08/06/2015    Created.
*******************************************************************************/
bool SCH_IsHTTPActive(void)
{
    bool rtn;
    OS_SchedLock();
    if (SCH_HTTPActiveCount==SCH_HTTP_ACTIVE_MAX_COUNT) rtn = false;
    else rtn = true;
    OS_SchedUnlock();
    return rtn;
}

/*****************************************************************************//**
* @brief This function is called each time there is a REST request from the
*    app.  It resets a one second counter that is used to block out remote
*    server requests and scene rescheduling.
*
* Note: This function runs in the context of another task.
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 08/06/2015    Created.
*******************************************************************************/
void SCH_ResetHTTPActiveCount(void)
{
    OS_SchedLock();
    SCH_HTTPActiveCount = 0;
    OS_SchedUnlock();
}

/*****************************************************************************//**
* @brief This function loads the scheduler with a function that runs at midnight.
*
* Note: This function runs in the context of another task.
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 04/23/2015    Created.
*******************************************************************************/
static void SCH_schedule_midnight(void)
{
    DAY_STRUCT day;
    day.hour = 0;
    day.minute = 0;
    day.second = 0;
    SCH_MidnightHandle = SCH_ScheduleDaily(&day, SCH_handle_midnight);
}

/*****************************************************************************//**
* @brief This function is the task that handles the scheduling of events.
*
* @param temp is a unused variable.
* @return nothing.
* @author Neal Shurmantine
* @version
* 02/13/2015    Created.
* 02/17/2015    Renamed.
*******************************************************************************/
void *SCH_schedule_task(void *Temp)
{
    //holds the event bits that were set when the task wakes up
    uint16_t event_active;
    
    //the task will wake up on this timeout if no events have occurred.
    SCH_WaitTime = WAIT_TIME_INFINITE;

    //create the event for this task plus mailboxes and timer
    SCH_EventHandle = OS_EventCreate(0,false);
    SCH_TickTimer = OS_TimerCreate(SCH_EventHandle, SCH_TICK_EVENT);
    SCH_NewScheduleMbox = OS_MboxCreate(SCH_EventHandle,SCH_NEW_SCHEDULE_EVENT); 
    SCH_RemoveEventMbox = OS_MboxCreate(SCH_EventHandle,SCH_REMOVE_SCHEDULED_EVENT); 

    OS_TimerSetCyclicInterval(SCH_TickTimer,SCH_TICK_INTERVAL);

    SCH_ExpectedEvents = SCH_TICK_EVENT
                 | SCH_NEW_SCHEDULE_EVENT
                 | SCH_MODIFY_SCHEDULED_SCENES_EVENT
                 | SCH_TIME_CHANGE_EVENT
                 | SCH_REMOVE_SCHEDULED_EVENT
                 | SCH_DEBUG_REQ_DAILY_LIST;

printf("SCH_schedule_task\n");
    while(1) {
        event_active = OS_TaskWaitEvents(SCH_EventHandle, SCH_ExpectedEvents, SCH_WaitTime);
        event_active &= SCH_ExpectedEvents;
        if (event_active & SCH_TIME_CHANGE_EVENT) {
            OS_EventClear(SCH_EventHandle,SCH_TIME_CHANGE_EVENT);
            SCH_handle_time_change();
        }
        if (event_active & SCH_REMOVE_SCHEDULED_EVENT) {
            SCH_handle_remove_event_at_token();
        }
        if (event_active & SCH_NEW_SCHEDULE_EVENT) {
            SCH_handle_new_schedule_request();
        }
        if (event_active & SCH_MODIFY_SCHEDULED_SCENES_EVENT) {
            OS_EventClear(SCH_EventHandle,SCH_MODIFY_SCHEDULED_SCENES_EVENT);
            SCH_schedule_scene_refresh(true,SCH_HTTP_ACTIVE_MAX_COUNT);
        }
        if (event_active & SCH_DEBUG_REQ_DAILY_LIST) {
            OS_EventClear(SCH_EventHandle, SCH_DEBUG_REQ_DAILY_LIST);
            SCH_debug_handle_event_request();
        }
        if (event_active & SCH_TICK_EVENT)
        {
            OS_EventClear(SCH_EventHandle,SCH_TICK_EVENT);
            if (SCH_HTTPActiveCount < SCH_HTTP_ACTIVE_MAX_COUNT) {
                ++SCH_HTTPActiveCount;
            }
            SCH_monitor_schedule_list();
            _watchdog_start(SCH_WATCHDOG_INTERVAL);
            OS_TimerSetCyclicInterval(SCH_TickTimer,SCH_TICK_INTERVAL);
        }
    }
}

/*****************************************************************************//**
* @brief Called from within context of SCH_ScheduleTask.  Displays the expiration
*     time of each event on the schedule list.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 06/04/2015    Created.
*******************************************************************************/
static void SCH_debug_handle_event_request(void)
{
    SCH_EVENT_STRUCT_PTR p_event_rec = p_HeadEvent;
    struct tm date;

    while(p_event_rec != NULL) {
        if (p_event_rec->isDailyEvent == true) {
            localtime_r(&p_event_rec->time,&date);
            if (p_event_rec->isSceneEvent == true) {
                printf("Scene %02d-%02d ", date.tm_mon+1,date.tm_mday);
            }
            else {
                printf("Daily %02d-%02d ", date.tm_mon+1,date.tm_mday);
            }
            printf("%02d:%02d:%02d\n",date.tm_hour,date.tm_min,date.tm_sec);
        }
        else {
            printf("Count down %d sec\n",p_event_rec->count_down);
        }
        p_event_rec = p_event_rec->p_next;
    }
}

/*****************************************************************************//**
* @brief Called from within context of SCH_ScheduleTask.  Removes an event with
*   a given token value.
*
* @param token.
* @return nothing.
* @author Neal Shurmantine
* @version
* 04/23/2015    Created.
*******************************************************************************/
static void SCH_remove_event_at_token(uint16_t token)
{
    SCH_EVENT_STRUCT_PTR p_event_rec = p_HeadEvent;
    while(p_event_rec != NULL) {
        if ((p_event_rec->isSceneEvent == false) &&
                (p_event_rec->event_id == token) ) {
            SCH_remove_event(p_event_rec);
            break;
        }
        p_event_rec = p_event_rec->p_next;
    }
}

/*****************************************************************************//**
* @brief Called at midnight.  It is a chance to program the scheduled scenes for
*  the day.
*
* @param unused. Required parameter for scheduler callbacks.
* @return nothing.
* @author Neal Shurmantine
* @version
* 04/23/2015    Created.
*******************************************************************************/
static void SCH_handle_midnight(uint16_t unused)
{
    SCH_re_schedule_midnight();
    if (SCH_IsTimeSet == true) {
        SCH_schedule_scene_refresh(true,10);
    }
}

/*****************************************************************************//**
* @brief Reschedule the midnight task from within the schedule task;
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 08/05/2015    Created.
*******************************************************************************/
static void SCH_re_schedule_midnight(void)
{
    SCH_EVENT_STRUCT_PTR p_event_rec;

    //get memory for a new record (freed when alarm occurs)
    p_event_rec = (SCH_EVENT_STRUCT_PTR)OS_GetMemBlock(sizeof(SCH_EVENT_STRUCT));
    SCH_add_record_to_list(p_event_rec);

    p_event_rec->p_callback = SCH_handle_midnight;
    p_event_rec->isSceneEvent = false;
    p_event_rec->isDailyEvent = true;

    OS_SchedLock();
    p_event_rec->event_id = SCH_TokenCounter++;
    if (SCH_TokenCounter == NULL_TOKEN) {
        SCH_TokenCounter = 1;
    }
    OS_SchedUnlock();
    SCH_MidnightHandle = p_event_rec->event_id;
    p_event_rec->day.hour = 0;
    p_event_rec->day.minute = 0;
    p_event_rec->day.second = 0;
    SCH_is_happening_today(&p_event_rec->day, &p_event_rec->time);
}

/*****************************************************************************//**
* @brief Another task has requested that a previously scheduled event, that has not
*  been executed, be removed from the schedule list.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 04/23/2015    Created.
*******************************************************************************/
static void SCH_handle_remove_event_at_token(void)
{
    uint16_t * p_token = (uint16_t *)OS_MessageGet(SCH_RemoveEventMbox);
    SCH_remove_event_at_token(*p_token);
    OS_ReleaseMsgMemBlock((uint8_t *)p_token);
}

/*****************************************************************************//**
* @brief This is a separate task that goes to sleep until until sometime between
*    12:00 and 1:00 AM.  When it wakes up, the hub is reset.  The minutes and
*    seconds after midnight is the same minutes and seconds that this task
*    is started in order for there to be randomization in the reset time (and
*    the time that the hub checks for firmware following reset.
*
* @param uint32_t.  Unused, required for task creation.
* @return nothing.

* @author Neal Shurmantine
* @version
* 08/06/2015    Created.
*******************************************************************************/
void reset_task(uint32_t temp)
{
    time_t now;
    time_t now1;
    struct tm date;
    uint32_t min;
    uint32_t sec;
    uint32_t now_seconds;
    uint32_t diff;
    printf("Begin reset countdown\n");
    OS_GetTimeLocal(&now);
    
    now_seconds = now;

    localtime_r(&now, &date);

    //save current minute and second
    // task will fire at this minute and
    // second after midnight
    min = date.tm_min;
    sec = date.tm_sec;

    //find time of midnight today
    date.tm_hour = 0;
    date.tm_min = 0;
    date.tm_sec = 0;
    now = mktime(&date);
    //add a full day
    now += DAY_IN_SEC;
    localtime_r(&now,&date);
    //find time to reset tomorrow
    //use current minutes and seconds
    //  for randomization
    date.tm_hour = 0;
    date.tm_min = min;
    date.tm_sec = sec;
    now = mktime(&date);
    //calculate how many seconds until reset
    diff = (now - now_seconds);
    //convert to milliseconds for task sleep function
    diff *= SEC_IN_MS;

//diff = 181000;
    OS_TaskSleep(diff);
    LOG_LogEvent("Resetting...");

    printf("Saving time to flash\n");
    time(&now1);
    writeRestartTimeToFlash(&now1);
    SCH_commit_to_flash_if_necessary();

    //get current time, log and display
    OS_GetTimeLocal(&now);
    char time_str[TIME_STRING_MAX_LENGTH];
    SCH_MakeTimeString(time_str,&now);
    printf("%s\n",time_str);
    //sleep long enough for display to show up
    OS_TaskSleep(200);
    //reset the hub
    RESET_HUB();
}

/*****************************************************************************//**
* @brief This function executes when the timer expires requiring the scheduled
*   scenes to be refreshed.  If the HTTP task has been busy recently then
*   reschedule.
*
* @param none.
* @return nothing.

* @author Neal Shurmantine
* @version
* 08/06/2015    Created.
*******************************************************************************/
static void SCH_handle_scene_refresh_expire(uint16_t unused)
{
    SCH_RefreshHandle = NULL_TOKEN;
    if (SCH_HTTPActiveCount < SCH_HTTP_ACTIVE_MAX_COUNT) {
        SCH_schedule_scene_refresh(false,10);
    }
    else {
        SCH_refresh_scene_events();
    }
}                 

/*****************************************************************************//**
* @brief This task is used to schedule an event that will refresh the scheduled
*      scenes.  
*
* @param remove.  Boolean that, if true, removes all currently scheduled scenes.
* @return nothing.

* @author Neal Shurmantine
* @version
* 08/06/2015    Created.
*******************************************************************************/
static void SCH_schedule_scene_refresh(bool remove, uint32_t countdown)
{
    if (remove == true) {
        SCH_remove_all_scene_events();
    }
    if (SCH_RefreshHandle != NULL_TOKEN) {
        SCH_remove_event_at_token(SCH_RefreshHandle);
    }
    SCH_RefreshHandle = SCH_ScheduleEventPostSeconds(countdown,SCH_handle_scene_refresh_expire);
}

/*****************************************************************************//**
* @brief This function handles a time change event.  The scheduled scene list
*     is set to be refreshed after a period of time.  If this is the first time
*     that the time has been set then the hub is programmed to reset early then
*     next morning.  All the daily events on the schedule that are not scheduled
*     scenes are immediately refreshed.  The midnight event is also scheduled
*     although this may go away.
*
* @param none.
* @return nothing.

* @author Neal Shurmantine
* @version
* 04/18/2015    Created.
*******************************************************************************/
static void SCH_handle_time_change(void)
{
    static bool sunrise_sunset_prog = false;
    if ((SCH_is_time_change_significant() == true) || (sunrise_sunset_prog == false) ) {
        if (SCH_AppTimeChange == true) {
            SCH_schedule_scene_refresh(true,SCH_HTTP_ACTIVE_MAX_COUNT);
        }
        else {
            SCH_schedule_scene_refresh(true,10);
        }
        if (sunrise_sunset_found == true) {
            sunrise_sunset_prog = true;
        }
    }
    if (SCH_IsTimeSet == false) {
#ifdef USE_ME
        OS_TaskCreate(RESET_TASK_NUM, 0);
#endif
//        AWS_BeginConnection();
        SCH_IsTimeSet = true;
    }
    SCH_EVENT_STRUCT_PTR p_event_rec = p_HeadEvent;
    while(p_event_rec != NULL) {
        if (p_event_rec->isSceneEvent == false) {
            if (p_event_rec->isDailyEvent == true) {
                SCH_is_happening_today(&p_event_rec->day, &p_event_rec->time);
            }
        }
        p_event_rec = p_event_rec->p_next;
    }
    SCH_remove_event_at_token(SCH_MidnightHandle);
    SCH_re_schedule_midnight();
}                 

/*****************************************************************************//**
* @brief Determine if the hubs current time is more than 60 seconds different
*    than the time it received from the app or remote connect.  Return true
*    if time has changed more than 60 seconds.
*
* @param none. 
* @return bool.
* @author Neal Shurmantine
* @version
* 08/09/2015    Created.
*******************************************************************************/
static bool SCH_is_time_change_significant(void)
{
    OS_GetTimeLocal(&SCH_NewTime);
    int32_t diff = SCH_OldTime - SCH_NewTime;
    if ( (diff > SCH_SIGNIFICANT_TIME_CHANGE) || (diff < -SCH_SIGNIFICANT_TIME_CHANGE) )
	{
    	printf("Time change is significant\n");
        return true;
	}
    else
	{
    	printf("Time change is not significant\n");
        return false;
	}
}

/*****************************************************************************//**
* @brief Put new scheduled event in the linked list of scheduled events.
*
* @param none. New event request is in the mailbox.
* @return nothing.
* @author Neal Shurmantine
* @version
* 03/04/2015    Created.
*******************************************************************************/
static void SCH_handle_new_schedule_request(void)
{
    SCH_EVENT_REQUEST_PTR p_req = (SCH_EVENT_REQUEST_PTR)OS_MessageGet(SCH_NewScheduleMbox);
    SCH_EVENT_STRUCT_PTR p_event_rec;

    //get memory for a new record (freed when alarm occurs)
    p_event_rec = (SCH_EVENT_STRUCT_PTR)OS_GetMemBlock(sizeof(SCH_EVENT_STRUCT));
    SCH_add_record_to_list(p_event_rec);
    p_event_rec->p_callback = p_req->p_callback;
    p_event_rec->isSceneEvent = p_req->isSceneEvent;
    p_event_rec->isDailyEvent = p_req->isDailyEvent;
    p_event_rec->count_down = p_req->count_down;
    p_event_rec->event_id = p_req->event_id;
    if (p_req->isDailyEvent == true) {
        memcpy(&p_event_rec->day, &p_req->day,sizeof(DAY_STRUCT));
        SCH_is_happening_today(&p_req->day, &p_event_rec->time);
    }
    OS_ReleaseMsgMemBlock((uint8_t *)p_req);
}

/*****************************************************************************//**
* @brief Adds a new event record to the linked list by modifying the next 
*   and previous pointers in its record and modifying those pointers
*   in adjacent records as needed.
*
* @param p_event_rec. Pointer to new event record to be added.
* @return nothing.
* @author Neal Shurmantine
* @version
* 04/18/2015    Created.
*******************************************************************************/
static void SCH_add_record_to_list(SCH_EVENT_STRUCT_PTR p_event_rec)
{
    SCH_EVENT_STRUCT_PTR p_last_rec;
    //if no records in the list
    if (p_HeadEvent == NULL)
    {
        //create list with this record only
        p_HeadEvent = p_event_rec;
        p_TailEvent = p_event_rec;
        p_event_rec->p_next = NULL;
        p_event_rec->p_previous = NULL;
    }
    else
    {
        //add this record to the end of the list
        p_last_rec = p_TailEvent;
        p_last_rec->p_next = p_event_rec;
        p_event_rec->p_previous = p_last_rec;
        p_TailEvent = p_event_rec;
        p_event_rec->p_next = NULL;
    }
    p_event_rec->p_next = NULL;
}

/*****************************************************************************//**
* @brief Go through schedule list, looking for any expired alarms. If an alarm is
*        expired then execute its callback function and remove from list.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 03/04/2015    Created.
*******************************************************************************/
static void SCH_monitor_schedule_list(void)
{
    time_t now;
    SCH_EVENT_STRUCT_PTR p_event_rec = p_HeadEvent;
    SCH_EVENT_STRUCT_PTR p_temp;
    OS_GetTimeLocal(&now);
    while(p_event_rec != NULL) {
        p_temp = p_event_rec->p_next;
        if (p_event_rec->isDailyEvent == false) {
            if (--p_event_rec->count_down <= 0) {
                (*p_event_rec->p_callback)(p_event_rec->event_id);
                SCH_remove_event(p_event_rec);
            }
        }
        else if (now >= p_event_rec->time) {
            (*p_event_rec->p_callback)(p_event_rec->event_id);
            SCH_remove_event(p_event_rec);
        }
        p_event_rec = p_temp;
    }
}

/*****************************************************************************//**
* @brief Remove a specific event from the linked list of events and release
*   memory.
*
* @param p_rec.  Pointer to record to be removed.
* @return nothing.
* @author Neal Shurmantine
* @version
* 03/04/2015    Created.
*******************************************************************************/
static void SCH_remove_event(SCH_EVENT_STRUCT_PTR p_rec)
{
    SCH_EVENT_STRUCT_PTR p_prev_list;
    SCH_EVENT_STRUCT_PTR p_next_list;

    //remove from list of pending msgs
    if ((p_HeadEvent != p_rec) && (p_TailEvent != p_rec))
    {
        p_prev_list = p_rec->p_previous;
        p_next_list = p_rec->p_next;
        p_prev_list->p_next = p_next_list;
        p_next_list->p_previous = p_prev_list;
    }
    else
    {
        if (p_HeadEvent == p_rec)
        {
            p_HeadEvent = p_rec->p_next;
            p_next_list = p_rec->p_next;
            if (p_next_list != NULL)
            {
                p_next_list->p_previous = NULL;
            }
        }
        if (p_TailEvent == p_rec)
        {
            p_TailEvent = p_rec->p_previous;
            p_prev_list = p_rec->p_previous;
            if (p_prev_list != NULL)
            {
                p_prev_list->p_next = NULL;
            }
        }
    }
    OS_ReleaseMemBlock((uint8_t *)p_rec);
}

/*****************************************************************************//**
* @brief Remove all scheduled scene events from the linked list of 
*    events.  Memory is released also.
*
* @param event_id.  Scheduled scene event to be removed.
* @return nothing.
* @author Neal Shurmantine
* @version
* 04/18/2015    Created.
*******************************************************************************/
static void SCH_remove_all_scene_events(void)
{
    SCH_EVENT_STRUCT_PTR p_next;
    SCH_EVENT_STRUCT_PTR p_event_rec = p_HeadEvent;
    while (p_event_rec != NULL) {
        p_next = p_event_rec->p_next;
        if (p_event_rec->isSceneEvent == true) {
            SCH_remove_event(p_event_rec);
        }
        p_event_rec = p_next;
    }
    if (SCH_pScheduleEventList != NULL) {
        OS_ReleaseMemBlock((void *)SCH_pScheduleEventList);
        SCH_pScheduleEventList = NULL;
    }
}

/*****************************************************************************//**
* @brief Scheduled scene database has changed or time has changed.  Each 
*     scheduled scene event is removed from the list.  The scheduled scene 
*     event database is then checked, one entry at a time, and enabled events
*   are added to the list of scheduled events.
*
* @param token.  If this is a callback from the scheduler then token will be non-zero.
* @return nothing.
* @author Neal Shurmantine
* @version
* 04/18/2015    Created.
*******************************************************************************/
static void SCH_refresh_scene_events(void)
{
    SCH_EVENT_STRUCT_PTR p_event_rec;
    strScheduledEvent * p_single_schedule;
    uint16_t index;
    time_t event_time;
    uint16_t scene_count = 0;

    while (_mutex_try_lock(&flashDeviceMutex) != MQX_EOK) {
//        printf("Sched waiting for mutex\n");
        OS_TaskSleep(100);
    }
    if (isEnableScheduledEvents() == true) {
        FF_ReadListOfScheduledEvents(&SCH_pScheduleEventList);
        if (SCH_pScheduleEventList->count != 0) {
            p_single_schedule = (strScheduledEvent*)SCH_pScheduleEventList->db_list;
            for (index=0; index < SCH_pScheduleEventList->count; ++index) {
                if (SCH_compute_scene_event(p_single_schedule, &event_time) == true) {
                    ++scene_count;
                    p_event_rec = (SCH_EVENT_STRUCT_PTR)OS_GetMemBlock(sizeof(SCH_EVENT_STRUCT));
                    SCH_add_record_to_list(p_event_rec);
                    memcpy(&p_event_rec->time, &event_time, sizeof(time_t));
                    p_event_rec->event_id = p_single_schedule->uID;
                    p_event_rec->p_callback = SCH_execute_scene_now;
                    p_event_rec->isSceneEvent = true;
                    p_event_rec->isDailyEvent = true;
                }
                ++p_single_schedule;
            }
            if (scene_count) {
                printf("Added %d scenes to scheduler\n", scene_count);
            }
        }
    }

    _mutex_unlock(&flashDeviceMutex);
}

/*****************************************************************************//**
* @brief Scene is enabled for today and is a sunrise or sunset event.  This 
*   function determines if the event may happen today based on the current
*   time.   If the scheduled time has yet to occur then the time structure
*   for when is filled out.
*
* @param is_sunrise.  True if sunrise, false if sunset.
* @param minute.  Signed value of number of minutes before or after sunrise/sunset.
* @param p_when.  time_t pointer to the time if it will happen today.
* @return bool. True if it will occur today.
* @author Neal Shurmantine
* @version
* 04/18/2015    Created.
*******************************************************************************/
static bool SCH_check_sunrise_sunset(bool is_sunrise, 
                                    int16_t minute, 
                                    time_t * p_when)
{
    bool rtn_val = false;
    struct tm todays_date;
    time_t now;
    OS_GetTimeLocal(&now);
    time_t event_time;
    localtime_r(&now, &todays_date);
    if (is_sunrise == true) {
        todays_date.tm_hour = sunriseTimeInMinutes / 60;
        todays_date.tm_min = sunriseTimeInMinutes % 60;
    }
    else {
        todays_date.tm_hour = sunsetTimeInMinutes / 60;
        todays_date.tm_min = sunsetTimeInMinutes % 60;
    }
    todays_date.tm_sec = 0;
    event_time = mktime(&todays_date);
    event_time += (minute*60);
    if (event_time > now) {
        *p_when = event_time;
        rtn_val = true;
    }
    return rtn_val;
}

/*****************************************************************************//**
* @brief This function determines if the event may happen today based
*    on the current time.   Time structure is filled out with the next
*    time this event will occur.
*
* @param hour.  hour of the day.
* @param minute.  minute of the day.
* @param p_when.  time_t pointer to the next time event will happen.
* @return bool. True if it will occur today.
* @author Neal Shurmantine
* @version
* 04/18/2015    Created.
*******************************************************************************/
static bool SCH_is_happening_today(DAY_STRUCT_PTR p_day, time_t * p_when)
{
    struct tm date;
    bool happening_today;
    OS_GetTimeLocal(p_when);

    localtime_r(p_when, &date);
    if ((date.tm_hour < p_day->hour) 
        || ((date.tm_hour == p_day->hour) && (date.tm_min < p_day->minute))
        || ((date.tm_hour == p_day->hour) && (date.tm_min == p_day->minute) && (date.tm_sec < p_day->second))) {
        happening_today = true;
    }
    else {
        happening_today = false; 
        //schedule event to happen tomorrow instead
        date.tm_hour = 0;
        date.tm_min = 0;
        date.tm_sec = 0;
        *p_when = mktime(&date);
        *p_when += DAY_IN_SEC;
        localtime_r(p_when,&date);
    }
    date.tm_hour = p_day->hour;
    date.tm_min = p_day->minute;
    date.tm_sec = p_day->second;
    *p_when = mktime(&date);
    return happening_today;
}

/*****************************************************************************//**
* @brief Determines, based on the day of the week and the enabled flags, if
*        an scheduled event is enabled for the day.
*
* @param p_event.  Pointer to a scheduled events structure
* @return bool. True if event is enabled for current day of week.
* @author Neal Shurmantine
* @version
* 04/18/2015    Created.
*******************************************************************************/
static bool SCH_is_event_enabled_today(strScheduledEvent *p_event)
{
    time_t now;
    struct tm date;
    uint8_t mask;
    OS_GetTimeLocal(&now);
    localtime_r(&now, &date);
    mask = 1<<date.tm_wday;
    if ( p_event->enabledFlags.byte & mask ) {
        return true;
    }
    return false;
}

/*****************************************************************************//**
* @brief Analyzes the event structure for a particular scheduled event to
*     determine if it should be scheduled today.  If so then it returns true
*     and the parameter p_when will contain the time setting for the
*     scheduled event.
*
* @param event_id.  Id of event in the scheduled events database.
* @param p_when.  TIME_STRUCTURE that may contain the time that the event occurs.
* @return bool. True if event is to be scheduled
* @author Neal Shurmantine
* @version
* 04/18/2015    Created.
*******************************************************************************/
static bool SCH_compute_scene_event(strScheduledEvent *sched_event, time_t * p_when)
{
    bool event_found = false;
    DAY_STRUCT day;

    if (sched_event->enabledFlags.flags.isEnabled == true) {
        if (SCH_is_event_enabled_today(sched_event) == true) {
            if (sched_event->typeFlags.flags.isClock == true) {
                day.hour = sched_event->hours;
                day.minute = sched_event->minutes;
                day.second = 0;
                event_found = SCH_is_happening_today(&day,p_when);
            }
            else if (sunrise_sunset_found == true) {
                event_found = SCH_check_sunrise_sunset(sched_event->typeFlags.flags.isSunrise, 
                                    sched_event->minutes,
                                        p_when);
            }
        }
    }
    return event_found;
}

/*****************************************************************************//**
* @brief Go through the list of events and find one with a matching ID.
*
* @param event_id.  Id of event in the scheduled events database.
* @return pointer to a structure holding the strScheduledEvent.
* @author Neal Shurmantine
* @version
* 07/17/2015    Created.
*******************************************************************************/
static strScheduledEvent * SCH_find_scheduled_event_data(uint16_t event_id)
{
    strScheduledEvent * p_check_event = NULL;
    uint16_t n;
    p_check_event = (strScheduledEvent *)&SCH_pScheduleEventList->db_list;
    for (n=0; n < SCH_pScheduleEventList->count; ++n) {
        if (event_id == p_check_event->uID) {
            return p_check_event;
        }
        ++p_check_event;
    }

    return NULL;
}

/*****************************************************************************//**
* @brief An event timer has expired and this function determines if it is a
*    scene or multiroom scene and executes it.
*
* @param event_id.  Id of event in the scheduled events database.
* @return nothing.
* @author Neal Shurmantine
* @version
* 04/18/2015    Created.
*******************************************************************************/
static void SCH_execute_scene_now(uint16_t event_id)
{
    char tag[MAX_TAG_LABEL_SIZE];
    strScheduledEvent * p_event = SCH_find_scheduled_event_data(event_id);
    if (p_event != NULL) {
        SCH_DisplayCurrentTime();
        if (RAS_IsNestActionsActive() == true) {
            printf("Skip Scheduled Event, Nest active\n");
        }
        else {
        	if (p_event->typeFlags.flags.isMultiSceneID == true) {
            	sendMultiSceneMsgToShades(p_event->sceneOrMultiSceneID);
                printf("Scheduled Execute MultiScene No. %d\n",p_event->sceneOrMultiSceneID);
            	sprintf(tag,"Scheduled Multi %d",p_event->sceneOrMultiSceneID);
            	LOG_LogEvent(tag);
        	}
        	else {
            	ExecuteSceneFromRemoteConnect(p_event->sceneOrMultiSceneID);
                printf("Scheduled Execute Scene No. %d\n",p_event->sceneOrMultiSceneID);
            	sprintf(tag,"Scheduled Scene %d",p_event->sceneOrMultiSceneID);
            	LOG_LogEvent(tag);
            }
        }
    }
    else {
        sprintf(tag,"Sched ID %d Not Found",event_id);
        LOG_LogEvent(tag);
    }
}

/*****************************************************************************//**
* @brief Generate a random number to use when doing daily scheduling.
*
* @param max_minutes.  The random number will be an integer between zero
        and this value.
* @return random value.
* @author Neal Shurmantine
* @version
* 05/19/2015    Created.
*******************************************************************************/
uint32_t SCH_Randomize(uint32_t max_minutes)
{
    time_t a_time;
    uint16_t seed;
    uint32_t new_minutes;
    uint64_t seed_long = RC_GetNordicUuid();

    time(&a_time);
    seed = (uint16_t)a_time;
    seed ^= (uint16_t)seed_long;
    seed ^= (uint16_t)(seed_long>>16);
    seed ^= (uint16_t)(seed_long>>32);
    seed ^= (uint16_t)(seed_long>>48);
    srand(seed);
    new_minutes = rand()%max_minutes;
    return new_minutes;
}
