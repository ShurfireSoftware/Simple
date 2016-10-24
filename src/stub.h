#ifndef _STUB_H_
#define _STUB_H_

#include <stdio.h>

#define HW_VERSION              "19"
#define IO_ERROR    0

#define ItsMaxHubIdLength_  16
#define ItsHubKeyLength_ 64
#define ItsMaxTimeZoneNameLength_ 40

//in NBT_NordicBootloadTask
uint32_t NBT_GetNordicFirmwareVersion(void);
void NBT_BeginNordicDownload(void);
bool NBT_VerifyNordicFiles(void);
bool NBT_IsNordicDownloadActive(void);

char *getHubId(void);
char *getHubKey(void);
char *getEmail(void);

typedef struct date_struct
{
   /*! \brief Range from 1970 to 2099. */
   int16_t YEAR;

   /*! \brief Range from 1 to 12. */
   int16_t MONTH;

   /*! \brief Range from 1 to 31 (depending on month). */
   int16_t DAY;

   /*! \brief Range from 0 to 23. */
   int16_t HOUR;

   /*! \brief Range from 0 to 59. */
   int16_t MINUTE;

   /*! \brief Range from 0 to 59. */
   int16_t SECOND;

   /*! \brief Range from 0 to 999. */
   int16_t MILLISEC;
   
   /*! \brief Day of the week. Sunday is day 0. Range is from 0 to 6. */
   int16_t WDAY;
   
   /*! \brief Day of the year. Range is from 0 to 365. */
   int16_t YDAY;

} DATE_STRUCT, * DATE_STRUCT_PTR;

/*
Data Type: struct tm
This is the data type used to represent a broken-down time. The structure contains at least the following members, which can appear in any order.
int tm_sec
This is the number of full seconds since the top of the minute (normally in the range 0 through 59, but the actual upper limit is 60, to allow for leap seconds if leap second support is available).
int tm_min
This is the number of full minutes since the top of the hour (in the range 0 through 59).
int tm_hour
This is the number of full hours past midnight (in the range 0 through 23).
int tm_mday
This is the ordinal day of the month (in the range 1 through 31). Watch out for this one! As the only ordinal number in the structure, it is inconsistent with the rest of the structure.
int tm_mon
This is the number of full calendar months since the beginning of the year (in the range 0 through 11). Watch out for this one! People usually use ordinal numbers for month-of-year (where January = 1).
int tm_year
This is the number of full calendar years since 1900.
int tm_wday
This is the number of full days since Sunday (in the range 0 through 6).
int tm_yday
This is the number of full days since the beginning of the year (in the range 0 through 365).
int tm_isdst
This is a flag that indicates whether Daylight Saving Time is (or was, or will be) in effect at the time described. The value is positive if Daylight Saving Time is in effect, zero if it is not, and negative if the information is not available.
long int tm_gmtoff
This field describes the time zone that was used to compute this broken-down time value, including any adjustment for daylight saving; it is the number of seconds that you must add to UTC to get local time. You can also think of this as the number of seconds east of UTC. For example, for U.S. Eastern Standard Time, the value is -5*60*60. The tm_gmtoff field is derived from BSD and is a GNU library extension; it is not visible in a strict ISO C environment.
const char *tm_zone
This field is the name for the time zone that was used to compute this broken-down time value. Like tm_gmtoff, this field is a BSD and GNU extension, and is not visible in a strict ISO C environment.
*/

typedef struct time_struct
{

   /*! \brief The number of seconds in the time. */
   uint32_t     SECONDS;

   /*! \brief The number of milliseconds in the time. */
   uint32_t     MILLISECONDS;

} TIME_STRUCT, * TIME_STRUCT_PTR;
/*
time_t equivalent to time_struct.SECONDS
*/

/*
Data Type: struct timeval
The struct timeval structure represents an elapsed time. It is declared in `sys/time.h' and has the following members:
long int tv_sec
This represents the number of whole seconds of elapsed time.
long int tv_usec
This is the rest of the elapsed time (a fraction of a second), represented as the number of microseconds. It is always less than one million.
*/

/*


//equal to _time_to_date
struct tm * localtime_r (const time_t *time, struct tm *resultp)
//equal to _time_from_date
time_t mktime (struct tm *brokentime)
//return the current time that has time zone included
 time_t time (time_t *result)
//return current utc time
int gettimeofday (struct timeval *tp, struct timezone *tzp)
//set current time
int settimeofday (const struct timeval *tp, const struct timezone *tzp)


code sample:
#define SIZE 256

int
main (void)
{
  char buffer[SIZE];
  time_t curtime;
  struct tm *loctime;

  // Get the current time.
  curtime = time (NULL);

  // Convert it to local time representation.
  loctime = localtime (&curtime);

  // Print out the date and time in the standard format.
  fputs (asctime (loctime), stdout);

  // Print it out in a nice format.
  strftime (buffer, SIZE, "Today is %A, %B %d.\n", loctime);
  fputs (buffer, stdout);
  strftime (buffer, SIZE, "The time is %I:%M %p.\n", loctime);
  fputs (buffer, stdout);

  return 0;
}
*/
#include "rf_serial_api.h"

typedef struct {
    uint8_t unused;
} MUTEX_STRUCT;

typedef enum httpsrv_req_method
{
    HTTPSRV_REQ_UNKNOWN,
    HTTPSRV_REQ_GET,
    HTTPSRV_REQ_POST,
    HTTPSRV_REQ_HEAD,
    // EAI: Added support for PUT, DELETE, PATCH and OPTIONS
    HTTPSRV_REQ_PUT,
    HTTPSRV_REQ_DELETE,
    HTTPSRV_REQ_PATCH,
    HTTPSRV_REQ_OPTIONS
} HTTPSRV_REQ_METHOD;

