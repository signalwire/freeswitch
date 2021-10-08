/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/playout.h
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

#if !defined(_SPANDSP_PRIVATE_PLAYOUT_H_)
#define _SPANDSP_PRIVATE_PLAYOUT_H_

struct playout_frame_s
{
    /*! The actual frame data */
    void *data;
    /*! The type of frame */
    int type;
    /*! The timestamp assigned by the sending end */
    timestamp_t sender_stamp;
    /*! The timespan covered by the data in this frame */
    timestamp_t sender_len;
    /*! The timestamp assigned by the receiving end */
    timestamp_t receiver_stamp;
    /*! Pointer to the next earlier frame */
    struct playout_frame_s *earlier;
    /*! Pointer to the next later frame */
    struct playout_frame_s *later;
};

/*!
    Playout (jitter buffer) descriptor. This defines the working state
    for a single instance of playout buffering.
*/
struct playout_state_s
{
    /*! True if the buffer is dynamically sized */
    bool dynamic;
    /*! The minimum length (dynamic) or fixed length (static) of the buffer */
    int min_length;
    /*! The maximum length (dynamic) or fixed length (static) of the buffer */
    int max_length;
    /*! The target filter threshold for adjusting dynamic buffering. */
    int dropable_threshold;

    int start;

    /*! The queued frame list */
    playout_frame_t *first_frame;
    playout_frame_t *last_frame;
    /*! The free frame pool */
    playout_frame_t *free_frames;

    /*! The total frames input to the buffer, to date. */
    int frames_in;
    /*! The total frames output from the buffer, to date. */
    int frames_out;
    /*! The number of frames received out of sequence. */
    int frames_oos;
    /*! The number of frames which were discarded, due to late arrival. */
    int frames_late;
    /*! The number of frames which were never received. */
    int frames_missing;
    /*! The number of frames trimmed from the stream, due to buffer shrinkage. */
    int frames_trimmed;

    timestamp_t latest_expected;
    /*! The present jitter adjustment */
    timestamp_t current;
    /*! The sender_stamp of the last speech frame */
    timestamp_t last_speech_sender_stamp;
    /*! The duration of the last speech frame */
    timestamp_t last_speech_sender_len;

    int not_first;
    /*! The time since the target buffer length was last changed. */
    timestamp_t since_last_step;
    /*! Filter state for tracking the packets arriving just in time */
    int32_t state_just_in_time;
    /*! Filter state for tracking the packets arriving late */
    int32_t state_late;
    /*! The current target length of the buffer */
    int target_buffer_length;
    /*! The current actual length of the buffer, which may lag behind the target value */
    int actual_buffer_length;
};

#endif
/*- End of file ------------------------------------------------------------*/
