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
 *
 * Contributors:
 * David Yat Sin <dyatsin@sangoma.com>
 * 
 */

#ifdef WIN32
#define _WIN32_WINNT 0x0501 // To make GetSystemTimes visible in windows.h
#include <windows.h>
#else /* LINUX */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#endif

#include "openzap.h"
#include "zap_cpu_monitor.h"
struct zap_cpu_monitor_stats
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
static zap_status_t zap_cpu_read_stats(struct zap_cpu_monitor_stats *p,
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
#define CPU_INFO_FORMAT "cpu  %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu"
	static const char procfile[] = "/proc/stat";
	int rc = 0;
	int myerrno = 0;
	int elements = 0;
	const char *cpustr = NULL;
	char statbuff[1024];

	if (!p->initd) {
		p->procfd = open(procfile, O_RDONLY, 0);
		if(p->procfd == -1) {
			zap_log(ZAP_LOG_ERROR, "Failed to open CPU statistics file %s: %s\n", procfile, strerror(myerrno));
			return ZAP_FAIL;
		}
		p->initd = 1;
	} else {
		lseek(p->procfd, 0L, SEEK_SET);
	}

	rc = read(p->procfd, statbuff, sizeof(statbuff) - 1);
	if (rc <= 0) {
		myerrno = errno;
		zap_log(ZAP_LOG_ERROR, "Failed to read CPU statistics file %s: %s\n", procfile, strerror(myerrno));
		return ZAP_FAIL;
	}

	cpustr = strstr(statbuff, "cpu ");
	if (!cpustr) {
		zap_log(ZAP_LOG_ERROR, "wrong format for Linux proc cpu statistics: missing cpu string\n");
		return ZAP_FAIL;
	}

	elements = sscanf(cpustr, CPU_INFO_FORMAT, user, nice, system, idle, iowait, irq, softirq, steal);
	if (elements != CPU_ELEMENTS) {
		zap_log(ZAP_LOG_ERROR, "wrong format for Linux proc cpu statistics: expected %d elements, but just found %d\n", CPU_ELEMENTS, elements);
		return ZAP_FAIL;
	}
	return ZAP_SUCCESS;
}
#endif

#ifdef __linux__
OZ_DECLARE(zap_status_t) zap_cpu_get_system_idle_time (struct zap_cpu_monitor_stats *p, double *idle_percentage)
{
	unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
	unsigned long long usertime, kerneltime, idletime, totaltime, halftime;

	if (zap_cpu_read_stats(p, &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal)) {
		zap_log(ZAP_LOG_ERROR, "Failed to retrieve Linux CPU statistics\n");
		return ZAP_FAIL;
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
		return ZAP_SUCCESS;
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
		return ZAP_SUCCESS;
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

	return ZAP_SUCCESS;
}

#elif defined (WIN32) || defined (WIN64)
OZ_DECLARE(zap_status_t) zap_cpu_get_system_idle_time(struct zap_cpu_monitor_stats *p, double *idle_percentage)
{
	FILETIME idleTime;
	FILETIME kernelTime;
	FILETIME userTime;
  
	if (!::GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
		return false;
	}
  
	__int64 i64UserTime = (__int64)userTime.dwLowDateTime | ((__int64)userTime.dwHighDateTime << 32);

	__int64 i64KernelTime = (__int64)kernelTime.dwLowDateTime | ((__int64)kernelTime.dwHighDateTime << 32);

	__int64 i64IdleTime = (__int64)idleTime.dwLowDateTime | ((__int64)idleTime.dwHighDateTime << 32);

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
	return ZAP_SUCCESS;
}
#else
/* Unsupported */
OZ_DECLARE(zap_status_t) zap_cpu_get_system_idle_time(struct zap_cpu_monitor_stats *p, double *idle_percentage)
{
	return ZAP_FAIL;
}
#endif

OZ_DECLARE(struct zap_cpu_monitor_stats*) zap_new_cpu_monitor(void)
{
	return calloc(1, sizeof(struct zap_cpu_monitor_stats));
}

OZ_DECLARE(void) zap_delete_cpu_monitor(struct zap_cpu_monitor_stats *p)
{
#ifdef __linux__
	close(p->procfd);
#endif
	free(p);
}


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
