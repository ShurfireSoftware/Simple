/***************************************************************************//**
 * @file rf_serial_api.h
 * @brief Include file containing structures and definitions for use when
 *         communicating with the Nordic.
 *
 * @author Neal Shurmantine
 * @copyright (c) 2014 Hunter Douglas. All rights reserved.
 *
 * @version
 * 11/20/2014   Created.
 * 07/20/2015        Added version parameter to SceneControllerUpdatePacketRequest()
 * 08/12/2016   Modified for use with Hub2.0
 ******************************************************************************/

#ifndef _RF_SERIAL_API_H_
#define _RF_SERIAL_API_H_

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

/*
Serial Buffer Requirements:
    Add 3 for overhead,
    Add extra for excaping


MSG_TYPE_GET_ATTR_REQ           2
MSG_TYPE_SET_ATTR_REQ           34
MSG_TYPE_RESET_REQ              2
MSG_TYPE_START_REQ              3
MSG_TYPE_SEND_SHADE_DATA_REQ    13+
MSG_TYPE_SEND_BEACON_REQ        2
MSG_TYPE_SEND_GROUP_SET_REQ     13

MSG_TYPE_GET_ATTR_CONF          35
MSG_TYPE_SET_ATTR_CONF          3
MSG_TYPE_RESET_CONF             2
MSG_TYPE_START_CONF             2
MSG_TYPE_SEND_SHADE_DATA_CONF   3
MSG_TYPE_SEND_BEACON_CONF       2
MSG_TYPE_SEND_GROUP_SET_CONF    2

MSG_TYPE_SHADE_DATA_INDICATION  21+
MSG_TYPE_BEACON_INDICATION      24+
MSG_TYPE_GROUP_SET_INDICATION   12

Network header size     14
Largest Shade Data Request          34 + 14
Largest Shade Indication Payload    34
Largest System Indication Payload   44

Largest Send Serial Buffer is 13 + 48 + 3 + var = 64+
Largest Receive Serial Buffer is 21 + 34 + 3 + var = 58+
*/

typedef enum COARSE_BATTERY_LEVEL_TAG
{
    NO_VALUE,
    RED,
    YELLOW,
    GREEN,
    PLUGGED
} COARSE_BATTERY_LEVEL;

//#include "shadeprocessing.h"
#define ItsExpandedShadeAPI_
#define ItsMaxAddShadeCount_ 1024
#define ItsLowBatteryThreshold_ 12
#define ItsMaxPositionCount_ 5
#define ItsMaxShadeCount_ 256
#define ItsMaxDefinedShadeType_ 246
#define ItsMaxNameLength_ 64
typedef enum {pkNone = 0, pkPrimaryRail, pkSecondaryRail, pkVaneTilt, pkError} ePosKind;
typedef struct {
    uint8_t posCount;
    uint16_t position[ItsMaxPositionCount_];
    ePosKind posKind[ItsMaxPositionCount_];
} strPositions;
typedef struct {
    char name[ItsMaxNameLength_];
    uint8_t sceneMemberCount, type, batteryStrength, batteryStatus;
    uint16_t uID, roomID, groupID;
    bool roomAssigned, nameAssigned, groupAssigned;
} strShade;

typedef struct {
    uint16_t uID;
    bool justDiscovered;
    strPositions positions;
} strShadeVariables;



#define START_OF_HEADER     0x7e
#define ESCAPE_TOKEN        0x7d
#define ALL_NETWORK_ID 0xFFFF
#define FACTORY_DEFAULT_NETWORK_ID  0x1111
#define ALL_DEVICES_ADDRESS 0xFFFF


#define MSG_TYPE_GET_ATTR_REQ           0x04
#define MSG_TYPE_SET_ATTR_REQ           0x06
#define MSG_TYPE_RESET_REQ              0x1D
#define MSG_TYPE_START_REQ              0x1F
#define MSG_TYPE_SEND_SHADE_DATA_REQ    0x0C
#define MSG_TYPE_SEND_BEACON_REQ        0x0F
#define MSG_TYPE_SEND_GROUP_SET_REQ     0x12

#define MSG_TYPE_GET_ATTR_CONF          0x05
#define MSG_TYPE_SET_ATTR_CONF          0x07
#define MSG_TYPE_RESET_CONF             0x1E
#define MSG_TYPE_START_CONF             0x20
#define MSG_TYPE_SEND_SHADE_DATA_CONF   0x0D
#define MSG_TYPE_SEND_BEACON_CONF       0x10
#define MSG_TYPE_SEND_GROUP_SET_CONF    0x14
#define MSG_TYPE_SYSTEM                 0xff
#define MSG_TYPE_SYSTEM_VERSION         0x02

#define MSG_TYPE_SHADE_DATA_INDICATION  0x0E
#define MSG_TYPE_BEACON_INDICATION      0x11
#define MSG_TYPE_GROUP_SET_INDICATION   0x15
#define MSG_TYPE_SYSTEM_INDICATION        0xff


typedef unsigned char        P3_UInt8_Type;
typedef signed char            P3_Int8_Type;
typedef P3_UInt8_Type        P3_Byte_Type;

typedef unsigned short        P3_UInt16_Type;
typedef signed short        P3_Int16_Type;

typedef unsigned long        P3_UInt32_Type;
typedef signed long            P3_Int32_Type;

typedef unsigned long long    P3_UInt64_Type;
typedef signed long long    P3_Int64_Type;

typedef unsigned char        P3_Bool_Type;

