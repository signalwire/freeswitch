/*******************************************************************************

    KHOMP generic endpoint/channel library.
    Copyright (C) 2007-2010 Khomp Ind. & Com.

  The contents of this file are subject to the Mozilla Public License 
  Version 1.1 (the "License"); you may not use this file except in compliance 
  with the License. You may obtain a copy of the License at 
  http://www.mozilla.org/MPL/ 

  Software distributed under the License is distributed on an "AS IS" basis,
  WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for
  the specific language governing rights and limitations under the License.

  Alternatively, the contents of this file may be used under the terms of the
  "GNU Lesser General Public License 2.1" license (the “LGPL" License), in which
  case the provisions of "LGPL License" are applicable instead of those above.

  If you wish to allow use of your version of this file only under the terms of
  the LGPL License and not to allow others to use your version of this file 
  under the MPL, indicate your decision by deleting the provisions above and 
  replace them with the notice and other provisions required by the LGPL 
  License. If you do not delete the provisions above, a recipient may use your 
  version of this file under either the MPL or the LGPL License.

  The LGPL header follows below:

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; if not, write to the Free Software Foundation, 
    Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*******************************************************************************/

#ifndef _DEFS_H_
#define _DEFS_H_

/* formatation macros */
#include "revision.h"

#define KHOMP_LOG __FILE__, __SWITCH_FUNC__, __LINE__
#define KHOMP_EVENT_MAINT "khomp::maintenance"

#define OBJ_FMT(dev,obj,msg) \
    FMT("%s (d=%02d,c=%03d) " msg) % __SWITCH_FUNC__ % dev % obj

#define OBJ_MSG(dev,obj,msg) \
    "%s (d=%02d,c=%03d) " msg, __SWITCH_FUNC__, dev, obj

#define PVT_FMT(tgt,msg) \
    FMT("%s (d=%02d,c=%03d) " msg) % __SWITCH_FUNC__ % tgt.device % tgt.object

#define PVT_MSG(tgt,msg) \
    "%s (d=%02d,c=%03d) " msg, __SWITCH_FUNC__, tgt.device, tgt.object

#define D(x) ( FMT( "%s: " x ) % __func__ )

#define STR(fmt) \
    STG(fmt).c_str()

/* version controller macro */
#define SWITCH_LESS_THAN(x,y,z)                                                           \
   (FS_VERSION_MICRO != -1) &&                                                            \
   (((FS_VERSION_MAJOR == x)  && (FS_VERSION_MINOR == y)  &&  (FS_VERSION_MICRO <= z)) || \
   ((FS_VERSION_MAJOR == x)  && (FS_VERSION_MINOR < y)) || (FS_VERSION_MAJOR < x))

/* signalling groups macro */
#define CASE_R2_SIG         \
    case ksigR2Digital:     \
    case ksigContinuousEM:  \
    case ksigPulsedEM:      \
    case ksigUserR2Digital: \
    case ksigOpenCAS:       \
    case ksigOpenR2
/*
    case ksigLineSide:      \
    case ksigCAS_EL7:       \
    case ksigE1LC
*/

#define CASE_RDSI_SIG       \
    case ksigPRI_EndPoint:  \
    case ksigPRI_Network:   \
    case ksigPRI_Passive:   \
    case ksigOpenCCS

#define CASE_FLASH_GRP          \
    case ksigLineSide:          \
    case ksigCAS_EL7:           \
    case ksigE1LC

/******************************************************************************/

/* Buffering size constants */

#define SILENCE_PACKS               2

#define KHOMP_READ_PACKET_TIME     16                            // board sample (ms)

#define KHOMP_READ_PACKET_SIZE    (KHOMP_READ_PACKET_TIME *   8) // asterisk sample size (bytes)

#define KHOMP_MIN_READ_PACKET_SIZE (10 * 8)                      // min size to return on khomp_read
#define KHOMP_MAX_READ_PACKET_SIZE (30 * 8)                      // max size to return on khomp_read

#define KHOMP_AUDIO_BUFFER_SIZE   (KHOMP_READ_PACKET_SIZE  *  8) // buffer size (bytes)

/* debug and log macros */
#define DBG(x,y) \
    { \
        if (K::Logger::Logg.classe( C_DBG_##x ).enabled()) \
                K::Logger::Logg( C_DBG_##x , y ); \
    }

#define LOG(x,y) \
    { \
        K::Logger::Logg( C_##x , y ); \
    }

#define LOGC(x,y) \
    { \
        if (K::Logger::Logg.classe( C_##x ).enabled()) \
            LOG( x , y ); \
    }

/* useful to debug arguments */
#define DEBUG_CLI_CMD() \
{ \
    K::Logger::Logg2(C_CLI,stream,FMT("argc: %d ") % argc); \
    for(int i = 0;i< argc;i++)                              \
        K::Logger::Logg2(C_CLI,stream,FMT("argv[%d]:%s ") % i % argv[i]); \
}

/* macros to cli commands */
#define ARG_CMP(a,b) (argv[a] && !strncasecmp(argv[a],b,sizeof(b))) 
#define EXEC_CLI_CMD(command) Cli::command.execute(argc,argv)

/* macro to string treats */
#define SAFE_sprintf(a, ...)    snprintf(a,sizeof(a), __VA_ARGS__)
#define SAFE_strcasecmp(a,b)    strncasecmp(a, b, sizeof(b))

/* tags for timers */
#define TM_VAL_CALL    (unsigned int)0x01
#define TM_VAL_CHANNEL (unsigned int)0x02

/* macro to creating contexts */
#define BEGIN_CONTEXT do
#define END_CONTEXT while(false);

/* Define log type */
typedef enum
{
    C_CLI,      /* cli msgs    */

    C_ERROR,    /* errors      */
    C_WARNING,  /* warnings    */
    C_MESSAGE,  /* normal msgs */

    C_EVENT,    /* k3l events   */
    C_COMMAND,  /* k3l commands */

    C_AUDIO_EV, /* k3l audio events */
    C_MODEM_EV, /* gsm modem events */
    C_LINK_STT, /* link status msgs */
    C_CAS_MSGS, /* cas events msgs  */

    C_DBG_FUNC,
    C_DBG_LOCK,
    C_DBG_THRD,
    C_DBG_STRM,
    
    C_DBG_CONF,
}
class_type;

typedef enum
{
    O_CONSOLE,
    O_GENERIC,
    O_R2TRACE,
}
output_type;

typedef enum
{
    SCE_SHOW_WARNING,
    SCE_SHOW_DEBUG,
    SCE_SHOW_SAME,
    SCE_HIDE
}
send_cmd_error_type;

typedef enum 
{
    T_UNKNOWN = 2,
    T_TRUE = 1,
    T_FALSE = 0,
}
TriState;
    

#endif
