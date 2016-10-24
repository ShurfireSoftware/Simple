/***************************************************************************//**
 * @file   NBT_NordicBootloadTask.c
 * @brief  This module provides the task that reads a new firmware file from
 *         the SD card and sends it to the Nordic in chunks.
 * 
 * @author Neal Shurmantine
 * @copyright (c) 2015 Hunter Douglas. All rights reserved.
 *
 * @date Created: 03/13/2015
 * @date Last updated: 03/13/2015
 *
 * @version
 * 03/13/2015   Created.
 ******************************************************************************/

/* Includes
*******************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>

#include "util.h"
#include "config.h"
#include "os.h"
#include "rf_serial_api.h"
#include "SCH_ScheduleTask.h"
#include "stub.h"
#include "rfu_uart.h"
#include "file_names.h"

#ifdef USE_ME
#include "shell.h"
#include <stdint.h>
#include <ipcfg.h>
#include "RMT_RemoteServers.h"
#include "IO_InputOutput.h"
#endif

//#define DEBUG_PRINT

/* Global Variables
*******************************************************************************/

/* Local Constants and Definitions
*******************************************************************************/
//sample fw file name: FW_NOR_19_00010001.bin
#define NBT_SERIAL_RX_EVENT_BIT     BIT0
#define NBT_BOOTLOAD_REQ_EVENT      BIT1
#define BOOTLOAD_TIMEOUT            5000
#define CONNECT_TIMEOUT             100
#define UPDATE_PACKET_BUFF_SIZE     240
#define MAX_CONNECT_TRIES       5
#define CONNECT_CHAR            'U'

#define	SOH	0x03
#define	ESC	0x1b

typedef enum NBT_STATE_Tag
{
    st_idle,
    st_avaiable,
    st_connect,
    st_waiting_for_request,
    st_done
}NBT_STATE_TYPE;

// firmware available packet
// +---+---+---------+--------------+--------------+-----------+-----------+---------+--------+-------+
// | F | A | product | revision LSB | revision MSB | Minor ver | Major ver | size 8  | size 16|size 24|
// +---+---+---------+--------------+--------------+-----------+-----------+---------+--------+-------+
typedef struct FW_AVAILABLE_PACKET_TAG
{
    uint8_t len;
    uint8_t hdr1;
    uint8_t hdr2;
    uint8_t product;
    uint8_t ver_0_7;
    uint8_t ver_8_15;
    uint8_t ver_16_23;
    uint8_t ver_24_31;
    uint8_t size_0_7;
    uint8_t size_8_15;
    uint8_t size_16_23;
}  __attribute__((packed)) FW_AVAILABLE_PACKET_STRUCT;

// firmware update packet
// +---+---+----------------+-----------+-----------+------+
// + F | B | block size 2^n | block lsb |  block msb| data |
// +---+---+----------------+-----------+-----------+------+
typedef struct FW_UPDATE_PACKET_TAG
{
    uint8_t len;
    uint8_t hdr1;
    uint8_t hdr2;
    uint8_t block_size;
    uint8_t block_ptr_0_7;
    uint8_t block_ptr_8_15;
    uint8_t data_buff[UPDATE_PACKET_BUFF_SIZE];
}  __attribute__((packed)) FW_UPDATE_PACKET_STRUCT, *FW_UPDATE_PACKET_STRUCT_PTR;

// firmware request packet
// +---+---+----------------+----------+-----------+
// | F | R | block size 2^n | block lsb| block msb |
// +---+---+----------------+----------+-----------+
typedef struct FW_REQUEST_TAG
{
    uint8_t hdr1;
    uint8_t hdr2;
    uint8_t size;
    uint8_t block_0_7;
    uint8_t block_8_15;
}  __attribute__((packed)) FW_REQUEST_STRUCT, *FW_REQUEST_STRUCT_PTR;


// define decode states
// Idle - looking for a SOH, all else is ignored
// length - have SOH now get the length
// collect - have length now collect the rest
// escape - saw an esc and need to process next byte
// escape length - saw and esc in the length byte, special since it's the length
typedef enum parseState_tag
{
    idle = 0,
    length,
    collect,
    escape,
    escapeLength,
    end
}parseState_Type;

