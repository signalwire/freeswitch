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
#include "private/switch_core_pvt.h"

#ifdef __linux__
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/sysinfo.h>//UC
#endif

struct profile_timer
{
	/* bool, just used to retrieve the values for the first time and not calculate the percentage of idle time */
	int valid_last_times;

	/* last calculated percentage of idle time */
	double last_percentage_of_idle_time;
	double *percentage_of_idle_time_ring;
	unsigned int last_idle_time_index;
	unsigned int cpu_idle_smoothing_depth;

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
	int procfd_mem;//UC
	int initd_mem;//UC
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
	} else {
	  statbuff[rc] = '\0';
	}

	cpustr = strstr(statbuff, "cpu ");
	if (!cpustr) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "wrong format for Linux proc cpu statistics: missing cpu string\n");
		return -1;
	}

	/* test each of the known formats starting from the bigger one */
	elements = sscanf(cpustr, CPU_INFO_FORMAT_3, user, nice, system, idle, iowait, irq, softirq, steal, &guest);
	if (elements == CPU_ELEMENTS_3) {
		*user += guest; /* guest operating system's run in user space */
		return 0;
	}

	elements = sscanf(cpustr, CPU_INFO_FORMAT_2, user, nice, system, idle, iowait, irq, softirq, steal);
	if (elements == CPU_ELEMENTS_2) {
		return 0;
	}

	elements = sscanf(cpustr, CPU_INFO_FORMAT_1, user, nice, system, idle, iowait, irq, softirq);
	if (elements == CPU_ELEMENTS_1) {
		*steal = 0;
		return 0;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Unexpected format for Linux proc cpu statistics: %s\n", cpustr);
	return -1;
}

SWITCH_DECLARE(switch_bool_t) switch_get_system_idle_time(switch_profile_timer_t *p, double *idle_percentage)
{
	unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
	unsigned long long usertime, kerneltime, idletime, totaltime, halftime;
	int x;

	*idle_percentage = 100.0;
	if (p->disabled) {
		return SWITCH_FALSE;
	}

	if (read_cpu_stats(p, &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to retrieve Linux CPU statistics, disabling profile timer ...\n");
		p->disabled = 1;
		return SWITCH_FALSE;
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
		return SWITCH_TRUE;
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
		return SWITCH_TRUE;
	}

	halftime = totaltime / 2UL;

	p->last_idle_time_index += 1;
	if ( p->last_idle_time_index >= p->cpu_idle_smoothing_depth ) {
	  p->last_idle_time_index = 0;
	}
	p->percentage_of_idle_time_ring[p->last_idle_time_index] = ((100 * idletime + halftime) / totaltime);

	p->last_percentage_of_idle_time = 0;
	for ( x = 0; x < p->cpu_idle_smoothing_depth; x++ ) {
	  //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "IDLE TIME: (%d)[%lf]\n", x, p->percentage_of_idle_time_ring[x]);
	  p->last_percentage_of_idle_time += p->percentage_of_idle_time_ring[x];
	}
	p->last_percentage_of_idle_time /= p->cpu_idle_smoothing_depth;

	*idle_percentage = p->last_percentage_of_idle_time;
	//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "IDLE TIME finalized:   [%lf]\n", *idle_percentage);

	p->last_user_time = user;
	p->last_nice_time = nice;
	p->last_system_time = system;
	p->last_irq_time = irq;
	p->last_soft_irq_time = softirq;
	p->last_io_wait_time = iowait;
	p->last_steal_time = steal;
	p->last_idle_time = idle;

	return SWITCH_TRUE;
}


//UC
SWITCH_DECLARE(switch_bool_t) switch_get_mem_info(switch_profile_timer_t *p, unsigned long long *mem_total, unsigned long long *mem_free)
{
#define MEM_INFO_FORMAT_1 "Cached: %llu %*s\n"
#define MEM_ELEMENTS_1 1
	static const char procfile[] = "/proc/meminfo";
	int rc = 0;
	int myerrno = 0;
	int elements = 0;
	const char *memstr = NULL;
	char statbuff[1024];

	struct sysinfo info;

	*mem_total = 0;
	*mem_free = 0;
	
	if (!p->initd_mem) {
		p->procfd_mem = open(procfile, O_RDONLY, 0);
		if(p->procfd_mem == -1) {
			myerrno = errno;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to open MEM statistics file %s: %s\n", procfile, strerror(myerrno));
			return SWITCH_FALSE;
		}
		p->initd_mem = 1;
	} else {
		lseek(p->procfd_mem, 0L, SEEK_SET);
	}

	rc = read(p->procfd_mem, statbuff, sizeof(statbuff) - 1);
	if (rc <= 0) {
		myerrno = errno;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to read MEM statistics file %s: %s\n", procfile, strerror(myerrno));
		return SWITCH_FALSE;
	} else {
	  statbuff[rc] = '\0';
	}

	memstr = strstr(statbuff, "Cached: ");
	if (!memstr) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "wrong format for Linux proc cpu statistics: missing cpu string\n");
		return SWITCH_FALSE;
	}

	elements = sscanf(memstr, MEM_INFO_FORMAT_1, mem_free);
	if (elements == MEM_ELEMENTS_1) {

		sysinfo(&info);
		
		*mem_free += info.freeram/1024;
		*mem_free += info.bufferram/1024;
		*mem_total = info.totalram/1024;

		return SWITCH_TRUE;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Unexpected format for Linux proc cpu statistics: %s\n", memstr);

	return SWITCH_FALSE;
		
}
//UC
SWITCH_DECLARE(switch_bool_t) switch_get_df_info(switch_profile_timer_t *p, unsigned long long *flashsize, unsigned long long *flashuse)
{
	FILE *fp;
	int myerrno = 0;
	unsigned long long fstotal = 0;
	
	char dfbuff[1024];
	char devbuff[128];
	*flashsize = 0;
	*flashuse = 0;

	system("df /shdisk > /tmp/df");
	fp = fopen("/tmp/df","r");
	if(fp == NULL){
		myerrno = errno;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to open df statistics file %s: %s\n", "/tmp/df", strerror(myerrno));
		return SWITCH_FALSE;
	}
	while (fgets(dfbuff, sizeof(dfbuff), fp) != NULL) {
		
		if (sscanf(dfbuff, "%s %llu %llu %llu %*s\n", devbuff,&fstotal, flashuse, flashsize) == 4){
			*flashsize += *flashuse;
				break;
		}
	}
	fclose(fp);
	return SWITCH_TRUE;

}


