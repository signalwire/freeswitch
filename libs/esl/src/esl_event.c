/*
 * Copyright (c) 2007-2012, Anthony Minessale II
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


#include <esl.h>
#include <esl_event.h>

static char *my_dup(const char *s)
{
	size_t len = strlen(s) + 1;
	void *new = malloc(len);
	esl_assert(new);

	return (char *) memcpy(new, s, len);
}

#ifndef ALLOC
#define ALLOC(size) malloc(size)
#endif
#ifndef DUP
#define DUP(str) my_dup(str)
#endif
#ifndef FREE
#define FREE(ptr) esl_safe_free(ptr)
#endif

/* make sure this is synced with the esl_event_types_t enum in esl_types.h
   also never put any new ones before EVENT_ALL
*/
static const char *EVENT_NAMES[] = {
	"CUSTOM",
	"CLONE",
	"CHANNEL_CREATE",
	"CHANNEL_DESTROY",
	"CHANNEL_STATE",
	"CHANNEL_CALLSTATE",
	"CHANNEL_ANSWER",
	"CHANNEL_HANGUP",
	"CHANNEL_HANGUP_COMPLETE",
	"CHANNEL_EXECUTE",
	"CHANNEL_EXECUTE_COMPLETE",
	"CHANNEL_HOLD",
	"CHANNEL_UNHOLD",
	"CHANNEL_BRIDGE",
	"CHANNEL_UNBRIDGE",
	"CHANNEL_PROGRESS",
	"CHANNEL_PROGRESS_MEDIA",
	"CHANNEL_OUTGOING",
	"CHANNEL_PARK",
	"CHANNEL_UNPARK",
	"CHANNEL_APPLICATION",
	"CHANNEL_ORIGINATE",
	"CHANNEL_UUID",
	"API",
	"LOG",
	"INBOUND_CHAN",
	"OUTBOUND_CHAN",
	"STARTUP",
	"SHUTDOWN",
	"PUBLISH",
	"UNPUBLISH",
	"TALK",
	"NOTALK",
	"SESSION_CRASH",
	"MODULE_LOAD",
	"MODULE_UNLOAD",
	"DTMF",
	"MESSAGE",
	"PRESENCE_IN",
	"NOTIFY_IN",
	"PHONE_FEATURE",
	"PHONE_FEATURE_SUBSCRIBE",
	"PRESENCE_OUT",
	"PRESENCE_PROBE",
	"MESSAGE_WAITING",
	"MESSAGE_QUERY",
	"ROSTER",
	"CODEC",
	"BACKGROUND_JOB",
	"DETECTED_SPEECH",
	"DETECTED_TONE",
	"PRIVATE_COMMAND",
	"HEARTBEAT",
	"TRAP",
	"ADD_SCHEDULE",
	"DEL_SCHEDULE",
	"EXE_SCHEDULE",
	"RE_SCHEDULE",
	"RELOADXML",
	"NOTIFY",
	"SEND_MESSAGE",
	"RECV_MESSAGE",
	"REQUEST_PARAMS",
	"CHANNEL_DATA",
	"GENERAL",
	"COMMAND",
	"SESSION_HEARTBEAT",
	"CLIENT_DISCONNECTED",
	"SERVER_DISCONNECTED",
	"SEND_INFO",
	"RECV_INFO",
	"RECV_RTCP_MESSAGE",
	"CALL_SECURE",
	"NAT",
	"RECORD_START",
	"RECORD_STOP",
	"PLAYBACK_START",
	"PLAYBACK_STOP",
	"CALL_UPDATE",
	"FAILURE",
	"SOCKET_DATA",
	"MEDIA_BUG_START",
	"MEDIA_BUG_STOP",
	"CONFERENCE_DATA_QUERY",
	"CONFERENCE_DATA",
	"CALL_SETUP_REQ",
	"CALL_SETUP_RESULT",
	"CALL_DETAIL",
	"DEVICE_STATE",
	"ALL"
};

ESL_DECLARE(const char *)esl_event_name(esl_event_types_t event)
{
	return EVENT_NAMES[event];
}

ESL_DECLARE(esl_status_t) esl_name_event(const char *name, esl_event_types_t *type)
{
	esl_event_types_t x;

	for (x = 0; x <= ESL_EVENT_ALL; x++) {
		if ((strlen(name) > 13 && !strcasecmp(name + 13, EVENT_NAMES[x])) || !strcasecmp(name, EVENT_NAMES[x])) {
			*type = x;
			return ESL_SUCCESS;
		}
	}

	return ESL_FAIL;
}


