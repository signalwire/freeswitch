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

#include "openzap.h"
#include "zap_isdn.h"
#include <stdarg.h>
#ifdef ZAP_WANPIPE_SUPPORT
#include "zap_wanpipe.h"
#endif
#ifdef ZAP_ZT_SUPPORT
#include "zap_zt.h"
#endif

static int time_is_init = 0;

static void time_init(void)
{
#ifdef WIN32
	timeBeginPeriod(1);
#endif
	time_is_init = 1;
}

static void time_end(void)
{
#ifdef WIN32
	timeEndPeriod(1);
#endif
	time_is_init = 0;
}

zap_time_t zap_current_time_in_ms(void)
{
#ifdef WIN32
	return timeGetTime();
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return ((tv.tv_sec * 1000) + (tv.tv_usec / 1000));
#endif
}

static struct {
	zap_hash_t *interface_hash;
	zap_mutex_t *mutex;
	struct zap_span spans[ZAP_MAX_SPANS_INTERFACE];
	uint32_t span_index;
} globals;


/* enum lookup funcs */
ZAP_ENUM_NAMES(TONEMAP_NAMES, TONEMAP_STRINGS)
ZAP_STR2ENUM(zap_str2zap_tonemap, zap_tonemap2str, zap_tonemap_t, TONEMAP_NAMES, ZAP_TONEMAP_INVALID)

ZAP_ENUM_NAMES(OOB_NAMES, OOB_STRINGS)
ZAP_STR2ENUM(zap_str2zap_oob_event, zap_oob_event2str, zap_oob_event_t, OOB_NAMES, ZAP_OOB_INVALID)

ZAP_ENUM_NAMES(TRUNK_TYPE_NAMES, TRUNK_STRINGS)
ZAP_STR2ENUM(zap_str2zap_trunk_type, zap_trunk_type2str, zap_trunk_type_t, TRUNK_TYPE_NAMES, ZAP_TRUNK_NONE)

ZAP_ENUM_NAMES(SIGNAL_NAMES, SIGNAL_STRINGS)
ZAP_STR2ENUM(zap_str2zap_signal_event, zap_signal_event2str, zap_signal_event_t, SIGNAL_NAMES, ZAP_SIGEVENT_INVALID)

ZAP_ENUM_NAMES(CHANNEL_STATE_NAMES, CHANNEL_STATE_STRINGS)
ZAP_STR2ENUM(zap_str2zap_channel_state, zap_channel_state2str, zap_channel_state_t, CHANNEL_STATE_NAMES, ZAP_CHANNEL_STATE_INVALID)

