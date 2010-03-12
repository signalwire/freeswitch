/*
 * Copyright (c) 2007, Anthony Minessale II
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
	"CHANNEL_ANSWER",
	"CHANNEL_HANGUP",
	"CHANNEL_HANGUP_COMPLETE",
	"CHANNEL_EXECUTE",
	"CHANNEL_EXECUTE_COMPLETE",
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
	"CALL_SECURE",
	"NAT",
	"RECORD_START",
	"RECORD_STOP",
	"CALL_UPDATE",
	"FAILURE",
	"SOCKET_DATA",
	"MEDIA_BUG_START",
	"MEDIA_BUG_START",
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


ESL_DECLARE(char *)esl_event_get_header(esl_event_t *event, const char *header_name)
{
	esl_event_header_t *hp;
	esl_ssize_t hlen = -1;
	unsigned long hash = 0;

	esl_assert(event);

	if (!header_name) return NULL;
	
	hash = esl_ci_hashfunc_default(header_name, &hlen);
	
	for (hp = event->headers; hp; hp = hp->next) {
		if ((!hp->hash || hash == hp->hash) && !strcasecmp(hp->name, header_name) ) {
			return hp->value;
		}
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
	esl_status_t status = ESL_FAIL;
	int x = 0;
	esl_ssize_t hlen = -1;
	unsigned long hash = 0;

	tp = event->headers;
	while (tp) {
		hp = tp;
		tp = tp->next;
		
		x++;
		esl_assert(x < 1000);
		hash = esl_ci_hashfunc_default(header_name, &hlen);

		if (hp->name && (!hp->hash || hash == hp->hash) && !strcasecmp(header_name, hp->name) && (esl_strlen_zero(val) || !strcmp(hp->value, val))) {
			if (lp) {
				lp->next = hp->next;
			} else {
				event->headers = hp->next;
			}
			if (hp == event->last_header || !hp->next) {
				event->last_header = lp;
			}
			FREE(hp->name);
			FREE(hp->value);
			memset(hp, 0, sizeof(*hp));
			FREE(hp);

			status = ESL_SUCCESS;
		} else {
			lp = hp;
		}
	}

	return status;
}

static esl_status_t esl_event_base_add_header(esl_event_t *event, esl_stack_t stack, const char *header_name, char *data)
{
	esl_event_header_t *header;
	esl_ssize_t hlen = -1;
	
	header = ALLOC(sizeof(*header));
	esl_assert(header);

	if ((event->flags & EF_UNIQ_HEADERS)) {
		esl_event_del_header(event, header_name);
	}

	memset(header, 0, sizeof(*header));

	header->name = DUP(header_name);
	header->value = data;
	header->hash = esl_ci_hashfunc_default(header->name, &hlen);
	
	if (stack == ESL_STACK_TOP) {
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
	esl_event_t *ep = *event, *this_event;
	esl_event_header_t *hp, *this_header;

	for (ep = *event ; ep ;) {
		this_event = ep;
		ep = ep->next;
		
		for (hp = this_event->headers; hp;) {
			this_header = hp;
			hp = hp->next;
			FREE(this_header->name);
			FREE(this_header->value);
			memset(this_header, 0, sizeof(*this_header));
			FREE(this_header);
		}
		FREE(this_event->body);
		FREE(this_event->subclass_name);
		memset(this_event, 0, sizeof(*this_event));
		FREE(this_event);
	}
	*event = NULL;
}



ESL_DECLARE(esl_status_t) esl_event_dup(esl_event_t **event, esl_event_t *todup)
{
	esl_event_header_t *hp;

	if (esl_event_create_subclass(event, ESL_EVENT_CLONE, todup->subclass_name) != ESL_SUCCESS) {
		return ESL_FAIL;
	}

	(*event)->event_id = todup->event_id;

	(*event)->event_user_data = todup->event_user_data;
	(*event)->bind_user_data = todup->bind_user_data;

	for (hp = todup->headers; hp; hp = hp->next) {
		esl_event_add_header_string(*event, ESL_STACK_BOTTOM, hp->name, hp->value);
	}

	if (todup->body) {
		(*event)->body = DUP(todup->body);
	}

	(*event)->key = todup->key;

	return ESL_SUCCESS;
}

ESL_DECLARE(esl_status_t) esl_event_serialize(esl_event_t *event, char **str, esl_bool_t encode)
{
	size_t len = 0;
	esl_event_header_t *hp;
	size_t llen = 0, dlen = 0, blocksize = 512, encode_len = 1536, new_len = 0;
	char *buf;
	char *encode_buf = NULL;	/* used for url encoding of variables to make sure unsafe things stay out of the serialized copy */
	int clen = 0;

	*str = NULL;

	dlen = blocksize * 2;

	if (!(buf = malloc(dlen))) {
		return ESL_FAIL;
	}

	/* go ahead and give ourselves some space to work with, should save a few reallocs */
	if (!(encode_buf = malloc(encode_len))) {
		esl_safe_free(buf);
		return ESL_FAIL;
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

		if (!strcasecmp(hp->name, "content-length")) {
			clen++;
		}


		new_len = (strlen(hp->value) * 3) + 1;

		if (encode_len < new_len) {
			char *tmp;
			/* esl_log_printf(ESL_CHANNEL_LOG, ESL_LOG_INFO, "Allocing %d was %d.\n", ((strlen(hp->value) * 3) + 1), encode_len); */
			/* we can use realloc for initial alloc as well, if encode_buf is zero it treats it as a malloc */

			/* keep track of the size of our allocation */
			encode_len = new_len;

			if (!(tmp = realloc(encode_buf, encode_len))) {
				/* oh boy, ram's gone, give back what little we grabbed and bail */
				esl_safe_free(buf);
				esl_safe_free(encode_buf);
				return ESL_FAIL;
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
			dlen += (blocksize + (len + llen));
			if ((m = realloc(buf, dlen))) {
				buf = m;
			} else {
				/* we seem to be out of memory trying to resize the serialize string, give back what we already have and give up */
				esl_safe_free(buf);
				esl_safe_free(encode_buf);
				return ESL_FAIL;
			}
		}

		snprintf(buf + len, dlen - len, "%s: %s\n", hp->name, *encode_buf == '\0' ? "_undef_" : encode_buf);
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
			dlen += (blocksize + (len + llen));
			if ((m = realloc(buf, dlen))) {
				buf = m;
			} else {
				esl_safe_free(buf);
				return ESL_FAIL;
			}
		}
		
		if (blen && !clen) {
			snprintf(buf + len, dlen - len, "Content-Length: %d\n\n%s", (int)strlen(event->body), event->body);
		} else {
			snprintf(buf + len, dlen - len, "\n");
		}
	} else {
		snprintf(buf + len, dlen - len, "\n");
	}


	*str = buf;

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
