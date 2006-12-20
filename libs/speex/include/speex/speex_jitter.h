/* Copyright (C) 2002 Jean-Marc Valin */
/**
   @file speex_jitter.h
   @brief Adaptive jitter buffer for Speex
*/
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   
   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
   
   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
   
   - Neither the name of the Xiph.org Foundation nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.
   
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef SPEEX_JITTER_H
#define SPEEX_JITTER_H

#include "speex.h"
#include "speex_bits.h"

#ifdef __cplusplus
extern "C" {
#endif

struct JitterBuffer_;

typedef struct JitterBuffer_ JitterBuffer;

typedef struct _JitterBufferPacket JitterBufferPacket;

struct _JitterBufferPacket {
   char        *data;
   spx_uint32_t len;
   spx_uint32_t timestamp;
   spx_uint32_t span;
};


#define JITTER_BUFFER_OK 0
#define JITTER_BUFFER_MISSING 1
#define JITTER_BUFFER_INCOMPLETE 2
#define JITTER_BUFFER_INTERNAL_ERROR -1
#define JITTER_BUFFER_BAD_ARGUMENT -2

/** Initialise jitter buffer */
JitterBuffer *jitter_buffer_init(int tick);

/** Reset jitter buffer */
void jitter_buffer_reset(JitterBuffer *jitter);

/** Destroy jitter buffer */
void jitter_buffer_destroy(JitterBuffer *jitter);

/** Put one packet into the jitter buffer */
void jitter_buffer_put(JitterBuffer *jitter, const JitterBufferPacket *packet);

/** Get one packet from the jitter buffer */
int jitter_buffer_get(JitterBuffer *jitter, JitterBufferPacket *packet, spx_uint32_t *current_timestamp);

/** Get pointer timestamp of jitter buffer */
int jitter_buffer_get_pointer_timestamp(JitterBuffer *jitter);

/** Advance by one tick */
void jitter_buffer_tick(JitterBuffer *jitter);


/** Speex jitter-buffer state. */
typedef struct SpeexJitter {
   SpeexBits current_packet;                                              /**< Current Speex packet                */
   int valid_bits;                                                        /**< True if Speex bits are valid        */
   JitterBuffer *packets;
   void *dec;                                                             /**< Pointer to Speex decoder            */
   spx_int32_t frame_size;                                                        /**< Frame size of Speex decoder         */
} SpeexJitter;

/** Initialise jitter buffer */
void speex_jitter_init(SpeexJitter *jitter, void *decoder, int sampling_rate);

/** Destroy jitter buffer */
void speex_jitter_destroy(SpeexJitter *jitter);

/** Put one packet into the jitter buffer */
void speex_jitter_put(SpeexJitter *jitter, char *packet, int len, int timestamp);

/** Get one packet from the jitter buffer */
void speex_jitter_get(SpeexJitter *jitter, spx_int16_t *out, int *start_offset);

/** Get pointer timestamp of jitter buffer */
int speex_jitter_get_pointer_timestamp(SpeexJitter *jitter);

#ifdef __cplusplus
}
#endif


#endif