typedef enum rtcs_ssl_init_type
{
    RTCS_SSL_SERVER,
    RTCS_SSL_CLIENT
}RTCS_SSL_INIT_TYPE;

typedef struct rtcs_ssl_params_struct
{
    char*              cert_file;       /* Client or Server Certificate file.*/
    char*              priv_key_file;   /* Client or Server private key file.*/
    char*              ca_file;         /* CA (Certificate Authority) certificate file.*/
    RTCS_SSL_INIT_TYPE init_type;
    char *             ciphers;
    bool               no_verify;       /* do not verify server */
}RTCS_SSL_PARAMS_STRUCT;

#define MQX_EOK     0

typedef union {
	struct {
		bool daySunday : 1, dayMonday : 1, dayTuesday : 1, dayWednesday : 1,
			dayThursday : 1, dayFriday : 1, daySaturday : 1, isEnabled : 1;
	} flags;
	uint8_t byte; 
} schedEventEnabledFlags;

typedef union {
	struct {
		bool isClock: 1, isSunrise: 1, isMultiSceneID: 1, unusedBit03: 1,
			unusedBit04 : 1, unusedBit05: 1, unusedBit06: 1, unusedBit07: 1;
	} flags;
	uint8_t byte;
} schedEventTypeFlags;

typedef struct {
	uint16_t uID, sceneOrMultiSceneID; 
	schedEventEnabledFlags enabledFlags;
	schedEventTypeFlags typeFlags;
	uint8_t hours;
	int16_t minutes;
} strScheduledEvent;


typedef enum {dtNone = 0, dtShades, dtSceneController, dtAny} eDiscoveryType;
void LED_Flicker(bool is_active);
void LED_NetworkID(bool is_active);
void LED_NordicFlash(bool is_active);
void LED_RemoteFirmwareDownload(bool is_active);

bool maintainFlash(uint8_t);
//void OS_GetTimeLocal(TIME_STRUCT_PTR);
bool _time_to_date(TIME_STRUCT_PTR t, DATE_STRUCT_PTR d);
uint8_t _mutex_try_lock(MUTEX_STRUCT * mutex);
void _mutex_unlock(MUTEX_STRUCT * mutex);

void sendTextMessageToSlaveHubs(char * p_msg);
void sendSystemIndicationToSlaveHubs(void *p);
void sendNetworkIDToSlaveHubs(uint16_t networkId);
void writeRestartTimeToFlash(time_t * now1);
void RESET_HUB(void);
bool isEnableScheduledEvents(void);
bool sendMultiSceneMsgToShades(uint16_t id);
bool ExecuteSceneFromRemoteConnect(uint16_t thisSceneID);
void OS_SchedLock(void);
void OS_SchedUnlock(void);
void _watchdog_start(uint32_t SCH_WATCHDOG_INTERVAL);
void _watchdog_stop(void);
void writeDataBufferToCurrentSector(void);
void writeVectorBufferToCurrentSector(void);
void writeSunriseToFlash(time_t * time);
void writeSunsetToFlash(time_t * time);
void readRestartTimeFromFlash(time_t * time);
bool IO_IsSelfTestActive(void);
void setLocalTimeOffset(int32_t time_offset);
void retrieveRegistrationData(void);
void RDS_SyncDataImmediately(uint16_t token);
void RDS_SyncDataWithRemoteConnect(void);
void RDS_TriggerRemoteSync(uint16_t token);
void RDS_CloseJSONFile(FILE * p_file);
uint16_t RDS_ReadJSONFile(char* p_buff, uint16_t count, FILE * p_file);
FILE * RDS_OpenJSONFile(void);
uint32_t RDS_GetJSONSize(void);
void RMT_FaultNotification(uint16_t unused);
void RMT_SetPin(char * p_pin);
void sendShadeCommandInstructionToSlaveHubs(SHADE_COMMAND_INSTRUCTION_PTR p_cfg_rec);

void getRemoteConnectPin( char * p_pin);
bool isHubRegistered(void);
bool isRegistrationActive(void);

typedef struct SHADE_DB_STR_TAG
{
    uint16_t uID;
    uint16_t roomID;
    uint16_t groupID;
    char name[ItsMaxNameLength_];
    uint8_t sceneMemberCount;
    uint8_t type;
    uint8_t batteryStrength;
    uint8_t batteryStatus;
    uint8_t order;
    bool roomAssigned;
    bool nameAssigned;
    bool groupAssigned;
    uint8_t posKind1;
    uint16_t position1;
    uint8_t posKind2;
    uint16_t position2;
} SHADE_DB_STR, * SHADE_DB_STR_PTR;

typedef struct ALL_RAW_DB_STR_TAG
{
    int16_t count;
    uint8_t db_list[];
} __attribute__((packed)) ALL_RAW_DB_STR, *ALL_RAW_DB_STR_PTR;

void FF_ReadListOfScheduledEvents(ALL_RAW_DB_STR_PTR * p_sched_list_str);

bool isNestHomeAwayEnabled(void);
bool isNestRushHourEnabled(void);
void executeAwayScene(void);
void executeRHRScene(void);
bool isNewAwayScene(void);
bool isNewRHRScene(void);

void clearRegistrationData(void);
void clearIntegrations(void);
uint32_t setEnableScheduledEvents(bool value);

#endif