static char *cut_path(char *in)
{
	char *p, *ret = in;
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

static void null_logger(char *file, const char *func, int line, int level, char *fmt, ...)
{
	if (file && func && line && level && fmt) {
		return;
	}
	return;
}


static char *LEVEL_NAMES[] = {
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

static int zap_log_level = 7;

static void default_logger(char *file, const char *func, int line, int level, char *fmt, ...)
{
	char *fp;
	char data[1024];
	va_list ap;

	if (level < 0 || level > 7) {
		level = 7;
	}
	if (level > zap_log_level) {
		return;
	}
	
	fp = cut_path(file);

	va_start(ap, fmt);

	vsnprintf(data, sizeof(data), fmt, ap);


	fprintf(stderr, "[%s] %s:%d %s() %s", LEVEL_NAMES[level], file, line, func, data);

	va_end(ap);

}

zap_logger_t zap_log = null_logger;

void zap_global_set_logger(zap_logger_t logger)
{
	if (logger) {
		zap_log = logger;
	} else {
		zap_log = null_logger;
	}
}

void zap_global_set_default_logger(int level)
{
	if (level < 0 || level > 7) {
		level = 7;
	}

	zap_log = default_logger;
	zap_log_level = level;
}

static int equalkeys(void *k1, void *k2)
{
    return strcmp((char *) k1, (char *) k2) ? 0 : 1;
}

static uint32_t hashfromstring(void *ky)
{
	unsigned char *str = (unsigned char *) ky;
	uint32_t hash = 0;
    int c;
	
	while ((c = *str++)) {
        hash = c + (hash << 6) + (hash << 16) - hash;
	}

    return hash;
}

zap_status_t zap_span_create(zap_io_interface_t *zio, zap_span_t **span)
{
	zap_span_t *new_span = NULL;
	zap_status_t status = ZAP_FAIL;

	assert(zio != NULL);

	zap_mutex_lock(globals.mutex);
	if (globals.span_index < ZAP_MAX_SPANS_INTERFACE) {
		new_span = &globals.spans[++globals.span_index];
		memset(new_span, 0, sizeof(*new_span));
		zap_set_flag(new_span, ZAP_SPAN_CONFIGURED);
		new_span->span_id = globals.span_index;
		new_span->zio = zio;
		zap_mutex_create(&new_span->mutex);
		zap_copy_string(new_span->tone_map[ZAP_TONEMAP_DIAL], "%(1000,0,350,440)", ZAP_TONEMAP_LEN);
		zap_copy_string(new_span->tone_map[ZAP_TONEMAP_RING], "%(2000,4000,440,480)", ZAP_TONEMAP_LEN);
		zap_copy_string(new_span->tone_map[ZAP_TONEMAP_BUSY], "%(500,500,480,620)", ZAP_TONEMAP_LEN);
		zap_copy_string(new_span->tone_map[ZAP_TONEMAP_ATTN], "%(100,100,1400,2060,2450,2600)", ZAP_TONEMAP_LEN);
		new_span->trunk_type = ZAP_TRUNK_NONE;
		*span = new_span;
		status = ZAP_SUCCESS;
	}
	zap_mutex_unlock(globals.mutex);	

	return status;
}

zap_status_t zap_span_close_all(zap_io_interface_t *zio)
{
	zap_span_t *span;
	uint32_t i, j;

	zap_mutex_lock(globals.mutex);
	for(i = 0; i < globals.span_index; i++) {
		span = &globals.spans[i];

		for(j = 0; j < span->chan_count; j++) {
			if (zap_test_flag((&span->channels[j]), ZAP_CHANNEL_CONFIGURED)) {
				zap_mutex_destroy(&span->channels[j].mutex);
				zap_buffer_destroy(&span->channels[j].digit_buffer);
				zap_buffer_destroy(&span->channels[j].dtmf_buffer);
			}
		}

		if (span->mutex) {
			zap_mutex_destroy(&span->mutex);
		}
		if (span->isdn_data) {
			free(span->isdn_data);
		}
		if (span->analog_data) {
			free(span->isdn_data);
		}
	}
	zap_mutex_unlock(globals.mutex);

	return i ? ZAP_SUCCESS : ZAP_FAIL;
}

zap_status_t zap_span_load_tones(zap_span_t *span, char *mapname)
{
	zap_config_t cfg;
	char *var, *val;
	int x = 0;

	if (!zap_config_open_file(&cfg, "tones.conf")) {
		snprintf(span->last_error, sizeof(span->last_error), "error loading tones.");
		return ZAP_FAIL;
	}
	
	while (zap_config_next_pair(&cfg, &var, &val)) {
		if (!strcasecmp(cfg.category, mapname) && var && val) {
			int index = zap_str2zap_tonemap(var);
			char valbuf[512];

			if (index > ZAP_TONEMAP_INVALID || index == ZAP_TONEMAP_NONE) {
				zap_log(ZAP_LOG_WARNING, "Unknown tone name %s\n", var);
			} else {
				char *p, *next;
				int i = 0;
				zap_set_string(valbuf, val);
				val = valbuf;
				if ((p = strchr(val, ':'))) {
					*p++ = '\0';
					do {
						teletone_process_t this;
						next = strchr(p, ',');
						this = atof(p);
						span->tone_detect_map[index].freqs[i++] = this;
						if (next) {
							p = next + 1;
						}
					} while (next);

					zap_copy_string(span->tone_map[index], val, sizeof(span->tone_map[index]));
					
				}
				zap_log(ZAP_LOG_DEBUG, "added tone [%s] = [%s]\n", var, val);
				zap_copy_string(span->tone_map[index], val, sizeof(span->tone_map[index]));
				x++;
			}
		}
	}
	
	if (!x) {
		snprintf(span->last_error, sizeof(span->last_error), "error loading tones.");
		return ZAP_FAIL;
	}

	return ZAP_SUCCESS;
	
}

zap_status_t zap_span_add_channel(zap_span_t *span, zap_socket_t sockfd, zap_chan_type_t type, zap_channel_t **chan)
{
	if (span->chan_count < ZAP_MAX_CHANNELS_SPAN) {
		zap_channel_t *new_chan;
		new_chan = &span->channels[++span->chan_count];
		new_chan->type = type;
		new_chan->sockfd = sockfd;
		new_chan->zio = span->zio;
		new_chan->span_id = span->span_id;
		new_chan->chan_id = span->chan_count;
		new_chan->span = span;
		zap_mutex_create(&new_chan->mutex);
		zap_buffer_create(&new_chan->digit_buffer, 128, 128, 0);
		zap_set_flag(new_chan, ZAP_CHANNEL_CONFIGURED | ZAP_CHANNEL_READY);
		*chan = new_chan;
		return ZAP_SUCCESS;
	}

	return ZAP_FAIL;
}

zap_status_t zap_span_find(uint32_t id, zap_span_t **span)
{
	zap_span_t *fspan;

	if (id > ZAP_MAX_SPANS_INTERFACE) {
		return ZAP_FAIL;
	}

	zap_mutex_lock(globals.mutex);
	fspan = &globals.spans[id];
	zap_mutex_unlock(globals.mutex);

	if (!zap_test_flag(fspan, ZAP_SPAN_CONFIGURED)) {
		return ZAP_FAIL;
	}

	*span = fspan;

	return ZAP_SUCCESS;
	
}

zap_status_t zap_span_set_event_callback(zap_span_t *span, zio_event_cb_t event_callback)
{
	zap_mutex_lock(span->mutex);
	span->event_callback = event_callback;
	zap_mutex_unlock(span->mutex);
	return ZAP_SUCCESS;
}


zap_status_t zap_span_poll_event(zap_span_t *span, uint32_t ms)
{
	assert(span->zio != NULL);

	if (span->zio->poll_event) {
		return span->zio->poll_event(span, ms);
	} else {
		zap_log(ZAP_LOG_ERROR, "poll_event method not implemented in module %s!", span->zio->name);
	}

	return ZAP_NOTIMPL;
}

zap_status_t zap_span_next_event(zap_span_t *span, zap_event_t **event)
{
	assert(span->zio != NULL);

	if (span->zio->next_event) {
		return span->zio->next_event(span, event);
	} else {
		zap_log(ZAP_LOG_ERROR, "next_event method not implemented in module %s!", span->zio->name);
	}
	
	return ZAP_NOTIMPL;
}

zap_status_t zap_channel_set_event_callback(zap_channel_t *zchan, zio_event_cb_t event_callback)
{
	zap_mutex_lock(zchan->mutex);
	zchan->event_callback = event_callback;
	zap_mutex_unlock(zchan->mutex);
	return ZAP_SUCCESS;
}

zap_status_t zap_channel_clear_token(zap_channel_t *zchan, int32_t token_id)
{
	zap_status_t status = ZAP_FAIL;
	
	zap_mutex_lock(zchan->mutex);
	if (token_id == -1) {
		memset(zchan->tokens, 0, sizeof(zchan->tokens));
		zchan->token_count = 0;
	} else if (*zchan->tokens[token_id] != '\0') {
		char tokens[ZAP_MAX_TOKENS][ZAP_TOKEN_STRLEN];
		int32_t i, count = zchan->token_count;
		memcpy(tokens, zchan->tokens, sizeof(tokens));
		memset(zchan->tokens, 0, sizeof(zchan->tokens));
		zchan->token_count = 0;		

		for (i = 0; i < count; i++) {
			if (i != token_id) {
				zap_copy_string(zchan->tokens[zchan->token_count], tokens[i], sizeof(zchan->tokens[zchan->token_count]));
				zchan->token_count++;
			}
		}

		status = ZAP_SUCCESS;
	}
	zap_mutex_unlock(zchan->mutex);

	return status;
}

void zap_channel_rotate_tokens(zap_channel_t *zchan)
{
	if (zchan->token_count) {
		memmove(zchan->tokens[1], zchan->tokens[0], zchan->token_count * ZAP_TOKEN_STRLEN);
		zap_copy_string(zchan->tokens[0], zchan->tokens[zchan->token_count], ZAP_TOKEN_STRLEN);
		*zchan->tokens[zchan->token_count] = '\0';
	}
}

zap_status_t zap_channel_add_token(zap_channel_t *zchan, char *token)
{
	zap_status_t status = ZAP_FAIL;

	zap_mutex_lock(zchan->mutex);
	if (zchan->token_count < ZAP_MAX_TOKENS) {
		memmove(zchan->tokens[1], zchan->tokens[0], zchan->token_count * ZAP_TOKEN_STRLEN);
		zap_copy_string(zchan->tokens[0], token, ZAP_TOKEN_STRLEN);
		zchan->token_count++;
	}
	zap_mutex_unlock(zchan->mutex);

	return status;
}


zap_status_t zap_channel_set_state(zap_channel_t *zchan, zap_channel_state_t state)
{
	int ok = 1;

	zap_mutex_unlock(zchan->mutex);

	if (zchan->state == ZAP_CHANNEL_STATE_DOWN) {

		switch(state) {
		case ZAP_CHANNEL_STATE_BUSY:
			ok = 0;
			break;
		default:
			break;
		}
	}

	if (state == zchan->state) {
		ok = 0;
	}

	if (ok) {
		zap_set_flag(zchan, ZAP_CHANNEL_STATE_CHANGE);	
		zchan->last_state = zchan->state; 
		zchan->state = state;
	}

	zap_mutex_unlock(zchan->mutex);

	return ok ? ZAP_SUCCESS : ZAP_FAIL;
}

zap_status_t zap_channel_open_any(uint32_t span_id, zap_direction_t direction, zap_channel_t **zchan)
{
	zap_status_t status = ZAP_FAIL;
	zap_channel_t *check;
	uint32_t i,j;
	zap_span_t *span;
	uint32_t span_max;

	zap_mutex_lock(globals.mutex);

	if (span_id) {
		span_max = span_id;
		j = span_id;
	} else {
		span_max = globals.span_index;
		if (direction == ZAP_TOP_DOWN) {
			j = 1;
		} else {
			j = span_max;
		}
	}

	for(;;) {
		span = &globals.spans[j];
		if (!zap_test_flag(span, ZAP_SPAN_CONFIGURED)) {
			goto next_loop;
		}

		if (direction == ZAP_TOP_DOWN) {
			if (j > span_max) {
				break;
			}
		} else {
			if (j == 0) {
				break;
			}
		}
		
		if (direction == ZAP_TOP_DOWN) {
			i = 1;
		} else {
			i = span->chan_count;
		}	
		
		for(;;) {
		
			if (direction == ZAP_TOP_DOWN) {
				if (i > span->chan_count) {
					break;
				}
			} else {
				if (i == 0) {
					break;
				}
			}
			
			check = &span->channels[i];
			
			if (zap_test_flag(check, ZAP_CHANNEL_READY) && !zap_test_flag(check, ZAP_CHANNEL_INUSE)) {

				status = check->zio->open(check);
				
				if (status == ZAP_SUCCESS) {
					zap_set_flag(check, ZAP_CHANNEL_INUSE);
					zap_channel_open_chan(check);
					*zchan = check;
					return status;
				}
			}
			
			if (direction == ZAP_TOP_DOWN) {
				i++;
			} else {
				i--;
			}

		}
		
	next_loop:

		if (direction == ZAP_TOP_DOWN) {
			j++;
		} else {
			j--;
		}
	}

	zap_mutex_unlock(globals.mutex);

	return status;
}

static zap_status_t zap_channel_reset(zap_channel_t *zchan)
{
	zap_clear_flag(zchan, ZAP_CHANNEL_OPEN);
	zchan->event_callback = NULL;
	zap_clear_flag(zchan, ZAP_CHANNEL_DTMF_DETECT);
	zap_clear_flag(zchan, ZAP_CHANNEL_SUPRESS_DTMF);
	zap_channel_done(zchan);
	zap_clear_flag_locked(zchan, ZAP_CHANNEL_HOLD);
	memset(zchan->tokens, 0, sizeof(zchan->tokens));
	zchan->token_count = 0;

	if (zchan->tone_session.buffer) {
		teletone_destroy_session(&zchan->tone_session);
		memset(&zchan->tone_session, 0, sizeof(zchan->tone_session));
	}

	if (zchan->dtmf_buffer) {
		zap_buffer_zero(zchan->dtmf_buffer);
	}

	if (zchan->digit_buffer) {
		zap_buffer_zero(zchan->digit_buffer);
	}

	zchan->dtmf_on = zchan->dtmf_off = 0;

	if (zap_test_flag(zchan, ZAP_CHANNEL_TRANSCODE)) {
		zchan->effective_codec = zchan->native_codec;
		zchan->packet_len = zchan->native_interval * (zchan->effective_codec == ZAP_CODEC_SLIN ? 16 : 8);
		zap_clear_flag(zchan, ZAP_CHANNEL_TRANSCODE);
	}

	return ZAP_SUCCESS;
}

zap_status_t zap_channel_open_chan(zap_channel_t *zchan)
{
	zap_status_t status = ZAP_FAIL;
	
	if ((status = zap_mutex_trylock(zchan->mutex)) != ZAP_SUCCESS) {
		return status;
	}
	if (zap_test_flag(zchan, ZAP_CHANNEL_READY) && !zap_test_flag(zchan, ZAP_CHANNEL_OPEN)) {
		status = zchan->span->zio->open(zchan);
		if (status == ZAP_SUCCESS) {
			zap_set_flag(zchan, ZAP_CHANNEL_OPEN);
		}
	}
	zap_mutex_unlock(zchan->mutex);

	return status;
}

zap_status_t zap_channel_open(uint32_t span_id, uint32_t chan_id, zap_channel_t **zchan)
{
	zap_status_t status = ZAP_FAIL;

	zap_mutex_lock(globals.mutex);

	if (span_id < ZAP_MAX_SPANS_INTERFACE && chan_id < ZAP_MAX_CHANNELS_SPAN) {
		zap_channel_t *check;

		check = &globals.spans[span_id].channels[chan_id];

		if ((status = zap_mutex_trylock(check->mutex)) != ZAP_SUCCESS) {
			goto done;
		}
		
		if (zap_test_flag(check, ZAP_CHANNEL_READY) && !zap_test_flag(check, ZAP_CHANNEL_INUSE)) {
			if (!zap_test_flag(check, ZAP_CHANNEL_OPEN)) {
				status = check->zio->open(check);
				if (status == ZAP_SUCCESS) {
					zap_set_flag(check, ZAP_CHANNEL_OPEN);
				}
			} else {
				status = ZAP_SUCCESS;
			}
			zap_set_flag(check, ZAP_CHANNEL_INUSE);
			*zchan = check;
		}
		zap_mutex_unlock(check->mutex);
	}

 done:
	
	zap_mutex_unlock(globals.mutex);

	return status;
}

zap_status_t zap_channel_outgoing_call(zap_channel_t *zchan)
{
	assert(zchan != NULL);
	if (zchan->span->outgoing_call) {
		return zchan->span->outgoing_call(zchan);
	} else {
		zap_log(ZAP_LOG_ERROR, "outgoing_call method not implemented!\n");
	}
	
	return ZAP_FAIL;
}

zap_status_t zap_channel_done(zap_channel_t *zchan)
{

	assert(zchan != NULL);

	memset(&zchan->caller_data, 0, sizeof(zchan->caller_data));
	zap_clear_flag_locked(zchan, ZAP_CHANNEL_INUSE);

	return ZAP_SUCCESS;
}

zap_status_t zap_channel_use(zap_channel_t *zchan)
{

	assert(zchan != NULL);

	zap_set_flag_locked(zchan, ZAP_CHANNEL_INUSE);

	return ZAP_SUCCESS;
}

zap_status_t zap_channel_close(zap_channel_t **zchan)
{
	zap_channel_t *check;
	zap_status_t status = ZAP_FAIL;

	assert(zchan != NULL);
	check = *zchan;
	assert(check != NULL);
	*zchan = NULL;

	zap_mutex_lock(check->mutex);
	if (zap_test_flag(check, ZAP_CHANNEL_OPEN)) {
		status = check->zio->close(check);
		if (status == ZAP_SUCCESS) {
			zap_channel_reset(check);
			*zchan = NULL;
		}
	}

	zap_mutex_unlock(check->mutex);
	
	return status;
}


static zap_status_t zchan_activate_dtmf_buffer(zap_channel_t *zchan)
{

	if (zchan->dtmf_buffer) {
		return ZAP_SUCCESS;
	}

	if (zap_buffer_create(&zchan->dtmf_buffer, 1024, 3192, 0) != ZAP_SUCCESS) {
		zap_log(ZAP_LOG_ERROR, "Failed to allocate DTMF Buffer!\n");
		snprintf(zchan->last_error, sizeof(zchan->last_error), "buffer error");
		return ZAP_FAIL;
	} else {
		zap_log(ZAP_LOG_DEBUG, "Created DTMF Buffer!\n");
	}
	
	if (!zchan->dtmf_on) {
		zchan->dtmf_on = 250;
	}

	if (!zchan->dtmf_off) {
		zchan->dtmf_off = 50;
	}
	
	memset(&zchan->tone_session, 0, sizeof(zchan->tone_session));
	teletone_init_session(&zchan->tone_session, 0, NULL, NULL);
	
	zchan->tone_session.rate = 8000;
	zchan->tone_session.duration = zchan->dtmf_on * (zchan->tone_session.rate / 1000);
	zchan->tone_session.wait = zchan->dtmf_off * (zchan->tone_session.rate / 1000);
	/*
	  zchan->tone_session.debug = 1;
	  zchan->tone_session.debug_stream = stdout;
	*/

	return ZAP_SUCCESS;
}

zap_status_t zap_channel_command(zap_channel_t *zchan, zap_command_t command, void *obj)
{
	zap_status_t status = ZAP_FAIL;

	assert(zchan != NULL);
	assert(zchan->zio != NULL);

	zap_mutex_lock(zchan->mutex);

	switch(command) {
	case ZAP_COMMAND_SET_INTERVAL:
		{
			if (!zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_INTERVAL)) {
				zchan->effective_interval = ZAP_COMMAND_OBJ_INT;
				if (zchan->effective_interval == zchan->native_interval) {
					zap_clear_flag(zchan, ZAP_CHANNEL_BUFFER);
				} else {
					zap_set_flag(zchan, ZAP_CHANNEL_BUFFER);
				}
				zchan->packet_len = zchan->native_interval * (zchan->effective_codec == ZAP_CODEC_SLIN ? 16 : 8);
				GOTO_STATUS(done, ZAP_SUCCESS);
			}
		}
		break;
	case ZAP_COMMAND_GET_INTERVAL:
		{
			if (!zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_INTERVAL)) {
				ZAP_COMMAND_OBJ_INT = zchan->effective_interval;
				GOTO_STATUS(done, ZAP_SUCCESS);
			}
		}
		break;
	case ZAP_COMMAND_SET_CODEC:
		{
			if (!zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_CODECS)) {
				zchan->effective_codec = ZAP_COMMAND_OBJ_INT;
				
				if (zchan->effective_codec == zchan->native_codec) {
					zap_clear_flag(zchan, ZAP_CHANNEL_TRANSCODE);
				} else {
					zap_set_flag(zchan, ZAP_CHANNEL_TRANSCODE);
				}
				zchan->packet_len = zchan->native_interval * (zchan->effective_codec == ZAP_CODEC_SLIN ? 16 : 8);
				GOTO_STATUS(done, ZAP_SUCCESS);
			}
		}
		break;

	case ZAP_COMMAND_SET_NATIVE_CODEC:
		{
			if (!zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_CODECS)) {
				zchan->effective_codec = zchan->native_codec;
				zap_clear_flag(zchan, ZAP_CHANNEL_TRANSCODE);
				zchan->packet_len = zchan->native_interval * (zchan->effective_codec == ZAP_CODEC_SLIN ? 16 : 8);
				GOTO_STATUS(done, ZAP_SUCCESS);
			}
		}
		break;

	case ZAP_COMMAND_GET_CODEC: 
		{
			if (!zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_CODECS)) {
				ZAP_COMMAND_OBJ_INT = zchan->effective_codec;
				GOTO_STATUS(done, ZAP_SUCCESS);
			}
		}
		break;
	case ZAP_COMMAND_GET_NATIVE_CODEC: 
		{
			if (!zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_CODECS)) {
				ZAP_COMMAND_OBJ_INT = zchan->native_codec;
				GOTO_STATUS(done, ZAP_SUCCESS);
			}
		}
		break;
	case ZAP_COMMAND_ENABLE_PROGRESS_DETECT:
		{
			/* if they don't have thier own, use ours */
			teletone_multi_tone_init(&zchan->span->tone_finder[ZAP_TONEMAP_DIAL], &zchan->span->tone_detect_map[ZAP_TONEMAP_DIAL]);
			teletone_multi_tone_init(&zchan->span->tone_finder[ZAP_TONEMAP_RING], &zchan->span->tone_detect_map[ZAP_TONEMAP_RING]);
			teletone_multi_tone_init(&zchan->span->tone_finder[ZAP_TONEMAP_BUSY], &zchan->span->tone_detect_map[ZAP_TONEMAP_BUSY]);
			zap_set_flag(zchan, ZAP_CHANNEL_PROGRESS_DETECT);
			GOTO_STATUS(done, ZAP_SUCCESS);
		}
		break;
	case ZAP_COMMAND_DISABLE_PROGRESS_DETECT:
		{
			zap_clear_flag_locked(zchan, ZAP_CHANNEL_PROGRESS_DETECT);
			GOTO_STATUS(done, ZAP_SUCCESS);
		}
		break;
	case ZAP_COMMAND_ENABLE_TONE_DETECT:
		{
			/* if they don't have thier own, use ours */
			if (!zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_DTMF)) {
				zap_tone_type_t tt = ZAP_COMMAND_OBJ_INT;
				if (tt == ZAP_TONE_DTMF) {
					teletone_dtmf_detect_init (&zchan->dtmf_detect, 8000);
					zap_set_flag_locked(zchan, ZAP_CHANNEL_DTMF_DETECT);
					zap_set_flag_locked(zchan, ZAP_CHANNEL_SUPRESS_DTMF);
					GOTO_STATUS(done, ZAP_SUCCESS);
				} else {
					snprintf(zchan->last_error, sizeof(zchan->last_error), "invalid command");
					GOTO_STATUS(done, ZAP_FAIL);
				}
			}
		}
		break;
	case ZAP_COMMAND_DISABLE_TONE_DETECT:
		{
			if (!zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_DTMF)) {
				zap_tone_type_t tt = ZAP_COMMAND_OBJ_INT;
                if (tt == ZAP_TONE_DTMF) {
                    teletone_dtmf_detect_init (&zchan->dtmf_detect, 8000);
                    zap_clear_flag(zchan, ZAP_CHANNEL_DTMF_DETECT);
					zap_clear_flag(zchan, ZAP_CHANNEL_SUPRESS_DTMF);
					GOTO_STATUS(done, ZAP_SUCCESS);
                } else {
                    snprintf(zchan->last_error, sizeof(zchan->last_error), "invalid command");
					GOTO_STATUS(done, ZAP_FAIL);
                }
			}
		}
	case ZAP_COMMAND_GET_DTMF_ON_PERIOD:
		{
			if (!zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_DTMF)) {
				ZAP_COMMAND_OBJ_INT = zchan->dtmf_on;
				GOTO_STATUS(done, ZAP_SUCCESS);
			}
		}
		break;
	case ZAP_COMMAND_GET_DTMF_OFF_PERIOD:
		{
			if (!zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_DTMF)) {
				ZAP_COMMAND_OBJ_INT = zchan->dtmf_on;
				GOTO_STATUS(done, ZAP_SUCCESS);
			}
		}
		break;
	case ZAP_COMMAND_SET_DTMF_ON_PERIOD:
		{
			if (!zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_DTMF)) {
				int val = ZAP_COMMAND_OBJ_INT;
				if (val > 10 && val < 1000) {
					zchan->dtmf_on = val;
					GOTO_STATUS(done, ZAP_SUCCESS);
				} else {
					snprintf(zchan->last_error, sizeof(zchan->last_error), "invalid value %d range 10-1000", val);
					GOTO_STATUS(done, ZAP_FAIL);
				}
			}
		}
		break;
	case ZAP_COMMAND_SET_DTMF_OFF_PERIOD:
		{
			if (!zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_DTMF)) {
				int val = ZAP_COMMAND_OBJ_INT;
				if (val > 10 && val < 1000) {
					zchan->dtmf_off = val;
					GOTO_STATUS(done, ZAP_SUCCESS);
				} else {
					snprintf(zchan->last_error, sizeof(zchan->last_error), "invalid value %d range 10-1000", val);
					GOTO_STATUS(done, ZAP_FAIL);
				}
			}
		}
		break;
	case ZAP_COMMAND_SEND_DTMF:
		{
			if (!zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_DTMF)) {
				char *cur;
				char *digits = ZAP_COMMAND_OBJ_CHAR_P;
				int x = 0;

				if (!zchan->dtmf_buffer) {
					if ((status = zchan_activate_dtmf_buffer(zchan)) != ZAP_SUCCESS) {
						GOTO_STATUS(done, status);
					}
				}
				zap_log(ZAP_LOG_DEBUG, "Adding DTMF SEQ [%s]\n", digits);	
				
				for (cur = digits; *cur; cur++) {
					int wrote = 0;
					if ((wrote = teletone_mux_tones(&zchan->tone_session, &zchan->tone_session.TONES[(int)*cur]))) {
						zap_buffer_write(zchan->dtmf_buffer, zchan->tone_session.buffer, wrote * 2);
						x++;
					}
				}
				
				zchan->skip_read_frames = 200 * x;
				GOTO_STATUS(done, ZAP_SUCCESS);
			}
		}
		break;
	default:
		break;
	}

	if (!zchan->zio->command) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "method not implemented");
		GOTO_STATUS(done, ZAP_FAIL);
	}

    status = zchan->zio->command(zchan, command, obj);

 done:
	zap_mutex_unlock(zchan->mutex);
	return status;

}

