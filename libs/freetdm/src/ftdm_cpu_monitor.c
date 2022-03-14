/*
 * Copyright (c) 2010, Sangoma Technologies
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
#   if (_WIN32_WINNT < 0x0501)
#       error "Need to target at least Windows XP/Server 2003 because GetSystemTimes is needed"
#   endif
#   include <windows.h>
#else /* LINUX */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#endif

#include "private/ftdm_core.h"
#include "ftdm_cpu_monitor.h"
struct ftdm_cpu_monitor_stats
{
	/* bool, just used to retrieve the values for the first time and not calculate the percentage of idle time */
	int valid_last_times;

	/* last calculated percentage of idle time */
	double last_percentage_of_idle_time;

#ifdef __linux__
	/* the cpu feature gets disabled on errors */
	int disabled;

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
static ftdm_status_t ftdm_cpu_read_stats(struct ftdm_cpu_monitor_stats *p,
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
// also man 5 proc is useful.
#define CPU_ELEMENTS_1 7 // change this if you change the format string
#define CPU_INFO_FORMAT_1 "cpu  %llu %llu %llu %llu %llu %llu %llu"

#define CPU_ELEMENTS_2 8 // change this if you change the format string
#define CPU_INFO_FORMAT_2 "cpu  %llu %llu %llu %llu %llu %llu %llu %llu"

#define CPU_ELEMENTS_3 9 // change this if you change the format string
#define CPU_INFO_FORMAT_3 "cpu  %llu %llu %llu %llu %llu %llu %llu %llu %llu"

	static const char procfile[] = "/proc/stat";
	int rc = 0;
	int myerrno = 0;
	int elements = 0;
	const char *cpustr = NULL;
	char statbuff[1024];
	unsigned long long guest = 0;

	if (!p->initd) {
		p->procfd = open(procfile, O_RDONLY, 0);
		if(p->procfd == -1) {
			ftdm_log(FTDM_LOG_ERROR, "Failed to open CPU statistics file %s: %s\n", procfile, strerror(myerrno));
			return FTDM_FAIL;
		}
		p->initd = 1;
	} else {
		lseek(p->procfd, 0L, SEEK_SET);
	}

	rc = read(p->procfd, statbuff, sizeof(statbuff) - 1);
	if (rc <= 0) {
		myerrno = errno;
		ftdm_log(FTDM_LOG_ERROR, "Failed to read CPU statistics file %s: %s\n", procfile, strerror(myerrno));
		return FTDM_FAIL;
	}

	cpustr = strstr(statbuff, "cpu ");
	if (!cpustr) {
		ftdm_log(FTDM_LOG_ERROR, "wrong format for Linux proc cpu statistics: missing cpu string\n");
		return FTDM_FAIL;
	}

	/* test each of the known formats starting from the bigger one */
	elements = sscanf(cpustr, CPU_INFO_FORMAT_3, user, nice, system, idle, iowait, irq, softirq, steal, &guest);
	if (elements == CPU_ELEMENTS_3) {
		user += guest; /* guest operating system's run in user space */
		return FTDM_SUCCESS;
	}

	elements = sscanf(cpustr, CPU_INFO_FORMAT_2, user, nice, system, idle, iowait, irq, softirq, steal);
	if (elements == CPU_ELEMENTS_2) {
		return FTDM_SUCCESS;
	}

	elements = sscanf(cpustr, CPU_INFO_FORMAT_1, user, nice, system, idle, iowait, irq, softirq);
	if (elements == CPU_ELEMENTS_1) {
		*steal = 0;
		return FTDM_SUCCESS;
	}

	ftdm_log(FTDM_LOG_ERROR, "Unexpected format for Linux proc cpu statistics:%s\n", cpustr);
	return FTDM_FAIL;
}
#endif

#ifdef __linux__
FT_DECLARE(ftdm_status_t) ftdm_cpu_get_system_idle_time (struct ftdm_cpu_monitor_stats *p, double *idle_percentage)
{
	unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
	unsigned long long usertime, kerneltime, idletime, totaltime, halftime;

	*idle_percentage = 100.0;
	if (p->disabled) {
		return FTDM_FAIL;
	}
	if (ftdm_cpu_read_stats(p, &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal)) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to retrieve Linux CPU statistics - disabling cpu monitor\n");
		p->disabled = 1;
		return FTDM_FAIL;
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
		return FTDM_SUCCESS;
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
		return FTDM_SUCCESS;
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

	return FTDM_SUCCESS;
}

#elif defined (__WINDOWS__)
FT_DECLARE(ftdm_status_t) ftdm_cpu_get_system_idle_time(struct ftdm_cpu_monitor_stats *p, double *idle_percentage)
{
	FILETIME idleTime;
	FILETIME kernelTime;
	FILETIME userTime;
	int64_t i64UserTime;
	int64_t i64KernelTime;
	int64_t i64IdleTime;
  
	if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
		return FTDM_FAIL;
	}
  
	i64UserTime = (int64_t)userTime.dwLowDateTime | ((int64_t)userTime.dwHighDateTime << 32);

	i64KernelTime = (int64_t)kernelTime.dwLowDateTime | ((int64_t)kernelTime.dwHighDateTime << 32);

	i64IdleTime = (int64_t)idleTime.dwLowDateTime | ((int64_t)idleTime.dwHighDateTime << 32);

	if (p->valid_last_times) {
		int64_t i64User = i64UserTime - p->i64LastUserTime;
		int64_t i64Kernel = i64KernelTime - p->i64LastKernelTime;
		int64_t i64Idle = i64IdleTime - p->i64LastIdleTime;
		int64_t i64System = i64User + i64Kernel;
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
	return FTDM_SUCCESS;
}
#else
/* Unsupported */
FT_DECLARE(ftdm_status_t) ftdm_cpu_get_system_idle_time(struct ftdm_cpu_monitor_stats *p, double *idle_percentage)
{
	*idle_percentage = 100.0;
	return FTDM_FAIL;
}
#endif

FT_DECLARE(struct ftdm_cpu_monitor_stats*) ftdm_new_cpu_monitor(void)
{
	return calloc(1, sizeof(struct ftdm_cpu_monitor_stats));
}

FT_DECLARE(void) ftdm_delete_cpu_monitor(struct ftdm_cpu_monitor_stats *p)
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