typedef enum P3_Attribute_Tag {
    Attribute_PHY_BASE0                    = 0x00, /*!< address BASE0 */
    Attribute_PHY_BASE1                    = 0x01, /*!< address BASE1 */
    Attribute_PHY_Prefix_0                = 0x02, /*!< address Prefix 0 */
    Attribute_PHY_Prefix_1                = 0x03, /*!< address Prefix 1 */
    Attribute_PHY_Prefix_2                = 0x04, /*!< address Prefix 2 */
    Attribute_PHY_Prefix_3                = 0x05, /*!< address Prefix 3 */
    Attribute_PHY_Prefix_4                = 0x06, /*!< address Prefix 4 */
    Attribute_PHY_Prefix_5                = 0x07, /*!< address Prefix 5 */
    Attribute_PHY_Prefix_6                = 0x08, /*!< address Prefix 6 */
    Attribute_PHY_Prefix_7                = 0x09, /*!< address Prefix 7 */
    Attribute_PHY_Current_Bitrate        = 0x0A, /*!< transceiver bitrate */
    Attribute_PHY_Current_Frequency        = 0x0B, /*!< transceiver frequency */
    Attribute_PHY_TX_Power                = 0x0C, /*!< transmitter power level */
    Attribute_PHY_8BT_Time_uS            = 0x0D, /*!< time to transmit 8-bits */

    Attribute_DLL_Promiscuous_Mode        = 0x20, /*!< RX packet filter off */
    Attribute_DLL_Low_Power                = 0x21, /*!< use power saving features */
    Attribute_DLL_Network_Id            = 0x22, /*!< network address */
    Attribute_DLL_Device_Id                = 0x23, /*!< device address */
    Attribute_DLL_SEQ_Number            = 0x24, /*!< current TX sequence number */
    Attribute_DLL_Max_Backoff_Count        = 0x25, /*!< channel busy failure count */
    Attribute_DLL_Max_Backoff_Exp        = 0x26, /*!< maximum CSMA exponent */
    Attribute_DLL_Min_Backoff_Exp        = 0x27, /*!< minimum CSMA exponent */
    Attribute_DLL_Ack_TO_Time_8BT        = 0x28, /*!< time to wait for ACK */
    Attribute_DLL_RX_On_Time_uS            = 0x29, /*!< time receiver is left on when idle */
    Attribute_DLL_TX_Persist_Time_uS    = 0x2A, /*!< stale transmit timeout time */
    Attribute_DLL_Group_Bitfield        = 0x2B, /*!< group membership bitfield */
    Attribute_DLL_TX_Burst_Time_mS        = 0x2C,  /*!< time repeating a transmission */
    Attribute_DLL_Unique_Id                = 0x2D,  /*!< 64-bit unique address */
    Attribute_Net_SEQ_Number            = 0x40  /*!< network-header TX SEQ number */
} P3_Attribute_Type;

typedef union P3_Attribute_Value_Tag {
    /* PHY Attributes */
    P3_UInt32_Type    Address_Base;
    P3_UInt8_Type    Address_Prefix;
    P3_UInt8_Type    Bitrate;
    P3_UInt8_Type    Frequency;
    P3_Int8_Type    Tx_Power;
    P3_UInt8_Type    Time_8BT_uS;
    /* DLL Attributes */
    P3_UInt8_Type    Promiscuous_Mode;
    P3_UInt8_Type    Low_Power;
    P3_UInt16_Type    Network_Id;
    P3_UInt16_Type    Device_Id;
    P3_UInt8_Type    DLL_SEQ_Number;
    P3_UInt8_Type    Max_Backoff_Count;
    P3_UInt8_Type    Max_Backoff_Exp;
    P3_UInt8_Type    Min_Backoff_Exp;
    P3_UInt32_Type    Time_uS;
    P3_Byte_Type    Group_Bitfield[32u];
    P3_UInt64_Type    Unique_Id;
} __attribute__((packed)) P3_Attribute_Value_Type;

typedef struct P3_Attribute_Config_Tag {
    P3_Attribute_Type type;
    uint16_t size;
    void * value;
} P3_Attribute_Config_Type, *P3_Attribute_Config_Type_Ptr;

#define MAX_POSITION_KINDS  3

typedef struct RC_NV_STRUCT_TAG
{
    P3_UInt8_Type I_Am_Programmed;
    P3_Int8_Type PhyTxPower;
    P3_UInt8_Type DllLowPower;
    P3_UInt16_Type DllNetworkId;
    P3_UInt16_Type DllDeviceId;
    P3_UInt8_Type DllMaxBackoffCount;
    P3_UInt8_Type DllMaxBackoffExp;
    P3_UInt8_Type DllMinBackoffExp;
} RC_NV_STRUCT_TYPE, *RC_NV_STRUCT_TYPE_PTR;

/**
* @brief Union of a short and array of two bytes that is useful for<br/>
*  decomposing the short, expecially when endianess is a factor.
*/
typedef union
{
    uint16_t short_id;
    uint8_t bytes[2];
    uint8_t group_list[8];
} ID_TYPE_T;

typedef enum
{
    DIRECTION_UP,
    DIRECTION_DOWN,
    DIRECTION_STOP
} MOVE_DIRECTION_T;

// marker 06/30/2015 - this might change
#define SC_SCENE_CONTROLLER_TEXT_SIZE   25

#define MAX_SER_CONFIG_SIZE     88
#define MAX_RADIO_CONFIG_SER_MESSAGE  50
#define MAX_CMD_PAYLOAD         34

