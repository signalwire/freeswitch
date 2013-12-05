/*
 * SpanDSP - a series of DSP components for telephony
 *
 * pseudo_terminals.c - pseudo terminal handling.
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
#else
#include <pty.h>
#endif
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#endif

#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES

#include "spandsp.h"

#include "pseudo_terminals.h"

int next_id = 0;
const char *device_root_name = "/dev/spandsp";

int psuedo_terminal_close(modem_t *modem)
{
#if defined(WIN32)
    if (modem->master)
    {
        CloseHandle(modem->master);
        modem->master = 0;
    }
#else
    if (modem->master > -1)
    {
        shutdown(modem->master, 2);
        close(modem->master);
        modem->master = -1;
    }
#endif

    if (modem->slave > -1)
    {
        shutdown(modem->slave, 2);
        close(modem->slave);
        modem->slave = -1;
    }

    if (unlink(modem->devlink))
        return -1;

    return 0;
}
/*- End of function --------------------------------------------------------*/

int psuedo_terminal_create(modem_t *modem)
{
#if defined(WIN32)
    COMMTIMEOUTS timeouts = {0};
#endif

    memset(modem, 0, sizeof(*modem));

    span_log_init(&modem->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&modem->logging, "PTY");

    span_log_set_level(&modem->logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(&modem->logging, "PTY");

    modem->master = -1;
    modem->slave = -1;

#if USE_OPENPTY
    if (openpty(&modem->master, &modem->slave, NULL, NULL, NULL))
    {
        span_log(&modem->logging, SPAN_LOG_ERROR, "Fatal error: failed to initialize pty\n");
        return -1;
    }
    modem->stty = ttyname(modem->slave);
#else
#if defined(WIN32)
    modem->slot = 4 + next_id++; /* need work here we start at COM4 for now*/
    snprintf(modem->devlink, sizeof(modem->devlink), "COM%d", modem->slot);

    modem->master = CreateFile(modem->devlink,
                               GENERIC_READ | GENERIC_WRITE,
                               0,
                               0,
                               OPEN_EXISTING,
                               FILE_FLAG_OVERLAPPED,
                               0);
    if (modem->master == INVALID_HANDLE_VALUE)
    {
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
            span_log(&modem->logging, SPAN_LOG_ERROR, "Fatal error: Serial port does not exist\n");
        else
            span_log(&modem->logging, SPAN_LOG_ERROR, "Fatal error: Serial port open error\n");
        return -1;
    }
#elif !defined(HAVE_POSIX_OPENPT)
    modem->master = open("/dev/ptmx", O_RDWR);
#else
    modem->master = posix_openpt(O_RDWR | O_NOCTTY);
#endif

#if !defined(WIN32)
    if (modem->master < 0)
        span_log(&modem->logging, SPAN_LOG_ERROR, "Fatal error: failed to initialize UNIX98 master pty\n");

    if (grantpt(modem->master) < 0)
        span_log(&modem->logging, SPAN_LOG_ERROR, "Fatal error: failed to grant access to slave pty\n");

    if (unlockpt(modem->master) < 0)
        span_log(&modem->logging, SPAN_LOG_ERROR, "Fatal error: failed to unlock slave pty\n");

    if ((modem->stty = ptsname(modem->master)) == NULL)
        span_log(&modem->logging, SPAN_LOG_ERROR, "Fatal error: failed to obtain slave pty filename\n");

    if ((modem->slave = open(modem->stty, O_RDWR)) < 0)
        span_log(&modem->logging, SPAN_LOG_ERROR, "Fatal error: failed to open slave pty %s\n", modem->stty);
#endif

#if defined(SOLARIS)
    ioctl(modem->slave, I_PUSH, "ptem");
    ioctl(modem->slave, I_PUSH, "ldterm");
#endif
#endif

#if defined(WIN32)
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;

    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    SetCommMask(modem->master, EV_RXCHAR);

    if (!SetCommTimeouts(modem->master, &timeouts))
    {
        span_log(&modem->logging, SPAN_LOG_ERROR, "Cannot set up non-blocking read on %s\n", modem->devlink);
        psuedo_terminal_close(modem);
        return -1;
    }
    modem->threadAbort = CreateEvent(NULL, true, false, NULL);
#else
    modem->slot = next_id++;
    snprintf(modem->devlink, sizeof(modem->devlink), "%s/%d", device_root_name, modem->slot);

    /* Remove any stale link which might be present */
    unlink(modem->devlink);

    if (symlink(modem->stty, modem->devlink))
    {
        span_log(&modem->logging, SPAN_LOG_ERROR, "Fatal error: failed to create %s symbolic link\n", modem->devlink);
        psuedo_terminal_close(modem);
        return -1;
    }

    if (fcntl(modem->master, F_SETFL, fcntl(modem->master, F_GETFL, 0) | O_NONBLOCK))
    {
        span_log(&modem->logging, SPAN_LOG_ERROR, "Cannot set up non-blocking read on %s\n", ttyname(modem->master));
        psuedo_terminal_close(modem);
        return -1;
    }
#endif
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
