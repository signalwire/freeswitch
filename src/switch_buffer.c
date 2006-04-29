/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 *
 * switch_buffer.c -- Data Buffering Code
 *
 */
#include <switch.h>
#include <switch_buffer.h>

static uint32_t buffer_id = 0;

struct switch_buffer {
	unsigned char *data;
	switch_size_t used;
	switch_size_t datalen;
	uint32_t id;
};

SWITCH_DECLARE(switch_status) switch_buffer_create(switch_memory_pool_t *pool, switch_buffer_t **buffer, switch_size_t max_len)
{
	switch_buffer_t *new_buffer;

	if ((new_buffer = switch_core_alloc(pool, sizeof(switch_buffer_t))) != 0
		&& (new_buffer->data = switch_core_alloc(pool, max_len)) != 0) {
		new_buffer->datalen = max_len;
		new_buffer->id = buffer_id++;
		*buffer = new_buffer;
		return SWITCH_STATUS_SUCCESS;
	}
	return SWITCH_STATUS_MEMERR;
}

SWITCH_DECLARE(switch_size_t) switch_buffer_len(switch_buffer_t *buffer)
{

	assert(buffer != NULL);

	return buffer->datalen;

}


SWITCH_DECLARE(switch_size_t) switch_buffer_freespace(switch_buffer_t *buffer)
{
	assert(buffer != NULL);

	return (switch_size_t) (buffer->datalen - buffer->used);
}

SWITCH_DECLARE(switch_size_t) switch_buffer_inuse(switch_buffer_t *buffer)
{
	assert(buffer != NULL);

	return buffer->used;
}

SWITCH_DECLARE(switch_size_t) switch_buffer_toss(switch_buffer_t *buffer, switch_size_t datalen)
{
	switch_size_t reading = 0;

	assert(buffer != NULL);

	if (buffer->used < 1) {
		buffer->used = 0;
		return 0;
	} else if (buffer->used >= datalen) {
		reading = datalen;
	} else {
		reading = buffer->used;
	}

	memmove(buffer->data, buffer->data + reading, buffer->datalen - reading);
	buffer->used -= datalen;

	return buffer->datalen;
}

SWITCH_DECLARE(switch_size_t) switch_buffer_read(switch_buffer_t *buffer, void *data, switch_size_t datalen)
{
	switch_size_t reading = 0;

	assert(buffer != NULL);
	assert(data != NULL);


	if (buffer->used < 1) {
		buffer->used = 0;
		return 0;
	} else if (buffer->used >= datalen) {
		reading = datalen;
	} else {
		reading = buffer->used;
	}

	memcpy(data, buffer->data, reading);
	memmove(buffer->data, buffer->data + reading, buffer->datalen - reading);
	buffer->used -= reading;
	//if (buffer->id == 3) printf("%u o %d = %d\n", buffer->id, (uint32_t)reading, (uint32_t)buffer->used);
	return reading;
}

SWITCH_DECLARE(switch_size_t) switch_buffer_write(switch_buffer_t *buffer, void *data, switch_size_t datalen)
{
	switch_size_t freespace;

	assert(buffer != NULL);
	assert(data != NULL);
	assert(buffer->data != NULL);
	
	freespace = buffer->datalen - buffer->used;
	if (freespace < datalen) {
		return 0;
	} else {
		memcpy(buffer->data + buffer->used, data, datalen);
		buffer->used += datalen;
	}
	//if (buffer->id == 3) printf("%u i %d = %d\n", buffer->id, (uint32_t)datalen, (uint32_t)buffer->used);

	return buffer->used;
}

SWITCH_DECLARE(void) switch_buffer_zero(switch_buffer_t *buffer)
{
	assert(buffer != NULL);
    assert(buffer->data != NULL);

	buffer->used = 0;
	
}
