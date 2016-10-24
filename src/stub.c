#include <string.h>
#include "util.h"
#include "rf_serial_api.h"
#include "stub.h"
#include "SCH_ScheduleTask.h"
#include "RMT_RemoteServers.h"
#include "os.h"

int32_t currentTimeOffset=0;

float latitude = 40;
float longitude = -105;
//float latitude = 31.228611;
//float longitude = 121.474722;

MUTEX_STRUCT flashDeviceMutex = {0};
uint16_t RMT_TimeServerTokenSeconds = NULL_TOKEN;
bool flashDataNeedsWritten = false;
bool flashVectorNeedsWritten = false;
uint16_t sunriseTimeInMinutes, sunsetTimeInMinutes;
uint16_t LastPort;
char timeZoneName[ItsMaxTimeZoneNameLength_] = "America\\/Denver";
//strcpy(timeZoneName,"Asia/Shanghai");
char RMT_Pin[5] = {0,0,0,0,0};


const strScheduledEvent event[] = {
    {  
        .uID = 13566,
        .sceneOrMultiSceneID = 18268,
        .enabledFlags.flags={.isEnabled = 1,.daySunday = 1,.dayMonday = 1,.dayTuesday = 1,.dayWednesday = 1,.dayThursday = 1,.dayFriday = 1,.daySaturday = 1},
        .typeFlags.byte = 0,
        .hours = 11,
        .minutes = 0
    },
    {  
        .uID = 49065,
        .sceneOrMultiSceneID = 61716,
        .enabledFlags.flags={.isEnabled = 1,.daySunday = 1,.dayMonday = 1,.dayTuesday = 1,.dayWednesday = 1,.dayThursday = 1,.dayFriday = 1,.daySaturday = 1},
        .typeFlags.byte = 0,
        .hours = 11,
        .minutes = 5
    },
    {  
        .uID = 45954,
        .sceneOrMultiSceneID = 19824,
        .enabledFlags.flags={.isEnabled = 1,.daySunday = 1,.dayMonday = 1,.dayTuesday = 1,.dayWednesday = 1,.dayThursday = 1,.dayFriday = 1,.daySaturday = 1},
        .typeFlags.byte = 0,
        .hours = 11,
        .minutes = 10
    },
    {  
        .uID = 60126,
        .sceneOrMultiSceneID = 15088,
        .enabledFlags.flags={.isEnabled = 1,.daySunday = 1,.dayMonday = 1,.dayTuesday = 1,.dayWednesday = 1,.dayThursday = 1,.dayFriday = 1,.daySaturday = 1},
        .typeFlags.byte = 0,
        .hours = 12,
        .minutes = 0
    },
    {  
        .uID = 21414,
        .sceneOrMultiSceneID = 56185,
        .enabledFlags.flags={.isEnabled = 1,.daySunday = 1,.dayMonday = 1,.dayTuesday = 1,.dayWednesday = 1,.dayThursday = 1,.dayFriday = 1,.daySaturday = 1},
        .typeFlags.byte = 0,
        .hours = 12,
        .minutes = 1
    },
    {  
        .uID = 62511,
        .sceneOrMultiSceneID = 42740,
        .enabledFlags.flags={.isEnabled = 1,.daySunday = 1,.dayMonday = 1,.dayTuesday = 1,.dayWednesday = 1,.dayThursday = 1,.dayFriday = 1,.daySaturday = 1},
        .typeFlags.byte = 0,
        .hours = 12,
        .minutes = 2
    },
    {  
        .uID = 23037,
        .sceneOrMultiSceneID = 49099,
        .enabledFlags.flags={.isEnabled = 1,.daySunday = 1,.dayMonday = 1,.dayTuesday = 1,.dayWednesday = 1,.dayThursday = 1,.dayFriday = 1,.daySaturday = 1},
        .typeFlags.byte = 0,
        .hours = 12,
        .minutes = 3
    },
    {  
        .uID = 40355,
        .sceneOrMultiSceneID = 28534,
        .enabledFlags.flags={.isEnabled = 1,.daySunday = 1,.dayMonday = 1,.dayTuesday = 1,.dayWednesday = 1,.dayThursday = 1,.dayFriday = 1,.daySaturday = 1},
        .typeFlags.byte = 0,
        .hours = 12,
        .minutes = 4
    },
    {  
        .uID = 34064,
        .sceneOrMultiSceneID = 30885,
        .enabledFlags.flags={.isEnabled = 1,.daySunday = 1,.dayMonday = 1,.dayTuesday = 1,.dayWednesday = 1,.dayThursday = 1,.dayFriday = 1,.daySaturday = 1},
        .typeFlags.byte = 0,
        .hours = 12,
        .minutes = 5
    },
    {  
        .uID = 43568,
        .sceneOrMultiSceneID = 21414,
        .enabledFlags.flags={.isEnabled = 1,.daySunday = 1,.dayMonday = 1,.dayTuesday = 1,.dayWednesday = 1,.dayThursday = 1,.dayFriday = 1,.daySaturday = 1},
        .typeFlags.byte = 0,
        .hours = 12,
        .minutes = 6
    },
    {  
        .uID = 15883,
        .sceneOrMultiSceneID = 34892,
        .enabledFlags.flags={.isEnabled = 1,.daySunday = 1,.dayMonday = 1,.dayTuesday = 1,.dayWednesday = 1,.dayThursday = 1,.dayFriday = 1,.daySaturday = 1},
        .typeFlags.byte = 0,
        .hours = 12,
        .minutes = 7
    },
    {  
        .uID = 22208,
        .sceneOrMultiSceneID = 27772,
        .enabledFlags.flags={.isEnabled = 1,.daySunday = 1,.dayMonday = 1,.dayTuesday = 1,.dayWednesday = 1,.dayThursday = 1,.dayFriday = 1,.daySaturday = 1},
        .typeFlags.byte = 0,
        .hours = 12,
        .minutes = 8
    }
};


