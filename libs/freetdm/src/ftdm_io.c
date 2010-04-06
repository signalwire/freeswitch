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
 *
 * Contributors: 
 *
 * Moises Silva <moy@sangoma.com>
 * David Yat Sin <dyatsin@sangoma.com>
 *
 */

#define _GNU_SOURCE
#ifndef WIN32
#endif
#include "freetdm.h"
#include <stdarg.h>
#ifdef WIN32
#include <io.h>
#endif
#ifdef FTDM_PIKA_SUPPORT
#include "ftdm_pika.h"
#endif
#include "ftdm_cpu_monitor.h"

#define SPAN_PENDING_CHANS_QUEUE_SIZE 1000

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

FT_DECLARE(ftdm_time_t) ftdm_current_time_in_ms(void)
{
#ifdef WIN32
	return timeGetTime();
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return ((tv.tv_sec * 1000) + (tv.tv_usec / 1000));
#endif
}

typedef struct {
	uint8_t         running;
	uint8_t         alarm;
	uint32_t        interval;
	uint8_t         alarm_action_flags;
	uint8_t         set_alarm_threshold;
	uint8_t         reset_alarm_threshold;
	ftdm_interrupt_t *interrupt;
} cpu_monitor_t;

static struct {
	ftdm_hash_t *interface_hash;
	ftdm_hash_t *module_hash;
	ftdm_hash_t *span_hash;
	ftdm_hash_t *group_hash;
	ftdm_mutex_t *mutex;
	ftdm_mutex_t *span_mutex;
	ftdm_mutex_t *group_mutex;
	uint32_t span_index;
	uint32_t group_index;
	uint32_t running;
	ftdm_span_t *spans;
	ftdm_group_t *groups;
	cpu_monitor_t cpu_monitor;
} globals;

static uint8_t ftdm_cpu_monitor_disabled = 0;

enum ftdm_enum_cpu_alarm_action_flags
{
	FTDM_CPU_ALARM_ACTION_WARN   = (1 << 0),
	FTDM_CPU_ALARM_ACTION_REJECT = (1 << 1)
};

/* enum lookup funcs */
FTDM_ENUM_NAMES(TONEMAP_NAMES, TONEMAP_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_tonemap, ftdm_tonemap2str, ftdm_tonemap_t, TONEMAP_NAMES, FTDM_TONEMAP_INVALID)

FTDM_ENUM_NAMES(OOB_NAMES, OOB_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_oob_event, ftdm_oob_event2str, ftdm_oob_event_t, OOB_NAMES, FTDM_OOB_INVALID)

FTDM_ENUM_NAMES(TRUNK_TYPE_NAMES, TRUNK_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_trunk_type, ftdm_trunk_type2str, ftdm_trunk_type_t, TRUNK_TYPE_NAMES, FTDM_TRUNK_NONE)

FTDM_ENUM_NAMES(START_TYPE_NAMES, START_TYPE_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_analog_start_type, ftdm_analog_start_type2str, ftdm_analog_start_type_t, START_TYPE_NAMES, FTDM_ANALOG_START_NA)

FTDM_ENUM_NAMES(SIGNAL_NAMES, SIGNAL_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_signal_event, ftdm_signal_event2str, ftdm_signal_event_t, SIGNAL_NAMES, FTDM_SIGEVENT_INVALID)

FTDM_ENUM_NAMES(CHANNEL_STATE_NAMES, CHANNEL_STATE_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_channel_state, ftdm_channel_state2str, ftdm_channel_state_t, CHANNEL_STATE_NAMES, FTDM_CHANNEL_STATE_INVALID)

FTDM_ENUM_NAMES(MDMF_TYPE_NAMES, MDMF_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_mdmf_type, ftdm_mdmf_type2str, ftdm_mdmf_type_t, MDMF_TYPE_NAMES, MDMF_INVALID)

FTDM_ENUM_NAMES(CHAN_TYPE_NAMES, CHAN_TYPE_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_chan_type, ftdm_chan_type2str, ftdm_chan_type_t, CHAN_TYPE_NAMES, FTDM_CHAN_TYPE_COUNT)

FTDM_ENUM_NAMES(SIGNALING_STATUS_NAMES, SIGSTATUS_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_signaling_status, ftdm_signaling_status2str, ftdm_signaling_status_t, SIGNALING_STATUS_NAMES, FTDM_SIG_STATE_INVALID)

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

static int ftdm_log_level = 7;

static void default_logger(const char *file, const char *func, int line, int level, const char *fmt, ...)
{
	const char *fp;
	char data[1024];
	va_list ap;

	if (level < 0 || level > 7) {
		level = 7;
	}
	if (level > ftdm_log_level) {
		return;
	}
	
	fp = cut_path(file);

	va_start(ap, fmt);

	vsnprintf(data, sizeof(data), fmt, ap);


	fprintf(stderr, "[%s] %s:%d %s() %s", LEVEL_NAMES[level], file, line, func, data);

	va_end(ap);

}

static __inline__ void *ftdm_std_malloc(void *pool, ftdm_size_t size)
{
	void *ptr = malloc(size);
	pool = NULL; /* fix warning */
	ftdm_assert_return(ptr != NULL, NULL, "Out of memory");
	return ptr;
}

static __inline__ void *ftdm_std_calloc(void *pool, ftdm_size_t elements, ftdm_size_t size)
{
	void *ptr = calloc(elements, size);
	pool = NULL;
	ftdm_assert_return(ptr != NULL, NULL, "Out of memory");
	return ptr;
}

static __inline__ void ftdm_std_free(void *pool, void *ptr)
{
	pool = NULL;
	ftdm_assert_return(ptr != NULL, , "Attempted to free null pointer");
	free(ptr);
}

FT_DECLARE_DATA ftdm_memory_handler_t g_ftdm_mem_handler = 
{
	/*.pool =*/ NULL,
	/*.malloc =*/ ftdm_std_malloc,
	/*.calloc =*/ ftdm_std_calloc,
	/*.free =*/ ftdm_std_free
};

FT_DECLARE_DATA ftdm_crash_policy_t g_ftdm_crash_policy = FTDM_CRASH_NEVER;

static ftdm_status_t ftdm_set_caller_data(ftdm_span_t *span, ftdm_caller_data_t *caller_data)
{
	if (!caller_data) {
		ftdm_log(FTDM_LOG_CRIT, "Error: trying to set caller data, but no caller_data!\n");
		return FTDM_FAIL;
	}

	if (caller_data->cid_num.plan == FTDM_NPI_INVALID) {
		caller_data->cid_num.plan = span->default_caller_data.cid_num.plan;
	}

	if (caller_data->cid_num.type == FTDM_TON_INVALID) {
		caller_data->cid_num.type = span->default_caller_data.cid_num.type;
	}

	if (caller_data->ani.plan == FTDM_NPI_INVALID) {
		caller_data->ani.plan = span->default_caller_data.ani.plan;
	}

	if (caller_data->ani.type == FTDM_TON_INVALID) {
		caller_data->ani.type = span->default_caller_data.ani.type;
	}

	if (caller_data->rdnis.plan == FTDM_NPI_INVALID) {
		caller_data->rdnis.plan = span->default_caller_data.rdnis.plan;
	}

	if (caller_data->rdnis.type == FTDM_NPI_INVALID) {
		caller_data->rdnis.type = span->default_caller_data.rdnis.type;
	}
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_set_caller_data(ftdm_channel_t *ftdmchan, ftdm_caller_data_t *caller_data)
{
	ftdm_status_t err = FTDM_SUCCESS;
	if (!ftdmchan) {
		ftdm_log(FTDM_LOG_CRIT, "Error: trying to set caller data, but no ftdmchan!\n");
		return FTDM_FAIL;
	}
	if ((err = ftdm_set_caller_data(ftdmchan->span, caller_data)) != FTDM_SUCCESS) {
		return err; 
	}
	ftdmchan->caller_data = *caller_data;
	return FTDM_SUCCESS;
}

FT_DECLARE_DATA ftdm_logger_t ftdm_log = null_logger;

FT_DECLARE(void) ftdm_global_set_crash_policy(ftdm_crash_policy_t policy)
{
	g_ftdm_crash_policy |= policy;
}

FT_DECLARE(ftdm_status_t) ftdm_global_set_memory_handler(ftdm_memory_handler_t *handler)
{
	if (!handler) {
		return FTDM_FAIL;
	}
	if (!handler->malloc) {
		return FTDM_FAIL;
	}
	if (!handler->calloc) {
		return FTDM_FAIL;
	}
	if (!handler->free) {
		return FTDM_FAIL;
	}
	memcpy(&g_ftdm_mem_handler, handler, sizeof(*handler));
	return FTDM_SUCCESS;
}

FT_DECLARE(void) ftdm_global_set_logger(ftdm_logger_t logger)
{
	if (logger) {
		ftdm_log = logger;
	} else {
		ftdm_log = null_logger;
	}
}

FT_DECLARE(void) ftdm_global_set_default_logger(int level)
{
	if (level < 0 || level > 7) {
		level = 7;
	}

	ftdm_log = default_logger;
	ftdm_log_level = level;
}

FT_DECLARE_NONSTD(int) ftdm_hash_equalkeys(void *k1, void *k2)
{
    return strcmp((char *) k1, (char *) k2) ? 0 : 1;
}

FT_DECLARE_NONSTD(uint32_t) ftdm_hash_hashfromstring(void *ky)
{
	unsigned char *str = (unsigned char *) ky;
	uint32_t hash = 0;
    int c;
	
	while ((c = *str++)) {
        hash = c + (hash << 6) + (hash << 16) - hash;
	}

    return hash;
}

static ftdm_status_t ftdm_channel_destroy(ftdm_channel_t *ftdmchan)
{

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_CONFIGURED)) {

		while (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_INTHREAD)) {
			ftdm_log(FTDM_LOG_INFO, "Waiting for thread to exit on channel %u:%u\n", ftdmchan->span_id, ftdmchan->chan_id);
			ftdm_sleep(500);
		}

		ftdm_mutex_lock(ftdmchan->pre_buffer_mutex);
		ftdm_buffer_destroy(&ftdmchan->pre_buffer);
		ftdm_mutex_unlock(ftdmchan->pre_buffer_mutex);

		ftdm_buffer_destroy(&ftdmchan->digit_buffer);
		ftdm_buffer_destroy(&ftdmchan->gen_dtmf_buffer);
		ftdm_buffer_destroy(&ftdmchan->dtmf_buffer);
		ftdm_buffer_destroy(&ftdmchan->fsk_buffer);
		ftdmchan->pre_buffer_size = 0;

		hashtable_destroy(ftdmchan->variable_hash);

		ftdm_safe_free(ftdmchan->dtmf_hangup_buf);

		if (ftdmchan->tone_session.buffer) {
			teletone_destroy_session(&ftdmchan->tone_session);
			memset(&ftdmchan->tone_session, 0, sizeof(ftdmchan->tone_session));
		}

		
		if (ftdmchan->span->fio->channel_destroy) {
			ftdm_log(FTDM_LOG_INFO, "Closing channel %s:%u:%u fd:%d\n", ftdmchan->span->type, ftdmchan->span_id, ftdmchan->chan_id, ftdmchan->sockfd);
			if (ftdmchan->span->fio->channel_destroy(ftdmchan) == FTDM_SUCCESS) {
				ftdm_clear_flag_locked(ftdmchan, FTDM_CHANNEL_CONFIGURED);
			} else {
				ftdm_log(FTDM_LOG_ERROR, "Error Closing channel %u:%u fd:%d\n", ftdmchan->span_id, ftdmchan->chan_id, ftdmchan->sockfd);
			}
		}

		ftdm_mutex_destroy(&ftdmchan->mutex);
		ftdm_mutex_destroy(&ftdmchan->pre_buffer_mutex);
	}
	
	return FTDM_SUCCESS;
}

static ftdm_status_t ftdm_span_destroy(ftdm_span_t *span)
{
	ftdm_status_t status = FTDM_SUCCESS;
	unsigned j;

	ftdm_mutex_lock(span->mutex);

	/* stop the signaling */
	if (span->stop) {
		status = span->stop(span);
	} 

	/* destroy the channels */
	ftdm_clear_flag(span, FTDM_SPAN_CONFIGURED);
	for(j = 1; j <= span->chan_count && span->channels[j]; j++) {
		ftdm_channel_t *cur_chan = span->channels[j];
		if (cur_chan) {
			if (ftdm_test_flag(cur_chan, FTDM_CHANNEL_CONFIGURED)) {
				ftdm_channel_destroy(cur_chan);
			}
			ftdm_safe_free(cur_chan);
			cur_chan = NULL;
		}
	}

	/* destroy the I/O for the span */
	if (span->fio && span->fio->span_destroy) {
		ftdm_log(FTDM_LOG_INFO, "Destroying span %u type (%s)\n", span->span_id, span->type);
		if (span->fio->span_destroy(span) != FTDM_SUCCESS) {
			status = FTDM_FAIL;
		}
		ftdm_safe_free(span->type);
		ftdm_safe_free(span->dtmf_hangup);
	}

	/* destroy final basic resources of the span data structure */
	if (span->pendingchans) {
		ftdm_queue_destroy(&span->pendingchans);
	}
	ftdm_mutex_unlock(span->mutex);
	ftdm_mutex_destroy(&span->mutex);
	ftdm_safe_free(span->signal_data);

	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_get_alarms(ftdm_channel_t *ftdmchan)
{
	ftdm_status_t status = FTDM_FAIL;

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_CONFIGURED)) {
		if (ftdmchan->span->fio->get_alarms) {
			if ((status = ftdmchan->span->fio->get_alarms(ftdmchan)) == FTDM_SUCCESS) {
				*ftdmchan->last_error = '\0';
				if (ftdm_test_alarm_flag(ftdmchan, FTDM_ALARM_RED)) {
					snprintf(ftdmchan->last_error + strlen(ftdmchan->last_error), sizeof(ftdmchan->last_error) - strlen(ftdmchan->last_error), "RED/");
				}
				if (ftdm_test_alarm_flag(ftdmchan, FTDM_ALARM_YELLOW)) {
					snprintf(ftdmchan->last_error + strlen(ftdmchan->last_error), sizeof(ftdmchan->last_error) - strlen(ftdmchan->last_error), "YELLOW/");
				}
				if (ftdm_test_alarm_flag(ftdmchan, FTDM_ALARM_BLUE)) {
					snprintf(ftdmchan->last_error + strlen(ftdmchan->last_error), sizeof(ftdmchan->last_error) - strlen(ftdmchan->last_error), "BLUE/");
				}
				if (ftdm_test_alarm_flag(ftdmchan, FTDM_ALARM_LOOPBACK)) {
					snprintf(ftdmchan->last_error + strlen(ftdmchan->last_error), sizeof(ftdmchan->last_error) - strlen(ftdmchan->last_error), "LOOP/");
				}
				if (ftdm_test_alarm_flag(ftdmchan, FTDM_ALARM_RECOVER)) {
					snprintf(ftdmchan->last_error + strlen(ftdmchan->last_error), sizeof(ftdmchan->last_error) - strlen(ftdmchan->last_error), "RECOVER/");
				}
				*(ftdmchan->last_error + strlen(ftdmchan->last_error) - 1) = '\0';

			}
		} else {
			status = FTDM_NOTIMPL;
		}
	}
	
	return status;
}

static void ftdm_span_add(ftdm_span_t *span)
{
	ftdm_span_t *sp;
	ftdm_mutex_lock(globals.span_mutex);
	for (sp = globals.spans; sp && sp->next; sp = sp->next);
	if (sp) {
		sp->next = span;
	} else {
		globals.spans = span;
	}
	hashtable_insert(globals.span_hash, (void *)span->name, span, HASHTABLE_FLAG_NONE);
	ftdm_mutex_unlock(globals.span_mutex);
}

#if 0
static void ftdm_span_del(ftdm_span_t *span)
{
	ftdm_span_t *last = NULL, *sp;

	ftdm_mutex_lock(globals.span_mutex);
	for (sp = globals.spans; sp; sp = sp->next) {
		
		if (sp == span) {
			if (last) {
				last->next = sp->next;
			} else {
				globals.spans = sp->next;
			}
			hashtable_remove(globals.span_hash, (void *)sp->name);
			break;
		}

		last = sp;
	}
	ftdm_mutex_unlock(globals.span_mutex);
}
#endif

FT_DECLARE(ftdm_status_t) ftdm_span_stop(ftdm_span_t *span)
{
	if (span->stop) {
		span->stop(span);
		return FTDM_SUCCESS;
	}
	
	return FTDM_FAIL;
}

