/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 * switch.h -- Main Library Header
 *
 */
/*! \file switch.h
    \brief Main Library Header
*/

#ifndef SWITCH_H
#define SWITCH_H

#ifdef __cplusplus
#define SWITCH_BEGIN_EXTERN_C       extern "C" {
#define SWITCH_END_EXTERN_C         }
#else
#define SWITCH_BEGIN_EXTERN_C
#define SWITCH_END_EXTERN_C
#endif


//Need to include this before any other includes (MSVC Bug)
#include <switch_platform.h>

#ifndef WIN32
#include <switch_am_config.h>
#endif

#include <assert.h>
#include <setjmp.h>
#include <switch_apr.h>
#include <switch_sqlite.h>
#include <switch_types.h>
#include <switch_core.h>
#include <switch_loadable_module.h>
#include <switch_console.h>
#include <switch_utils.h>
#include <switch_caller.h>
#include <switch_config.h>
#include <switch_frame.h>
#include <switch_module_interfaces.h>
#include <switch_channel.h>
#include <switch_buffer.h>
#include <switch_event.h>
#include <switch_resample.h>
#include <switch_ivr.h>
#include <switch_rtp.h>
#include <switch_stun.h>
#include <switch_log.h>
#include <switch_xml.h>
#include <pcre.h>


/** \mainpage FreeSWITCH
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application

 * \section intro Introduction
 *
 * \section supports Supported Platforms
 * Freeswitch has been built on the following platforms:
 *
 *  - Linux (x86)
 *  - Windows (MSVC 2005)
 *  - Mac OS X (intel & ppc )
 *  - OpenBSD, FreeBSD 6
 *
 * \section depends Dependencies
 *  Freeswitch makes heavy use of external libraries.  
 *
 *  libFreeSwitch:
 *		- APR (http://apr.apache.org)
 *		- SQLite (http://www.sqlite.org)
 *		- libresample (http://ccrma-www.stanford.edu/~jos/resample/Free_Resampling_Software.html)
 *
 *	Additionally, the experimental external modules make use of several external modules:
 *
 *	mod_exosip:
 *		- eXoSIP (http://savannah.nongnu.org/projects/exosip/)
 *
 *	mod_iax:
 *		- libiax2 (forked from http://iaxclient.sourceforge.net/)
 *
 *	mod_speex
 *		- libspeex (http://www.speex.org/)
 *
 *	mod_portaudio
 *		- portaudio (http://www.portaudio.com/)
 *
 *	mod_woomera
 *		- openh323/woomera (http://www.voxgratia.org/)
 *
 *	mod_xmpp_event
 *		- iksemel (http://iksemel.jabberstudio.org/)
 *
 *	mod_sndfile
 *		- libsndfile (http://www.mega-nerd.com/libsndfile/)
 *
 * \section license Licensing
 *
 * Freeswitch is licensed under the terms of the MPL 1.1
 *
 */
#endif
