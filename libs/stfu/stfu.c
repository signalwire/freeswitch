/*
 * STFU (S)ort (T)ransportable (F)ramed (U)tterances
 * Copyright (c) 2007-2012 Anthony Minessale II <anthm@freeswitch.org>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * THOSE WHO DISAGREE MAY CERTAINLY STFU
 */
#include "stfu.h"

//#define DB_JB 1

#ifndef UINT_MAX
#  define UINT_MAX        4294967295U
#endif

#ifndef UINT16_MAX
#  define UINT16_MAX        65535
#endif

#ifdef _MSC_VER
/* warning C4706: assignment within conditional expression*/
#pragma warning(disable: 4706)
/* warning C4996: 'strdup': The POSIX name for this item is deprecated. Instead, use the ISO C++ conformant name: _strdup. See online help for details. */
#pragma warning(disable:4996)
#endif

#define least1(_z) (_z ? _z : 1)

static int stfu_log_level = 7;

struct stfu_queue {
	struct stfu_frame *array;
	struct stfu_frame int_frame;
	uint32_t real_array_size;
	uint32_t array_size;
	uint32_t array_len;	
	uint32_t wr_len;
    uint32_t last_index;
    int32_t last_jitter;
};
typedef struct stfu_queue stfu_queue_t;

struct stfu_instance {
	struct stfu_queue a_queue;
	struct stfu_queue b_queue;
	struct stfu_queue c_queue;
	struct stfu_queue *in_queue;
	struct stfu_queue *out_queue;
	struct stfu_queue *old_queue;
    struct stfu_frame *last_frame;
	uint32_t cur_ts;
	uint16_t cur_seq;
	uint32_t last_wr_ts;
	uint32_t last_rd_ts;
	uint32_t samples_per_packet;
	uint32_t samples_per_second;
	uint32_t miss_count;
	uint32_t max_plc;
    uint32_t qlen;
    uint32_t most_qlen;
    uint32_t max_qlen;
    uint32_t orig_qlen;
    uint32_t packet_count;
    uint32_t consecutive_good_count;
    uint32_t consecutive_bad_count;
    uint32_t period_good_count;
    uint32_t period_bad_count;
    uint32_t period_packet_in_count;
    uint32_t period_packet_out_count;
    uint32_t period_missing_count;

    uint32_t period_need_range;
    uint32_t period_need_range_avg;
    uint32_t period_clean_count;

    uint32_t session_clean_count;
    uint32_t session_missing_count;

    uint32_t session_packet_in_count;
    uint32_t session_packet_out_count;

    uint32_t sync_out;
    uint32_t sync_in;

    int32_t ts_offset;
    int32_t ts_drift;
    int32_t max_drift;
    uint32_t drift_dropped_packets;
    uint32_t drift_max_dropped;

    int32_t ts_diff;
    int32_t last_ts_diff;
    int32_t same_ts;
    
    uint32_t period_time;
    uint32_t decrement_time;
    
    uint32_t plc_len;
    uint32_t plc_pt;
    uint32_t diff;
    uint32_t diff_total;
    uint8_t ready;
    uint8_t debug;

    char *name;
    stfu_n_call_me_t callback;
    void *udata;
};

static void stfu_n_reset_counters(stfu_instance_t *i);
static void null_logger(const char *file, const char *func, int line, int level, const char *fmt, ...);
static void default_logger(const char *file, const char *func, int line, int level, const char *fmt, ...);

stfu_logger_t stfu_log = null_logger;

int32_t stfu_n_get_drift(stfu_instance_t *i)
{
    return i->ts_drift;
}

int32_t stfu_n_get_most_qlen(stfu_instance_t *i)
{
    return i->most_qlen;
}

void stfu_global_set_logger(stfu_logger_t logger)
{
	if (logger) {
		stfu_log = logger;
	} else {
		stfu_log = null_logger;
	}
}

void stfu_global_set_default_logger(int level)
{
	if (level < 0 || level > 7) {
		level = 7;
	}

	stfu_log = default_logger;
	stfu_log_level = level;
}



