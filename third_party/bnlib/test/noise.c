/*
 * Copyright (c) 1993-1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * Get environmental noise.
 */

#include "first.h"
#include <time.h>	/* For time measurement code */

#ifndef MSDOS
#ifdef __MSDOS
#define MSDOS 1
#endif
#endif
#ifndef MSDOS
#ifdef __MSDOS__
#define MSDOS 1
#endif
#endif
#ifndef UNIX
#ifdef unix
#define UNIX 1
#endif
#endif
#ifndef UNIX
#ifdef __unix
#define UNIX 1
#endif
#endif
#ifndef UNIX
#ifdef __unix__
#define UNIX 1
#endif
#endif

#ifdef MSDOS

#if __BORLANDC__
#define far __far   /* Borland C++ 3.1's <dos.h> kacks in ANSI mode.  Ugh! */
#endif

#include <dos.h>	/* for enable() and disable() */
#include <conio.h>	/* for inp() and outp() */

/*
 * This code gets as much information as possible out of 8253/8254 timer 0,
 * which ticks every .84 microseconds.  There are three cases:
 * 1) Original 8253.  15 bits available, as the low bit is unused.
 * 2) 8254, in mode 3.  The 16th bit is available from the status register.
 * 3) 8254, in mode 2.  All 16 bits of the counters are available.
 *    (This is not documented anywhere, but I've seen it!)
 *
 * This code repeatedly tries to latch the status (ignored by an 8253) and
 * sees if it looks like xx1101x0.  If not, it's definitely not an 8254.
 * Repeat this a few times to make sure it is an 8254.
 */
static int
has8254(void)
{
	int i, s1, s2;

	for (i = 0; i < 5; i++) {
		_disable();
		outp(0x43, 0xe2);	/* Latch status for timer 0 */
		s1 = inp(0x40);		/* If 8253, read timer low byte */
		outp(0x43, 0xe2);	/* Latch status for timer 0 */
		s2 = inp(0x40);		/* If 8253, read timer high byte */
		_enable();
		if ((s1 & 0x3d) != 0x34 || (s2 & 0x3d) != 0x34)
			return 0;	/* Ignoring status latch; 8253 */
	}
	return 1;	/* Status reads as expected; 8254 */
}

/* TODO: It might be better to capture this data in a keyboard ISR */
static unsigned
read8254(void)
{
	unsigned status, count;

	_disable();
	outp(0x43, 0xc2);	/* Latch status and count for timer 0 */
	status = inp(0x40);
	count = inp(0x40);
	count |= inp(0x40) << 8;
	_enable();
	/* The timer is usually in mode 3, but some motherboards use mode 2. */
	if (status & 2)
		count = count>>1 | (status & 0x80)<<8;

	return count;
}

static unsigned
read8253(void)
{
	unsigned count;

	_disable();
	outp(0x43, 0x00);	/* Latch count for timer 0 */
	count = (inp(0x40) & 0xff);
	count |= (inp(0x40) & 0xff) << 8;
	_enable();

	return count >> 1;
}
#endif /* MSDOS */

#ifdef UNIX
/*
 * This code uses five different timers, if available, in decreasing
 * priority order:
 * - gethrtime(), assumed unavailable unless USE_GETHRTIME=1
 * - clock_gettime(), auto-detected unless overridden with USE_CLOCK_GETTIME
 * - gettimeofday(), assumed available unless USE_GETTIMEOFDAY=0
 * - getitimer(), auto-detected unless overridden with USE_GETITIMER
 * - ftime(), assumed available unless USE_FTIME=0
 *
 * These are all accessed through the gettime(), timetype, and tickdiff()
 * macros.  The MINTICK constant is something to avoid the gettimeofday()
 * glitch wherein it increments the return value even if no tick has occurred.
 * When measuring the tick interval, if the difference between two successive
 * times is not at least MINTICK ticks, it is ignored.
 */

#include <sys/types.h>
#include <sys/times.h>	/* for times() */
#include <stdlib.h>	/* For qsort() */

