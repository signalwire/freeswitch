/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * kbunix.c - Unix keyboard input routines.
 */

/*
 * Define NOTERMIO if you don't have the termios stuff
 */

#include "first.h"
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>	/* For exit() */
#include <sys/types.h>

/* How to get cbreak mode */

#if defined(NOTERMIO)
#include <sgtty.h>	/* No termio: Use ioctl() TIOCGETP and TIOCSETP */
#elif defined(SVR2)
#include <termio.h>	/* SVR2: Use ioctl() TCGETA and TCSETAF */
#else /* Usual case */
#include <termios.h>	/* Posix: use tcgetattr/tcsetattr */
#endif

#ifdef sun /* including ioctl.h and termios.h gives a lot of warnings on sun */
#include <sys/filio.h>
#else
#include <sys/ioctl.h>		/* for FIONREAD */
#endif /* sun */

#ifndef FIONREAD
#define	FIONREAD	TIOCINQ
#endif

#include "posix.h"	/* For read(), sleep() */
#include "kb.h"
#if UNITTTEST
#define randEvent(c) (void)c
#else
#include "random.h"
#endif

#include "kludge.h"

/* The structure to hold the keyuboard's state */
#if defined(NOTERMIO)
static struct sgttyb kbState0, kbState1;
#elif defined(SVR2)
static struct termio kbState0, kbState1;
#else
static struct termios kbState0, kbState1;
#endif

#ifndef CBREAK
#define CBREAK RAW
#endif
/* The basic task of getting the terminal into CBREAK mode. */
static void
kbInternalCbreak(int fd)
{
#ifdef NOTERMIO

	if (ioctl(fd, TIOCGETP, &kbState0) < 0) {
		fprintf (stderr, "\nUnable to get terminal characteristics: ");
		perror("ioctl");
		exit(1);
	}
	kbState1 = kbState0;
	kbState1.sg_flags |= CBREAK;
	kbState1.sg_flags &= ~ECHO;
	ioctl(fd, TIOCSETP, &kbState1);

#else /* !NOTERMIO - the usual case */

#ifdef SVR2
	if (ioctl(fd, TCGETA, &kbState0) < 0)
#else
	if (tcgetattr(fd, &kbState0) < 0)
#endif
	{
		fprintf (stderr, "\nUnable to get terminal characteristics: ");
		perror("ioctl");
		exit(1);
	}
	kbState1 = kbState0;
	kbState1.c_cc[VMIN] = 1;
	kbState1.c_cc[VTIME] = 0;
	kbState1.c_lflag &= ~(ECHO|ICANON);
#ifdef SVR2
	ioctl(fd, TCSETAF, &kbState1);
#else
	tcsetattr(fd, TCSAFLUSH, &kbState1);
#endif /* not SVR2 */

#endif	/* !NOTERMIO */
}

/* Restore the terminal to normal operation */
static void
kbInternalNorm(int fd)
{
#if defined(NOTERMIO)
	ioctl(fd, TIOCSETP, &kbState0);
#elif defined(SVR2)
	ioctl(fd, TCSETAF, &kbState0);
#else /* Usual case */
	tcsetattr (fd, TCSAFLUSH, &kbState0);
#endif
}

/* State variables */
static volatile int kbCbreakFlag = 0;
static int kbFd = -1;

#ifdef SVR2
static int (*savesig)(int);
#else
static void (*savesig)(int);
#endif

/* A wrapper around SIGINT and SIGCONT to restore the terminal modes. */
static void
kbSig1(int sig)
{
	if (kbCbreakFlag)
		kbInternalNorm(kbFd);
	if (sig == SIGINT)
		signal(sig, savesig);
	else
		signal(sig, SIG_DFL);
	raise(sig);	/* Re-send the signal */
}

static void
kbAddSigs(void);

/* Resume cbreak after SIGCONT */
static void
kbSig2(int sig)
{
	(void)sig;
	if (kbCbreakFlag)
		kbInternalCbreak(kbFd);
	else
		kbAddSigs();
}

static void
kbAddSigs(void)
{
	savesig = signal (SIGINT, kbSig1);
#ifdef	SIGTSTP
	signal (SIGCONT, kbSig2);
	signal (SIGTSTP, kbSig1);
#endif
}

static void
kbRemoveSigs(void)
{
	signal (SIGINT, savesig);
#ifdef	SIGTSTP
	signal (SIGCONT, SIG_DFL);
	signal (SIGTSTP, SIG_DFL);
#endif
}


/* Now, at last, the externally callable functions */

void
kbCbreak(void)
{
	if (kbFd < 0) {
		kbFd = open("/dev/tty", O_RDWR);
		if (kbFd < 0) {
			fputs("Can't open tty; using stdin\n", stderr);
			kbFd = STDIN_FILENO;
		}
	}

	kbAddSigs();
	kbCbreakFlag = 1;
	kbInternalCbreak(kbFd);
}

void
kbNorm(void)
{
	kbInternalNorm(kbFd);
	kbCbreakFlag = 0;
	kbRemoveSigs();
}

int
kbGet(void)
{
	int i;
	char c;

	i = read(kbFd, &c, 1);
	if (i < 1)
		return -1;
	randEvent(c);
	return c;
}

/*
 * Flush any pending input.  If "thorough" is set, tries to be more
 * thorough about it.  Ideally, wait for 1 second of quiet, but we
 * may do something more primitive.
 *
 * kbCbreak() has the side effect of flushing the inout queue, so this
 * is not too critical.
 */
void
kbFlush(int thorough)
{
	if (thorough)
		sleep(1);
#if defined(TCIFLUSH)
	tcflush(kbFd, TCIFLUSH);
#elif defined(TIOCFLUSH)
#ifndef FREAD
#define FREAD 1	/* The usual value */
#endif
	ioctl(kbFd, TIOCFLUSH, FREAD);
#endif
}

#if UNITTEST	/* Self-contained test driver */

#include <ctype.h>

int
main(void)
{
	int c;

	puts("Going to cbreak mode...");
	kbCbreak();
	puts("In cbreak mode.  Please type.");
	for (;;) {
		c = kbGet();
		if (c == '\n' || c == '\r')
			break;
		printf("c = %d = '%c'\n", c, c);
		kbFlush(isupper(c));
	}
	puts("Returning to normal mode...");
	kbNorm();
	puts("Done.");
	return 0;
}

#endif /* UNITTEST */
