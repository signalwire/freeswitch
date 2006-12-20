/* Copyright (C) 2002 Jean-Marc Valin 
   File: speex_jitter.h

   Adaptive jitter buffer for Speex

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include "misc.h"
#include <speex/speex.h>
#include <speex/speex_bits.h>
#include <speex/speex_jitter.h>
#include <stdio.h>

#define LATE_BINS 10
#define MAX_MARGIN 30                     /**< Number of bins in margin histogram */

#define SPEEX_JITTER_MAX_BUFFER_SIZE 200   /**< Maximum number of packets in jitter buffer */



#define GT32(a,b) (((spx_int32_t)((a)-(b)))>0)
#define GE32(a,b) (((spx_int32_t)((a)-(b)))>=0)
#define LT32(a,b) (((spx_int32_t)((a)-(b)))<0)
#define LE32(a,b) (((spx_int32_t)((a)-(b)))<=0)

/** Jitter buffer structure */
struct JitterBuffer_ {
   spx_uint32_t pointer_timestamp;                                        /**< Timestamp of what we will *get* next */
   spx_uint32_t current_timestamp;                                        /**< Timestamp of the local clock (what we will *play* next) */

   char *buf[SPEEX_JITTER_MAX_BUFFER_SIZE];                               /**< Buffer of packets (NULL if slot is free) */
   spx_uint32_t timestamp[SPEEX_JITTER_MAX_BUFFER_SIZE];                  /**< Timestamp of packet                 */
   int span[SPEEX_JITTER_MAX_BUFFER_SIZE];                                /**< Timestamp of packet                 */
   int len[SPEEX_JITTER_MAX_BUFFER_SIZE];                                 /**< Number of bytes in packet           */

   int tick_size;                                                         /**< Output granularity                  */
   int reset_state;                                                       /**< True if state was just reset        */
   int buffer_margin;                                                     /**< How many frames we want to keep in the buffer (lower bound) */
   
   int lost_count;                                                        /**< Number of consecutive lost packets  */
   float shortterm_margin[MAX_MARGIN];                                    /**< Short term margin histogram         */
   float longterm_margin[MAX_MARGIN];                                     /**< Long term margin histogram          */
   float loss_rate;                                                       /**< Average loss rate                   */
};

/** Initialise jitter buffer */
JitterBuffer *jitter_buffer_init(int tick)
{
   JitterBuffer *jitter = (JitterBuffer*)speex_alloc(sizeof(JitterBuffer));
   if (jitter)
   {
      int i;
      for (i=0;i<SPEEX_JITTER_MAX_BUFFER_SIZE;i++)
         jitter->buf[i]=NULL;
      jitter->tick_size = tick;
      jitter->buffer_margin = 1;
      jitter_buffer_reset(jitter);
   }
   return jitter;
}

/** Reset jitter buffer */
void jitter_buffer_reset(JitterBuffer *jitter)
{
   int i;
   for (i=0;i<SPEEX_JITTER_MAX_BUFFER_SIZE;i++)
   {
      if (jitter->buf[i])
      {
         speex_free(jitter->buf[i]);
         jitter->buf[i] = NULL;
      }
   }
   /* Timestamp is actually undefined at this point */
   jitter->pointer_timestamp = 0;
   jitter->current_timestamp = 0;
   jitter->reset_state = 1;
   jitter->lost_count = 0;
   jitter->loss_rate = 0;
   for (i=0;i<MAX_MARGIN;i++)
   {
      jitter->shortterm_margin[i] = 0;
      jitter->longterm_margin[i] = 0;
   }
   /*fprintf (stderr, "reset\n");*/
}

/** Destroy jitter buffer */
void jitter_buffer_destroy(JitterBuffer *jitter)
{
   jitter_buffer_reset(jitter);
   speex_free(jitter);
}