ESL_DECLARE(esl_status_t) esl_event_create_subclass(esl_event_t **event, esl_event_types_t event_id, const char *subclass_name)
{
	*event = NULL;

	if ((event_id != ESL_EVENT_CLONE && event_id != ESL_EVENT_CUSTOM) && subclass_name) {
		return ESL_FAIL;
	}

	*event = ALLOC(sizeof(esl_event_t));
	esl_assert(*event);


	memset(*event, 0, sizeof(esl_event_t));

	if (event_id != ESL_EVENT_CLONE) {
		(*event)->event_id = event_id;
		esl_event_add_header_string(*event, ESL_STACK_BOTTOM, "Event-Name", esl_event_name((*event)->event_id));
	}

	if (subclass_name) {
		(*event)->subclass_name = DUP(subclass_name);
		esl_event_add_header_string(*event, ESL_STACK_BOTTOM, "Event-Subclass", subclass_name);
	}	
	
	return ESL_SUCCESS;
}


ESL_DECLARE(const char *)esl_priority_name(esl_priority_t priority)
{
	switch (priority) {			/*lol */
	case ESL_PRIORITY_NORMAL:
		return "NORMAL";
	case ESL_PRIORITY_LOW:
		return "LOW";
	case ESL_PRIORITY_HIGH:
		return "HIGH";
	default:
		return "INVALID";
	}
}

ESL_DECLARE(esl_status_t) esl_event_set_priority(esl_event_t *event, esl_priority_t priority)
{
	event->priority = priority;
	esl_event_add_header_string(event, ESL_STACK_TOP, "priority", esl_priority_name(priority));
	return ESL_SUCCESS;
}

#define ESL_HASH_KEY_STRING -1

static unsigned int esl_ci_hashfunc_default(const char *char_key, esl_ssize_t *klen)

{
    unsigned int hash = 0;
    const unsigned char *key = (const unsigned char *)char_key;
    const unsigned char *p;
    esl_ssize_t i;

    if (*klen == ESL_HASH_KEY_STRING) {
        for (p = key; *p; p++) {
            hash = hash * 33 + tolower(*p);
        }
        *klen = p - key;
    }
    else {
        for (p = key, i = *klen; i; i--, p++) {
            hash = hash * 33 + tolower(*p);
        }
    }

    return hash;
}

ESL_DECLARE(esl_event_header_t *) esl_event_get_header_ptr(esl_event_t *event, const char *header_name)
{
	esl_event_header_t *hp;
	esl_ssize_t hlen = -1;
	unsigned long hash = 0;

	esl_assert(event);

	if (!header_name)
		return NULL;

	hash = esl_ci_hashfunc_default(header_name, &hlen);

	for (hp = event->headers; hp; hp = hp->next) {
		if ((!hp->hash || hash == hp->hash) && !strcasecmp(hp->name, header_name)) {
			return hp;
		}
	}
	return NULL;
}

ESL_DECLARE(char *) esl_event_get_header_idx(esl_event_t *event, const char *header_name, int idx)
{
	esl_event_header_t *hp;

	if ((hp = esl_event_get_header_ptr(event, header_name))) {
		if (idx > -1) {
			if (idx < hp->idx) {
				return hp->array[idx];
			} else {
				return NULL;
			}
		}

		return hp->value;
	} else if (header_name && !strcmp(header_name, "_body")) {
		return event->body;
	}		

	return NULL;
}

ESL_DECLARE(char *)esl_event_get_body(esl_event_t *event)
{
	return (event ? event->body : NULL);
}