zap_status_t zap_channel_wait(zap_channel_t *zchan, zap_wait_flag_t *flags, int32_t to)
{
	assert(zchan != NULL);
	assert(zchan->zio != NULL);

    if (!zap_test_flag(zchan, ZAP_CHANNEL_OPEN)) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "channel not open");
        return ZAP_FAIL;
    }

	if (!zchan->zio->wait) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "method not implemented");
		return ZAP_FAIL;
	}

    return zchan->zio->wait(zchan, flags, to);

}

/*******************************/
ZIO_CODEC_FUNCTION(zio_slin2ulaw)
{
	int16_t sln_buf[512] = {0}, *sln = sln_buf;
	uint8_t *lp = data;
	uint32_t i;
	zap_size_t len = *datalen;

	if (max > len) {
		max = len;
	}

	memcpy(sln, data, max);
	
	for(i = 0; i < max; i++) {
		*lp++ = linear_to_ulaw(*sln++);
	}

	*datalen = max / 2;

	return ZAP_SUCCESS;

}


ZIO_CODEC_FUNCTION(zio_ulaw2slin)
{
	int16_t *sln = data;
	uint8_t law[1024] = {0}, *lp = law;
	uint32_t i;
	zap_size_t len = *datalen;
	
	if (max > len) {
		max = len;
	}

	memcpy(law, data, max);

	for(i = 0; i < max; i++) {
		*sln++ = ulaw_to_linear(*lp++);
	}
	
	*datalen = max * 2;

	return ZAP_SUCCESS;
}