/** Put one packet into the jitter buffer */
void jitter_buffer_put(JitterBuffer *jitter, const JitterBufferPacket *packet)
{
   int i,j;
   spx_int32_t arrival_margin;
   /*fprintf (stderr, "put packet %d %d\n", timestamp, span);*/
   if (jitter->reset_state)
   {
      jitter->reset_state=0;
      jitter->pointer_timestamp = packet->timestamp;
      jitter->current_timestamp = packet->timestamp;
      /*fprintf(stderr, "reset to %d\n", timestamp);*/
   }
   
   /* Cleanup buffer (remove old packets that weren't played) */
   for (i=0;i<SPEEX_JITTER_MAX_BUFFER_SIZE;i++)
   {
      if (jitter->buf[i] && LE32(jitter->timestamp[i] + jitter->span[i], jitter->pointer_timestamp))
      {
         /*fprintf (stderr, "cleaned (not played)\n");*/
         speex_free(jitter->buf[i]);
         jitter->buf[i] = NULL;
      }
   }

   /*Find an empty slot in the buffer*/
   for (i=0;i<SPEEX_JITTER_MAX_BUFFER_SIZE;i++)
   {
      if (jitter->buf[i]==NULL)
         break;
   }

   /*fprintf(stderr, "%d %d %f\n", timestamp, jitter->pointer_timestamp, jitter->drift_average);*/
   /*No place left in the buffer*/
   if (i==SPEEX_JITTER_MAX_BUFFER_SIZE)
   {
      int earliest=jitter->timestamp[0];
      i=0;
      for (j=1;j<SPEEX_JITTER_MAX_BUFFER_SIZE;j++)
      {
         if (!jitter->buf[i] || LT32(jitter->timestamp[j],earliest))
         {
            earliest = jitter->timestamp[j];
            i=j;
         }
      }
      speex_free(jitter->buf[i]);
      jitter->buf[i]=NULL;
      if (jitter->lost_count>20)
      {
         jitter_buffer_reset(jitter);
      }
      /*fprintf (stderr, "Buffer is full, discarding earliest frame %d (currently at %d)\n", timestamp, jitter->pointer_timestamp);*/      
   }
   
   /* Copy packet in buffer */
   jitter->buf[i]=(char*)speex_alloc(packet->len);
   for (j=0;j<packet->len;j++)
      jitter->buf[i][j]=packet->data[j];
   jitter->timestamp[i]=packet->timestamp;
   jitter->span[i]=packet->span;
   jitter->len[i]=packet->len;
   
   /* Adjust the buffer size depending on network conditions */
   arrival_margin = (packet->timestamp - jitter->current_timestamp) - jitter->buffer_margin*jitter->tick_size;
   
   if (arrival_margin >= -LATE_BINS*jitter->tick_size)
   {
      spx_int32_t int_margin;
      for (i=0;i<MAX_MARGIN;i++)
      {
         jitter->shortterm_margin[i] *= .98;
         jitter->longterm_margin[i] *= .995;
      }
      int_margin = LATE_BINS + arrival_margin/jitter->tick_size;
      if (int_margin>MAX_MARGIN-1)
         int_margin = MAX_MARGIN-1;
      if (int_margin>=0)
      {
         jitter->shortterm_margin[int_margin] += .02;
         jitter->longterm_margin[int_margin] += .005;
      }
   } else {
      
      /*fprintf (stderr, "way too late = %d\n", arrival_margin);*/
      if (jitter->lost_count>20)
      {
         jitter_buffer_reset(jitter);
      }
   }
#if 0 /* Enable to check how much is being buffered */
   if (rand()%1000==0)
   {
      int count = 0;
      for (j=0;j<SPEEX_JITTER_MAX_BUFFER_SIZE;j++)
      {
         if (jitter->buf[j])
            count++;
      }
      fprintf (stderr, "buffer_size = %d\n", count);
   }
#endif
}