#if !USE_GETHRTIME
#ifndef USE_CLOCK_GETTIME	/* Detect using CLOCK_REALTIME from <time.h> */
#ifdef CLOCK_REALTIMExxx	/* Stupid libc... */
#define USE_CLOCK_GETTIME 1
#else
#define USE_CLOCK_GETTIME 0
#endif
#endif

#if !USE_CLOCK_GETTIME
#include <sys/time.h>	/* For gettimeofday(), getitimer(), or ftime() */

#ifndef USE_GETTIMEOFDAY
#define USE_GETTIMEOFDAY 1	/* No way to tell, so assume it's there */
#endif

#if !USE_GETTIMEOFDAY
#ifndef USE_GETITIMER	/* Detect using ITIMER_REAL from <sys/time.h> */
#define USE_GETITIMER defined(ITIMER_REAL)
#endif

#if !USE_GETITIMER
#ifndef USE_FTIME
#define USE_FTIME 1
#endif

#endif /* !USE_GETITIMER */
#endif /* !USE_GETTIMEOFDAY */
#endif /* !USE_CLOCK_GETTIME */
#endif /* !USE_GETHRTIME */

#if USE_GETHRTIME

#define CHOICE_GETHRTIME 1
#include <sys/time.h>
typedef hrtime_t timetype;
#define gettime(s) (*(s) = gethrtime())
#define tickdiff(s,t) ((s)-(t))
#define MINTICK 0

#elif USE_CLOCK_GETTIME

#define CHOICE_CLOCK_GETTIME 1
typedef struct timespec timetype;
#define gettime(s) (void)clock_gettime(CLOCK_REALTIME, s)
#define tickdiff(s,t) (((s).tv_sec-(t).tv_sec)*1000000000 + \
	(s).tv_nsec - (t).tv_nsec)

#elif USE_GETTIMEOFDAY

#define CHOICE_GETTIMEOFDAY 1
typedef struct timeval timetype;
#define gettime(s) (void)gettimeofday(s, (struct timezone *)0)
#define tickdiff(s,t) (((s).tv_sec-(t).tv_sec)*1000000+(s).tv_usec-(t).tv_usec)
#define MINTICK 1

#elif USE_GETITIMER

#define CHOICE_GETITIMER 1
#include <signal.h>	/* For signal(), SIGALRM, SIG_IGN  */
typedef struct itimerval timetype;
#define gettime(s) (void)getitimer(ITIMER_REAL, s)
#define tickdiff(s,t) (((t).it_value.tv_sec-(s).it_value.tv_sec)*1000000 + \
	(t).it_value.tv_usec - (s).it_value.tv_usec)
#define MINTICK 1

#elif USE_FTIME		/* Use ftime() */

#define CHOICE_FTIME 1
#include <sys/timeb.h>
typedef struct timeb timetype;
#define gettime(s) (void)ftime(s)
#define tickdiff(s,t) (((s).time-(t).time)*1000 + (s).millitm - (t).millitm)
#define MINTICK	0

#else

#error No clock available - please define one.

#endif	/* End of complex choice of clock conditional */

#if CHOICE_CLOCK_GETTIME

static unsigned
noiseTickSize(void)
{
	struct timespec res;

	clock_getres(CLOCK_REALTIME, &res);
	return res.tv_nsec;
}

#else /* Normal clock resolution estimation */

#if NOISEDEBUG
#include <stdio.h>
#endif

#define N 15	/* Number of deltas to try (at least 5, preferably odd) */

/* Function needed for qsort() */
static int
noiseCompare(void const *p1, void const *p2)
{
	return *(unsigned const *)p1 > *(unsigned const *)p2 ?  1 :
	       *(unsigned const *)p1 < *(unsigned const *)p2 ? -1 : 0;
}