bool isEnableScheduledEvents(void)
{
    return true;
}

void retrieveRegistrationData(void)
{
}

bool sendMultiSceneMsgToShades(uint16_t id)
{
    return true;
}

bool ExecuteSceneFromRemoteConnect(uint16_t thisSceneID)
{
    return true;
}

char *getHubId(void)
{
    return "4FAB6C7A5458376F";
}

char *getEmail(void)
{
    return "firstname.lastname@somedomain.com";
}

char *getHubKey(void)
{
    return "fa7256de930e7580177d08645090bbd04a26ac22fdd8957b2efd65c64e52448e";
}

#ifndef USE_ME
void FF_ReadListOfScheduledEvents(ALL_RAW_DB_STR_PTR * p_sched_list_str)
{
    uint16_t n;
    uint16_t i;
    strScheduledEvent *p_single_schedule;

    n = sizeof(event)/sizeof(strScheduledEvent);
    *p_sched_list_str = (ALL_RAW_DB_STR_PTR)OS_GetMemBlock(2 + n * sizeof(strScheduledEvent));
    p_sched_list_str[0]->count = n;

    p_single_schedule = (strScheduledEvent*)&p_sched_list_str[0]->db_list;
    for (i=0; i < p_sched_list_str[0]->count; ++i) {
        memcpy(p_single_schedule,&event[i],sizeof(strScheduledEvent));
        ++p_single_schedule;
    }
}
#else
void FF_ReadListOfScheduledEvents(ALL_RAW_DB_STR_PTR * p_sched_list_str)
{
    *p_sched_list_str = (ALL_RAW_DB_STR_PTR)OS_GetMemBlock(2 + sizeof(strScheduledEvent));
    p_sched_list_str[0]->count = 0;
}
#endif

bool RCR_RegisterWithRemoteConnect(char *hub_id, char *hub_name, char *pv_key)
{
    return true;
}

bool RCR_UnregisterHub(void)
{
    return true;
}

bool isHubRegistered(void)
{
    return false;
}

bool isRegistrationActive(void)
{
    return false;
}

void clearRegistrationData(void)
{
}

uint32_t setEnableScheduledEvents(bool value)
{
    return 204;
}

void sendSystemIndicationToSlaveHubs(void *p)
{
}

void sendNetworkIDToSlaveHubs(uint16_t networkId)
{
}

uint16_t SI_GetShadeBattData( BATT_CHECK_STRUCT_PTR p_batt_check_data)
{
return 0;
}

uint16_t SI_GetShadeCount(void)
{
return 0;
}

void LED_Flicker(bool is_active)
{
}

void LED_NetworkID(bool is_active)
{
}

void LED_NordicFlash(bool is_active)
{
}

void LED_RemoteFirmwareDownload(bool is_active)
{
}

bool maintainFlash(uint8_t x)
{
return true;
}

bool _time_to_date(TIME_STRUCT_PTR t, DATE_STRUCT_PTR d)
{
return true;
}

void RDS_TriggerRemoteSync(uint16_t token)
{
}

uint8_t _mutex_try_lock(MUTEX_STRUCT * mutex)
{
    return MQX_EOK;
}

void RMT_SetPin(char * p_pin)
{
    memcpy(RMT_Pin, p_pin, 4);
    RMT_Pin[4] = 0;
}

void sendShadeCommandInstructionToSlaveHubs(SHADE_COMMAND_INSTRUCTION_PTR p_cfg_rec)
{
}

void writeRestartTimeToFlash(time_t * now1)
{
}

void RESET_HUB(void)
{
}

void sendTextMessageToSlaveHubs(char * p_msg)
{
}

void OS_SchedLock(void)
{
}

void OS_SchedUnlock(void)
{
}

void _watchdog_start(uint32_t SCH_WATCHDOG_INTERVAL)
{
}

void _watchdog_stop(void)
{
}

void writeDataBufferToCurrentSector(void)
{
}

void writeVectorBufferToCurrentSector(void)
{
}

void writeSunriseToFlash(time_t * time)
{
}

void writeSunsetToFlash(time_t * time)
{
}

void readRestartTimeFromFlash(time_t * time)
{
    *time = 0;
}

bool IO_IsSelfTestActive(void)
{
    return false;
}

void setLocalTimeOffset(int32_t time_offset)
{
}

void _mutex_unlock(MUTEX_STRUCT * mutex)
{
}

uint32_t RFS_ReportFaults(uint16_t total)
{
    return 0;
}

void getRemoteConnectPin( char * p_pin)
{
    p_pin[0] = '1';
    p_pin[1] = '2';
    p_pin[2] = '3';
    p_pin[3] = '4';
    p_pin[4] = 0;
}

void RDS_SyncDataImmediately(uint16_t token)
{
}

void RDS_SyncDataWithRemoteConnect(void)
{
}

void RDS_CloseJSONFile(FILE * p_file)
{
}

uint32_t RDS_GetJSONSize(void)
{
    return 1;
}

uint16_t RDS_ReadJSONFile(char* p_buff, uint16_t count, FILE * p_file)
{
    return 1;
}

FILE * RDS_OpenJSONFile(void)
{
    FILE * p_file;
    return p_file;
}

bool isNestHomeAwayEnabled(void)
{
    return false;
}

bool isNestRushHourEnabled(void)
{
    return false;
}

void executeAwayScene(void)
{
}

void executeRHRScene(void)
{
}

bool isNewAwayScene(void)
{
    return false;
}

bool isNewRHRScene(void)
{
    return false;
}

void clearIntegrations(void)
{
}