// async protocol
// +-----+--------+---------+
// | SOH | LENGTH | PAYLOAD |
// +-----+--------+---------+
// Where ESC (0x1b) is used to mask SOH and ESC
// SOH will never be without ESC

typedef enum Encoder_Return_Tag
{
    rt_okay,
    rt_msg_ready,
    rt_failed
}Encoder_Return_Type;

// incomming data is the same as RF packet payload
// length - payload
// +--------+---------+
// + length | payload |
// +--------+---------+
//

typedef struct Packet_Tag
{	
    uint8_t	size;
    uint8_t	payload[];
}Packet_Type;

typedef union Packet_Raw_Tag
{
    uint8_t	raw[256];
    Packet_Type	packet;
}Packet_Raw_Type;


/* Local Function Declarations
*******************************************************************************/
static Encoder_Return_Type	decode(uint8_t input);
static Encoder_Return_Type	encode(uint8_t *input, uint8_t *output, uint8_t *sizeofoutput);
static bool nbt_process_msg(void);
static int32_t file_get_block(uint32_t block_size, 
                        uint32_t block_number, 
                        uint8_t *p_buff);
static void nbt_send_fw_avail_packet(void);
static void nbt_reset_nordic(uint16_t dummy);
static void nbt_delete_md5(void);
static void nbt_delete_ver_file(void);
static uint32_t NBT_get_image_file_size(void);
//DEBUG:
static void print_data(uint8_t * msg_str, uint32_t len);

/* Local variables
*******************************************************************************/
static uint32_t NBT_WaitTime;
static uint16_t NBT_ExpectedEvents;
static void *NBT_EventHandle;
static uint32_t NBT_Version;
static uint32_t NBT_FileSize;
static NBT_STATE_TYPE NBT_State = st_idle;
static uint8_t NBT_SendBuff[256];
static bool NBT_Success;

// start looking for SOH
static parseState_Type CurrentParseState = idle;
// Using memory here to hold instance of packet
static Packet_Raw_Type  EncoderPacketOut; //Packet_Type
// index into payload being received
static uint8_t	parseIndex = 0;


/*****************************************************************************//**
* @brief Kicks off the download of new firmware to the Nordic on the Hub.
*   (sample fw file name: FW_NOR_19_00010001.bin)
* @param f_name.  Name of file to download.
* @return true if process began.
* @author Neal Shurmantine
* @since 03/13/2015
* @version Initial revision.
*******************************************************************************/
void NBT_BeginNordicDownload(void)
{
    NBT_FileSize = NBT_get_image_file_size();
    if (NBT_FileSize == 0) {
        printf("RF image file is blank\n");
        return;
    }
    NBT_Version = NBT_GetNordicFirmwareVersion();
    if (NBT_Version != 0) {
        printf("Begin Nordic Download\n");
        NBT_State = st_connect;
        OS_EventSet(NBT_EventHandle,NBT_BOOTLOAD_REQ_EVENT);
        LED_NordicFlash(true);
        NBT_Success = false;
    }
}

bool NBT_IsNordicDownloadActive(void)
{
    if (NBT_State == st_idle) {
        return false;
    }
    else {
        return true;
    }
}

static uint32_t NBT_get_image_file_size(void)
{
    struct stat buf;
    stat(RF_IMAGE_FILENAME, &buf);
    return buf.st_size;
}

bool NBT_VerifyNordicFiles(void)
{
    bool rslt;
    FILE * p_file = NULL;

    p_file = fopen(RF_IMAGE_FILENAME,"r");
    if (p_file != NULL) {
        rslt = true;
        fclose(p_file);
    }
    else {
        rslt = false;
    }
    if (rslt == true) {
        p_file = fopen(RF_MD5_FILE,"r");
        if (p_file != NULL) {
            fclose(p_file);
        }
        else {
            rslt = false;
        }
    }
    if (rslt == true) {
        p_file = fopen(RF_VERSION_FILE,"r");
        if (p_file != NULL) {
            fclose(p_file);
        }
        else {
            rslt = false;
        }
    }

    return rslt;
}

