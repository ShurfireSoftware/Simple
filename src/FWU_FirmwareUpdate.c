/***************************************************************************//**
 * @file   FWU_FirmwareUpdate.c
 * @brief  This module contains functions to check the remote server for a
 *         firmware update and download the file if required.
 * Note:
 *      The firmware file may reside on the remote server named in the format
 *      FW_FRE_19_000500FE.hex but the bootloader is looking for the file app.hex
 *
 * @author Neal Shurmantine
 * @copyright (c) 2015 Hunter Douglas. All rights reserved.
 *
 * @date Created: 02/17/2015
 * @date Last updated: 02/17/2015
 *
 * @version
 * 02/17/2015   Created.
 ******************************************************************************/

/* Includes
*******************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <openssl/md5.h>

#ifdef USE_ME
#include <md5_file.h>
#include <mmcau_safe_api.h>
#include <crypto_common.h>
#endif

#include "JSONParser_v2.h"
#include "revision.h"
#include "config.h"
#include "RMT_RemoteServers.h"
#include "os.h"
//#include "IO_InputOutput.h"
//#include "DataStruct.h"
#include "LOG_DataLogger.h"
#include "file_names.h"
#include "SCH_ScheduleTask.h"
 
/* Global Variables
*******************************************************************************/

/* Local Constants and Definitions
*******************************************************************************/
#define FIRMWARE_FILE_NAME_LENGTH   48
#define MD5_HEX_STR_LENGTH      (2*MD5_DIGEST_LENGTH+1)
#define MD5_READ_BLOCK_SIZE     256

#define ENABLE_SSL_ON_FWU_SERVER 1

//#define USE_FIXED_VERSION


//#define ENABLE_SSL_ON_FIRMWARE_UPDATE 1

//FIX ME - more error checking (set minimum with no errors to 1 minute)
#define MIN_FIRMWARE_UPDATE_TIME    30  //  This is the minimum time, in minutes, between consecutive firmware requests

// This structure contains the  parsed response to the firmware request
#define FW_MAX_URL_STR_LEN          128
#define FW_MAX_REL_DATE_STR_LEN     30

typedef struct {
    uint32_t            fre_ver;
    uint32_t            nor_ver;
    uint32_t            hw_ver;
    uint32_t            update_minutes;
    char                fre_url[FW_MAX_URL_STR_LEN];
    char                nor_url[FW_MAX_URL_STR_LEN];
    char                fre_md5[MD5_HEX_STR_LENGTH];
    char                nor_md5[MD5_HEX_STR_LENGTH];
    char                releaseDate[FW_MAX_REL_DATE_STR_LEN];
} FIRMWARE_DATA, *FIRMWARE_DATA_PTR;

typedef struct MD5_CHECK_TAG
{
    char boot_file_name[FIRMWARE_FILE_NAME_LENGTH];
    char md5_file_name[FIRMWARE_FILE_NAME_LENGTH];
    char md5_str[MD5_HEX_STR_LENGTH];
} MD5_CHECK_STRUCT, *MD5_CHECK_STRUCT_PTR;

#ifdef DEBUG_FIRMWARE_DOWNLOAD
#define ENABLE_FIRMWARE_DOWNLOAD
#endif

/* Local Function Declarations
*******************************************************************************/
static bool ParseUrl(char * url, char * domain, char ** resource);
static bool FWU_verify_update_files(void);
static void FWU_check_nordic_download(void);
static bool FWU_is_firmware_download_enabled(void);
static void FWU_image_download(REST_CLIENT_QUERY_STRUCT_PTR p_client,
                                FIRMWARE_DATA_PTR p_firmware_data);
static bool CheckMd5(MD5_CHECK_STRUCT_PTR firmwareData, eRestClientStatus *firmwareUpdateStatus);
static bool GetFirmwareImage(REST_CLIENT_QUERY_STRUCT_PTR p_client);
static uint32_t get_image_file_return(REST_CLIENT_QUERY_STRUCT_PTR p_client);
static bool ParseFirmwareUpdateData(char * serverResponse, FIRMWARE_DATA  * firmwareData);
static bool md5_file(char * f_name, char *hash_string);

