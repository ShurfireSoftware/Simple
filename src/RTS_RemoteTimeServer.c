/***************************************************************************//**
 * @file   RTS_RemoteTimeServer.c
 * @brief  This module contains functions to communicate with the Remote Connect
 *         server to get time data.
 *
 * @author Neal Shurmantine
 * @copyright (c) 2015 Hunter Douglas. All rights reserved.
 *
 *
 * @version
 * 05/11/2015   Created.
 ******************************************************************************/

/* Includes
*******************************************************************************/

#define ENABLE_JSON_PRINT


#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "util.h"
#include "os.h"
#include "config.h"
#include "SCH_ScheduleTask.h"
#include "RMT_RemoteServers.h"
#include "LOG_DataLogger.h"
#include "stub.h"
#include "JSONParser_v2.h"

#ifdef USE_ME
#include "JSONInterpreter.h"
#include "SPIFlashData.h"
#endif

/* Global Variables
*******************************************************************************/
extern char timeZoneName[ItsMaxTimeZoneNameLength_];
extern float latitude;
extern float longitude;

/* Local Constants and Definitions
*******************************************************************************/
//#define ENABLE_JSON_PRINT

#define ENABLE_SSL_ON_RTS_SERVER            1

#if ENABLE_SSL_ON_RTS_SERVER

// The location of the root certificate file
// This is only needed if you want to authenticate the server
// #define CA_FILE_RTS_SERVER  "cert:"              // to use a certificate in memory
// #define CA_FILE_RTS_SERVER  "c:\\hdcert.pem"     // to use a certificate on the SD card 


// The parameters needed to initialize SSL to talk to the hunter douglas server
static const RTCS_SSL_PARAMS_STRUCT hunterDouglasRTSServerSSLParameters = 
{
    NULL,                   /* Client or Server Certificate file.*/
    NULL,                   /* Client or Server private key file.*/
#ifdef CA_FILE_RTS_SERVER    
    CA_FILE_RTS_SERVER,     /* CA (Certificate Authority) certificate file.*/
#else
    NULL,
#endif
    RTCS_SSL_CLIENT,
    NULL,
#ifdef CA_FILE_RTS_SERVER    
    false                       // do not disable server verification
#else
    true                        // disable server verification
#endif
};

#endif

/* Local Function Declarations
*******************************************************************************/
static uint32_t rts_time_update_get_return(REST_CLIENT_QUERY_STRUCT_PTR p_query);
static bool parse_time_update_data(char * p_server_response);
static void print_json_data(void);
static bool rts_lat_long_set(void);

/* Local variables
*******************************************************************************/
static TIME_UPDATE_DATA RTS_TimeUpdateData;


/*****************************************************************************//**
* @brief This function processes the JSON time object received from a
*    remote connect time get.
*FORMAT:
* {"TimeInstance":{"latitude":40.0,"longitude":-105.0,"dstOffset":3600000,"rawOffset":-25200000,
*   "currentTimeUTC":"2015-05-11 19:41:17.724","sunriseTimeUTC":"2015-05-11 11:49:00.000",
*   "sunsetTimeUTC":"2015-05-12 02:04:00.000"}}
*NOTE: latitude and longitude are ignored.
*
* @param p_server_response.  Pointer to character array containing response.
* @return bool, true if data parsed correctly.
* @author Neal Shurmantine
* @version
* 05/11/2015    Created.
*******************************************************************************/
static bool parse_time_update_data(char * p_server_response) 
{
	JSON_PARSE_OBJECT   rootObject, timeInstanceObject, offSetObject, utcObject;
    bool                dataComplete = false;

    jv2_makeObjectFromString(&rootObject,p_server_response);

    RTS_TimeUpdateData.dst_offset = 0;
    RTS_TimeUpdateData.raw_offset = 0;
    // find the TimeInstance node
    if(jv2_findObject(&rootObject,"TimeInstance",&timeInstanceObject)) {
        dataComplete = true;

        // get daylight savings time offset
        if(jv2_findObject(&timeInstanceObject,"dstOffset",&offSetObject)) {
            dataComplete &= jv2_getObjectInt32(&offSetObject,&RTS_TimeUpdateData.dst_offset);
        }
        // get raw time offset
        if(jv2_findObject(&timeInstanceObject,"rawOffset",&offSetObject)) {
            dataComplete &= jv2_getObjectInt32(&offSetObject,&RTS_TimeUpdateData.raw_offset);
        }
        // get current UTC time
        if(jv2_findObject(&timeInstanceObject,"currentTimeUTC",&utcObject)) {
            dataComplete &= jv2_getObjectUTC(&utcObject,&RTS_TimeUpdateData.cur_time);
        }
        if (rts_lat_long_set() == true) {
            // get sunrise UTC time
            if(jv2_findObject(&timeInstanceObject,"sunriseTimeUTC",&utcObject)) {
                dataComplete &= jv2_getObjectUTC(&utcObject,&RTS_TimeUpdateData.sunrise);
            }
            // get sunset UTC time
            if(jv2_findObject(&timeInstanceObject,"sunsetTimeUTC",&utcObject)) {
                dataComplete &= jv2_getObjectUTC(&utcObject,&RTS_TimeUpdateData.sunset);
            }
        }
    }
    return dataComplete;
}

