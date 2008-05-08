/* 
 * Functions Windows doesn't have... but should
 * Copyright(C) 2001, Linux Support Services, Inc.
 *
 * Distributed under GNU LGPL.
 *
 * These are NOT fully compliant with BSD 4.3 and are not
 * threadsafe.
 *
 */

#ifndef _winpoop_h
#define _winpoop_h

#if defined(_MSC_VER)
#define INLINE __inline
#else
#define INLINE inline
#endif

#include <winsock.h>

void gettimeofday(struct timeval *tv, void /*struct timezone*/ *tz);

static INLINE int inet_aton(char *cp, struct in_addr *inp)
{
	int a1, a2, a3, a4;
	unsigned int saddr;
	if (sscanf(cp, "%d.%d.%d.%d", &a1, &a2, &a3, &a4) != 4)
		return 0;
	a1 &= 0xff;
	a2 &= 0xff;
	a3 &= 0xff;
	a4 &= 0xff; 
	saddr = (a1 << 24) | (a2 << 16) | (a3 << 8) | a4;
	inp->s_addr = htonl(saddr);
	return 1;
}

#endif
