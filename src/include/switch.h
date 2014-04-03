/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
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

#define SWITCH_VIDEO_IN_THREADS

#ifndef WIN32
#include <switch_am_config.h>
#endif

#define FREESWITCH_PEN "27880"
#define FREESWITCH_OID_PREFIX ".1.3.6.1.4.1." FREESWITCH_PEN
#define FREESWITCH_ITAD "543"
#define __EXTENSIONS__ 1
#ifndef MACOSX
#if !defined(_XOPEN_SOURCE) && !defined(__OpenBSD__) && !defined(__NetBSD__)
#ifndef __cplusplus
#define _XOPEN_SOURCE 600
#endif
#endif
#ifdef __linux__
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif
#endif
#ifndef __BSD_VISIBLE
#define __BSD_VISIBLE 1
#endif
#ifdef __linux__
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
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
#include <string.h>
#ifdef _MSC_VER
#include <Winsock2.h>
#if _MSC_VER < 1500
/* work around bug in msvc 2005 code analysis http://connect.microsoft.com/VisualStudio/feedback/ViewFeedback.aspx?FeedbackID=99397 */
#pragma warning(push)
#pragma warning(disable:6011)
#include <Ws2tcpip.h>
#pragma warning(pop)
#else
/* work around for warnings in vs 2010 */
#pragma warning (disable:6386)
#include <Ws2tcpip.h>
#pragma warning (default:6386)
#endif
#else
#include <strings.h>
#endif
#include <sys/stat.h>
#include <limits.h>
#include <sys/types.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

#include "switch_platform.h"
#include "switch_types.h"
#include "switch_stfu.h"
#include "switch_apr.h"
#include "switch_mprintf.h"
#include "switch_core_db.h"
#include "switch_dso.h"
#include "switch_regex.h"
#include "switch_core.h"
#include "switch_loadable_module.h"
#include "switch_console.h"
#include "switch_utils.h"
#include "switch_caller.h"
#include "switch_frame.h"
#include "switch_rtcp_frame.h"
#include "switch_module_interfaces.h"
#include "switch_channel.h"
#include "switch_buffer.h"
#include "switch_event.h"
#include "switch_resample.h"
#include "switch_ivr.h"
#include "switch_rtp.h"
#include "switch_log.h"
#include "switch_xml.h"
#include "switch_xml_config.h"
#include "switch_core_event_hook.h"
#include "switch_scheduler.h"
#include "switch_config.h"
#include "switch_nat.h"
#include "switch_odbc.h"
#include "switch_pgsql.h"
#include "switch_json.h"
#include "switch_limit.h"
#include "switch_core_media.h"
#include <libteletone.h>


/** \mainpage FreeSWITCH
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application

 * \section intro Introduction
 *
 * \section supports Supported Platforms
 * Freeswitch has been built on the following platforms:
 *
 *  - Linux (x86 & x86_64)
 *  - Windows (MSVC 2012/2013 & VC++ EE 2012/2013)
 *  - Mac OS X 10.7/10.8/10.9 (x86_64 Intel)
 *  - FreeBSD 8/9/10
 *  - NetBSD 6.x
 *  - OpenBSD 5.x
 *
 * \section depends Dependencies
 *  Freeswitch makes heavy use of external libraries.  
 *
 *  libFreeSWITCH:
 *		- APR (http://apr.apache.org)
 *		- APR-Util (http://apr.apache.org)
 *		- SQLite (http://www.sqlite.org)
 *		- Pcre (http://www.pcre.org/)
 *		- SRTP (http://srtp.sourceforge.net/srtp.html)
 *
 *	Additionally, the various external modules make use of several external modules:
 *
 *
 *  ASR/TTS
 *	mod_cepstral
 *		- Cepstral (commercial) (http://www.cepstral.com/)
 *
 *	mod_flite
 *		- Flite (http://www.speech.cs.cmu.edu/flite/)
 *
 *	mod_pocketsphinx
 *		- PocketSphinx (http://www.speech.cs.cmu.edu/pocketsphinx/)
 *
 *	mod_unimrcp
 *		- MRCP (http://www.unimrcp.org/)
 *
 *
 *  Codecs
 *  mod_amr
 *      - Passthru codec for amr narrowband (8kHz)
 *
 *  mod_amrwb
 *      - Passthru codec for amr wideband (16kHz)
 *
 *  mod_b64
 *      - Base64 codec tranfers data base64 encoded (http://www.b64codec.org)
 * 
 *  mod_bv
 *      - BroadVoice16 (8kHz) and BroadVoice32 (16kHz) (https://www.broadcom.com/support/broadvoice)
 *  mod_celt
 *      - Ultra-low delay audio codec (48kHz) (http://celt-codec.org)
 *
 *  mod_codec2
 *      - Codec2 is an open source low bit rate speech at 2400 bit/s and below. (http://www.rowetel.com/blog/?page_id=452)
 *
 *	mod_speex
 *		- libspeex (http://www.speex.org/)
 *
 *	mod_celt
 *		- libcelt (http://www.celt-codec.org/)
 *
 *	mod_siren
 *		- libg722_1 (http://www.polycom.com/company/about_us/technology/siren22/index.html)
 *
 * Digital Signal Processing
 *	mod_spandsp
 *		- codec, fax and modem (http://www.soft-switch.org/)
 *
 *  Directories
 *	mod_ldap
 *		- openldap (*nix only http://www.openldap.org/)
 * 
 *  Endpoints
 *	mod_portaudio
 *		- portaudio (http://www.portaudio.com/)
 *
 *	mod_dingaling
 *		- libdingaling (internal library distributed with freeswitch which depends on)
 *		- APR (http://apr.apache.org)
 *		- iksemel (http://iksemel.jabberstudio.org/)
 *
 *	mod_sofia
 *		- sofia-sip (http://opensource.nokia.com/projects/sofia-sip/)
 *
 *	mod_opal
 *		- libopal (http://www.opalvoip.org)
 *
 *	mod_freetdm
 *		- freetdm (http://wiki.freeswitch.org/wiki/FreeTDM)
 *
 *  Event Hanlders
 *	mod_xmpp_event
 *		- iksemel (http://iksemel.jabberstudio.org/)
 *
 *  Formats
 *	mod_sndfile
 *		- libsndfile (http://www.mega-nerd.com/libsndfile/)
 *
 *  Languages
 *	mod_perl
 *		- perl (http://www.perl.org/)
 *
 *	mod_lua
 *		- lua (http://www.lua.org)
 *
 *  XML interfaces
 *	mod_xml_rpc
 *		- xmlrpc-c (http://xmlrpc-c.sourceforge.net/)
 *
 *	mod_xml_curl
 *		- libcurl (http://curl.haxx.se/)
 *
 *  Network services
 *	mod_http
 *		- Abyss (http://www.aprelium.com/)
 *
 *	mod_enum
 *		- udns (http://www.corpit.ru/mjt/udns.html)
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
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
