/***************************************************************************//**
 * @file SCH_ScheduleTask.h
 * @brief Include file for SCH_ScheduleTask module.
 *
 * @author Neal Shurmantine
 * @copyright (c) 2015 Hunter Douglas. All rights reserved.
 *
 * @date Created: 02/13/2015
 * @date Last updated: 02/13/2015
 *
 * @version
 * 02/13/2015   Created.
 * 02/17/2015   Renamed from REM_RemoteConnect.
 ******************************************************************************/

#ifndef _SCH_SCHEDULE_TASK_H_
#define _SCH_SCHEDULE_TASK_H_

#include <time.h>
#include <stdint.h>
#include <stdbool.h>

#define NULL_TOKEN                          0

#define SEC_IN_MS           1000
#define MIN_IN_MS           (SEC_IN_MS * 60)
#define HOUR_IN_MS          (MIN_IN_MS * 60)
#define DAY_IN_MS           (HOUR_IN_MS * 24)
#define SEC                 1
#define MIN_IN_SEC          60
#define HOUR_IN_SEC         (MIN_IN_SEC * 60)
#define DAY_IN_SEC          (HOUR_IN_SEC * 24)

#ifdef ENABLE_DEBUG
#define SCH_HTTP_ACTIVE_MAX_COUNT   (20*SEC) /*use for cucumber test*/
#else
#define SCH_HTTP_ACTIVE_MAX_COUNT   (2*MIN_IN_SEC)
#endif

#define TIME_STRING_MAX_LENGTH  32

typedef struct {
    int32_t dst_offset;
    int32_t raw_offset;
    struct tm cur_time;
    struct tm sunrise;
    struct tm sunset;
} TIME_UPDATE_DATA, *TIME_UPDATE_DATA_PTR;

typedef struct {
    uint16_t hour;
    uint16_t minute;
    uint16_t second;    
} DAY_STRUCT, * DAY_STRUCT_PTR;

void SCH_ScheduleTaskTest(void);
void SCH_InitTimeVariables(void);
void SCH_ScheduleTaskInit(void);
void SCH_ProcessNewTime(TIME_UPDATE_DATA_PTR p_time_data);
void SCH_SetTime(time_t * p_new_time,int32_t timezone_offset);
void SCH_NewTimeSet(uint32_t time_diff);
uint16_t SCH_ScheduleDaily(DAY_STRUCT_PTR day, void(*)(uint16_t));
uint16_t SCH_ScheduleEventPostSeconds(uint32_t sec, void(*p_callback)(uint16_t));
void SCH_ModifyScheduledScenes(void);
void SCH_DisplayCurrentTime(void);
void SCH_DisplayTime(time_t * p_time);
void SCH_MakeTimeString(char * p_time_str, time_t * p_time);
void SCH_RemoveScheduledEvent(uint16_t token);
uint32_t SCH_Randomize(uint32_t max_minutes);
void SCH_DebugRequestScheduleList(void);
void SCH_ResetHTTPActiveCount(void);
bool SCH_IsHTTPActive(void);
typedef void(*callback)(uint16_t param);


#endif
