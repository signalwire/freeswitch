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
 * Rupa Schomaker <rupa@rupa.com>
 *
 * switch_limit.h - Limit generic implementations
 *
 */

 
 /*!
  \defgroup limit1 LIMIT code
  \ingroup core1
  \{
*/
#ifndef _SWITCH_LIMIT_H
#define _SWITCH_LIMIT_H

SWITCH_BEGIN_EXTERN_C

/*! 
  \brief Initilize the LIMIT Core System
  \param pool the memory pool to use for long term allocations
  \note Generally called by the core_init
*/
SWITCH_DECLARE(void) switch_limit_init(switch_memory_pool_t *pool);

/*!
  \brief Increment resource.  
  \param backend to use
  \param realm
  \param resource
  \param max - 0 means no limit, just count
  \param interval - 0 means no interval
  \return true/false - true ok, false over limit
*/
SWITCH_DECLARE(switch_status_t) switch_limit_incr(const char *backend, switch_core_session_t *session, const char *realm, const char *resource, const int max, const int interval);

/*!
  \brief Release resource.  
  \param backend to use
  \param realm
  \param resource
  \return true/false - true ok, false over limit
*/
SWITCH_DECLARE(switch_status_t) switch_limit_release(const char *backend, switch_core_session_t *session, const char *realm, const char *resource);

/*!
  \brief get usage count for resource
  \param backend to use
  \param realm
  \param resource
  \param rcount - output paramter, rate counter
*/
SWITCH_DECLARE(int) switch_limit_usage(const char *backend, const char *realm, const char *resource, uint32_t *rcount);

/*!
  \brief reset interval usage counter for a given resource
  \param backend
  \param realm
  \param resource
*/
SWITCH_DECLARE(switch_status_t) switch_limit_interval_reset(const char *backend, const char *realm, const char *resource);

/*!
  \brief reset all usage counters
  \param backend to use
*/
SWITCH_DECLARE(switch_status_t) switch_limit_reset(const char *backend);

/*!
  \brief fire event for limit usage
  \param backend to use
  \param realm
  \param resource
  \param usage
  \param rate
  \param max
  \param ratemax
*/
SWITCH_DECLARE(void) switch_limit_fire_event(const char *backend, const char *realm, const char *resource, uint32_t usage, uint32_t rate, uint32_t max, uint32_t ratemax);

/*!
  \brief retrieve arbitrary status information
  \param backend to use
  \note caller must free returned value
*/
SWITCH_DECLARE(char *) switch_limit_status(const char *backend);

/*! callback to init a backend */
#define SWITCH_LIMIT_INCR(name) static switch_status_t name (switch_core_session_t *session, const char *realm, const char *resource, const int max, const int interval)
#define SWITCH_LIMIT_RELEASE(name) static switch_status_t name (switch_core_session_t *session, const char *realm, const char *resource)
#define SWITCH_LIMIT_USAGE(name) static int name (const char *realm, const char *resource, uint32_t *rcount)
#define SWITCH_LIMIT_RESET(name) static switch_status_t name (void)
#define SWITCH_LIMIT_INTERVAL_RESET(name) static switch_status_t name (const char *realm, const char *resource)
#define SWITCH_LIMIT_STATUS(name) static char * name (void)

#define LIMIT_IGNORE_TRANSFER_VARIABLE "limit_ignore_transfer"
#define LIMIT_BACKEND_VARIABLE "limit_backend"
#define LIMIT_EVENT_USAGE "limit::usage"
#define LIMIT_DEF_XFER_EXTEN "limit_exceeded"

SWITCH_END_EXTERN_C
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
