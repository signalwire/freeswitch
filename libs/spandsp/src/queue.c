/*
 * SpanDSP - a series of DSP components for telephony
 *
 * queue.c - simple in-process message queuing
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
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

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdlib.h>
#include <inttypes.h>
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp/stdbool.h"
#endif
#if defined(HAVE_STDATOMIC_H)
#include <stdatomic.h>
#endif
#include <sys/types.h>

#define SPANDSP_FULLY_DEFINE_QUEUE_STATE_T
#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/queue.h"

#include "spandsp/private/queue.h"

SPAN_DECLARE(bool) queue_empty(queue_state_t *s)
{
    return (s->iptr == s->optr);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) queue_free_space(queue_state_t *s)
{
    int len;

    if ((len = s->optr - s->iptr - 1) < 0)
        len += s->len;
    /*endif*/
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) queue_contents(queue_state_t *s)
{
    int len;

    if ((len = s->iptr - s->optr) < 0)
        len += s->len;
    /*endif*/
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) queue_flush(queue_state_t *s)
{
    s->optr = s->iptr;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) queue_view(queue_state_t *s, uint8_t *buf, int len)
{
    int real_len;
    int to_end;
    int iptr;
    int optr;

    /* Snapshot the values (although only iptr should be changeable during this processing) */
    iptr = s->iptr;
    optr = s->optr;
    if ((real_len = iptr - optr) < 0)
        real_len += s->len;
    /*endif*/
    if (real_len < len)
    {
        if (s->flags & QUEUE_READ_ATOMIC)
            return -1;
        /*endif*/
    }
    else
    {
        real_len = len;
    }
    /*endif*/
    if (real_len == 0)
        return 0;
    /*endif*/
    to_end = s->len - optr;
    if (iptr < optr  &&  to_end < real_len)
    {
        /* A two step process */
        if (buf)
        {
            memcpy(buf, &s->data[optr], to_end);
            memcpy(&buf[to_end], s->data, real_len - to_end);
        }
        /*endif*/
    }
    else
    {
        /* A one step process */
        if (buf)
            memcpy(buf, &s->data[optr], real_len);
        /*endif*/
    }
    /*endif*/
    return real_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) queue_read(queue_state_t *s, uint8_t *buf, int len)
{
    int real_len;
    int to_end;
    int new_optr;
    int iptr;
    int optr;

    /* Snapshot the values (although only iptr should be changeable during this processing) */
    iptr = s->iptr;
    optr = s->optr;
    if ((real_len = iptr - optr) < 0)
        real_len += s->len;
    /*endif*/
    if (real_len < len)
    {
        if (s->flags & QUEUE_READ_ATOMIC)
            return -1;
        /*endif*/
    }
    else
    {
        real_len = len;
    }
    /*endif*/
    if (real_len == 0)
        return 0;
    /*endif*/
    to_end = s->len - optr;
    if (iptr < optr  &&  to_end < real_len)
    {
        /* A two step process */
        if (buf)
        {
            memcpy(buf, &s->data[optr], to_end);
            memcpy(&buf[to_end], s->data, real_len - to_end);
        }
        /*endif*/
        new_optr = real_len - to_end;
    }
    else
    {
        /* A one step process */
        if (buf)
            memcpy(buf, &s->data[optr], real_len);
        /*endif*/
        new_optr = optr + real_len;
        if (new_optr >= s->len)
            new_optr = 0;
        /*endif*/
    }
    /*endif*/
    /* Only change the pointer now we have really finished */
    s->optr = new_optr;
    return real_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) queue_read_byte(queue_state_t *s)
{
    int real_len;
    int iptr;
    int optr;
    int byte;

    /* Snapshot the values (although only iptr should be changeable during this processing) */
    iptr = s->iptr;
    optr = s->optr;
    if ((real_len = iptr - optr) < 0)
        real_len += s->len;
    /*endif*/
    if (real_len < 1)
        return -1;
    /*endif*/
    byte = s->data[optr];
    if (++optr >= s->len)
        optr = 0;
    /*endif*/
    /* Only change the pointer now we have really finished */
    s->optr = optr;
    return byte;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) queue_write(queue_state_t *s, const uint8_t *buf, int len)
{
    int real_len;
    int to_end;
    int new_iptr;
    int iptr;
    int optr;

    /* Snapshot the values (although only optr should be changeable during this processing) */
    iptr = s->iptr;
    optr = s->optr;

    if ((real_len = optr - iptr - 1) < 0)
        real_len += s->len;
    /*endif*/
    if (real_len < len)
    {
        if (s->flags & QUEUE_WRITE_ATOMIC)
            return -1;
        /*endif*/
    }
    else
    {
        real_len = len;
    }
    /*endif*/
    if (real_len == 0)
        return 0;
    /*endif*/
    to_end = s->len - iptr;
    if (iptr < optr  ||  to_end >= real_len)
    {
        /* A one step process */
        memcpy(&s->data[iptr], buf, real_len);
        new_iptr = iptr + real_len;
        if (new_iptr >= s->len)
            new_iptr = 0;
        /*endif*/
    }
    else
    {
        /* A two step process */
        memcpy(&s->data[iptr], buf, to_end);
        memcpy(s->data, &buf[to_end], real_len - to_end);
        new_iptr = real_len - to_end;
    }
    /*endif*/
    /* Only change the pointer now we have really finished */
    s->iptr = new_iptr;
    return real_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) queue_write_byte(queue_state_t *s, uint8_t byte)
{
    int real_len;
    int iptr;
    int optr;

    /* Snapshot the values (although only optr should be changeable during this processing) */
    iptr = s->iptr;
    optr = s->optr;

    if ((real_len = optr - iptr - 1) < 0)
        real_len += s->len;
    /*endif*/
    if (real_len < 1)
    {
        if (s->flags & QUEUE_WRITE_ATOMIC)
            return -1;
        /*endif*/
        return 0;
    }
    /*endif*/
    s->data[iptr] = byte;
    if (++iptr >= s->len)
        iptr = 0;
    /*endif*/
    /* Only change the pointer now we have really finished */
    s->iptr = iptr;
    return 1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) queue_state_test_msg(queue_state_t *s)
{
    uint16_t lenx;

    if (queue_view(s, (uint8_t *) &lenx, sizeof(uint16_t)) != sizeof(uint16_t))
        return -1;
    /*endif*/
    return lenx;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) queue_read_msg(queue_state_t *s, uint8_t *buf, int len)
{
    uint16_t lenx;

    /* If we assume the write message was atomic, this read message should be
      safe when conducted in multiple chunks */
    if (queue_read(s, (uint8_t *) &lenx, sizeof(uint16_t)) != sizeof(uint16_t))
        return -1;
    /*endif*/
    /* If we got this far, the actual message chunk should be guaranteed to be
       available */
    if (lenx == 0)
        return 0;
    /*endif*/
    if ((int) lenx > len)
    {
        len = queue_read(s, buf, len);
        /* Discard the rest of the message */
        queue_read(s, NULL, lenx - len);
        return len;
    }
    /*endif*/
    return queue_read(s, buf, lenx);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) queue_write_msg(queue_state_t *s, const uint8_t *buf, int len)
{
    int real_len;
    int to_end;
    int new_iptr;
    int iptr;
    int optr;
    uint16_t lenx;

    /* Snapshot the values (although only optr should be changeable during this processing) */
    iptr = s->iptr;
    optr = s->optr;

    if ((real_len = optr - iptr - 1) < 0)
        real_len += s->len;
    /*endif*/
    if (real_len < len + (int) sizeof(uint16_t))
        return -1;
    /*endif*/
    real_len = len + (int) sizeof(uint16_t);

    to_end = s->len - iptr;
    lenx = (uint16_t) len;
    if (iptr < optr  ||  to_end >= real_len)
    {
        /* A one step process */
        memcpy(&s->data[iptr], &lenx, sizeof(uint16_t));
        memcpy(&s->data[iptr + sizeof(uint16_t)], buf, len);
        new_iptr = iptr + real_len;
        if (new_iptr >= s->len)
            new_iptr = 0;
        /*endif*/
    }
    else
    {
        /* A two step process */
        if (to_end >= sizeof(uint16_t))
        {
            /* The actual message wraps around the end of the buffer */
            memcpy(&s->data[iptr], &lenx, sizeof(uint16_t));
            memcpy(&s->data[iptr + sizeof(uint16_t)], buf, to_end - sizeof(uint16_t));
            memcpy(s->data, &buf[to_end - sizeof(uint16_t)], real_len - to_end);
        }
        else
        {
            /* The message length wraps around the end of the buffer */
            memcpy(&s->data[iptr], (uint8_t *) &lenx, to_end);
            memcpy(s->data, ((uint8_t *) &lenx) + to_end, sizeof(uint16_t) - to_end);
            memcpy(&s->data[sizeof(uint16_t) - to_end], buf, len);
        }
        new_iptr = real_len - to_end;
    }
    /*endif*/
    /* Only change the pointer now we have really finished */
    s->iptr = new_iptr;
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(queue_state_t *) queue_init(queue_state_t *s, int len, int flags)
{
    if (s == NULL)
    {
        if ((s = (queue_state_t *) span_alloc(sizeof(*s) + len + 1)) == NULL)
            return NULL;
    }
    s->iptr =
    s->optr = 0;
    s->flags = flags;
    s->len = len + 1;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) queue_release(queue_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) queue_free(queue_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