static stfu_status_t stfu_n_resize_aqueue(stfu_queue_t *queue, uint32_t qlen)
{
    unsigned char *m;

    if (qlen <= queue->real_array_size) {
        queue->array_size = qlen;
        if (queue->array_len > qlen) {
            queue->array_len = qlen;
        }
    } else {
        m = realloc(queue->array, qlen * sizeof(struct stfu_frame));
        assert(m);
        memset(m + queue->array_size * sizeof(struct stfu_frame), 0, (qlen * sizeof(struct stfu_frame)) - (queue->array_size * sizeof(struct stfu_frame)));
        queue->array = (struct stfu_frame *) m;
        queue->real_array_size = queue->array_size = qlen;
    }

	return STFU_IT_WORKED;
}

static void stfu_n_init_aqueue(stfu_queue_t *queue, uint32_t qlen)
{

	queue->array = calloc(qlen, sizeof(struct stfu_frame));
	assert(queue->array != NULL);
	memset(queue->array, 0, sizeof(struct stfu_frame) * qlen);	
	queue->real_array_size = queue->array_size = qlen;
	queue->int_frame.plc = 1;
    memset(queue->int_frame.data, 255, sizeof(queue->int_frame.data));
}


void stfu_n_call_me(stfu_instance_t *i, stfu_n_call_me_t callback, void *udata)
{
    i->callback = callback;
    i->udata = udata;
}

void stfu_n_destroy(stfu_instance_t **i)
{
	stfu_instance_t *ii;

	if (i && *i) {
		ii = *i;
		*i = NULL;
        if (ii->name) free(ii->name);
		free(ii->a_queue.array);
		free(ii->b_queue.array);
		free(ii->c_queue.array);
		free(ii);
	}
}

void stfu_n_debug(stfu_instance_t *i, const char *name)
{
    if (i->name) free(i->name);

    if (name) {
        i->name = strdup(name);
        i->debug = 1;
    } else {
        i->name = strdup("none");
        i->debug = 0;
    }
}

void stfu_n_report(stfu_instance_t *i, stfu_report_t *r)
{
    stfu_assert(i);
	r->qlen = i->qlen;
	r->packet_in_count = i->period_packet_in_count;
	r->clean_count = i->period_clean_count;
	r->consecutive_good_count = i->consecutive_good_count;
	r->consecutive_bad_count = i->consecutive_bad_count;
}

stfu_status_t stfu_n_resize(stfu_instance_t *i, uint32_t qlen) 
{
    stfu_status_t s;

    if (i->qlen == i->max_qlen) {
        return STFU_IT_FAILED;
    }
    
    if (i->max_qlen && qlen > i->max_qlen) {
        if (i->qlen < i->max_qlen) {
            qlen = i->max_qlen;
        } else {
            return STFU_IT_FAILED;
        }
    }

    if ((s = stfu_n_resize_aqueue(&i->a_queue, qlen)) == STFU_IT_WORKED) {
        s = stfu_n_resize_aqueue(&i->b_queue, qlen);
        s = stfu_n_resize_aqueue(&i->c_queue, qlen);

        if (qlen > i->most_qlen) {
            i->most_qlen = qlen;
        }

        i->qlen = qlen;
        i->max_plc = 5;
        i->last_frame = NULL;
    }
    
    return s;
}

stfu_instance_t *stfu_n_init(uint32_t qlen, uint32_t max_qlen, uint32_t samples_per_packet, uint32_t samples_per_second, uint32_t max_drift_ms)
{
	struct stfu_instance *i;

	i = malloc(sizeof(*i));
	if (!i) {
		return NULL;
	}
	memset(i, 0, sizeof(*i));

    i->qlen = qlen;
    i->max_qlen = max_qlen;
    i->orig_qlen = qlen;
    i->samples_per_packet = samples_per_packet;

	stfu_n_init_aqueue(&i->a_queue, qlen);
	stfu_n_init_aqueue(&i->b_queue, qlen);
	stfu_n_init_aqueue(&i->c_queue, qlen);

    i->max_drift = (int32_t)(max_drift_ms * (samples_per_second / 1000) * -1);

    if (max_drift_ms && samples_per_packet) {
        i->drift_max_dropped = (samples_per_second * 2) / samples_per_packet;
    }

	i->in_queue = &i->a_queue;
	i->out_queue = &i->b_queue;
	i->old_queue = &i->c_queue;
    i->name = strdup("none");
    
    i->max_plc = i->qlen / 2;

    i->samples_per_second = samples_per_second ? samples_per_second : 8000;
    
    i->period_time = ((i->samples_per_second * 20) / i->samples_per_packet);
    i->decrement_time = ((i->samples_per_second * 15) / i->samples_per_packet);

	return i;
}