#define MAX_BEACON_PAYLOAD_SIZE     0x32
#define MAX_INDICATION_PAYLOAD_SIZE     0x32

#define MAX_SCENE_EXECUTE_SIZE    28

#define NETWORK_HEADER_SIZE         14

//RF Timer Expiration Values
//tick timer for timeout following the sending of a message to shade
#define SC_WAIT_RF_TICK                     200
//number of ticks to wait after sending a message to shade
//  before sending next
#define SC_SHADE_RF_TICK_MAX                5
#define SC_SHADE_BATT_REQ_RF_TICK_MAX       9
#define RNC_TICK_INTERVAL                   200
#define RNC_PROCESS_TICK_INTERVAL           200

//Serial Message Retries
#define RFO_MAX_RF_MSG_TRIES                1
#define RFO_MAX_NORDIC_MSG_TRIES            1
//Max RF Message Attempts with no response
//#define SC_MAX_MSG_TRIES                    5
#define SC_MAX_MSG_TRIES                    1

//Serial Timer Expiration Values
#define RFO_WAIT_NEXT_NORDIC_OUTBOUND       2
#define RFO_WAIT_RETRY_NORDIC_OUTBOUND      2
#define RFO_WAIT_NORDIC_SER_RESPONSE        1000

//pause between outbound RF messages to give shades a chance to respond
#define RFO_WAIT_NEXT_RF_OUTBOUND           3000
//if NAK was received or timeout occurred when sending an rf outbound
// message then wait 200 msec before retrying
#define RFO_WAIT_RETRY_RF_OUTBOUND          200
//wait up to 2 seconds for a response when sending an rf outbound
#define RFO_WAIT_RF_SER_RESPONSE            2000
//set the interval of the timer to zero to stop it
#define RFO_WAIT_MAX                        WAIT_TIME_INFINITE

#define MAX_GROUP_LIST_SIZE 8
typedef enum
{
    //address field omitted
    P3_Address_Mode_None,

    //2 byte network-unique address
    P3_Address_Mode_Device_Id,

    //Address field contains a list of 1 to 8 Group Indices
    P3_Address_Mode_Group_Id,

    //8 byte globally-unique address
    P3_Address_Mode_Unique_Id
} P3_Address_Mode_Type;

typedef union P3_Address_Internal_Tag
{
    //2 byte network-unique address
    uint16_t    Device_Id;

    //Address field contains a list of 1 to 8 Group Indices
    uint8_t        Group_Id[MAX_GROUP_LIST_SIZE];

    //8 byte globally-unique address
    uint64_t    Unique_Id;
} __attribute__((packed)) P3_Address_Internal_Type;

typedef enum
{
    SC_ABSOLUTE_DISCOVER = 0,
    SC_CONDITIONAL_DISCOVER,		//  1
    SC_GET_SHADE_POSITION,			//  2
    SC_SET_DISCOVERED_FLAG,			//  3
    SC_SET_SHADE_POSITION,			//  4
    SC_MOVE_SHADE,					//  5
    SC_REQUEST_SHADE_STATUS,		//  6
    SC_REQUEST_BATTERY_LEVEL,		//  7
    SC_REQUEST_SHADE_TYPE,			//  8
    SC_REQUEST_RECEIVER_FW,			//  9
    SC_REQUEST_MOTOR_FW,			// 10
    SC_SET_SCENE,					// 11
    SC_SET_SCENE_POSITION,			// 12
    SC_EXECUTE_SCENE,				// 13
    SC_DELETE_SCENE,				// 14
    SC_REQUEST_SCENE_POSITION,		// 15
    SC_JOG_SHADE,					// 16
    SC_GROUP_ASSIGN,				// 17
    SC_REQUEST_GROUP,				// 18
    SC_RESET_SHADE,					// 19
    SC_ISSUE_BEACON,				// 20
    SC_RAW_PAYLOAD,					// 21
    SC_REQUEST_DEBUG_STATUS,		// 22
    SC_SCENE_CTL_CLEARED_ACK,		// 23
    SC_SCENE_CTL_UPDATE_HEADER,		// 24
    SC_SCENE_CTL_UPDATE_PACKET,		// 25
    SC_SCENE_CTL_TRIGGER_ACK		// 26
} SHADE_COMMAND_TYPE;

//Shade reset bit fields
#define SR_RESET_NETWORK_ID             BIT0
#define SR_RANDOMIZE_DEVICE_ID          BIT1
#define SR_DEL_APPROVED_CONTROLLERS     BIT2
#define SR_DEL_GROUP_1_TO_6             BIT3
#define SR_DEL_GROUP_7_TO_255           BIT4
#define SR_CLEAR_DISCOVERED_FLAG        BIT5
#define SR_DELETE_SCENES                BIT8
#define SR_RECAL_NEXT_RUN               BIT9
#define SR_CLEAR_BOTTOM_LIMIT           BIT10
#define SR_CLEAR_TOP_LIMIT              BIT11
#define SR_REVERT_SHADE_TYPE_TO_DEFAULT BIT12

typedef struct SCENE_CTL_UPDATE_HEADER_TAG {
    uint8_t rec_count;
    uint8_t version;
    char name[SC_SCENE_CONTROLLER_TEXT_SIZE]; //25
}__attribute__((packed)) SC_SCENE_CTL_UPDATE_HDR_STR, *SC_SCENE_CTL_UPDATE_HDR_STR_PTR;