FT_DECLARE(ftdm_status_t) ftdm_span_create(ftdm_io_interface_t *fio, ftdm_span_t **span, const char *name)
{
	ftdm_span_t *new_span = NULL;
	ftdm_status_t status = FTDM_FAIL;

	ftdm_assert(fio != NULL, "No IO provided\n");

	ftdm_mutex_lock(globals.mutex);

	if (globals.span_index < FTDM_MAX_SPANS_INTERFACE) {
		new_span = ftdm_calloc(sizeof(*new_span), 1);
		ftdm_assert(new_span, "allocating span failed\n");

		status = ftdm_mutex_create(&new_span->mutex);
		ftdm_assert(status == FTDM_SUCCESS, "mutex creation failed\n");

		ftdm_set_flag(new_span, FTDM_SPAN_CONFIGURED);
		new_span->span_id = ++globals.span_index;
		new_span->fio = fio;
		ftdm_copy_string(new_span->tone_map[FTDM_TONEMAP_DIAL], "%(1000,0,350,440)", FTDM_TONEMAP_LEN);
		ftdm_copy_string(new_span->tone_map[FTDM_TONEMAP_RING], "%(2000,4000,440,480)", FTDM_TONEMAP_LEN);
		ftdm_copy_string(new_span->tone_map[FTDM_TONEMAP_BUSY], "%(500,500,480,620)", FTDM_TONEMAP_LEN);
		ftdm_copy_string(new_span->tone_map[FTDM_TONEMAP_ATTN], "%(100,100,1400,2060,2450,2600)", FTDM_TONEMAP_LEN);
		new_span->trunk_type = FTDM_TRUNK_NONE;
		new_span->data_type = FTDM_TYPE_SPAN;

		ftdm_mutex_lock(globals.span_mutex);
		if (!ftdm_strlen_zero(name) && hashtable_search(globals.span_hash, (void *)name)) {
			ftdm_log(FTDM_LOG_WARNING, "name %s is already used, substituting 'span%d' as the name\n", name, new_span->span_id);
			name = NULL;
		}
		ftdm_mutex_unlock(globals.span_mutex);
		
		if (!name) {
			char buf[128] = "";
			snprintf(buf, sizeof(buf), "span%d", new_span->span_id);
			name = buf;
		}
		new_span->name = ftdm_strdup(name);
		ftdm_span_add(new_span);
		*span = new_span;
		status = FTDM_SUCCESS;
	}
	ftdm_mutex_unlock(globals.mutex);
	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_span_close_all(void)
{
	ftdm_span_t *span;
	uint32_t i = 0, j;

	ftdm_mutex_lock(globals.span_mutex);
	for (span = globals.spans; span; span = span->next) {
		if (ftdm_test_flag(span, FTDM_SPAN_CONFIGURED)) {
			for(j = 1; j <= span->chan_count && span->channels[j]; j++) {
				ftdm_channel_t *toclose = span->channels[j];
				if (ftdm_test_flag(toclose, FTDM_CHANNEL_INUSE)) {
					ftdm_channel_close(&toclose);
				}
				i++;
			}
		} 
	}
	ftdm_mutex_unlock(globals.span_mutex);

	return i ? FTDM_SUCCESS : FTDM_FAIL;
}

FT_DECLARE(ftdm_status_t) ftdm_span_load_tones(ftdm_span_t *span, const char *mapname)
{
	ftdm_config_t cfg;
	char *var, *val;
	int x = 0;

	if (!ftdm_config_open_file(&cfg, "tones.conf")) {
		snprintf(span->last_error, sizeof(span->last_error), "error loading tones.");
		return FTDM_FAIL;
	}
	
	while (ftdm_config_next_pair(&cfg, &var, &val)) {
		int detect = 0;

		if (!strcasecmp(cfg.category, mapname) && var && val) {
			uint32_t index;
			char *name = NULL;

			if (!strncasecmp(var, "detect-", 7)) {
				name = var + 7;
				detect = 1;
			} else if (!strncasecmp(var, "generate-", 9)) {
				name = var + 9;
			} else {
				ftdm_log(FTDM_LOG_WARNING, "Unknown tone name %s\n", var);
				continue;
			}

			index = ftdm_str2ftdm_tonemap(name);

			if (index >= FTDM_TONEMAP_INVALID || index == FTDM_TONEMAP_NONE) {
				ftdm_log(FTDM_LOG_WARNING, "Unknown tone name %s\n", name);
			} else {
				if (detect) {
					char *p = val, *next;
					int i = 0;
					do {
						teletone_process_t this;
						next = strchr(p, ',');
						this = (teletone_process_t)atof(p);
						span->tone_detect_map[index].freqs[i++] = this;
						if (next) {
							p = next + 1;
						}
					} while (next);
					ftdm_log(FTDM_LOG_DEBUG, "added tone detect [%s] = [%s]\n", name, val);
				}  else {
					ftdm_log(FTDM_LOG_DEBUG, "added tone generation [%s] = [%s]\n", name, val);
					ftdm_copy_string(span->tone_map[index], val, sizeof(span->tone_map[index]));
				}
				x++;
			}
		}
	}

	ftdm_config_close_file(&cfg);
	
	if (!x) {
		snprintf(span->last_error, sizeof(span->last_error), "error loading tones.");
		return FTDM_FAIL;
	}

	return FTDM_SUCCESS;
	
}

#define FTDM_SLINEAR_MAX_VALUE 32767
#define FTDM_SLINEAR_MIN_VALUE -32767
static void reset_gain_table(uint8_t *gain_table, float new_gain, ftdm_codec_t codec_gain)
{
	/* sample value */
	uint8_t sv = 0;
	/* linear gain factor */
	float lingain = 0;
	/* linear value for each table sample */
	float linvalue = 0;
	/* amplified (or attenuated in case of negative amplification) sample value */
	int ampvalue = 0;

	/* gain tables are only for alaw and ulaw */
	if (codec_gain != FTDM_CODEC_ALAW && codec_gain != FTDM_CODEC_ULAW) {
		ftdm_log(FTDM_LOG_WARNING, "Not resetting gain table because codec is not ALAW or ULAW but %d\n", codec_gain);
		return;
	}

	if (!new_gain) {
		/* for a 0.0db gain table, each alaw/ulaw sample value is left untouched (0 ==0, 1 == 1, 2 == 2 etc)*/
		sv = 0;
		while (1) {
			gain_table[sv] = sv;
			if (sv == (FTDM_GAINS_TABLE_SIZE-1)) {
				break;
			}
			sv++;
		}
		return;
	}

	/* use the 20log rule to increase the gain: http://en.wikipedia.org/wiki/Gain, http:/en.wipedia.org/wiki/20_log_rule#Definitions */
	lingain = (float)pow(10.0, new_gain/ 20.0);
	sv = 0;
	while (1) {
		/* get the linear value for this alaw/ulaw sample value */
		linvalue = codec_gain == FTDM_CODEC_ALAW ? (float)alaw_to_linear(sv) : (float)ulaw_to_linear(sv);

		/* multiply the linear value and the previously calculated linear gain */
		ampvalue = (int)(linvalue * lingain);

		/* chop it if goes beyond the limits */
		if (ampvalue > FTDM_SLINEAR_MAX_VALUE) {
			ampvalue = FTDM_SLINEAR_MAX_VALUE;
		}

		if (ampvalue < FTDM_SLINEAR_MIN_VALUE) {
			ampvalue = FTDM_SLINEAR_MIN_VALUE;
		}
		gain_table[sv] = codec_gain == FTDM_CODEC_ALAW ? linear_to_alaw(ampvalue) : linear_to_ulaw(ampvalue);
		if (sv == (FTDM_GAINS_TABLE_SIZE-1)) {
			break;
		}
		sv++;
	}
}

FT_DECLARE(ftdm_status_t) ftdm_span_add_channel(ftdm_span_t *span, ftdm_socket_t sockfd, ftdm_chan_type_t type, ftdm_channel_t **chan)
{
	unsigned char i = 0;
	if (span->chan_count < FTDM_MAX_CHANNELS_SPAN) {
		ftdm_channel_t *new_chan = span->channels[++span->chan_count];

		if (!new_chan) {
			if (!(new_chan = ftdm_calloc(1, sizeof(*new_chan)))) {
				return FTDM_FAIL;
			}
			span->channels[span->chan_count] = new_chan;
		}

		new_chan->type = type;
		new_chan->sockfd = sockfd;
		new_chan->fio = span->fio;
		new_chan->span_id = span->span_id;
		new_chan->chan_id = span->chan_count;
		new_chan->span = span;
		new_chan->fds[0] = -1;
		new_chan->fds[1] = -1;
		new_chan->data_type = FTDM_TYPE_CHANNEL;
		if (!new_chan->dtmf_on) {
			new_chan->dtmf_on = FTDM_DEFAULT_DTMF_ON;
		}

		if (!new_chan->dtmf_off) {
			new_chan->dtmf_off = FTDM_DEFAULT_DTMF_OFF;
		}

		ftdm_mutex_create(&new_chan->mutex);
		ftdm_mutex_create(&new_chan->pre_buffer_mutex);

		ftdm_buffer_create(&new_chan->digit_buffer, 128, 128, 0);
		ftdm_buffer_create(&new_chan->gen_dtmf_buffer, 128, 128, 0);
		new_chan->variable_hash = create_hashtable(16, ftdm_hash_hashfromstring, ftdm_hash_equalkeys);

		new_chan->dtmf_hangup_buf = ftdm_calloc (span->dtmf_hangup_len + 1, sizeof (char));

		/* set 0.0db gain table */
		i = 0;
		while (1) {
			new_chan->txgain_table[i] = i;
			new_chan->rxgain_table[i] = i;
			if (i == (sizeof(new_chan->txgain_table)-1)) {
				break;
			}
			i++;
		}

		ftdm_set_flag(new_chan, FTDM_CHANNEL_CONFIGURED | FTDM_CHANNEL_READY);
		*chan = new_chan;
		return FTDM_SUCCESS;
	}

	return FTDM_FAIL;
}

FT_DECLARE(ftdm_status_t) ftdm_span_find_by_name(const char *name, ftdm_span_t **span)
{
	ftdm_status_t status = FTDM_FAIL;

	ftdm_mutex_lock(globals.span_mutex);
	if (!ftdm_strlen_zero(name)) {
		if ((*span = hashtable_search(globals.span_hash, (void *)name))) {
			status = FTDM_SUCCESS;
		} else {
			int span_id = atoi(name);

			ftdm_span_find(span_id, span);
			if (*span) {
				status = FTDM_SUCCESS;
			}
		}
	}
	ftdm_mutex_unlock(globals.span_mutex);
	
	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_span_find(uint32_t id, ftdm_span_t **span)
{
	ftdm_span_t *fspan = NULL, *sp;

	if (id > FTDM_MAX_SPANS_INTERFACE) {
		return FTDM_FAIL;
	}

	ftdm_mutex_lock(globals.span_mutex);
	for (sp = globals.spans; sp; sp = sp->next) {
		if (sp->span_id == id) {
			fspan = sp;
			break;
		}
	}
	ftdm_mutex_unlock(globals.span_mutex);

	if (!fspan || !ftdm_test_flag(fspan, FTDM_SPAN_CONFIGURED)) {
		return FTDM_FAIL;
	}

	*span = fspan;

	return FTDM_SUCCESS;
	
}

FT_DECLARE(ftdm_status_t) ftdm_span_set_event_callback(ftdm_span_t *span, fio_event_cb_t event_callback)
{
	ftdm_mutex_lock(span->mutex);
	span->event_callback = event_callback;
	ftdm_mutex_unlock(span->mutex);
	return FTDM_SUCCESS;
}


FT_DECLARE(ftdm_status_t) ftdm_span_poll_event(ftdm_span_t *span, uint32_t ms)
{
	assert(span->fio != NULL);

	if (span->fio->poll_event) {
		return span->fio->poll_event(span, ms);
	} else {
		ftdm_log(FTDM_LOG_ERROR, "poll_event method not implemented in module %s!", span->fio->name);
	}

	return FTDM_NOTIMPL;
}

FT_DECLARE(ftdm_status_t) ftdm_span_next_event(ftdm_span_t *span, ftdm_event_t **event)
{
	ftdm_status_t status = FTDM_FAIL;
	ftdm_sigmsg_t sigmsg;
	ftdm_assert_return(span->fio != NULL, FTDM_FAIL, "No I/O module attached to this span!\n");

	if (!span->fio->next_event) {
		ftdm_log(FTDM_LOG_ERROR, "next_event method not implemented in module %s!", span->fio->name);
		return FTDM_NOTIMPL;
	}

	status = span->fio->next_event(span, event);
	if (status != FTDM_SUCCESS) {
		return status;
	}

	/* before returning the event to the user we do some core operations with certain OOB events */
	memset(&sigmsg, 0, sizeof(sigmsg));
	sigmsg.span_id = span->span_id;
	sigmsg.chan_id = (*event)->channel->chan_id;
	sigmsg.channel = (*event)->channel;
	switch ((*event)->enum_id) {
	case FTDM_OOB_ALARM_CLEAR:
		{
			sigmsg.event_id = FTDM_SIGEVENT_ALARM_CLEAR;
			ftdm_clear_flag_locked((*event)->channel, FTDM_CHANNEL_IN_ALARM);
			ftdm_span_send_signal(span, &sigmsg);
		}
		break;
	case FTDM_OOB_ALARM_TRAP:
		{
			sigmsg.event_id = FTDM_SIGEVENT_ALARM_TRAP;
			ftdm_set_flag_locked((*event)->channel, FTDM_CHANNEL_IN_ALARM);
			ftdm_span_send_signal(span, &sigmsg);
		}
		break;
	default:
		/* NOOP */
		break;
	}

	return status;
}

static ftdm_status_t ftdmchan_fsk_write_sample(int16_t *buf, ftdm_size_t buflen, void *user_data)
{
	ftdm_channel_t *ftdmchan = (ftdm_channel_t *) user_data;
	ftdm_buffer_write(ftdmchan->fsk_buffer, buf, buflen * 2);
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_send_fsk_data(ftdm_channel_t *ftdmchan, ftdm_fsk_data_state_t *fsk_data, float db_level)
{
	struct ftdm_fsk_modulator fsk_trans;

	if (!ftdmchan->fsk_buffer) {
		ftdm_buffer_create(&ftdmchan->fsk_buffer, 128, 128, 0);
	} else {
		ftdm_buffer_zero(ftdmchan->fsk_buffer);
	}

	if (ftdmchan->token_count > 1) {
		ftdm_fsk_modulator_init(&fsk_trans, FSK_BELL202, ftdmchan->rate, fsk_data, db_level, 80, 5, 0, ftdmchan_fsk_write_sample, ftdmchan);
		ftdm_fsk_modulator_send_all((&fsk_trans));
	} else {
		ftdm_fsk_modulator_init(&fsk_trans, FSK_BELL202, ftdmchan->rate, fsk_data, db_level, 180, 5, 300, ftdmchan_fsk_write_sample, ftdmchan);
		ftdm_fsk_modulator_send_all((&fsk_trans));
		ftdmchan->buffer_delay = 3500 / ftdmchan->effective_interval;
	}

	return FTDM_SUCCESS;
}


FT_DECLARE(ftdm_status_t) ftdm_channel_set_event_callback(ftdm_channel_t *ftdmchan, fio_event_cb_t event_callback)
{
	ftdm_mutex_lock(ftdmchan->mutex);
	ftdmchan->event_callback = event_callback;
	ftdm_mutex_unlock(ftdmchan->mutex);
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_clear_token(ftdm_channel_t *ftdmchan, const char *token)
{
	ftdm_status_t status = FTDM_FAIL;
	
	ftdm_mutex_lock(ftdmchan->mutex);
	if (token == NULL) {
		memset(ftdmchan->tokens, 0, sizeof(ftdmchan->tokens));
		ftdmchan->token_count = 0;
	} else if (*token != '\0') {
		char tokens[FTDM_MAX_TOKENS][FTDM_TOKEN_STRLEN];
		int32_t i, count = ftdmchan->token_count;
		memcpy(tokens, ftdmchan->tokens, sizeof(tokens));
		memset(ftdmchan->tokens, 0, sizeof(ftdmchan->tokens));
		ftdmchan->token_count = 0;		

		for (i = 0; i < count; i++) {
			if (strcmp(tokens[i], token)) {
				ftdm_copy_string(ftdmchan->tokens[ftdmchan->token_count], tokens[i], sizeof(ftdmchan->tokens[ftdmchan->token_count]));
				ftdmchan->token_count++;
			}
		}

		status = FTDM_SUCCESS;
	}
	ftdm_mutex_unlock(ftdmchan->mutex);

	return status;
}

FT_DECLARE(void) ftdm_channel_rotate_tokens(ftdm_channel_t *ftdmchan)
{
	if (ftdmchan->token_count) {
		memmove(ftdmchan->tokens[1], ftdmchan->tokens[0], ftdmchan->token_count * FTDM_TOKEN_STRLEN);
		ftdm_copy_string(ftdmchan->tokens[0], ftdmchan->tokens[ftdmchan->token_count], FTDM_TOKEN_STRLEN);
		*ftdmchan->tokens[ftdmchan->token_count] = '\0';
	}
}

FT_DECLARE(void) ftdm_channel_replace_token(ftdm_channel_t *ftdmchan, const char *old_token, const char *new_token)
{
	unsigned int i;

	if (ftdmchan->token_count) {
		for(i = 0; i < ftdmchan->token_count; i++) {
			if (!strcmp(ftdmchan->tokens[i], old_token)) {
				ftdm_copy_string(ftdmchan->tokens[i], new_token, FTDM_TOKEN_STRLEN);
				break;
			}
		}
	}
}

FT_DECLARE(ftdm_status_t) ftdm_channel_add_token(ftdm_channel_t *ftdmchan, char *token, int end)
{
	ftdm_status_t status = FTDM_FAIL;

	ftdm_mutex_lock(ftdmchan->mutex);
	if (ftdmchan->token_count < FTDM_MAX_TOKENS) {
		if (end) {
			ftdm_copy_string(ftdmchan->tokens[ftdmchan->token_count++], token, FTDM_TOKEN_STRLEN);
		} else {
			memmove(ftdmchan->tokens[1], ftdmchan->tokens[0], ftdmchan->token_count * FTDM_TOKEN_STRLEN);
			ftdm_copy_string(ftdmchan->tokens[0], token, FTDM_TOKEN_STRLEN);
			ftdmchan->token_count++;
		}
		status = FTDM_SUCCESS;
	}
	ftdm_mutex_unlock(ftdmchan->mutex);

	return status;
}


FT_DECLARE(ftdm_status_t) ftdm_channel_complete_state(ftdm_channel_t *ftdmchan)
{
	ftdm_channel_state_t state = ftdmchan->state;

	if (state == FTDM_CHANNEL_STATE_PROGRESS) {
		ftdm_set_flag(ftdmchan, FTDM_CHANNEL_PROGRESS);
	} else if (state == FTDM_CHANNEL_STATE_UP) {
		ftdm_set_flag(ftdmchan, FTDM_CHANNEL_PROGRESS);
		ftdm_set_flag(ftdmchan, FTDM_CHANNEL_MEDIA);	
		ftdm_set_flag(ftdmchan, FTDM_CHANNEL_ANSWERED);	
	} else if (state == FTDM_CHANNEL_STATE_PROGRESS_MEDIA) {
		ftdm_set_flag(ftdmchan, FTDM_CHANNEL_PROGRESS);	
		ftdm_set_flag(ftdmchan, FTDM_CHANNEL_MEDIA);	
	}

	return FTDM_SUCCESS;
}

static int ftdm_parse_state_map(ftdm_channel_t *ftdmchan, ftdm_channel_state_t state, ftdm_state_map_t *state_map)
{
	int x = 0, ok = 0;
	ftdm_state_direction_t direction = ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND) ? ZSD_OUTBOUND : ZSD_INBOUND;

	for(x = 0; x < FTDM_MAP_NODE_SIZE; x++) {
		int i = 0, proceed = 0;
		if (!state_map->nodes[x].type) {
			break;
		}

		if (state_map->nodes[x].direction != direction) {
			continue;
		}
		
		if (state_map->nodes[x].check_states[0] == FTDM_ANY_STATE) {
			proceed = 1;
		} else {
			for(i = 0; i < FTDM_MAP_MAX; i++) {
				if (state_map->nodes[x].check_states[i] == ftdmchan->state) {
					proceed = 1;
					break;
				}
			}
		}

		if (!proceed) {
			continue;
		}
		
		for(i = 0; i < FTDM_MAP_MAX; i++) {
			ok = (state_map->nodes[x].type == ZSM_ACCEPTABLE);
			if (state_map->nodes[x].states[i] == FTDM_END) {
				break;
			}
			if (state_map->nodes[x].states[i] == state) {
				ok = !ok;
				goto end;
			}
		}
	}
 end:
	
	return ok;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_set_state(ftdm_channel_t *ftdmchan, ftdm_channel_state_t state, int lock)
{
	int ok = 1;
	
	if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_READY)) {
		return FTDM_FAIL;
	}

	if (ftdm_test_flag(ftdmchan->span, FTDM_SPAN_SUSPENDED)) {
		if (state != FTDM_CHANNEL_STATE_RESTART && state != FTDM_CHANNEL_STATE_DOWN) {
			return FTDM_FAIL;
		}
	}

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE)) {
		ftdm_log(FTDM_LOG_CRIT, "Ignored state change request from %s to %s, the previous state change has not been processed yet\n",
				ftdm_channel_state2str(ftdmchan->state), ftdm_channel_state2str(state));
		return FTDM_FAIL;
	}

	if (lock) {
		ftdm_mutex_lock(ftdmchan->mutex);
	}

	if (ftdmchan->span->state_map) {
		ok = ftdm_parse_state_map(ftdmchan, state, ftdmchan->span->state_map);
		goto end;
	}

	switch(ftdmchan->state) {
	case FTDM_CHANNEL_STATE_HANGUP:
	case FTDM_CHANNEL_STATE_TERMINATING:
		{
			ok = 0;
			switch(state) {
			case FTDM_CHANNEL_STATE_DOWN:
			case FTDM_CHANNEL_STATE_BUSY:
			case FTDM_CHANNEL_STATE_RESTART:
				ok = 1;
				break;
			default:
				break;
			}
		}
		break;
	case FTDM_CHANNEL_STATE_UP:
		{
			ok = 1;
			switch(state) {
			case FTDM_CHANNEL_STATE_PROGRESS:
			case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
			case FTDM_CHANNEL_STATE_RING:
				ok = 0;
				break;
			default:
				break;
			}
		}
		break;
	case FTDM_CHANNEL_STATE_DOWN:
		{
			ok = 0;
			
			switch(state) {
			case FTDM_CHANNEL_STATE_DIALTONE:
			case FTDM_CHANNEL_STATE_COLLECT:
			case FTDM_CHANNEL_STATE_DIALING:
			case FTDM_CHANNEL_STATE_RING:
			case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
			case FTDM_CHANNEL_STATE_PROGRESS:				
			case FTDM_CHANNEL_STATE_GET_CALLERID:
			case FTDM_CHANNEL_STATE_GENRING:
				ok = 1;
				break;
			default:
				break;
			}
		}
		break;
	case FTDM_CHANNEL_STATE_BUSY:
		{
			switch(state) {
			case FTDM_CHANNEL_STATE_UP:
				ok = 0;
				break;
			default:
				break;
			}
		}
		break;
	case FTDM_CHANNEL_STATE_RING:
		{
			switch(state) {
			case FTDM_CHANNEL_STATE_UP:
				ok = 1;
				break;
			default:
				break;
			}
		}
		break;
	default:
		break;
	}

 end:

	if (state == ftdmchan->state) {
		ok = 0;
	}
	

	if (ok) {
		ftdm_set_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE);	

		ftdm_mutex_lock(ftdmchan->span->mutex);
		ftdm_set_flag(ftdmchan->span, FTDM_SPAN_STATE_CHANGE);
		if (ftdmchan->span->pendingchans) {
			ftdm_queue_enqueue(ftdmchan->span->pendingchans, ftdmchan);
		}
		ftdm_mutex_unlock(ftdmchan->span->mutex);

		ftdmchan->last_state = ftdmchan->state; 
		ftdmchan->state = state;
	}

	if (lock) {
		ftdm_mutex_unlock(ftdmchan->mutex);
	}

	return ok ? FTDM_SUCCESS : FTDM_FAIL;
}