ZIO_CODEC_FUNCTION(zio_slin2alaw)
{
	int16_t sln_buf[512] = {0}, *sln = sln_buf;
	uint8_t *lp = data;
	uint32_t i;
	zap_size_t len = *datalen;

	if (max > len) {
		max = len;
	}

	memcpy(sln, data, max);
	
	for(i = 0; i < max; i++) {
		*lp++ = linear_to_alaw(*sln++);
	}

	*datalen = max / 2;

	return ZAP_SUCCESS;

}


ZIO_CODEC_FUNCTION(zio_alaw2slin)
{
	int16_t *sln = data;
	uint8_t law[1024] = {0}, *lp = law;
	uint32_t i;
	zap_size_t len = *datalen;
	
	if (max > len) {
		max = len;
	}

	memcpy(law, data, max);

	for(i = 0; i < max; i++) {
		*sln++ = alaw_to_linear(*lp++);
	}

	*datalen = max * 2;

	return ZAP_SUCCESS;
}

ZIO_CODEC_FUNCTION(zio_ulaw2alaw)
{
	zap_size_t len = *datalen;
	uint32_t i;
	uint8_t *lp = data;

	if (max > len) {
        max = len;
    }

	for(i = 0; i < max; i++) {
		*lp = ulaw_to_alaw(*lp);
		lp++;
	}

	return ZAP_SUCCESS;
}