typedef struct SCENE_CTL_UPDATE_PACKET_TAG {
    uint8_t rec_count;
    uint8_t version;
    uint8_t scene_type;
    uint16_t scene_id;
    char name[SC_SCENE_CONTROLLER_TEXT_SIZE];
}__attribute__((packed)) SC_SCENE_CTL_UPDATE_PACKET_STR, *SC_SCENE_CTL_UPDATE_PACKET_STR_PTR;

typedef struct SCENE_CTL_TRIGGER_ACK_TAG{
    uint8_t scene_type;
    uint16_t scene_id;
    uint8_t version;
}__attribute__((packed)) SC_SCENE_CTL_TRIGGER_ACK_STR, *SC_SCENE_CTL_TRIGGER_ACK_STR_PTR;

#define MAX_RF_SHADE_CMD_PAYLOAD_SIZE    sizeof(SC_SCENE_CTL_UPDATE_PACKET_STR))
#define MAX_SYSTEM_INDICATION_RF_PAYLOAD_SIZE (NETWORK_HEADER_SIZE + MAX_RF_SHADE_CMD_PAYLOAD_SIZE

typedef struct SYSTEM_INDICATION_STRUCT_TYPE
{
    uint8_t payload_len;
    uint8_t indication_type;
    uint8_t id;
	uint8_t payload[MAX_SYSTEM_INDICATION_RF_PAYLOAD_SIZE];
} __attribute__((packed)) SYSTEM_INDICATION_STRUCT, *SYSTEM_INDICATION_STRUCT_PTR;

// marker 11/18/2015 - send this struct to slave
typedef struct SHADE_COMMAND_INSTRUCTION_TYPE
{
    void(*p_callback)(void*);
    SHADE_COMMAND_TYPE cmd_type;
    uint8_t adr_mode;
    P3_Address_Internal_Type address;
    union {
        struct {
            uint8_t posCount;            // 1 byte
            uint16_t position[2];        // 4 bytes
            ePosKind posKind[2];        // 2 bytes
        }__attribute__((packed))  pos_data;
        struct {
            MOVE_DIRECTION_T dir;
        }__attribute__((packed))  move_data;
        struct {
            uint8_t count;
            uint8_t id_list[MAX_SCENE_EXECUTE_SIZE]; //28 bytes
            uint8_t posCount;            // 1 byte
            uint16_t position[2];        // 4 bytes
            ePosKind posKind[2];        // 2 bytes
        }__attribute__((packed))  scene_data;
        struct {
            uint8_t id;
            bool is_assigned;
        }__attribute__((packed))  group_data;
        SC_SCENE_CTL_UPDATE_HDR_STR scene_ctl_update_header;
        SC_SCENE_CTL_UPDATE_PACKET_STR scene_ctl_update_packet;
        SC_SCENE_CTL_TRIGGER_ACK_STR scene_ctl_trigger_ack;
        struct {
            uint8_t len;
            uint8_t msg[MAX_SCENE_EXECUTE_SIZE];
        }__attribute__((packed))  raw_data;
        SYSTEM_INDICATION_STRUCT system_indication;

        uint16_t reset_data;
    }__attribute__((packed)) data;
}__attribute__((packed)) SHADE_COMMAND_INSTRUCTION, *SHADE_COMMAND_INSTRUCTION_PTR;


typedef enum
{
    SC_RSLT_SUCCESS = 0x20,
    SC_RSLT_CHAN_ACCESS_FAIL = 0x21,
    SC_RSLT_INVAL_PARAM = 0x24,
    SC_RSLT_READ_ONLY_ATTR = 0x25,
    SC_RSLT_NOT_SUPPORTED = 0x26,
    SC_RSLT_TIMEOUT = 0x28,
    SC_RSLT_BUSY = 0x29
} SC_SER_RESP_CODE_T;

typedef enum
{
    WAITING_TO_SEND_STATE,
    WAITING_FOR_SER_ACK_STATE,
    WAITING_TO_SEND_NEXT
}SC_STATE_T;

typedef enum
{
    SINGLE,
    GROUP,
    ALL
} ADR_TYPE;

/**
 * @brief Transmit the data as a packet using CSMA.
 */
#define    TX_OPTION_NONE                    (0x00u)

/**
 * @brief Transmit a copy of the data repeatedly for @ref
 *        Attribute_DLL_TX_Burst_Time_mS "DLL_TX_Burst_Time_mS".
 */
#define    TX_OPTION_BURST                    (0x01u)

//not setting this bit forces nordic to send a system indication with network header
#define TX_OPTION_NO_NET_HEADER             (0x08u)
#define TX_OPTION   (TX_OPTION_BURST)

typedef uint8_t Tx_Options_Type;


//A group list is stored internally as an array of 8 bytes and is
//converted to an on-air group list as follows:

//  - If the first byte of a group list is zero then the list is
//    assumed have a length of 1 entry indexing the "all" group.
//  - If the first byte is nonzero then the array is treated as
//    either a list of 1-7 groups terminated by a zero byte, or
//    a list of 8 groups if all bytes in the array are nonzero.


