/*
 * SpanDSP - a series of DSP components for telephony
 *
 * pseudo_terminal_tests.c - pseudo terminal handling tests.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2012 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <inttypes.h>
#include <stdlib.h>

#if defined(WIN32)
#include <windows.h>
#else
#if defined(__APPLE__)
#include <util.h>
#include <sys/ioctl.h>
#elif defined(__FreeBSD__)
#include <libutil.h>
#include <termios.h>
#include <sys/socket.h>
#else
#include <pty.h>
#endif
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#endif

#include "spandsp.h"

#include "spandsp/t30_fcf.h"

#include "spandsp-sim.h"

#undef SPANDSP_EXPOSE_INTERNAL_STRUCTURES

#include "pseudo_terminals.h"

static int master(void)
{
    modem_t modem[10];
    char buf[1024];
    int len;
    int i;
#if !defined(WIN32)
    int tioflags;
#endif

    for (i = 0;  i < 10;  i++)
    {
        if (psuedo_terminal_create(&modem[i]))
        {
            printf("Failure\n");
            exit(2);
        }
        printf("%s %s\n", modem[i].devlink, modem[i].stty);
    }

    for (i = 0;  i < 10;  i++)
    {
#if !defined(WIN32)
        ioctl(modem[i].slave, TIOCMGET, &tioflags);
        tioflags |= TIOCM_RI;
        ioctl(modem[i].slave, TIOCMSET, &tioflags);
#endif
    }

    for (;;)
    {
        for (i = 0;  i < 10;  i++)
        {
            len = read(modem[i].master, buf, 4);
            if (len >= 0)
            {
                buf[len] = '\0';
                printf("%d %d '%s' %s\n", i, len, buf, strerror(errno));
#if !defined(WIN32)
                ioctl(modem[i].slave, TIOCMGET, &tioflags);
                tioflags |= TIOCM_RI;
                ioctl(modem[i].slave, TIOCMSET, &tioflags);
#endif
            }
        }
    }

    for (i = 0;  i < 10;  i++)
    {
        if (psuedo_terminal_close(&modem[i]))
        {
            printf("Failure\n");
            exit(2);
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int slave(void)
{
    int fd[10];
    char name[64];
    int i;
    int j;
#if !defined(WIN32)
    int tioflags;
#endif

    for (i = 0;  i < 10;  i++)
    {
        sprintf(name, "/dev/spandsp/%d", i);
        if ((fd[i] = open(name, O_RDWR)) < 0)
        {
            printf("Failed to open %s\n", name);
            exit(2);
        }
        printf("%s\n", name);
    }

    for (i = 0;  i < 10;  i++)
    {
#if !defined(WIN32)
        ioctl(fd[i], TIOCMGET, &tioflags);
        if ((tioflags & TIOCM_RI))
            printf("Ring %d\n", i);
        else
            printf("No ring %d\n", i);
#endif
    }

    for (j = 0;  j < 10;  j++)
    {
        for (i = 0;  i < 10;  i++)
        {
            write(fd[i], "FRED", 4);
#if !defined(WIN32)
            ioctl(fd[i], TIOCMGET, &tioflags);
            if ((tioflags & TIOCM_RI))
                printf("Ring %d\n", i);
#endif
        }
    }

    for (i = 0;  i < 10;  i++)
    {
        if (close(fd[i]))
        {
            printf("Failed to close %d\n", i);
            exit(2);
        }
    }

    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    if (argc < 2)
        master();
    else
        slave();
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