/*
 * Find the resolution of the high-resolution clock by sampling successive
 * values until a tick boundary, at which point the delta is entered into
 * a table.  An average near the median of the table is taken and returned
 * as the system tick size to eliminate outliers due to descheduling (high)
 * or tv0 not being the "zero" time in a given tick (low).
 *
 * Some trickery is needed to defeat the habit systems have of always
 * incrementing the microseconds field from gettimeofday() results so that
 * no two calls return the same value.  Thus, a "tick boundary" is assumed
 * when successive calls return a difference of more than MINTICK ticks.
 * (For gettimeofday(), this is set to 2 us.)  This catches cases where at
 * most one other task reads the clock between successive reads by this task.
 * More tasks in between are rare enough that they'll get cut off by the
 * median filter.
 *
 * When a tick boundary is found, the *first* time read during the previous
 * tick (tv0) is subtracted from the new time to get microseconds per tick.
 *
 * Suns have a 1 us timer, and as of SunOS 4.1, they return that timer, but
 * there is ~50 us of system-call overhead to get it, so this overestimates
 * the tick size considerably.  On SunOS 5.x/Solaris, the overhead has been
 * cut to about 2.5 us, so the measured time alternates between 2 and 3 us.
 * Some better algorithms will be required for future machines that really
 * do achieve 1 us granularity.
 *
 * Current best idea: discard all this hair and use Ueli Maurer's entropy
 * estimation scheme.  Assign each input event (delta) a sequence number.
 * 16 bits should be more than adequate.  Make a table of the last time
 * (by sequence number) each possibe input event occurred.  For practical
 * implementation, hash the event to a fixed-size code and consider two
 * events identical if they have the same hash code.  This will only ever
 * underestimate entropy.  Then use the number of bits in the difference
 * between the current sequence number and the previous one as the entropy
 * estimate.
 *
 * If it's desirable to use longer contexts, Maurer's original technique
 * just groups events into non-overlapping pairs and uses the technique on
 * the pairs.  If you want to increment the entropy numbers on each keystroke
 * for user-interface niceness, you can do the operation each time, but you
 * have to halve the sequence number difference before starting, and then you
 * have to halve the number of bits of entropy computed because you're adding
 * them twice.
 *
 * You can put the even and odd events into separate tables to close Maurer's
 * model exactly, or you can just dump them into the same table, which will
 * be more conservative.
 */
static unsigned
noiseTickSize(void)
{
	unsigned i = 0, j = 0,  diff, d[N];
	timetype tv0, tv1, tv2;

	gettime(&tv0);
	tv1 = tv0;
	do {
		gettime(&tv2);
		diff = (unsigned)tickdiff(tv2, tv1);
		if (diff > MINTICK) {
			d[i++] = diff;
			tv0 = tv2;
			j = 0;
		} else if (++j >= 4096)	/* Always getting <= MINTICK units */
			return MINTICK + !MINTICK;
		tv1 = tv2;
	} while (i < N);

	/* Return average of middle 5 values (rounding up) */
	qsort(d, N, sizeof(d[0]), noiseCompare);
	diff = (d[N/2-2]+d[N/2-1]+d[N/2]+d[N/2+1]+d[N/2+2]+4)/5;
#if NOISEDEBUG
	fprintf(stderr, "Tick size is %u\n", diff);
#endif
	return diff;
}

#endif /* Clock resolution measurement condition */

#endif /* UNIX */

#include "usuals.h"
#include "randpool.h"
#include "noise.h"

/*
 * Add as much environmentally-derived random noise as possible
 * to the randPool.  Typically, this involves reading the most
 * accurate system clocks available.
 *
 * Returns the number of ticks that have passed since the last call,
 * for entropy estimation purposes.
 */