#elif defined (WIN32) || defined (WIN64)

SWITCH_DECLARE(switch_bool_t) switch_get_system_idle_time(switch_profile_timer_t *p, double *idle_percentage)
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
		 unsigned int x;

		p->last_idle_time_index += 1;
		if ( p->last_idle_time_index >= p->cpu_idle_smoothing_depth ) {
		  p->last_idle_time_index = 0;
		}
		p->percentage_of_idle_time_ring[p->last_idle_time_index] = 100.0 * i64Idle / i64System;

		*idle_percentage = 0;
		for (x = 0; x < p->cpu_idle_smoothing_depth; x++ ) {
		  *idle_percentage += p->percentage_of_idle_time_ring[x];
		}
		*idle_percentage /= p->cpu_idle_smoothing_depth;
	} else {
		*idle_percentage = 100.0;
		p->valid_last_times = 1;
	}

	/* Remember current value for the next call */
	p->i64LastUserTime = i64UserTime;
	p->i64LastKernelTime = i64KernelTime;
	p->i64LastIdleTime = i64IdleTime;

	/* Success */
	return SWITCH_TRUE;
}

#else

  /* Unsupported */
SWITCH_DECLARE(switch_bool_t) switch_get_system_idle_time(switch_profile_timer_t *p, double *idle_percentage)
{
	*idle_percentage = 100.0;
	return SWITCH_FALSE;
}

#endif

SWITCH_DECLARE(switch_profile_timer_t *)switch_new_profile_timer(void)
{
  unsigned int x;
  switch_profile_timer_t *p = calloc(1, sizeof(switch_profile_timer_t));

  if (!p) return NULL;

  if (runtime.cpu_idle_smoothing_depth > 0) {
	  p->cpu_idle_smoothing_depth = runtime.cpu_idle_smoothing_depth;
  } else {
	  p->cpu_idle_smoothing_depth = 30;
  }

  p->percentage_of_idle_time_ring = calloc(1, sizeof(double) * p->cpu_idle_smoothing_depth);
  switch_assert(p->percentage_of_idle_time_ring);

  for ( x = 0; x < p->cpu_idle_smoothing_depth; x++ ) {
	  p->percentage_of_idle_time_ring[x] = 100.0;
  }

  return p;
}

SWITCH_DECLARE(void) switch_delete_profile_timer(switch_profile_timer_t **p)
{
	if (!p) return;

#ifdef __linux__
	close((*p)->procfd);
	close((*p)->procfd_mem);//UC
#endif
	free((*p)->percentage_of_idle_time_ring);
	free(*p);
	*p = NULL;
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
