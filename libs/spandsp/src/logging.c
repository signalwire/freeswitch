/*
 * SpanDSP - a series of DSP components for telephony
 *
 * logging.c - error and debug logging.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp/stdbool.h"
#endif

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/logging.h"

#include "spandsp/private/logging.h"

static void default_message_handler(void *user_data, int level, const char *text);

static message_handler_func_t __span_message = &default_message_handler;
static void *__user_data = NULL;

/* Note that this list *must* match the enum definition in logging.h */
static const char *severities[] =
{
    "NONE",
    "ERROR",
    "WARNING",
    "PROTOCOL_ERROR",
    "PROTOCOL_WARNING",
    "FLOW",
    "FLOW 2",
    "FLOW 3",
    "DEBUG 1",
    "DEBUG 2",
    "DEBUG 3"
};

static void default_message_handler(void *user_data, int level, const char *text)
{
    fprintf(stderr, "%s", text);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(bool) span_log_test(logging_state_t *s, int level)
{
    if (s  &&  (s->level & SPAN_LOG_SEVERITY_MASK) >= (level & SPAN_LOG_SEVERITY_MASK))
        return true;
    return false;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) span_log(logging_state_t *s, int level, const char *format, ...)
{
    char msg[1024 + 1];
    va_list arg_ptr;
    int len;
    struct tm *tim;
    struct timeval nowx;
    time_t now;

    if (span_log_test(s, level))
    {
        va_start(arg_ptr, format);
        len = 0;
        if ((level & SPAN_LOG_SUPPRESS_LABELLING) == 0)
        {
            if ((s->level & SPAN_LOG_SHOW_DATE))
            {
                gettimeofday(&nowx, NULL);
                now = nowx.tv_sec;
                tim = gmtime(&now);
                len += snprintf(msg + len,
                                1024 - len,
                                "%04d/%02d/%02d %02d:%02d:%02d.%03d ",
                                tim->tm_year + 1900,
                                tim->tm_mon + 1,
                                tim->tm_mday,
                                tim->tm_hour,
                                tim->tm_min,
                                tim->tm_sec,
                                (int) nowx.tv_usec/1000);
            }
            /*endif*/
            if ((s->level & SPAN_LOG_SHOW_SAMPLE_TIME))
            {
                now = s->elapsed_samples/s->samples_per_second;
                tim = gmtime(&now);
                len += snprintf(msg + len,
                                1024 - len,
                                "%02d:%02d:%02d.%03d ",
                                tim->tm_hour,
                                tim->tm_min,
                                tim->tm_sec,
                                (int) (s->elapsed_samples%s->samples_per_second)*1000/s->samples_per_second);
            }
            /*endif*/
            if ((s->level & SPAN_LOG_SHOW_SEVERITY)  &&  (level & SPAN_LOG_SEVERITY_MASK) <= SPAN_LOG_DEBUG_3)
                len += snprintf(msg + len, 1024 - len, "%s ", severities[level & SPAN_LOG_SEVERITY_MASK]);
            /*endif*/
            if ((s->level & SPAN_LOG_SHOW_PROTOCOL)  &&  s->protocol)
                len += snprintf(msg + len, 1024 - len, "%s ", s->protocol);
            /*endif*/
            if ((s->level & SPAN_LOG_SHOW_TAG)  &&  s->tag)
                len += snprintf(msg + len, 1024 - len, "%s ", s->tag);
            /*endif*/
        }
        /*endif*/
        vsnprintf(msg + len, 1024 - len, format, arg_ptr);
        if (s->span_message)
            s->span_message(s->user_data, level, msg);
        else if (__span_message)
            __span_message(s->user_data, level, msg);
        /*endif*/
        va_end(arg_ptr);
        return 1;
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) span_log_buf(logging_state_t *s, int level, const char *tag, const uint8_t *buf, int len)
{
    char msg[1024];
    int i;
    int msg_len;

    if (span_log_test(s, level))
    {
        msg_len = 0;
        if (tag)
            msg_len += snprintf(msg + msg_len, 1024 - msg_len, "%s", tag);
        for (i = 0;  i < len  &&  msg_len < 800;  i++)
            msg_len += snprintf(msg + msg_len, 1024 - msg_len, " %02x", buf[i]);
        snprintf(msg + msg_len, 1024 - msg_len, "\n");
        return span_log(s, level, msg);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) span_log_get_level(logging_state_t *s)
{
    return s->level;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) span_log_set_level(logging_state_t *s, int level)
{
    s->level = level;

    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) span_log_get_tag(logging_state_t *s)
{
    return s->tag;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) span_log_set_tag(logging_state_t *s, const char *tag)
{
    s->tag = tag;

    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) span_log_get_protocol(logging_state_t *s)
{
    return s->protocol;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) span_log_set_protocol(logging_state_t *s, const char *protocol)
{
    s->protocol = protocol;

    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) span_log_set_sample_rate(logging_state_t *s, int samples_per_second)
{
    s->samples_per_second = samples_per_second;

    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) span_log_bump_samples(logging_state_t *s, int samples)
{
    s->elapsed_samples += samples;

    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) span_log_set_message_handler(logging_state_t *s, message_handler_func_t func, void *user_data)
{
    s->span_message = func;
    s->user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) span_set_message_handler(message_handler_func_t func, void *user_data)
{
    __span_message = func;
    __user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) span_log_init(logging_state_t *s, int level, const char *tag)
{
    if (s == NULL)
    {
        if ((s = (logging_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    s->span_message = __span_message;
    s->level = level;
    s->tag = tag;
    s->protocol = NULL;
    s->samples_per_second = SAMPLE_RATE;
    s->elapsed_samples = 0;

    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) span_log_release(logging_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) span_log_free(logging_state_t *s)
{
    if (s)
        span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