word32
noise(void)
{
	word32 delta;

#if defined(MSDOS)
	static unsigned deltamask = 0;
	static unsigned prevt;
	unsigned t;
	time_t tnow;
	clock_t cnow;

	if (deltamask == 0)
		deltamask = has8254() ? 0xffff : 0x7fff;
	t = (deltamask & 0x8000) ? read8254() : read8253();
	randPoolAddBytes((byte const *)&t, sizeof(t));
	delta = deltamask & (t - prevt);
	prevt = t;

	/* Add more-significant time components. */
	cnow = clock();
	randPoolAddBytes((byte *)&cnow, sizeof(cnow));
	tnow = time((time_t *)0);
	randPoolAddBytes((byte *)&tnow, sizeof(tnow));
/* END OF DOS */
#elif defined(VMS)
	word32 t[2];	/* little-endian 64-bit timer */
	word32 d1;	/* MSW of difference */
	static word32 prevt[2];

	SYS$GETTIM(t);	/* VMS hardware clock increments by 100000 per tick */
	randPoolAddBytes((byte const *)t, sizeof(t));
	/* Get difference in d1 and delta, and old time in prevt */
	d1 = t[1] - prevt[1] + (t[0] < prevt[0]);
	prevt[1] = t[1];
	delta = t[0] - prevt[0];
	prevt[0] = t[0];
	
	/* Now, divide the 64-bit value by 100000 = 2^5 * 5^5 = 32 * 3125 */
	/* Divide value, MSW in d1 and LSW in delta, by 32 */
	delta >>= 5;
	delta |= d1 << (32-5);
	d1 >>= 5;
	/*
	 * Divide by 3125.  This fits into 16 bits, so the following
	 * code is possible.  2^32 = 3125 * 1374389 + 1671.
	 *
	 * This code has confused people reading it, so here's a detailed
	 * explanation.  First, since we only want a 32-bit result,
	 * reduce the input mod 3125 * 2^32 before starting.  This
	 * amounts to reducing the most significant word mod 3125 and
	 * leaving the least-significant word alone.
	 *
	 * Then, using / for mathematical (real, not integer) division, we
	 * want to compute floor(d1 * 2^32 + d0) / 3125), which I'll denote
	 * using the old [ ] syntax for floor, so it's
	 *   [ (d1 * 2^32 + d0) / 3125 ]
	 * = [ (d1 * (3125 * 1374389 + 1671) + d0) / 3125 ]
	 * = [ d1 * 1374389 + (d1 * 1671 + d0) / 3125 ]
	 * = d1 * 137438 + [ (d1 * 1671 + d0) / 3125 ]
	 * = d1 * 137438 + [ d0 / 3125 ] + [ (d1 * 1671 + d0 % 3125) / 3125 ]
	 *
	 * The C / operator, applied to integers, performs [ a / b ], so
	 * this can be implemented in C, and since d1 < 3125 (by the first
	 * modulo operation), d1 * 1671 + d0 % 3125 < 3125 * 1672, which
	 * is 5225000, less than 2^32, so it all fits into 32 bits.
	 */
	d1 %= 3125;	/* Ignore overflow past 32 bits */
	delta = delta/3125 + d1*1374389 + (delta%3125 + d1*1671) / 3125;
/* END OF VMS */
#elif defined(UNIX)
	timetype t;
	static unsigned ticksize = 0;
	static timetype prevt;

	gettime(&t);
#if CHOICE_GETITIMER
	/* If itimer isn't started, start it */
	if (t.it_value.tv_sec == 0 && t.it_value.tv_usec == 0) {
		/*
		 * start the timer - assume that PGP won't be running for
		 * more than 11 days, 13 hours, 46 minutes and 40 seconds.
		 */
		t.it_value.tv_sec = 1000000;
		t.it_interval.tv_sec = 1000000;
		t.it_interval.tv_usec = 0;
		signal(SIGALRM, SIG_IGN);	/* just in case.. */
		setitimer(ITIMER_REAL, &t, NULL);
		t.it_value.tv_sec = 0;
	}
	randPoolAddBytes((byte const *)&t.it_value, sizeof(t.it_value));
#else
	randPoolAddBytes((byte const *)&t, sizeof(t));
#endif

	if (!ticksize)
		ticksize = noiseTickSize();
	delta = (word32)(tickdiff(t, prevt) / ticksize);
	prevt = t;
/* END OF UNIX */
#else
#error Unknown OS - define UNIX or MSDOS or add code for high-resolution timers
#endif

	return delta;
}