ZIO_CODEC_FUNCTION(zio_alaw2ulaw)
{
	zap_size_t len = *datalen;
	uint32_t i;
	uint8_t *lp = data;

	if (max > len) {
        max = len;
    }

	for(i = 0; i < max; i++) {
		*lp = alaw_to_ulaw(*lp);
		lp++;
	}

	return ZAP_SUCCESS;
}

/******************************/

zap_size_t zap_channel_dequeue_dtmf(zap_channel_t *zchan, char *dtmf, zap_size_t len)
{
	zap_size_t bytes = 0;

	assert(zchan != NULL);

	if (zap_buffer_inuse(zchan->digit_buffer)) {
		zap_mutex_lock(zchan->mutex);
		if ((bytes = zap_buffer_read(zchan->digit_buffer, dtmf, len)) > 0) {
			*(dtmf + bytes) = '\0';
		}
		zap_mutex_unlock(zchan->mutex);
	}

	return bytes;
}


zap_status_t zap_channel_queue_dtmf(zap_channel_t *zchan, const char *dtmf)
{
	zap_status_t status;
	register zap_size_t len, inuse;
	zap_size_t wr = 0;
	const char *p;
	
	assert(zchan != NULL);

	zap_mutex_lock(zchan->mutex);

	inuse = zap_buffer_inuse(zchan->digit_buffer);
	len = strlen(dtmf);
	
	if (len + inuse > zap_buffer_len(zchan->digit_buffer)) {
		zap_buffer_toss(zchan->digit_buffer, strlen(dtmf));
	}

	p = dtmf;
	while (wr < len && p) {
		if (zap_is_dtmf(*p)) {
			wr++;
		} else {
			break;
		}
		p++;
	}

	status = zap_buffer_write(zchan->digit_buffer, dtmf, wr) ? ZAP_SUCCESS : ZAP_FAIL;
	zap_mutex_unlock(zchan->mutex);
	
	return status;
}

