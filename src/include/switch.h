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



#ifndef WIN32
#include <switch_am_config.h>
#endif

#define FREESWITCH_PEN "27880"
#define FREESWITCH_OID_PREFIX ".1.3.6.1.4.1." FREESWITCH_PEN
#define FREESWITCH_ITAD "543"
#define __EXTENSIONS__ 1
#ifndef MACOSX
#ifndef _XOPEN_SOURCE
#ifndef __cplusplus
#define _XOPEN_SOURCE 600
#endif
#endif
#ifndef __BSD_VISIBLE
#define __BSD_VISIBLE 1
#endif
#endif
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <assert.h>
#include <setjmp.h>
#include <ctype.h>
#include <fcntl.h>
#ifdef _MSC_VER
#include <Winsock2.h>
#if _MSC_VER < 1500
/* work around bug in msvc 2005 code analysis http://connect.microsoft.com/VisualStudio/feedback/ViewFeedback.aspx?FeedbackID=99397 */
#pragma warning(push)
#pragma warning(disable:6011)
#include <Ws2tcpip.h>
#pragma warning(pop)
#else
#include <Ws2tcpip.h>
#endif
#else
#include <strings.h>
#endif
#include <string.h>
#include <sys/stat.h>
#include <limits.h>
#include <sys/types.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

#include "switch_platform.h"
#include "switch_types.h"
#include "switch_apr.h"

#include "switch_core_db.h"
#include "switch_regex.h"
#include "switch_core.h"
#include "switch_loadable_module.h"
#include "switch_console.h"
#include "switch_utils.h"
#include "switch_caller.h"
#include "switch_frame.h"
#include "switch_module_interfaces.h"
#include "switch_channel.h"
#include "switch_buffer.h"
#include "switch_event.h"
#include "switch_resample.h"
#include "switch_ivr.h"
#include "switch_rtp.h"
#include "switch_log.h"
#include "switch_xml.h"
#include "switch_core_event_hook.h"
#include "switch_scheduler.h"
#include "switch_config.h"
#include <libteletone.h>

/** \mainpage FreeSWITCH
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application

 * \section intro Introduction
 *
 * \section supports Supported Platforms
 * Freeswitch has been built on the following platforms:
 *
 *  - Linux (x86 & x86_64)
 *  - Windows (MSVC 2005)
 *  - Mac OS X (intel & ppc )
 *  - OpenBSD, FreeBSD 6
 *
 * \section depends Dependencies
 *  Freeswitch makes heavy use of external libraries.  
 *
 *  libFreeSwitch:
 *		- APR (http://apr.apache.org)
 *		- APR-Util (http://apr.apache.org)
 *		- SQLite (http://www.sqlite.org)
 *		- libresample (http://ccrma-www.stanford.edu/~jos/resample/Free_Resampling_Software.html)
 *		- Pcre (http://www.pcre.org/)
 *		- SRTP (http://srtp.sourceforge.net/srtp.html)
 *
 *	Additionally, the experimental external modules make use of several external modules:
 *
 *
 *  ASR/TTS
 *	mod_cepstral
 *		- Cepstral (commercial) (http://www.cepstral.com/)
 *
 *  Codecs
 *	mod_speex
 *		- libspeex (http://www.speex.org/)
 *
 *  Directories
 *	mod_ldap
 *		- openldap (*nix only http://www.openldap.org/)
 * 
 *  Endpoints
 *	mod_iax
 *		- libiax2 (forked from http://iaxclient.sourceforge.net/)
 *
 *	mod_portaudio
 *		- portaudio (http://www.portaudio.com/)
 *
 *	mod_woomera
 *		- openh323/woomera (http://www.voxgratia.org/)
 *
 *	mod_dingaling
 *		- libdingaling (internal library distributed with freeswitch which depends on)
 *		- APR (http://apr.apache.org)
 *		- iksemel (http://iksemel.jabberstudio.org/)
 *
 *	mod_sofia
 *		- sofia-sip (http://opensource.nokia.com/projects/sofia-sip/)
 *
 *  Event Hanlders
 *	mod_xmpp_event
 *		- iksemel (http://iksemel.jabberstudio.org/)
 *
 *	mod_zeroconf
 *		- libhowl (No longer available http://www.porchdogsoft.com/products/howl/)
 *
 *	mod_cdr
 *		- Mysql (http://www.mysql.com/)
 *		- unixodbc (*nix only http://www.unixodbc.org/)
 *
 *  Formats
 *	mod_sndfile
 *		- libsndfile (http://www.mega-nerd.com/libsndfile/)
 *
 *  Languages
 *	mod_spidermonkey
 *		- spidermonkey (http://www.mozilla.org/js/spidermonkey/)
 *
 *	mod_perl
 *		- perl (http://www.perl.org/)
 *
 *  XML interfaces
 *	mod_xml_rpc
 *		- xmlrpc-c (http://xmlrpc-c.sourceforge.net/)
 *
 *
 * \section license Licensing
 *
 * Freeswitch is licensed under the terms of the MPL 1.1
 *
 */
#endif

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