static void stfu_n_reset_counters(stfu_instance_t *i)
{
    if (stfu_log != null_logger && i->debug) {
        stfu_log(STFU_LOG_EMERG, "%s COUNTER RESET........\n", i->name);
    }

    if (i->callback) {
        i->callback(i, i->udata);
    }

    i->consecutive_good_count = 0;
    i->consecutive_bad_count = 0;
    i->period_good_count = 0;
    i->period_clean_count = 0;
    i->period_bad_count = 0;
    i->period_packet_in_count = 0;
    i->period_packet_out_count = 0;
    i->period_missing_count = 0;

    i->period_need_range = 0;
    i->period_need_range_avg = 0;

    i->diff = 0;
    i->diff_total = 0;

}

void stfu_n_reset(stfu_instance_t *i)
{
    if (stfu_log != null_logger && i->debug) {
        stfu_log(STFU_LOG_EMERG, "%s RESET\n", i->name);
    }

    i->ready = 0;
	i->in_queue = &i->a_queue;
	i->out_queue = &i->b_queue;
	i->old_queue = &i->c_queue;

	i->in_queue->array_len = 0;
	i->out_queue->array_len = 0;
	i->out_queue->wr_len = 0;
	i->last_frame = NULL;
    i->in_queue->last_jitter = 0;
    i->out_queue->last_jitter = 0;


    stfu_n_reset_counters(i);
    stfu_n_sync(i, 1);
    
    i->cur_ts = 0;
    i->cur_seq = 0;
	i->last_wr_ts = 0;
	i->last_rd_ts = 0;
	i->miss_count = 0;	
    i->packet_count = 0;


}

stfu_status_t stfu_n_sync(stfu_instance_t *i, uint32_t packets)
{

    if (packets > i->qlen) {
        stfu_n_reset(i);
    } else {
        i->sync_out = packets;
        i->sync_in = packets;
    }

    return STFU_IT_WORKED;
}


static void stfu_n_swap(stfu_instance_t *i)
{
    stfu_queue_t *last_in = i->in_queue, *last_out = i->out_queue, *last_old = i->old_queue;
    
    i->ready = 1;
    
    i->in_queue = last_old;
    i->out_queue = last_in;
    i->old_queue = last_out;

    i->in_queue->array_len = 0;
    i->out_queue->wr_len = 0;
    i->last_frame = NULL;
    i->miss_count = 0;
    i->in_queue->last_index = 0;
    i->out_queue->last_index = 0;
    i->out_queue->last_jitter = 0;
}

