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

#include "openzap.h"

static zap_status_t zap_std_queue_create(zap_queue_t **outqueue, zap_size_t capacity);
static zap_status_t zap_std_queue_enqueue(zap_queue_t *queue, void *obj);
static void *zap_std_queue_dequeue(zap_queue_t *queue);
static zap_status_t zap_std_queue_wait(zap_queue_t *queue, int ms);
static zap_status_t zap_std_queue_destroy(zap_queue_t **inqueue);

struct zap_queue {
	zap_mutex_t *mutex;
	zap_condition_t *condition;
	zap_size_t capacity;
	zap_size_t size;
	unsigned rindex;
	unsigned windex;
	void **elements;
};

OZ_DECLARE_DATA zap_queue_handler_t g_zap_queue_handler = 
{
	/*.create = */ zap_std_queue_create,
	/*.enqueue = */ zap_std_queue_enqueue,
	/*.dequeue = */ zap_std_queue_dequeue,
	/*.wait = */ zap_std_queue_wait,
	/*.destroy = */ zap_std_queue_destroy
};

OZ_DECLARE(zap_status_t) zap_global_set_queue_handler(zap_queue_handler_t *handler)
{
	if (!handler ||
		!handler->create || 
		!handler->enqueue || 
		!handler->dequeue || 
		!handler->wait || 
		!handler->destroy) {
		return ZAP_FAIL;
	}
	memcpy(&g_zap_queue_handler, handler, sizeof(*handler));
	return ZAP_SUCCESS;
}

static zap_status_t zap_std_queue_create(zap_queue_t **outqueue, zap_size_t capacity)
{
	zap_queue_t *queue = NULL;
	zap_assert_return(outqueue, ZAP_FAIL, "Queue double pointer is null\n");
	zap_assert_return(capacity > 0, ZAP_FAIL, "Queue capacity is not bigger than 0\n");

	*outqueue = NULL;
	queue = zap_calloc(1, sizeof(*queue));
	if (!queue) {
		return ZAP_FAIL;
	}

	queue->elements = zap_calloc(1, (sizeof(void*)*capacity));
	if (!queue->elements) {
		goto failed;
	}
	queue->capacity = capacity;

	if (zap_mutex_create(&queue->mutex) != ZAP_SUCCESS) {
		goto failed;
	}

	if (zap_condition_create(&queue->condition, queue->mutex) != ZAP_SUCCESS) {
		goto failed;
	}

	*outqueue = queue;	
	return ZAP_SUCCESS;

failed:
	if (queue) {
		if (queue->condition) {
			zap_condition_destroy(&queue->condition);
		}
		if (queue->mutex) {
			zap_mutex_destroy(&queue->mutex);
		}
		zap_safe_free(queue->elements);
		zap_safe_free(queue);
	}
	return ZAP_FAIL;
}

static zap_status_t zap_std_queue_enqueue(zap_queue_t *queue, void *obj)
{
	zap_status_t status = ZAP_FAIL;

	zap_assert_return(queue != NULL, ZAP_FAIL, "Queue is null!");

	zap_mutex_lock(queue->mutex);

	if (queue->windex == queue->capacity) {
		/* try to see if we can wrap around */
		queue->windex = 0;
	}

	if (queue->size != 0 && queue->windex == queue->rindex) {
		zap_log(ZAP_LOG_ERROR, "Failed to enqueue obj %p in queue %p, no more room! windex == rindex == %d!\n", obj, queue, queue->windex);
		goto done;
	}

	queue->elements[queue->windex++] = obj;
	queue->size++;
	status = ZAP_SUCCESS;

	/* wake up queue reader */
	zap_condition_signal(queue->condition);

done:

	zap_mutex_unlock(queue->mutex);

	return status;
}

static void *zap_std_queue_dequeue(zap_queue_t *queue)
{
	void *obj = NULL;

	zap_assert_return(queue != NULL, NULL, "Queue is null!");

	zap_mutex_lock(queue->mutex);

	if (queue->size == 0) {
		goto done;
	}
	
	obj = queue->elements[queue->rindex];
	queue->elements[queue->rindex++] = NULL;
	queue->size--;
	if (queue->rindex == queue->size) {
		queue->rindex = 0;
	}

done:

	zap_mutex_unlock(queue->mutex);

	return obj;
}

static zap_status_t zap_std_queue_wait(zap_queue_t *queue, int ms)
{
	zap_status_t ret;
	zap_assert_return(queue != NULL, ZAP_FAIL, "Queue is null!");
	
	zap_mutex_lock(queue->mutex);

	/* if there is elements in the queue, no need to wait */
	if (queue->size != 0) {
		zap_mutex_unlock(queue->mutex);
		return ZAP_SUCCESS;
	}

	/* no elements on the queue, wait for someone to write an element */
	ret = zap_condition_wait(queue->condition, ms);

	/* got an element or timeout, bail out */
	zap_mutex_unlock(queue->mutex);

	return ret;
}

static zap_status_t zap_std_queue_destroy(zap_queue_t **inqueue)
{
	zap_queue_t *queue = NULL;
	zap_assert_return(inqueue != NULL, ZAP_FAIL, "Queue is null!");
	zap_assert_return(*inqueue != NULL, ZAP_FAIL, "Queue is null!");

	queue = *inqueue;
	zap_condition_destroy(&queue->condition);
	zap_mutex_destroy(&queue->mutex);
	zap_safe_free(queue->elements);
	zap_safe_free(queue);
	*inqueue = NULL;
	return ZAP_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
