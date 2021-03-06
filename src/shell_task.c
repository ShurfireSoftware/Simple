/*HEADER**********************************************************************
*
* Copyright 2011 Freescale Semiconductor, Inc.
* Copyright 1989-2008 ARC International
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
Provide MFS file system on external RAM
*
*   
*
*
*END************************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <shell.h>

#include "config.h"



/*TASK*-----------------------------------------------------------------
*
* Function Name  : Shell_Task
* Returned Value : void
* Comments       :
*
*END------------------------------------------------------------------*/
void *shell_task(void *temp) 
{ 
    (void)temp; /* suppress 'unused variable' warning */
    printf("Hunter Douglas Platinum Hub Test Interface\n\r");
    while(1) {
        Shell(Shell_commands);
    }
//    _task_block();
}
 

/* EOF */
