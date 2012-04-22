/*
 * SpanDSP - a series of DSP components for telephony
 *
 * playout.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005 Steve Underwood
 *
 * All rights reserved.
 *
 * This was kicked off from jitter buffering code
 *      Copyright (C) 2004, Horizon Wimba, Inc.
 *      Author Steve Kann <stevek@stevek.com>
 * However, there isn't a lot of the original left, now. The original
 * was licenced under the LGPL, so any remaining fragments are
 * compatible with the GPL licence used here.
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "spandsp/telephony.h"
#include "spandsp/playout.h"

static playout_frame_t *queue_get(playout_state_t *s, timestamp_t sender_stamp)
{
    playout_frame_t *frame;

    if ((frame = s->first_frame) == NULL)
        return NULL;

    if (sender_stamp >= frame->sender_stamp)
    {
        /* Remove this frame from the queue */
        if (frame->later)
        {
            frame->later->earlier = NULL;
            s->first_frame = frame->later;
        }
        else
        {
            /* The queue is now empty */
            s->first_frame = NULL;
            s->last_frame = NULL;
        }
        return frame;
    } 

    return NULL;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(timestamp_t) playout_next_due(playout_state_t *s)
{
    return s->last_speech_sender_stamp + s->last_speech_sender_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(timestamp_t) playout_current_length(playout_state_t *s)
{
    return s->target_buffer_length;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(playout_frame_t *) playout_get_unconditional(playout_state_t *s)
{
    playout_frame_t *frame;
    
    if ((frame = queue_get(s, 0x7FFFFFFF)))
    {
        /* Put it on the free list */
        frame->later = s->free_frames;
        s->free_frames = frame;

        /* We return the frame pointer, even though it's on the free list.
           The caller *must* copy the data before this frame has any chance
           of being reused. */
    }
    return frame;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) playout_get(playout_state_t *s, playout_frame_t *frameout, timestamp_t now)
{
    playout_frame_t *frame;

    /* Make the last_speech_sender_stamp the current expected one. */
    s->last_speech_sender_stamp += s->last_speech_sender_len;
    if ((frame = queue_get(s, s->last_speech_sender_stamp)) == NULL)
    {
        /* The required frame was not received (or at least not in time) */
        s->frames_missing++;
        return PLAYOUT_FILLIN;
    }

    if (s->dynamic  &&  frame->type == PLAYOUT_TYPE_SPEECH)
    {
        /* Assess whether the buffer length is appropriate */
        if (!s->not_first)
        {
            /* Prime things the first time through */
            s->not_first = TRUE;
            s->latest_expected = frame->receiver_stamp + s->min_length;
        }
        /* Leaky integrate the rate of occurance of frames received just in time and late */
        s->state_late += ((((frame->receiver_stamp > s->latest_expected)  ?  0x10000000  :  0) - s->state_late) >> 8);
        s->state_just_in_time += ((((frame->receiver_stamp > s->latest_expected - frame->sender_len)  ?  0x10000000  :  0) - s->state_just_in_time) >> 8);
        s->latest_expected += frame->sender_len;
        
        if (s->state_late > s->dropable_threshold)
        {
            if (s->since_last_step < 10)
            {
                if (s->target_buffer_length < s->max_length - 2)
                {
                    /* The late bin is too big - increase buffering */
                    s->target_buffer_length += 3*frame->sender_len;
                    s->latest_expected += 3*frame->sender_len;
                    s->state_just_in_time = s->dropable_threshold;
                    s->state_late = 0;
                    s->since_last_step = 0;

                    s->last_speech_sender_stamp -= 3*s->last_speech_sender_len;
                }
            }
            else
            {
                if (s->target_buffer_length < s->max_length)
                {
                    /* The late bin is too big - increase buffering */
                    s->target_buffer_length += frame->sender_len;
                    s->latest_expected += frame->sender_len;
                    s->state_just_in_time = s->dropable_threshold;
                    s->state_late = 0;
                    s->since_last_step = 0;

                    s->last_speech_sender_stamp -= s->last_speech_sender_len;
                }
            }
        }
        else if (s->since_last_step > 500  &&  s->state_just_in_time < s->dropable_threshold)
        {
            if (s->target_buffer_length > s->min_length)
            {
                /* The just-in-time bin is pretty small - decrease buffering */
                s->target_buffer_length -= frame->sender_len;
                s->latest_expected -= frame->sender_len;
                s->state_just_in_time = s->dropable_threshold;
                s->state_late = 0;
                s->since_last_step = 0;
    
                s->last_speech_sender_stamp += s->last_speech_sender_len;
            }
        }
        s->since_last_step++;
    }

    /* If its not a speech frame, just return it. */
    if (frame->type != PLAYOUT_TYPE_SPEECH)
    {
        /* Rewind last_speech_sender_stamp, since this isn't speech */
        s->last_speech_sender_stamp -= s->last_speech_sender_len;
            
        *frameout = *frame;
        /* Put it on the free list */
        frame->later = s->free_frames;
        s->free_frames = frame;
        
        s->frames_out++;
        return PLAYOUT_OK;
    }
    if (frame->sender_stamp < s->last_speech_sender_stamp)
    {
        /* This speech frame is late */
        *frameout = *frame;
        /* Put it on the free list */
        frame->later = s->free_frames;
        s->free_frames = frame;

        /* Rewind last_speech_sender_stamp, since we're just dumping */
        s->last_speech_sender_stamp -= s->last_speech_sender_len;
        s->frames_out++;
        s->frames_late++;
        s->frames_missing--;
        return PLAYOUT_DROP;
    }
    /* Keep track of frame sizes, to allow for variable sized frames */
    if (frame->sender_len > 0)
        s->last_speech_sender_len = frame->sender_len;

    /* Normal case. Return the frame, and increment stuff */
    *frameout = *frame;
    /* Put it on the free list */
    frame->later = s->free_frames;
    s->free_frames = frame;

    s->frames_out++;
    return PLAYOUT_OK;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) playout_put(playout_state_t *s, void *data, int type, timestamp_t sender_len, timestamp_t sender_stamp, timestamp_t receiver_stamp)
{
    playout_frame_t *frame;
    playout_frame_t *p;

    /* When a frame arrives we just queue it in order. We leave all the tricky stuff until frames
       are read from the queue. */
    s->frames_in++;

    /* Acquire a frame */
    if ((frame = s->free_frames))
    {
        s->free_frames = frame->later;
    }
    else
    {
        if ((frame = (playout_frame_t *) malloc(sizeof(*frame))) == NULL)
            return PLAYOUT_ERROR;
    }

    /* Fill out the frame */
    frame->data = data;
    frame->type = type;
    frame->sender_stamp = sender_stamp;
    frame->sender_len = sender_len;
    frame->receiver_stamp = receiver_stamp;

    /* Frames are kept in a list, sorted by the timestamp assigned by the sender. */
    if (s->last_frame == NULL)
    {
        /* The queue is empty. */
        frame->later = NULL;
        frame->earlier = NULL;
        s->first_frame = frame;
        s->last_frame = frame;
    }
    else if (sender_stamp >= s->last_frame->sender_stamp)
    {
        /* Frame goes at the end of the queue. */
        frame->later = NULL;
        frame->earlier = s->last_frame;
        s->last_frame->later = frame;
        s->last_frame = frame;
    }
    else
    {
        /* Frame is out of sequence. */
        s->frames_oos++;

        /* Find where it should go in the queue */
        p = s->last_frame;
        while (sender_stamp < p->sender_stamp  &&  p->earlier) 
            p = p->earlier;

        if (p->earlier)
        {
            /* It needs to go somewhere in the queue */
            frame->later = p->later;
            frame->earlier = p;
            p->later->earlier = frame;
            p->later = frame;
        }
        else
        {
            /* It needs to go at the very beginning of the queue */
            frame->later = p;
            frame->earlier = NULL;
            p->earlier = frame;
            s->first_frame = frame;
        }
    }

    if (s->start  &&  type == PLAYOUT_TYPE_SPEECH)
    {
        s->last_speech_sender_stamp = sender_stamp - sender_len - s->min_length;
        s->last_speech_sender_len = sender_len;
        s->start = FALSE;
    }

    return PLAYOUT_OK;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) playout_restart(playout_state_t *s, int min_length, int max_length)
{
    playout_frame_t *frame;
    playout_frame_t *next;

    /* Free all the frames on the free list */
    for (frame = s->free_frames;  frame;  frame = next)
    {
        next = frame->later;
        free(frame);
    }

    memset(s, 0, sizeof(*s));
    s->dynamic = (min_length < max_length);
    s->min_length = min_length;
    s->max_length = (max_length > min_length)  ?  max_length  :  min_length;
    s->dropable_threshold = 1*0x10000000/100;
    s->start = TRUE;
    s->since_last_step = 0x7FFFFFFF;
    /* Start with the minimum buffer length allowed, and work from there */
    s->actual_buffer_length = 
    s->target_buffer_length = (s->max_length - s->min_length)/2;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(playout_state_t *) playout_init(int min_length, int max_length)
{
    playout_state_t *s;

    if ((s = (playout_state_t *) malloc(sizeof(playout_state_t))) == NULL)
        return NULL;
    memset(s, 0, sizeof(*s));
    playout_restart(s, min_length, max_length);
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) playout_release(playout_state_t *s)
{
    playout_frame_t *frame;
    playout_frame_t *next;
    
    /* Free all the frames in the queue. In most cases these should have been
       removed already, so their associated data could be freed. */
    for (frame = s->first_frame;  frame;  frame = next)
    {
        next = frame->later;
        free(frame);
    }
    /* Free all the frames on the free list */
    for (frame = s->free_frames;  frame;  frame = next)
    {
        next = frame->later;
        free(frame);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) playout_free(playout_state_t *s)
{
    if (s)
    {
        playout_release(s);
        /* Finally, free ourselves! */ 
        free(s);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
