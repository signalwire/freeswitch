/*
 * SpanDSP - a series of DSP components for telephony
 *
 * pseudo_terminals.h - pseudo terminal handling.
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

#if !defined(HAVE_POSIX_OPENPT)  &&  !defined(HAVE_DEV_PTMX)  &&  !defined(WIN32)
#define USE_OPENPTY 1
#endif

struct modem_s
{
    int slot;
    int master;
    int slave;
    const char *stty;
    char devlink[128];
    int block_read;
    int block_write;
    logging_state_t logging;
};

typedef struct modem_s modem_t;

int pseudo_terminal_close(modem_t *modem);

int pseudo_terminal_create(modem_t *modem);