stfu_status_t stfu_n_add_data(stfu_instance_t *i, uint32_t ts, uint16_t seq, uint32_t pt, void *data, size_t datalen, uint32_t timer_ts, int last)
{
	uint32_t index = 0;
	stfu_frame_t *frame;
	size_t cplen = 0;
    int good_ts = 0;

    if (!i->samples_per_packet && ts && i->last_rd_ts) {
        i->ts_diff = ts - i->last_rd_ts;

        if (i->last_ts_diff == i->ts_diff) {
            if (++i->same_ts == 5) {
                i->samples_per_packet = i->ts_diff;
                if (i->max_drift && i->samples_per_packet) {
                    i->drift_max_dropped = (i->samples_per_second * 2) / i->samples_per_packet;
                }
            }
        } else {
            i->same_ts = 0;
        }
            
        i->last_ts_diff = i->ts_diff;

        if (!i->samples_per_packet) {
            i->last_rd_ts = ts;
            return STFU_IT_FAILED;
        }
    }
 
    if (timer_ts) {
        if (ts && !i->ts_offset) {
            i->ts_offset = timer_ts - ts;
        }

        i->ts_drift = ts + (i->ts_offset - timer_ts);

        if (i->ts_offset && i->ts_drift > 0) {
            i->ts_offset = timer_ts - ts;
            i->ts_drift = ts + (i->ts_offset - timer_ts);
        }


        if (i->max_drift) {
            if (i->ts_drift < i->max_drift) {
                if (++i->drift_dropped_packets < i->drift_max_dropped) {
                    stfu_log(STFU_LOG_EMERG, "%s TOO LATE !!! %u \n\n\n", i->name, ts);
                    return STFU_ITS_TOO_LATE;
                }
            } else {
                i->drift_dropped_packets = 0;
            }
        }
    }

    if (i->sync_in) {
        good_ts = 1;
        i->sync_in = 0;
    } else {

        if ((ts && ts == i->last_rd_ts + i->samples_per_packet) || (i->last_rd_ts > 4294900000u && ts < 5000)) {
            good_ts = 1;
        }

        if (i->last_wr_ts) {
            if ((ts <= i->last_wr_ts && (i->last_wr_ts != UINT_MAX || ts == i->last_wr_ts))) {
                if (stfu_log != null_logger && i->debug) {
                    stfu_log(STFU_LOG_EMERG, "%s TOO LATE !!! %u \n\n\n", i->name, ts);
                }
                if (i->in_queue->array_len < i->in_queue->array_size) {
                    i->in_queue->array_len++;
                }
                return STFU_ITS_TOO_LATE;
            }
        }
    }

    if (good_ts) {
        i->period_clean_count++;
        i->session_clean_count++;
    }

    i->period_packet_in_count++;
    i->session_packet_in_count++;

    i->period_need_range_avg = i->period_need_range / least1(i->period_missing_count);

    if (i->period_missing_count > i->qlen * 2) {
        if (stfu_log != null_logger && i->debug) {
            stfu_log(STFU_LOG_EMERG, "%s resize %u %u\n", i->name, i->qlen, i->qlen + 1);
        }
        stfu_n_resize(i, i->qlen + 1);
        stfu_n_reset_counters(i);
    } else {
        if (i->qlen > i->orig_qlen && (i->consecutive_good_count > i->decrement_time || i->period_clean_count > i->decrement_time)) {
            stfu_n_resize(i, i->qlen - 1);
            stfu_n_reset_counters(i);
            stfu_n_sync(i, i->qlen);
        }
    }

    
    i->diff = 0;
    
    if (i->last_wr_ts) {
        if (ts < 1000 && i->last_wr_ts > (UINT_MAX - 1000)) {
            i->diff = abs(((UINT_MAX - i->last_wr_ts) + ts) / i->samples_per_packet);
        } else if (ts) {
            i->diff = abs(i->last_wr_ts - ts) / i->samples_per_packet;
        }
    }
    
    i->diff_total += i->diff;

    if ((i->period_packet_in_count > i->period_time)) {
        //uint32_t avg;

        //avg = i->diff_total / least1(i->period_packet_in_count);

        i->period_packet_in_count = 0;

        if (i->period_missing_count == 0 && i->qlen > i->orig_qlen) {
            stfu_n_resize(i, i->qlen - 1);
            stfu_n_sync(i, i->qlen);
        }

        stfu_n_reset_counters(i);
    }


    

    if (stfu_log != null_logger && i->debug) {
        stfu_log(STFU_LOG_EMERG, "I: %s %u/%u i=%u/%u - g:%u/%u c:%u/%u b:%u - %u:%u - %u %d %u %u %d %d %d/%d\n", i->name,
                 i->qlen, i->max_qlen, i->period_packet_in_count, i->period_time, i->consecutive_good_count, 
                 i->decrement_time, i->period_clean_count, i->decrement_time, i->consecutive_bad_count,
                 ts, ts / i->samples_per_packet, 
                 i->period_missing_count, i->period_need_range_avg,
                 i->last_wr_ts, ts, i->diff, i->diff_total / least1(i->period_packet_in_count), i->ts_drift, i->max_drift);
    }

	if (last || i->in_queue->array_len == i->in_queue->array_size) {
        stfu_n_swap(i);
    }

	if (last) {
		return STFU_IM_DONE;
	}

    index = i->in_queue->array_len++;
    assert(index < i->in_queue->array_size);
	frame = &i->in_queue->array[index];

	if (i->in_queue->array_len == i->in_queue->array_size) {
        stfu_n_swap(i);
    }

	if ((cplen = datalen) > sizeof(frame->data)) {
		cplen = sizeof(frame->data);
	}

    i->last_rd_ts = ts;
    i->packet_count++;

	memcpy(frame->data, data, cplen);

    frame->pt = pt;
	frame->ts = ts;
    frame->seq = seq;
	frame->dlen = cplen;
	frame->was_read = 0;	

	return STFU_IT_WORKED;
}

