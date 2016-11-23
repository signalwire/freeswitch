/*
 * Copyright (c) 2007-2014, Anthony Minessale II
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

#include <ks.h>

#ifdef WIN32
static CRITICAL_SECTION timer_section;
static ks_time_t win32_tick_time_since_start = -1;
static DWORD win32_last_get_time_tick = 0;

static uint8_t win32_use_qpc = 0;
static uint64_t win32_qpc_freq = 0;
static int timer_init;
static inline void win32_init_timers(void)
{
	OSVERSIONINFOEX version_info; /* Used to fetch current OS version from Windows */
	InitializeCriticalSection(&timer_section);
	EnterCriticalSection(&timer_section);

	ZeroMemory(&version_info, sizeof(OSVERSIONINFOEX));
	version_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

	/* Check if we should use timeGetTime() (pre-Vista) or QueryPerformanceCounter() (Vista and later) */

	if (GetVersionEx((OSVERSIONINFO*) &version_info)) {
		if (version_info.dwPlatformId == VER_PLATFORM_WIN32_NT && version_info.dwMajorVersion >= 6) {
			if (QueryPerformanceFrequency((LARGE_INTEGER*)&win32_qpc_freq) && win32_qpc_freq > 0) {
				/* At least Vista, and QueryPerformanceFrequency() suceeded, enable qpc */
				win32_use_qpc = 1;
			} else {
				/* At least Vista, but QueryPerformanceFrequency() failed, disable qpc */
				win32_use_qpc = 0;
			}
		} else {
			/* Older then Vista, disable qpc */
			win32_use_qpc = 0;
		}
	} else {
		/* Unknown version - we want at least Vista, disable qpc */
		win32_use_qpc = 0;
	}

	if (win32_use_qpc) {
		uint64_t count = 0;

		if (!QueryPerformanceCounter((LARGE_INTEGER*)&count) || count == 0) {
			/* Call to QueryPerformanceCounter() failed, disable qpc again */
			win32_use_qpc = 0;
		}
	}

	if (!win32_use_qpc) {
		/* This will enable timeGetTime() instead, qpc init failed */
		win32_last_get_time_tick = timeGetTime();
		win32_tick_time_since_start = win32_last_get_time_tick;
	}

	LeaveCriticalSection(&timer_section);

	timer_init = 1;
}

KS_DECLARE(ks_time_t) ks_time_now(void)
{
	ks_time_t now;
	
	if (!timer_init) {
		win32_init_timers();
	}

	if (win32_use_qpc) {
		/* Use QueryPerformanceCounter */
		uint64_t count = 0;
		QueryPerformanceCounter((LARGE_INTEGER*)&count);
		now = ((count * 1000000) / win32_qpc_freq);
	} else {
		/* Use good old timeGetTime() */
		DWORD tick_now;
		DWORD tick_diff;
		
		tick_now = timeGetTime();
		if (win32_tick_time_since_start != -1) {
			EnterCriticalSection(&timer_section);
			/* just add diff (to make it work more than 50 days). */
			tick_diff = tick_now - win32_last_get_time_tick;
			win32_tick_time_since_start += tick_diff;
			
			win32_last_get_time_tick = tick_now;
			now = (win32_tick_time_since_start * 1000);
				LeaveCriticalSection(&timer_section);
		} else {
			/* If someone is calling us before timer is initialized,
			 * return the current tick
			 */
			now = (tick_now * 1000);
		}
	}

	return now;
}

KS_DECLARE(ks_time_t) ks_time_now_sec(void)
{
	ks_time_t now;
	
	if (!timer_init) {
		win32_init_timers();
	}

	if (win32_use_qpc) {
		/* Use QueryPerformanceCounter */
		uint64_t count = 0;
		QueryPerformanceCounter((LARGE_INTEGER*)&count);
		now = (count / win32_qpc_freq);
	} else {
		/* Use good old timeGetTime() */
		DWORD tick_now;
		DWORD tick_diff;
		
		tick_now = timeGetTime();
		if (win32_tick_time_since_start != -1) {
			EnterCriticalSection(&timer_section);
			/* just add diff (to make it work more than 50 days). */
			tick_diff = tick_now - win32_last_get_time_tick;
			win32_tick_time_since_start += tick_diff;
			
			win32_last_get_time_tick = tick_now;
			now = (win32_tick_time_since_start / 1000);
				LeaveCriticalSection(&timer_section);
		} else {
			/* If someone is calling us before timer is initialized,
			 * return the current tick
			 */
			now = (tick_now / 1000);
		}
	}

	return now;
}

KS_DECLARE(void) ks_sleep(ks_time_t microsec)
{

	LARGE_INTEGER perfCnt, start, now;
	
	QueryPerformanceFrequency(&perfCnt);
	QueryPerformanceCounter(&start);
	
	do {
		QueryPerformanceCounter((LARGE_INTEGER*) &now);
	} while ((now.QuadPart - start.QuadPart) / (float)(perfCnt.QuadPart) * 1000 * 1000 < (DWORD)microsec);
	
}

#else //!WINDOWS, UNIX ETC
KS_DECLARE(ks_time_t) ks_time_now(void)
{
	ks_time_t now;

#if (defined(HAVE_CLOCK_GETTIME) && defined(CLOCK_REALTIME))
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	now = (int64_t)ts.tv_sec * 1000000 + ((int64_t)ts.tv_nsec / 1000);
#else 
	struct timeval tv;
	gettimeofday(&tv, NULL);
	now = tv.tv_sec * 1000000 + tv.tv_usec;
#endif

	return now;
}

KS_DECLARE(ks_time_t) ks_time_now_sec(void)
{
	ks_time_t now;

#if (defined(HAVE_CLOCK_GETTIME) && defined(CLOCK_REALTIME))
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	now = (int64_t)ts.tv_sec;
#else 
	struct timeval tv;
	gettimeofday(&tv, NULL);
	now = tv.tv_sec;
#endif

	return now;
}

#if !defined(HAVE_CLOCK_NANOSLEEP) && !defined(__APPLE__)
static void generic_sleep(ks_time_t microsec)
{
#ifdef HAVE_USLEEP
	usleep(microsec);
#else
	struct timeval tv;
	tv.tv_usec = ks_time_usec(microsec);
	tv.tv_sec = ks_time_sec(microsec);
	select(0, NULL, NULL, NULL, &tv);
#endif
}
#endif

KS_DECLARE(void) ks_sleep(ks_time_t microsec)
{
#if defined(HAVE_CLOCK_NANOSLEEP) || defined(__APPLE__)
	struct timespec ts;
#endif
	
#if defined(HAVE_CLOCK_NANOSLEEP)
	ts.tv_sec = ks_time_sec(microsec);
	ts.tv_nsec = ks_time_nsec(microsec);
	clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
#elif defined(__APPLE__)
	ts.tv_sec = ks_time_sec(microsec);
	ts.tv_nsec = ks_time_usec(microsec) * 850;
	nanosleep(&ts, NULL);
#else
	generic_sleep(microsec);
#endif
	
#if defined(__APPLE__)
	sched_yield();
#endif
	
}

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