uint32_t NBT_GetNordicFirmwareVersion(void)
{
    uint32_t version = 0;
    char *rtn;
    char ver_str[10];

    FILE * ver_file = fopen(RF_VERSION_FILE, "r");
    ver_str[0] = 0;
    if (ver_file != NULL) {
        rtn = fgets(ver_str,sizeof(ver_str), ver_file);
        if (rtn == ver_str) {
            version = atoi(ver_str);
        }
        fclose(ver_file);
    } 

    return version;
}

/*****************************************************************************//**
* @brief This function initializes the Nordic bootload task and holds the 
*   the main loop for the task.  
*
* @param temp is a unused variable.
* @return nothing.
* @author Neal Shurmantine
* @since 03/13/2015
* @version Initial revision.
*******************************************************************************/
void *nbt_nordic_download_task(void * temp)
{ 
    uint16_t event_active;
    uint8_t c;
    uint16_t connect_count;
    char connect_char = CONNECT_CHAR;

    NBT_EventHandle = RFU_Register_Bootload_Event(0, NBT_SERIAL_RX_EVENT_BIT);

    NBT_ExpectedEvents = NBT_BOOTLOAD_REQ_EVENT | NBT_SERIAL_RX_EVENT_BIT;
    NBT_WaitTime = WAIT_TIME_INFINITE;
printf("nbt_nordic_download_task\n");
    while(1)
    {
        event_active = OS_TaskWaitEvents(NBT_EventHandle, NBT_ExpectedEvents, NBT_WaitTime);
        event_active &= NBT_ExpectedEvents;
        if (event_active & NBT_BOOTLOAD_REQ_EVENT)
        {
            OS_EventClear(NBT_EventHandle,NBT_BOOTLOAD_REQ_EVENT);
            if (NBT_State == st_connect) {
                connect_count = 0;
                RFU_SetBootloadActive(true);
                RFU_SendMsg(1,&connect_char);
                NBT_WaitTime = CONNECT_TIMEOUT;
            }
            else if (NBT_State == st_avaiable) {
                nbt_send_fw_avail_packet();
            }
            else if (NBT_State == st_waiting_for_request) {
                if (nbt_process_msg() == true) {

                }
                else {
//FIX ME - handle error
                }
            }
            else if (NBT_State == st_done) {
printf("Exit\n");
                NBT_WaitTime = WAIT_TIME_INFINITE;
                RFU_SetBootloadActive(false);
                LED_NordicFlash(false);
                if (NBT_Success == false) {
                    NBT_State = st_idle;
                    RC_ResetRadio();
                }
            }
        }
        if (event_active & NBT_SERIAL_RX_EVENT_BIT)
        {
            if (RFU_GetRxChar(&c) == true) {
                if (NBT_State == st_connect) {
                    if (c == CONNECT_CHAR) {
                        NBT_WaitTime = BOOTLOAD_TIMEOUT;
                        NBT_State = st_avaiable;
                        OS_EventSet(NBT_EventHandle,NBT_BOOTLOAD_REQ_EVENT);
                    }
                }
                else {
                    if (decode(c) == rt_msg_ready) {
                        OS_EventSet(NBT_EventHandle,NBT_BOOTLOAD_REQ_EVENT);
                    }
                }
            }
        }
        if (event_active == 0) {
            if ((NBT_State == st_connect) 
                && (++connect_count < MAX_CONNECT_TRIES)) {
                    RFU_SendMsg(1,&connect_char);
            }
            else {
if (connect_count == MAX_CONNECT_TRIES) {
printf("Did Not Sync\n");
}
else {
printf("connect count=%d\n",connect_count);
}
                NBT_State = st_done;
                OS_EventSet(NBT_EventHandle,NBT_BOOTLOAD_REQ_EVENT);
            }
        }
    }
}

