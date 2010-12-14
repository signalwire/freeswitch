/*
 * STFU (S)ort (T)ransportable (F)ramed (U)tterances
 * Copyright (c) 2007 Anthony Minessale II <anthm@freeswitch.org>
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
#ifdef _MSC_VER
/* warning C4706: assignment within conditional expression*/
#pragma warning(disable: 4706)
#endif

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
	struct stfu_queue *in_queue;
	struct stfu_queue *out_queue;
    struct stfu_frame *last_frame;
	uint32_t cur_ts;
	uint32_t cur_seq;
	uint32_t last_wr_ts;
	uint32_t last_wr_seq;
	uint32_t last_rd_ts;
	uint32_t samples_per_packet;
	uint32_t samples_per_second;
	uint32_t miss_count;
	uint32_t max_plc;
    uint32_t qlen;
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

    uint32_t sync;


    int32_t ts_diff;
    int32_t last_ts_diff;
    int32_t same_ts;
    
    uint32_t last_seq;

    uint32_t period_time;
    uint32_t decrement_time;
    
    uint32_t plc_len;

    stfu_n_call_me_t callback;
    void *udata;
};

static void stfu_n_reset_counters(stfu_instance_t *i);

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
		free(ii->a_queue.array);
		free(ii->b_queue.array);
		free(ii);
	}
}

void stfu_n_report(stfu_instance_t *i, stfu_report_t *r)
{
    assert(i);
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
        i->qlen = qlen;
        i->max_plc = 5;
        i->last_frame = NULL;
    }
    
    return s;
}

stfu_instance_t *stfu_n_init(uint32_t qlen, uint32_t max_qlen, uint32_t samples_per_packet, uint32_t samples_per_second)
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
	i->in_queue = &i->a_queue;
	i->out_queue = &i->b_queue;
    
    i->max_plc = i->qlen / 2;

    i->samples_per_second = samples_per_second ? samples_per_second : 8000;
    
    i->period_time = ((i->samples_per_second * 20) / i->samples_per_packet);
    i->decrement_time = ((i->samples_per_second * 15) / i->samples_per_packet);

	return i;
}

static void stfu_n_reset_counters(stfu_instance_t *i)
{
#ifdef DB_JB
    printf("COUNTER RESET........\n");
#endif

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
}

void stfu_n_reset(stfu_instance_t *i)
{
#ifdef DB_JB
    printf("RESET\n");
#endif
	i->in_queue = &i->a_queue;
	i->out_queue = &i->b_queue;
	i->in_queue->array_len = 0;
	i->out_queue->array_len = 0;
	i->out_queue->wr_len = 0;
	i->last_frame = NULL;

    i->in_queue->last_jitter = 0;
    i->out_queue->last_jitter = 0;

    stfu_n_reset_counters(i);

    i->last_seq = 0;

    i->cur_ts = 0;
    i->cur_seq = 0;
	i->last_wr_ts = 0;
	i->last_wr_seq = 0;
	i->last_rd_ts = 0;
	i->miss_count = 0;	
    i->packet_count = 0;
}

stfu_status_t stfu_n_sync(stfu_instance_t *i, uint32_t packets)
{

    if (packets > i->qlen) {
        stfu_n_reset(i);
    } else {
        i->sync = packets;
    }

    return STFU_IT_WORKED;
}


