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
* $FileName: sh_io.h$
* $Version : 3.8.1.0$
* $Date    : Sep-19-2011$
*
* Comments:
*
*   This file contains an IO shell command.
*
*END************************************************************************/

#ifndef __sh_io_h__
#define __sh_io_h__
#include <stdint.h>

int32_t Shell_test(int32_t argc, char * argv[] );
int32_t Shell_aws(int32_t argc, char * argv[] );
int32_t Shell_md5(int32_t argc, char * argv[] );

int32_t Shell_read_nid(int32_t argc, char * argv[] );
int32_t Shell_read_time(int32_t argc, char * argv[] );
int32_t Shell_set_nid(int32_t argc, char * argv[] );
int32_t Shell_position_single_shade(int32_t argc, char * argv[] );
int32_t Shell_jog(int32_t argc, char * argv[] );
int32_t Shell_get_shade_position(int32_t argc, char * argv[] );

#endif

/* EOF */