/* Local variables
*******************************************************************************/
// g_firmwareUpdateStatus is a global that contains the current status of the firmware update task.
// It should be eFWU_OK if we are able to contact the server and properly parse the responses, etc. 
// It will be set to an error code if something goes wrong.
eRestClientStatus   g_firmwareUpdateStatus = eFWU_OK;



#if ENABLE_SSL_ON_FWU_SERVER

// The location of the root certificate file
// This is only needed if you want to authenticate the server
// #define CA_FILE_FWU_SERVER  "cert:"              // to use a certificate in memory
// #define CA_FILE_FWU_SERVER  "c:\\hdcert.pem"     // to use a certificate on the SD card 


// The parameters needed to initialize SSL to talk to the hunter douglas server
static const RTCS_SSL_PARAMS_STRUCT hunterDouglasFWUServerSSLParameters = 
{
    NULL,                   /* Client or Server Certificate file.*/
    NULL,                   /* Client or Server private key file.*/
#ifdef CA_FILE_FWU_SERVER    
    CA_FILE_FWU_SERVER, /* CA (Certificate Authority) certificate file.*/
#else
    NULL,
#endif
    RTCS_SSL_CLIENT,
    NULL,
#ifdef CA_FILE_FWU_SERVER    
    false                       // do not disable server verification
#else
    true                        // disable server verification
#endif
};

#endif

#if ENABLE_SSL_ON_FIRMWARE_UPDATE

// The location of the root certificate file
// This is only needed if you want to authenticate the server
// #define CA_FILE_FWU_UPDATE  "cert:"              // to use a certificate in memory
// #define CA_FILE_FWU_UPDATE  "c:\\hdcert.pem"     // to use a certificate on the SD card 


// The parameters needed to initialize SSL to talk to the hunter douglas server
static const RTCS_SSL_PARAMS_STRUCT hunterDouglasFWUpdateSSLParameters = 
{
    NULL,                   /* Client or Server Certificate file.*/
    NULL,                   /* Client or Server private key file.*/
#ifdef CA_FILE_FWU_UPDATE    
    CA_FILE_FWU_UPDATE, /* CA (Certificate Authority) certificate file.*/
#else
    NULL,
#endif
    RTCS_SSL_CLIENT,
    NULL,
#ifdef CA_FILE_FWU_UPDATE    
    false                       // do not disable server verification
#else
    true                        // disable server verification
#endif
};

#endif

/*****************************************************************************//**
* @brief This function allows development code to avoid downloading new firmware
*   files
*
* @param none.
* @return bool. True if firmware file download is enabled.
* @author Neal Shurmantine
* @version
* 04/28/2015    Created.
*******************************************************************************/
static bool FWU_is_firmware_download_enabled(void)
{
#ifdef ENABLE_FIRMWARE_DOWNLOAD
    return true;
#else
    return false;
#endif
}

/*****************************************************************************//**
* @brief Initialize the Remote Connect task.
*
* @param temp is a unused variable.
* @return nothing.
* @author Neal Shurmantine
* @version
* 02/17/2015    Created.
*******************************************************************************/
void FWU_FirmwareUpdateInit(void)
{
    FILE * p_file;

    p_file = fopen(MD5_FILENAME,"r");
    if (p_file != NULL) {
        fclose(p_file);
        remove(MD5_FILENAME);
        printf("Deleting Nordic MD5 File\n");
    }
    FWU_check_nordic_download();
}

/*****************************************************************************//**
* @brief This function is called immediately after reset.  The SD card is checked
*    for the presence of all three Nordic files.  If they are all present, 
*    indicating the Nordic has not been downloaded, then the Nordic download
*    process is started.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 04/28/2015    Created.
*******************************************************************************/
static void FWU_check_nordic_download(void)
{
#ifdef USE_ME
    if (NBT_VerifyNordicFiles() == true) {
        NBT_BeginNordicDownload();
    }
#endif
}