ESL_DECLARE(esl_status_t) esl_event_del_header_val(esl_event_t *event, const char *header_name, const char *val)
{
	esl_event_header_t *hp, *lp = NULL, *tp;
	esl_status_t status = (esl_status_t) ESL_FALSE;
	int x = 0;
	esl_ssize_t hlen = -1;
	unsigned long hash = 0;

	tp = event->headers;
	while (tp) {
		hp = tp;
		tp = tp->next;

		x++;
		esl_assert(x < 1000000);
		hash = esl_ci_hashfunc_default(header_name, &hlen);

		if ((!hp->hash || hash == hp->hash) && (hp->name && !strcasecmp(header_name, hp->name)) && (esl_strlen_zero(val) || !strcmp(hp->value, val))) {
			if (lp) {
				lp->next = hp->next;
			} else {
				event->headers = hp->next;
			}
			if (hp == event->last_header || !hp->next) {
				event->last_header = lp;
			}
			FREE(hp->name);

			if (hp->idx) {
				int i = 0;

				for (i = 0; i < hp->idx; i++) {
					FREE(hp->array[i]);
				}
				FREE(hp->array);
			}

			FREE(hp->value);
			
			memset(hp, 0, sizeof(*hp));
#ifdef ESL_EVENT_RECYCLE
			if (esl_queue_trypush(EVENT_HEADER_RECYCLE_QUEUE, hp) != ESL_SUCCESS) {
				FREE(hp);
			}
#else
			FREE(hp);
#endif
			status = ESL_SUCCESS;
		} else {
			lp = hp;
		}
	}

	return status;
}

static esl_event_header_t *new_header(const char *header_name)
{
	esl_event_header_t *header;

#ifdef ESL_EVENT_RECYCLE
		void *pop;
		if (esl_queue_trypop(EVENT_HEADER_RECYCLE_QUEUE, &pop) == ESL_SUCCESS) {
			header = (esl_event_header_t *) pop;
		} else {
#endif
			header = ALLOC(sizeof(*header));
			esl_assert(header);
#ifdef ESL_EVENT_RECYCLE
		}
#endif	

		memset(header, 0, sizeof(*header));
		header->name = DUP(header_name);

		return header;

}

ESL_DECLARE(int) esl_event_add_array(esl_event_t *event, const char *var, const char *val)
{
	char *data;
	char **array;
	int max = 0;
	int len;
	const char *p;
	int i;

	if (strlen(val) < 8) {
		return -1;
	}

	p = val + 7;

	max = 1;

	while((p = strstr(p, "|:"))) {
		max++;
		p += 2;
	}

	data = strdup(val + 7);
	
	len = (sizeof(char *) * max) + 1;
	array = malloc(len);
	esl_assert(array);
	memset(array, 0, len);
	
	esl_separate_string_string(data, "|:", array, max);
	
	for(i = 0; i < max; i++) {
		esl_event_add_header_string(event, ESL_STACK_PUSH, var, array[i]);
	}

	free(array);
	free(data);

	return 0;
}