stfu_status_t stfu_n_add_data(stfu_instance_t *i, uint32_t ts, uint32_t seq, uint32_t pt, void *data, size_t datalen, int last)
{
	uint32_t index;
	stfu_frame_t *frame;
	size_t cplen = 0;
    int good_seq = 0, good_ts = 0;

    if (!i->samples_per_packet && ts && i->last_rd_ts) {
        i->ts_diff = ts - i->last_rd_ts;

        if (i->last_ts_diff == i->ts_diff) {
            if (++i->same_ts == 5) {
                i->samples_per_packet = i->ts_diff;
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
 
    if ((seq && seq == i->last_seq + 1) || (i->last_seq > 65500 && seq == 0)) {
        good_seq = 1;
    }

    if ((ts && ts == i->last_rd_ts + i->samples_per_packet) || (i->last_rd_ts > 4294900000 && ts < 5000)) {
        good_ts = 1;
    }


    if (good_seq || good_ts) {
        i->period_clean_count++;
        i->session_clean_count++;
    }

    i->period_packet_in_count++;
    i->session_packet_in_count++;

    if (i->session_packet_in_count == 150) {
        return STFU_IT_WORKED;
    }

    i->period_need_range_avg = i->period_need_range / (i->period_missing_count || 1);
    
    if (i->period_missing_count > i->qlen * 2) {
        stfu_n_resize(i, i->qlen + 1);
        stfu_n_reset_counters(i);
    }

    if (i->qlen > i->orig_qlen && (i->consecutive_good_count > i->decrement_time || i->period_clean_count > i->decrement_time)) {
        stfu_n_resize(i, i->qlen - 1);
        stfu_n_reset_counters(i);
        stfu_n_sync(i, i->qlen);
    }

    if ((i->period_packet_in_count > i->period_time)) {
        i->period_packet_in_count = 0;

        if (i->period_missing_count == 0 && i->qlen > i->orig_qlen) {
            stfu_n_resize(i, i->qlen - 1);
            stfu_n_sync(i, i->qlen);
        }

        stfu_n_reset_counters(i);
    }

#ifdef DB_JB
    printf("%u i=%u/%u - g:%u/%u c:%u/%u b:%u - %u/%u - %u %d\n", 
           i->qlen, i->period_packet_in_count, i->period_time, i->consecutive_good_count, 
           i->decrement_time, i->period_clean_count, i->decrement_time, i->consecutive_bad_count,
           seq, ts, 
           i->period_missing_count, i->period_need_range_avg);
#endif


	if (last || i->in_queue->array_len == i->in_queue->array_size) {
		stfu_queue_t *other_queue;

		other_queue = i->in_queue;
		i->in_queue = i->out_queue;
		i->out_queue = other_queue;

		i->in_queue->array_len = 0;
		i->out_queue->wr_len = 0;
		i->last_frame = NULL;
		i->miss_count = 0;
        i->in_queue->last_index = 0;
        i->out_queue->last_index = 0;
        i->out_queue->last_jitter = 0;
    }

	if (last) {
		return STFU_IM_DONE;
	}

    for(index = 0; index < i->out_queue->array_size; index++) {
        if (i->in_queue->array[index].was_read) {
            break;
        }
    }

    index = i->in_queue->array_len++;

	frame = &i->in_queue->array[index];

	if ((cplen = datalen) > sizeof(frame->data)) {
		cplen = sizeof(frame->data);
	}

    i->last_seq = seq;
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

static int stfu_n_find_any_frame(stfu_instance_t *in, stfu_frame_t **r_frame)
{
    uint32_t i = 0;
    stfu_frame_t *frame = NULL;
    stfu_queue_t *queue;

    assert(r_frame);
    
    *r_frame = NULL;

    for (queue = in->out_queue ; queue && queue != in->in_queue ; queue = in->in_queue) {

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

    }

    return 0;    
}


static int stfu_n_find_frame(stfu_instance_t *in, stfu_queue_t *queue, uint32_t ts, uint32_t seq, stfu_frame_t **r_frame)
{
    uint32_t i = 0;
    stfu_frame_t *frame = NULL;

    assert(r_frame);
    
    *r_frame = NULL;

    for(i = 0; i < queue->real_array_size; i++) {
        frame = &queue->array[i];
        
        if (((seq || in->last_seq) && frame->seq == seq) || frame->ts == ts) {
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

stfu_frame_t *stfu_n_read_a_frame(stfu_instance_t *i)
{
	stfu_frame_t *rframe = NULL;
    int found = 0;

	if (!i->samples_per_packet || ((i->out_queue->wr_len == i->out_queue->array_len) || !i->out_queue->array_len)) {
		return NULL;
	}

    if (i->cur_ts == 0) {
		i->cur_ts = i->out_queue->array[0].ts;
    } else {
		i->cur_ts += i->samples_per_packet;
    }
    
    if (i->cur_seq == 0) {
        i->cur_seq = i->out_queue->array[0].seq;
    } else {
        i->cur_seq++;
    }

    if (!(found = stfu_n_find_frame(i, i->out_queue, i->cur_ts, i->cur_seq, &rframe))) {
        found = stfu_n_find_frame(i, i->in_queue, i->cur_ts, i->cur_seq, &rframe);
    }

    if (!found && i->sync) {
#ifdef DB_JB
        printf("SYNC %u\n", i->sync);
#endif
        if ((found = stfu_n_find_any_frame(i, &rframe))) {
            i->cur_seq = rframe->seq;
            i->cur_ts = rframe->ts;
        }
        i->sync--;
    }


    if (!found && i->samples_per_packet) {
#ifdef DB_JB
        int y;
        stfu_frame_t *frame = NULL;
#endif
        int32_t delay = i->last_rd_ts - i->cur_ts;
        uint32_t need  = abs(i->last_rd_ts - i->cur_ts) / i->samples_per_packet;

        
        i->period_missing_count++;
        i->session_missing_count++;
        i->period_need_range += need;

#ifdef DB_JB        
        printf("MISSING %u %u %u %u %d %u %d\n", i->cur_seq, i->cur_ts, i->packet_count, i->last_rd_ts, delay, i->qlen, need);        
#endif

        if (i->packet_count > i->orig_qlen * 100 && delay > 0 && need > i->qlen && need < (i->qlen + 5)) {
            i->packet_count = 0;
        }

#ifdef DB_JB        
        for(y = 0; y < i->out_queue->array_size; y++) {
            if ((y % 5) == 0) printf("\n");
            frame = &i->out_queue->array[y];
            printf("%u:%u\t", frame->seq, frame->ts);
        }
        printf("\n\n");


        for(y = 0; y < i->in_queue->array_size; y++) {
            if ((y % 5) == 0) printf("\n");
            frame = &i->in_queue->array[y];
            printf("%u:%u\t", frame->seq, frame->ts);
        }
        printf("\n\n");
#endif

        if (delay < 0) {
            stfu_n_reset(i);
            return NULL;
        }
    }

#ifdef DB_JB
    if (found) {
        printf("O: %u:%u %u\n", rframe->seq, rframe->seq, rframe->plc);
    } else {
        printf("DATA: %u %u %d %s %d\n", i->packet_count, i->consecutive_good_count, i->out_queue->last_jitter, found ? "found" : "not found", i->qlen);
    }
#endif


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
        i->last_wr_seq = rframe->seq;
		i->miss_count = 0;
        if (rframe->dlen) {
            i->plc_len = rframe->dlen;
        }
    } else {
        i->last_wr_ts = i->cur_ts;
        i->last_wr_seq = i->cur_seq;
        rframe = &i->out_queue->int_frame;
        rframe->dlen = i->plc_len;
        
#if 0
        if (i->last_frame) {
            /* poor man's plc..  Copy the last frame, but we flag it so you can use a better one if you wish */
            if (i->miss_count) {
                memset(rframe->data, 255, rframe->dlen);
            } else {
                memcpy(rframe->data, i->last_frame->data, rframe->dlen);
            }
        }
#endif
        rframe->ts = i->cur_ts;

        i->miss_count++;

#ifdef DB_JB
        printf("PLC %d %d %ld %u %u\n", i->miss_count, rframe->plc, rframe->dlen, rframe->seq, rframe->ts);
#endif

        if (i->miss_count > i->max_plc) {
            stfu_n_reset(i);
            rframe = NULL;
        }
    }

	return rframe;
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