//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |      0x0D+    |     0x0C      |  SOURCE MODE  |  DEST MODE    |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |                                                               |
//    |                    DEST ADDRESS (8 bytes)                     |
//    |                                                               |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |   TX_OPTIONS  |  TX_HANDLE    |                               |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               /
//    /                      PAYLOAD (length varies)                  /
//    |                                                               |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
typedef struct SHADE_DATA_REQUEST_HEADER_TYPE
{
    uint8_t length;
    uint8_t request_type;
    uint8_t source_mode;
    uint8_t dest_mode;
    P3_Address_Internal_Type dest_adr;
    uint8_t tx_options;
    uint8_t tx_handle;
}__attribute__((packed)) SHADE_DATA_REQUEST_HEADER_STRUCT, *SHADE_DATA_REQUEST_HEADER_STRUCT_PTR;

#define SHADE_DATA_REQUEST_HDR_SIZE     (sizeof(SHADE_DATA_REQUEST_HEADER_STRUCT) - 1)
typedef struct SHADE_DATA_REQ_MSG_TYPE
{
    void(*p_callback)(void*);
    uint8_t payload_len;
    SHADE_DATA_REQUEST_HEADER_STRUCT hdr;
    uint8_t msg_payload[MAX_SYSTEM_INDICATION_RF_PAYLOAD_SIZE];
} __attribute__((packed)) SHADE_DATA_REQ_MSG_STRUCT, *SHADE_DATA_REQ_MSG_STRUCT_PTR;


//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |      0x15+    |     0x0E      |  SOURCE MODE  |               |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+               |
//    |                                                               |
//    |                     SOURCE ADDR (8 bytes)     +-+-+-+-+-+-+-+-+
//    |                                               |  DEST MODE    |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |                                                               |
//    |                    DEST ADDRESS (8 bytes)                     |
//    |                                                               |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |    SEQ NUM    |     RSSI      |                               |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               /
//    /                      PAYLOAD (length varies)                  /
//    |                                                               |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#define SHADE_INDICATION_HEADER_SIZE    21
typedef struct SHADE_DATA_INDICATION_TYPE
{
    uint8_t payload_len;
    uint8_t indication_type;
    uint8_t source_mode;
    P3_Address_Internal_Type source_adr;
    uint8_t dest_mode;
    P3_Address_Internal_Type dest_adr;
    uint8_t seq_num;
    uint8_t rssi;
    uint8_t msg_payload[];
} __attribute__((packed)) SHADE_DATA_INDICATION_STRUCT, *SHADE_DATA_INDICATION_STRUCT_PTR;

//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |     0x02      |     0x0F      |  TX OPTIONS   |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
typedef struct BEACON_REQUEST_HEADER_TYPE
{
    uint8_t length;
    uint8_t request_type;
    uint8_t tx_options;
}__attribute__((packed)) BEACON_REQUEST_HEADER_STRUCT, *BEACON_REQUEST_HEADER_STRUCT_PTR;

#define BEACON_REQUEST_HDR_SIZE     (sizeof(BEACON_REQUEST_HEADER_STRUCT) - 1)

//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |      0x18+    |     0x11      | DEST MODE     |               /
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+               /
//    /                                                               /
//    /                       DEST ADDR (8 bytes)     +-+-+-+-+-+-+-+-+
//    /                                               |               |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+               /
//    |                                                               /
//    |                    SOURCE UNIQUE ID (8 bytes) +-+-+-+-+-+-+-+-+
//    /                                               |       SOURCE  /
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    / NETWORK ID    |      SOURCE DEVICE ID         |    SEQ NUM    |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |     RSSI      |      PAYLOAD (length varies)                  /
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#define BEACON_INDICATION_HEADER_SIZE    24
typedef struct BEACON_INDICATION_TYPE
{
    uint8_t payload_len;
    uint8_t indication_type;
    uint8_t dest_mode;
    P3_Address_Internal_Type dest_adr;
    P3_Address_Internal_Type source_adr;
    uint16_t source_network_id;
    uint16_t source_device_id;
    uint8_t seq_num;
    uint8_t rssi;
    uint8_t msg_payload[];
} __attribute__((packed)) BEACON_INDICATION_STRUCT, *BEACON_INDICATION_STRUCT_PTR;

//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |      0x0D     |      0x12     |  DEST MODE    |               /
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+               /
//    /                                                               /
//    /                  DEST ADDRESS (8 bytes)       +-+-+-+-+-+-+-+-+
//    /                                               |   GROUP ID    |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |STAT(in or out)|  TX OPTIONS   |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
typedef struct GROUP_SET_REQUEST_HEADER_TYPE
{
    uint8_t length;
    uint8_t request_type;
    uint8_t dest_mode;
    P3_Address_Internal_Type dest_adr;
    uint8_t group_id;
    uint8_t is_assigned;
    uint8_t tx_options;
}__attribute__((packed)) GROUP_SET_REQUEST_HEADER_STRUCT, *GROUP_SET_REQUEST_HEADER_STRUCT_PTR;

#define GROUP_SET_REQUEST_HDR_SIZE     (sizeof(GROUP_SET_REQUEST_HEADER_STRUCT) - 1)



//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |      0x0C     |      0x15     |  SOURCE MODE  |               |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+               |
//    |                                                               /
//    /                       SOURCE ADDR (8 bytes)                   /
//    |                                               +-+-+-+-+-+-+-+-+
//    |                                               |   GROUP ID    |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |STAT(in or out)|
//    +-+-+-+-+-+-+-+-+
typedef struct GROUP_INDICATION_TYPE
{
    uint8_t payload_len;
    uint8_t indication_type;
    uint8_t source_mode;
    P3_Address_Internal_Type source_adr;
    uint8_t group_id;
    uint8_t status;
} __attribute__((packed)) GROUP_INDICATION_STRUCT, *GROUP_INDICATION_STRUCT_PTR;