FT_DECLARE(ftdm_status_t) ftdm_group_channel_use_count(ftdm_group_t *group, uint32_t *count)
{
	uint32_t j;

	*count = 0;
	
	if (!group) {
		return FTDM_FAIL;
	}
	
	for(j = 0; j < group->chan_count && group->channels[j]; j++) {
		if (group->channels[j]) {
			if (ftdm_test_flag(group->channels[j], FTDM_CHANNEL_INUSE)) {
				(*count)++;
			}
		}
	}
	
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_open_by_group(uint32_t group_id, ftdm_direction_t direction, ftdm_caller_data_t *caller_data, ftdm_channel_t **ftdmchan)
{
	ftdm_status_t status = FTDM_FAIL;
	ftdm_channel_t *check;
	uint32_t i, count;
	ftdm_group_t *group = NULL;

	if (group_id) {
		ftdm_group_find(group_id, &group);
	}

	if (!group) {
		ftdm_log(FTDM_LOG_ERROR, "Group %d not defined!\n", group_id);
		*ftdmchan = NULL;
		return FTDM_FAIL;
	}

	ftdm_group_channel_use_count(group, &count);

	if (count >= group->chan_count) {
		ftdm_log(FTDM_LOG_ERROR, "All circuits are busy (%d channels used out of %d available).\n", count, group->chan_count);
		*ftdmchan = NULL;
		return FTDM_FAIL;
	}

	
	if (direction == FTDM_TOP_DOWN) {
		i = 0;
	} else {
		i = group->chan_count-1;
	}

	ftdm_mutex_lock(group->mutex);
	for (;;) {
		if (direction == FTDM_TOP_DOWN) {
			if (i >= group->chan_count) {
				break;
			}
		} else {
			if (i < 0) {
				break;
			}
		}
	
		if (!(check = group->channels[i])) {
			status = FTDM_FAIL;
			break;
		}

		if (ftdm_test_flag(check, FTDM_CHANNEL_READY) && 
			!ftdm_test_flag(check, FTDM_CHANNEL_INUSE) && 
			!ftdm_test_flag(check, FTDM_CHANNEL_SUSPENDED) && 
			!ftdm_test_flag(check, FTDM_CHANNEL_IN_ALARM) &&
			check->state == FTDM_CHANNEL_STATE_DOWN && 
			FTDM_IS_VOICE_CHANNEL(check)
			) {
			ftdm_span_t* span = NULL;
			ftdm_span_find(check->span_id, &span);
			if (span && span->channel_request) {
				status = span->channel_request(span, check->chan_id, direction, caller_data, ftdmchan);
				break;
			}

			status = check->fio->open(check);
				
			if (status == FTDM_SUCCESS) {
				ftdm_set_flag(check, FTDM_CHANNEL_INUSE);
				ftdm_channel_open_chan(check);
				*ftdmchan = check;
				break;
			}
		}
		
		if (direction == FTDM_TOP_DOWN) {
			i++;
		} else {
			i--;
		}	
	}
	ftdm_mutex_unlock(group->mutex);
	return status;
}


FT_DECLARE(ftdm_status_t) ftdm_span_channel_use_count(ftdm_span_t *span, uint32_t *count)
{
	uint32_t j;

	*count = 0;
	
	if (!span || !ftdm_test_flag(span, FTDM_SPAN_CONFIGURED)) {
		return FTDM_FAIL;
	}
	
	for(j = 1; j <= span->chan_count && span->channels[j]; j++) {
		if (span->channels[j]) {
			if (ftdm_test_flag(span->channels[j], FTDM_CHANNEL_INUSE)) {
				(*count)++;
			}
		}
	}
	
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_open_by_span(uint32_t span_id, ftdm_direction_t direction, ftdm_caller_data_t *caller_data, ftdm_channel_t **ftdmchan)
{
	ftdm_status_t status = FTDM_FAIL;
	ftdm_channel_t *check;
	uint32_t i, j, count;
	ftdm_span_t *span = NULL;
	uint32_t span_max;

	if (span_id) {
		ftdm_span_find(span_id, &span);

		if (!span || !ftdm_test_flag(span, FTDM_SPAN_CONFIGURED)) {
			ftdm_log(FTDM_LOG_CRIT, "SPAN NOT DEFINED!\n");
			*ftdmchan = NULL;
            		return FTDM_FAIL;
		}

		ftdm_span_channel_use_count(span, &count);

		if (count >= span->chan_count) {
			ftdm_log(FTDM_LOG_CRIT, "All circuits are busy.\n");
			*ftdmchan = NULL;
			return FTDM_FAIL;
		}

		if (span->channel_request && !ftdm_test_flag(span, FTDM_SPAN_SUGGEST_CHAN_ID)) {
			ftdm_set_caller_data(span, caller_data);
			return span->channel_request(span, 0, direction, caller_data, ftdmchan);
		}
		
		span_max = span_id;
		j = span_id;
	} else {
		ftdm_log(FTDM_LOG_CRIT, "No span supplied\n");
		*ftdmchan = NULL;
		return FTDM_FAIL;
	}
	
	ftdm_mutex_lock(span->mutex);
	
	if (direction == FTDM_TOP_DOWN) {
		i = 1;
	} else {
		i = span->chan_count;
	}	
		
	for(;;) {

		if (direction == FTDM_TOP_DOWN) {
			if (i > span->chan_count) {
				break;
			}
		} else {
			if (i == 0) {
				break;
			}
		}
			
		if (!(check = span->channels[i])) {
			status = FTDM_FAIL;
			break;
		}
			
		if (ftdm_test_flag(check, FTDM_CHANNEL_READY) && 
			!ftdm_test_flag(check, FTDM_CHANNEL_INUSE) && 
			!ftdm_test_flag(check, FTDM_CHANNEL_SUSPENDED) && 
			!ftdm_test_flag(check, FTDM_CHANNEL_IN_ALARM) && 
			check->state == FTDM_CHANNEL_STATE_DOWN && 
			FTDM_IS_VOICE_CHANNEL(check)
			) {

			if (span && span->channel_request) {
				ftdm_set_caller_data(span, caller_data);
				status = span->channel_request(span, i, direction, caller_data, ftdmchan);
				break;
			}

			status = check->fio->open(check);
				
			if (status == FTDM_SUCCESS) {
				ftdm_set_flag(check, FTDM_CHANNEL_INUSE);
				ftdm_channel_open_chan(check);
				*ftdmchan = check;
				break;
			}
		}
		
		if (direction == FTDM_TOP_DOWN) {
			i++;
		} else {
			i--;
		}
	}

	ftdm_mutex_unlock(span->mutex);

	return status;
}

static ftdm_status_t ftdm_channel_reset(ftdm_channel_t *ftdmchan)
{
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_OPEN);
	ftdmchan->event_callback = NULL;
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_DTMF_DETECT);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_SUPRESS_DTMF);
	ftdm_channel_done(ftdmchan);
	ftdm_clear_flag_locked(ftdmchan, FTDM_CHANNEL_HOLD);

	memset(ftdmchan->tokens, 0, sizeof(ftdmchan->tokens));
	ftdmchan->token_count = 0;

	ftdm_channel_flush_dtmf(ftdmchan);

	if (ftdmchan->gen_dtmf_buffer) {
		ftdm_buffer_zero(ftdmchan->gen_dtmf_buffer);
	}

	if (ftdmchan->digit_buffer) {
		ftdm_buffer_zero(ftdmchan->digit_buffer);
	}

	if (!ftdmchan->dtmf_on) {
		ftdmchan->dtmf_on = FTDM_DEFAULT_DTMF_ON;
	}

	if (!ftdmchan->dtmf_off) {
		ftdmchan->dtmf_off = FTDM_DEFAULT_DTMF_OFF;
	}
	
	memset(ftdmchan->dtmf_hangup_buf, '\0', ftdmchan->span->dtmf_hangup_len);

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_TRANSCODE)) {
		ftdmchan->effective_codec = ftdmchan->native_codec;
		ftdmchan->packet_len = ftdmchan->native_interval * (ftdmchan->effective_codec == FTDM_CODEC_SLIN ? 16 : 8);
		ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_TRANSCODE);
	}

	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_init(ftdm_channel_t *ftdmchan)
{
	if (ftdmchan->init_state != FTDM_CHANNEL_STATE_DOWN) {
		ftdm_set_state_locked(ftdmchan, ftdmchan->init_state);
		ftdmchan->init_state = FTDM_CHANNEL_STATE_DOWN;
	}

	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_open_chan(ftdm_channel_t *ftdmchan)
{
	ftdm_status_t status = FTDM_FAIL;

	assert(ftdmchan != NULL);

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_SUSPENDED)) {
		snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "%s", "Channel is suspended\n");
		return FTDM_FAIL;
	}

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_IN_ALARM)) {
		snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "%s", "Channel is alarmed\n");
		return FTDM_FAIL;
	}

	if (globals.cpu_monitor.alarm && 
	    globals.cpu_monitor.alarm_action_flags & FTDM_CPU_ALARM_ACTION_REJECT) {
		snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "%s", "CPU usage alarm is on - refusing to open channel\n");
		ftdm_log(FTDM_LOG_WARNING, "CPU usage alarm is on - refusing to open channel\n");
		ftdmchan->caller_data.hangup_cause = FTDM_CAUSE_SWITCH_CONGESTION;
		return FTDM_FAIL;
	}
	
	if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_READY) || (status = ftdm_mutex_trylock(ftdmchan->mutex)) != FTDM_SUCCESS) {
		snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "Channel is not ready or is in use %d %d", ftdm_test_flag(ftdmchan, FTDM_CHANNEL_READY), status);
		return status;
	}

	status = FTDM_FAIL;

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_READY)) {
		status = ftdmchan->span->fio->open(ftdmchan);
		if (status == FTDM_SUCCESS) {
			ftdm_set_flag(ftdmchan, FTDM_CHANNEL_OPEN | FTDM_CHANNEL_INUSE);
		}
	} else {
		snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "%s", "Channel is not ready");
	}

	ftdm_mutex_unlock(ftdmchan->mutex);
	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_open(uint32_t span_id, uint32_t chan_id, ftdm_channel_t **ftdmchan)
{
	ftdm_channel_t *check;
	ftdm_status_t status = FTDM_FAIL;
	ftdm_span_t *span = NULL;

	ftdm_mutex_unlock(globals.mutex);
	ftdm_span_find(span_id, &span);

	if (!span || !ftdm_test_flag(span, FTDM_SPAN_CONFIGURED) || chan_id >= FTDM_MAX_CHANNELS_SPAN) {
		ftdm_log(FTDM_LOG_CRIT, "SPAN NOT DEFINED!\n");
		*ftdmchan = NULL;
		goto done;
	}

	if (span->channel_request) {
		ftdm_log(FTDM_LOG_ERROR, "Individual channel selection not implemented on this span.\n");
		*ftdmchan = NULL;
		goto done;
	}
	
	if (!(check = span->channels[chan_id])) {
		ftdm_log(FTDM_LOG_ERROR, "Invalid Channel %d\n", chan_id);
		*ftdmchan = NULL;
		goto done;
	}

	if (ftdm_test_flag(check, FTDM_CHANNEL_SUSPENDED) || ftdm_test_flag(check, FTDM_CHANNEL_IN_ALARM) ||
		!ftdm_test_flag(check, FTDM_CHANNEL_READY) || (status = ftdm_mutex_trylock(check->mutex)) != FTDM_SUCCESS) {
		*ftdmchan = NULL;
		goto done;
	}
	
	status = FTDM_FAIL;

	if (ftdm_test_flag(check, FTDM_CHANNEL_READY) && (!ftdm_test_flag(check, FTDM_CHANNEL_INUSE) || 
													(check->type == FTDM_CHAN_TYPE_FXS && check->token_count == 1))) {
		if (!ftdm_test_flag(check, FTDM_CHANNEL_OPEN)) {
			status = check->fio->open(check);
			if (status == FTDM_SUCCESS) {
				ftdm_set_flag(check, FTDM_CHANNEL_OPEN);
			}
		} else {
			status = FTDM_SUCCESS;
		}
		ftdm_set_flag(check, FTDM_CHANNEL_INUSE);
		*ftdmchan = check;
	}
	ftdm_mutex_unlock(check->mutex);

	done:
	ftdm_mutex_unlock(globals.mutex);

	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_outgoing_call(ftdm_channel_t *ftdmchan)
{
	ftdm_status_t status;

	assert(ftdmchan != NULL);
	
	if (ftdmchan->span->outgoing_call) {
		if ((status = ftdmchan->span->outgoing_call(ftdmchan)) == FTDM_SUCCESS) {
			ftdm_set_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND);
		}
		return status;
	} else {
		ftdm_log(FTDM_LOG_ERROR, "outgoing_call method not implemented!\n");
	}
	
	return FTDM_FAIL;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_set_sig_status(ftdm_channel_t *ftdmchan, ftdm_signaling_status_t sigstatus)
{
	ftdm_assert_return(ftdmchan != NULL, FTDM_FAIL, "Null channel\n");
	ftdm_assert_return(ftdmchan->span != NULL, FTDM_FAIL, "Null span\n");
	
	if (ftdmchan->span->set_channel_sig_status) {
		return ftdmchan->span->set_channel_sig_status(ftdmchan, sigstatus);
	} else {
		ftdm_log(FTDM_LOG_ERROR, "set_channel_sig_status method not implemented!\n");
		return FTDM_FAIL;
	}
}

FT_DECLARE(ftdm_status_t) ftdm_channel_get_sig_status(ftdm_channel_t *ftdmchan, ftdm_signaling_status_t *sigstatus)
{
	ftdm_assert_return(ftdmchan != NULL, FTDM_FAIL, "Null channel\n");
	ftdm_assert_return(ftdmchan->span != NULL, FTDM_FAIL, "Null span\n");
	ftdm_assert_return(sigstatus != NULL, FTDM_FAIL, "Null sig status parameter\n");
	
	if (ftdmchan->span->get_channel_sig_status) {
		return ftdmchan->span->get_channel_sig_status(ftdmchan, sigstatus);
	} else {
		ftdm_log(FTDM_LOG_ERROR, "get_channel_sig_status method not implemented!\n");
		return FTDM_FAIL;
	}
}

FT_DECLARE(ftdm_status_t) ftdm_span_set_sig_status(ftdm_span_t *span, ftdm_signaling_status_t sigstatus)
{
	ftdm_assert_return(span != NULL, FTDM_FAIL, "Null span\n");
	
	if (span->set_span_sig_status) {
		return span->set_span_sig_status(span, sigstatus);
	} else {
		ftdm_log(FTDM_LOG_ERROR, "set_span_sig_status method not implemented!\n");
		return FTDM_FAIL;
	}
}

FT_DECLARE(ftdm_status_t) ftdm_span_get_sig_status(ftdm_span_t *span, ftdm_signaling_status_t *sigstatus)
{
	ftdm_assert_return(span != NULL, FTDM_FAIL, "Null span\n");
	ftdm_assert_return(sigstatus != NULL, FTDM_FAIL, "Null sig status parameter\n");
	
	if (span->get_span_sig_status) {
		return span->get_span_sig_status(span, sigstatus);
	} else {
		ftdm_log(FTDM_LOG_ERROR, "get_span_sig_status method not implemented!\n");
		return FTDM_FAIL;
	}
}

FT_DECLARE(ftdm_status_t) ftdm_channel_done(ftdm_channel_t *ftdmchan)
{
	assert(ftdmchan != NULL);

	ftdm_mutex_lock(ftdmchan->mutex);

	memset(&ftdmchan->caller_data, 0, sizeof(ftdmchan->caller_data));

	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_INUSE);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_WINK);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_FLASH);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_HOLD);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_OFFHOOK);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_RINGING);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_PROGRESS_DETECT);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_CALLERID_DETECT);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_3WAY);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_PROGRESS);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_MEDIA);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_ANSWERED);
	ftdm_mutex_lock(ftdmchan->pre_buffer_mutex);
	ftdm_buffer_destroy(&ftdmchan->pre_buffer);
	ftdmchan->pre_buffer_size = 0;
	ftdm_mutex_unlock(ftdmchan->pre_buffer_mutex);

	ftdmchan->init_state = FTDM_CHANNEL_STATE_DOWN;
	ftdmchan->state = FTDM_CHANNEL_STATE_DOWN;

	ftdm_log(FTDM_LOG_DEBUG, "channel done %u:%u\n", ftdmchan->span_id, ftdmchan->chan_id);

	ftdm_mutex_unlock(ftdmchan->mutex);

	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_use(ftdm_channel_t *ftdmchan)
{

	assert(ftdmchan != NULL);

	ftdm_set_flag_locked(ftdmchan, FTDM_CHANNEL_INUSE);

	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_close(ftdm_channel_t **ftdmchan)
{
	ftdm_channel_t *check;
	ftdm_status_t status = FTDM_FAIL;

	assert(ftdmchan != NULL);
	check = *ftdmchan;
	*ftdmchan = NULL;

	if (!check) {
		return FTDM_FAIL;
	}

	if (!ftdm_test_flag(check, FTDM_CHANNEL_INUSE)) {
		ftdm_log(FTDM_LOG_WARNING, "Called ftdm_channel_close but never ftdm_channel_open in chan %d:%d??\n", check->span_id, check->chan_id);
		return FTDM_FAIL;
	}

	if (ftdm_test_flag(check, FTDM_CHANNEL_CONFIGURED)) {
		ftdm_mutex_lock(check->mutex);
		if (ftdm_test_flag(check, FTDM_CHANNEL_OPEN)) {
			status = check->fio->close(check);
			if (status == FTDM_SUCCESS) {
				ftdm_clear_flag(check, FTDM_CHANNEL_INUSE);
				ftdm_channel_reset(check);
				*ftdmchan = NULL;
			}
		}
		check->ring_count = 0;
		ftdm_mutex_unlock(check->mutex);
	}
	
	return status;
}


static ftdm_status_t ftdmchan_activate_dtmf_buffer(ftdm_channel_t *ftdmchan)
{

	if (!ftdmchan->dtmf_buffer) {
		if (ftdm_buffer_create(&ftdmchan->dtmf_buffer, 1024, 3192, 0) != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "Failed to allocate DTMF Buffer!\n");
			snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "buffer error");
			return FTDM_FAIL;
		} else {
			ftdm_log(FTDM_LOG_DEBUG, "Created DTMF Buffer!\n");
		}
	}

	
	if (!ftdmchan->tone_session.buffer) {
		memset(&ftdmchan->tone_session, 0, sizeof(ftdmchan->tone_session));
		teletone_init_session(&ftdmchan->tone_session, 0, NULL, NULL);
	}

	ftdmchan->tone_session.rate = ftdmchan->rate;
	ftdmchan->tone_session.duration = ftdmchan->dtmf_on * (ftdmchan->tone_session.rate / 1000);
	ftdmchan->tone_session.wait = ftdmchan->dtmf_off * (ftdmchan->tone_session.rate / 1000);
	ftdmchan->tone_session.volume = -7;

	/*
	  ftdmchan->tone_session.debug = 1;
	  ftdmchan->tone_session.debug_stream = stdout;
	*/

	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_command(ftdm_channel_t *ftdmchan, ftdm_command_t command, void *obj)
{
	ftdm_status_t status = FTDM_FAIL;
	
	assert(ftdmchan != NULL);
	assert(ftdmchan->fio != NULL);

	ftdm_mutex_lock(ftdmchan->mutex);

	switch(command) {

	case FTDM_COMMAND_ENABLE_CALLERID_DETECT:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_CALLERID)) {
				if (ftdm_fsk_demod_init(&ftdmchan->fsk, ftdmchan->rate, ftdmchan->fsk_buf, sizeof(ftdmchan->fsk_buf)) != FTDM_SUCCESS) {
					snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "%s", strerror(errno));
					GOTO_STATUS(done, FTDM_FAIL);
				}
				ftdm_set_flag_locked(ftdmchan, FTDM_CHANNEL_CALLERID_DETECT);
			}
		}
		break;
	case FTDM_COMMAND_DISABLE_CALLERID_DETECT:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_CALLERID)) {
				ftdm_fsk_demod_destroy(&ftdmchan->fsk);
				ftdm_clear_flag_locked(ftdmchan, FTDM_CHANNEL_CALLERID_DETECT);
			}
		}
		break;
	case FTDM_COMMAND_TRACE_INPUT:
		{
			char *path = (char *) obj;
			if (ftdmchan->fds[0] > 0) {
				close(ftdmchan->fds[0]);
				ftdmchan->fds[0] = -1;
			}
			if ((ftdmchan->fds[0] = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) > -1) {
				ftdm_log(FTDM_LOG_DEBUG, "Tracing channel %u:%u to [%s]\n", ftdmchan->span_id, ftdmchan->chan_id, path);	
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
			
			snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "%s", strerror(errno));
			GOTO_STATUS(done, FTDM_FAIL);
		}
		break;
	case FTDM_COMMAND_TRACE_OUTPUT:
		{
			char *path = (char *) obj;
			if (ftdmchan->fds[1] > 0) {
				close(ftdmchan->fds[1]);
				ftdmchan->fds[1] = -1;
			}
			if ((ftdmchan->fds[1] = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) > -1) {
				ftdm_log(FTDM_LOG_DEBUG, "Tracing channel %u:%u to [%s]\n", ftdmchan->span_id, ftdmchan->chan_id, path);	
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
			
			snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "%s", strerror(errno));
			GOTO_STATUS(done, FTDM_FAIL);
		}
		break;
	case FTDM_COMMAND_SET_INTERVAL:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_INTERVAL)) {
				ftdmchan->effective_interval = FTDM_COMMAND_OBJ_INT;
				if (ftdmchan->effective_interval == ftdmchan->native_interval) {
					ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_BUFFER);
				} else {
					ftdm_set_flag(ftdmchan, FTDM_CHANNEL_BUFFER);
				}
				ftdmchan->packet_len = ftdmchan->native_interval * (ftdmchan->effective_codec == FTDM_CODEC_SLIN ? 16 : 8);
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
		}
		break;
	case FTDM_COMMAND_GET_INTERVAL:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_INTERVAL)) {
				FTDM_COMMAND_OBJ_INT = ftdmchan->effective_interval;
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
		}
		break;
	case FTDM_COMMAND_SET_CODEC:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_CODECS)) {
				ftdmchan->effective_codec = FTDM_COMMAND_OBJ_INT;
				
				if (ftdmchan->effective_codec == ftdmchan->native_codec) {
					ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_TRANSCODE);
				} else {
					ftdm_set_flag(ftdmchan, FTDM_CHANNEL_TRANSCODE);
				}
				ftdmchan->packet_len = ftdmchan->native_interval * (ftdmchan->effective_codec == FTDM_CODEC_SLIN ? 16 : 8);
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
		}
		break;

	case FTDM_COMMAND_SET_NATIVE_CODEC:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_CODECS)) {
				ftdmchan->effective_codec = ftdmchan->native_codec;
				ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_TRANSCODE);
				ftdmchan->packet_len = ftdmchan->native_interval * (ftdmchan->effective_codec == FTDM_CODEC_SLIN ? 16 : 8);
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
		}
		break;

	case FTDM_COMMAND_GET_CODEC: 
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_CODECS)) {
				FTDM_COMMAND_OBJ_INT = ftdmchan->effective_codec;
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
		}
		break;
	case FTDM_COMMAND_GET_NATIVE_CODEC: 
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_CODECS)) {
				FTDM_COMMAND_OBJ_INT = ftdmchan->native_codec;
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
		}
		break;
	case FTDM_COMMAND_ENABLE_PROGRESS_DETECT:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_PROGRESS)) {
				/* if they don't have thier own, use ours */
				ftdm_channel_clear_detected_tones(ftdmchan);
				ftdm_channel_clear_needed_tones(ftdmchan);
				teletone_multi_tone_init(&ftdmchan->span->tone_finder[FTDM_TONEMAP_DIAL], &ftdmchan->span->tone_detect_map[FTDM_TONEMAP_DIAL]);
				teletone_multi_tone_init(&ftdmchan->span->tone_finder[FTDM_TONEMAP_RING], &ftdmchan->span->tone_detect_map[FTDM_TONEMAP_RING]);
				teletone_multi_tone_init(&ftdmchan->span->tone_finder[FTDM_TONEMAP_BUSY], &ftdmchan->span->tone_detect_map[FTDM_TONEMAP_BUSY]);
				ftdm_set_flag(ftdmchan, FTDM_CHANNEL_PROGRESS_DETECT);
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
		}
		break;
	case FTDM_COMMAND_DISABLE_PROGRESS_DETECT:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_PROGRESS)) {
				ftdm_clear_flag_locked(ftdmchan, FTDM_CHANNEL_PROGRESS_DETECT);
				ftdm_channel_clear_detected_tones(ftdmchan);
				ftdm_channel_clear_needed_tones(ftdmchan);
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
		}
		break;
	case FTDM_COMMAND_ENABLE_DTMF_DETECT:
		{
			/* if they don't have thier own, use ours */
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_DTMF_DETECT)) {
				ftdm_tone_type_t tt = FTDM_COMMAND_OBJ_INT;
				if (tt == FTDM_TONE_DTMF) {
					teletone_dtmf_detect_init (&ftdmchan->dtmf_detect, ftdmchan->rate);
					ftdm_set_flag_locked(ftdmchan, FTDM_CHANNEL_DTMF_DETECT);
					ftdm_set_flag_locked(ftdmchan, FTDM_CHANNEL_SUPRESS_DTMF);
					GOTO_STATUS(done, FTDM_SUCCESS);
				} else {
					snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "invalid command");
					GOTO_STATUS(done, FTDM_FAIL);
				}
			}
		}
		break;
	case FTDM_COMMAND_DISABLE_DTMF_DETECT:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_DTMF_DETECT)) {
				ftdm_tone_type_t tt = FTDM_COMMAND_OBJ_INT;
                if (tt == FTDM_TONE_DTMF) {
                    teletone_dtmf_detect_init (&ftdmchan->dtmf_detect, ftdmchan->rate);
                    ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_DTMF_DETECT);
					ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_SUPRESS_DTMF);
					GOTO_STATUS(done, FTDM_SUCCESS);
                } else {
                    snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "invalid command");
					GOTO_STATUS(done, FTDM_FAIL);
                }
			}
		}

	case FTDM_COMMAND_SET_PRE_BUFFER_SIZE:
		{
			int val = FTDM_COMMAND_OBJ_INT;

			if (val < 0) {
				val = 0;
			}

			ftdmchan->pre_buffer_size = val * 8;

			ftdm_mutex_lock(ftdmchan->pre_buffer_mutex);
			if (!ftdmchan->pre_buffer_size) {
				ftdm_buffer_destroy(&ftdmchan->pre_buffer);
			} else if (!ftdmchan->pre_buffer) {
				ftdm_buffer_create(&ftdmchan->pre_buffer, 1024, ftdmchan->pre_buffer_size, 0);
			}
			ftdm_mutex_unlock(ftdmchan->pre_buffer_mutex);

			GOTO_STATUS(done, FTDM_SUCCESS);

		}
		break;
	case FTDM_COMMAND_GET_DTMF_ON_PERIOD:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_DTMF_GENERATE)) {
				FTDM_COMMAND_OBJ_INT = ftdmchan->dtmf_on;
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
		}
		break;
	case FTDM_COMMAND_GET_DTMF_OFF_PERIOD:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_DTMF_GENERATE)) {
				FTDM_COMMAND_OBJ_INT = ftdmchan->dtmf_on;
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
		}
		break;
	case FTDM_COMMAND_SET_DTMF_ON_PERIOD:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_DTMF_GENERATE)) {
				int val = FTDM_COMMAND_OBJ_INT;
				if (val > 10 && val < 1000) {
					ftdmchan->dtmf_on = val;
					GOTO_STATUS(done, FTDM_SUCCESS);
				} else {
					snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "invalid value %d range 10-1000", val);
					GOTO_STATUS(done, FTDM_FAIL);
				}
			}
		}
		break;
	case FTDM_COMMAND_SET_DTMF_OFF_PERIOD:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_DTMF_GENERATE)) {
				int val = FTDM_COMMAND_OBJ_INT;
				if (val > 10 && val < 1000) {
					ftdmchan->dtmf_off = val;
					GOTO_STATUS(done, FTDM_SUCCESS);
				} else {
					snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "invalid value %d range 10-1000", val);
					GOTO_STATUS(done, FTDM_FAIL);
				}
			}
		}
		break;
	case FTDM_COMMAND_SEND_DTMF:
		{
			if (!ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_DTMF_GENERATE)) {
				char *digits = FTDM_COMMAND_OBJ_CHAR_P;
				
				if ((status = ftdmchan_activate_dtmf_buffer(ftdmchan)) != FTDM_SUCCESS) {
					GOTO_STATUS(done, status);
				}
				
				ftdm_buffer_write(ftdmchan->gen_dtmf_buffer, digits, strlen(digits));
				
				GOTO_STATUS(done, FTDM_SUCCESS);
			}
		}
		break;

	case FTDM_COMMAND_DISABLE_ECHOCANCEL:
		{
			ftdm_mutex_lock(ftdmchan->pre_buffer_mutex);
			ftdm_buffer_destroy(&ftdmchan->pre_buffer);
			ftdmchan->pre_buffer_size = 0;
			ftdm_mutex_unlock(ftdmchan->pre_buffer_mutex);
		}
		break;

	case FTDM_COMMAND_SET_RX_GAIN:
		{
			ftdmchan->rxgain = FTDM_COMMAND_OBJ_FLOAT;
			reset_gain_table(ftdmchan->rxgain_table, ftdmchan->rxgain, ftdmchan->native_codec);
			if (ftdmchan->rxgain == 0.0) {
				ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_USE_RX_GAIN);
			} else {
				ftdm_set_flag(ftdmchan, FTDM_CHANNEL_USE_RX_GAIN);
			}
			GOTO_STATUS(done, FTDM_SUCCESS);
		}
		break;
	case FTDM_COMMAND_GET_RX_GAIN:
		{
			FTDM_COMMAND_OBJ_FLOAT = ftdmchan->rxgain;
			GOTO_STATUS(done, FTDM_SUCCESS);
		}
		break;
	case FTDM_COMMAND_SET_TX_GAIN:
		{
			ftdmchan->txgain = FTDM_COMMAND_OBJ_FLOAT;
			reset_gain_table(ftdmchan->txgain_table, ftdmchan->txgain, ftdmchan->native_codec);
			if (ftdmchan->txgain == 0.0) {
				ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_USE_TX_GAIN);
			} else {
				ftdm_set_flag(ftdmchan, FTDM_CHANNEL_USE_TX_GAIN);
			}
			GOTO_STATUS(done, FTDM_SUCCESS);
		}
		break;
	case FTDM_COMMAND_GET_TX_GAIN:
		{
			FTDM_COMMAND_OBJ_FLOAT = ftdmchan->txgain;
			GOTO_STATUS(done, FTDM_SUCCESS);
		}
		break;
	default:
		break;
	}

	if (!ftdmchan->fio->command) {
		snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "method not implemented");
		ftdm_log(FTDM_LOG_ERROR, "no command function defined by the I/O freetdm module!\n");	
		GOTO_STATUS(done, FTDM_FAIL);
	}

    	status = ftdmchan->fio->command(ftdmchan, command, obj);

	if (status == FTDM_NOTIMPL) {
		snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "I/O command %d not implemented in backend", command);
		ftdm_log(FTDM_LOG_ERROR, "I/O backend does not support command %d!\n", command);	
	}
