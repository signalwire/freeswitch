/*
 * Copyright (c) 2009, Sangoma Technologies
 * Moises Silva <moy@sangoma.com>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "private/ftdm_core.h"

static ftdm_status_t ftdm_std_queue_create(ftdm_queue_t **outqueue, ftdm_size_t capacity);
static ftdm_status_t ftdm_std_queue_enqueue(ftdm_queue_t *queue, void *obj);
static void *ftdm_std_queue_dequeue(ftdm_queue_t *queue);
static ftdm_status_t ftdm_std_queue_wait(ftdm_queue_t *queue, int ms);
static ftdm_status_t ftdm_std_queue_get_interrupt(ftdm_queue_t *queue, ftdm_interrupt_t **interrupt);
static ftdm_status_t ftdm_std_queue_destroy(ftdm_queue_t **inqueue);

struct ftdm_queue {
	ftdm_mutex_t *mutex;
	ftdm_interrupt_t *interrupt;
	ftdm_size_t capacity;
	ftdm_size_t size;
	unsigned rindex;
	unsigned windex;
	void **elements;
};

FT_DECLARE_DATA ftdm_queue_handler_t g_ftdm_queue_handler = 
{
	/*.create = */ ftdm_std_queue_create,
	/*.enqueue = */ ftdm_std_queue_enqueue,
	/*.dequeue = */ ftdm_std_queue_dequeue,
	/*.wait = */ ftdm_std_queue_wait,
	/*.get_interrupt = */ ftdm_std_queue_get_interrupt,
	/*.destroy = */ ftdm_std_queue_destroy
};

FT_DECLARE(ftdm_status_t) ftdm_global_set_queue_handler(ftdm_queue_handler_t *handler)
{
	if (!handler ||
		!handler->create || 
		!handler->enqueue || 
		!handler->dequeue || 
		!handler->wait || 
		!handler->get_interrupt || 
		!handler->destroy) {
		return FTDM_FAIL;
	}
	memcpy(&g_ftdm_queue_handler, handler, sizeof(*handler));
	return FTDM_SUCCESS;
}

static ftdm_status_t ftdm_std_queue_create(ftdm_queue_t **outqueue, ftdm_size_t capacity)
{
	ftdm_queue_t *queue = NULL;
	ftdm_assert_return(outqueue, FTDM_FAIL, "Queue double pointer is null\n");
	ftdm_assert_return(capacity > 0, FTDM_FAIL, "Queue capacity is not bigger than 0\n");

	*outqueue = NULL;
	queue = ftdm_calloc(1, sizeof(*queue));
	if (!queue) {
		return FTDM_FAIL;
	}

	queue->elements = ftdm_calloc(1, (sizeof(void*)*capacity));
	if (!queue->elements) {
		goto failed;
	}
	queue->capacity = capacity;

	if (ftdm_mutex_create(&queue->mutex) != FTDM_SUCCESS) {
		goto failed;
	}

	if (ftdm_interrupt_create(&queue->interrupt, FTDM_INVALID_SOCKET, FTDM_NO_FLAGS) != FTDM_SUCCESS) {
		goto failed;
	}

	*outqueue = queue;	
	return FTDM_SUCCESS;

failed:
	if (queue) {
		if (queue->interrupt) {
			ftdm_interrupt_destroy(&queue->interrupt);
		}
		if (queue->mutex) {
			ftdm_mutex_destroy(&queue->mutex);
		}
		ftdm_safe_free(queue->elements);
		ftdm_safe_free(queue);
	}
	return FTDM_FAIL;
}

static ftdm_status_t ftdm_std_queue_enqueue(ftdm_queue_t *queue, void *obj)
{
	ftdm_status_t status = FTDM_FAIL;

	ftdm_assert_return(queue != NULL, FTDM_FAIL, "Queue is null!");

	ftdm_mutex_lock(queue->mutex);

	if (queue->windex == queue->capacity) {
		/* try to see if we can wrap around */
		queue->windex = 0;
	}

	if (queue->size != 0 && queue->windex == queue->rindex) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to enqueue obj %p in queue %p, no more room! windex == rindex == %d!\n", obj, queue, queue->windex);
		goto done;
	}

	queue->elements[queue->windex++] = obj;
	queue->size++;
	status = FTDM_SUCCESS;

	/* wake up queue reader */
	ftdm_interrupt_signal(queue->interrupt);

done:

	ftdm_mutex_unlock(queue->mutex);

	return status;
}

static void *ftdm_std_queue_dequeue(ftdm_queue_t *queue)
{
	void *obj = NULL;

	ftdm_assert_return(queue != NULL, NULL, "Queue is null!");

	ftdm_mutex_lock(queue->mutex);

	if (queue->size == 0) {
		goto done;
	}
	
	obj = queue->elements[queue->rindex];
	queue->elements[queue->rindex++] = NULL;
	queue->size--;
	if (queue->rindex == queue->capacity) {
		queue->rindex = 0;
	}

done:

	ftdm_mutex_unlock(queue->mutex);

	return obj;
}

static ftdm_status_t ftdm_std_queue_wait(ftdm_queue_t *queue, int ms)
{
	ftdm_status_t ret;
	ftdm_assert_return(queue != NULL, FTDM_FAIL, "Queue is null!");
	
	ftdm_mutex_lock(queue->mutex);

	/* if there is elements in the queue, no need to wait */
	if (queue->size != 0) {
		ftdm_mutex_unlock(queue->mutex);
		return FTDM_SUCCESS;
	}

	/* no elements on the queue, wait for someone to write an element */
	ret = ftdm_interrupt_wait(queue->interrupt, ms);

	/* got an element or timeout, bail out */
	ftdm_mutex_unlock(queue->mutex);

	return ret;
}

static ftdm_status_t ftdm_std_queue_get_interrupt(ftdm_queue_t *queue, ftdm_interrupt_t **interrupt)
{
	ftdm_assert_return(queue != NULL, FTDM_FAIL, "Queue is null!\n");
	ftdm_assert_return(interrupt != NULL, FTDM_FAIL, "Queue is null!\n");
	*interrupt = queue->interrupt;
	return FTDM_SUCCESS;
}

static ftdm_status_t ftdm_std_queue_destroy(ftdm_queue_t **inqueue)
{
	ftdm_queue_t *queue = NULL;
	ftdm_assert_return(inqueue != NULL, FTDM_FAIL, "Queue is null!\n");
	ftdm_assert_return(*inqueue != NULL, FTDM_FAIL, "Queue is null!\n");

	queue = *inqueue;
	ftdm_interrupt_destroy(&queue->interrupt);
	ftdm_mutex_destroy(&queue->mutex);
	ftdm_safe_free(queue->elements);
	ftdm_safe_free(queue);
	*inqueue = NULL;
	return FTDM_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
