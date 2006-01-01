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
extern "C" {
#endif

#ifndef WIN32
#include <config.h>
#endif

#include <assert.h>

#include <switch_platform.h>
#include <switch_apr.h>
#include <switch_sqlite.h>
#include <switch_types.h>
#include <switch_core.h>
#include <switch_loadable_module.h>
#include <switch_console.h>
#include <switch_utils.h>
#include <switch_caller.h>
#include <switch_mutex.h>
#include <switch_config.h>
#include <switch_frame.h>
#include <switch_module_interfaces.h>
#include <switch_channel.h>
#include <switch_buffer.h>
#include <switch_event.h>

#ifdef __cplusplus
}
#endif

#endif