done:
	ftdm_mutex_unlock(ftdmchan->mutex);
	return status;

}

FT_DECLARE(ftdm_status_t) ftdm_channel_wait(ftdm_channel_t *ftdmchan, ftdm_wait_flag_t *flags, int32_t to)
{
	assert(ftdmchan != NULL);
	assert(ftdmchan->fio != NULL);

    if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OPEN)) {
		snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "channel not open");
        return FTDM_FAIL;
    }

	if (!ftdmchan->fio->wait) {
		snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "method not implemented");
		return FTDM_FAIL;
	}

    return ftdmchan->fio->wait(ftdmchan, flags, to);

}

/*******************************/
FIO_CODEC_FUNCTION(fio_slin2ulaw)
{
	int16_t sln_buf[512] = {0}, *sln = sln_buf;
	uint8_t *lp = data;
	uint32_t i;
	ftdm_size_t len = *datalen;

	if (max > len) {
		max = len;
	}

	memcpy(sln, data, max);
	
	for(i = 0; i < max; i++) {
		*lp++ = linear_to_ulaw(*sln++);
	}

	*datalen = max / 2;

	return FTDM_SUCCESS;

}


FIO_CODEC_FUNCTION(fio_ulaw2slin)
{
	int16_t *sln = data;
	uint8_t law[1024] = {0}, *lp = law;
	uint32_t i;
	ftdm_size_t len = *datalen;
	
	if (max > len) {
		max = len;
	}

	memcpy(law, data, max);

	for(i = 0; i < max; i++) {
		*sln++ = ulaw_to_linear(*lp++);
	}
	
	*datalen = max * 2;

	return FTDM_SUCCESS;
}