static esl_status_t esl_event_base_add_header(esl_event_t *event, esl_stack_t stack, const char *header_name, char *data)
{
	esl_event_header_t *header = NULL;
	esl_ssize_t hlen = -1;
	int exists = 0, fly = 0;
	char *index_ptr;
	int index = 0;
	char *real_header_name = NULL;

	if (!strcmp(header_name, "_body")) {
		esl_event_set_body(event, data);
	}

	if ((index_ptr = strchr(header_name, '['))) {
		index_ptr++;
		index = atoi(index_ptr);
		real_header_name = DUP(header_name);
		if ((index_ptr = strchr(real_header_name, '['))) {
			*index_ptr++ = '\0';
		}
		header_name = real_header_name;
	}
	
	if (index_ptr || (stack & ESL_STACK_PUSH) || (stack & ESL_STACK_UNSHIFT)) {
		
		if (!(header = esl_event_get_header_ptr(event, header_name)) && index_ptr) {

			header = new_header(header_name);

			if (esl_test_flag(event, ESL_EF_UNIQ_HEADERS)) {
				esl_event_del_header(event, header_name);
			}

			fly++;
		}
		
		if (header || (header = esl_event_get_header_ptr(event, header_name))) {
			
			if (index_ptr) {
				if (index > -1 && index <= 4000) {
					if (index < header->idx) {
						FREE(header->array[index]);
						header->array[index] = DUP(data);
					} else {
						int i;
						char **m;
					
						m = realloc(header->array, sizeof(char *) * (index + 1));
						esl_assert(m);
						header->array = m;
						for (i = header->idx; i < index; i++) {
							m[i] = DUP("");
						}
						m[index] = DUP(data);
						header->idx = index + 1;
						if (!fly) {
							exists = 1;
						}

						goto redraw;
					}
				}
				goto end;
			} else {
				if ((stack & ESL_STACK_PUSH) || (stack & ESL_STACK_UNSHIFT)) {
					exists++;
					stack &= ~(ESL_STACK_TOP | ESL_STACK_BOTTOM);
				} else {
					header = NULL;
				}
			}
		}
	}


	if (!header) {

		if (esl_strlen_zero(data)) {
			esl_event_del_header(event, header_name);
			FREE(data);
			goto end;
		}

		if (esl_test_flag(event, ESL_EF_UNIQ_HEADERS)) {
			esl_event_del_header(event, header_name);
		}

		if (strstr(data, "ARRAY::")) {
			esl_event_add_array(event, header_name, data);
			FREE(data);
			goto end;
		}


		header = new_header(header_name);
	}
	
	if ((stack & ESL_STACK_PUSH) || (stack & ESL_STACK_UNSHIFT)) {
		char **m = NULL;
		esl_size_t len = 0;
		char *hv;
		int i = 0, j = 0;

		if (header->value && !header->idx) {
			m = malloc(sizeof(char *));
			esl_assert(m);
			m[0] = header->value;
			header->value = NULL;
			header->array = m;
			header->idx++;
			m = NULL;
		}

		i = header->idx + 1;
		m = realloc(header->array, sizeof(char *) * i); 
		esl_assert(m);

		if ((stack & ESL_STACK_PUSH)) {
			m[header->idx] = data;
		} else if ((stack & ESL_STACK_UNSHIFT)) {
			for (j = header->idx; j > 0; j--) {
				m[j] = m[j-1];
			}
			m[0] = data;
		}

		header->idx++;		
		header->array = m;

	redraw:
		len = 0;
		for(j = 0; j < header->idx; j++) {
			len += strlen(header->array[j]) + 2;
		}

		if (len) {
			len += 8;
			hv = realloc(header->value, len);
			esl_assert(hv);
			header->value = hv;

			if (header->idx > 1) {
				esl_snprintf(header->value, len, "ARRAY::");
			} else {
				*header->value = '\0';
			}

			for(j = 0; j < header->idx; j++) {
				esl_snprintf(header->value + strlen(header->value), len - strlen(header->value), "%s%s", j == 0 ? "" : "|:", header->array[j]);
			}
		}

	} else {
		header->value = data;
	}

	if (!exists) {
		header->hash = esl_ci_hashfunc_default(header->name, &hlen);

		if ((stack & ESL_STACK_TOP)) {
			header->next = event->headers;
			event->headers = header;
			if (!event->last_header) {
				event->last_header = header;
			}
		} else {
			if (event->last_header) {
				event->last_header->next = header;
			} else {
				event->headers = header;
				header->next = NULL;
			}
			event->last_header = header;
		}
	}

 end:

	esl_safe_free(real_header_name);

	return ESL_SUCCESS;
}

ESL_DECLARE(esl_status_t) esl_event_add_header(esl_event_t *event, esl_stack_t stack, const char *header_name, const char *fmt, ...)
{
	int ret = 0;
	char *data;
	va_list ap;

	va_start(ap, fmt);
	ret = esl_vasprintf(&data, fmt, ap);
	va_end(ap);

	if (ret == -1) {
		return ESL_FAIL;
	}

	return esl_event_base_add_header(event, stack, header_name, data);
}

ESL_DECLARE(esl_status_t) esl_event_add_header_string(esl_event_t *event, esl_stack_t stack, const char *header_name, const char *data)
{
	if (data) {
		return esl_event_base_add_header(event, stack, header_name, DUP(data));
	}
	return ESL_FAIL;
}

ESL_DECLARE(esl_status_t) esl_event_set_body(esl_event_t *event, const char *body)
{
	esl_safe_free(event->body);

	if (body) {
		event->body = DUP(body);
	}
	
	return ESL_SUCCESS;
}

ESL_DECLARE(esl_status_t) esl_event_add_body(esl_event_t *event, const char *fmt, ...)
{
	int ret = 0;
	char *data;

	va_list ap;
	if (fmt) {
		va_start(ap, fmt);
		ret = esl_vasprintf(&data, fmt, ap);
		va_end(ap);

		if (ret == -1) {
			return ESL_FAIL;
		} else {
			esl_safe_free(event->body);
			event->body = data;
			return ESL_SUCCESS;
		}
	} else {
		return ESL_FAIL;
	}
}