/*****************************************************************************//**
* @brief Send the Firmware Available Packt to the Nordic.  
*
// firmware available packet
// +---+---+---------+--------------+--------------+-----------+-----------+---------+--------+-------+
// | F | A | product | revision LSB | revision MSB | Minor ver | Major ver | size 8  | size 16|size 24|
// +---+---+---------+--------------+--------------+-----------+-----------+---------+--------+-------+
*
* @param temp is a unused variable.
* @return nothing.
* @author Neal Shurmantine
* @since 03/13/2015
* @version Initial revision.
*******************************************************************************/
static void nbt_send_fw_avail_packet(void)
{
    uint8_t size;
    FW_AVAILABLE_PACKET_STRUCT packet;
    packet.hdr1 = 'F';
    packet.hdr2 = 'A';
    packet.product = atoi(HW_VERSION);
    packet.ver_0_7 = (uint8_t)( 0xff & NBT_Version);
    packet.ver_8_15 = (uint8_t)( 0xff & NBT_Version>>8);
    packet.ver_16_23 = (uint8_t)( 0xff & NBT_Version>>16);
    packet.ver_24_31 = (uint8_t)( 0xff & NBT_Version>>24);
    packet.size_0_7 = (uint8_t)( 0xff & NBT_FileSize);
    packet.size_8_15 = (uint8_t)( 0xff & NBT_FileSize>>8);
    packet.size_16_23 = (uint8_t)( 0xff & NBT_FileSize>>16);
    packet.len = sizeof(packet) - 1;
    encode((uint8_t*)&packet,NBT_SendBuff,&size);
    RFU_SendMsg(size,(char*)NBT_SendBuff);

    print_data((uint8_t*)&packet, sizeof(packet));

    NBT_State = st_waiting_for_request;
}

static void nbt_reset_nordic(uint16_t dummy)
{
printf("Resetting Nordic...\n");
    OS_TaskSleep(5);
    nbt_delete_md5();
    nbt_delete_ver_file();
    RC_ResetRadio();
    NBT_State = st_idle;
}

static void nbt_delete_md5(void)
{
    FILE * p_file;

    p_file = fopen(RF_MD5_FILE,"r");
    if (p_file != NULL) {
        fclose(p_file);
        remove(RF_MD5_FILE);
        printf("Deleting Nordic MD5 File\n");
    }
}

static void nbt_delete_ver_file(void)
{
    FILE * p_file;

    p_file = fopen(RF_VERSION_FILE,"r");
    if (p_file != NULL) {
        fclose(p_file);
        remove(RF_VERSION_FILE);
        printf("Deleting Nordic Ver File\n");
    }
}

/*****************************************************************************//**
* @brief Process firmware request packet and send firmware update packet
*
*  Request from Nordic:
// +---+---+----------------+----------+-----------+
// | F | R | block size 2^n | block lsb| block msb |
// +---+---+----------------+----------+-----------+
//
*  Update to Nordic:
// +---+---+----------------+-----------+-----------+-------//-------+
// + F | B | block size 2^n | block lsb |  block msb|      data      |
// +---+---+----------------+-----------+-----------+-------//-------+
*
*
* @param none.  Uses content of update_packet.
* @return nothing.
* @author Neal Shurmantine
* @since 03/13/2015
* @version Initial revision.
*******************************************************************************/
static bool nbt_process_msg(void)
{
    bool rtn = false;
    uint32_t block_size;
    uint32_t block_pointer;
    int32_t count;
    uint8_t size;
    FW_UPDATE_PACKET_STRUCT update_packet;
    FW_REQUEST_STRUCT_PTR p_req_packet;
    p_req_packet = (FW_REQUEST_STRUCT_PTR)&EncoderPacketOut.packet.payload[0];

#ifdef DEBUG_PRINT
    for (int n = 0; n < EncoderPacketOut.packet.size; ++n) {
        printf("%02X ",EncoderPacketOut.packet.payload[n]);
    }
    printf("\n");
#endif
    update_packet.hdr1 = 'F';
    update_packet.hdr2 = 'B';
    if ( EncoderPacketOut.packet.size == sizeof(FW_REQUEST_STRUCT)) {
        block_size = (uint32_t)p_req_packet->size;
        block_pointer = (uint32_t)p_req_packet->block_0_7
                        + (uint32_t)p_req_packet->block_8_15 * 0x100;
        if (block_pointer == 0xffff) {
            NBT_State = st_done;
printf("Download Complete\n");
NBT_Success = true;
            SCH_ScheduleEventPostSeconds(2, nbt_reset_nordic);
            OS_EventSet(NBT_EventHandle,NBT_BOOTLOAD_REQ_EVENT);
        }
        else {
//printf("%08x\n",block_pointer);
            count = file_get_block(1<<(uint32_t)block_size, block_pointer, update_packet.data_buff);
            if (count != IO_ERROR) {
                update_packet.block_size = p_req_packet->size;
                update_packet.block_ptr_0_7 = p_req_packet->block_0_7;
                update_packet.block_ptr_8_15 = p_req_packet->block_8_15;
                update_packet.len = (uint8_t)sizeof(FW_UPDATE_PACKET_STRUCT) 
                                    - UPDATE_PACKET_BUFF_SIZE - 1 + count;
                encode((uint8_t*)&update_packet,NBT_SendBuff,&size);
                RFU_SendMsg(size,(char*)NBT_SendBuff);

                print_data((uint8_t*)&update_packet, update_packet.len+1);

                rtn = true;
            }
        }
    }
    return rtn;
}