FIO_CODEC_FUNCTION(fio_slin2alaw)
{
	int16_t sln_buf[512] = {0}, *sln = sln_buf;
	uint8_t *lp = data;
	uint32_t i;
	ftdm_size_t len = *datalen;

	if (max > len) {
		max = len;
	}

	memcpy(sln, data, max);
	
	for(i = 0; i < max; i++) {
		*lp++ = linear_to_alaw(*sln++);
	}

	*datalen = max / 2;

	return FTDM_SUCCESS;

}


FIO_CODEC_FUNCTION(fio_alaw2slin)
{
	int16_t *sln = data;
	uint8_t law[1024] = {0}, *lp = law;
	uint32_t i;
	ftdm_size_t len = *datalen;
	
	if (max > len) {
		max = len;
	}

	memcpy(law, data, max);

	for(i = 0; i < max; i++) {
		*sln++ = alaw_to_linear(*lp++);
	}

	*datalen = max * 2;

	return FTDM_SUCCESS;
}

FIO_CODEC_FUNCTION(fio_ulaw2alaw)
{
	ftdm_size_t len = *datalen;
	uint32_t i;
	uint8_t *lp = data;

	if (max > len) {
        max = len;
    }

	for(i = 0; i < max; i++) {
		*lp = ulaw_to_alaw(*lp);
		lp++;
	}

	return FTDM_SUCCESS;
}

FIO_CODEC_FUNCTION(fio_alaw2ulaw)
{
	ftdm_size_t len = *datalen;
	uint32_t i;
	uint8_t *lp = data;

	if (max > len) {
        max = len;
    }

	for(i = 0; i < max; i++) {
		*lp = alaw_to_ulaw(*lp);
		lp++;
	}

	return FTDM_SUCCESS;
}

/******************************/

FT_DECLARE(void) ftdm_channel_clear_detected_tones(ftdm_channel_t *ftdmchan)
{
	uint32_t i;

	memset(ftdmchan->detected_tones, 0, sizeof(ftdmchan->detected_tones[0]) * FTDM_TONEMAP_INVALID);
	
	for (i = 1; i < FTDM_TONEMAP_INVALID; i++) {
		ftdmchan->span->tone_finder[i].tone_count = 0;
	}
}

FT_DECLARE(void) ftdm_channel_clear_needed_tones(ftdm_channel_t *ftdmchan)
{
	memset(ftdmchan->needed_tones, 0, sizeof(ftdmchan->needed_tones[0]) * FTDM_TONEMAP_INVALID);
}

FT_DECLARE(ftdm_size_t) ftdm_channel_dequeue_dtmf(ftdm_channel_t *ftdmchan, char *dtmf, ftdm_size_t len)
{
	ftdm_size_t bytes = 0;

	assert(ftdmchan != NULL);

	if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_READY)) {
		return FTDM_FAIL;
	}

	if (ftdmchan->digit_buffer && ftdm_buffer_inuse(ftdmchan->digit_buffer)) {
		ftdm_mutex_lock(ftdmchan->mutex);
		if ((bytes = ftdm_buffer_read(ftdmchan->digit_buffer, dtmf, len)) > 0) {
			*(dtmf + bytes) = '\0';
		}
		ftdm_mutex_unlock(ftdmchan->mutex);
	}

	return bytes;
}

FT_DECLARE(void) ftdm_channel_flush_dtmf(ftdm_channel_t *ftdmchan)
{
	if (ftdmchan->digit_buffer && ftdm_buffer_inuse(ftdmchan->digit_buffer)) {
		ftdm_mutex_lock(ftdmchan->mutex);
		ftdm_buffer_zero(ftdmchan->digit_buffer);
		ftdm_mutex_unlock(ftdmchan->mutex);
	}
}

FT_DECLARE(ftdm_status_t) ftdm_channel_queue_dtmf(ftdm_channel_t *ftdmchan, const char *dtmf)
{
	ftdm_status_t status;
	register ftdm_size_t len, inuse;
	ftdm_size_t wr = 0;
	const char *p;
	
	assert(ftdmchan != NULL);

	if (ftdmchan->pre_buffer) {
		ftdm_buffer_zero(ftdmchan->pre_buffer);
	}

	ftdm_mutex_lock(ftdmchan->mutex);

	inuse = ftdm_buffer_inuse(ftdmchan->digit_buffer);
	len = strlen(dtmf);
	
	if (len + inuse > ftdm_buffer_len(ftdmchan->digit_buffer)) {
		ftdm_buffer_toss(ftdmchan->digit_buffer, strlen(dtmf));
	}

	if (ftdmchan->span->dtmf_hangup_len) {
		for (p = dtmf; ftdm_is_dtmf(*p); p++) {
			memmove (ftdmchan->dtmf_hangup_buf, ftdmchan->dtmf_hangup_buf + 1, ftdmchan->span->dtmf_hangup_len - 1);
			ftdmchan->dtmf_hangup_buf[ftdmchan->span->dtmf_hangup_len - 1] = *p;
			if (!strcmp(ftdmchan->dtmf_hangup_buf, ftdmchan->span->dtmf_hangup)) {
				ftdm_log(FTDM_LOG_DEBUG, "DTMF hangup detected.\n");
				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
				break;
			}
		}
	}

	p = dtmf;
	while (wr < len && p) {
		if (ftdm_is_dtmf(*p)) {
			wr++;
		} else {
			break;
		}
		p++;
	}

	status = ftdm_buffer_write(ftdmchan->digit_buffer, dtmf, wr) ? FTDM_SUCCESS : FTDM_FAIL;
	ftdm_mutex_unlock(ftdmchan->mutex);
	
	return status;
}


static ftdm_status_t handle_dtmf(ftdm_channel_t *ftdmchan, ftdm_size_t datalen)
{
	ftdm_buffer_t *buffer = NULL;
	ftdm_size_t dblen = 0;
	int wrote = 0;

	if (ftdmchan->gen_dtmf_buffer && (dblen = ftdm_buffer_inuse(ftdmchan->gen_dtmf_buffer))) {
		char digits[128] = "";
		char *cur;
		int x = 0;				 
		
		if (dblen > sizeof(digits) - 1) {
			dblen = sizeof(digits) - 1;
		}

		if (ftdm_buffer_read(ftdmchan->gen_dtmf_buffer, digits, dblen) && !ftdm_strlen_zero_buf(digits)) {
			ftdm_log(FTDM_LOG_DEBUG, "%d:%d GENERATE DTMF [%s]\n", ftdmchan->span_id, ftdmchan->chan_id, digits);	
		
			cur = digits;

			if (*cur == 'F') {
				ftdm_channel_command(ftdmchan, FTDM_COMMAND_FLASH, NULL);
				cur++;
			}

			for (; *cur; cur++) {
				if ((wrote = teletone_mux_tones(&ftdmchan->tone_session, &ftdmchan->tone_session.TONES[(int)*cur]))) {
					ftdm_buffer_write(ftdmchan->dtmf_buffer, ftdmchan->tone_session.buffer, wrote * 2);
					x++;
				} else {
					ftdm_log(FTDM_LOG_ERROR, "%d:%d Problem Adding DTMF SEQ [%s]\n", ftdmchan->span_id, ftdmchan->chan_id, digits);
					return FTDM_FAIL;
				}
			}

			if (x) {
				ftdmchan->skip_read_frames = (wrote / (ftdmchan->effective_interval * 8)) + 4;
			}
		}
	}
	

	if (!ftdmchan->buffer_delay || --ftdmchan->buffer_delay == 0) {
		if (ftdmchan->dtmf_buffer && (dblen = ftdm_buffer_inuse(ftdmchan->dtmf_buffer))) {
			buffer = ftdmchan->dtmf_buffer;
		} else if (ftdmchan->fsk_buffer && (dblen = ftdm_buffer_inuse(ftdmchan->fsk_buffer))) {
			buffer = ftdmchan->fsk_buffer;			
		}
	}

	if (buffer) {
		ftdm_size_t dlen = datalen;
		uint8_t auxbuf[1024];
		ftdm_size_t len, br, max = sizeof(auxbuf);
		
		if (ftdmchan->native_codec != FTDM_CODEC_SLIN) {
			dlen *= 2;
		}
		
		len = dblen > dlen ? dlen : dblen;

		br = ftdm_buffer_read(buffer, auxbuf, len);		
		if (br < dlen) {
			memset(auxbuf + br, 0, dlen - br);
		}

		if (ftdmchan->native_codec != FTDM_CODEC_SLIN) {
			if (ftdmchan->native_codec == FTDM_CODEC_ULAW) {
				fio_slin2ulaw(auxbuf, max, &dlen);
			} else if (ftdmchan->native_codec == FTDM_CODEC_ALAW) {
				fio_slin2alaw(auxbuf, max, &dlen);
			}
		}
		
		return ftdmchan->fio->write(ftdmchan, auxbuf, &dlen);
	} 

	return FTDM_SUCCESS;

}


FT_DECLARE(void) ftdm_generate_sln_silence(int16_t *data, uint32_t samples, uint32_t divisor)
{
    int16_t x;
    uint32_t i;
    int sum_rnd = 0;
    int16_t rnd2 = (int16_t) ftdm_current_time_in_ms() * (int16_t) (intptr_t) data;

    assert(divisor);

    for (i = 0; i < samples; i++, sum_rnd = 0) {
        for (x = 0; x < 6; x++) {
            rnd2 = rnd2 * 31821U + 13849U;
            sum_rnd += rnd2 ;
        }
        //switch_normalize_to_16bit(sum_rnd);
        *data = (int16_t) ((int16_t) sum_rnd / (int) divisor);

        data++;
    }
}