static int stfu_n_find_any_frame(stfu_instance_t *in, stfu_queue_t *queue, stfu_frame_t **r_frame)
{
    uint32_t i = 0;
    stfu_frame_t *frame = NULL;

    stfu_assert(r_frame);
    
    *r_frame = NULL;

    for(i = 0; i < queue->real_array_size; i++) {
        frame = &queue->array[i];
        if (!frame->was_read) {
            *r_frame = frame;
            queue->last_index = i;
            frame->was_read = 1;
            in->period_packet_out_count++;
            in->session_packet_out_count++;
            return 1;
        }
    }

    return 0;    
}


static int stfu_n_find_frame(stfu_instance_t *in, stfu_queue_t *queue, uint32_t min_ts, uint32_t max_ts, stfu_frame_t **r_frame)
{
    uint32_t i = 0;
    stfu_frame_t *frame = NULL;

    if (r_frame) {
        *r_frame = NULL;
    }

    for(i = 0; i < queue->array_size; i++) {
        frame = &queue->array[i];
        
        if (frame->ts == max_ts || (frame->ts > min_ts && frame->ts < max_ts)) {
            if (r_frame) {
                *r_frame = frame;
                queue->last_index = i;
                frame->was_read = 1;
                in->period_packet_out_count++;
                in->session_packet_out_count++;
            }
            return 1;
        }
    }

    return 0;
}

