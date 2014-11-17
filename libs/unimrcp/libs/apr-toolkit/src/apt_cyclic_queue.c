/*
 * Copyright 2008-2014 Arsen Chaloyan
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * $Id: apt_cyclic_queue.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include <stdlib.h>
#include "apt_cyclic_queue.h"

struct apt_cyclic_queue_t {
	void       **data;
	apr_size_t   max_size;
	apr_size_t   actual_size;
	apr_size_t   head;
	apr_size_t   tail;
};

static apt_bool_t apt_cyclic_queue_resize(apt_cyclic_queue_t *queue);


APT_DECLARE(apt_cyclic_queue_t*) apt_cyclic_queue_create(apr_size_t size)
{
	apt_cyclic_queue_t *queue = malloc(sizeof(apt_cyclic_queue_t));
	queue->max_size = size;
	queue->actual_size = 0;
	queue->data = malloc(sizeof(void*) * queue->max_size);
	queue->head = queue->tail = 0;
	return queue;
}

APT_DECLARE(void) apt_cyclic_queue_destroy(apt_cyclic_queue_t *queue)
{
	if(queue->data) {
		free(queue->data);
		queue->data = NULL;
	}
	free(queue);
}

APT_DECLARE(apt_bool_t) apt_cyclic_queue_push(apt_cyclic_queue_t *queue, void *obj)
{
	if(queue->actual_size >= queue->max_size) {
		if(apt_cyclic_queue_resize(queue) != TRUE) {
			return FALSE;
		}
	}
	
	queue->data[queue->head] = obj;
	queue->head = (queue->head + 1) % queue->max_size;
	queue->actual_size++;
	return TRUE;
}

APT_DECLARE(void*) apt_cyclic_queue_pop(apt_cyclic_queue_t *queue)
{
	void *obj = NULL;
	if(queue->actual_size) {
		obj = queue->data[queue->tail];
		queue->tail = (queue->tail + 1) % queue->max_size;
		queue->actual_size--;
	}
	return obj;
}

APT_DECLARE(void) apt_cyclic_queue_clear(apt_cyclic_queue_t *queue)
{
	queue->actual_size = 0;
	queue->head = queue->tail = 0;
}

APT_DECLARE(apt_bool_t) apt_cyclic_queue_is_empty(const apt_cyclic_queue_t *queue)
{
	return queue->actual_size ? TRUE : FALSE;
}

static apt_bool_t apt_cyclic_queue_resize(apt_cyclic_queue_t *queue)
{
	apr_size_t new_size = queue->max_size + queue->max_size/2;
	void **new_data = malloc(sizeof(void*) * new_size);
	apr_size_t offset;

	offset = queue->max_size - queue->head;
	memcpy(new_data, queue->data + queue->head, sizeof(void*) * offset);
	if(queue->head) {
		memcpy(new_data + offset, queue->data, sizeof(void*) * queue->head);
	}

	queue->tail = 0;
	queue->head = queue->max_size;
	queue->max_size = new_size;
	free(queue->data);
	queue->data = new_data;
	return TRUE;
}