FT_DECLARE(ftdm_status_t) ftdm_channel_read(ftdm_channel_t *ftdmchan, void *data, ftdm_size_t *datalen)
{
	ftdm_status_t status = FTDM_FAIL;
	fio_codec_t codec_func = NULL;
	ftdm_size_t max = *datalen;
	unsigned i = 0;

	ftdm_assert_return(ftdmchan != NULL, FTDM_FAIL, "ftdmchan is null\n");
	ftdm_assert_return(ftdmchan->fio != NULL, FTDM_FAIL, "No I/O module attached to ftdmchan\n");
	
    if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OPEN)) {
		snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "channel not open");
        return FTDM_FAIL;
    }

	if (!ftdmchan->fio->read) {
		snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "method not implemented");
		return FTDM_FAIL;
	}

    status = ftdmchan->fio->read(ftdmchan, data, datalen);
	if (ftdmchan->fds[0] > -1) {
		int dlen = (int) *datalen;
		if (write(ftdmchan->fds[0], data, dlen) != dlen) {
			snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "file write error!");
			return FTDM_FAIL;
		}
	}

	if (status == FTDM_SUCCESS) {
		if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_USE_RX_GAIN) 
			&& (ftdmchan->native_codec == FTDM_CODEC_ALAW || ftdmchan->native_codec == FTDM_CODEC_ULAW)) {
			unsigned char *rdata = data;
			for (i = 0; i < *datalen; i++) {
				rdata[i] = ftdmchan->rxgain_table[rdata[i]];
			}
		}
		handle_dtmf(ftdmchan, *datalen);
	}

	if (status == FTDM_SUCCESS && ftdm_test_flag(ftdmchan, FTDM_CHANNEL_TRANSCODE) && ftdmchan->effective_codec != ftdmchan->native_codec) {
		if (ftdmchan->native_codec == FTDM_CODEC_ULAW && ftdmchan->effective_codec == FTDM_CODEC_SLIN) {
			codec_func = fio_ulaw2slin;
		} else if (ftdmchan->native_codec == FTDM_CODEC_ULAW && ftdmchan->effective_codec == FTDM_CODEC_ALAW) {
			codec_func = fio_ulaw2alaw;
		} else if (ftdmchan->native_codec == FTDM_CODEC_ALAW && ftdmchan->effective_codec == FTDM_CODEC_SLIN) {
			codec_func = fio_alaw2slin;
		} else if (ftdmchan->native_codec == FTDM_CODEC_ALAW && ftdmchan->effective_codec == FTDM_CODEC_ULAW) {
			codec_func = fio_alaw2ulaw;
		}

		if (codec_func) {
			status = codec_func(data, max, datalen);
		} else {
			snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "codec error!");
			status = FTDM_FAIL;
		}
	}

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_DTMF_DETECT) || ftdm_test_flag(ftdmchan, FTDM_CHANNEL_PROGRESS_DETECT) || 
		ftdm_test_flag(ftdmchan, FTDM_CHANNEL_CALLERID_DETECT)) {
		uint8_t sln_buf[1024] = {0};
		int16_t *sln;
		ftdm_size_t slen = 0;
		char digit_str[80] = "";

		if (ftdmchan->effective_codec == FTDM_CODEC_SLIN) {
			sln = data;
			slen = *datalen / 2;
		} else {
			ftdm_size_t len = *datalen;
			uint32_t i;
			uint8_t *lp = data;

			slen = sizeof(sln_buf) / 2;
			if (len > slen) {
				len = slen;
			}

			sln = (int16_t *) sln_buf;
			for(i = 0; i < len; i++) {
				if (ftdmchan->effective_codec == FTDM_CODEC_ULAW) {
					*sln++ = ulaw_to_linear(*lp++);
				} else if (ftdmchan->effective_codec == FTDM_CODEC_ALAW) {
					*sln++ = alaw_to_linear(*lp++);
				} else {
					snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "codec error!");
					return FTDM_FAIL;
				}
			}
			sln = (int16_t *) sln_buf;
			slen = len;
		}

		if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_CALLERID_DETECT)) {
			if (ftdm_fsk_demod_feed(&ftdmchan->fsk, sln, slen) != FTDM_SUCCESS) {
				ftdm_size_t type, mlen;
				char str[128], *sp;
				
				while(ftdm_fsk_data_parse(&ftdmchan->fsk, &type, &sp, &mlen) == FTDM_SUCCESS) {
					*(str+mlen) = '\0';
					ftdm_copy_string(str, sp, ++mlen);
					ftdm_clean_string(str);
					ftdm_log(FTDM_LOG_DEBUG, "FSK: TYPE %s LEN %d VAL [%s]\n", ftdm_mdmf_type2str(type), mlen-1, str);
					
					switch(type) {
					case MDMF_DDN:
					case MDMF_PHONE_NUM:
						{
							if (mlen > sizeof(ftdmchan->caller_data.ani)) {
								mlen = sizeof(ftdmchan->caller_data.ani);
							}
							ftdm_set_string(ftdmchan->caller_data.ani.digits, str);
							ftdm_set_string(ftdmchan->caller_data.cid_num.digits, ftdmchan->caller_data.ani.digits);
						}
						break;
					case MDMF_NO_NUM:
						{
							ftdm_set_string(ftdmchan->caller_data.ani.digits, *str == 'P' ? "private" : "unknown");
							ftdm_set_string(ftdmchan->caller_data.cid_name, ftdmchan->caller_data.ani.digits);
						}
						break;
					case MDMF_PHONE_NAME:
						{
							if (mlen > sizeof(ftdmchan->caller_data.cid_name)) {
								mlen = sizeof(ftdmchan->caller_data.cid_name);
							}
							ftdm_set_string(ftdmchan->caller_data.cid_name, str);
						}
						break;
					case MDMF_NO_NAME:
						{
							ftdm_set_string(ftdmchan->caller_data.cid_name, *str == 'P' ? "private" : "unknown");
						}
					case MDMF_DATETIME:
						{
							if (mlen > sizeof(ftdmchan->caller_data.cid_date)) {
								mlen = sizeof(ftdmchan->caller_data.cid_date);
							}
							ftdm_set_string(ftdmchan->caller_data.cid_date, str);
						}
						break;
					}
				}
				ftdm_channel_command(ftdmchan, FTDM_COMMAND_DISABLE_CALLERID_DETECT, NULL);
			}
		}

		if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_PROGRESS_DETECT) && !ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_PROGRESS)) {
			uint32_t i;

			for (i = 1; i < FTDM_TONEMAP_INVALID; i++) {
				if (ftdmchan->span->tone_finder[i].tone_count) {
					if (ftdmchan->needed_tones[i] && teletone_multi_tone_detect(&ftdmchan->span->tone_finder[i], sln, (int)slen)) {
						if (++ftdmchan->detected_tones[i]) {
							ftdmchan->needed_tones[i] = 0;
							ftdmchan->detected_tones[0]++;
						}
					}
				}
			}
		}
	
		
		if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_DTMF_DETECT) && !ftdm_channel_test_feature(ftdmchan, FTDM_CHANNEL_FEATURE_DTMF_DETECT)) {
			teletone_dtmf_detect(&ftdmchan->dtmf_detect, sln, (int)slen);
			teletone_dtmf_get(&ftdmchan->dtmf_detect, digit_str, sizeof(digit_str));

			if(*digit_str) {
				fio_event_cb_t event_callback = NULL;

				if (ftdmchan->state == FTDM_CHANNEL_STATE_CALLWAITING && (*digit_str == 'D' || *digit_str == 'A')) {
					ftdmchan->detected_tones[FTDM_TONEMAP_CALLWAITING_ACK]++;
				} else {
					ftdm_channel_queue_dtmf(ftdmchan, digit_str);

					if (ftdmchan->span->event_callback) {
						event_callback = ftdmchan->span->event_callback;
					} else if (ftdmchan->event_callback) {
						event_callback = ftdmchan->event_callback;
					}

					if (event_callback) {
						ftdmchan->event_header.channel = ftdmchan;
						ftdmchan->event_header.e_type = FTDM_EVENT_DTMF;
						ftdmchan->event_header.data = digit_str;
						event_callback(ftdmchan, &ftdmchan->event_header);
						ftdmchan->event_header.e_type = FTDM_EVENT_NONE;
						ftdmchan->event_header.data = NULL;
					}
					if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_SUPRESS_DTMF)) {
						ftdmchan->skip_read_frames = 20;
					}
				}
			}
		}
	}

	if (ftdmchan->skip_read_frames > 0 || ftdm_test_flag(ftdmchan, FTDM_CHANNEL_MUTE)) {
		
		ftdm_mutex_lock(ftdmchan->pre_buffer_mutex);
		if (ftdmchan->pre_buffer && ftdm_buffer_inuse(ftdmchan->pre_buffer)) {
			ftdm_buffer_zero(ftdmchan->pre_buffer);
		}
		ftdm_mutex_unlock(ftdmchan->pre_buffer_mutex);


		memset(data, 255, *datalen);

		if (ftdmchan->skip_read_frames > 0) {
			ftdmchan->skip_read_frames--;
		}
	}  else	{
		ftdm_mutex_lock(ftdmchan->pre_buffer_mutex);
		if (ftdmchan->pre_buffer_size && ftdmchan->pre_buffer) {
			ftdm_buffer_write(ftdmchan->pre_buffer, data, *datalen);
			if (ftdm_buffer_inuse(ftdmchan->pre_buffer) >= ftdmchan->pre_buffer_size) {
				ftdm_buffer_read(ftdmchan->pre_buffer, data, *datalen);
			} else {
				memset(data, 255, *datalen);
			}
		}
		ftdm_mutex_unlock(ftdmchan->pre_buffer_mutex);
	}


	return status;
}


FT_DECLARE(ftdm_status_t) ftdm_channel_write(ftdm_channel_t *ftdmchan, void *data, ftdm_size_t datasize, ftdm_size_t *datalen)
{
	ftdm_status_t status = FTDM_FAIL;
	fio_codec_t codec_func = NULL;
	ftdm_size_t max = datasize;
	unsigned int i = 0;

	assert(ftdmchan != NULL);
	assert(ftdmchan->fio != NULL);

	if (!ftdmchan->buffer_delay && 
		((ftdmchan->dtmf_buffer && ftdm_buffer_inuse(ftdmchan->dtmf_buffer)) ||
		 (ftdmchan->fsk_buffer && ftdm_buffer_inuse(ftdmchan->fsk_buffer)))) {
		/* read size writing DTMF ATM */
		return FTDM_SUCCESS;
	}


    if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OPEN)) {
		snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "channel not open");
        return FTDM_FAIL;
    }

	if (!ftdmchan->fio->write) {
		snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "method not implemented");
		return FTDM_FAIL;
	}
	
	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_TRANSCODE) && ftdmchan->effective_codec != ftdmchan->native_codec) {
		if (ftdmchan->native_codec == FTDM_CODEC_ULAW && ftdmchan->effective_codec == FTDM_CODEC_SLIN) {
			codec_func = fio_slin2ulaw;
		} else if (ftdmchan->native_codec == FTDM_CODEC_ULAW && ftdmchan->effective_codec == FTDM_CODEC_ALAW) {
			codec_func = fio_alaw2ulaw;
		} else if (ftdmchan->native_codec == FTDM_CODEC_ALAW && ftdmchan->effective_codec == FTDM_CODEC_SLIN) {
			codec_func = fio_slin2alaw;
		} else if (ftdmchan->native_codec == FTDM_CODEC_ALAW && ftdmchan->effective_codec == FTDM_CODEC_ULAW) {
			codec_func = fio_ulaw2alaw;
		}

		if (codec_func) {
			status = codec_func(data, max, datalen);
		} else {
			snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "codec error!");
			status = FTDM_FAIL;
		}
	}	
	
	if (ftdmchan->fds[1] > -1) {
		int dlen = (int) *datalen;
		if ((write(ftdmchan->fds[1], data, dlen)) != dlen) {
			snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "file write error!");
			return FTDM_FAIL;
		}
	}

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_USE_TX_GAIN) 
		&& (ftdmchan->native_codec == FTDM_CODEC_ALAW || ftdmchan->native_codec == FTDM_CODEC_ULAW)) {
		unsigned char *wdata = data;
		for (i = 0; i < *datalen; i++) {
			wdata[i] = ftdmchan->txgain_table[wdata[i]];
		}
	}
    status = ftdmchan->fio->write(ftdmchan, data, datalen);

	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_clear_vars(ftdm_channel_t *ftdmchan)
{
	if(ftdmchan->variable_hash) {
		hashtable_destroy(ftdmchan->variable_hash);
	}
	ftdmchan->variable_hash = create_hashtable(16, ftdm_hash_hashfromstring, ftdm_hash_equalkeys);

	if(!ftdmchan->variable_hash)
		return FTDM_FAIL;
	
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_add_var(ftdm_channel_t *ftdmchan, const char *var_name, const char *value)
{
	char *t_name = 0, *t_val = 0;

	if(!ftdmchan->variable_hash || !var_name || !value)
	{
		return FTDM_FAIL;
	}

	t_name = ftdm_strdup(var_name);
	t_val = ftdm_strdup(value);

	if(hashtable_insert(ftdmchan->variable_hash, t_name, t_val, HASHTABLE_FLAG_FREE_KEY | HASHTABLE_FLAG_FREE_VALUE)) {
		return FTDM_SUCCESS;
	}
	return FTDM_FAIL;
}

FT_DECLARE(const char *) ftdm_channel_get_var(ftdm_channel_t *ftdmchan, const char *var_name)
{
	if(!ftdmchan->variable_hash || !var_name)
	{
		return NULL;
	}
	return (const char *) hashtable_search(ftdmchan->variable_hash, (void *)var_name);
}

static struct {
	ftdm_io_interface_t *pika_interface;
} interfaces;


FT_DECLARE(char *) ftdm_api_execute(const char *type, const char *cmd)
{
	ftdm_io_interface_t *fio = NULL;
	char *dup = NULL, *p;
	char *rval = NULL;

	if (type && !cmd) {
		dup = ftdm_strdup(type);
		if ((p = strchr(dup, ' '))) {
			*p++ = '\0';
			cmd = p;
		}

		type = dup;
	}
	
	ftdm_mutex_lock(globals.mutex);
	if (!(fio = (ftdm_io_interface_t *) hashtable_search(globals.interface_hash, (void *)type))) {
		ftdm_load_module_assume(type);
		if ((fio = (ftdm_io_interface_t *) hashtable_search(globals.interface_hash, (void *)type))) {
			ftdm_log(FTDM_LOG_INFO, "auto-loaded '%s'\n", type);
		}
	}
	ftdm_mutex_unlock(globals.mutex);

	if (fio && fio->api) {
		ftdm_stream_handle_t stream = { 0 };
		ftdm_status_t status;
		FTDM_STANDARD_STREAM(stream);
		status = fio->api(&stream, cmd);
		
		if (status != FTDM_SUCCESS) {
			ftdm_safe_free(stream.data);
		} else {
			rval = (char *) stream.data;
		}
	}

	ftdm_safe_free(dup);
	
	return rval;
}

static void ftdm_set_channels_gains(ftdm_span_t *span, int currindex, float rxgain, float txgain)
{
	unsigned chan_index = 0;

	if (!span->chan_count) {
		return;
	}

	for (chan_index = currindex+1; chan_index <= span->chan_count; chan_index++) {
		if (!FTDM_IS_VOICE_CHANNEL(span->channels[chan_index])) {
			continue;
		}
		ftdm_channel_command(span->channels[chan_index], FTDM_COMMAND_SET_RX_GAIN, &rxgain);
		ftdm_channel_command(span->channels[chan_index], FTDM_COMMAND_SET_TX_GAIN, &txgain);
	}
}



static ftdm_status_t ftdm_group_add_channels(const char* name, ftdm_span_t* span, int currindex);
static ftdm_status_t load_config(void)
{
	char cfg_name[] = "freetdm.conf";
	ftdm_config_t cfg;
	char *var, *val;
	int catno = -1;
	int currindex = 0;
	ftdm_span_t *span = NULL;
	unsigned configured = 0, d = 0;
	char name[80] = "";
	char number[25] = "";
	char group_name[80] = "default";
	ftdm_io_interface_t *fio = NULL;
	ftdm_analog_start_type_t tmp;
	float rxgain = 0.0;
	float txgain = 0.0;
	ftdm_size_t len = 0;

	if (!ftdm_config_open_file(&cfg, cfg_name)) {
		return FTDM_FAIL;
	}
	
	while (ftdm_config_next_pair(&cfg, &var, &val)) {
		if (*cfg.category == '#') {
			if (cfg.catno != catno) {
				ftdm_log(FTDM_LOG_DEBUG, "Skipping %s\n", cfg.category);
				catno = cfg.catno;
			}
		} else if (!strncasecmp(cfg.category, "span", 4)) {
			if (cfg.catno != catno) {
				char *type = cfg.category + 4;
				char *name;
				
				if (*type == ' ') {
					type++;
				}
				
				ftdm_log(FTDM_LOG_DEBUG, "found config for span\n");
				catno = cfg.catno;
				
				if (ftdm_strlen_zero(type)) {
					ftdm_log(FTDM_LOG_CRIT, "failure creating span, no type specified.\n");
					span = NULL;
					continue;
				}

				if ((name = strchr(type, ' '))) {
					*name++ = '\0';
				}

				ftdm_mutex_lock(globals.mutex);
				if (!(fio = (ftdm_io_interface_t *) hashtable_search(globals.interface_hash, type))) {
					ftdm_load_module_assume(type);
					if ((fio = (ftdm_io_interface_t *) hashtable_search(globals.interface_hash, type))) {
						ftdm_log(FTDM_LOG_INFO, "auto-loaded '%s'\n", type);
					}
				}
				ftdm_mutex_unlock(globals.mutex);

				if (!fio) {
					ftdm_log(FTDM_LOG_CRIT, "failure creating span, no such type '%s'\n", type);
					span = NULL;
					continue;
				}

				if (!fio->configure_span) {
					ftdm_log(FTDM_LOG_CRIT, "failure creating span, no configure_span method for '%s'\n", type);
					span = NULL;
					continue;
				}

				if (ftdm_span_create(fio, &span, name) == FTDM_SUCCESS) {
					span->type = ftdm_strdup(type);
					d = 0;

					ftdm_log(FTDM_LOG_DEBUG, "created span %d (%s) of type %s\n", span->span_id, span->name, type);
					
				} else {
					ftdm_log(FTDM_LOG_CRIT, "failure creating span of type %s\n", type);
					span = NULL;
					continue;
				}
			}

			if (!span) {
				continue;
			}

			ftdm_log(FTDM_LOG_DEBUG, "span %d [%s]=[%s]\n", span->span_id, var, val);
			
			if (!strcasecmp(var, "trunk_type")) {
				span->trunk_type = ftdm_str2ftdm_trunk_type(val);
				ftdm_log(FTDM_LOG_DEBUG, "setting trunk type to '%s'\n", ftdm_trunk_type2str(span->trunk_type)); 
			} else if (!strcasecmp(var, "name")) {
				if (!strcasecmp(val, "undef")) {
					*name = '\0';
				} else {
					ftdm_copy_string(name, val, sizeof(name));
				}
			} else if (!strcasecmp(var, "number")) {
				if (!strcasecmp(val, "undef")) {
					*number = '\0';
				} else {
					ftdm_copy_string(number, val, sizeof(number));
				}
			} else if (!strcasecmp(var, "analog-start-type")) {
				if (span->trunk_type == FTDM_TRUNK_FXS || span->trunk_type == FTDM_TRUNK_FXO || span->trunk_type == FTDM_TRUNK_EM) {
					if ((tmp = ftdm_str2ftdm_analog_start_type(val)) != FTDM_ANALOG_START_NA) {
						span->start_type = tmp;
						ftdm_log(FTDM_LOG_DEBUG, "changing start type to '%s'\n", ftdm_analog_start_type2str(span->start_type)); 
					}
				} else {
					ftdm_log(FTDM_LOG_ERROR, "This option is only valid on analog trunks!\n");
				}
			} else if (!strcasecmp(var, "fxo-channel")) {
				if (span->trunk_type == FTDM_TRUNK_NONE) {
					span->trunk_type = FTDM_TRUNK_FXO;										
					ftdm_log(FTDM_LOG_DEBUG, "setting trunk type to '%s' start(%s)\n", ftdm_trunk_type2str(span->trunk_type), 
							ftdm_analog_start_type2str(span->start_type));
				}
				if (span->trunk_type == FTDM_TRUNK_FXO) {
					currindex = span->chan_count;
					configured += fio->configure_span(span, val, FTDM_CHAN_TYPE_FXO, name, number);
					ftdm_set_channels_gains(span, currindex, rxgain, txgain);
					ftdm_group_add_channels(group_name, span, currindex);
				} else {
					ftdm_log(FTDM_LOG_WARNING, "Cannot add FXO channels to an FXS trunk!\n");
				}
			} else if (!strcasecmp(var, "fxs-channel")) {
				if (span->trunk_type == FTDM_TRUNK_NONE) {
					span->trunk_type = FTDM_TRUNK_FXS;
					ftdm_log(FTDM_LOG_DEBUG, "setting trunk type to '%s' start(%s)\n", ftdm_trunk_type2str(span->trunk_type), 
							ftdm_analog_start_type2str(span->start_type));
				}
				if (span->trunk_type == FTDM_TRUNK_FXS) {
					currindex = span->chan_count;
					configured += fio->configure_span(span, val, FTDM_CHAN_TYPE_FXS, name, number);
					ftdm_set_channels_gains(span, currindex, rxgain, txgain);
					ftdm_group_add_channels(group_name, span, currindex);
				} else {
					ftdm_log(FTDM_LOG_WARNING, "Cannot add FXS channels to an FXO trunk!\n");
				}
			} else if (!strcasecmp(var, "em-channel")) {
				if (span->trunk_type == FTDM_TRUNK_NONE) {
					span->trunk_type = FTDM_TRUNK_EM;
					ftdm_log(FTDM_LOG_DEBUG, "setting trunk type to '%s' start(%s)\n", ftdm_trunk_type2str(span->trunk_type), 
							ftdm_analog_start_type2str(span->start_type));
				}
				if (span->trunk_type == FTDM_TRUNK_EM) {
					currindex = span->chan_count;
					configured += fio->configure_span(span, val, FTDM_CHAN_TYPE_EM, name, number);
					ftdm_set_channels_gains(span, currindex, rxgain, txgain);
					ftdm_group_add_channels(group_name, span, currindex);
				} else {
					ftdm_log(FTDM_LOG_WARNING, "Cannot add EM channels to a non-EM trunk!\n");
				}
			} else if (!strcasecmp(var, "b-channel")) {
				currindex = span->chan_count;
				configured += fio->configure_span(span, val, FTDM_CHAN_TYPE_B, name, number);
				ftdm_set_channels_gains(span, currindex, rxgain, txgain);
				ftdm_group_add_channels(group_name, span, currindex);
			} else if (!strcasecmp(var, "d-channel")) {
				if (d) {
					ftdm_log(FTDM_LOG_WARNING, "ignoring extra d-channel\n");
				} else {
					ftdm_chan_type_t qtype;
					if (!strncasecmp(val, "lapd:", 5)) {
						qtype = FTDM_CHAN_TYPE_DQ931;
						val += 5;
					} else {
						qtype = FTDM_CHAN_TYPE_DQ921;
					}
					configured += fio->configure_span(span, val, qtype, name, number);
					d++;
				}
			} else if (!strcasecmp(var, "cas-channel")) {
				currindex = span->chan_count;
				configured += fio->configure_span(span, val, FTDM_CHAN_TYPE_CAS, name, number);	
				ftdm_set_channels_gains(span, currindex, rxgain, txgain);
				ftdm_group_add_channels(group_name, span, currindex);
			} else if (!strcasecmp(var, "dtmf_hangup")) {
				span->dtmf_hangup = ftdm_strdup(val);
				span->dtmf_hangup_len = strlen(val);
			} else if (!strcasecmp(var, "txgain")) {
				if (sscanf(val, "%f", &txgain) != 1) {
					ftdm_log(FTDM_LOG_ERROR, "invalid txgain: '%s'\n", val);
				}
			} else if (!strcasecmp(var, "rxgain")) {
				if (sscanf(val, "%f", &rxgain) != 1) {
					ftdm_log(FTDM_LOG_ERROR, "invalid rxgain: '%s'\n", val);
				}
			} else if (!strcasecmp(var, "group")) {
				len = strlen(val);
				if (len >= sizeof(group_name)) {
					len = sizeof(group_name) - 1;
					ftdm_log(FTDM_LOG_WARNING, "Truncating group name %s to %zd length\n", val, len);
				}
				memcpy(group_name, val, len);
				group_name[len] = '\0';
			} else {
				ftdm_log(FTDM_LOG_ERROR, "unknown span variable '%s'\n", var);
			}
		} else if (!strncasecmp(cfg.category, "general", 7)) {
			if (!strncasecmp(var, "cpu_monitoring_interval", sizeof("cpu_monitoring_interval")-1)) {
				if (atoi(val) > 0) {
					globals.cpu_monitor.interval = atoi(val);
				} else {
					ftdm_log(FTDM_LOG_ERROR, "Invalid cpu monitoring interval %s\n", val);
				}
			} else if (!strncasecmp(var, "cpu_set_alarm_threshold", sizeof("cpu_set_alarm_threshold")-1)) {
				if (atoi(val) > 0 && atoi(val) < 100) {
					globals.cpu_monitor.set_alarm_threshold = atoi(val);
				} else {
					ftdm_log(FTDM_LOG_ERROR, "Invalid cpu alarm set threshold %s\n", val);
				}
			} else if (!strncasecmp(var, "cpu_reset_alarm_threshold", sizeof("cpu_reset_alarm_threshold")-1)) {
				if (atoi(val) > 0 && atoi(val) < 100) {
					globals.cpu_monitor.reset_alarm_threshold = atoi(val);
					if (globals.cpu_monitor.reset_alarm_threshold > globals.cpu_monitor.set_alarm_threshold) {
						globals.cpu_monitor.reset_alarm_threshold = globals.cpu_monitor.set_alarm_threshold - 10;
						ftdm_log(FTDM_LOG_ERROR, "Cpu alarm reset threshold must be lower than set threshold"
								", setting threshold to %d\n", globals.cpu_monitor.reset_alarm_threshold);
					}
				} else {
					ftdm_log(FTDM_LOG_ERROR, "Invalid cpu alarm reset threshold %s\n", val);
				}
			} else if (!strncasecmp(var, "cpu_alarm_action", sizeof("cpu_alarm_action")-1)) {
				char* p = val;
				do {
					if (!strncasecmp(p, "reject", sizeof("reject")-1)) {
						globals.cpu_monitor.alarm_action_flags |= FTDM_CPU_ALARM_ACTION_REJECT;
					} else if (!strncasecmp(p, "warn", sizeof("warn")-1)) {
						globals.cpu_monitor.alarm_action_flags |= FTDM_CPU_ALARM_ACTION_WARN;
					}
					p = strchr(p, ',');
					if (p) {
						while(*p++) if (*p != 0x20) break;
					}
				} while (p);
			}
		} else {
			ftdm_log(FTDM_LOG_ERROR, "unknown param [%s] '%s' / '%s'\n", cfg.category, var, val);
		}
	}
	ftdm_config_close_file(&cfg);

	ftdm_log(FTDM_LOG_INFO, "Configured %u channel(s)\n", configured);
	
	return configured ? FTDM_SUCCESS : FTDM_FAIL;
}

static ftdm_status_t process_module_config(ftdm_io_interface_t *fio)
{
	ftdm_config_t cfg;
	char *var, *val;
	char filename[256] = "";
	
	ftdm_assert_return(fio != NULL, FTDM_FAIL, "fio argument is null\n");

	snprintf(filename, sizeof(filename), "%s.conf", fio->name);

	if (!fio->configure) {
		ftdm_log(FTDM_LOG_DEBUG, "Module %s does not support configuration.\n", fio->name);	
		return FTDM_FAIL;
	}

	if (!ftdm_config_open_file(&cfg, filename)) {
		ftdm_log(FTDM_LOG_ERROR, "Cannot open %s\n", filename);	
		return FTDM_FAIL;
	}

	while (ftdm_config_next_pair(&cfg, &var, &val)) {
		fio->configure(cfg.category, var, val, cfg.lineno);
	}

	ftdm_config_close_file(&cfg);	

	return FTDM_SUCCESS;
}

FT_DECLARE(char *) ftdm_build_dso_path(const char *name, char *path, ftdm_size_t len)
{
#ifdef WIN32
    const char *ext = ".dll";
    //const char *EXT = ".DLL";
#define FTDM_MOD_DIR "." //todo
#elif defined (MACOSX) || defined (DARWIN)
    const char *ext = ".dylib";
    //const char *EXT = ".DYLIB";
#else
    const char *ext = ".so";
    //const char *EXT = ".SO";
#endif
	if (*name == *FTDM_PATH_SEPARATOR) {
		snprintf(path, len, "%s%s", name, ext);
	} else {
		snprintf(path, len, "%s%s%s%s", FTDM_MOD_DIR, FTDM_PATH_SEPARATOR, name, ext);
	}
	return path;	
}

FT_DECLARE(ftdm_status_t) ftdm_global_add_io_interface(ftdm_io_interface_t *interface1)
{
	ftdm_status_t ret = FTDM_SUCCESS;
	ftdm_mutex_lock(globals.mutex);
	if (hashtable_search(globals.interface_hash, (void *)interface1->name)) {
		ftdm_log(FTDM_LOG_ERROR, "Interface %s already loaded!\n", interface1->name);
	} else {
		hashtable_insert(globals.interface_hash, (void *)interface1->name, interface1, HASHTABLE_FLAG_NONE);
	}
	ftdm_mutex_unlock(globals.mutex);
	return ret;
}

FT_DECLARE(int) ftdm_load_module(const char *name)
{
	ftdm_dso_lib_t lib;
	int count = 0, x = 0;
	char path[128] = "";
	char *err;
	ftdm_module_t *mod;

	ftdm_build_dso_path(name, path, sizeof(path));

	if (!(lib = ftdm_dso_open(path, &err))) {
		ftdm_log(FTDM_LOG_ERROR, "Error loading %s [%s]\n", path, err);
		ftdm_safe_free(err);
		return 0;
	}
	
	if (!(mod = (ftdm_module_t *) ftdm_dso_func_sym(lib, "ftdm_module", &err))) {
		ftdm_log(FTDM_LOG_ERROR, "Error loading %s [%s]\n", path, err);
		ftdm_safe_free(err);
		return 0;
	}

	if (mod->io_load) {
		ftdm_io_interface_t *interface1 = NULL; /* name conflict w/windows here */

		if (mod->io_load(&interface1) != FTDM_SUCCESS || !interface1 || !interface1->name) {
			ftdm_log(FTDM_LOG_ERROR, "Error loading %s\n", path);
		} else {
			ftdm_log(FTDM_LOG_INFO, "Loading IO from %s [%s]\n", path, interface1->name);
			if (ftdm_global_add_io_interface(interface1) == FTDM_SUCCESS) {
				process_module_config(interface1);
				x++;
			}
		}
	}

	if (mod->sig_load) {
		if (mod->sig_load() != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "Error loading %s\n", path);
		} else {
			ftdm_log(FTDM_LOG_INFO, "Loading SIG from %s\n", path);
			x++;
		}
	}

	if (x) {
		char *p;
		mod->lib = lib;
		ftdm_set_string(mod->path, path);
		if (mod->name[0] == '\0') {
			if (!(p = strrchr(path, *FTDM_PATH_SEPARATOR))) {
				p = path;
			}
			ftdm_set_string(mod->name, p);
		}

		ftdm_mutex_lock(globals.mutex);
		if (hashtable_search(globals.module_hash, (void *)mod->name)) {
			ftdm_log(FTDM_LOG_ERROR, "Module %s already loaded!\n", mod->name);
			ftdm_dso_destroy(&lib);
		} else {
			hashtable_insert(globals.module_hash, (void *)mod->name, mod, HASHTABLE_FLAG_NONE);
			count++;
		}
		ftdm_mutex_unlock(globals.mutex);
	} else {
		ftdm_log(FTDM_LOG_ERROR, "Unloading %s\n", path);
		ftdm_dso_destroy(&lib);
	}
	
	return count;
}

