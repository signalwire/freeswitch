/*
 * SpanDSP - a series of DSP components for telephony
 *
 * pseudo_terminals_tests.c - pseudo terminal handling tests.
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
#include <fcntl.h>
#include <poll.h>
#endif

#include "spandsp.h"

#include "pseudo_terminals.h"

int main(int argc, char *argv[])
{
    modem_t modem[10];
    int i;

    for (i = 0;  i < 10;  i++)
    {
        if (psuedo_terminal_create(&modem[i]))
            printf("Failure\n");
        printf("%s %s\n", modem[i].devlink, modem[i].stty);
    }
    getchar();
    for (i = 0;  i < 10;  i++)
    {
        if (psuedo_terminal_close(&modem[i]))
            printf("Failure\n");
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