/*****************************************************************************//**
* @brief Check to see if there are 3 valid Nordic files and 2 Freescale files
*    on the SD card.
*
* @param none.
* @return bool.  True if all five files exist
* @author Neal Shurmantine
* @version
* 04/28/2015    Created.
*******************************************************************************/
static bool FWU_verify_update_files(void)
{
    bool rslt;
    FILE * p_file = NULL;
    
    rslt = NBT_VerifyNordicFiles();

    if (rslt == true) {
        p_file = fopen(BOOT_FILENAME,"r");
        if (p_file != NULL) {
            fclose(p_file);
        }
        else {
            rslt = false;
        }
    }

    if (rslt == true) {
        p_file = fopen(MD5_FILENAME,"r");
        if (p_file != NULL) {
            fclose(p_file);
        }
        else {
            rslt = false;
        }
    }

    return rslt;
}

/*****************************************************************************//**
* @brief Return the firmware version as a 4 byte number.
*
* @param none.
* @return version as a uint32_t.
* @author Neal Shurmantine
* @version
* 03/31/2015    Created.
*******************************************************************************/
uint32_t FWU_GetFirmwareVersion(void)
{
    union {
        struct {
            uint16_t rev;
            uint8_t minor;
            uint8_t major;
        }__attribute__((packed)) a_str;
        uint32_t a_long;
    } ver_union;
#ifdef USE_FIXED_VERSION
    ver_union.a_str.rev = 574;
    ver_union.a_str.minor = 1;
    ver_union.a_str.major = 1;
#else
    ver_union.a_str.rev = ItsBuild_;
    ver_union.a_str.minor = ItsSubRevision_;
    ver_union.a_str.major = ItsRevision_;
#endif
    return ver_union.a_long;
}

/*****************************************************************************//**
* @brief  This is a callback function following a check for firmware update.  If
*         an update is required then this function starts the process of getting
*         the image file.
*
* @param p_client.  
* @return nothing.
* @author Neal Shurmantine
* @version
* 02/24/2015    Created.
*******************************************************************************/
uint32_t FWU_GetFwReturn(REST_CLIENT_QUERY_STRUCT_PTR p_client)
{
  	FIRMWARE_DATA firmwareData;

    DisconnectFromServer(p_client);
    firmwareData.update_minutes = 0;
    if (ParseFirmwareUpdateData(p_client->buffer, &firmwareData)) {
        if (firmwareData.fre_ver <= FWU_GetFirmwareVersion()) {
            printf("Freescale firmware is up to date\n\n");
        }
        else {
            if (FWU_is_firmware_download_enabled() == true) {
                FWU_image_download(p_client,&firmwareData);
            }
            else {
                printf("Freescale firmware is not up to date, but download is disabled\n  (local: %d, remote: %d)\n",
                FWU_GetFirmwareVersion(), firmwareData.fre_ver);
            #ifdef USE_FIXED_VERSION
                printf("  NB: currently using a fixed version number for debugging\n");
            #endif
            }
        }
    }
    else {
        p_client->status = eFWU_CANT_PARSE_SERVER_RESPONSE; 
    }
    return firmwareData.update_minutes;
}