stfu_frame_t *stfu_n_read_a_frame(stfu_instance_t *i)
{
	stfu_frame_t *rframe = NULL;
    int found = 0;

	if (!i->samples_per_packet) {
        return NULL;
    }
    
    if (!i->ready) {
        if (stfu_log != null_logger && i->debug) {
            stfu_log(STFU_LOG_EMERG, "%s JITTERBUFFER NOT READY: IGNORING FRAME\n", i->name);
        }
        return NULL;
    }


    if (i->cur_ts == 0 && i->last_wr_ts < 1000) {
        uint32_t x = 0;
        for (x = 0; x < i->out_queue->array_len; x++) {
            if (!i->out_queue->array[x].was_read) {
                i->cur_ts = i->out_queue->array[x].ts;
                i->cur_seq = i->out_queue->array[x].seq;
                break;
            }
            if (i->cur_ts == 0) {
                if (stfu_log != null_logger && i->debug) {
                    stfu_log(STFU_LOG_EMERG, "%s JITTERBUFFER ERROR: PUNTING\n", i->name);
                    return NULL;
                }
            }
        }
    } else {
        i->cur_ts = i->cur_ts + i->samples_per_packet;
        i->cur_seq++;
    }
    
    found = stfu_n_find_frame(i, i->out_queue, i->last_wr_ts, i->cur_ts, &rframe);

    if (found) {
        if (i->out_queue->array_len) {
            i->out_queue->array_len--;
        }
    } else {
        found = stfu_n_find_frame(i, i->in_queue, i->last_wr_ts, i->cur_ts, &rframe);

        if (!found) {
            found = stfu_n_find_frame(i, i->old_queue, i->last_wr_ts, i->cur_ts, &rframe);
        }
    }

    if (found) {
        i->cur_ts = rframe->ts;
        i->cur_seq = rframe->seq;
    }

    if (i->sync_out) {
        if (!found) {
            if ((found = stfu_n_find_any_frame(i, i->out_queue, &rframe))) {
                i->cur_ts = rframe->ts;
                i->cur_seq = rframe->seq;
            }
            
            if (stfu_log != null_logger && i->debug) {
                stfu_log(STFU_LOG_EMERG, "%s SYNC %u %u:%u\n", i->name, i->sync_out, i->cur_ts, i->cur_ts / i->samples_per_packet);
            }

        }
        i->sync_out = 0;
    }

    if (!i->cur_ts) {
        if (stfu_log != null_logger && i->debug) {
            stfu_log(STFU_LOG_EMERG, "%s NO TS\n", i->name);
        }
        return NULL;
    }


    if (!found && i->samples_per_packet) {
        uint32_t y;
        stfu_frame_t *frame = NULL;

        int32_t delay = i->last_rd_ts - i->cur_ts;
        uint32_t need  = abs(i->last_rd_ts - i->cur_ts) / i->samples_per_packet;

        
        i->period_missing_count++;
        i->session_missing_count++;
        i->period_need_range += need;

        if (stfu_log != null_logger && i->debug) {        
            stfu_log(STFU_LOG_EMERG, "%s MISSING %u:%u %u %u %d %u %d\n", i->name, 
                     i->cur_ts, i->cur_ts / i->samples_per_packet, i->packet_count, i->last_rd_ts, delay, i->qlen, need);        
        }

        if (i->packet_count > i->orig_qlen * 100 && delay > 0 && need > i->qlen && need < (i->qlen + 5)) {
            i->packet_count = 0;
        }

        if (stfu_log != null_logger && i->debug) {        
            stfu_log(STFU_LOG_EMERG, "%s ", i->name);
            for(y = 0; y < i->out_queue->array_size; y++) {
                if ((y % 5) == 0) stfu_log(STFU_LOG_EMERG, "\n%s ", i->name);
                frame = &i->out_queue->array[y];
                stfu_log(STFU_LOG_EMERG, "%u:%u\t", frame->ts, frame->ts / i->samples_per_packet);
            }
            stfu_log(STFU_LOG_EMERG, "\n%s ", i->name);


            for(y = 0; y < i->in_queue->array_size; y++) {
                if ((y % 5) == 0) stfu_log(STFU_LOG_EMERG, "\n%s ", i->name);
                frame = &i->in_queue->array[y];
                stfu_log(STFU_LOG_EMERG, "%u:%u\t", frame->ts, frame->ts / i->samples_per_packet);
            }
            stfu_log(STFU_LOG_EMERG, "\n%s\n\n\n", i->name);

        }

        if (delay < 0) {
            stfu_n_reset(i);
            return NULL;
        }
    }

    if (stfu_log != null_logger && i->debug) {
        if (found) {
            stfu_log(STFU_LOG_EMERG, "%s O: %u:%u %u\n", i->name, rframe->ts, rframe->ts / i->samples_per_packet, rframe->plc);
        }
    }

    if (found) {
        i->consecutive_good_count++;
        i->period_good_count++;
        i->consecutive_bad_count = 0;
    } else {
        i->consecutive_bad_count++;
        i->period_bad_count++;
        i->consecutive_good_count = 0;
    }

    if (found) {
        i->last_frame = rframe;
        i->out_queue->wr_len++;
        i->last_wr_ts = rframe->ts;

        i->miss_count = 0;
        if (rframe->dlen) {
            i->plc_len = rframe->dlen;
        }

        i->plc_pt = rframe->pt;

    } else {
        i->last_wr_ts = i->cur_ts;
        rframe = &i->out_queue->int_frame;
        rframe->dlen = i->plc_len;
        rframe->pt = i->plc_pt;
        rframe->ts = i->cur_ts;
        rframe->seq = i->cur_seq;
        i->miss_count++;
        
        if (stfu_log != null_logger && i->debug) {
            stfu_log(STFU_LOG_EMERG, "%s PLC %d %d %ld %u:%u\n", i->name, 
                     i->miss_count, rframe->plc, rframe->dlen, rframe->ts, rframe->ts / i->samples_per_packet);
        }

        if (i->miss_count > i->max_plc) {
            stfu_n_reset(i);
            rframe = NULL;
        }
    }

    return rframe;
}