zap_status_t zap_channel_read(zap_channel_t *zchan, void *data, zap_size_t *datalen)
{
	zap_status_t status = ZAP_FAIL;
	zio_codec_t codec_func = NULL;
	zap_size_t max = *datalen;

	assert(zchan != NULL);
	assert(zchan->zio != NULL);
	assert(zchan->zio != NULL);
	
    if (!zap_test_flag(zchan, ZAP_CHANNEL_OPEN)) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "channel not open");
        return ZAP_FAIL;
    }

	if (!zchan->zio->read) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "method not implemented");
		return ZAP_FAIL;
	}

    status = zchan->zio->read(zchan, data, datalen);

	if (status == ZAP_SUCCESS && zap_test_flag(zchan, ZAP_CHANNEL_TRANSCODE) && zchan->effective_codec != zchan->native_codec) {
		if (zchan->native_codec == ZAP_CODEC_ULAW && zchan->effective_codec == ZAP_CODEC_SLIN) {
			codec_func = zio_ulaw2slin;
		} else if (zchan->native_codec == ZAP_CODEC_ULAW && zchan->effective_codec == ZAP_CODEC_ALAW) {
			codec_func = zio_ulaw2alaw;
		} else if (zchan->native_codec == ZAP_CODEC_ALAW && zchan->effective_codec == ZAP_CODEC_SLIN) {
			codec_func = zio_alaw2slin;
		} else if (zchan->native_codec == ZAP_CODEC_ALAW && zchan->effective_codec == ZAP_CODEC_ULAW) {
			codec_func = zio_alaw2ulaw;
		}

		if (codec_func) {
			status = codec_func(data, max, datalen);
		} else {
			snprintf(zchan->last_error, sizeof(zchan->last_error), "codec error!");
			status = ZAP_FAIL;
		}
	}

	if (zap_test_flag(zchan, ZAP_CHANNEL_DTMF_DETECT) || zap_test_flag(zchan, ZAP_CHANNEL_PROGRESS_DETECT)) {
		uint8_t sln_buf[1024] = {0};
		int16_t *sln;
		zap_size_t slen = 0;
		char digit_str[80] = "";

		if (zchan->effective_codec == ZAP_CODEC_SLIN) {
			sln = data;
			slen = *datalen / 2;
		} else {
			zap_size_t len = *datalen;
			uint32_t i;
			uint8_t *lp = data;

			slen = sizeof(sln_buf) / 2;
			if (len > slen) {
				len = slen;
			}

			sln = (int16_t *) sln_buf;
			for(i = 0; i < len; i++) {
				if (zchan->effective_codec == ZAP_CODEC_ULAW) {
					*sln++ = ulaw_to_linear(*lp++);
				} else if (zchan->effective_codec == ZAP_CODEC_ALAW) {
					*sln++ = alaw_to_linear(*lp++);
				} else {
					snprintf(zchan->last_error, sizeof(zchan->last_error), "codec error!");
					return ZAP_FAIL;
				}
			}
			sln = (int16_t *) sln_buf;
			slen = len;
		}

		if (zap_test_flag(zchan, ZAP_CHANNEL_PROGRESS_DETECT)) {
			uint32_t i;
			
			for (i = 0; i < ZAP_TONEMAP_INVALID+1; i++) {
				if (zchan->span->tone_finder[i].tone_count) {
					if (teletone_multi_tone_detect(&zchan->span->tone_finder[i], sln, (int)slen) && i != zchan->last_detected_tone) {
						zchan->detected_tone = i;
						zchan->last_detected_tone = i;
					}
				}
			}
		}

		if (zap_test_flag(zchan, ZAP_CHANNEL_DTMF_DETECT)) {

			teletone_dtmf_detect(&zchan->dtmf_detect, sln, (int)slen);
			teletone_dtmf_get(&zchan->dtmf_detect, digit_str, sizeof(digit_str));

			if(*digit_str) {
				zio_event_cb_t event_callback = NULL;

				zap_channel_queue_dtmf(zchan, digit_str);

				if (zchan->span->event_callback) {
					event_callback = zchan->span->event_callback;
				} else if (zchan->event_callback) {
					event_callback = zchan->event_callback;
				}

				if (event_callback) {
					zchan->event_header.channel = zchan;
					zchan->event_header.e_type = ZAP_EVENT_DTMF;
					zchan->event_header.data = digit_str;
					event_callback(zchan, &zchan->event_header);
					zchan->event_header.e_type = ZAP_EVENT_NONE;
					zchan->event_header.data = NULL;
				}
				if (zap_test_flag(zchan, ZAP_CHANNEL_SUPRESS_DTMF)) {
					zchan->skip_read_frames = 20;
				}
				if (zchan->skip_read_frames > 0) {
					memset(data, 0, *datalen);
					zchan->skip_read_frames--;
				}  
			}
		}
	}
	return status;
}