ESL_DECLARE(void) esl_event_destroy(esl_event_t **event)
{
	esl_event_t *ep = *event;
	esl_event_header_t *hp, *this;

	if (ep) {
		for (hp = ep->headers; hp;) {
			this = hp;
			hp = hp->next;
			FREE(this->name);

			if (this->idx) {
				int i = 0;

				for (i = 0; i < this->idx; i++) {
					FREE(this->array[i]);
				}
				FREE(this->array);
			}

			FREE(this->value);
			

#ifdef ESL_EVENT_RECYCLE
			if (esl_queue_trypush(EVENT_HEADER_RECYCLE_QUEUE, this) != ESL_SUCCESS) {
				FREE(this);
			}
#else
			FREE(this);
#endif


		}
		FREE(ep->body);
		FREE(ep->subclass_name);
#ifdef ESL_EVENT_RECYCLE
		if (esl_queue_trypush(EVENT_RECYCLE_QUEUE, ep) != ESL_SUCCESS) {
			FREE(ep);
		}
#else
		FREE(ep);
#endif

	}
	*event = NULL;
}

ESL_DECLARE(void) esl_event_merge(esl_event_t *event, esl_event_t *tomerge)
{
	esl_event_header_t *hp;
	
	esl_assert(tomerge && event);

	for (hp = tomerge->headers; hp; hp = hp->next) {
		if (hp->idx) {
			int i;
			
			for(i = 0; i < hp->idx; i++) {
				esl_event_add_header_string(event, ESL_STACK_PUSH, hp->name, hp->array[i]);
			}
		} else {
			esl_event_add_header_string(event, ESL_STACK_BOTTOM, hp->name, hp->value);
		}
	}
}

ESL_DECLARE(esl_status_t) esl_event_dup(esl_event_t **event, esl_event_t *todup)
{
	esl_event_header_t *hp;

	if (esl_event_create_subclass(event, ESL_EVENT_CLONE, todup->subclass_name) != ESL_SUCCESS) {
		return ESL_GENERR;
	}

	(*event)->event_id = todup->event_id;
	(*event)->event_user_data = todup->event_user_data;
	(*event)->bind_user_data = todup->bind_user_data;
	(*event)->flags = todup->flags;
	for (hp = todup->headers; hp; hp = hp->next) {
		if (todup->subclass_name && !strcmp(hp->name, "Event-Subclass")) {
			continue;
		}
		
		if (hp->idx) {
			int i;
			for (i = 0; i < hp->idx; i++) {
				esl_event_add_header_string(*event, ESL_STACK_PUSH, hp->name, hp->array[i]);
			}
		} else {
			esl_event_add_header_string(*event, ESL_STACK_BOTTOM, hp->name, hp->value);
		}
	}

	if (todup->body) {
		(*event)->body = DUP(todup->body);
	}

	(*event)->key = todup->key;

	return ESL_SUCCESS;
}


ESL_DECLARE(esl_status_t) esl_event_serialize(esl_event_t *event, char **str, esl_bool_t encode)
{
	esl_size_t len = 0;
	esl_event_header_t *hp;
	esl_size_t llen = 0, dlen = 0, blocksize = 512, encode_len = 1536, new_len = 0;
	char *buf;
	char *encode_buf = NULL;	/* used for url encoding of variables to make sure unsafe things stay out of the serialized copy */

	*str = NULL;

	dlen = blocksize * 2;

	if (!(buf = malloc(dlen))) {
		abort();
	}

	/* go ahead and give ourselves some space to work with, should save a few reallocs */
	if (!(encode_buf = malloc(encode_len))) {
		abort();
	}

	/* esl_log_printf(ESL_CHANNEL_LOG, ESL_LOG_INFO, "hit serialized!.\n"); */
	for (hp = event->headers; hp; hp = hp->next) {
		/*
		 * grab enough memory to store 3x the string (url encode takes one char and turns it into %XX)
		 * so we could end up with a string that is 3 times the originals length, unlikely but rather
		 * be safe than destroy the string, also add one for the null.  And try to be smart about using 
		 * the memory, allocate and only reallocate if we need more.  This avoids an alloc, free CPU
		 * destroying loop.
		 */

		if (hp->idx) {
			int i;
			new_len = 0;
			for(i = 0; i < hp->idx; i++) {
				new_len += (strlen(hp->array[i]) * 3) + 1;
			}
		} else {
			new_len = (strlen(hp->value) * 3) + 1;
		}

		if (encode_len < new_len) {
			char *tmp;

			/* keep track of the size of our allocation */
			encode_len = new_len;

			if (!(tmp = realloc(encode_buf, encode_len))) {
				abort();
			}

			encode_buf = tmp;
		}

		/* handle any bad things in the string like newlines : etc that screw up the serialized format */


		if (encode) {
			esl_url_encode(hp->value, encode_buf, encode_len);
		} else {
			esl_snprintf(encode_buf, encode_len, "%s", hp->value);
		}


		llen = strlen(hp->name) + strlen(encode_buf) + 8;

		if ((len + llen) > dlen) {
			char *m;
			char *old = buf;
			dlen += (blocksize + (len + llen));
			if ((m = realloc(buf, dlen))) {
				buf = m;
			} else {
				buf = old;
				abort();
			}
		}

		esl_snprintf(buf + len, dlen - len, "%s: %s\n", hp->name, *encode_buf == '\0' ? "_undef_" : encode_buf);
		len = strlen(buf);
	}

	/* we are done with the memory we used for encoding, give it back */
	esl_safe_free(encode_buf);

	if (event->body) {
		int blen = (int) strlen(event->body);
		llen = blen;

		if (blen) {
			llen += 25;
		} else {
			llen += 5;
		}

		if ((len + llen) > dlen) {
			char *m;
			char *old = buf;
			dlen += (blocksize + (len + llen));
			if ((m = realloc(buf, dlen))) {
				buf = m;
			} else {
				buf = old;
				abort();
			}
		}

		if (blen) {
			esl_snprintf(buf + len, dlen - len, "Content-Length: %d\n\n%s", blen, event->body);
		} else {
			esl_snprintf(buf + len, dlen - len, "\n");
		}
	} else {
		esl_snprintf(buf + len, dlen - len, "\n");
	}

	*str = buf;

	return ESL_SUCCESS;
}

