/*HEADER**********************************************************************
*
* Copyright 2008 Freescale Semiconductor, Inc.
* Copyright 2004-2008 Embedded Access Inc.
*
* This software is owned or controlled by Freescale Semiconductor.
* Use of this software is governed by the Freescale MQX RTOS License
* distributed with this Material.
* See the MQX_RTOS_LICENSE file distributed for more details.
*
* Brief License Summary:
* This software is provided in source form for you to use free of charge,
* but it is not open source software. You are allowed to use this software
* but you cannot redistribute it or derivative works of it in source form.
* The software may be used only in connection with a product containing
* a Freescale microprocessor, microcontroller, or digital signal processor.
* See license agreement file for full license terms including other
* restrictions.
*****************************************************************************
*
* Comments:
*
*   
*
*
*END************************************************************************/

#include <stdint.h>
#include <stddef.h>
#include <shell.h>

#include "sh_io.h"


const SHELL_COMMAND_STRUCT Shell_commands[] = {
    { "aws",    Shell_aws },
    { "md5",  Shell_md5 },
    { "read_nid", Shell_read_nid },
    { "read_time", Shell_read_time },
    { "set_nid", Shell_set_nid },
    { "pos_single", Shell_position_single_shade },
    { "jog", Shell_jog },
    { "req_shade_pos",     Shell_get_shade_position },
    { "test",      Shell_test },
    { "help",      Shell_help },
    { "?",         Shell_command_list },
    { NULL,        NULL }
};

/* EOF */
