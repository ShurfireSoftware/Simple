/***************************************************************************//**
 * @file   LOG_DataLogger.c
 * @brief  This module provides a way of logging events with a timestamp.
 * 
 * @author Neal Shurmantine
 * @copyright (c) 2015 Hunter Douglas. All rights reserved.
 *
 * @version
 * 04/22/2015   Created.
 ******************************************************************************/

/* Includes
*******************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include "config.h"
#include "os.h"
#include "LOG_DataLogger.h"
#include "SCH_ScheduleTask.h"
#include "file_names.h"

/* Global Variables
*******************************************************************************/

/* Local Constants and Definitions
*******************************************************************************/
#define LOG_MAX_FILE_SIZE   1000000

/* Local Function Declarations
*******************************************************************************/
static void LOG_write_tag_to_file(TAG_STRUCT_PTR p_tag);
static uint32_t LOG_get_file_size(void);
static void LOG_move_log_file_to_backup_and_erase(void);

/* Local variables
*******************************************************************************/
static bool LOG_LogEnabled=false;

/*****************************************************************************//**
* @brief  If a file named "log" is on the SD card then enable event logging
*          to a file.
*
* @param none.  
* @return nothing.
* @author Neal Shurmantine
* @version
* 07/19/2015    Created.
*******************************************************************************/
void LOG_InitLogging(void)
{
    FILE * p_file = NULL;
    
    p_file = fopen(LOG_ENABLE_FILENAME,"r");
    if (p_file != NULL) {
        fclose(p_file);
        LOG_LogEnabled = true;
    }
    else {
        LOG_LogEnabled = false;
    }
}

/*****************************************************************************//**
* @brief  If enabled for logging, log this time-stamped event to a file.
*
* @param tag_label. A pointer to a string to be logged. 
* @return nothing.
* @author Neal Shurmantine
*******************************************************************************/
void LOG_LogEvent(char * tag_label)
{
    if ( LOG_LogEnabled == true ) {
        time_t now;
        TAG_STRUCT tag;
        OS_GetTimeLocal(&now);
        tag.time_struct = now;
        memcpy(&tag.label,tag_label,MAX_TAG_LABEL_SIZE);
        tag.label[MAX_TAG_LABEL_SIZE-1] = 0;
        LOG_write_tag_to_file(&tag);
    }
}

/*****************************************************************************//**
* @brief  If enabled for logging, log this time-stamped event to a file.
*
* @param p_tag.  A pointer to a tag structure that contains time and a string  
* @param p_str.  A pointer to a string that is returned, containing a time-stamp
*                and event description.
* @return nothing.
* @author Neal Shurmantine
*******************************************************************************/
void LOG_CreateTagString(TAG_STRUCT_PTR p_tag, char * p_str)
{
    time_t time;
    time = p_tag->time_struct;
    char time_str[TIME_STRING_MAX_LENGTH];
    SCH_MakeTimeString(time_str, &time);
    sprintf(p_str, "%s - %s\n",time_str, p_tag->label);
}

/*****************************************************************************//**
* @brief  Called from shell, lists the events saved in the log..
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
*******************************************************************************/
void LOG_DisplayLogFile(void)
{
    FILE * fs_ptr;
    char t[MAX_TAG_LABEL_SIZE + 20];
    char *rtn;

    fs_ptr = fopen(LOG_FILENAME, "r");
    if (fs_ptr != NULL) {
        do {
            rtn = fgets(t, MAX_TAG_LABEL_SIZE + 20, fs_ptr);
            if (rtn == t) {
                printf("%s\n",t);
            }
        } while (rtn == t);
        fclose(fs_ptr);
    }
}

/*****************************************************************************//**
* @brief  Write the time-stamp and event description to a file.  If the log file 
*         is full then copy to backup file and create a new log file.
*
* @param p_tag.  Pointer to a TAG structure.
* @return nothing.
* @author Neal Shurmantine
*******************************************************************************/
static void LOG_write_tag_to_file(TAG_STRUCT_PTR p_tag)
{
    char d_str[MAX_TAG_LABEL_SIZE + 20];
    FILE * fs_ptr;
    if ( LOG_get_file_size() > LOG_MAX_FILE_SIZE) {
        LOG_move_log_file_to_backup_and_erase();
    }
    LOG_CreateTagString(p_tag, d_str);
    fs_ptr = fopen(LOG_FILENAME, "a");
    if (fs_ptr != NULL) {
        fprintf (fs_ptr, "%s\r\n",d_str);
        fclose(fs_ptr);
    }
}

/*****************************************************************************//**
* @brief  Handles saving the log file to a backup and clearing the log file for a
*         new entry.
* @param none.
* @return nothing.
* @author Neal Shurmantine
*******************************************************************************/
static void LOG_move_log_file_to_backup_and_erase(void)
{
    FILE * p_file = NULL;
    
    p_file = fopen(LOG_BACKUP_FILENAME,"r");
    if (p_file != NULL) {
        fclose(p_file);
        remove(LOG_BACKUP_FILENAME);
    }
    rename (LOG_FILENAME, LOG_BACKUP_FILENAME);
}

/*****************************************************************************//**
* @brief  Returns the size of the log file in bytes.
* @param none.
* @return uint32_t.  Size of file.
* @author Neal Shurmantine
*******************************************************************************/
static uint32_t LOG_get_file_size(void)
{
    struct stat buf;
    stat(LOG_FILENAME, &buf);
    return buf.st_size;
}

void LOG_RestResponse(char *p_buffer)
{
    FILE * fs_ptr;

    fs_ptr = fopen(LOG_REST_FILE_NAME, "a");
    if (fs_ptr != NULL) {
        fprintf (fs_ptr, "%s\r\n",p_buffer);
        fclose(fs_ptr);
    }
}