ESL_DECLARE(esl_status_t) esl_event_create_json(esl_event_t **event, const char *json)
{
	esl_event_t *new_event;
	cJSON *cj, *cjp;


	if (!(cj = cJSON_Parse(json))) {
		return (esl_status_t) ESL_FALSE;
	}

	if (esl_event_create(&new_event, ESL_EVENT_CLONE) != ESL_SUCCESS) {
		cJSON_Delete(cj);
		return (esl_status_t) ESL_FALSE;
	}

	for (cjp = cj->child; cjp; cjp = cjp->next) {
		char *name = cjp->string;
		char *value = cjp->valuestring;

		if (name && value) {
			if (!strcasecmp(name, "_body")) {
				esl_event_add_body(new_event, value, ESL_VA_NONE);
			} else {
				if (!strcasecmp(name, "event-name")) {
					esl_event_del_header(new_event, "event-name");
					esl_name_event(value, &new_event->event_id);
				}

				esl_event_add_header_string(new_event, ESL_STACK_BOTTOM, name, value);
			}

		} else if (name) {
			if (cjp->type == cJSON_Array) {
				int i, x = cJSON_GetArraySize(cjp);

				for (i = 0; i < x; i++) {
					cJSON *item = cJSON_GetArrayItem(cjp, i);

					if (item != NULL && item->type == cJSON_String && item->valuestring) {
						esl_event_add_header_string(new_event, ESL_STACK_PUSH, name, item->valuestring);
					}
				}
			}
		}
	}
	
	cJSON_Delete(cj);
	*event = new_event;
	return ESL_SUCCESS;
}

ESL_DECLARE(esl_status_t) esl_event_serialize_json(esl_event_t *event, char **str)
{
	esl_event_header_t *hp;
	cJSON *cj;

	*str = NULL;
	
	cj = cJSON_CreateObject();

	for (hp = event->headers; hp; hp = hp->next) {
		if (hp->idx) {
			cJSON *a = cJSON_CreateArray();
			int i;

			for(i = 0; i < hp->idx; i++) {
				cJSON_AddItemToArray(a, cJSON_CreateString(hp->array[i]));
			}
			
			cJSON_AddItemToObject(cj, hp->name, a);
			
		} else {
			cJSON_AddItemToObject(cj, hp->name, cJSON_CreateString(hp->value));
		}
	}

	if (event->body) {
		int blen = (int) strlen(event->body);
		char tmp[25];

		esl_snprintf(tmp, sizeof(tmp), "%d", blen);

		cJSON_AddItemToObject(cj, "Content-Length", cJSON_CreateString(tmp));
		cJSON_AddItemToObject(cj, "_body", cJSON_CreateString(event->body));
	}

	*str = cJSON_Print(cj);
	cJSON_Delete(cj);
	
	return ESL_SUCCESS;
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