STFU_DECLARE(int32_t) stfu_n_copy_next_frame(stfu_instance_t *jb, uint32_t timestamp, uint16_t seq, uint16_t distance, stfu_frame_t *next_frame)
{
	uint32_t i = 0, j = 0;
#ifdef WIN32
#pragma warning (disable:4204)
#endif
	stfu_queue_t *queues[] = { jb->out_queue, jb->in_queue, jb->old_queue};
#ifdef WIN32
#pragma warning (default:4204)
#endif
	stfu_queue_t *queue = NULL;
	stfu_frame_t *frame = NULL;

	uint32_t target_ts = 0;

#ifdef WIN32
	UNREFERENCED_PARAMETER(seq);
#endif
	if (!next_frame) return 0;

	target_ts = timestamp + (distance - 1) * jb->samples_per_packet;

	for (i = 0; i < sizeof(queues)/sizeof(queues[0]); i++) {
		queue = queues[i];

		if (!queue) continue;

		for(j = 0; j < queue->array_size; j++) {
			frame = &queue->array[j];
			/* FIXME: ts rollover happened? bad luck */
			if (frame->ts > target_ts) {
				memcpy(next_frame, frame, sizeof(stfu_frame_t));
				return 1;
			}
		}
	}

	return 0;
}


#ifdef WIN32
#ifndef vsnprintf
#define vsnprintf _vsnprintf
#endif
#endif


int vasprintf(char **ret, const char *format, va_list ap);

int stfu_vasprintf(char **ret, const char *fmt, va_list ap)
{
#if !defined(WIN32) && !defined(__sun)
	return vasprintf(ret, fmt, ap);
#else
	char *buf;
	int len;
	size_t buflen;
	va_list ap2;
	char *tmp = NULL;

#ifdef _MSC_VER
#if _MSC_VER >= 1500
	/* hack for incorrect assumption in msvc header files for code analysis */
	__analysis_assume(tmp);
#endif
	ap2 = ap;
#else
	va_copy(ap2, ap);
#endif

	len = vsnprintf(tmp, 0, fmt, ap2);

	if (len > 0 && (buf = malloc((buflen = (size_t) (len + 1)))) != NULL) {
		len = vsnprintf(buf, buflen, fmt, ap);
		*ret = buf;
	} else {
		*ret = NULL;
		len = -1;
	}

	va_end(ap2);
	return len;
#endif
}




int stfu_snprintf(char *buffer, size_t count, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vsnprintf(buffer, count-1, fmt, ap);
	if (ret < 0)
		buffer[count-1] = '\0';
	va_end(ap);
	return ret;
}

static void null_logger(const char *file, const char *func, int line, int level, const char *fmt, ...)
{
	if (file && func && line && level && fmt) {
		return;
	}
	return;
}



static const char *LEVEL_NAMES[] = {
	"EMERG",
	"ALERT",
	"CRIT",
	"ERROR",
	"WARNING",
	"NOTICE",
	"INFO",
	"DEBUG",
	NULL
};

static const char *cut_path(const char *in)
{
	const char *p, *ret = in;
	char delims[] = "/\\";
	char *i;

	for (i = delims; *i; i++) {
		p = in;
		while ((p = strchr(p, *i)) != 0) {
			ret = ++p;
		}
	}
	return ret;
}


static void default_logger(const char *file, const char *func, int line, int level, const char *fmt, ...)
{
	const char *fp;
	char *data;
	va_list ap;
	int ret;
	
	if (level < 0 || level > 7) {
		level = 7;
	}
	if (level > stfu_log_level) {
		return;
	}
	
	fp = cut_path(file);

	va_start(ap, fmt);

	ret = stfu_vasprintf(&data, fmt, ap);

	if (ret != -1) {
		fprintf(stderr, "[%s] %s:%d %s() %s", LEVEL_NAMES[level], fp, line, func, data);
		free(data);
	}

	va_end(ap);

}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
