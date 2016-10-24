/***************************************************************************//**
 * @file LOG_DataLogger.h
 * @brief Include file for IO_InputOutput module.
 *
 * @author Neal Shurmantine
 * @copyright (c) 2015 Hunter Douglas. All rights reserved.
 *
 * @version
 * 04/22/2015   Created.
 ******************************************************************************/

#ifndef _LOG_DATA_LOGGER_H_
#define _LOG_DATA_LOGGER_H_

#include <time.h>

#define MAX_TAG_LABEL_SIZE  30
typedef struct
{
    uint64_t time;
    time_t time_struct;
    char label[MAX_TAG_LABEL_SIZE];
}TAG_STRUCT, *TAG_STRUCT_PTR;


void LOG_LogEvent(char * tag_label);
void LOG_DisplayLogFile(void);
void LOG_RestResponse(char *p_buffer);
void LOG_InitLogging(void);

#endif
