/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2011, Anthony Minessale II <anthm@freeswitch.org>
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
 * timerfd_wrap.h -- timerfd syscall wrapper
 *
 */
/*! \file timerfd_wrap.h
    \brief timerfd syscall wrapper
*/

#ifndef TIMERFD_WRAP_H
#define TIMERFD_WRAP_H
SWITCH_BEGIN_EXTERN_C

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>


#ifndef __NR_timerfd
#if defined(__x86_64__)
#define __NR_timerfd_create 283
#define __NR_timerfd_settime 286
#define __NR_timerfd_gettime 287
#elif defined(__i386__)
#define __NR_timerfd_create 322
#define __NR_timerfd_settime 325
#define __NR_timerfd_gettime 326
#else
#error invalid system
#endif
#endif

#define TFD_TIMER_ABSTIME (1 << 0)

int timerfd_create(int clockid, int flags) 
{

	return syscall(__NR_timerfd_create, clockid, flags);
}

int timerfd_settime(int ufc, int flags, const struct itimerspec *utmr, struct itimerspec *otmr) 
{

	return syscall(__NR_timerfd_settime, ufc, flags, utmr, otmr);
}

int timerfd_gettime(int ufc, struct itimerspec *otmr) 
{

	return syscall(__NR_timerfd_gettime, ufc, otmr);
}

SWITCH_END_EXTERN_C

#endif