/*****************************************************************************//**
* @brief  It has already been determined that a newer firmware version is
*     available on the remote connect server.  This function attempts to download
*     the new firmware files.
*
* @param p_client.  Pointer to REST_CLIENT_QUERY_STRUCT
* @param p_firmware_data.  Pointer to FIRMWARE_DATA.
* @return nothing.
* @author Neal Shurmantine
* @version
* 04/28/2015    Created.
*******************************************************************************/
static void FWU_image_download(REST_CLIENT_QUERY_STRUCT_PTR p_client,
                                FIRMWARE_DATA_PTR p_firmware_data)
{
    char domain[FW_MAX_URL_STR_LEN];
    char * resource;              
    eRestClientStatus status; 
    MD5_CHECK_STRUCT p_md5_str;
    if (ParseUrl(p_firmware_data->fre_url, domain, &resource)) {
        LED_RemoteFirmwareDownload(true);
        status = p_client->status;

        LoadDefaultClientData(p_client,NULL,HTTPSRV_REQ_GET,get_image_file_return);

        p_client->status = status;
        p_client->save_file_name = BOOT_FILENAME;
        p_client->domain = domain;
        strcpy(p_client->resource,resource);
        p_client->socket_options.receive_no_wait = false; //recv waits until data received
        p_client->socket_options.connection_timeout = 120 * SEC_IN_MS;

        //get freescale firmware
       if (GetFirmwareImage(p_client)) {
            printf("New Freescale image loaded\n\n");
            strcpy(p_md5_str.boot_file_name, BOOT_FILENAME);
            strcpy(p_md5_str.md5_file_name, MD5_FILENAME);
            strcpy(p_md5_str.md5_str, p_firmware_data->fre_md5);
            CheckMd5(&p_md5_str,&p_client->status);
            DisconnectFromServer(p_client);
        }
        if (ParseUrl(p_firmware_data->nor_url, domain, &resource)) {
            //printf("Nordic version = %d\n",firmwareData.nor_ver);
            status = p_client->status;

            LoadDefaultClientData(p_client,NULL,HTTPSRV_REQ_GET,get_image_file_return);

            p_client->status = status;
            p_client->domain = domain;
            strcpy(p_client->resource,resource);
            p_client->socket_options.receive_no_wait = false;
            p_client->save_file_name = RF_IMAGE_FILENAME;
            p_client->socket_options.connection_timeout = 120 * SEC_IN_MS;


            //get nordic firmware
            if (GetFirmwareImage(p_client)) {
                printf("New Nordic image loaded\n\n");
                strcpy(p_md5_str.boot_file_name,RF_IMAGE_FILENAME);
                strcpy(p_md5_str.md5_file_name,RF_MD5_FILE);
                strcpy(p_md5_str.md5_str, p_firmware_data->nor_md5);
                if ( CheckMd5(&p_md5_str,&p_client->status) == true) {


                    FILE * ver_file = fopen(RF_VERSION_FILE, "w");
                    if (ver_file) {
                        char t[10];
                        sprintf(t,"%d",p_firmware_data->nor_ver);
                        fwrite(t, 1, strlen(t), ver_file);
                        fclose(ver_file);
                    } 
                    else {
                        p_client->status = eFWU_CANT_WRITE_VERSION_FILE;
                    }

                }
                DisconnectFromServer(p_client);
            }
        }
        else {
            p_client->status = eFWU_CANT_PARSE_FILE_URL;
        }
        LED_RemoteFirmwareDownload(false);
    }
    else {
        p_client->status = eFWU_CANT_PARSE_FILE_URL;
    }
    if (p_client->status == eFWU_OK) {
        if (FWU_verify_update_files() == true) {
            LOG_LogEvent("New Firmware Loaded");
            RESET_HUB();
        }
        else {
            p_client->status = eFWU_DOWNLOAD_INCOMPLETE;
       }
    }
}

/*****************************************************************************//**
* @brief Execute a GET to check for a firmware update.
*
* @param 
* @return nothing.

* @version

*******************************************************************************/
uint32_t FWU_CheckForUpdate(void)
{
    uint32_t nextUpdate = 0;

    LOG_LogEvent("Check Firmware Server");
    REST_CLIENT_QUERY_STRUCT_PTR p_query = (REST_CLIENT_QUERY_STRUCT_PTR)OS_GetMemBlock(sizeof(REST_CLIENT_QUERY_STRUCT));

    LoadDefaultClientData(p_query, &hunterDouglasFWUServerSSLParameters,HTTPSRV_REQ_GET,FWU_GetFwReturn);

    snprintf(p_query->resource,MAX_RESOURCE_NAME_LENGTH,HUB_FW_GET_RESOURCE,RMT_GetAPIVersion(),FWU_GetFirmwareVersion(),RMT_GetHWVersion());

    printf("Checking for firmware update(%s%s)\n",p_query->domain,p_query->resource);

    ConnectToServer(p_query);
	
    if (p_query->connection.socket) {
        if (GetResource(p_query)) {
            nextUpdate = p_query->callback(p_query);
			
			// printf("fw marker2\n");
        }
        else {
            DisconnectFromServer(p_query);
        }
    }
	
    g_firmwareUpdateStatus = p_query->status;
	
	// marker 03/14/2016
	if(g_firmwareUpdateStatus != eFWU_OK) {
		printf("*** Error: firmware update status = %d", g_firmwareUpdateStatus);
		if(g_firmwareUpdateStatus == eFWU_CANT_PARSE_SERVER_RESPONSE)
		 	printf(", \"eFWU_CANT_PARSE_SERVER_RESPONSE\"");
		printf(" ***\n");
	}
	
    if (nextUpdate < MIN_FIRMWARE_UPDATE_TIME) {
        nextUpdate = MIN_FIRMWARE_UPDATE_TIME;
    }
    OS_ReleaseMemBlock((void*)p_query);
    return nextUpdate * MIN_IN_SEC;
}