/*****************************************************************************//**
* @brief Read a block of data from a file  
*
* @param f_name.  Name of file to read.
* @param block_size.  Number of bytes in the block.
* @param block_number.  How many blocks into the file to start reading.
* @param p_buff.  Pointer to buffer where data is stored.
* @return number of bytes read.
* @author Neal Shurmantine
* @since 03/13/2015
* @version Initial revision.
*******************************************************************************/
static int32_t file_get_block(uint32_t block_size, uint32_t block_number, uint8_t *p_buff)
{
    uint32_t p_addr;
    uint32_t char_count;

    p_addr = block_size * block_number;
#ifdef USE_ME    
    FILE * p_file = NULL;

    p_file = fopen(RF_IMAGE_FILENAME,"r");
    if (p_file == NULL) {
        ReleaseSDCard();
        return IO_ERROR;
    }
    if (fseek(p_file,p_addr,IO_SEEK_SET) == IO_ERROR) {
        fclose(p_file);
        ReleaseSDCard();
        return IO_ERROR;
    }
    char_count = _io_read(p_file, p_buff, block_size);
    if (char_count != IO_ERROR) {
        for (int i = char_count; i < block_size; ++i) {
            p_buff[i] = 0xff;
        }
        char_count = block_size;
    }
    fclose(p_file);
    return char_count;
#else
    return 0;
#endif
}


//--------------------------------------DEBUG Start--------------------------------------------

static void print_data(uint8_t * msg_str, uint32_t len)
{
#ifdef DEBUG_PRINT
    for (uint8_t c = 0; c < len; ++c)
    {
        printf("%02X ",msg_str[c]);
    }
    printf("\r\n");
#endif
}

// firmware available packet
// +---+---+---------+--------------+--------------+-----------+-----------+---------+--------+-------+
// | F | A | product | revision LSB | revision MSB | Minor ver | Major ver | size 8  | size 16|size 24|
// +---+---+---------+--------------+--------------+-----------+-----------+---------+--------+-------+
uint8_t test_str1[] = {0x00, 0x46, 0x41, 0x05, 0x01, 0x02, 0x03, 0x04, 0x00, 0x01, 0x00};
void rfo_test_btl_encode(void)
{
    uint8_t size;
    uint8_t send_buff[128];
    uint8_t len;
    
    len = sizeof(test_str1) - 1;
    test_str1[0] = len;
    encode(test_str1,send_buff, &size);
    RFU_SendMsg(size,(char*)send_buff);
}

//--------------------------------------DEBUG End--------------------------------------------