//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |      0x02     |      0x1E     |    STATUS     |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
typedef struct RESET_CONFIRMATION_TYPE
{
    uint8_t payload_len;
    uint8_t indication_type;
    uint8_t status;
} __attribute__((packed)) RESET_CONFIRMATION_STRUCT, *RESET_CONFIRMATION_STRUCT_PTR;

//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |      0x03     |  TYPE: 0x07   |     STATUS    | P3_Attribute_Type |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
typedef struct SET_ATTR_CONFIRMATION_TYPE
{
    uint8_t payload_len;
    uint8_t indication_type;
    uint8_t status;
    uint8_t attr_id;
} __attribute__((packed)) SET_ATTR_CONFIRMATION_STRUCT, *SET_ATTR_CONFIRMATION_STRUCT_PTR;

//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |      SIZE     |  TYPE: 0x05   |     STATUS    | ID: see below |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |                         VALUE0..VALUEn                        |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
typedef struct GET_ATTR_CONFIRMATION_TYPE
{
    uint8_t payload_len;
    uint8_t indication_type;
    uint8_t status;
    uint8_t attr_id;
    P3_Attribute_Value_Type value;
} __attribute__((packed)) GET_ATTR_CONFIRMATION_STRUCT, *GET_ATTR_CONFIRMATION_STRUCT_PTR;

//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |      0x02     |      0x20     |    STATUS     |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
typedef struct START_CONFIRMATION_TYPE
{
    uint8_t payload_len;
    uint8_t indication_type;
    uint8_t status;
} __attribute__((packed)) START_CONFIRMATION_STRUCT, *START_CONFIRMATION_STRUCT_PTR;

//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |      0x03     |     0x0D      |     STATUS    |  TX_HANDLE    |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
typedef struct SHADE_DATA_CONFIRMATION_TYPE
{
    uint8_t payload_len;
    uint8_t indication_type;
    uint8_t status;
    uint8_t handle;
} __attribute__((packed)) SHADE_DATA_CONFIRMATION_STRUCT, *SHADE_DATA_CONFIRMATION_STRUCT_PTR;

//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |      0x02     |      0x10     |    STATUS     |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
typedef struct BEACON_CONFIRMATION_TYPE
{
    uint8_t payload_len;
    uint8_t indication_type;
    uint8_t status;
} __attribute__((packed)) BEACON_CONFIRMATION_STRUCT, *BEACON_CONFIRMATION_STRUCT_PTR;

//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |      0x02     |      0x13     |    STATUS     |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
typedef struct GROUP_CONFIRMATION_TYPE
{
    uint8_t payload_len;
    uint8_t indication_type;
    uint8_t status;
} __attribute__((packed)) GROUP_CONFIRMATION_STRUCT, *GROUP_CONFIRMATION_STRUCT_PTR;

typedef struct GENERIC_CONFIRMATION_TYPE
{
    uint8_t payload_len;
    uint8_t confirmation_type;
    uint8_t status;
} __attribute__((packed)) GENERIC_CONFIRMATION_STRUCT, *GENERIC_CONFIRMATION_STRUCT_PTR;

//typedef struct METRICS_TAG
//{
//} __attribute__((packed)) METRICS_STRUCT, *METRICS_STRUCT_PTR;

typedef struct GENERIC_INDICATION_TYPE
{
    uint8_t payload_len;
    uint8_t indication_type;
    uint8_t status;
} __attribute__((packed)) GENERIC_INDICATION_STRUCT, *GENERIC_INDICATION_STRUCT_PTR;

typedef struct GENERIC_KEY_TYPE
{
    uint8_t length;
    uint8_t msg[];
} __attribute__((packed)) GENERIC_KEY_STRUCT, *GENERIC_KEY_STRUCT_PTR;

//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |      0x02     |      0xFF     |    STATUS     |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
typedef struct RESET_INDICATION_STRUCT_TYPE
{
    uint8_t length;
    uint8_t indication_type;
    uint8_t status;
} __attribute__((packed)) RESET_INDICATION_STRUCT, *RESET_INDICATION_STRUCT_PTR;

typedef union
{
    GENERIC_INDICATION_STRUCT generic_ind;
    GENERIC_CONFIRMATION_STRUCT generic_conf;
    GENERIC_KEY_STRUCT generic;
    RESET_CONFIRMATION_STRUCT reset;
    SET_ATTR_CONFIRMATION_STRUCT set_attr;
    GET_ATTR_CONFIRMATION_STRUCT get_attr;
    START_CONFIRMATION_STRUCT start;
    SHADE_DATA_CONFIRMATION_STRUCT shade_conf;
    SHADE_DATA_INDICATION_STRUCT shade_data_ind;
    BEACON_CONFIRMATION_STRUCT beacon_conf;
    BEACON_INDICATION_STRUCT beacon_ind;
    GROUP_CONFIRMATION_STRUCT group_conf;
    GROUP_INDICATION_STRUCT group_ind;
    RESET_INDICATION_STRUCT reset_ind;
    SYSTEM_INDICATION_STRUCT system_ind;
} __attribute__((packed)) PARSE_KEY_STRUCT, *PARSE_KEY_STRUCT_PTR;

typedef enum
{
    DESTINATION_NORDIC,
    DESTINATION_SHADE,
    DESTINATION_NONE
} DESTINATION_DEVICE_TYPE;