/*****************************************************************************//**
* @brief Returns the status of the firmware update process.
*
* @param none.
* @return nothing.
* @version
*******************************************************************************/
eRestClientStatus FWU_GetStatus(void)
{
    return g_firmwareUpdateStatus;
}


/*!
 * \brief Parses a URL into a domain and a resource. 
 * 
 * Given the url: http://homeauto.hunterdouglas.com/api/firmware?revision=2000000&hardware=100
 * it will return the domain: homeauto.hunterdouglas.com
 * and the resource: api/firmware?revision=2000000&hardware=100
 *
 * \param[in]   url
 * \param[out]  domain
 * \param[out]  resource
 * 
 * \return true if parsed, false if not.
 */
static bool ParseUrl(char * url, char * domain, char ** resource)
{
    char *      domainStart;
    char *      domainEnd;

    // Only HTTP is currently supported
    if (strncasecmp(url,"http",4)!=0) {
        return false;
    }

    // skip the scheme, 
    domainStart = strstr(url,"://");
    if (domainStart == NULL) {
        return false;
    }
    domainStart += strlen("://");

    domainEnd = strchr(domainStart,'/');
    if (domainEnd == NULL) {
        return false;
    }

    *resource = domainEnd;

    memcpy(domain, domainStart, domainEnd-domainStart);
    domain[domainEnd-domainStart] = 0;
    return true;
}

/*!
 * \brief Parse a JSON firmware update resonse into a C structure
 * 
 * \param[in]   serverResponse - the data to parse
 * \param[out]  firmwareData   - the parsed data
 * 
 * \return true if completely parsed, false if not completely parsed.
 */
static bool ParseFirmwareUpdateData(char * serverResponse, FIRMWARE_DATA  * firmwareData) 
{
	JSON_PARSE_OBJECT rootObject;
	JSON_PARSE_OBJECT revisionObject;
	JSON_PARSE_OBJECT nextUpdateObject;
	JSON_PARSE_OBJECT firmwareObject;
    bool dataComplete = false;

    jv2_makeObjectFromString(&rootObject,serverResponse);

    // find the firmware node
    if(jv2_findObject(&rootObject,"firmware",&firmwareObject)) {
        dataComplete = true;
        
        // get freescale revision 
        if(jv2_findObject(&firmwareObject,"revision",&revisionObject)) {
            dataComplete &= jv2_getObjectUint32(&revisionObject,&firmwareData->fre_ver);
            dataComplete &= firmwareData->fre_ver != 0;
        } else {
            dataComplete=false;
        }
        // get freescale URL 
        dataComplete &= jv2_findObjectString(&firmwareObject,"fwUrl",firmwareData->fre_url);
        dataComplete &= firmwareData->fre_url[0] != 0;
        // get freescale MD5 signature 
        dataComplete &= jv2_findObjectString(&firmwareObject,"fwMd5",firmwareData->fre_md5);
        dataComplete &= firmwareData->fre_md5[0] != 0;

        // get nordic revision 
        if(jv2_findObject(&firmwareObject,"rfRevision",&revisionObject)) {
            dataComplete &= jv2_getObjectUint32(&revisionObject,&firmwareData->nor_ver);
            dataComplete &= firmwareData->nor_ver != 0;
        } else {
            dataComplete=false;
        }
        // get nordic URL 
        dataComplete &= jv2_findObjectString(&firmwareObject,"rfUrl",firmwareData->nor_url);
        dataComplete &= firmwareData->nor_url[0] != 0;
        // get nordic MD5 signature 
        dataComplete &= jv2_findObjectString(&firmwareObject,"rfMd5",firmwareData->nor_md5);
        dataComplete &= firmwareData->nor_md5[0] != 0;


        // get release date 
        dataComplete &= jv2_findObjectString(&firmwareObject,"releaseDate",firmwareData->releaseDate);
        dataComplete &= firmwareData->releaseDate[0] != 0;

       // get next update 
        if(jv2_findObject(&firmwareObject,"nextUpdate",&nextUpdateObject)) {
            dataComplete &= jv2_getObjectUint32(&nextUpdateObject,&firmwareData->update_minutes);
            dataComplete &= firmwareData->update_minutes != 0;
        } else {
            dataComplete=false;
        }
     }
    
    return dataComplete;
}

