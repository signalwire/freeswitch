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
 * THOSE WHO DISAGREE MAY CERTIANLY STFU
 */
#include "stfu.h"

#ifdef _MSC_VER
/* warning C4706: assignment within conditional expression*/
#pragma warning(disable: 4706)
#endif

struct stfu_queue {
	struct stfu_frame *array;
	struct stfu_frame int_frame;
	uint32_t array_size;
	uint32_t array_len;	
	uint32_t wr_len;
	uint32_t last_index;
};
typedef struct stfu_queue stfu_queue_t;

struct stfu_instance {
	struct stfu_queue a_queue;
	struct stfu_queue b_queue;
	struct stfu_queue *in_queue;
	struct stfu_queue *out_queue;
	uint32_t last_ts;
	uint32_t interval;
	uint32_t miss_count;
	uint8_t running;
};


static stfu_status_t stfu_n_resize_aqueue(stfu_queue_t *queue, uint32_t qlen)
{
    unsigned char *m;

    if (qlen <= queue->array_size) {
        return STFU_IT_FAILED;;
    }

	m = realloc(queue->array, qlen * sizeof(struct stfu_frame));
    assert(m);
    memset(m + queue->array_size, 0, qlen * sizeof(struct stfu_frame) - queue->array_size);
    queue->array = (struct stfu_frame *) m;
	queue->array_size = qlen;
	return STFU_IT_WORKED;
}

static void stfu_n_init_aqueue(stfu_queue_t *queue, uint32_t qlen)
{
	queue->array = calloc(qlen, sizeof(struct stfu_frame));
	assert(queue->array != NULL);
	memset(queue->array, 0, sizeof(struct stfu_frame) * qlen);	
	queue->array_size = qlen;
	queue->int_frame.plc = 1;
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
    r->in_len = i->in_queue->array_len;
    r->in_size = i->in_queue->array_size;
    r->out_len = i->out_queue->array_len;
    r->out_size = i->out_queue->array_size;
}

stfu_status_t stfu_n_resize(stfu_instance_t *i, uint32_t qlen) 
{
    stfu_status_t s;

    if ((s = stfu_n_resize_aqueue(&i->a_queue, qlen)) == STFU_IT_WORKED) {
        s = stfu_n_resize_aqueue(&i->b_queue, qlen);
    }
    
    return s;
}

stfu_instance_t *stfu_n_init(uint32_t qlen)
{
	struct stfu_instance *i;

	i = malloc(sizeof(*i));
	if (!i) {
		return NULL;
	}
	memset(i, 0, sizeof(*i));
	stfu_n_init_aqueue(&i->a_queue, qlen);
	stfu_n_init_aqueue(&i->b_queue, qlen);
	i->in_queue = &i->a_queue;
	i->out_queue = &i->b_queue;
	return i;
}

void stfu_n_reset(stfu_instance_t *i)
{
	i->in_queue = &i->a_queue;
	i->out_queue = &i->b_queue;
	i->in_queue->array_len = 0;
	i->out_queue->array_len = 0;
	i->out_queue->wr_len = 0;
	i->out_queue->last_index = 0;
	i->miss_count = 0;	
	i->last_ts = 0;
	i->running = 0;
	i->miss_count = 0;
	i->interval = 0;
}

static int32_t stfu_n_measure_interval(stfu_queue_t *queue)
{
	uint32_t index;
	int32_t d, most = 0, last = 0, this, track[STFU_MAX_TRACK] = {0};

	for(index = 0; index < queue->array_len; index++) {
		this = queue->array[index].ts;
		if (last) {

			if ((d = this - last) > 0 && d / 10 < STFU_MAX_TRACK) {
				track[(d/10)]++;
			}
		}

		last = this;
	}

	for(index = 0; index < STFU_MAX_TRACK; index++) {
		if (track[index] > track[most]) {
			most = index;
		}
	}

	return most * 10;
}

static int16_t stfu_n_process(stfu_instance_t *i, stfu_queue_t *queue)
{
	if (!i->interval && !(i->interval = stfu_n_measure_interval(queue))) {
		return -1;
	}

	return 0;
}