typedef struct RNC_CFG_REC_TAG
{
    DESTINATION_DEVICE_TYPE dest_dev_type;
    uint8_t dest_mode;
    P3_Address_Internal_Type id;
    SC_STATE_T state;
    uint8_t rf_retry_count;
    uint8_t rf_retry_max;
    uint8_t rf_ack_tick_count;
    uint32_t serial_timeout;
    uint8_t serial_retry_max;
    uint8_t expected_msg_response;
    void(*p_callback)(void*);
    struct RNC_CFG_REC_TAG* p_prev_rec;
    struct RNC_CFG_REC_TAG* p_next_rec;
    uint8_t ser_msg[MAX_SER_CONFIG_SIZE];
}RNC_CONFIG_REC, *RNC_CONFIG_REC_PTR;

typedef struct DISCOVERY_DATA_TYPE
{
    uint64_t uuid;
    uint16_t network_id;
    uint16_t device_id;
    uint8_t shade_type;
} __attribute__((packed)) DISCOVERY_DATA_STRUCT, *DISCOVERY_DATA_STRUCT_PTR;

typedef struct SC_SHADE_DISCOVER_TAG
{
    DISCOVERY_DATA_STRUCT shade_rsp;
    struct SC_SHADE_DISCOVER_TAG* p_prev_rec;
    struct SC_SHADE_DISCOVER_TAG* p_next_rec;
}SC_SHADE_DISC_REC, *SC_SHADE_DISC_REC_PTR;

typedef struct
{
    ID_TYPE_T device_id;
    uint8_t brand;
    uint8_t battery_level;
    uint8_t scene_count;
    uint16_t rf_fw_ver;
    uint16_t motor_fw_ver;
} SHADE_INFO, *SHADE_INFO_PTR;

typedef struct
{
    uint16_t device_id;
    uint8_t brand;
    uint16_t battery_level;
    uint16_t pos_val;
    ePosKind kind;
} __attribute__((packed)) SHADE_STATE, *SHADE_STATE_PTR;

typedef struct
{
    uint16_t device_id;
    strPositions positions;
} SHADE_POSITION, *SHADE_POSITION_PTR;

#define MAX_BATTERY_MEASUREMENTS_PER_SHADE      5
typedef struct BATTERY_CHECK_STRUCT_TAG
{
    uint16_t replicate_counter;
    uint16_t replicate_level[MAX_BATTERY_MEASUREMENTS_PER_SHADE];
    uint16_t shade_id;
    uint16_t bat_level;
    COARSE_BATTERY_LEVEL coarse_lvl;
    COARSE_BATTERY_LEVEL last_coarse_lvl;
    uint8_t type;
} BATT_CHECK_STRUCT, *BATT_CHECK_STRUCT_PTR;


//maximum size of serial message coming from nordic when shade request is sent with tx option not equal to no network header
#define MAX_SYSTEM_INDICATION_SERIAL_RESPONSE_SIZE sizeof(SYSTEM_INDICATION_STRUCT)

void RC_InitRadio(void);
bool RC_IsSerialFlashProgrammed(void);
void RC_ResetRadio(void);
void RC_RestoreRadioDefault(void);
void RC_HandleRadioConfirmation(PARSE_KEY_STRUCT_PTR p_ser_msg);
void RC_AssignNewNetworkId(uint16_t new_id);
bool RC_IsNetworkIdAssigned(void);
bool RC_IsRadioOn(void);
uint64_t RC_GetNordicUuid(void);
uint16_t RC_GetNetworkId(void);
uint16_t RC_CreateRandomNetworkId(void);
void RC_HandleResetIndication(SYSTEM_INDICATION_STRUCT_PTR);
void RC_HandleVersionIndication(SYSTEM_INDICATION_STRUCT_PTR);
void RC_ForceNordicReset(void);
void RC_RequestVersion(void);

COARSE_BATTERY_LEVEL SC_GetCoarseBatteryLevel(uint8_t shade_type, uint8_t voltage);
void SC_ScheduleBatteryCheck(void);
uint8_t* SC_ProcessShadeConfirmation(PARSE_KEY_STRUCT_PTR p_ser_msg);
void SC_HandleShadeConfirmationResult(uint8_t* p_ser_rsp);
void SC_LoadNewCommand(SHADE_COMMAND_INSTRUCTION_PTR p_cmd);

void SC_HandleShadeIndication(PARSE_KEY_STRUCT_PTR p_rf_response);
void SC_HandleShadeIndicationFromSlave(PARSE_KEY_STRUCT_PTR p_rf_response, uint16_t size);
//void SC_HandleSystemIndicationFromMaster(SYSTEM_INDICATION_STRUCT_PTR p_sys_ind);

void SC_ProcessRFTimeout(void);
void SC_GetNextMessageToSend(void);
void SC_ProcessDiscoveredList(void);

