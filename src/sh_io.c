/**HEADER********************************************************************
* 
* Copyright (c) 2008 Freescale Semiconductor;
* All Rights Reserved
*
* Copyright (c) 2004-2008 Embedded Access Inc.;
* All Rights Reserved
*
*************************************************************************** 
*
* THIS SOFTWARE IS PROVIDED BY FREESCALE "AS IS" AND ANY EXPRESSED OR 
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  
* IN NO EVENT SHALL FREESCALE OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
* IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
* THE POSSIBILITY OF SUCH DAMAGE.
*
**************************************************************************
*
* $FileName: sh_io.c$
* $Version : 3.8.1.0$
* $Date    : Sep-19-2011$
*
* Comments:
*
*   This file contains an IO shell command.
*
*END************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <shell.h>

#include "sh_io.h"

#define TRUE true
#define FALSE false
#include "rf_serial_api.h"
#include "SCH_ScheduleTask.h"
#include "stub.h"
#include "ipc_client_cmd_to_db.h"
#include "os.h"

#define SHELL_MAX_ARGS       4

void test_json(void)
{
    ALL_RAW_DB_STR_PTR p_sched = IPC_Client_GetScheduledEvents();
    printf("Sched count = %d\n",p_sched->count);
    OS_ReleaseMemBlock(p_sched);
}

int32_t Shell_test(int32_t argc, char * argv[] )
{
    bool print_usage;
    bool shorthelp = false;
    int32_t return_code = SHELL_EXIT_SUCCESS;

    print_usage = Shell_check_help_request(argc, argv, &shorthelp );

    if (!print_usage) {

        switch (argc) {
            case 1:
                test_json();
                break;
            default:
                printf("Error, %s invoked with incorrect number of arguments\n", argv[0]);
                return_code = SHELL_EXIT_ERROR;
                print_usage = true;
                break;
        }
    }
    if (print_usage) {
        if (shorthelp) {
            printf("%s No Parameters\n", argv[0]);
        }
        else {
            printf("Usage: %s No Parameters\n", argv[0]);
        }
    }
    return return_code;
}

int32_t Shell_read_nid(int32_t argc, char * argv[] )
{ 
    bool print_usage;
    uint16_t nid;
    bool shorthelp = FALSE;
    int32_t return_code = SHELL_EXIT_SUCCESS;

    print_usage = Shell_check_help_request(argc, argv, &shorthelp );

    if (!print_usage) {
        switch (argc) {
            case 1:

                nid = RC_GetNetworkId();
                printf("Network ID = %04x\n", nid);
                break;
            default:
                printf("Error, %s invoked with incorrect number of arguments\n", argv[0]);
                return_code = SHELL_EXIT_ERROR;
                print_usage = TRUE;
                break;
        }
    }
    if (print_usage) {
        if (shorthelp) {
            printf("%s No Parameters\n", argv[0]);
        }
        else {
            printf("Usage: %s No Parameters\n", argv[0]);
        }
    }
    return return_code;
}

int32_t Shell_read_time(int32_t argc, char * argv[] )
{
    bool print_usage;
    bool shorthelp = FALSE;
    int32_t return_code = SHELL_EXIT_SUCCESS;

    print_usage = Shell_check_help_request(argc, argv, &shorthelp );

    if (!print_usage) {
        switch (argc) {
            case 1:
                SCH_DisplayCurrentTime();
                break;
            default:
                printf("Error, %s invoked with incorrect number of arguments\n", argv[0]);
                return_code = SHELL_EXIT_ERROR;
                print_usage = TRUE;
                break;
        }
    }
    if (print_usage) {
        if (shorthelp) {
            printf("%s No Parameters\n", argv[0]);
        }
        else {
            printf("Usage: %s No Parameters\n", argv[0]);
        }
    }
    return return_code;
}

int32_t Shell_aws(int32_t argc, char * argv[] )
{ 
	bool print_usage;
    bool shorthelp = FALSE;
    int32_t return_code = SHELL_EXIT_SUCCESS;
#ifdef USE_ME

    print_usage = Shell_check_help_request(argc, argv, &shorthelp );

    if (!print_usage) {
        if (argc !=  2) {
            printf("Error, invalid number of parameters\n");
            return_code = SHELL_EXIT_ERROR;
            print_usage=TRUE;
        }
        else {
            if (!strcmp(argv[1],"start")) {
                AWS_BeginConnection();
            }
            else if (!strcmp(argv[1],"stop")) {
                AWS_EndConnection();
            }
            else if (!strcmp(argv[1],"publish")) {
                AWS_PublishActionStatus(1,2);
            }
            else {
                printf("Error, invalid parameter\n");
                return_code = SHELL_EXIT_ERROR;
                print_usage=TRUE;
            }
        }
    }
    if (print_usage) {
        if (shorthelp) {
            printf("%s <start or stop or publish>\n", argv[0]);
        }
        else {
            printf("%s <start or stop or publish>\n", argv[0]);
        }
    }
#endif
    return return_code;
}

int32_t Shell_md5(int32_t argc, char * argv[] )
{ 
	bool print_usage;

    bool shorthelp = FALSE;
    int32_t return_code = SHELL_EXIT_SUCCESS;
#ifdef USE_ME

    print_usage = Shell_check_help_request(argc, argv, &shorthelp );

    if (!print_usage) {
        if (argc !=  1) {
            printf("Error, invalid number of parameters\n");
            return_code = SHELL_EXIT_ERROR;
            print_usage=TRUE;
        }
        else {
            printf("Not Implemented\n");
        }
    }
    if (print_usage) {
        if (shorthelp) {
            printf("%s <start or stop or publish>\n", argv[0]);
        }
        else {
            printf("%s <start or stop or publish>\n", argv[0]);
        }
    }
#endif
    return return_code;
}

int32_t Shell_set_nid(int32_t argc, char * argv[] )
{
    bool print_usage;
    bool shorthelp = FALSE;
    int32_t return_code = SHELL_EXIT_SUCCESS;
    uint16_t nid;

    print_usage = Shell_check_help_request(argc, argv, &shorthelp );

    if (!print_usage) {
        if (argc !=  2) {
            printf("Error, invalid number of parameters\n");
            return_code = SHELL_EXIT_ERROR;
            print_usage=TRUE;
        }
        else {
            if ((argv[1][0] == '0') && (argv[1][1] == 'x')) {
                nid = strtol(argv[1],NULL,0);
            }
            else {
                nid = atoi((char*)argv[1]);
            }
//_lpm_write_rfvbat(0, 0x1234);
            RC_AssignNewNetworkId(nid);
        }
    }
    if (print_usage)  {
        if (shorthelp)  {
            printf("%s <nid>\n", argv[0]);
        }
        else  {
            printf("Usage: %s <nid>\n", argv[0]);
            printf("   <nid>     = integer(decimal or hex)\n");
        }
    }
    return return_code;
}

int32_t Shell_position_single_shade(int32_t argc, char * argv[] )
{ 
    bool print_usage;
    bool shorthelp = FALSE;
    int32_t return_code = SHELL_EXIT_SUCCESS;
    P3_Address_Internal_Type address;
    uint16_t id;
    strPositions pos;

    print_usage = Shell_check_help_request(argc, argv, &shorthelp );

    if (!print_usage) {
        if (argc !=  4) {
            printf("Error, invalid number of parameters\n");
            return_code = SHELL_EXIT_ERROR;
            print_usage=TRUE;
        }
        else {
            if ((argv[1][0] == '0') && (argv[1][1] == 'x')) {
                id = strtol(argv[1],NULL,0);
            }
            else {
                id = atoi((char*)argv[1]);
            }
            pos.posKind[0] = (ePosKind)atoi((char*)argv[2]);
            if ((uint16_t)pos.posKind[0] > 3) {
                print_usage = TRUE;
            }
            if ((argv[3][0] == '0') && (argv[3][1] == 'x')) {
                pos.position[0] = strtol(argv[3],NULL,0);
            }
            else {
                pos.position[0] = atoi((char*)argv[3]);
            }
            if (!print_usage) {

                address.Unique_Id = 0;
                address.Device_Id = id;
                pos.posCount = 1;
                SC_SetShadePosition( P3_Address_Mode_Device_Id,
                            &address,&pos);
            }
        }
    }
    if (print_usage)  {
        if (shorthelp)  {
            printf("%s <id> <kind> <value>\n", argv[0]);
        }
        else  {
            printf("Usage: %s <id> <kind> <dir(u or d)>\n", argv[0]);
            printf("   <id>     = integer(decimal or hex)\n");
            printf("   <kind>   = 1,2 or 3\n");
            printf("   <value>  = integer(decimal or hex)\n");
        }
    }
    return return_code;
}
int32_t Shell_jog(int32_t argc, char * argv[] )
{ 
    bool print_usage;
    bool shorthelp = FALSE;
    int32_t return_code = SHELL_EXIT_SUCCESS;
    uint16_t id;
    P3_Address_Internal_Type address;

    print_usage = Shell_check_help_request(argc, argv, &shorthelp );

    if (!print_usage) {
        if (argc !=  2) {
            printf("Error, invalid number of parameters\n");
            return_code = SHELL_EXIT_ERROR;
            print_usage=TRUE;
        }
        else {
            if ((argv[1][0] == '0') && (argv[1][1] == 'x')) {
                id = strtol(argv[1],NULL,0);
            }
            else {
                id = atoi((char*)argv[1]);
            }
            address.Unique_Id = 0;
            address.Device_Id = id;
            SC_JogShade(P3_Address_Mode_Device_Id, &address);
        }
    }
    if (print_usage)  {
        if (shorthelp)  {
            printf("%s <id>\n", argv[0]);
        }
        else  {
            printf("Usage: %s <id>\n", argv[0]);
            printf("   <id>      = integer(decimal or hex)\n");
        }
    }
    return return_code;
}

int32_t Shell_get_shade_position(int32_t argc, char * argv[] )
{
    bool print_usage;
    bool shorthelp = FALSE;
    int32_t return_code = SHELL_EXIT_SUCCESS;
    uint16_t id;
    P3_Address_Internal_Type address;

    print_usage = Shell_check_help_request(argc, argv, &shorthelp );

    if (!print_usage) {
        if (argc !=  2) {
            printf("Error, invalid number of parameters\n");
            return_code = SHELL_EXIT_ERROR;
            print_usage=TRUE;
        }
        else {
            if ((argv[1][0] == '0') && (argv[1][1] == 'x')) {
                id = strtol(argv[1],NULL,0);
            }
            else {
                id = atoi((char*)argv[1]);
            }
            address.Unique_Id = 0;
            address.Device_Id = id;
            SC_GetShadePosition(P3_Address_Mode_Device_Id, &address);
        }
    }
    if (print_usage)  {
        if (shorthelp)  {
            printf("%s <id>\n", argv[0]);
        }
        else  {
            printf("Usage: %s <id>\n", argv[0]);
            printf("   <id>      = integer(decimal or hex)\n");
        }
    }
    return return_code;
}


/* EOF */