FT_DECLARE(int) ftdm_load_module_assume(const char *name)
{
	char buf[256] = "";

	snprintf(buf, sizeof(buf), "ftmod_%s", name);
	return ftdm_load_module(buf);
}

FT_DECLARE(int) ftdm_load_modules(void)
{
	char cfg_name[] = "modules.conf";
	ftdm_config_t cfg;
	char *var, *val;
	int count = 0;

	if (!ftdm_config_open_file(&cfg, cfg_name)) {
        return FTDM_FAIL;
    }

	while (ftdm_config_next_pair(&cfg, &var, &val)) {
        if (!strcasecmp(cfg.category, "modules")) {
			if (!strcasecmp(var, "load")) {
				count += ftdm_load_module(val);
			}
		}
	}
			
	return count;
}

FT_DECLARE(ftdm_status_t) ftdm_unload_modules(void)
{
	ftdm_hash_iterator_t *i = NULL;
	ftdm_dso_lib_t lib = NULL;
	char modpath[255] = { 0 };

	/* stop signaling interfaces first as signaling depends on I/O and not the other way around */
	for (i = hashtable_first(globals.module_hash); i; i = hashtable_next(i)) {
		const void *key = NULL;
		void *val = NULL;
		ftdm_module_t *mod = NULL;

		hashtable_this(i, &key, NULL, &val);
		
		if (!key || !val) {
			continue;
		}
		
		mod = (ftdm_module_t *) val;

		if (!mod->sig_unload) {
			continue;
		}

		ftdm_log(FTDM_LOG_INFO, "Unloading signaling interface %s\n", mod->name);
		
		if (mod->sig_unload() != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "Error unloading signaling interface %s\n", mod->name);
			continue;
		}

		ftdm_log(FTDM_LOG_INFO, "Unloaded signaling interface %s\n", mod->name);
	}

	/* Now go ahead with I/O interfaces */
	for (i = hashtable_first(globals.module_hash); i; i = hashtable_next(i)) {
		const void *key = NULL;
		void *val = NULL;
		ftdm_module_t *mod = NULL;

		hashtable_this(i, &key, NULL, &val);
		
		if (!key || !val) {
			continue;
		}
		
		mod = (ftdm_module_t *) val;

		if (!mod->io_unload) {
			continue;
		}

		ftdm_log(FTDM_LOG_INFO, "Unloading I/O interface %s\n", mod->name);
		
		if (mod->io_unload() != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "Error unloading I/O interface %s\n", mod->name);
			continue;
		}

		ftdm_log(FTDM_LOG_INFO, "Unloaded I/O interface %s\n", mod->name);
	}

	/* Now unload the actual shared object/dll */
	for (i = hashtable_first(globals.module_hash); i; i = hashtable_next(i)) {
		ftdm_module_t *mod = NULL;
		const void *key = NULL;
		void *val = NULL;

		hashtable_this(i, &key, NULL, &val);

		if (!key || !val) {
			continue;
		}

		mod = (ftdm_module_t *) val;

		lib = mod->lib;
		snprintf(modpath, sizeof(modpath), "%s", mod->path);
		ftdm_log(FTDM_LOG_INFO, "Unloading module %s\n", modpath);
		ftdm_dso_destroy(&lib);
		ftdm_log(FTDM_LOG_INFO, "Unloaded module %s\n", modpath);
	}

	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_configure_span(const char *type, ftdm_span_t *span, fio_signal_cb_t sig_cb, ...)
{
	ftdm_module_t *mod = (ftdm_module_t *) hashtable_search(globals.module_hash, (void *)type);
	ftdm_status_t status = FTDM_FAIL;

	if (!mod) {
		ftdm_load_module_assume(type);
		if ((mod = (ftdm_module_t *) hashtable_search(globals.module_hash, (void *)type))) {
			ftdm_log(FTDM_LOG_INFO, "auto-loaded '%s'\n", type);
		}
	}

	if (mod && mod->sig_configure) {
		va_list ap;
		va_start(ap, sig_cb);
		status = mod->sig_configure(span, sig_cb, ap);
		if (status == FTDM_SUCCESS && ftdm_test_flag(span, FTDM_SPAN_USE_CHAN_QUEUE)) {
			status = ftdm_queue_create(&span->pendingchans, SPAN_PENDING_CHANS_QUEUE_SIZE);
		}
		va_end(ap);
	} else {
		ftdm_log(FTDM_LOG_ERROR, "can't find '%s'\n", type);
		status = FTDM_FAIL;
	}

	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_configure_span_signaling(const char *type, ftdm_span_t *span, fio_signal_cb_t sig_cb, ftdm_conf_parameter_t *parameters) 
{
	ftdm_module_t *mod = (ftdm_module_t *) hashtable_search(globals.module_hash, (void *)type);
	ftdm_status_t status = FTDM_FAIL;

	ftdm_assert_return(type != NULL, FTDM_FAIL, "No signaling type");
	ftdm_assert_return(span != NULL, FTDM_FAIL, "No span");
	ftdm_assert_return(sig_cb != NULL, FTDM_FAIL, "No signaling callback");
	ftdm_assert_return(parameters != NULL, FTDM_FAIL, "No parameters");

	if (!mod) {
		ftdm_load_module_assume(type);
		if ((mod = (ftdm_module_t *) hashtable_search(globals.module_hash, (void *)type))) {
			ftdm_log(FTDM_LOG_INFO, "auto-loaded '%s'\n", type);
		}
	}

	if (!mod) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to load module type: %s\n", type);
		return FTDM_FAIL;
	}

	if (mod->configure_span_signaling) {
		status = mod->configure_span_signaling(span, sig_cb, parameters);
	} else {
		ftdm_log(FTDM_LOG_ERROR, "Module %s did not implement the signaling configuration method\n", type);
	}

	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_span_start(ftdm_span_t *span)
{
	if (span->start) {
		return span->start(span);
	}

	return FTDM_FAIL;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_add_to_group(const char* name, ftdm_channel_t* ftdmchan)
{
	unsigned int i;
	ftdm_group_t* group = NULL;
	
	ftdm_mutex_lock(globals.group_mutex);

	ftdm_assert_return(ftdmchan != NULL, FTDM_FAIL, "Cannot add a null channel to a group\n");

	if (ftdm_group_find_by_name(name, &group) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_DEBUG, "Creating new group:%s\n", name);
		ftdm_group_create(&group, name);
	}

	/*verify that group does not already include this channel first */
	for(i = 0; i < group->chan_count; i++) {
		if (group->channels[i]->physical_span_id == ftdmchan->physical_span_id &&
				group->channels[i]->physical_chan_id == ftdmchan->physical_chan_id) {

			ftdm_mutex_unlock(globals.group_mutex);
			ftdm_log(FTDM_LOG_DEBUG, "Channel %d:%d is already added to group %s\n", 
					group->channels[i]->physical_span_id,
					group->channels[i]->physical_chan_id,
					name);
			return FTDM_SUCCESS;
		}
	}

	if (group->chan_count >= FTDM_MAX_CHANNELS_GROUP) {
		ftdm_log(FTDM_LOG_ERROR, "Max number of channels exceeded (max:%d)\n", FTDM_MAX_CHANNELS_GROUP);
		ftdm_mutex_unlock(globals.group_mutex);
		return FTDM_FAIL;
	}

	group->channels[group->chan_count++] = ftdmchan;
	ftdm_mutex_unlock(globals.group_mutex);
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_remove_from_group(ftdm_group_t* group, ftdm_channel_t* ftdmchan)
{
	unsigned int i, j;
	//Need to test this function
	ftdm_mutex_lock(globals.group_mutex);

	for (i=0; i < group->chan_count; i++) {
			if (group->channels[i]->physical_span_id == ftdmchan->physical_span_id &&
					group->channels[i]->physical_chan_id == ftdmchan->physical_chan_id) {

				j=i;
				while(j < group->chan_count-1) {
					group->channels[j] = group->channels[j+1];
					j++;
				}
				group->channels[group->chan_count--] = NULL;
				if (group->chan_count <=0) {
					/* Delete group if it is empty */
					hashtable_remove(globals.group_hash, (void *)group->name);
				}
				ftdm_mutex_unlock(globals.group_mutex);
				return FTDM_SUCCESS;
			}
	}

	ftdm_mutex_unlock(globals.group_mutex);
	//Group does not contain this channel
	return FTDM_FAIL;
}

static ftdm_status_t ftdm_group_add_channels(const char* name, ftdm_span_t* span, int currindex)
{
	unsigned chan_index = 0;

	ftdm_assert_return(strlen(name) > 0, FTDM_FAIL, "Invalid group name provided\n");
	ftdm_assert_return(currindex >= 0, FTDM_FAIL, "Invalid current channel index provided\n");

	if (!span->chan_count) {
		return FTDM_SUCCESS;
	}

	for (chan_index = currindex+1; chan_index <= span->chan_count; chan_index++) {
		if (!FTDM_IS_VOICE_CHANNEL(span->channels[chan_index])) {
			continue;
		}
		if (ftdm_channel_add_to_group(name, span->channels[chan_index])) {
			ftdm_log(FTDM_LOG_ERROR, "Failed to add chan:%d to group:%s\n", chan_index, name);
		}
	}
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_group_find(uint32_t id, ftdm_group_t **group)
{
	ftdm_group_t *fgroup = NULL, *grp;

	if (id > FTDM_MAX_GROUPS_INTERFACE) {
		return FTDM_FAIL;
	}

	
	ftdm_mutex_lock(globals.group_mutex);
	for (grp = globals.groups; grp; grp = grp->next) {
		if (grp->group_id == id) {
			fgroup = grp;
			break;
		}
	}
	ftdm_mutex_unlock(globals.group_mutex);

	if (!fgroup) {
		return FTDM_FAIL;
	}

	*group = fgroup;

	return FTDM_SUCCESS;
	
}

FT_DECLARE(ftdm_status_t) ftdm_group_find_by_name(const char *name, ftdm_group_t **group)
{
	ftdm_status_t status = FTDM_FAIL;
	*group = NULL;
	ftdm_mutex_lock(globals.group_mutex);
	if (!ftdm_strlen_zero(name)) {
		if ((*group = hashtable_search(globals.group_hash, (void *) name))) {
			status = FTDM_SUCCESS;
		}
	}
	ftdm_mutex_unlock(globals.group_mutex);
	return status;
}

static void ftdm_group_add(ftdm_group_t *group)
{
	ftdm_group_t *grp;
	ftdm_mutex_lock(globals.group_mutex);
	
	for (grp = globals.groups; grp && grp->next; grp = grp->next);

	if (grp) {
		grp->next = group;
	} else {
		globals.groups = group;
	}
	hashtable_insert(globals.group_hash, (void *)group->name, group, HASHTABLE_FLAG_NONE);

	ftdm_mutex_unlock(globals.group_mutex);
}


FT_DECLARE(ftdm_status_t) ftdm_group_create(ftdm_group_t **group, const char *name)
{
	ftdm_group_t *new_group = NULL;
	ftdm_status_t status = FTDM_FAIL;

	ftdm_mutex_lock(globals.mutex);
	if (globals.group_index < FTDM_MAX_GROUPS_INTERFACE) {
		new_group = ftdm_calloc(1, sizeof(*new_group));
		
		ftdm_assert(new_group != NULL, "Failed to create new ftdm group, expect a crash\n");

		status = ftdm_mutex_create(&new_group->mutex);

		ftdm_assert(status == FTDM_SUCCESS, "Failed to create group mutex, expect a crash\n");

		new_group->group_id = ++globals.group_index;
		new_group->name = ftdm_strdup(name);
		ftdm_group_add(new_group);
		*group = new_group;
		status = FTDM_SUCCESS;
	} else {
		ftdm_log(FTDM_LOG_ERROR, "Group %s was not added, we exceeded the max number of groups\n", name);
	}
	ftdm_mutex_unlock(globals.mutex);
	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_span_send_signal(ftdm_span_t *span, ftdm_sigmsg_t *sigmsg)
{
	ftdm_status_t status = FTDM_FAIL;

	if (span->signal_cb) {
		if (sigmsg->channel) {
			ftdm_mutex_lock(sigmsg->channel->mutex);
		}

		status = span->signal_cb(sigmsg);

		if (sigmsg->channel) {
			ftdm_mutex_unlock(sigmsg->channel->mutex);
		}
	}

	return status;
}

static void *ftdm_cpu_monitor_run(ftdm_thread_t *me, void *obj)
{
	cpu_monitor_t *monitor = (cpu_monitor_t *)obj;
	struct ftdm_cpu_monitor_stats *cpu_stats = ftdm_new_cpu_monitor();
	if (!cpu_stats) {
		return NULL;
	}
	monitor->running = 1;

	while(ftdm_running()) {
		double time;
		if (ftdm_cpu_get_system_idle_time(cpu_stats, &time)) {
			break;
		}

		if (monitor->alarm) {
			if ((int)time >= (100 - monitor->set_alarm_threshold)) {
				ftdm_log(FTDM_LOG_DEBUG, "CPU alarm OFF (idle:%d)\n", (int) time);
				monitor->alarm = 0;
			}
			if (monitor->alarm_action_flags & FTDM_CPU_ALARM_ACTION_WARN) {
			ftdm_log(FTDM_LOG_WARNING, "CPU alarm is ON (cpu usage:%d)\n", (int) (100-time));
			}
		} else {
			if ((int)time <= (100-monitor->reset_alarm_threshold)) {
				ftdm_log(FTDM_LOG_DEBUG, "CPU alarm ON (idle:%d)\n", (int) time);
				monitor->alarm = 1;
			}
		}
		ftdm_interrupt_wait(monitor->interrupt, monitor->interval);
	}

	ftdm_delete_cpu_monitor(cpu_stats);
	monitor->running = 0;
	return NULL;
}

static ftdm_status_t ftdm_cpu_monitor_start(void)
{
	if (ftdm_interrupt_create(&globals.cpu_monitor.interrupt, FTDM_INVALID_SOCKET) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "Failed to create CPU monitor interrupt\n");
		return FTDM_FAIL;
	}

	if (ftdm_thread_create_detached(ftdm_cpu_monitor_run, &globals.cpu_monitor) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "Failed to create cpu monitor thread!!\n");
		return FTDM_FAIL;
	}
	return FTDM_SUCCESS;
}

static void ftdm_cpu_monitor_stop(void)
{
	if (!globals.cpu_monitor.interrupt) {
		return;
	}

	if (!globals.cpu_monitor.running) {
		return;
	}

	if (ftdm_interrupt_signal(globals.cpu_monitor.interrupt) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "Failed to interrupt the CPU monitor\n");
		return;
	}

	while (globals.cpu_monitor.running) {
		ftdm_sleep(10);
	}

	ftdm_interrupt_destroy(&globals.cpu_monitor.interrupt);
}

FT_DECLARE(void) ftdm_cpu_monitor_disable(void)
{
	ftdm_cpu_monitor_disabled = 1;
}


FT_DECLARE(ftdm_status_t) ftdm_global_init(void)
{
	memset(&globals, 0, sizeof(globals));

	time_init();
	
	ftdm_thread_override_default_stacksize(FTDM_THREAD_STACKSIZE);

	memset(&interfaces, 0, sizeof(interfaces));
	globals.interface_hash = create_hashtable(16, ftdm_hash_hashfromstring, ftdm_hash_equalkeys);
	globals.module_hash = create_hashtable(16, ftdm_hash_hashfromstring, ftdm_hash_equalkeys);
	globals.span_hash = create_hashtable(16, ftdm_hash_hashfromstring, ftdm_hash_equalkeys);
	globals.group_hash = create_hashtable(16, ftdm_hash_hashfromstring, ftdm_hash_equalkeys);
	ftdm_mutex_create(&globals.mutex);
	ftdm_mutex_create(&globals.span_mutex);
	ftdm_mutex_create(&globals.group_mutex);
	globals.running = 1;
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_global_configuration(void)
{
	int modcount = 0;

	if (!globals.running) {
		return FTDM_FAIL;
	}
	
	modcount = ftdm_load_modules();

	ftdm_log(FTDM_LOG_NOTICE, "Modules configured: %d \n", modcount);

	globals.cpu_monitor.interval = 1000;
	globals.cpu_monitor.alarm_action_flags = FTDM_CPU_ALARM_ACTION_WARN | FTDM_CPU_ALARM_ACTION_REJECT;
	globals.cpu_monitor.set_alarm_threshold = 80;
	globals.cpu_monitor.reset_alarm_threshold = 70;

	if (load_config() != FTDM_SUCCESS) {
		globals.running = 0;
		ftdm_log(FTDM_LOG_ERROR, "FreeTDM global configuration failed!\n");
		return FTDM_FAIL;
	}

	if (!ftdm_cpu_monitor_disabled) {
		if (ftdm_cpu_monitor_start() != FTDM_SUCCESS) {
			return FTDM_FAIL;
		}
	}


	return FTDM_SUCCESS;
}

FT_DECLARE(uint32_t) ftdm_running(void)
{
	return globals.running;
}


FT_DECLARE(ftdm_status_t) ftdm_global_destroy(void)
{
	ftdm_span_t *sp;

	time_end();

	globals.running = 0;	

	ftdm_cpu_monitor_stop();

	globals.span_index = 0;

	ftdm_span_close_all();
	
	ftdm_mutex_lock(globals.span_mutex);
	for (sp = globals.spans; sp;) {
		ftdm_span_t *cur_span = sp;
		sp = sp->next;

		if (cur_span) {
			if (ftdm_test_flag(cur_span, FTDM_SPAN_CONFIGURED)) {
				ftdm_span_destroy(cur_span);
			}

			hashtable_remove(globals.span_hash, (void *)cur_span->name);
			ftdm_safe_free(cur_span->type);
			ftdm_safe_free(cur_span->name);
			ftdm_safe_free(cur_span);
			cur_span = NULL;
		}
	}
	globals.spans = NULL;
	ftdm_mutex_unlock(globals.span_mutex);

	ftdm_unload_modules();

	ftdm_mutex_lock(globals.mutex);
	hashtable_destroy(globals.interface_hash);
	hashtable_destroy(globals.module_hash);
	hashtable_destroy(globals.span_hash);
	ftdm_mutex_unlock(globals.mutex);
	ftdm_mutex_destroy(&globals.mutex);
	ftdm_mutex_destroy(&globals.span_mutex);

	memset(&globals, 0, sizeof(globals));
	return FTDM_SUCCESS;
}


FT_DECLARE(uint32_t) ftdm_separate_string(char *buf, char delim, char **array, int arraylen)
{
	int argc;
	char *ptr;
	int quot = 0;
	char qc = '\'';
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

	/* strip quotes and leading / trailing spaces */
	for (x = 0; x < argc; x++) {
		char *p;

		while(*(array[x]) == ' ') {
			(array[x])++;
		}
		p = array[x];
		while((p = strchr(array[x], qc))) {
			memmove(p, p+1, strlen(p));
			p++;
		}
		p = array[x] + (strlen(array[x]) - 1);
		while(*p == ' ') {
			*p-- = '\0';
		}
	}

	return argc;
}

FT_DECLARE(void) ftdm_bitstream_init(ftdm_bitstream_t *bsp, uint8_t *data, uint32_t datalen, ftdm_endian_t endian, uint8_t ss)
{
	memset(bsp, 0, sizeof(*bsp));
	bsp->data = data;
	bsp->datalen = datalen;
	bsp->endian = endian;
	bsp->ss = ss;
	
	if (endian < 0) {
		bsp->top = bsp->bit_index = 7;
		bsp->bot = 0;
	} else {
		bsp->top = bsp->bit_index = 0;
		bsp->bot = 7;
	}

}

FT_DECLARE(int8_t) ftdm_bitstream_get_bit(ftdm_bitstream_t *bsp)
{
	int8_t bit = -1;
	

	if (bsp->byte_index >= bsp->datalen) {
		goto done;
	}

	if (bsp->ss) {
		if (!bsp->ssv) {
			bsp->ssv = 1;
			return 0;
		} else if (bsp->ssv == 2) {
			bsp->byte_index++;
			bsp->ssv = 0;
			return 1;
		}
	}




	bit = (bsp->data[bsp->byte_index] >> (bsp->bit_index)) & 1;
	
	if (bsp->bit_index == bsp->bot) {
		bsp->bit_index = bsp->top;
		if (bsp->ss) {
			bsp->ssv = 2;
			goto done;
		} 

		if (++bsp->byte_index > bsp->datalen) {
			bit = -1;
			goto done;
		}
		
	} else {
		bsp->bit_index = bsp->bit_index + bsp->endian;
	}


 done:
	return bit;
}

FT_DECLARE(void) print_hex_bytes(uint8_t *data, ftdm_size_t dlen, char *buf, ftdm_size_t blen)
{
	char *bp = buf;
	uint8_t *byte = data;
	uint32_t i, j = 0;

	if (blen < (dlen * 3) + 2) {
        return;
    }

	*bp++ = '[';
	j++;

	for(i = 0; i < dlen; i++) {
		snprintf(bp, blen-j, "%02x ", *byte++);
		bp += 3;
		j += 3;
	}

	*--bp = ']';

}

FT_DECLARE(void) print_bits(uint8_t *b, int bl, char *buf, int blen, ftdm_endian_t e, uint8_t ss)
{
	ftdm_bitstream_t bs;
	int j = 0, c = 0;
	int8_t bit;
	uint32_t last;

	if (blen < (bl * 10) + 2) {
        return;
    }

	ftdm_bitstream_init(&bs, b, bl, e, ss);
	last = bs.byte_index;	
	while((bit = ftdm_bitstream_get_bit(&bs)) > -1) {
		buf[j++] = bit ? '1' : '0';
		if (bs.byte_index != last) {
			buf[j++] = ' ';
			last = bs.byte_index;
			if (++c == 8) {
				buf[j++] = '\n';
				c = 0;
			}
		}
	}

}



FT_DECLARE_NONSTD(ftdm_status_t) ftdm_console_stream_raw_write(ftdm_stream_handle_t *handle, uint8_t *data, ftdm_size_t datalen)
{
	ftdm_size_t need = handle->data_len + datalen;
	
	if (need >= handle->data_size) {
		void *new_data;
		need += handle->alloc_chunk;

		if (!(new_data = realloc(handle->data, need))) {
			return FTDM_MEMERR;
		}

		handle->data = new_data;
		handle->data_size = need;
	}

	memcpy((uint8_t *) (handle->data) + handle->data_len, data, datalen);
	handle->data_len += datalen;
	handle->end = (uint8_t *) (handle->data) + handle->data_len;
	*(uint8_t *)handle->end = '\0';

	return FTDM_SUCCESS;
}

FT_DECLARE(int) ftdm_vasprintf(char **ret, const char *fmt, va_list ap) /* code from switch_apr.c */
{
#ifdef HAVE_VASPRINTF
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

	if (len > 0 && (buf = ftdm_malloc((buflen = (size_t) (len + 1)))) != NULL) {
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

FT_DECLARE_NONSTD(ftdm_status_t) ftdm_console_stream_write(ftdm_stream_handle_t *handle, const char *fmt, ...)
{
	va_list ap;
	char *buf = handle->data;
	char *end = handle->end;
	int ret = 0;
	char *data = NULL;

	if (handle->data_len >= handle->data_size) {
		return FTDM_FAIL;
	}

	va_start(ap, fmt);
	ret = ftdm_vasprintf(&data, fmt, ap);
	va_end(ap);

	if (data) {
		ftdm_size_t remaining = handle->data_size - handle->data_len;
		ftdm_size_t need = strlen(data) + 1;

		if ((remaining < need) && handle->alloc_len) {
			ftdm_size_t new_len;
			void *new_data;

			new_len = handle->data_size + need + handle->alloc_chunk;
			if ((new_data = realloc(handle->data, new_len))) {
				handle->data_size = handle->alloc_len = new_len;
				handle->data = new_data;
				buf = handle->data;
				remaining = handle->data_size - handle->data_len;
				handle->end = (uint8_t *) (handle->data) + handle->data_len;
				end = handle->end;
			} else {
				ftdm_log(FTDM_LOG_CRIT, "Memory Error!\n");
				ftdm_safe_free(data);
				return FTDM_FAIL;
			}
		}

		if (remaining < need) {
			ret = -1;
		} else {
			ret = 0;
			snprintf(end, remaining, "%s", data);
			handle->data_len = strlen(buf);
			handle->end = (uint8_t *) (handle->data) + handle->data_len;
		}
		ftdm_safe_free(data);
	}

	return ret ? FTDM_FAIL : FTDM_SUCCESS;
}

FT_DECLARE(char *) ftdm_strdup(const char *str)
{
	ftdm_size_t len = strlen(str) + 1;
	void *new = ftdm_malloc(len);

	if (!new) {
		return NULL;
	}

	return (char *)memcpy(new, str, len);
}

FT_DECLARE(char *) ftdm_strndup(const char *str, ftdm_size_t inlen)
{
	char *new = NULL;
	ftdm_size_t len = strlen(str) + 1;
	if (len > (inlen+1)) {
		len = inlen+1;
	}
	new = (char *)ftdm_malloc(len);

	if (!new) {
		return NULL;
	}
	
	memcpy(new, str, len-1);
	new[len-1] = 0;
	return new;
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