void RNC_InitRFNetworkCommunicationTasks(void);
void IO_StartDiscoveryTimer(void);
void RNC_SendNordicConfigRequest(RNC_CONFIG_REC_PTR p_cfg_rec);
void RNC_SendShadeRequest(SHADE_COMMAND_INSTRUCTION_PTR p_cfg_rec);
// void RNC_SendShadeRequestForSlave(SHADE_COMMAND_INSTRUCTION_PTR p_cfg_rec);
// void RNC_SendSystemIndicationForSlave(SYSTEM_INDICATION_STRUCT_PTR p_sys_ind);
void RNC_SendNordicConfigConfirmation(PARSE_KEY_STRUCT_PTR p_ser_msg);
void RNC_SendShadeConfirmation(PARSE_KEY_STRUCT_PTR p_ser_msg);
void RNC_SendShadeIndication(PARSE_KEY_STRUCT_PTR p_ser_msg);
void RNC_SendSystemIndication(PARSE_KEY_STRUCT_PTR p_ser_msg);
void RNC_BeginDiscoveryProcessing(void);
void RNC_StartTickTimer(void);
void RNC_StopTickTimer(void);
void RNC_NotifySerialTimeout(DESTINATION_DEVICE_TYPE type);
void RNC_AddTransportLayer(uint8_t * p_msg_send, uint8_t * p_msg_raw);
bool SC_IsNetworkJoiningActive(void);


void SC_MoveShade( P3_Address_Mode_Type adr_mode,
                            P3_Address_Internal_Type * address,
                            MOVE_DIRECTION_T dir);
void SC_EnableNetworkJoin(void);
void SC_DisableNetworkJoin(uint16_t token);
void SC_DiscoverShades(bool is_absolute,uint16_t type);
void SC_ConditionalDiscovery( void );
//void SC_AbsoluteDiscovery( void );
bool SC_IsDiscoveryActive(void);
void SC_SetDiscoveredFlag( P3_Address_Mode_Type adr_mode,
                            P3_Address_Internal_Type * address );

void SC_GetShadePosition( P3_Address_Mode_Type adr_mode,
                            P3_Address_Internal_Type * address );
void SC_SetShadePosition( P3_Address_Mode_Type adr_mode,
                            P3_Address_Internal_Type * address,
                            strPositions * pos);

void SC_RequestShadeStatus(P3_Address_Mode_Type adr_mode,
                             P3_Address_Internal_Type * address);
void SC_RequestShadeType(P3_Address_Mode_Type adr_mode,
                             P3_Address_Internal_Type * address);
void SC_RequestBatteryLevel(P3_Address_Mode_Type adr_mode,
                             P3_Address_Internal_Type * address,
                             void(*p_callback)(void*));
void SC_CheckShadeBattery(P3_Address_Mode_Type adr_mode,
                             P3_Address_Internal_Type * address);
void SC_BeginBatteryCheckProcess(uint16_t);

void SC_SetSceneToCurrent( P3_Address_Mode_Type adr_mode,
                            P3_Address_Internal_Type * address,
                            uint8_t scene_id);
void SC_SetSceneAtPosition( P3_Address_Mode_Type adr_mode,
                            P3_Address_Internal_Type * address,
                            uint8_t scene_id,
                            strPositions * pos );

void SC_ExecuteScene(uint8_t scene_count, uint8_t * scene_id);
void SC_ExecuteSceneProc(uint8_t scene_count,
                        uint8_t * scene_id,
                        void(*p_callback)(void*));
void SC_DeleteScene( P3_Address_Mode_Type adr_mode,
                        P3_Address_Internal_Type * address, 
                        uint8_t scene_id);
void SC_RequestScenePosition( P3_Address_Mode_Type adr_mode,
                        P3_Address_Internal_Type * address, 
                        uint8_t scene_id);
void SC_GroupAssign(P3_Address_Mode_Type adr_mode,
                        P3_Address_Internal_Type * address,
                        uint8_t group_id,
                        bool isAssigned);
void SC_RequestGroup(P3_Address_Mode_Type adr_mode,
                        P3_Address_Internal_Type * address);
void SC_IssueBeacon(void);
void SC_JogShade(P3_Address_Mode_Type adr_mode,
                             P3_Address_Internal_Type * address);
void SC_ResetShade(P3_Address_Mode_Type adr_mode,
                        P3_Address_Internal_Type * address,
                        uint16_t cfg);
bool SC_ResetAllShades(void);
void SC_RequestReceiverFW(P3_Address_Mode_Type adr_mode,
                             P3_Address_Internal_Type * address);
void SC_RequestMotorFW(P3_Address_Mode_Type adr_mode,
                             P3_Address_Internal_Type * address);
uint16_t SC_GetLowBatteryCount(void);
void SC_DebugForceBatteryCheck(void);

//Scene Controller output interface functions
void SC_SceneControllerClearedAck(uint16_t controller_id);
void SC_SceneControllerUpdateHeader(uint16_t controller_id, SC_SCENE_CTL_UPDATE_HDR_STR_PTR p_update_hdr);
void SC_SceneControllerUpdatePacket(uint16_t controller_id, SC_SCENE_CTL_UPDATE_PACKET_STR_PTR p_update_packet);
void SC_SceneControllerTriggerAck(uint16_t controller_id, SC_SCENE_CTL_TRIGGER_ACK_STR_PTR p_trigger_ack);

//debug functions
void SC_Test_DeleteAllShades(void);
void SC_Test(void);
void SC_GetDebugStatus(P3_Address_Mode_Type adr_mode,
                             P3_Address_Internal_Type * address);
void RC_TestReadNetworkId(void);

void SC_NotifySerialComplete(uint8_t * p_status);

uint16_t SI_GetShadeBattData( BATT_CHECK_STRUCT_PTR p_batt_check_data);
uint16_t SI_GetShadeCount(void);

void LED_off(void);
void IO_reset_radio_return(uint16_t);

void sc_beacon_indication_received(PARSE_KEY_STRUCT_PTR);
void RNC_sendNetworkIDToSlaves(void);
void scheduleToSendNetworkIdToSlaves(void);

#endif
