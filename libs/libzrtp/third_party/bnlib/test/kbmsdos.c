/*
 * Copyright (c) 1993  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * MS-DOS non-echoing keyboard routines.
 */

#include <conio.h>	/* For getch() and kbhit() */
#include <signal.h>	/* For raise() */
#ifdef _MSC_VER
#include <time.h>	/* For clock() */
#else
#include <dos.h>	/* For sleep() */
#endif

#include "kb.h"
#include "random.h"	/* For randEvent() */

/* These are pretty boring */
void kbCbreak(void) { }
void kbNorm(void) { }

int kbGet(void)
{
	int c;

	c = getch();
	if (c == 0)
		c = 0x100 + getch();

	/*
	 * Borland C's getch() uses int 0x21 function 0x7,
	 * which does not detect break.  So we do it explicitly.
	 */
	if (c == 3)
		raise(SIGINT);

	randEvent(c);

	return c;
}

#ifdef _MSC_VER
/*
 * Microsoft Visual C 1.5 (at least) does not have sleep() in the
 * library.  So we use this crude approximation.  ("crude" because,
 * assuming CLOCKS_PER_SEC is 18.2, it rounds to 18 to avoid floating
 * point math.)
 */
#ifndef CLOCKS_PER_SEC
#define CLOCKS_PER_SEC CLK_TCK
#endif
static unsigned
sleep(unsigned t)
{
	clock_t target;

	target = clock() + t * (unsigned)CLOCKS_PER_SEC;
	while (clock() < target)
		;
	return 0;
}
#endif

void kbFlush(int thorough)
{
	do {
		while(kbhit())
			(void)getch();
		if (!thorough)
			break;
		/* Extra thorough: wait for one second of quiet */
		sleep(1);
	} while (kbhit());
}