/** Get one packet from the jitter buffer */
int jitter_buffer_get(JitterBuffer *jitter, JitterBufferPacket *packet, spx_uint32_t *start_offset)
{
   int i, j;
   float late_ratio_short;
   float late_ratio_long;
   float ontime_ratio_short;
   float ontime_ratio_long;
   float early_ratio_short;
   float early_ratio_long;
   int chunk_size;
   int incomplete = 0;
   
   if (LT32(jitter->current_timestamp+jitter->tick_size, jitter->pointer_timestamp))
   {
      jitter->current_timestamp = jitter->pointer_timestamp;
      speex_warning("did you forget to call jitter_buffer_tick() by any chance?");
   }
   /*fprintf (stderr, "get packet %d %d\n", jitter->pointer_timestamp, jitter->current_timestamp);*/

   /* FIXME: This should be only what remaining of the current tick */
   chunk_size = jitter->tick_size;
   
   /* Compiling arrival statistics */
   
   late_ratio_short = 0;
   late_ratio_long = 0;
   for (i=0;i<LATE_BINS;i++)
   {
      late_ratio_short += jitter->shortterm_margin[i];
      late_ratio_long += jitter->longterm_margin[i];
   }
   ontime_ratio_short = jitter->shortterm_margin[LATE_BINS];
   ontime_ratio_long = jitter->longterm_margin[LATE_BINS];
   early_ratio_short = early_ratio_long = 0;
   for (i=LATE_BINS+1;i<MAX_MARGIN;i++)
   {
      early_ratio_short += jitter->shortterm_margin[i];
      early_ratio_long += jitter->longterm_margin[i];
   }
   if (0&&jitter->pointer_timestamp%1000==0)
   {
      /*fprintf (stderr, "%f %f %f %f %f %f\n", early_ratio_short, early_ratio_long, ontime_ratio_short, ontime_ratio_long, late_ratio_short, late_ratio_long);*/
      /*fprintf (stderr, "%f %f\n", early_ratio_short + ontime_ratio_short + late_ratio_short, early_ratio_long + ontime_ratio_long + late_ratio_long);*/
   }
   
   /* Adjusting the buffering */
   
   if (late_ratio_short > .1 || late_ratio_long > .03)
   {
      /* If too many packets are arriving late */
      jitter->shortterm_margin[MAX_MARGIN-1] += jitter->shortterm_margin[MAX_MARGIN-2];
      jitter->longterm_margin[MAX_MARGIN-1] += jitter->longterm_margin[MAX_MARGIN-2];
      for (i=MAX_MARGIN-3;i>=0;i--)
      {
         jitter->shortterm_margin[i+1] = jitter->shortterm_margin[i];
         jitter->longterm_margin[i+1] = jitter->longterm_margin[i];         
      }
      jitter->shortterm_margin[0] = 0;
      jitter->longterm_margin[0] = 0;            
      jitter->pointer_timestamp -= jitter->tick_size;
      jitter->current_timestamp -= jitter->tick_size;
      /*fprintf (stderr, "i");*/
      /*fprintf (stderr, "interpolate (getting some slack)\n");*/
   } else if (late_ratio_short + ontime_ratio_short < .005 && late_ratio_long + ontime_ratio_long < .01 && early_ratio_short > .8)
   {
      /* Many frames arriving early */
      jitter->shortterm_margin[0] += jitter->shortterm_margin[1];
      jitter->longterm_margin[0] += jitter->longterm_margin[1];
      for (i=1;i<MAX_MARGIN-1;i++)
      {
         jitter->shortterm_margin[i] = jitter->shortterm_margin[i+1];
         jitter->longterm_margin[i] = jitter->longterm_margin[i+1];         
      }
      jitter->shortterm_margin[MAX_MARGIN-1] = 0;
      jitter->longterm_margin[MAX_MARGIN-1] = 0;      
      /*fprintf (stderr, "drop frame\n");*/
      /*fprintf (stderr, "d");*/
      jitter->pointer_timestamp += jitter->tick_size;
      jitter->current_timestamp += jitter->tick_size;
      /*fprintf (stderr, "dropping packet (getting more aggressive)\n");*/
   }
   
   /* Searching for the packet that fits best */
   
   /* Search the buffer for a packet with the right timestamp and spanning the whole current chunk */
   for (i=0;i<SPEEX_JITTER_MAX_BUFFER_SIZE;i++)
   {
      if (jitter->buf[i] && jitter->timestamp[i]==jitter->pointer_timestamp && GE32(jitter->timestamp[i]+jitter->span[i],jitter->pointer_timestamp+chunk_size))
         break;
   }
   
   /* If no match, try for an "older" packet that still spans (fully) the current chunk */
   if (i==SPEEX_JITTER_MAX_BUFFER_SIZE)
   {
      for (i=0;i<SPEEX_JITTER_MAX_BUFFER_SIZE;i++)
      {
         if (jitter->buf[i] && jitter->timestamp[i]<=jitter->pointer_timestamp && GE32(jitter->timestamp[i]+jitter->span[i],jitter->pointer_timestamp+chunk_size))
            break;
      }
   }
   
   /* If still no match, try for an "older" packet that spans part of the current chunk */
   if (i==SPEEX_JITTER_MAX_BUFFER_SIZE)
   {
      for (i=0;i<SPEEX_JITTER_MAX_BUFFER_SIZE;i++)
      {
         if (jitter->buf[i] && jitter->timestamp[i]<=jitter->pointer_timestamp && GT32(jitter->timestamp[i]+jitter->span[i],jitter->pointer_timestamp))
            break;
      }
   }
   
   /* If still no match, try for earliest packet possible */
   if (i==SPEEX_JITTER_MAX_BUFFER_SIZE)
   {
      int found = 0;
      spx_uint32_t best_time=0;
      int best_span=0;
      int besti=0;
      for (i=0;i<SPEEX_JITTER_MAX_BUFFER_SIZE;i++)
      {
         /* check if packet starts within current chunk */
         if (jitter->buf[i] && LT32(jitter->timestamp[i],jitter->pointer_timestamp+chunk_size) && GE32(jitter->timestamp[i],jitter->pointer_timestamp))
         {
            if (!found || LT32(jitter->timestamp[i],best_time) || (jitter->timestamp[i]==best_time && GT32(jitter->span[i],best_span)))
            {
               best_time = jitter->timestamp[i];
               best_span = jitter->span[i];
               besti = i;
               found = 1;
            }
         }
      }
      if (found)
      {
         i=besti;
         incomplete = 1;
         /*fprintf (stderr, "incomplete: %d %d %d %d\n", jitter->timestamp[i], jitter->pointer_timestamp, chunk_size, jitter->span[i]);*/
      }
   }

   /* If we find something */
   if (i!=SPEEX_JITTER_MAX_BUFFER_SIZE)
   {
      /* We (obviously) haven't lost this packet */
      jitter->lost_count = 0;
      jitter->loss_rate = .999*jitter->loss_rate;
      /* Check for potential overflow */
      packet->len = jitter->len[i];
      /* Copy packet */
      for (j=0;j<packet->len;j++)
         packet->data[j] = jitter->buf[i][j];
      /* Remove packet */
      speex_free(jitter->buf[i]);
      jitter->buf[i] = NULL;
      /* Set timestamp and span (if requested) */
      if (start_offset)
         *start_offset = jitter->timestamp[i]-jitter->pointer_timestamp;
      packet->timestamp = jitter->timestamp[i];
      packet->span = jitter->span[i];
      /* Point at the end of the current packet */
      jitter->pointer_timestamp = jitter->timestamp[i]+jitter->span[i];
      if (incomplete)
         return JITTER_BUFFER_INCOMPLETE;
      else
         return JITTER_BUFFER_OK;
   }
   
   
   /* If we haven't found anything worth returning */
   /*fprintf (stderr, "not found\n");*/
   jitter->lost_count++;
   /*fprintf (stderr, "m");*/
   /*fprintf (stderr, "lost_count = %d\n", jitter->lost_count);*/
   jitter->loss_rate = .999*jitter->loss_rate + .001;
   if (start_offset)
      *start_offset = 0;
   packet->timestamp = jitter->pointer_timestamp;
   packet->span = jitter->tick_size;
   jitter->pointer_timestamp += chunk_size;
   packet->len = 0;
   return JITTER_BUFFER_MISSING;

}