/*****************************************************************************//**
* @brief This is a callback function following connection to the server to retrieve
*       the firmware image.  The contents of the hex file is read from the server,
*       converted to binary and written to the SD drive.
*
* @param temp is a unused variable.
* @return nothing.
* @author Neal Shurmantine
* @version
* 02/17/2015    Created.
*******************************************************************************/
static uint32_t get_image_file_return(REST_CLIENT_QUERY_STRUCT_PTR p_client)
{
    int32_t bytes,len=0, used, left, total_bytes;
    char * lenStr;
    char * data;
    FILE * imageFile;
    
    char resp_code[24];
    memcpy(resp_code,p_client->buffer,24);
    resp_code[23] = 0;
    char *p_ok;
//Good:
//  HTTP/1.1 200 OK
//Not Good:
//  HTTP/1.1 404 Not Found
    p_ok = strstr(resp_code,"200 OK");

    if (p_ok == NULL) {
        printf("Firmware file not available\n");
        p_client->status = eFWU_CANT_RETRIEVE_FILE;
        return false;
    }

    lenStr = endstrstr(p_client->buffer, "Content-Length: ");
    if (lenStr) {
        len = strtol(lenStr,NULL,0);
    }
    data = endstrstr(p_client->buffer, "\r\n\r\n");

    printf("Length = %d",len);

    if (data == NULL) {
        p_client->status = eFWU_CANT_RETRIEVE_FILE;
        return false;
    }

    used = data - p_client->buffer;
    left = sizeof(p_client->buffer) - used;
    memcpy(p_client->buffer, data, left);


    imageFile = fopen(p_client->save_file_name, "w");
    if (imageFile == NULL) {
        p_client->status = eFWU_CANT_CREATE_LOCAL_FILE;
        return false;
    }

    bytes = RecvFromSocket(p_client, &p_client->buffer[left], used);
    if (bytes > 0) {
        total_bytes = left+bytes;
        fwrite(p_client->buffer, 1, total_bytes, imageFile);
    }
    
    while ((bytes > 0) && (total_bytes<len)){
        #if 0
        uint32_t        row,col,i

        i=0;
        for (row=0;row<bytes;row+=16) {
            printf("%03x: ", row);
            for (col=0;col<16;col++) {
                i = row+col;
                if (i < bytes) {
                    printf("%02x ", p_client->buffer[i]);
                } else {
                    printf("   ");
                }
            }
            for (col=0;(col<16);col++) {
                i = row+col;
                if (i < bytes) {
                    putchar(isgraph(p_client->buffer[i])?p_client->buffer[i]:'.');
                }
            }
            putchar('\n');
        }
        #else
            printf(".");
        #endif

        bytes = RecvFromSocket(p_client, p_client->buffer, sizeof(p_client->buffer));
        if (bytes > 0) {
            if (bytes != fwrite(p_client->buffer, 1, bytes, imageFile)) {
                p_client->status = eFWU_CANT_WRITE_LOCAL_FILE;
                fclose(imageFile);
                return false;
            }
            total_bytes += bytes;
        }
     }
     printf("\nFirmware image written, %d %d \n",total_bytes,len);

     fclose(imageFile);
     return true;
}



/*!
 * \brief Check to see if there is a new firmware imageto load, and if so, get it and store it on the SD card 
 * 
 * \param[in]   domain   - domain of the update server
 * \param[in]   resource - resource to request
 * \param[out]  error code  
 * 
 * \return current status.
 */
