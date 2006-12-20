/* iksemel (XML parser for Jabber)
** Copyright (C) 2000-2003 Gurer Ozen <madcat@e-kolay.net>
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU Lesser General Public License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <limits.h>
#else
#include <sys/time.h>
#endif

#include "iksemel.h"

/* timing functions */

#ifdef _WIN32
static DWORD start_tv;

void
t_reset (void)
{
	start_tv = GetTickCount ();
}

unsigned long
t_elapsed (void)
{
	DWORD end_tv;

	end_tv = GetTickCount ();
	if (end_tv < start_tv)
		return UINT_MAX - (start_tv - end_tv - 1);
	else
		return end_tv - start_tv;
}

#else
static struct timeval start_tv;

void
t_reset (void)
{
	gettimeofday (&start_tv, NULL);
}

unsigned long
t_elapsed (void)
{
	unsigned long msec;
	struct timeval cur_tv;

	gettimeofday (&cur_tv, NULL);
	msec = (cur_tv.tv_sec * 1000) + (cur_tv.tv_usec / 1000);
	msec -= (start_tv.tv_sec * 1000) + (start_tv.tv_usec / 1000);
	return msec;
}
#endif

/* memory functions */

static void *
m_malloc (size_t size)
{
	void *ptr = malloc (size);
	printf ("MEM: malloc (%d) => %p\n", size, ptr);
	return ptr;
}

static void
m_free (void *ptr)
{
	printf ("MEM: free (%p)\n", ptr);
}

void
m_trace (void)
{
	iks_set_mem_funcs (m_malloc, m_free);
}
