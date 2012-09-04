/*
 * SpanDSP - a series of DSP components for telephony
 *
 * schedule_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
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

/*! \page schedule_tests_page Event scheduler tests
\section schedule_tests_page_sec_1 What does it do?
???.

\section schedule_tests_page_sec_2 How does it work?
???.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
#include "spandsp.h"

uint64_t when1;
uint64_t when2;

static void callback1(span_sched_state_t *s, void *user_data)
{
    int id;
    uint64_t when;

    when = span_schedule_time(s);
    printf("1: Callback at %f %" PRId64 "\n", (float) when/1000000.0, when - when1);
    if ((when - when1))
    {
        printf("Callback occured at the wrong time.\n");
        exit(2);
    }
    id = span_schedule_event(s, 500000, callback1, NULL);
    when1 = when + 500000;
    when = span_schedule_next(s);
    printf("1: Event %d, earliest is %" PRId64 "\n", id, when);
}

static void callback2(span_sched_state_t *s, void *user_data)
{
    int id;
    uint64_t when;

    when = span_schedule_time(s);
    printf("2: Callback at %f %" PRId64 "\n", (float) when/1000000.0, when - when2);
    id = span_schedule_event(s, 550000, callback2, NULL);
    if ((when - when2) != 10000)
    {
        printf("Callback occured at the wrong time.\n");
        exit(2);
    }
    when2 = when + 550000;
    when = span_schedule_next(s);
    printf("2: Event %d, earliest is %" PRId64 "\n", id, when);
}

int main(int argc, char *argv[])
{
    int i;
    span_sched_state_t sched;
    uint64_t when;

    span_schedule_init(&sched);

    span_schedule_event(&sched, 500000, callback1, NULL);
    span_schedule_event(&sched, 550000, callback2, NULL);
    when1 = span_schedule_time(&sched) + 500000;
    when2 = span_schedule_time(&sched) + 550000;
    //span_schedule_del(&sched, id);
    
    for (i = 0;  i < 100000000;  i += 20000)
        span_schedule_update(&sched, 20000);
    when = span_schedule_time(&sched);
    if ((when1 - when) < 0  ||  (when1 - when) > 500000  ||  (when2 - when) < 0  ||  (when2 - when) > 550000)
    {
        printf("Callback failed to occur.\n");
        exit(2);
    }
    span_schedule_release(&sched);

    printf("Tests passed.\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