static bool GetFirmwareImage(REST_CLIENT_QUERY_STRUCT_PTR p_query)
{
    bool update = false;
    
    ConnectToServer(p_query);
    if (p_query->connection.socket) {
        if (GetResource(p_query)) {
            update = p_query->callback(p_query);
        }
        else {
            DisconnectFromServer(p_query);
        }
    }
    return update;
}


/*!
 * \brief Checks to see if the loaded file's MD5 signature is what it should be 
 * 
 * \param[in]  firmwareData - 
 * \param[out]  error code  
 *  
 * \return true if hash matches, false otherwise.
 */

static bool CheckMd5(MD5_CHECK_STRUCT_PTR  p_md5_str, eRestClientStatus *firmwareUpdateStatus)
{
    char computed_hash_string[MD5_HEX_STR_LENGTH];
    FILE * p_md5_file;
    bool result = false;

    if (md5_file(p_md5_str->boot_file_name, computed_hash_string)) {
        printf("Loaded hash string = %s\n",p_md5_str->md5_str);
        printf("Computed hash string = %s\n",computed_hash_string);
        if (strcasecmp(p_md5_str->md5_str,computed_hash_string)==0) {
            printf("Good MD5 hash string of image\n");

            p_md5_file = fopen(p_md5_str->md5_file_name, "w");
            if (p_md5_file) {
                fwrite(computed_hash_string, 1, MD5_HEX_STR_LENGTH, p_md5_file);
                fclose(p_md5_file);
                printf("%s written\n", p_md5_str->md5_file_name);
                result = true;
            }
            else {
                *firmwareUpdateStatus = eFWU_CANT_CREATE_LOCAL_FILE;
            }
        }
        else {
            printf("Error comparing hash string of image\n");
            printf("comparing lengths: %d %d\n",strlen(p_md5_str->md5_str),strlen(computed_hash_string));
            *firmwareUpdateStatus = eFWU_MD5_HASH_CHECK_ERROR;
        }
    } else {
        printf("Error computing hash string of image\n");
        *firmwareUpdateStatus = eFWU_CANT_COMPUTE_MD5_HASH_OF_FILE;
    }

    return result;
}

static bool md5_file(char * f_name, char *hash_string)
{
    unsigned char hash[MD5_DIGEST_LENGTH];
    unsigned char data[MD5_READ_BLOCK_SIZE];
    int i;
    FILE *in_file;
    MD5_CTX mdContext;
    int bytes;
    char s_str[3];

    in_file = fopen(f_name, "rb");
    if (in_file == NULL) {
        printf ("%s can't be opened.\n", f_name);
        return false;
    }

    MD5_Init (&mdContext);
    while ((bytes = fread (data, 1, MD5_READ_BLOCK_SIZE, in_file)) != 0)
        MD5_Update (&mdContext, data, bytes);
    MD5_Final (hash,&mdContext);

    hash_string[0] = 0;
    for(i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(s_str,"%02x", hash[i]);
        strcat(hash_string,s_str);
    }

    fclose(in_file);
    return true;
}



/*
Send: 
GET /api/v1/firmware?revision=393560&hardware=19 HTTP/1.1
Accept-Language: en-us
Host: homeauto.hunterdouglas.com
Content-Type: application/json
Content-Length: 0
Authorization: Basic NEZBQjZDN0E1NDU4Mzc2RjoxMjM0


Received: 
HTTP/1.1 200 OK
Date: Tue, 16 Jun 2015 14:55:59 GMT
Server: Apache-Coyote/1.1
Content-Type: application/json
Transfer-Encoding: chunked

174
{
    "firmware":
    {
        "revision":393640,
        "rfRevision":1409,
        "hardware":19000,
        "fwUrl":"http://homeauto.hunterdouglas.com/firmware/19000/393640/app.hex",
        "fwMd5":"3ba11f7885d10d795f35bdb217c7eb4f",
        "rfUrl":"http://homeauto.hunterdouglas.com/firmware/19000/393640/rf.bin",
        "rfMd5":"66395906feeb9c7b3ef4c32aaef19152",
        "requires":0,
        "releaseDate":"2015-06-11 09:47:53.000",
        "nextUpdate":10080
    }
}
0
*/