zap_status_t zap_channel_write(zap_channel_t *zchan, void *data, zap_size_t datasize, zap_size_t *datalen)
{
	zap_status_t status = ZAP_FAIL;
	zio_codec_t codec_func = NULL;
	zap_size_t dtmf_blen, max = datasize;
	
	assert(zchan != NULL);
	assert(zchan->zio != NULL);

    if (!zap_test_flag(zchan, ZAP_CHANNEL_OPEN)) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "channel not open");
        return ZAP_FAIL;
    }

	if (!zchan->zio->write) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "method not implemented");
		return ZAP_FAIL;
	}
	
	if (zap_test_flag(zchan, ZAP_CHANNEL_TRANSCODE) && zchan->effective_codec != zchan->native_codec) {
		if (zchan->native_codec == ZAP_CODEC_ULAW && zchan->effective_codec == ZAP_CODEC_SLIN) {
			codec_func = zio_slin2ulaw;
		} else if (zchan->native_codec == ZAP_CODEC_ULAW && zchan->effective_codec == ZAP_CODEC_ALAW) {
			codec_func = zio_alaw2ulaw;
		} else if (zchan->native_codec == ZAP_CODEC_ALAW && zchan->effective_codec == ZAP_CODEC_SLIN) {
			codec_func = zio_slin2alaw;
		} else if (zchan->native_codec == ZAP_CODEC_ALAW && zchan->effective_codec == ZAP_CODEC_ULAW) {
			codec_func = zio_ulaw2alaw;
		}

		if (codec_func) {
			status = codec_func(data, max, datalen);
		} else {
			snprintf(zchan->last_error, sizeof(zchan->last_error), "codec error!");
			status = ZAP_FAIL;
		}
	}

	if (zchan->dtmf_buffer && (dtmf_blen = zap_buffer_inuse(zchan->dtmf_buffer)) && (!zchan->dtmf_delay || --zchan->dtmf_delay == 0)) {
		zap_size_t dlen = *datalen;
		uint8_t auxbuf[1024];
		zap_size_t len, br;
		
		if (zchan->native_codec != ZAP_CODEC_SLIN) {
			dlen *= 2;
		}

		len = dtmf_blen > dlen ? dlen : dtmf_blen;

		br = zap_buffer_read(zchan->dtmf_buffer, auxbuf, len);		

		if (br < dlen) {
			memset(auxbuf + br, 0, dlen - br);
		}

		memcpy(data, auxbuf, dlen);

		if (zchan->native_codec != ZAP_CODEC_SLIN) {
			if (zchan->native_codec == ZAP_CODEC_ULAW) {
				*datalen = dlen;
				zio_slin2ulaw(data, max, datalen);
			} else if (zchan->native_codec == ZAP_CODEC_ALAW) {
				*datalen = dlen;
				zio_slin2alaw(data, max, datalen);
			}
		}
		
	} 

    status = zchan->zio->write(zchan, data, datalen);

	return status;
}

static struct {
	zap_io_interface_t *wanpipe_interface;
	zap_io_interface_t *zt_interface;
} interfaces;


static zap_status_t load_config(void)
{
	char cfg_name[] = "openzap.conf";
	zap_config_t cfg;
	char *var, *val;
	int catno = -1;
	zap_span_t *span = NULL;
	unsigned configured = 0, d = 0;
	char name[80] = "";
	char number[25] = "";
	zap_io_interface_t *zio = NULL;

	if (!zap_config_open_file(&cfg, cfg_name)) {
		return ZAP_FAIL;
	}
	
	while (zap_config_next_pair(&cfg, &var, &val)) {
		if (!strncasecmp(cfg.category, "span", 4)) {
			if (cfg.catno != catno) {
				char *type = cfg.category + 4;
				if (*type == ' ') {
					type++;
				}

				zap_log(ZAP_LOG_DEBUG, "found config for span\n");
				catno = cfg.catno;
				
				if (zap_strlen_zero(type)) {
					zap_log(ZAP_LOG_CRIT, "failure creating span, no type specified.\n");
					span = NULL;
					continue;
				}

				zap_mutex_lock(globals.mutex);
				zio = (zap_io_interface_t *) hashtable_search(globals.interface_hash, type);
				zap_mutex_unlock(globals.mutex);

				if (!zio) {
					zap_log(ZAP_LOG_CRIT, "failure creating span, no such type '%s'\n", type);
					span = NULL;
					continue;
				}

				if (zap_span_create(zio, &span) == ZAP_SUCCESS) {
					zap_log(ZAP_LOG_DEBUG, "created span %d of type %s\n", span->span_id, type);
				} else {
					zap_log(ZAP_LOG_CRIT, "failure creating span of type %s\n", type);
					span = NULL;
					continue;
				}
			}

			if (!span) {
				continue;
			}

			zap_log(ZAP_LOG_DEBUG, "span %d [%s]=[%s]\n", span->span_id, var, val);
			
			if (!strcasecmp(var, "trunk_type")) {
				span->trunk_type = zap_str2zap_trunk_type(val);
				zap_log(ZAP_LOG_DEBUG, "setting trunk type to '%s'\n", zap_trunk_type2str(span->trunk_type)); 
			} else if (!strcasecmp(var, "name")) {
				if (!strcasecmp(val, "undef")) {
					*name = '\0';
				} else {
					zap_copy_string(name, val, sizeof(name));
				}
			} else if (!strcasecmp(var, "number")) {
				if (!strcasecmp(val, "undef")) {
					*number = '\0';
				} else {
					zap_copy_string(number, val, sizeof(number));
				}
			} else if (!strcasecmp(var, "fxo-channel")) {
				if (span->trunk_type == ZAP_TRUNK_NONE) {
					span->trunk_type = ZAP_TRUNK_FXO;
					zap_log(ZAP_LOG_DEBUG, "setting trunk type to '%s'\n", zap_trunk_type2str(span->trunk_type)); 
				}
				if (span->trunk_type == ZAP_TRUNK_FXO) {
					configured += zio->configure_span(span, val, ZAP_CHAN_TYPE_FXO, name, number);
				} else {
					zap_log(ZAP_LOG_WARNING, "Cannot add FXO channels to an FXS trunk!\n");
				}
			} else if (!strcasecmp(var, "fxs-channel")) {
				if (span->trunk_type == ZAP_TRUNK_NONE) {
					span->trunk_type = ZAP_TRUNK_FXS;
					zap_log(ZAP_LOG_DEBUG, "setting trunk type to '%s'\n", zap_trunk_type2str(span->trunk_type)); 
				}
				if (span->trunk_type == ZAP_TRUNK_FXS) {
					configured += zio->configure_span(span, val, ZAP_CHAN_TYPE_FXS, name, number);
				} else {
					zap_log(ZAP_LOG_WARNING, "Cannot add FXS channels to an FXO trunk!\n");
				}
			} else if (!strcasecmp(var, "b-channel")) {
				configured += zio->configure_span(span, val, ZAP_CHAN_TYPE_B, name, number);
			} else if (!strcasecmp(var, "d-channel")) {
				if (d) {
					zap_log(ZAP_LOG_WARNING, "ignoring extra d-channel\n");
				} else {
					zap_chan_type_t qtype;
					if (!strncasecmp(val, "lapd:", 5)) {
						qtype = ZAP_CHAN_TYPE_DQ931;
						val += 5;
					} else {
						qtype = ZAP_CHAN_TYPE_DQ921;
					}
					configured += zio->configure_span(span, val, qtype, name, number);
					d++;
				}
			}
		} else {
			zap_log(ZAP_LOG_ERROR, "unknown param [%s] '%s' / '%s'\n", cfg.category, var, val);
		}
	}
	zap_config_close_file(&cfg);

	zap_log(ZAP_LOG_INFO, "Configured %u channel(s)\n", configured);
	
	return configured ? ZAP_SUCCESS : ZAP_FAIL;
}

