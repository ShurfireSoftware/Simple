/***************************************************************************//**
 * @file   RC_RadioConfig.c
 * @brief  This module manages the configuration of the Nordic Stack Parameters.
 * 
 * @author Neal Shurmantine
 * @copyright (c) 2014 Hunter Douglas. All rights reserved.
 *
 * @version
 * 11/17/2014   Created.
 * 
 ******************************************************************************/

/* Includes
*******************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/stat.h>
//#include <fcntl.h>
#include <unistd.h>

#include "rf_serial_api.h"
#include "util.h"
#include <time.h>

#include "os.h"
#include "file_names.h"
#include "stub.h"
#include "LOG_DataLogger.h"
#include "rfo_outbound.h"

/* Local Constants and Definitions
*******************************************************************************/
#define NORDIC_FILE_LOC 0

const P3_UInt32_Type DEFAULT_PHY_BASE0 = 0xffff;
const P3_UInt32_Type DEFAULT_PHY_BASE1 =  0xFFFF;
const P3_UInt8_Type DEFAULT_PHY_Prefix_0 =  0xFF;
const P3_UInt8_Type DEFAULT_PHY_Prefix_1 =  0xFF;
const P3_UInt8_Type DEFAULT_PHY_Prefix_2 =  0xFF;
const P3_UInt8_Type DEFAULT_PHY_Prefix_3 =  0xFF;
const P3_UInt8_Type DEFAULT_PHY_Prefix_4 =  0xFF;
const P3_UInt8_Type DEFAULT_PHY_Prefix_5 =  0xFF;
const P3_UInt8_Type DEFAULT_PHY_Prefix_6 =  0xFF;
const P3_UInt8_Type DEFAULT_PHY_Prefix_7 =  0xFF;
const P3_UInt8_Type DEFAULT_PHY_Current_Bitrate =  0xFF;
const P3_UInt8_Type DEFAULT_PHY_Current_Frequency =  0xFF;
const P3_Int8_Type DEFAULT_PHY_TX_Power =  0;
const P3_UInt8_Type DEFAULT_PHY_8BT_Time_uS =  0xFF;
const P3_UInt8_Type DEFAULT_DLL_Promiscuous_Mode =  0;
const P3_UInt8_Type DEFAULT_DLL_Low_Power =  0;
const P3_UInt16_Type DEFAULT_DLL_Network_Id =  0xFFFF;
const P3_UInt16_Type DEFAULT_DLL_Device_Id =  0;
const P3_UInt8_Type DEFAULT_DLL_SEQ_Number =  0x10;
const P3_UInt8_Type DEFAULT_DLL_Max_Backoff_Count =  7;
const P3_UInt8_Type DEFAULT_DLL_Max_Backoff_Exp =  7;
const P3_UInt8_Type DEFAULT_DLL_Min_Backoff_Exp =  3;
const P3_UInt8_Type DEFAULT_DLL_Ack_TO_Time_8BT =  0xFF;
const P3_UInt8_Type DEFAULT_DLL_RX_On_Time_uS =  0xFF;
const P3_UInt32_Type DEFAULT_DLL_TX_Persist_Time_uS =  0xFF;
const P3_Byte_Type DEFAULT_DLL_Group_Bitfield[32] =  {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
const P3_UInt32_Type DEFAULT_DLL_TX_Burst_Time_mS =  0xFF;
const P3_UInt8_Type DEFAULT_NET_SEQ_Number =  0;

#define I_AM_PROGRAMMED_DEFAULT 0x34

#define RC_DEFAULT_FREQ     0x07
#define RC_DEFAULT_BIT_RATE 0x00

static RC_NV_STRUCT_TYPE RC_NonVolatile;

const P3_Attribute_Config_Type AttributeSetup[] =
{
    { Attribute_DLL_Low_Power, sizeof(DEFAULT_DLL_Low_Power), (void*)&RC_NonVolatile.DllLowPower},
    { Attribute_DLL_Network_Id, sizeof(DEFAULT_DLL_Network_Id), (void*)&RC_NonVolatile.DllNetworkId},
    { Attribute_DLL_Device_Id, sizeof(DEFAULT_DLL_Device_Id), (void*)&RC_NonVolatile.DllDeviceId},
    { Attribute_DLL_Group_Bitfield, sizeof(DEFAULT_DLL_Group_Bitfield), (void*)&DEFAULT_DLL_Group_Bitfield},
    { (P3_Attribute_Type)0, 0, (void*)0}
};

typedef enum RC_PROG_STATE_TAG
{
    RC_STATE_RESET,
    RC_STATE_GET_CONFIG,
    RC_STATE_SET_CONFIG,
    RC_STATE_START,
    RC_STATE_READ_SEQ,
    RC_STATE_SET_SEQ
} RC_PROG_STATE_ENUM;

/* Local Function Declarations
*******************************************************************************/
static void rc_set_attribute(P3_Attribute_Config_Type_Ptr p_attribute);
static void rc_get_attribute(P3_Attribute_Type attribute);
static void rc_send_start(void);
static void rc_process_startup_confirmation(void *p_msg);
static bool rc_get_cfg_memory(void);
static void rc_release_cfg_memory(void);
static void RC_set_nordic_defaults(void);
static void RC_nv_file_write(void);
static bool RC_nv_file_read(void);

/* Local variables
*******************************************************************************/
static uint16_t InitIndex;
static RNC_CONFIG_REC_PTR pCurrentCfgRec;
static bool RC_RadioReady=false;
static uint8_t MessageBuffer[MAX_RADIO_CONFIG_SER_MESSAGE];
static uint64_t RC_NordicUuid = 0;
static RC_PROG_STATE_ENUM RCProgState;
static uint16_t RC_FreeCfgRecCount = 0;
//static void(*p_RC_callback)(void);

/*****************************************************************************//**
* @brief This function runs in the context of the calling task and is called<br/>
* before the RF handling tasks have been started.  Its purpose is to recover the<br/>
* RF stack attribute settings from non-volatile memory or write defaults to memory<br/>
* on first bootup following factory firmware programming.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @since 11/21/2014
* @version Initial revision.
*******************************************************************************/
void RC_InitRadio(void)
{
    //set the "reset" line to Nordic as high impedence
#ifdef USE_ME
    LWGPIO_STRUCT nordic_reset;
    lwgpio_init(&nordic_reset,   (GPIO_PORT_B | GPIO_PIN10),   LWGPIO_DIR_INPUT , LWGPIO_VALUE_NOCHANGE);
    lwgpio_set_functionality(&nordic_reset, LWGPIO_MUX_GPIO);
#endif
    if (RC_IsSerialFlashProgrammed() == false) {
        RC_set_nordic_defaults();
        RC_nv_file_write();
    }
}

/*****************************************************************************//**
* @brief This function checks the serial flash to see if the I_Am_Programmed
*   location in serial flash contains a valid value.
*
* @param none.
* @return bool. Returns true if the serial flash has already been programmed.
* @author Neal Shurmantine
* @since 04/28/2015
* @version Initial revision.
*******************************************************************************/
bool RC_IsSerialFlashProgrammed(void)
{
    RC_nv_file_read();
    if (RC_NonVolatile.I_Am_Programmed != I_AM_PROGRAMMED_DEFAULT) {
        return false;
    }
    else {
        return true;
    }
}

/*****************************************************************************//**
* @brief This function writes a new network ID to the Nordic and to serial flash.
*
* <b>Note:</b>  This function runs in the context of the calling task.
* @param p_status is a pointer to a uint16_t.
* @return nothing.
* @author Neal Shurmantine
* @since 2014-11-11
* @version Initial revision.
*******************************************************************************/
void RC_AssignNewNetworkId(uint16_t new_id)
{
    RC_NonVolatile.DllNetworkId = new_id;
    RC_nv_file_write();
    RC_ResetRadio();
}

/*****************************************************************************//**
* @brief This function starts the soft reset sequence and RF stack attribute<br/>
*  programming of the Nordic.  The end of this process is the Nordic ready to <br/>
*  transmit and receive RF messages.
*
* <b>Note:</b>  This function runs in the context of the calling task.
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @since 2014-11-11
* @version Initial revision.
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |     0x02      |      0x1D     | RESET_ATTRIB  |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*******************************************************************************/
void RC_ResetRadio(void)
{
    RC_RadioReady = false;
    if (rc_get_cfg_memory() == true) {
        pCurrentCfgRec->expected_msg_response = MSG_TYPE_RESET_CONF;
        pCurrentCfgRec->serial_timeout = 10;
        pCurrentCfgRec->p_callback = rc_process_startup_confirmation;
        RCProgState = RC_STATE_RESET;
        MessageBuffer[0] = 0x02;
        MessageBuffer[1] = MSG_TYPE_RESET_REQ;
        MessageBuffer[2] = 0x01;
        RNC_AddTransportLayer(pCurrentCfgRec->ser_msg, MessageBuffer);
printf("RC_ResetRadio\n");
        RNC_SendNordicConfigRequest(pCurrentCfgRec);
    }
}

/*****************************************************************************//**
* @brief Restores the Nordic to factory defaults.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 11/25/2014    Created.
*******************************************************************************/
void RC_RestoreRadioDefault(void)
{
    RC_set_nordic_defaults();
    RC_nv_file_write();
    RC_InitRadio();
    LED_NetworkID(false);
//FIX ME
//  call RC_ResetRadio() ?
}

/*****************************************************************************//**
* @brief This function is called to determine if the Nordic is up and running,<br/>
*  waiting to receive and transmit RF messages.
*
* @param p_attribute is a pointer to a structure of type P3_Attribute_Config_Type_Ptr.
* @return nothing.
* @author Neal Shurmantine
* @since 11/21/2014
* @version Initial revision.
*******************************************************************************/
bool RC_IsRadioOn(void)
{
    return RC_RadioReady;
}

/*****************************************************************************//**
* @brief This function is called to request the Nordic firmware versoin.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @since 03/22/2016
* @version Initial revision.
*******************************************************************************/
void RC_RequestVersion(void)
{
    if (rc_get_cfg_memory() == true) {
        pCurrentCfgRec->expected_msg_response = MSG_TYPE_SYSTEM_INDICATION;
        pCurrentCfgRec->serial_timeout = 10;
        MessageBuffer[0] = 0x02;
        MessageBuffer[1] = MSG_TYPE_SYSTEM;
        MessageBuffer[2] = MSG_TYPE_SYSTEM_VERSION;
        RNC_AddTransportLayer(pCurrentCfgRec->ser_msg, MessageBuffer);

printf("RC_RequestVersion\n");
        RNC_SendNordicConfigRequest(pCurrentCfgRec);
    }
}

/*****************************************************************************//**
* @brief This function is called if a serial message was received from the nordic
*   indicating it was just reset.
*
* @param p_val. Unused, memory should be released.
* @return nothing.
* @author Neal Shurmantine
* @version
* 05/05/2015    Created.
*******************************************************************************/
void RC_HandleResetIndication(SYSTEM_INDICATION_STRUCT_PTR p_val)
{
//FIX ME - update inteface spec
    OS_ReleaseMsgMemBlock(p_val);
    LOG_LogEvent("Nordic Reset Received");
    RC_ResetRadio();
}

/*****************************************************************************//**
* @brief This function is called if a serial message was received from the nordic
*   in response to a version request.
*
* @param p_val. pointer to system payload
* @return nothing.
* @author Neal Shurmantine
* @version
* 03/22/2016    Created.
*******************************************************************************/
void RC_HandleVersionIndication(SYSTEM_INDICATION_STRUCT_PTR p_val)
{
    uint32_t *p_version;
    p_version = (uint32_t*)&p_val->payload;
    printf("Nordic version = 0x%08X\n",*p_version);
    OS_ReleaseMsgMemBlock(p_val);
}

/*****************************************************************************//**
* @brief This function forces Nordic to be reset by switching the "reset" line
*    from high impedence to low for 50 milliseconds.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @version
* 05/05/2015    Created.
*******************************************************************************/
void RC_ForceNordicReset(void)
{
#ifdef USE_ME
    LWGPIO_STRUCT nordic_reset;
    lwgpio_init(&nordic_reset,   (GPIO_PORT_B | GPIO_PIN10),   LWGPIO_DIR_OUTPUT , LWGPIO_VALUE_LOW);
    lwgpio_set_functionality(&nordic_reset, LWGPIO_MUX_GPIO);
    OS_TaskSleep(50);
    lwgpio_init(&nordic_reset,   (GPIO_PORT_B | GPIO_PIN10),   LWGPIO_DIR_INPUT , LWGPIO_VALUE_NOCHANGE);
    lwgpio_set_functionality(&nordic_reset, LWGPIO_MUX_GPIO);
    LOG_LogEvent("Forced Nordic Reset");
#endif
}

/*****************************************************************************//**
* @brief This function returns the currently assigned network ID.
*
* @param p_attribute is a pointer to a structure of type P3_Attribute_Config_Type_Ptr.
* @return nothing.
* @author Neal Shurmantine
* @since 11/21/2014
* @version Initial revision.
*******************************************************************************/
uint16_t RC_GetNetworkId(void)
{
    return RC_NonVolatile.DllNetworkId;
}

/*****************************************************************************//**
* @brief This function creates a random network ID. Values of 0, 0xffff, 0x1111</br>
*        are avoided.
*
* @param none.
* @return new network ID.
* @author Neal Shurmantine
* @since 11/21/2014
* @version Initial revision.
*******************************************************************************/
uint16_t RC_CreateRandomNetworkId(void)
{
    clock_t time;
    uint16_t seed;
    uint16_t new_nid;

    do {
        time = clock();
        seed = (uint16_t)time;
        srand(seed);
        new_nid = rand()%0x10000;
    } while ((new_nid == FACTORY_DEFAULT_NETWORK_ID)
                || (new_nid == ALL_NETWORK_ID)
                || (new_nid == 0) );
    return new_nid;
}

/*****************************************************************************//**
* @brief This function is called to determine if the network ID is not a default</br>
*        value.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @since Feb, 2015
* @version Initial revision.
*******************************************************************************/
bool RC_IsNetworkIdAssigned(void)
{
    bool rtn_val;
    switch (RC_NonVolatile.DllNetworkId)
    {
        case ALL_NETWORK_ID:
        case FACTORY_DEFAULT_NETWORK_ID:
        case 0:
            rtn_val = false;
            break;
        default:
            rtn_val = true;
            break;
    }
    return rtn_val;
}

/*****************************************************************************//**
* @brief This function returns the 64 bit UUID of the Nordic.
*
* @return 64 bit UUID.
* @author Neal Shurmantine
* @since 01/09/2015
* @version Initial revision.
*******************************************************************************/
uint64_t RC_GetNordicUuid(void)
{
    return RC_NordicUuid;
}

/*****************************************************************************//**
* @brief This function creates and sends a command to the Nordic to set
*       an attribute parameter.
*
* @param p_attribute is a pointer to a structure of type P3_Attribute_Config_Type_Ptr.
* @return nothing.
* @author Neal Shurmantine
* @since 11/21/2014
* @version Initial revision.
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |      SIZE     |  TYPE: 0x06   | P3_Attribute_Type |           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+               |
    |                         VALUE0..VALUEn                        |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*******************************************************************************/
static void rc_set_attribute(P3_Attribute_Config_Type_Ptr p_attribute)
{
    uint16_t i;
    uint8_t *p_value;
    p_value = p_attribute->value;
       
    if (rc_get_cfg_memory() == true) {
        pCurrentCfgRec->expected_msg_response = MSG_TYPE_SET_ATTR_CONF;
        pCurrentCfgRec->serial_timeout = 10;
        MessageBuffer[0] = p_attribute->size+2;
        MessageBuffer[1] = MSG_TYPE_SET_ATTR_REQ;
        MessageBuffer[2] = p_attribute->type;

        for (i = 0; i < p_attribute->size; ++i) {
            MessageBuffer[3+i] = p_value[i];
        }
        RNC_AddTransportLayer(pCurrentCfgRec->ser_msg, MessageBuffer);

printf("rc_set_attribute\n");
        RNC_SendNordicConfigRequest(pCurrentCfgRec);
    }
}

/*****************************************************************************//**
* @brief This function creates and sends a message to the Nordic to get
*     an attribute.
*
* @param p_attribute is a pointer to a structure of type P3_Attribute_Config_Type_Ptr.
* @return nothing.
* @author Neal Shurmantine
* @since 11/21/2014
* @version Initial revision.
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |      0x02     |      0x04     | P3_Attr_Type  |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*******************************************************************************/
static void rc_get_attribute(P3_Attribute_Type attribute)
{
    if (rc_get_cfg_memory() == true) {
        pCurrentCfgRec->expected_msg_response = MSG_TYPE_GET_ATTR_CONF;
        pCurrentCfgRec->serial_timeout = 10;
        MessageBuffer[0] = 2;
        MessageBuffer[1] = MSG_TYPE_GET_ATTR_REQ;
        MessageBuffer[2] = attribute;
        RNC_AddTransportLayer(pCurrentCfgRec->ser_msg, MessageBuffer);

printf("rc_get_attribute\n");
        RNC_SendNordicConfigRequest(pCurrentCfgRec);
    }
}

/*****************************************************************************//**
* @brief This function creates and sends the start message to the Nordic
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @since 2014-11-11
* @version Initial revision.
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |     0x03      |      0x1F     |   FREQUENCY   |    BITRATE    |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*******************************************************************************/
static void rc_send_start(void)
{
    if (rc_get_cfg_memory() == true) {
        pCurrentCfgRec->expected_msg_response = MSG_TYPE_START_CONF;
        pCurrentCfgRec->serial_timeout = 10;
        MessageBuffer[0] = 0x03;
        MessageBuffer[1] = MSG_TYPE_START_REQ;
        MessageBuffer[2] = RC_DEFAULT_FREQ; //Freq
        MessageBuffer[3] = RC_DEFAULT_BIT_RATE; //Bit Rate
        RNC_AddTransportLayer(pCurrentCfgRec->ser_msg, MessageBuffer);
printf("rc_send_start\n");
        RNC_SendNordicConfigRequest(pCurrentCfgRec);
    }
}

/*****************************************************************************//**
* @brief This function allocates memory for the radio configuration message.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @since 11/21/2014
* @version Initial revision.
*******************************************************************************/
static bool rc_get_cfg_memory(void)
{
//printf("get");
    if (pCurrentCfgRec == NULL) {
        pCurrentCfgRec = (RNC_CONFIG_REC_PTR)OS_GetMsgMemBlock(sizeof(RNC_CONFIG_REC));
        pCurrentCfgRec->dest_dev_type = DESTINATION_NORDIC;
//printf("1\n");
        RC_FreeCfgRecCount = 0;
        return true;
    }
    else {
//FIX ME
//printf("0\n");
        if (++RC_FreeCfgRecCount > 2) {
            //something is wrong, free memory and retry
            OS_ReleaseMsgMemBlock(pCurrentCfgRec);
            pCurrentCfgRec = (RNC_CONFIG_REC_PTR)OS_GetMsgMemBlock(sizeof(RNC_CONFIG_REC));
            pCurrentCfgRec->dest_dev_type = DESTINATION_NORDIC;
            return true;
//printf("1\n");
       }
        return false;
    }
}

/*****************************************************************************//**
* @brief This function releases the memory block holding the configuration record
*      if it exists.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @since 11/21/2014
* @version Initial revision.
*******************************************************************************/
static void rc_release_cfg_memory(void)
{
    if (pCurrentCfgRec != NULL) {
        OS_ReleaseMsgMemBlock((void *)pCurrentCfgRec);
        pCurrentCfgRec = NULL;
    }
}

/*****************************************************************************//**
* @brief This function processes a Nordic serial confirmation message
*
* <b>Note:</b>  This function runs in the context of the calling task.
* @param p_ser_msg.  A pointer to the serial message in a PARSE_KEY_STRUCT.
* @return nothing.
* @author Neal Shurmantine
* @since 2014-11-11
* @version Initial revision.
*******************************************************************************/
void RC_HandleRadioConfirmation(PARSE_KEY_STRUCT_PTR p_ser_msg)
{
    uint8_t res_type = p_ser_msg->generic.msg[0];
    uint8_t result;
    bool expected_msg = false;
    switch (res_type) {
        case MSG_TYPE_RESET_CONF:
            if (pCurrentCfgRec->expected_msg_response == MSG_TYPE_RESET_CONF)
            {
                result = p_ser_msg->reset.status;
                expected_msg = true;
//printf("Reset Conf %x\n", result);
            }
            break;
        case MSG_TYPE_SET_ATTR_CONF:
            if (pCurrentCfgRec->expected_msg_response == MSG_TYPE_SET_ATTR_CONF)
            {
                result = p_ser_msg->set_attr.status;
                expected_msg = true;
//printf("Set Attr Conf %x\n", result);
            }
            break;
        case MSG_TYPE_GET_ATTR_CONF:
            if (pCurrentCfgRec->expected_msg_response == MSG_TYPE_GET_ATTR_CONF)
            {
                result = p_ser_msg->generic.msg[1];
                expected_msg = true;

//printf("Get Attr Conf %x attribute value:", result);
//for (int n = 0; n < sizeof(p_ser_msg->get_attr.value); ++n) {
//    printf(" %02x",p_ser_msg->get_attr.value.Group_Bitfield[n]);
//}
//printf("\n");

            }
            break;
        case MSG_TYPE_START_CONF:
            if (pCurrentCfgRec->expected_msg_response == MSG_TYPE_START_CONF)
            {
                result = p_ser_msg->start.status;
                expected_msg = true;
if ( result == 0x20) {
    printf("Nordic Startup Successful\n");
}
//printf("Start Conf %x\n\r", result);
            }
            break;
        default:
            break;
    }
    if (expected_msg == true) {
        RFO_NotifySerialResponse(result);
        (*pCurrentCfgRec->p_callback)((void *)p_ser_msg);
    }
    OS_ReleaseMsgMemBlock((void *)p_ser_msg);
}

/*****************************************************************************//**
* @brief This is a callback function that is used to process the nordic startup</br>
*        sequence.
*
* @param p_msg is a pointer to a structure of type PARSE_KEY_STRUCT_PTR.
* @return nothing.
* @author Neal Shurmantine
* @since March 23, 2014
* @version Initial revision.
*******************************************************************************/
static void rc_process_startup_confirmation(void *p_msg)
{
//printf("Callback executed\n");
    PARSE_KEY_STRUCT_PTR p_ser_msg = (PARSE_KEY_STRUCT_PTR)p_msg;
    switch (RCProgState)
    {
        case RC_STATE_RESET:
            if (p_ser_msg->reset.status == SC_RSLT_SUCCESS) {
//printf("reset successful\n");
                rc_release_cfg_memory();
                RCProgState = RC_STATE_GET_CONFIG;
                rc_get_attribute(Attribute_DLL_Unique_Id);
                pCurrentCfgRec->p_callback = rc_process_startup_confirmation;
            }
            else {
//printf("reset unsuccessful\n");
        //FIX ME
                //Retry??
                //rc_release_cfg_memory()?
            }

            break;
        case RC_STATE_GET_CONFIG:
            if ((p_ser_msg->get_attr.status == SC_RSLT_SUCCESS) 
                    && (p_ser_msg->get_attr.attr_id == Attribute_DLL_Unique_Id))
            {
//printf("get config successful\n");
                rc_release_cfg_memory();
                RC_NordicUuid = p_ser_msg->get_attr.value.Unique_Id;
                InitIndex = 0;
                RCProgState = RC_STATE_SET_CONFIG;
                rc_set_attribute((P3_Attribute_Config_Type_Ptr)&AttributeSetup[InitIndex]);
                pCurrentCfgRec->p_callback = rc_process_startup_confirmation;
            }
            else {
//printf("get config unsuccessful\n");
        //FIX ME
                //Retry?? //release memory?
                //rc_release_cfg_memory()?
            }
            break;
        case RC_STATE_SET_CONFIG:
            if (p_ser_msg->set_attr.status == SC_RSLT_SUCCESS) {
//printf("set config successful\n");
                rc_release_cfg_memory();
                ++InitIndex;
                if (AttributeSetup[InitIndex].size != 0) {
                    rc_set_attribute((P3_Attribute_Config_Type_Ptr)&AttributeSetup[InitIndex]);
                            pCurrentCfgRec->p_callback = rc_process_startup_confirmation;
                }
                else {
                    RCProgState = RC_STATE_START;
                    rc_send_start();
                    pCurrentCfgRec->p_callback = rc_process_startup_confirmation;
                }
            }
else {
//printf("set config unsuccessful\n");
}
            break;
        case RC_STATE_START:
            if (p_ser_msg->start.status == SC_RSLT_SUCCESS) {
                rc_release_cfg_memory();
                RC_RadioReady = true;
                }
            else{
                //FIX ME
            }
            LED_NetworkID(RC_IsNetworkIdAssigned());
            break;
        default:
            break;
    }
}

/*****************************************************************************//**
* @brief This function writes the Nordic configuration parameters to serial
*     flash.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @since 11/21/2014
* @version Initial revision.
*******************************************************************************/
static void RC_nv_file_write(void)
{
    uint8_t * spi_buffer;

    spi_buffer=(uint8_t *)OS_GetMemBlock(sizeof(RC_NV_STRUCT_TYPE));

    RC_NV_STRUCT_TYPE_PTR temp;
    temp = (RC_NV_STRUCT_TYPE_PTR)spi_buffer;
    temp->I_Am_Programmed = RC_NonVolatile.I_Am_Programmed;
    temp->PhyTxPower = RC_NonVolatile.PhyTxPower;
    temp->DllLowPower = RC_NonVolatile.DllLowPower;
    temp->DllNetworkId = RC_NonVolatile.DllNetworkId;
    temp->DllDeviceId = RC_NonVolatile.DllDeviceId;
    temp->DllMaxBackoffCount = RC_NonVolatile.DllMaxBackoffCount;
    temp->DllMaxBackoffExp = RC_NonVolatile.DllMaxBackoffExp;
    temp->DllMinBackoffExp = RC_NonVolatile.DllMinBackoffExp;

    FILE * f_handle;
    
    f_handle = fopen(RADIO_CONFIG_FILENAME, "w");
    fwrite(spi_buffer,sizeof(RC_NV_STRUCT_TYPE),1,f_handle);
    fclose(f_handle);

    OS_ReleaseMemBlock(spi_buffer);
}

/*****************************************************************************//**
* @brief This function reads serial flash to retrieve the Nordic configuration
*    parameters.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @since 11/21/2014
* @version Initial revision.
*******************************************************************************/
static bool RC_nv_file_read(void)
{
    bool rtn = false;
    uint8_t * spi_buffer;
    RC_NV_STRUCT_TYPE_PTR temp;
    FILE * f_handle;

    spi_buffer=(uint8_t *)OS_GetMemBlock(sizeof(RC_NV_STRUCT_TYPE));
    f_handle = fopen(RADIO_CONFIG_FILENAME, "r");
    if (f_handle != NULL) {
        if (fread(spi_buffer,sizeof(RC_NV_STRUCT_TYPE),1,f_handle) == 1) {
            temp = (RC_NV_STRUCT_TYPE_PTR)spi_buffer;
            RC_NonVolatile.I_Am_Programmed = temp->I_Am_Programmed;
            RC_NonVolatile.PhyTxPower = temp->PhyTxPower;
            RC_NonVolatile.DllLowPower = temp->DllLowPower;
            RC_NonVolatile.DllNetworkId = temp->DllNetworkId;
            RC_NonVolatile.DllDeviceId = temp->DllDeviceId;
            RC_NonVolatile.DllMaxBackoffCount = temp->DllMaxBackoffCount;
            RC_NonVolatile.DllMaxBackoffExp = temp->DllMaxBackoffExp;
            RC_NonVolatile.DllMinBackoffExp = temp->DllMinBackoffExp;
            rtn = true;
        }
        fclose(f_handle);
    }
    
    OS_ReleaseMemBlock(spi_buffer);
    return rtn;
}

/*****************************************************************************//**
* @brief This function loads the Nordic Nonvolatile structure with default
*    values.
*
* @param none.
* @return nothing.
* @author Neal Shurmantine
* @since 11/21/2014
* @version Initial revision.
*******************************************************************************/
static void RC_set_nordic_defaults(void)
{
    RC_NonVolatile.I_Am_Programmed = I_AM_PROGRAMMED_DEFAULT;
    RC_NonVolatile.PhyTxPower = DEFAULT_PHY_TX_Power;
    RC_NonVolatile.DllLowPower = DEFAULT_DLL_Low_Power;
    RC_NonVolatile.DllNetworkId = DEFAULT_DLL_Network_Id;
    RC_NonVolatile.DllDeviceId = DEFAULT_DLL_Device_Id;
    RC_NonVolatile.DllMaxBackoffCount = DEFAULT_DLL_Max_Backoff_Count;
    RC_NonVolatile.DllMaxBackoffExp = DEFAULT_DLL_Max_Backoff_Exp;
    RC_NonVolatile.DllMinBackoffExp = DEFAULT_DLL_Min_Backoff_Exp;
}