/** Get pointer timestamp of jitter buffer */
int jitter_buffer_get_pointer_timestamp(JitterBuffer *jitter)
{
   return jitter->pointer_timestamp;
}

void jitter_buffer_tick(JitterBuffer *jitter)
{
   jitter->current_timestamp += jitter->tick_size;
}





void speex_jitter_init(SpeexJitter *jitter, void *decoder, int sampling_rate)
{
   jitter->dec = decoder;
   speex_decoder_ctl(decoder, SPEEX_GET_FRAME_SIZE, &jitter->frame_size);

   jitter->packets = jitter_buffer_init(jitter->frame_size);

   speex_bits_init(&jitter->current_packet);
   jitter->valid_bits = 0;

}

void speex_jitter_destroy(SpeexJitter *jitter)
{
   jitter_buffer_destroy(jitter->packets);
   speex_bits_destroy(&jitter->current_packet);
}

void speex_jitter_put(SpeexJitter *jitter, char *packet, int len, int timestamp)
{
   JitterBufferPacket p;
   p.data = packet;
   p.len = len;
   p.timestamp = timestamp;
   p.span = jitter->frame_size;
   jitter_buffer_put(jitter->packets, &p);
}

void speex_jitter_get(SpeexJitter *jitter, short *out, int *current_timestamp)
{
   int i;
   int ret;
   char data[2048];
   JitterBufferPacket packet;
   packet.data = data;
   
   if (jitter->valid_bits)
   {
      /* Try decoding last received packet */
      ret = speex_decode_int(jitter->dec, &jitter->current_packet, out);
      if (ret == 0)
      {
         jitter_buffer_tick(jitter->packets);
         return;
      } else {
         jitter->valid_bits = 0;
      }
   }

   ret = jitter_buffer_get(jitter->packets, &packet, NULL);
   
   if (ret != JITTER_BUFFER_OK)
   {
      /* No packet found */

      /*fprintf (stderr, "lost/late frame\n");*/
      /*Packet is late or lost*/
      speex_decode_int(jitter->dec, NULL, out);
   } else {
      speex_bits_read_from(&jitter->current_packet, packet.data, packet.len);
      /* Decode packet */
      ret = speex_decode_int(jitter->dec, &jitter->current_packet, out);
      if (ret == 0)
      {
         jitter->valid_bits = 1;
      } else {
         /* Error while decoding */
         for (i=0;i<jitter->frame_size;i++)
            out[i]=0;
      }
   }
   jitter_buffer_tick(jitter->packets);
}

int speex_jitter_get_pointer_timestamp(SpeexJitter *jitter)
{
   return jitter_buffer_get_pointer_timestamp(jitter->packets);
}