static zap_status_t process_module_config(zap_io_interface_t *zio)
{
	zap_config_t cfg;
	char *var, *val;
	char filename[256] = "";
	assert(zio != NULL);

	snprintf(filename, sizeof(filename), "%s.conf", zio->name);

	if (!zio->configure) {
		zap_log(ZAP_LOG_ERROR, "Module %s does not support configuration.\n", zio->name);	
		return ZAP_FAIL;
	}

	if (!zap_config_open_file(&cfg, filename)) {
		zap_log(ZAP_LOG_ERROR, "Cannot open %s\n", filename);	
		return ZAP_FAIL;
	}

	while (zap_config_next_pair(&cfg, &var, &val)) {
		zio->configure(cfg.category, var, val, cfg.lineno);
	}

	zap_config_close_file(&cfg);	

	return ZAP_SUCCESS;
}

zap_status_t zap_global_init(void)
{
	int modcount;

	time_init();
	zap_isdn_init();
	
	memset(&interfaces, 0, sizeof(interfaces));
	globals.interface_hash = create_hashtable(16, hashfromstring, equalkeys);
	modcount = 0;
	zap_mutex_create(&globals.mutex);
	
#ifdef ZAP_WANPIPE_SUPPORT
	if (wanpipe_init(&interfaces.wanpipe_interface) == ZAP_SUCCESS) {
		zap_mutex_lock(globals.mutex);
		hashtable_insert(globals.interface_hash, (void *)interfaces.wanpipe_interface->name, interfaces.wanpipe_interface);
		process_module_config(interfaces.wanpipe_interface);
		zap_mutex_unlock(globals.mutex);
		modcount++;
	} else {
		zap_log(ZAP_LOG_ERROR, "Error initilizing wanpipe.\n");	
	}
#endif

#ifdef ZAP_ZT_SUPPORT
	if (zt_init(&interfaces.zt_interface) == ZAP_SUCCESS) {
		zap_mutex_lock(globals.mutex);
		hashtable_insert(globals.interface_hash, (void *)interfaces.zt_interface->name, interfaces.zt_interface);
		process_module_config(interfaces.zt_interface);
		zap_mutex_unlock(globals.mutex);
		modcount++;
	} else {
		zap_log(ZAP_LOG_ERROR, "Error initilizing zt.\n");	
	}
#endif

	if (!modcount) {
		zap_log(ZAP_LOG_ERROR, "Error initilizing anything.\n");	
		return ZAP_FAIL;
	}

	if (load_config() == ZAP_SUCCESS) {
		return ZAP_SUCCESS;
	}

	zap_log(ZAP_LOG_ERROR, "No modules configured!\n");
	return ZAP_FAIL;
}

zap_status_t zap_global_destroy(void)
{
	unsigned int i,j;
	time_end();
	
	for(i = 1; i <= globals.span_index; i++) {
		zap_span_t *cur_span = &globals.spans[i];

		if (zap_test_flag(cur_span, ZAP_SPAN_CONFIGURED)) {
			zap_mutex_lock(cur_span->mutex);
			for(j = 1; j <= cur_span->chan_count; j++) {
				zap_channel_t *cur_chan = &cur_span->channels[j];
				if (zap_test_flag(cur_chan, ZAP_CHANNEL_CONFIGURED)) {
					if (cur_span->zio->destroy_channel) {
						zap_log(ZAP_LOG_INFO, "Closing channel %u:%u fd:%d\n", cur_chan->span_id, cur_chan->chan_id, cur_chan->sockfd);
						if (cur_span->zio->destroy_channel(cur_chan) == ZAP_SUCCESS) {
							zap_clear_flag_locked(cur_chan, ZAP_CHANNEL_CONFIGURED);
						} else {
							zap_log(ZAP_LOG_ERROR, "Error Closing channel %u:%u fd:%d\n", cur_chan->span_id, cur_chan->chan_id, cur_chan->sockfd);
						}
					}
				}
			}
			zap_mutex_unlock(cur_span->mutex);
		}
	}


#ifdef ZAP_ZT_SUPPORT
	if (interfaces.zt_interface) {
		zap_span_close_all(interfaces.zt_interface);
		zt_destroy();		
	}
#endif
#ifdef ZAP_WANPIPE_SUPPORT
	if (interfaces.wanpipe_interface) {
		zap_span_close_all(interfaces.wanpipe_interface);
		wanpipe_destroy();
	}
#endif

	zap_mutex_lock(globals.mutex);
	hashtable_destroy(globals.interface_hash, 0, 0);
	zap_mutex_unlock(globals.mutex);
	zap_mutex_destroy(&globals.mutex);
	return ZAP_SUCCESS;
}


uint32_t zap_separate_string(char *buf, char delim, char **array, int arraylen)
{
	int argc;
	char *ptr;
	int quot = 0;
	char qc = '"';
	char *e;
	int x;

	if (!buf || !array || !arraylen) {
		return 0;
	}

	memset(array, 0, arraylen * sizeof(*array));

	ptr = buf;

	for (argc = 0; *ptr && (argc < arraylen - 1); argc++) {
		array[argc] = ptr;
		for (; *ptr; ptr++) {
			if (*ptr == qc) {
				if (quot) {
					quot--;
				} else {
					quot++;
				}
			} else if ((*ptr == delim) && !quot) {
				*ptr++ = '\0';
				break;
			}
		}
	}

	if (*ptr) {
		array[argc++] = ptr;
	}

	/* strip quotes */
	for (x = 0; x < argc; x++) {
		if (*(array[x]) == qc) {
			(array[x])++;
			if ((e = strchr(array[x], qc))) {
				*e = '\0';
			}
		}
	}

	return argc;
}

void print_bits(uint8_t *b, int bl, char *buf, int blen, int e)
{
	int i,j = 0, k, l = 0;

	if (blen < (bl * 10) + 2) {
		return;
	}

	for (k = 0 ; k < bl; k++) {
		buf[j++] = '[';
		if (e) {
			for(i = 7; i >= 0; i--) {
				buf[j++] = ((b[k] & (1 << i)) ? '1' : '0');
			}
		} else {
			for(i = 0; i < 8; i++) {
				buf[j++] = ((b[k] & (1 << i)) ? '1' : '0');
			}
		}
		buf[j++] = ']';
		buf[j++] = ' ';
		if (++l == 6) {
			buf[j++] = '\n';
			l = 0;
		}
	}
	
	buf[j++] = '\0';

}