stfu_status_t stfu_n_add_data(stfu_instance_t *i, uint32_t ts, uint32_t pt, void *data, size_t datalen, int last)
{
	uint32_t index;
	stfu_frame_t *frame;
	size_t cplen = 0;

	if (last || i->in_queue->array_len == i->in_queue->array_size) {
		stfu_queue_t *other_queue;

		if (i->out_queue->wr_len < i->out_queue->array_len) {
			return STFU_IT_FAILED;
		}

		other_queue = i->in_queue;
		i->in_queue = i->out_queue;
		i->out_queue = other_queue;

		i->in_queue->array_len = 0;
		i->out_queue->wr_len = 0;
		i->out_queue->last_index = 0;
		i->miss_count = 0;

		if (stfu_n_process(i, i->out_queue) < 0) {
            if (i->in_queue->array_len == i->in_queue->array_size && i->out_queue->array_len == i->out_queue->array_size) {
                stfu_n_resize(i, i->out_queue->array_size * 2);
            }
			//return STFU_IT_FAILED;
		}
		for(index = 0; index < i->out_queue->array_len; index++) {
			i->out_queue->array[index].was_read = 0;
		}
	}

	if (last) {
		return STFU_IM_DONE;
	}

	index = i->in_queue->array_len++;
	frame = &i->in_queue->array[index];

	if ((cplen = datalen) > sizeof(frame->data)) {
		cplen = sizeof(frame->data);
	}

	memcpy(frame->data, data, cplen);
    frame->pt = pt;
	frame->ts = ts;
	frame->dlen = cplen;
	frame->was_read = 0;	

	return STFU_IT_WORKED;
}

stfu_frame_t *stfu_n_read_a_frame(stfu_instance_t *i)
{
	uint32_t index, index2;
	uint32_t should_have = 0;
	stfu_frame_t *frame = NULL, *rframe = NULL;

	if (((i->out_queue->wr_len == i->out_queue->array_len) || !i->out_queue->array_len)) {
		return NULL;
	}

	if (i->running) {
		should_have = i->last_ts + i->interval;
	} else {
		should_have = i->out_queue->array[0].ts;
	}

	for(index = 0; index < i->out_queue->array_len; index++) {
		if (i->out_queue->array[index].was_read) {
			continue;
		}

		frame = &i->out_queue->array[index];

		if (frame->ts != should_have) {
			unsigned int tried = 0;
			for (index2 = 0; index2 < i->out_queue->array_len; index2++) {
				if (i->out_queue->array[index2].was_read) {
					continue;
				}
				tried++;
				if (i->out_queue->array[index2].ts == should_have) {
					rframe = &i->out_queue->array[index2];
					i->out_queue->last_index = index2;
					goto done;
				}
			}
			for (index2 = 0; index2 < i->in_queue->array_len; index2++) {
				if (i->in_queue->array[index2].was_read) {
					continue;
				}
				tried++;
				if (i->in_queue->array[index2].ts == should_have) {
					rframe = &i->in_queue->array[index2];
					goto done;
				}
			}

			i->miss_count++;

			if (i->miss_count > 10 || (i->in_queue->array_len == i->in_queue->array_size) || tried >= i->in_queue->array_size) {
				i->running = 0;
				i->interval = 0;
				i->out_queue->wr_len = i->out_queue->array_size;
				return NULL;
			}

			i->last_ts = should_have;
			rframe = &i->out_queue->int_frame;
			rframe->dlen = i->out_queue->array[i->out_queue->last_index].dlen;
			/* poor man's plc..  Copy the last frame, but we flag it so you can use a better one if you wish */
			memcpy(rframe->data, i->out_queue->array[i->out_queue->last_index].data, rframe->dlen);
			rframe->ts = should_have;
			i->out_queue->wr_len++;
			i->running = 1;
			return rframe;			
		} else {
			rframe = &i->out_queue->array[index];
			i->out_queue->last_index = index;
			goto done;
		}
	}

done:

	if (rframe) {
		i->out_queue->wr_len++;
		i->last_ts = rframe->ts;
		rframe->was_read = 1;
		i->running = 1;
		i->miss_count = 0;
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