/***************************************************************************//**
 * @file encoder.c
 * @brief Packet encoding/decoding for PC downloading firmware updates
 *
 * @author Paul Ward
 * @copyright (c) 2014 -2015 Hunter Douglas. All rights reserved.
 *
 * @date Created: 7/14/2014
 * @date Last Modified: 2/19/2015
 * *
 ******************************************************************************/

/*
 * Change Log:
 * 2015-02-19 Added comments
 * 2014-07-14 [PBW] Creation
 */
 

/*! ***************************************************************************
 * @brief encode data with SOH and ESC
 *
 * @Details
 * Encode data stream
 * data consists of length | payload
 * encode adds an SOH to output
 * encode will replace (equilivant) SOH and ESC within the payload to ESC 
 * ESC+SOH  or ESC ESC+ESC so that SOH is unique
 * encoded data will always be at least +1 in length and could be x 2 if every 
 * character matches SOH or ESC
 * Decoder will strip ESC from data and return to original value via ESC-
 * byte sized value should roll over so any value may be used for SOH or ESC
 * @param uint8_t * input - data to be encoded
 * 				uint8_t * output - pointer to encoded data 
 * 				uint8_t * sizeofoutput - number of bytes of encoded output
 * @return Encoder_Return_Type
 * @warning (256) don't overflow output buffer or all carp swim loose
 *****************************************************************************/
static Encoder_Return_Type	encode(uint8_t *input, uint8_t *output, uint8_t *sizeofoutput)
{
    uint8_t	length = *input;
    uint8_t	index;
    uint8_t	count = 1;
    *output++ = SOH;
        
    for(index = 0; length >= index; index++)
    {
        if(*input == SOH || *input == ESC)
        {
            *output++ = ESC;
            count++;
            *output++ = *input++ + ESC;
            count++;
        }
        else
        {
            *output++ = *input++;
            count++;
        }
    }
    *sizeofoutput = count;
    return	rt_okay;
}

/*! ***************************************************************************
 * @brief decode data stripping off SOH and ESC sequences
 *
 * @Details
 * Decoding of packet data consists of a byte driven state machine
 * Receive byte events from the serial port call encode to process the stream.
 * Each byte from the input stream is decoded so that the SOH will identify the
 * next byte as the length
 * sequences are striped of ESC
 * ANY indication of a SOH resets the state machine to the start
 * The length is set after a SOH and will also be ESC if necessary
 * Collect stashes the payload
 * Once the length has been met the packet done flag is set and the state machine returns
 * to idle.
 * If the packet data is not processed before another packet is received the current packet
 * will be lost.
 * This is not expected in a command/response environment but if needed a queue may be added.
 *
 * @param uint8_t	input - byte to decode
 *
 * @return Encoder_Return_Type
 *****************************************************************************/
static Encoder_Return_Type	decode(uint8_t input)
{
    Encoder_Return_Type rtn = rt_okay;
    switch(CurrentParseState)
    {
        case idle:
            if(input == SOH)
            {
                CurrentParseState = length;
            }
            break;
        case length:
            if(input == SOH)
            {
                break;
            }
            if(input == ESC)
            {
                CurrentParseState = escapeLength;
                break;
            }
            EncoderPacketOut.packet.size = input;
            parseIndex = 0;
            CurrentParseState = collect;
            break;
        case collect:
            if(input == SOH)
            {
                CurrentParseState = length;
                break;
            }
            if(input == ESC)
            {
                CurrentParseState = escape;
                break;
            }
            EncoderPacketOut.packet.payload[parseIndex++] = input;
            if(parseIndex >= EncoderPacketOut.packet.size)
            {
                rtn = rt_msg_ready;
                CurrentParseState = idle;
            }
            break;
        case escape:
            EncoderPacketOut.packet.payload[parseIndex++] = input - ESC;
            if(parseIndex >= EncoderPacketOut.packet.size)
            {
                rtn = rt_msg_ready;
                CurrentParseState = idle;
            }
            else
            {
                CurrentParseState = collect;
            }
            break;
        case escapeLength:
            EncoderPacketOut.packet.size = input - ESC;
            parseIndex = 0;
            CurrentParseState = collect;
            break;
        default:
        case end:
            break;
    }
    
    return rtn;
}