/*****************************************************************************//**
* @brief Callback function from time GET that is sent to remote server.
*
* @param p_query.  Pointer to REST_CLIENT_QUERY_STRUCT
* @return uint32_t.  This value is a pointer to RTS_TimeUpdateData or NULL if
*                    JSON did not parse correctly.
* @author Neal Shurmantine
* @version
* 05/11/2015    Created.
*******************************************************************************/
static uint32_t rts_time_update_get_return(REST_CLIENT_QUERY_STRUCT_PTR p_query)
{
    uint32_t rslt = 0;
    if ( parse_time_update_data(p_query->buffer)==true) {
        rslt = (uint32_t)&RTS_TimeUpdateData;
    }
    print_json_data();
    return rslt;
}

static void print_json_data(void)
{
#ifdef ENABLE_JSON_PRINT
    time_t time;

    printf("JSON DATA: \n\r");
    printf("dst_offset %d\n",RTS_TimeUpdateData.dst_offset);
    printf("raw_offset %d\n",RTS_TimeUpdateData.raw_offset);
    time = mktime(&RTS_TimeUpdateData.cur_time);
    printf("cur_time UTC ");
    SCH_DisplayTime(&time);
    time = mktime(&RTS_TimeUpdateData.sunrise);
    printf("sunrise UTC ");
    SCH_DisplayTime(&time);
    time = mktime(&RTS_TimeUpdateData.sunset);
    printf("sunset UTC ");
    SCH_DisplayTime(&time);
#endif
}

/*****************************************************************************//**
* @brief This function is called to start the whole process of contacting the
*        remote connect server and getting the time.
*
* @param none
* @return A pointer to the returned data or NULL.
* @author Neal Shurmantine
* @version
* 05/11/2015    Created.
*******************************************************************************/
uint32_t RTS_CheckTimeUpdate(void)
{
    uint32_t update_success=0;
    char lat_str[9];
    char lon_str[9];
    char tz_name[ItsMaxTimeZoneNameLength_];

    REST_CLIENT_QUERY_STRUCT_PTR p_query = (REST_CLIENT_QUERY_STRUCT_PTR)OS_GetMemBlock(sizeof(REST_CLIENT_QUERY_STRUCT));

    LoadDefaultClientData(p_query, &hunterDouglasRTSServerSSLParameters,HTTPSRV_REQ_GET,rts_time_update_get_return);

    jv2_percentEncodeURIData(timeZoneName, tz_name);
    if ( rts_lat_long_set() == true ) {
        sprintf(lat_str,"%1.3f",latitude);
        sprintf(lon_str,"%1.3f",longitude);
        snprintf(p_query->resource,MAX_RESOURCE_NAME_LENGTH, HUB_TIME_GET_RESOURCE, RMT_GetAPIVersion(), tz_name, lat_str, lon_str);
    }
    else {
        snprintf(p_query->resource,MAX_RESOURCE_NAME_LENGTH, HUB_TIME_GET_RESOURCE_NO_LAT_LON, RMT_GetAPIVersion(), tz_name);
    }

    LOG_LogEvent("Check Time Server");
    ConnectToServer(p_query);
    if (p_query->connection.socket) {
        if (GetResource(p_query)) {
            update_success = p_query->callback(p_query);
        }
        DisconnectFromServer(p_query);
    }
    OS_ReleaseMemBlock((void*)p_query);
    return update_success;
}

/*****************************************************************************//**
* @brief Determines if the Hub knows its latitude and longitude.
*
* @param none
* @return true if latitude and longitude have been set.
* @author Neal Shurmantine
* @version
* 05/11/2015    Created.
*******************************************************************************/
static bool rts_lat_long_set(void)
{
    return ( (latitude !=0) && (longitude != 0) );
}

