/*
 * Copyright (c) 2009, Sangoma Technologies
 * Moises Silva <moy@sangoma.com>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "switch.h"
#include "switch_profile.h"

#ifdef __linux__
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#endif

struct profile_timer 
{
	/* bool, just used to retrieve the values for the first time and not calculate the percentage of idle time */
	int valid_last_times;

	/* last calculated percentage of idle time */
	double last_percentage_of_idle_time;

#ifdef __linux__
	/* all of these are the Linux jiffies last retrieved count */
	unsigned long long last_user_time;
	unsigned long long last_system_time;
	unsigned long long last_idle_time;

	unsigned long long last_nice_time;
	unsigned long long last_irq_time;
	unsigned long long last_soft_irq_time;
	unsigned long long last_io_wait_time;
	unsigned long long last_steal_time;

	/* /proc/stat file descriptor used to retrieve the counters */
	int procfd;
	int initd;
#elif defined (WIN32)  || defined (WIN64)
	__int64 i64LastUserTime;
	__int64 i64LastKernelTime;
	__int64 i64LastIdleTime;
#else
  /* Unsupported */
#endif
};

#ifdef __linux__
static int read_cpu_stats(switch_profile_timer_t *p, 
		      unsigned long long *user, 
                      unsigned long long *nice, 
                      unsigned long long *system, 
                      unsigned long long *idle, 
                      unsigned long long *iowait, 
                      unsigned long long *irq, 
                      unsigned long long *softirq, 
                      unsigned long long *steal)
{
// the output of proc should not change that often from one kernel to other
// see fs/proc/proc_misc.c or fs/proc/stat.c in the Linux kernel for more details
// also man 5 proc is useful
#define CPU_ELEMENTS 8 // change this if you change the format string
#define CPU_INFO_FORMAT "cpu  %llu %llu %llu %llu %llu %llu %llu %llu"
	static const char procfile[] = "/proc/stat";
	int rc = 0;
	int myerrno = 0;
	int elements = 0;
	const char *cpustr = NULL;
	char statbuff[1024];

	if (!p->initd) {
		p->procfd = open(procfile, O_RDONLY, 0);
		if(p->procfd == -1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to open CPU statistics file %s: %s\n", procfile, strerror(myerrno));
			return -1;
		}
		p->initd = 1;
	} else {
		lseek(p->procfd, 0L, SEEK_SET);
	}

	rc = read(p->procfd, statbuff, sizeof(statbuff) - 1);
	if (rc <= 0) {
		myerrno = errno;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to read CPU statistics file %s: %s\n", procfile, strerror(myerrno));
		return -1;
	}

	cpustr = strstr(statbuff, "cpu ");
	if (!cpustr) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "wrong format for Linux proc cpu statistics: missing cpu string\n");
		return -1;
	}

	elements = sscanf(cpustr, CPU_INFO_FORMAT, user, nice, system, idle, iowait, irq, softirq, steal);
	if (elements != CPU_ELEMENTS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "wrong format for Linux proc cpu statistics: expected %d elements, but just found %d\n", CPU_ELEMENTS, elements);
		return -1;
	}
	return 0;
}

SWITCH_DECLARE(int) switch_get_system_idle_time(switch_profile_timer_t *p, double *idle_percentage)
{
	unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
	unsigned long long usertime, kerneltime, idletime, totaltime, halftime;

	if (read_cpu_stats(p, &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to retrieve Linux CPU statistics\n");
		return -1;
	}

	if (!p->valid_last_times) {
		// we dont strictly need to save all of them but I feel code is more clear if we do
		p->valid_last_times = 1;
		p->last_user_time = user;
		p->last_nice_time = nice;
		p->last_system_time = system;
		p->last_irq_time = irq;
		p->last_soft_irq_time = softirq;
		p->last_io_wait_time = iowait;
		p->last_steal_time = steal;
		p->last_idle_time = idle;
		p->last_percentage_of_idle_time = 100.0;
		*idle_percentage = p->last_percentage_of_idle_time;
		return 0;
	}

	usertime = (user - p->last_user_time) + (nice - p->last_nice_time);
	kerneltime = (system - p->last_system_time) + (irq - p->last_irq_time) + (softirq - p->last_soft_irq_time);
	kerneltime += (iowait - p->last_io_wait_time);
	kerneltime += (steal - p->last_steal_time);
	idletime = (idle - p->last_idle_time);

	totaltime = usertime + kerneltime + idletime;

	if (totaltime <= 0) {
		// this may happen if not enough time has elapsed and the jiffies counters are the same than the last time we checked
		// jiffies depend on timer interrupts which depend on the number of HZ compile time setting of the kernel
		// typical configs set HZ to 100 (that means, 100 jiffies updates per second, that is one each 10ms)
		// avoid an arithmetic exception and return the same values
		*idle_percentage = p->last_percentage_of_idle_time;
		return 0;
	}

	halftime = totaltime / 2UL;

	p->last_percentage_of_idle_time = ((100 * idletime + halftime) / totaltime);
	*idle_percentage = p->last_percentage_of_idle_time;

	p->last_user_time = user;
	p->last_nice_time = nice;
	p->last_system_time = system;
	p->last_irq_time = irq;
	p->last_soft_irq_time = softirq;
	p->last_io_wait_time = iowait;
	p->last_steal_time = steal;
	p->last_idle_time = idle;

	return 0;
}

#elif defined (WIN32) || defined (WIN64)

SWITCH_DECLARE(int) switch_get_system_idle_time(switch_profile_timer_t *p, double *idle_percentage)
{
	FILETIME idleTime;
	FILETIME kernelTime;
	FILETIME userTime;
	__int64 i64UserTime, i64KernelTime, i64IdleTime;
  
	if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
		return SWITCH_FALSE;
	}
  
	i64UserTime = (__int64)userTime.dwLowDateTime | ((__int64)userTime.dwHighDateTime << 32);

	i64KernelTime = (__int64)kernelTime.dwLowDateTime | ((__int64)kernelTime.dwHighDateTime << 32);

	i64IdleTime = (__int64)idleTime.dwLowDateTime | ((__int64)idleTime.dwHighDateTime << 32);

	if (p->valid_last_times) {
		__int64 i64User = i64UserTime - p->i64LastUserTime;
		__int64 i64Kernel = i64KernelTime - p->i64LastKernelTime;
		__int64 i64Idle = i64IdleTime - p->i64LastIdleTime;
		__int64 i64System = i64User + i64Kernel;
		*idle_percentage = 100.0 * i64Idle / i64System;
	} else {
		*idle_percentage = 100.0;
		p->valid_last_times = 1;
	}

	/* Remember current value for the next call */
	p->i64LastUserTime = i64UserTime;
	p->i64LastKernelTime = i64KernelTime;
	p->i64LastIdleTime = i64IdleTime;

	/* Success */
	return 0;
}

#else

  /* Unsupported */
SWITCH_DECLARE(int) switch_get_system_idle_time(switch_profile_timer_t *p, double *idle_percentage)
{
	return SWITCH_FALSE;
}

#endif

SWITCH_DECLARE(switch_profile_timer_t *)switch_new_profile_timer(void)
{
	return calloc(1, sizeof(switch_profile_timer_t));
}

SWITCH_DECLARE(void) switch_delete_profile_timer(switch_profile_timer_t **p)
{
	if (!p) return;

#ifdef __linux__
	close((*p)->procfd);
#endif
	free(*p);
	*p = NULL;
}


