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

#define _GNU_SOURCE
#ifndef WIN32
#endif
#include "openzap.h"
#include <stdarg.h>
#ifdef WIN32
#include <io.h>
#endif
#ifdef ZAP_PIKA_SUPPORT
#include "zap_pika.h"
#endif
#include "zap_cpu_monitor.h"

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


OZ_DECLARE(zap_time_t) zap_current_time_in_ms(void)
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
	uint8_t		running;
	uint8_t 	alarm;
	uint32_t	interval;
	uint8_t		alarm_action_flags;
	uint8_t		set_alarm_threshold;
	uint8_t		reset_alarm_threshold;
	zap_interrupt_t *interrupt;
} cpu_monitor_t;

static struct {
	zap_hash_t *interface_hash;
	zap_hash_t *module_hash;
	zap_hash_t *span_hash;
	zap_mutex_t *mutex;
	zap_mutex_t *span_mutex;
	uint32_t span_index;
	uint32_t running;
	zap_span_t *spans;
	cpu_monitor_t cpu_monitor;
} globals;

static uint8_t zap_cpu_monitor_disabled = 0;

enum zap_enum_cpu_alarm_action_flags
{
	ZAP_CPU_ALARM_ACTION_WARN = (1 << 0),
	ZAP_CPU_ALARM_ACTION_REJECT = (1 << 1)
};

/* enum lookup funcs */
ZAP_ENUM_NAMES(TONEMAP_NAMES, TONEMAP_STRINGS)
ZAP_STR2ENUM(zap_str2zap_tonemap, zap_tonemap2str, zap_tonemap_t, TONEMAP_NAMES, ZAP_TONEMAP_INVALID)

ZAP_ENUM_NAMES(OOB_NAMES, OOB_STRINGS)
ZAP_STR2ENUM(zap_str2zap_oob_event, zap_oob_event2str, zap_oob_event_t, OOB_NAMES, ZAP_OOB_INVALID)

ZAP_ENUM_NAMES(TRUNK_TYPE_NAMES, TRUNK_STRINGS)
ZAP_STR2ENUM(zap_str2zap_trunk_type, zap_trunk_type2str, zap_trunk_type_t, TRUNK_TYPE_NAMES, ZAP_TRUNK_NONE)

ZAP_ENUM_NAMES(START_TYPE_NAMES, START_TYPE_STRINGS)
ZAP_STR2ENUM(zap_str2zap_analog_start_type, zap_analog_start_type2str, zap_analog_start_type_t, START_TYPE_NAMES, ZAP_ANALOG_START_NA)

ZAP_ENUM_NAMES(SIGNAL_NAMES, SIGNAL_STRINGS)
ZAP_STR2ENUM(zap_str2zap_signal_event, zap_signal_event2str, zap_signal_event_t, SIGNAL_NAMES, ZAP_SIGEVENT_INVALID)

ZAP_ENUM_NAMES(CHANNEL_STATE_NAMES, CHANNEL_STATE_STRINGS)
ZAP_STR2ENUM(zap_str2zap_channel_state, zap_channel_state2str, zap_channel_state_t, CHANNEL_STATE_NAMES, ZAP_CHANNEL_STATE_INVALID)

ZAP_ENUM_NAMES(MDMF_TYPE_NAMES, MDMF_STRINGS)
ZAP_STR2ENUM(zap_str2zap_mdmf_type, zap_mdmf_type2str, zap_mdmf_type_t, MDMF_TYPE_NAMES, MDMF_INVALID)

ZAP_ENUM_NAMES(CHAN_TYPE_NAMES, CHAN_TYPE_STRINGS)
ZAP_STR2ENUM(zap_str2zap_chan_type, zap_chan_type2str, zap_chan_type_t, CHAN_TYPE_NAMES, ZAP_CHAN_TYPE_COUNT)

static zap_status_t zap_cpu_monitor_start(void);
static void zap_cpu_monitor_stop(void);

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


static int zap_log_level = 7;

/* Cpu monitor thread */
static void *zap_cpu_monitor_run(zap_thread_t *me, void *obj);

static void default_logger(const char *file, const char *func, int line, int level, const char *fmt, ...)
{
	const char *fp;
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

static zap_status_t zap_set_caller_data(zap_span_t *span, zap_caller_data_t *caller_data)
{
	if (!caller_data) {
		zap_log(ZAP_LOG_CRIT, "Error: trying to set caller data, but no caller_data!\n");
		return ZAP_FAIL;
	}

	if (caller_data->cid_num.plan == ZAP_NPI_INVALID) {
		caller_data->cid_num.plan = span->default_caller_data.cid_num.plan;
	}

	if (caller_data->cid_num.type == ZAP_TON_INVALID) {
		caller_data->cid_num.type = span->default_caller_data.cid_num.type;
	}

	if (caller_data->ani.plan == ZAP_NPI_INVALID) {
		caller_data->ani.plan = span->default_caller_data.ani.plan;
	}

	if (caller_data->ani.type == ZAP_TON_INVALID) {
		caller_data->ani.type = span->default_caller_data.ani.type;
	}

	if (caller_data->rdnis.plan == ZAP_NPI_INVALID) {
		caller_data->rdnis.plan = span->default_caller_data.rdnis.plan;
	}

	if (caller_data->rdnis.type == ZAP_NPI_INVALID) {
		caller_data->rdnis.type = span->default_caller_data.rdnis.type;
	}
	return ZAP_SUCCESS;
}

OZ_DECLARE(zap_status_t) zap_channel_set_caller_data(zap_channel_t *zchan, zap_caller_data_t *caller_data)
{
	zap_status_t err = ZAP_SUCCESS;
	if (!zchan) {
		zap_log(ZAP_LOG_CRIT, "Error: trying to set caller data, but no zchan!\n");
		return ZAP_FAIL;
	}
	if ((err = zap_set_caller_data(zchan->span, caller_data)) != ZAP_SUCCESS) {
		return err; 
	}
	zchan->caller_data = *caller_data;
	return ZAP_SUCCESS;
}

OZ_DECLARE_DATA zap_logger_t zap_log = null_logger;

OZ_DECLARE(void) zap_global_set_logger(zap_logger_t logger)
{
	if (logger) {
		zap_log = logger;
	} else {
		zap_log = null_logger;
	}
}

OZ_DECLARE(void) zap_global_set_default_logger(int level)
{
	if (level < 0 || level > 7) {
		level = 7;
	}

	zap_log = default_logger;
	zap_log_level = level;
}

OZ_DECLARE_NONSTD(int) zap_hash_equalkeys(void *k1, void *k2)
{
    return strcmp((char *) k1, (char *) k2) ? 0 : 1;
}

OZ_DECLARE_NONSTD(uint32_t) zap_hash_hashfromstring(void *ky)
{
	unsigned char *str = (unsigned char *) ky;
	uint32_t hash = 0;
    int c;
	
	while ((c = *str++)) {
        hash = c + (hash << 6) + (hash << 16) - hash;
	}

    return hash;
}


static zap_status_t zap_span_destroy(zap_span_t *span)
{
	zap_status_t status = ZAP_FAIL;
	
	if (zap_test_flag(span, ZAP_SPAN_CONFIGURED)) {
		if (span->stop) {
			span->stop(span);
		}
		if (span->zio && span->zio->span_destroy) {
			zap_log(ZAP_LOG_INFO, "Destroying span %u type (%s)\n", span->span_id, span->type);
			status = span->zio->span_destroy(span);
			zap_safe_free(span->type);
			zap_safe_free(span->dtmf_hangup);
		}
	}

	return status;
}

static zap_status_t zap_channel_destroy(zap_channel_t *zchan)
{

	if (zap_test_flag(zchan, ZAP_CHANNEL_CONFIGURED)) {

		while (zap_test_flag(zchan, ZAP_CHANNEL_INTHREAD)) {
			zap_log(ZAP_LOG_INFO, "Waiting for thread to exit on channel %u:%u\n", zchan->span_id, zchan->chan_id);
			zap_sleep(500);
		}

#ifdef ZAP_DEBUG_DTMF
		zap_mutex_destroy(&zchan->dtmfdbg.mutex);
#endif
		zap_mutex_lock(zchan->pre_buffer_mutex);
		zap_buffer_destroy(&zchan->pre_buffer);
		zap_mutex_unlock(zchan->pre_buffer_mutex);

		zap_buffer_destroy(&zchan->digit_buffer);
		zap_buffer_destroy(&zchan->gen_dtmf_buffer);
		zap_buffer_destroy(&zchan->dtmf_buffer);
		zap_buffer_destroy(&zchan->fsk_buffer);
		zchan->pre_buffer_size = 0;

		hashtable_destroy(zchan->variable_hash);

		zap_safe_free(zchan->dtmf_hangup_buf);

		if (zchan->tone_session.buffer) {
			teletone_destroy_session(&zchan->tone_session);
			memset(&zchan->tone_session, 0, sizeof(zchan->tone_session));
		}

		
		if (zchan->span->zio->channel_destroy) {
			zap_log(ZAP_LOG_INFO, "Closing channel %s:%u:%u fd:%d\n", zchan->span->type, zchan->span_id, zchan->chan_id, zchan->sockfd);
			if (zchan->span->zio->channel_destroy(zchan) == ZAP_SUCCESS) {
				zap_clear_flag_locked(zchan, ZAP_CHANNEL_CONFIGURED);
			} else {
				zap_log(ZAP_LOG_ERROR, "Error Closing channel %u:%u fd:%d\n", zchan->span_id, zchan->chan_id, zchan->sockfd);
			}
		}

		zap_mutex_destroy(&zchan->mutex);
		zap_mutex_destroy(&zchan->pre_buffer_mutex);
	}
	
	return ZAP_SUCCESS;
}


OZ_DECLARE(zap_status_t) zap_channel_get_alarms(zap_channel_t *zchan)
{
	zap_status_t status = ZAP_FAIL;

	if (zap_test_flag(zchan, ZAP_CHANNEL_CONFIGURED)) {
		if (zchan->span->zio->get_alarms) {
			if ((status = zchan->span->zio->get_alarms(zchan)) == ZAP_SUCCESS) {
				*zchan->last_error = '\0';
				if (zap_test_alarm_flag(zchan, ZAP_ALARM_RED)) {
					snprintf(zchan->last_error + strlen(zchan->last_error), sizeof(zchan->last_error) - strlen(zchan->last_error), "RED/");
				}
				if (zap_test_alarm_flag(zchan, ZAP_ALARM_YELLOW)) {
					snprintf(zchan->last_error + strlen(zchan->last_error), sizeof(zchan->last_error) - strlen(zchan->last_error), "YELLOW/");
				}
				if (zap_test_alarm_flag(zchan, ZAP_ALARM_BLUE)) {
					snprintf(zchan->last_error + strlen(zchan->last_error), sizeof(zchan->last_error) - strlen(zchan->last_error), "BLUE/");
				}
				if (zap_test_alarm_flag(zchan, ZAP_ALARM_LOOPBACK)) {
					snprintf(zchan->last_error + strlen(zchan->last_error), sizeof(zchan->last_error) - strlen(zchan->last_error), "LOOP/");
				}
				if (zap_test_alarm_flag(zchan, ZAP_ALARM_RECOVER)) {
					snprintf(zchan->last_error + strlen(zchan->last_error), sizeof(zchan->last_error) - strlen(zchan->last_error), "RECOVER/");
				}
				*(zchan->last_error + strlen(zchan->last_error) - 1) = '\0';

			}
		} else {
			status = ZAP_NOTIMPL;
		}
	}
	
	return status;
}

static void zap_span_add(zap_span_t *span)
{
	zap_span_t *sp;
	zap_mutex_lock(globals.span_mutex);
	for (sp = globals.spans; sp && sp->next; sp = sp->next);
	if (sp) {
		sp->next = span;
	} else {
		globals.spans = span;
	}
	hashtable_insert(globals.span_hash, (void *)span->name, span, HASHTABLE_FLAG_NONE);
	zap_mutex_unlock(globals.span_mutex);
}

#if 0
static void zap_span_del(zap_span_t *span)
{
	zap_span_t *last = NULL, *sp;

	zap_mutex_lock(globals.span_mutex);
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
	zap_mutex_unlock(globals.span_mutex);
}
#endif

OZ_DECLARE(zap_status_t) zap_span_stop(zap_span_t *span)
{
	if (span->stop) {
		span->stop(span);
		return ZAP_SUCCESS;
	}
	
	return ZAP_FAIL;
}

OZ_DECLARE(zap_status_t) zap_span_create(zap_io_interface_t *zio, zap_span_t **span, const char *name)
{
	zap_span_t *new_span = NULL;
	zap_status_t status = ZAP_FAIL;

	assert(zio != NULL);

	zap_mutex_lock(globals.mutex);

	if (globals.span_index < ZAP_MAX_SPANS_INTERFACE) {
		new_span = malloc(sizeof(*new_span));
		assert(new_span);
		memset(new_span, 0, sizeof(*new_span));
		status = zap_mutex_create(&new_span->mutex);
		assert(status == ZAP_SUCCESS);

		zap_set_flag(new_span, ZAP_SPAN_CONFIGURED);
		new_span->span_id = ++globals.span_index;
		new_span->zio = zio;
		zap_copy_string(new_span->tone_map[ZAP_TONEMAP_DIAL], "%(1000,0,350,440)", ZAP_TONEMAP_LEN);
		zap_copy_string(new_span->tone_map[ZAP_TONEMAP_RING], "%(2000,4000,440,480)", ZAP_TONEMAP_LEN);
		zap_copy_string(new_span->tone_map[ZAP_TONEMAP_BUSY], "%(500,500,480,620)", ZAP_TONEMAP_LEN);
		zap_copy_string(new_span->tone_map[ZAP_TONEMAP_ATTN], "%(100,100,1400,2060,2450,2600)", ZAP_TONEMAP_LEN);
		new_span->trunk_type = ZAP_TRUNK_NONE;
		new_span->data_type = ZAP_TYPE_SPAN;

		zap_mutex_lock(globals.span_mutex);
		if (!zap_strlen_zero(name) && hashtable_search(globals.span_hash, (void *)name)) {
			zap_log(ZAP_LOG_WARNING, "name %s is already used, substituting 'span%d' as the name\n", name, new_span->span_id);
			name = NULL;
		}
		zap_mutex_unlock(globals.span_mutex);
		
		if (!name) {
			char buf[128] = "";
			snprintf(buf, sizeof(buf), "span%d", new_span->span_id);
			name = buf;
		}
		new_span->name = strdup(name);
		zap_span_add(new_span);
		*span = new_span;
		status = ZAP_SUCCESS;
	}
	zap_mutex_unlock(globals.mutex);
	return status;
}

OZ_DECLARE(zap_status_t) zap_span_close_all(void)
{
	zap_span_t *span;
	uint32_t i = 0, j;

	zap_mutex_lock(globals.span_mutex);
	for (span = globals.spans; span; span = span->next) {
		if (zap_test_flag(span, ZAP_SPAN_CONFIGURED)) {
			for(j = 1; j <= span->chan_count && span->channels[j]; j++) {
				zap_channel_destroy(span->channels[j]);
				i++;
			}
		} 
	}
	zap_mutex_unlock(globals.span_mutex);

	return i ? ZAP_SUCCESS : ZAP_FAIL;
}

OZ_DECLARE(zap_status_t) zap_span_load_tones(zap_span_t *span, const char *mapname)
{
	zap_config_t cfg;
	char *var, *val;
	int x = 0;

	if (!zap_config_open_file(&cfg, "tones.conf")) {
		snprintf(span->last_error, sizeof(span->last_error), "error loading tones.");
		return ZAP_FAIL;
	}
	
	while (zap_config_next_pair(&cfg, &var, &val)) {
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
				zap_log(ZAP_LOG_WARNING, "Unknown tone name %s\n", var);
				continue;
			}

			index = zap_str2zap_tonemap(name);

			if (index >= ZAP_TONEMAP_INVALID || index == ZAP_TONEMAP_NONE) {
				zap_log(ZAP_LOG_WARNING, "Unknown tone name %s\n", name);
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
					zap_log(ZAP_LOG_DEBUG, "added tone detect [%s] = [%s]\n", name, val);
				}  else {
					zap_log(ZAP_LOG_DEBUG, "added tone generation [%s] = [%s]\n", name, val);
					zap_copy_string(span->tone_map[index], val, sizeof(span->tone_map[index]));
				}
				x++;
			}
		}
	}

	zap_config_close_file(&cfg);
	
	if (!x) {
		snprintf(span->last_error, sizeof(span->last_error), "error loading tones.");
		return ZAP_FAIL;
	}

	return ZAP_SUCCESS;
	
}

#define ZAP_SLINEAR_MAX_VALUE 32767
#define ZAP_SLINEAR_MIN_VALUE -32767
static void reset_gain_table(unsigned char *gain_table, float new_gain, zap_codec_t codec_gain)
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
	if (codec_gain != ZAP_CODEC_ALAW && codec_gain != ZAP_CODEC_ULAW) {
		zap_log(ZAP_LOG_WARNING, "Not resetting gain table because codec is not ALAW or ULAW but %d\n", codec_gain);
		return;
	}

	if (!new_gain) {
		/* for a 0.0db gain table, each alaw/ulaw sample value is left untouched (0 ==0, 1 == 1, 2 == 2 etc)*/
		sv = 0;
		while (1) {
			gain_table[sv] = (unsigned char)sv;
			if (sv == (ZAP_GAINS_TABLE_SIZE - 1)) {
				break;
			}
			sv++;
		}
		return;
	}

	/* use the 20log rule to increase the gain: http://en.wikipedia.org/wiki/Gain, http:/en.wikipedia.org/wiki/20_log_rule#Definitions */
	lingain = (float)pow(10.0f, new_gain/20.0f);
	sv = 0;
	while (1) {
		/* get the linear value for this alaw/ulaw sample value */
		linvalue = codec_gain == ZAP_CODEC_ALAW ? (float)alaw_to_linear(sv) : (float)ulaw_to_linear(sv);

		/* multiply the linear value and the previously calculated linear gain */
		ampvalue = (int)(linvalue * lingain);

		/* chop it if goes beyond the limits */
		if (ampvalue > ZAP_SLINEAR_MAX_VALUE) {
			ampvalue = ZAP_SLINEAR_MAX_VALUE;
		}

		if (ampvalue < ZAP_SLINEAR_MIN_VALUE) {
			ampvalue = ZAP_SLINEAR_MIN_VALUE;
		}
		gain_table[sv] = codec_gain == ZAP_CODEC_ALAW ? linear_to_alaw(ampvalue) : linear_to_ulaw(ampvalue);
		if (sv == (ZAP_GAINS_TABLE_SIZE-1)) {
			break;
		}
		sv++;
	}
}

OZ_DECLARE(zap_status_t) zap_span_add_channel(zap_span_t *span, zap_socket_t sockfd, zap_chan_type_t type, zap_channel_t **chan)
{
	unsigned i = 0;
	if (span->chan_count < ZAP_MAX_CHANNELS_SPAN) {
		zap_channel_t *new_chan = span->channels[++span->chan_count];

		if (!new_chan) {
			if (!(new_chan = malloc(sizeof(*new_chan)))) {
				return ZAP_FAIL;
			}
			span->channels[span->chan_count] = new_chan;
			memset(new_chan, 0, sizeof(*new_chan));
		}

		new_chan->type = type;
		new_chan->sockfd = sockfd;
		new_chan->zio = span->zio;
		new_chan->span_id = span->span_id;
		new_chan->chan_id = span->chan_count;
		new_chan->span = span;
		new_chan->fds[0] = -1;
		new_chan->fds[1] = -1;
		new_chan->data_type = ZAP_TYPE_CHANNEL;
		if (!new_chan->dtmf_on) {
			new_chan->dtmf_on = ZAP_DEFAULT_DTMF_ON;
		}

		if (!new_chan->dtmf_off) {
			new_chan->dtmf_off = ZAP_DEFAULT_DTMF_OFF;
		}

		zap_mutex_create(&new_chan->mutex);
		zap_mutex_create(&new_chan->pre_buffer_mutex);
#ifdef ZAP_DEBUG_DTMF
		zap_mutex_create(&new_chan->dtmfdbg.mutex);
#endif

		zap_buffer_create(&new_chan->digit_buffer, 128, 128, 0);
		zap_buffer_create(&new_chan->gen_dtmf_buffer, 128, 128, 0);
		new_chan->variable_hash = create_hashtable(16, zap_hash_hashfromstring, zap_hash_equalkeys);

		new_chan->dtmf_hangup_buf = calloc (span->dtmf_hangup_len + 1, sizeof (char));

		/* set 0.0db gain table */
		i = 0;
		while (1) {
			new_chan->txgain_table[i] = (unsigned char)i;
			new_chan->rxgain_table[i] = (unsigned char)i;
			if (i == (sizeof(new_chan->txgain_table)-1)) {
				break;
			}
			i++;
		}

		zap_set_flag(new_chan, ZAP_CHANNEL_CONFIGURED | ZAP_CHANNEL_READY);
		*chan = new_chan;
		return ZAP_SUCCESS;
	}

	return ZAP_FAIL;
}

OZ_DECLARE(zap_status_t) zap_span_find_by_name(const char *name, zap_span_t **span)
{
	zap_status_t status = ZAP_FAIL;

	zap_mutex_lock(globals.span_mutex);
	if (!zap_strlen_zero(name)) {
		if ((*span = hashtable_search(globals.span_hash, (void *)name))) {
			status = ZAP_SUCCESS;
		} else {
			int span_id = atoi(name);

			zap_span_find(span_id, span);
			if (*span) {
				status = ZAP_SUCCESS;
			}
		}
	}
	zap_mutex_unlock(globals.span_mutex);
	
	return status;
}

OZ_DECLARE(zap_status_t) zap_span_find(uint32_t id, zap_span_t **span)
{
	zap_span_t *fspan = NULL, *sp;

	if (id > ZAP_MAX_SPANS_INTERFACE) {
		return ZAP_FAIL;
	}

	zap_mutex_lock(globals.span_mutex);
	for (sp = globals.spans; sp; sp = sp->next) {
		if (sp->span_id == id) {
			fspan = sp;
			break;
		}
	}
	zap_mutex_unlock(globals.span_mutex);

	if (!fspan || !zap_test_flag(fspan, ZAP_SPAN_CONFIGURED)) {
		return ZAP_FAIL;
	}

	*span = fspan;

	return ZAP_SUCCESS;
	
}

OZ_DECLARE(zap_status_t) zap_span_set_event_callback(zap_span_t *span, zio_event_cb_t event_callback)
{
	zap_mutex_lock(span->mutex);
	span->event_callback = event_callback;
	zap_mutex_unlock(span->mutex);
	return ZAP_SUCCESS;
}


OZ_DECLARE(zap_status_t) zap_span_poll_event(zap_span_t *span, uint32_t ms)
{
	assert(span->zio != NULL);

	if (span->zio->poll_event) {
		return span->zio->poll_event(span, ms);
	} else {
		zap_log(ZAP_LOG_ERROR, "poll_event method not implemented in module %s!", span->zio->name);
	}

	return ZAP_NOTIMPL;
}

OZ_DECLARE(zap_status_t) zap_span_next_event(zap_span_t *span, zap_event_t **event)
{
	assert(span->zio != NULL);

	if (span->zio->next_event) {
		return span->zio->next_event(span, event);
	} else {
		zap_log(ZAP_LOG_ERROR, "next_event method not implemented in module %s!", span->zio->name);
	}
	
	return ZAP_NOTIMPL;
}

static zap_status_t zchan_fsk_write_sample(int16_t *buf, zap_size_t buflen, void *user_data)
{
	zap_channel_t *zchan = (zap_channel_t *) user_data;
	zap_buffer_write(zchan->fsk_buffer, buf, buflen * 2);
	return ZAP_SUCCESS;
}

OZ_DECLARE(zap_status_t) zap_channel_send_fsk_data(zap_channel_t *zchan, zap_fsk_data_state_t *fsk_data, float db_level)
{
	struct zap_fsk_modulator fsk_trans;

	if (!zchan->fsk_buffer) {
		zap_buffer_create(&zchan->fsk_buffer, 128, 128, 0);
	} else {
		zap_buffer_zero(zchan->fsk_buffer);
	}

	if (zchan->token_count > 1) {
		zap_fsk_modulator_init(&fsk_trans, FSK_BELL202, zchan->rate, fsk_data, db_level, 80, 5, 0, zchan_fsk_write_sample, zchan);
		zap_fsk_modulator_send_all((&fsk_trans));
	} else {
		zap_fsk_modulator_init(&fsk_trans, FSK_BELL202, zchan->rate, fsk_data, db_level, 180, 5, 300, zchan_fsk_write_sample, zchan);
		zap_fsk_modulator_send_all((&fsk_trans));
		zchan->buffer_delay = 3500 / zchan->effective_interval;
	}

	return ZAP_SUCCESS;
}


OZ_DECLARE(zap_status_t) zap_channel_set_event_callback(zap_channel_t *zchan, zio_event_cb_t event_callback)
{
	zap_mutex_lock(zchan->mutex);
	zchan->event_callback = event_callback;
	zap_mutex_unlock(zchan->mutex);
	return ZAP_SUCCESS;
}

OZ_DECLARE(zap_status_t) zap_channel_clear_token(zap_channel_t *zchan, const char *token)
{
	zap_status_t status = ZAP_FAIL;
	
	zap_mutex_lock(zchan->mutex);
	if (token == NULL) {
		memset(zchan->tokens, 0, sizeof(zchan->tokens));
		zchan->token_count = 0;
	} else if (*token != '\0') {
		char tokens[ZAP_MAX_TOKENS][ZAP_TOKEN_STRLEN];
		int32_t i, count = zchan->token_count;
		memcpy(tokens, zchan->tokens, sizeof(tokens));
		memset(zchan->tokens, 0, sizeof(zchan->tokens));
		zchan->token_count = 0;		

		for (i = 0; i < count; i++) {
			if (strcmp(tokens[i], token)) {
				zap_copy_string(zchan->tokens[zchan->token_count], tokens[i], sizeof(zchan->tokens[zchan->token_count]));
				zchan->token_count++;
			}
		}

		status = ZAP_SUCCESS;
	}
	zap_mutex_unlock(zchan->mutex);

	return status;
}

OZ_DECLARE(void) zap_channel_rotate_tokens(zap_channel_t *zchan)
{
	if (zchan->token_count) {
		memmove(zchan->tokens[1], zchan->tokens[0], zchan->token_count * ZAP_TOKEN_STRLEN);
		zap_copy_string(zchan->tokens[0], zchan->tokens[zchan->token_count], ZAP_TOKEN_STRLEN);
		*zchan->tokens[zchan->token_count] = '\0';
	}
}

OZ_DECLARE(void) zap_channel_replace_token(zap_channel_t *zchan, const char *old_token, const char *new_token)
{
	unsigned int i;

	if (zchan->token_count) {
		for(i = 0; i < zchan->token_count; i++) {
			if (!strcmp(zchan->tokens[i], old_token)) {
				zap_copy_string(zchan->tokens[i], new_token, ZAP_TOKEN_STRLEN);
				break;
			}
		}
	}
}

OZ_DECLARE(zap_status_t) zap_channel_add_token(zap_channel_t *zchan, char *token, int end)
{
	zap_status_t status = ZAP_FAIL;

	zap_mutex_lock(zchan->mutex);
	if (zchan->token_count < ZAP_MAX_TOKENS) {
		if (end) {
			zap_copy_string(zchan->tokens[zchan->token_count++], token, ZAP_TOKEN_STRLEN);
		} else {
			memmove(zchan->tokens[1], zchan->tokens[0], zchan->token_count * ZAP_TOKEN_STRLEN);
			zap_copy_string(zchan->tokens[0], token, ZAP_TOKEN_STRLEN);
			zchan->token_count++;
		}
		status = ZAP_SUCCESS;
	}
	zap_mutex_unlock(zchan->mutex);

	return status;
}


OZ_DECLARE(zap_status_t) zap_channel_complete_state(zap_channel_t *zchan)
{
	zap_channel_state_t state = zchan->state;

	if (state == ZAP_CHANNEL_STATE_PROGRESS) {
		zap_set_flag(zchan, ZAP_CHANNEL_PROGRESS);
	} else if (state == ZAP_CHANNEL_STATE_UP) {
		zap_set_flag(zchan, ZAP_CHANNEL_PROGRESS);
		zap_set_flag(zchan, ZAP_CHANNEL_MEDIA);	
		zap_set_flag(zchan, ZAP_CHANNEL_ANSWERED);	
	} else if (state == ZAP_CHANNEL_STATE_PROGRESS_MEDIA) {
		zap_set_flag(zchan, ZAP_CHANNEL_PROGRESS);	
		zap_set_flag(zchan, ZAP_CHANNEL_MEDIA);	
	}

	return ZAP_SUCCESS;
}

static int zap_parse_state_map(zap_channel_t *zchan, zap_channel_state_t state, zap_state_map_t *state_map)
{
	int x = 0, ok = 0;
	zap_state_direction_t direction = zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND) ? ZSD_OUTBOUND : ZSD_INBOUND;

	for(x = 0; x < ZAP_MAP_NODE_SIZE; x++) {
		int i = 0, proceed = 0;
		if (!state_map->nodes[x].type) {
			break;
		}

		if (state_map->nodes[x].direction != direction) {
			continue;
		}
		
		if (state_map->nodes[x].check_states[0] == ZAP_ANY_STATE) {
			proceed = 1;
		} else {
			for(i = 0; i < ZAP_MAP_MAX; i++) {
				if (state_map->nodes[x].check_states[i] == zchan->state) {
					proceed = 1;
					break;
				}
			}
		}

		if (!proceed) {
			continue;
		}
		
		for(i = 0; i < ZAP_MAP_MAX; i++) {
			ok = (state_map->nodes[x].type == ZSM_ACCEPTABLE);
			if (state_map->nodes[x].states[i] == ZAP_END) {
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

OZ_DECLARE(zap_status_t) zap_channel_set_state(zap_channel_t *zchan, zap_channel_state_t state, int lock)
{
	int ok = 1;
	
	if (!zap_test_flag(zchan, ZAP_CHANNEL_READY)) {
		zap_log(ZAP_LOG_ERROR, "%d:%d Cannot set state in channel that is not ready\n",
				zchan->span_id, zchan->chan_id);
		return ZAP_FAIL;
	}

	if (zap_test_flag(zchan->span, ZAP_SPAN_SUSPENDED)) {
		if (state != ZAP_CHANNEL_STATE_RESTART && state != ZAP_CHANNEL_STATE_DOWN) {
			zap_log(ZAP_LOG_ERROR, "%d:%d Cannot set state in channel that is suspended\n",
				zchan->span_id, zchan->chan_id);
			return ZAP_FAIL;
		}
	}

	if (lock) {
		zap_mutex_lock(zchan->mutex);
	}

	if (zchan->span->state_map) {
		ok = zap_parse_state_map(zchan, state, zchan->span->state_map);
		goto end;
	}

	switch(zchan->state) {
	case ZAP_CHANNEL_STATE_HANGUP:
	case ZAP_CHANNEL_STATE_TERMINATING:
		{
			ok = 0;
			switch(state) {
			case ZAP_CHANNEL_STATE_DOWN:
			case ZAP_CHANNEL_STATE_BUSY:
			case ZAP_CHANNEL_STATE_RESTART:
				ok = 1;
				break;
			default:
				break;
			}
		}
		break;
	case ZAP_CHANNEL_STATE_UP:
		{
			ok = 1;
			switch(state) {
			case ZAP_CHANNEL_STATE_PROGRESS:
			case ZAP_CHANNEL_STATE_PROGRESS_MEDIA:
			case ZAP_CHANNEL_STATE_RING:
				ok = 0;
				break;
			default:
				break;
			}
		}
		break;
	case ZAP_CHANNEL_STATE_DOWN:
		{
			ok = 0;
			
			switch(state) {
			case ZAP_CHANNEL_STATE_DIALTONE:
			case ZAP_CHANNEL_STATE_COLLECT:
			case ZAP_CHANNEL_STATE_DIALING:
			case ZAP_CHANNEL_STATE_RING:
			case ZAP_CHANNEL_STATE_PROGRESS_MEDIA:
			case ZAP_CHANNEL_STATE_PROGRESS:				
			case ZAP_CHANNEL_STATE_IDLE:				
			case ZAP_CHANNEL_STATE_GET_CALLERID:
			case ZAP_CHANNEL_STATE_GENRING:
				ok = 1;
				break;
			default:
				break;
			}
		}
		break;
	case ZAP_CHANNEL_STATE_BUSY:
		{
			switch(state) {
			case ZAP_CHANNEL_STATE_UP:
				ok = 0;
				break;
			default:
				break;
			}
		}
		break;
	case ZAP_CHANNEL_STATE_RING:
		{
			switch(state) {
			case ZAP_CHANNEL_STATE_UP:
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

	if (state == zchan->state) {
		ok = 0;
	}
	

	if (ok) {
		zap_set_flag(zchan, ZAP_CHANNEL_STATE_CHANGE);	
		zap_set_flag_locked(zchan->span, ZAP_SPAN_STATE_CHANGE);	
		zchan->last_state = zchan->state; 
		zchan->state = state;
	}

	if (lock) {
		zap_mutex_unlock(zchan->mutex);
	}

	return ok ? ZAP_SUCCESS : ZAP_FAIL;
}

OZ_DECLARE(zap_status_t) zap_span_channel_use_count(zap_span_t *span, uint32_t *count)
{
	uint32_t j;

	*count = 0;
	
	if (!span || !zap_test_flag(span, ZAP_SPAN_CONFIGURED)) {
		return ZAP_FAIL;
	}
	
	for(j = 1; j <= span->chan_count && span->channels[j]; j++) {
		if (span->channels[j]) {
			if (zap_test_flag(span->channels[j], ZAP_CHANNEL_INUSE)) {
				(*count)++;
			}
		}
	}
	
	return ZAP_SUCCESS;
}

OZ_DECLARE(zap_status_t) zap_channel_open_any(uint32_t span_id, zap_direction_t direction, zap_caller_data_t *caller_data, zap_channel_t **zchan)
{
	zap_status_t status = ZAP_FAIL;
	zap_channel_t *check;
	uint32_t i, j, count;
	zap_span_t *span = NULL;
	uint32_t span_max;

	if (span_id) {
		zap_span_find(span_id, &span);

		if (!span || !zap_test_flag(span, ZAP_SPAN_CONFIGURED)) {
			zap_log(ZAP_LOG_CRIT, "SPAN NOT DEFINED!\n");
			*zchan = NULL;
            return ZAP_FAIL;
		}

		zap_span_channel_use_count(span, &count);

		if (count >= span->chan_count) {
			zap_log(ZAP_LOG_CRIT, "All circuits are busy.\n");
			*zchan = NULL;
			return ZAP_FAIL;
		}

		if (span->channel_request && !span->suggest_chan_id) {
			zap_set_caller_data(span, caller_data);
			return span->channel_request(span, 0, direction, caller_data, zchan);
		}
		
		span_max = span_id;
		j = span_id;
	} else {
		zap_log(ZAP_LOG_CRIT, "No span supplied\n");
		*zchan = NULL;
		return ZAP_FAIL;
	}
	
	zap_mutex_lock(span->mutex);
	
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
			
		if (!(check = span->channels[i])) {
			status = ZAP_FAIL;
			break;
		}
			
		if (zap_test_flag(check, ZAP_CHANNEL_READY) && 
			!zap_test_flag(check, ZAP_CHANNEL_INUSE) && 
			!zap_test_flag(check, ZAP_CHANNEL_SUSPENDED) && 
			check->state == ZAP_CHANNEL_STATE_DOWN && 
			check->type != ZAP_CHAN_TYPE_DQ921 &&
			check->type != ZAP_CHAN_TYPE_DQ931
			
			) {

			if (span && span->channel_request) {
				zap_set_caller_data(span, caller_data);
				status = span->channel_request(span, i, direction, caller_data, zchan);
				break;
			}

			status = check->zio->open(check);
				
			if (status == ZAP_SUCCESS) {
				zap_set_flag(check, ZAP_CHANNEL_INUSE);
				zap_channel_open_chan(check);
				*zchan = check;
				break;
			}
		}
		
		if (direction == ZAP_TOP_DOWN) {
			i++;
		} else {
			i--;
		}
	}

	zap_mutex_unlock(span->mutex);

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

	if (zchan->dtmf_buffer) {
		zap_buffer_zero(zchan->dtmf_buffer);
	}

	if (zchan->gen_dtmf_buffer) {
		zap_buffer_zero(zchan->gen_dtmf_buffer);
	}

	if (zchan->digit_buffer) {
		zap_buffer_zero(zchan->digit_buffer);
	}

	if (!zchan->dtmf_on) {
		zchan->dtmf_on = ZAP_DEFAULT_DTMF_ON;
	}

	if (!zchan->dtmf_off) {
		zchan->dtmf_off = ZAP_DEFAULT_DTMF_OFF;
	}
	
	memset(zchan->dtmf_hangup_buf, '\0', zchan->span->dtmf_hangup_len);

	if (zap_test_flag(zchan, ZAP_CHANNEL_TRANSCODE)) {
		zchan->effective_codec = zchan->native_codec;
		zchan->packet_len = zchan->native_interval * (zchan->effective_codec == ZAP_CODEC_SLIN ? 16 : 8);
		zap_clear_flag(zchan, ZAP_CHANNEL_TRANSCODE);
	}

	return ZAP_SUCCESS;
}

OZ_DECLARE(zap_status_t) zap_channel_init(zap_channel_t *zchan)
{

	if (zchan->init_state != ZAP_CHANNEL_STATE_DOWN) {
		zap_set_state_locked(zchan, zchan->init_state);
		zchan->init_state = ZAP_CHANNEL_STATE_DOWN;
	}

	return ZAP_SUCCESS;
}

OZ_DECLARE(zap_status_t) zap_channel_open_chan(zap_channel_t *zchan)
{
	zap_status_t status = ZAP_FAIL;

	assert(zchan != NULL);

	if (zap_test_flag(zchan, ZAP_CHANNEL_SUSPENDED)) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "%s", "Channel is suspended");
		return ZAP_FAIL;
	}
	if (globals.cpu_monitor.alarm &&
			globals.cpu_monitor.alarm_action_flags & ZAP_CPU_ALARM_ACTION_REJECT) {

		snprintf(zchan->last_error, sizeof(zchan->last_error), "%s", "CPU usage alarm is on - refusing to open channel\n");
		zap_log(ZAP_LOG_WARNING, "CPU usage alarm is on - refusing to open channel\n");
		zchan->caller_data.hangup_cause = ZAP_CAUSE_SWITCH_CONGESTION;
		return ZAP_FAIL;
	}
	
	if (!zap_test_flag(zchan, ZAP_CHANNEL_READY) || (status = zap_mutex_trylock(zchan->mutex)) != ZAP_SUCCESS) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "Channel is not ready or is in use %d %d", zap_test_flag(zchan, ZAP_CHANNEL_READY), status);
		return status;
	}

	status = ZAP_FAIL;

	if (zap_test_flag(zchan, ZAP_CHANNEL_READY)) {
		status = zchan->span->zio->open(zchan);
		if (status == ZAP_SUCCESS) {
			zap_set_flag(zchan, ZAP_CHANNEL_OPEN | ZAP_CHANNEL_INUSE);
		}
	} else {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "%s", "Channel is not ready");
	}

	zap_mutex_unlock(zchan->mutex);
	return status;
}

OZ_DECLARE(zap_status_t) zap_channel_open(uint32_t span_id, uint32_t chan_id, zap_channel_t **zchan)
{
	zap_channel_t *check;
	zap_status_t status = ZAP_FAIL;
	zap_span_t *span = NULL;

	zap_mutex_lock(globals.mutex);
	zap_span_find(span_id, &span);

	if (!span || !zap_test_flag(span, ZAP_SPAN_CONFIGURED) || chan_id >= ZAP_MAX_CHANNELS_SPAN) {
		zap_log(ZAP_LOG_CRIT, "SPAN NOT DEFINED!\n");
		*zchan = NULL;
		goto done;
	}

	if (span->channel_request) {
		zap_log(ZAP_LOG_ERROR, "Individual channel selection not implemented on this span.\n");
		*zchan = NULL;
		goto done;
	}
	
	if (!(check = span->channels[chan_id])) {
		zap_log(ZAP_LOG_ERROR, "Invalid Channel %d\n", chan_id);
		*zchan = NULL;
		goto done;
	}

	if (zap_test_flag(check, ZAP_CHANNEL_SUSPENDED) || 
		!zap_test_flag(check, ZAP_CHANNEL_READY) || (status = zap_mutex_trylock(check->mutex)) != ZAP_SUCCESS) {
		*zchan = NULL;
		goto done;
	}

	status = ZAP_FAIL;	
	if ((!zap_test_flag(check, ZAP_CHANNEL_INUSE)) || 
	    (check->type == ZAP_CHAN_TYPE_FXS && 
	     check->token_count == 1 &&
	     zap_channel_test_feature(check, ZAP_CHANNEL_FEATURE_CALLWAITING))) {
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

	done:
	zap_mutex_unlock(globals.mutex);

	return status;
}

OZ_DECLARE(zap_status_t) zap_channel_outgoing_call(zap_channel_t *zchan)
{
	zap_status_t status;

	assert(zchan != NULL);
	
	if (zchan->span->outgoing_call) {
		if ((status = zchan->span->outgoing_call(zchan)) == ZAP_SUCCESS) {
			zap_set_flag(zchan, ZAP_CHANNEL_OUTBOUND);
		}
		return status;
	} else {
		zap_log(ZAP_LOG_ERROR, "outgoing_call method not implemented!\n");
	}
	
	return ZAP_FAIL;
}

#ifdef ZAP_DEBUG_DTMF
static void close_dtmf_debug(zap_channel_t *zchan)
{
	zap_mutex_lock(zchan->dtmfdbg.mutex);

	if (zchan->dtmfdbg.file) {
		zap_log_chan_msg(zchan, ZAP_LOG_DEBUG, "closing debug dtmf file\n");
		fclose(zchan->dtmfdbg.file);
		zchan->dtmfdbg.file = NULL;
	}
	zchan->dtmfdbg.windex = 0;
	zchan->dtmfdbg.wrapped = 0;

	zap_mutex_unlock(zchan->dtmfdbg.mutex);
}
#endif

OZ_DECLARE(zap_status_t) zap_channel_done(zap_channel_t *zchan)
{
	assert(zchan != NULL);

	memset(&zchan->caller_data, 0, sizeof(zchan->caller_data));

	zap_clear_flag_locked(zchan, ZAP_CHANNEL_INUSE);
	zap_clear_flag_locked(zchan, ZAP_CHANNEL_OUTBOUND);
	zap_clear_flag_locked(zchan, ZAP_CHANNEL_WINK);
	zap_clear_flag_locked(zchan, ZAP_CHANNEL_FLASH);
	zap_clear_flag_locked(zchan, ZAP_CHANNEL_STATE_CHANGE);
	zap_clear_flag_locked(zchan, ZAP_CHANNEL_HOLD);
	zap_clear_flag_locked(zchan, ZAP_CHANNEL_OFFHOOK);
	zap_clear_flag_locked(zchan, ZAP_CHANNEL_RINGING);
	zap_clear_flag_locked(zchan, ZAP_CHANNEL_PROGRESS_DETECT);
	zap_clear_flag_locked(zchan, ZAP_CHANNEL_CALLERID_DETECT);
	zap_clear_flag_locked(zchan, ZAP_CHANNEL_3WAY);
	zap_clear_flag_locked(zchan, ZAP_CHANNEL_PROGRESS);
	zap_clear_flag_locked(zchan, ZAP_CHANNEL_MEDIA);
	zap_clear_flag_locked(zchan, ZAP_CHANNEL_ANSWERED);
	zap_mutex_lock(zchan->pre_buffer_mutex);
	zap_buffer_destroy(&zchan->pre_buffer);
	zchan->pre_buffer_size = 0;
	zap_mutex_unlock(zchan->pre_buffer_mutex);
#ifdef ZAP_DEBUG_DTMF
	close_dtmf_debug(zchan);
#endif

	zap_channel_flush_dtmf(zchan);

	zchan->init_state = ZAP_CHANNEL_STATE_DOWN;
	zchan->state = ZAP_CHANNEL_STATE_DOWN;
	zap_log(ZAP_LOG_DEBUG, "channel done %u:%u\n", zchan->span_id, zchan->chan_id);

	return ZAP_SUCCESS;
}

OZ_DECLARE(zap_status_t) zap_channel_use(zap_channel_t *zchan)
{

	assert(zchan != NULL);

	zap_set_flag_locked(zchan, ZAP_CHANNEL_INUSE);

	return ZAP_SUCCESS;
}

OZ_DECLARE(zap_status_t) zap_channel_close(zap_channel_t **zchan)
{
	zap_channel_t *check;
	zap_status_t status = ZAP_FAIL;

	assert(zchan != NULL);
	check = *zchan;
	*zchan = NULL;

	if (!check) {
		return ZAP_FAIL;
	}

	if (zap_test_flag(check, ZAP_CHANNEL_CONFIGURED)) {
		zap_mutex_lock(check->mutex);
		if (zap_test_flag(check, ZAP_CHANNEL_OPEN)) {
			status = check->zio->close(check);
			if (status == ZAP_SUCCESS) {
				zap_channel_reset(check);
				*zchan = NULL;
			}
		}
		check->ring_count = 0;
		zap_mutex_unlock(check->mutex);
	}
	
	return status;
}


static zap_status_t zchan_activate_dtmf_buffer(zap_channel_t *zchan)
{

	if (!zchan->dtmf_buffer) {
		if (zap_buffer_create(&zchan->dtmf_buffer, 1024, 3192, 0) != ZAP_SUCCESS) {
			zap_log(ZAP_LOG_ERROR, "Failed to allocate DTMF Buffer!\n");
			snprintf(zchan->last_error, sizeof(zchan->last_error), "buffer error");
			return ZAP_FAIL;
		} else {
			zap_log(ZAP_LOG_DEBUG, "Created DTMF Buffer!\n");
		}
	}

	
	if (!zchan->tone_session.buffer) {
		memset(&zchan->tone_session, 0, sizeof(zchan->tone_session));
		teletone_init_session(&zchan->tone_session, 0, NULL, NULL);
	}

	zchan->tone_session.rate = zchan->rate;
	zchan->tone_session.duration = zchan->dtmf_on * (zchan->tone_session.rate / 1000);
	zchan->tone_session.wait = zchan->dtmf_off * (zchan->tone_session.rate / 1000);
	zchan->tone_session.volume = -7;

	/*
	  zchan->tone_session.debug = 1;
	  zchan->tone_session.debug_stream = stdout;
	*/

	return ZAP_SUCCESS;
}

OZ_DECLARE(zap_status_t) zap_channel_command(zap_channel_t *zchan, zap_command_t command, void *obj)
{
	zap_status_t status = ZAP_FAIL;
	
	assert(zchan != NULL);
	assert(zchan->zio != NULL);

	zap_mutex_lock(zchan->mutex);

	switch(command) {

	case ZAP_COMMAND_ENABLE_CALLERID_DETECT:
		{
			if (!zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_CALLERID)) {
				if (zap_fsk_demod_init(&zchan->fsk, zchan->rate, zchan->fsk_buf, sizeof(zchan->fsk_buf)) != ZAP_SUCCESS) {
					snprintf(zchan->last_error, sizeof(zchan->last_error), "%s", strerror(errno));
					GOTO_STATUS(done, ZAP_FAIL);
				}
				zap_set_flag_locked(zchan, ZAP_CHANNEL_CALLERID_DETECT);
			}
		}
		break;
	case ZAP_COMMAND_DISABLE_CALLERID_DETECT:
		{
			if (!zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_CALLERID)) {
				zap_fsk_demod_destroy(&zchan->fsk);
				zap_clear_flag_locked(zchan, ZAP_CHANNEL_CALLERID_DETECT);
			}
		}
		break;
	case ZAP_COMMAND_TRACE_INPUT:
		{
			char *path = (char *) obj;
			if (zchan->fds[0] > 0) {
				close(zchan->fds[0]);
				zchan->fds[0] = -1;
			}
			if ((zchan->fds[0] = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) > -1) {
				zap_log(ZAP_LOG_DEBUG, "Tracing channel %u:%u to [%s]\n", zchan->span_id, zchan->chan_id, path);	
				GOTO_STATUS(done, ZAP_SUCCESS);
			}
			
			snprintf(zchan->last_error, sizeof(zchan->last_error), "%s", strerror(errno));
			GOTO_STATUS(done, ZAP_FAIL);
		}
		break;
	case ZAP_COMMAND_TRACE_OUTPUT:
		{
			char *path = (char *) obj;
			if (zchan->fds[1] > 0) {
				close(zchan->fds[1]);
				zchan->fds[1] = -1;
			}
			if ((zchan->fds[1] = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) > -1) {
				zap_log(ZAP_LOG_DEBUG, "Tracing channel %u:%u to [%s]\n", zchan->span_id, zchan->chan_id, path);	
				GOTO_STATUS(done, ZAP_SUCCESS);
			}
			
			snprintf(zchan->last_error, sizeof(zchan->last_error), "%s", strerror(errno));
			GOTO_STATUS(done, ZAP_FAIL);
		}
		break;
	case ZAP_COMMAND_TRACE_END_ALL:
		{
			if (zchan->fds[0] > 0) {
				close(zchan->fds[0]);
				zchan->fds[0] = -1;
			}
			if (zchan->fds[1] > 0) {
				close(zchan->fds[1]);
				zchan->fds[1] = -1;
			}
			GOTO_STATUS(done, ZAP_SUCCESS);
		}
		break;
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
			if (!zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_PROGRESS)) {
				/* if they don't have thier own, use ours */
				zap_channel_clear_detected_tones(zchan);
				zap_channel_clear_needed_tones(zchan);
				teletone_multi_tone_init(&zchan->span->tone_finder[ZAP_TONEMAP_DIAL], &zchan->span->tone_detect_map[ZAP_TONEMAP_DIAL]);
				teletone_multi_tone_init(&zchan->span->tone_finder[ZAP_TONEMAP_RING], &zchan->span->tone_detect_map[ZAP_TONEMAP_RING]);
				teletone_multi_tone_init(&zchan->span->tone_finder[ZAP_TONEMAP_BUSY], &zchan->span->tone_detect_map[ZAP_TONEMAP_BUSY]);
				zap_set_flag(zchan, ZAP_CHANNEL_PROGRESS_DETECT);
				GOTO_STATUS(done, ZAP_SUCCESS);
			}
		}
		break;
	case ZAP_COMMAND_DISABLE_PROGRESS_DETECT:
		{
			if (!zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_PROGRESS)) {
				zap_clear_flag_locked(zchan, ZAP_CHANNEL_PROGRESS_DETECT);
				zap_channel_clear_detected_tones(zchan);
				zap_channel_clear_needed_tones(zchan);
				GOTO_STATUS(done, ZAP_SUCCESS);
			}
		}
		break;
	case ZAP_COMMAND_ENABLE_DTMF_DETECT:
		{
			/* if they don't have thier own, use ours */
			if (!zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_DTMF_DETECT)) {
				teletone_dtmf_detect_init (&zchan->dtmf_detect, zchan->rate);
				zap_set_flag_locked(zchan, ZAP_CHANNEL_DTMF_DETECT);
				zap_set_flag_locked(zchan, ZAP_CHANNEL_SUPRESS_DTMF);
				GOTO_STATUS(done, ZAP_SUCCESS);
			}
		}
		break;
	case ZAP_COMMAND_DISABLE_DTMF_DETECT:
		{
			if (!zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_DTMF_DETECT)) {
                    		teletone_dtmf_detect_init (&zchan->dtmf_detect, zchan->rate);
                    		zap_clear_flag(zchan, ZAP_CHANNEL_DTMF_DETECT);
				zap_clear_flag(zchan, ZAP_CHANNEL_SUPRESS_DTMF);
				GOTO_STATUS(done, ZAP_SUCCESS);
			}
		}
		break;
	case ZAP_COMMAND_SET_PRE_BUFFER_SIZE:
		{
			int val = ZAP_COMMAND_OBJ_INT;

			if (val < 0) {
				val = 0;
			}

			zchan->pre_buffer_size = val * 8;

			zap_mutex_lock(zchan->pre_buffer_mutex);
			if (!zchan->pre_buffer_size) {
				zap_buffer_destroy(&zchan->pre_buffer);
			} else if (!zchan->pre_buffer) {
				zap_buffer_create(&zchan->pre_buffer, 1024, zchan->pre_buffer_size, 0);
			}
			zap_mutex_unlock(zchan->pre_buffer_mutex);

			GOTO_STATUS(done, ZAP_SUCCESS);

		}
		break;
	case ZAP_COMMAND_GET_DTMF_ON_PERIOD:
		{
			if (!zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_DTMF_GENERATE)) {
				ZAP_COMMAND_OBJ_INT = zchan->dtmf_on;
				GOTO_STATUS(done, ZAP_SUCCESS);
			}
		}
		break;
	case ZAP_COMMAND_GET_DTMF_OFF_PERIOD:
		{
			if (!zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_DTMF_GENERATE)) {
				ZAP_COMMAND_OBJ_INT = zchan->dtmf_on;
				GOTO_STATUS(done, ZAP_SUCCESS);
			}
		}
		break;
	case ZAP_COMMAND_SET_DTMF_ON_PERIOD:
		{
			if (!zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_DTMF_GENERATE)) {
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
			if (!zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_DTMF_GENERATE)) {
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
			if (!zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_DTMF_GENERATE)) {
				char *digits = ZAP_COMMAND_OBJ_CHAR_P;
				
				if ((status = zchan_activate_dtmf_buffer(zchan)) != ZAP_SUCCESS) {
					GOTO_STATUS(done, status);
				}
				
				zap_buffer_write(zchan->gen_dtmf_buffer, digits, strlen(digits));
				
				GOTO_STATUS(done, ZAP_SUCCESS);
			}
		}
		break;

	case ZAP_COMMAND_DISABLE_ECHOCANCEL:
		{
			zap_mutex_lock(zchan->pre_buffer_mutex);
			zap_buffer_destroy(&zchan->pre_buffer);
			zchan->pre_buffer_size = 0;
			zap_mutex_unlock(zchan->pre_buffer_mutex);
		}
		break;

	case ZAP_COMMAND_SET_RX_GAIN:
		{
			zchan->rxgain = ZAP_COMMAND_OBJ_FLOAT;
			reset_gain_table(zchan->rxgain_table, zchan->rxgain, zchan->native_codec);
			if (zchan->rxgain == 0.0) {
				zap_clear_flag(zchan, ZAP_CHANNEL_USE_RX_GAIN);
			} else {
				zap_set_flag(zchan, ZAP_CHANNEL_USE_RX_GAIN);
			}
			GOTO_STATUS(done, ZAP_SUCCESS);
		}
		break;
	case ZAP_COMMAND_GET_RX_GAIN:
		{
			ZAP_COMMAND_OBJ_FLOAT = zchan->rxgain;
			GOTO_STATUS(done, ZAP_SUCCESS);
		}
		break;
	case ZAP_COMMAND_SET_TX_GAIN:
		{
			zchan->txgain = ZAP_COMMAND_OBJ_FLOAT;
			reset_gain_table(zchan->txgain_table, zchan->txgain, zchan->native_codec);
			if (zchan->txgain == 0.0) {
				zap_clear_flag(zchan, ZAP_CHANNEL_USE_TX_GAIN);
			} else {
				zap_set_flag(zchan, ZAP_CHANNEL_USE_TX_GAIN);
			}
			GOTO_STATUS(done, ZAP_SUCCESS);
		}
		break;
	case ZAP_COMMAND_GET_TX_GAIN:
		{
			ZAP_COMMAND_OBJ_FLOAT = zchan->txgain;
			GOTO_STATUS(done, ZAP_SUCCESS);
		}
		break;
	default:
		break;
	}

	if (!zchan->zio->command) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "method not implemented");
		zap_log(ZAP_LOG_ERROR, "no command function defined by the I/O openzap module!\n");	
		GOTO_STATUS(done, ZAP_FAIL);
	}

    status = zchan->zio->command(zchan, command, obj);

	if (status == ZAP_NOTIMPL) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "I/O command %d not implemented in backend", command);
		zap_log(ZAP_LOG_ERROR, "I/O backend does not support command %d!\n", command);	
	}
done:
	zap_mutex_unlock(zchan->mutex);
	return status;

}

OZ_DECLARE(zap_status_t) zap_channel_wait(zap_channel_t *zchan, zap_wait_flag_t *flags, int32_t to)
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

OZ_DECLARE(void) zap_channel_clear_detected_tones(zap_channel_t *zchan)
{
	uint32_t i;

	memset(zchan->detected_tones, 0, sizeof(zchan->detected_tones[0]) * ZAP_TONEMAP_INVALID);
	
	for (i = 1; i < ZAP_TONEMAP_INVALID; i++) {
		zchan->span->tone_finder[i].tone_count = 0;
	}
}

OZ_DECLARE(void) zap_channel_clear_needed_tones(zap_channel_t *zchan)
{
	memset(zchan->needed_tones, 0, sizeof(zchan->needed_tones[0]) * ZAP_TONEMAP_INVALID);
}

OZ_DECLARE(zap_size_t) zap_channel_dequeue_dtmf(zap_channel_t *zchan, char *dtmf, zap_size_t len)
{
	zap_size_t bytes = 0;

	assert(zchan != NULL);

	if (!zap_test_flag(zchan, ZAP_CHANNEL_READY)) {
		return ZAP_FAIL;
	}

	if (zchan->digit_buffer && zap_buffer_inuse(zchan->digit_buffer)) {
		zap_mutex_lock(zchan->mutex);
		if ((bytes = zap_buffer_read(zchan->digit_buffer, dtmf, len)) > 0) {
			*(dtmf + bytes) = '\0';
		}
		zap_mutex_unlock(zchan->mutex);
	}

	return bytes;
}

OZ_DECLARE(void) zap_channel_flush_dtmf(zap_channel_t *zchan)
{
	if (zchan->digit_buffer && zap_buffer_inuse(zchan->digit_buffer)) {
		zap_mutex_lock(zchan->mutex);
		zap_buffer_zero(zchan->digit_buffer);
		zap_mutex_unlock(zchan->mutex);
	}
}

OZ_DECLARE(zap_status_t) zap_channel_queue_dtmf(zap_channel_t *zchan, const char *dtmf)
{
	zap_status_t status;
	register zap_size_t len, inuse;
	zap_size_t wr = 0;
	const char *p;
	
	assert(zchan != NULL);

	zap_log_chan(zchan, ZAP_LOG_DEBUG, "Queuing DTMF %s\n", dtmf);

#ifdef ZAP_DEBUG_DTMF
	zap_mutex_lock(zchan->dtmfdbg.mutex);
	if (!zchan->dtmfdbg.file) {
		struct tm currtime;
		time_t currsec;
		char dfile[512];

		currsec = time(NULL);
		localtime_r(&currsec, &currtime);

		snprintf(dfile, sizeof(dfile), "dtmf-s%dc%d-20%d-%d-%d-%d:%d:%d.%s", 
				zchan->span_id, zchan->chan_id, 
				currtime.tm_year-100, currtime.tm_mon+1, currtime.tm_mday,
				currtime.tm_hour, currtime.tm_min, currtime.tm_sec, zchan->native_codec == ZAP_CODEC_ULAW ? "ulaw" : zchan->native_codec == ZAP_CODEC_ALAW ? "alaw" : "sln");
		zchan->dtmfdbg.file = fopen(dfile, "w");
		if (!zchan->dtmfdbg.file) {
			zap_log_chan(zchan, ZAP_LOG_ERROR, "failed to open debug dtmf file %s\n", dfile);
		} else {
			/* write the saved audio buffer */
			int rc = 0;
			int towrite = sizeof(zchan->dtmfdbg.buffer) - zchan->dtmfdbg.windex;
		
			zap_log_chan(zchan, ZAP_LOG_DEBUG, "created debug DTMF file %s\n", dfile);
			zchan->dtmfdbg.closetimeout = DTMF_DEBUG_TIMEOUT;
			if (zchan->dtmfdbg.wrapped) {
				rc = fwrite(&zchan->dtmfdbg.buffer[zchan->dtmfdbg.windex], 1, towrite, zchan->dtmfdbg.file);
				if (rc != towrite) {
					zap_log_chan(zchan, ZAP_LOG_ERROR, "only wrote %d out of %d bytes in DTMF debug buffer\n", rc, towrite);
				}
			}
			if (zchan->dtmfdbg.windex) {
				towrite = zchan->dtmfdbg.windex;
				rc = fwrite(&zchan->dtmfdbg.buffer[0], 1, towrite, zchan->dtmfdbg.file);
				if (rc != towrite) {
					zap_log_chan(zchan, ZAP_LOG_ERROR, "only wrote %d out of %d bytes in DTMF debug buffer\n", rc, towrite);
				}
			}
			zchan->dtmfdbg.windex = 0;
			zchan->dtmfdbg.wrapped = 0;
		}
	} else {
			zchan->dtmfdbg.closetimeout = DTMF_DEBUG_TIMEOUT;
	}
	zap_mutex_unlock(zchan->dtmfdbg.mutex);
#endif

	if (zchan->pre_buffer) {
		zap_buffer_zero(zchan->pre_buffer);
	}

	zap_mutex_lock(zchan->mutex);

	inuse = zap_buffer_inuse(zchan->digit_buffer);
	len = strlen(dtmf);
	
	if (len + inuse > zap_buffer_len(zchan->digit_buffer)) {
		zap_buffer_toss(zchan->digit_buffer, strlen(dtmf));
	}

	if (zchan->span->dtmf_hangup_len) {
		for (p = dtmf; zap_is_dtmf(*p); p++) {
			memmove (zchan->dtmf_hangup_buf, zchan->dtmf_hangup_buf + 1, zchan->span->dtmf_hangup_len - 1);
			zchan->dtmf_hangup_buf[zchan->span->dtmf_hangup_len - 1] = *p;
			if (!strcmp(zchan->dtmf_hangup_buf, zchan->span->dtmf_hangup)) {
				zap_log(ZAP_LOG_DEBUG, "DTMF hangup detected.\n");
				zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
				break;
			}
		}
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


static zap_status_t handle_dtmf(zap_channel_t *zchan, zap_size_t datalen)
{
	zap_buffer_t *buffer = NULL;
	zap_size_t dblen = 0;
	int wrote = 0;

	if (zchan->gen_dtmf_buffer && (dblen = zap_buffer_inuse(zchan->gen_dtmf_buffer))) {
		char digits[128] = "";
		char *cur;
		int x = 0;				 
		
		if (dblen > sizeof(digits) - 1) {
			dblen = sizeof(digits) - 1;
		}

		if (zap_buffer_read(zchan->gen_dtmf_buffer, digits, dblen) && !zap_strlen_zero_buf(digits)) {
			zap_log(ZAP_LOG_DEBUG, "%d:%d GENERATE DTMF [%s]\n", zchan->span_id, zchan->chan_id, digits);	
		
			cur = digits;

			if (*cur == 'F') {
				zap_channel_command(zchan, ZAP_COMMAND_FLASH, NULL);
				cur++;
			}

			for (; *cur; cur++) {
				if ((wrote = teletone_mux_tones(&zchan->tone_session, &zchan->tone_session.TONES[(int)*cur]))) {
					zap_buffer_write(zchan->dtmf_buffer, zchan->tone_session.buffer, wrote * 2);
					x++;
				} else {
					zap_log(ZAP_LOG_ERROR, "%d:%d Problem Adding DTMF SEQ [%s]\n", zchan->span_id, zchan->chan_id, digits);
					return ZAP_FAIL;
				}
			}

			if (x) {
				zchan->skip_read_frames = (wrote / (zchan->effective_interval * 8)) + 4;
			}
		}
	}
	

	if (!zchan->buffer_delay || --zchan->buffer_delay == 0) {
		if (zchan->dtmf_buffer && (dblen = zap_buffer_inuse(zchan->dtmf_buffer))) {
			buffer = zchan->dtmf_buffer;
		} else if (zchan->fsk_buffer && (dblen = zap_buffer_inuse(zchan->fsk_buffer))) {
			buffer = zchan->fsk_buffer;			
		}
	}

	if (buffer) {
		zap_size_t dlen = datalen;
		uint8_t auxbuf[1024];
		zap_size_t len, br, max = sizeof(auxbuf);
		
		if (zchan->native_codec != ZAP_CODEC_SLIN) {
			dlen *= 2;
		}
		
		len = dblen > dlen ? dlen : dblen;

		br = zap_buffer_read(buffer, auxbuf, len);		
		if (br < dlen) {
			memset(auxbuf + br, 0, dlen - br);
		}

		if (zchan->native_codec != ZAP_CODEC_SLIN) {
			if (zchan->native_codec == ZAP_CODEC_ULAW) {
				zio_slin2ulaw(auxbuf, max, &dlen);
			} else if (zchan->native_codec == ZAP_CODEC_ALAW) {
				zio_slin2alaw(auxbuf, max, &dlen);
			}
		}
		
		return zchan->zio->write(zchan, auxbuf, &dlen);
	} 

	return ZAP_SUCCESS;

}


OZ_DECLARE(void) zap_generate_sln_silence(int16_t *data, uint32_t samples, uint32_t divisor)
{
    int16_t x;
    uint32_t i;
    int sum_rnd = 0;
    int16_t rnd2 = (int16_t) zap_current_time_in_ms() * (int16_t) (intptr_t) data;

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



OZ_DECLARE(zap_status_t) zap_channel_read(zap_channel_t *zchan, void *data, zap_size_t *datalen)
{
	zap_status_t status = ZAP_FAIL;
	zio_codec_t codec_func = NULL;
	zap_size_t max = *datalen;
	unsigned i = 0;

	assert(zchan != NULL);
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
	if (zchan->fds[0] > -1) {
		int dlen = (int) *datalen;
		if (write(zchan->fds[0], data, dlen) != dlen) {
			snprintf(zchan->last_error, sizeof(zchan->last_error), "file write error!");
			return ZAP_FAIL;
		}
	}

#ifdef ZAP_DEBUG_DTMF
	if (status == ZAP_SUCCESS) {
		int dlen = (int) *datalen;
		int rc = 0;
		zap_mutex_lock(zchan->dtmfdbg.mutex);
		if (!zchan->dtmfdbg.file) {
			/* no file yet, write to our circular buffer */
			int windex = zchan->dtmfdbg.windex;
			int avail = sizeof(zchan->dtmfdbg.buffer) - windex;
			char *dataptr = data;

			if (dlen > avail) {
				int diff = dlen - avail;
				/* write only what we can and the rest at the beginning of the buffer */
				memcpy(&zchan->dtmfdbg.buffer[windex], dataptr, avail);
				memcpy(&zchan->dtmfdbg.buffer[0], &dataptr[avail], diff);
				windex = diff;
				/*zap_log_chan(zchan, ZAP_LOG_DEBUG, "wrapping around dtmf read buffer up to index %d\n\n", windex);*/
				zchan->dtmfdbg.wrapped = 1;
			} else {
				memcpy(&zchan->dtmfdbg.buffer[windex], dataptr, dlen);
				windex += dlen;
			}
			if (windex == sizeof(zchan->dtmfdbg.buffer)) {
				/*zap_log_chan_msg(zchan, ZAP_LOG_DEBUG, "wrapping around dtmf read buffer\n");*/
				windex = 0;
				zchan->dtmfdbg.wrapped = 1;
			}
			zchan->dtmfdbg.windex = windex;
		} else {
			rc = fwrite(data, 1, dlen, zchan->dtmfdbg.file);
			if (rc != dlen) {
				zap_log(ZAP_LOG_WARNING, "DTMF debugger wrote only %d out of %d bytes: %s\n", rc, datalen, strerror(errno));
			}
			zchan->dtmfdbg.closetimeout--;
			if (!zchan->dtmfdbg.closetimeout) {
				close_dtmf_debug(zchan);
			}
		}
		zap_mutex_unlock(zchan->dtmfdbg.mutex);
	}
#endif

	if (status == ZAP_SUCCESS) {
		if (zap_test_flag(zchan, ZAP_CHANNEL_USE_RX_GAIN) 
			&& (zchan->native_codec == ZAP_CODEC_ALAW || zchan->native_codec == ZAP_CODEC_ULAW)) {
			unsigned char *rdata = data;
			for (i = 0; i < *datalen; i++) {
				rdata[i] = zchan->rxgain_table[rdata[i]];
			}
		}
		handle_dtmf(zchan, *datalen);
	}

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

	if (zap_test_flag(zchan, ZAP_CHANNEL_DTMF_DETECT) || zap_test_flag(zchan, ZAP_CHANNEL_PROGRESS_DETECT) || 
		zap_test_flag(zchan, ZAP_CHANNEL_CALLERID_DETECT)) {
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

		if (zap_test_flag(zchan, ZAP_CHANNEL_CALLERID_DETECT)) {
			if (zap_fsk_demod_feed(&zchan->fsk, sln, slen) != ZAP_SUCCESS) {
				zap_size_t type, mlen;
				char str[128], *sp;
				
				while(zap_fsk_data_parse(&zchan->fsk, &type, &sp, &mlen) == ZAP_SUCCESS) {
					*(str+mlen) = '\0';
					zap_copy_string(str, sp, ++mlen);
					zap_clean_string(str);
					zap_log(ZAP_LOG_DEBUG, "FSK: TYPE %s LEN %d VAL [%s]\n", zap_mdmf_type2str(type), mlen-1, str);
					
					switch(type) {
					case MDMF_DDN:
					case MDMF_PHONE_NUM:
						{
							if (mlen > sizeof(zchan->caller_data.ani)) {
								mlen = sizeof(zchan->caller_data.ani);
							}
							zap_set_string(zchan->caller_data.ani.digits, str);
							zap_set_string(zchan->caller_data.cid_num.digits, zchan->caller_data.ani.digits);
						}
						break;
					case MDMF_NO_NUM:
						{
							zap_set_string(zchan->caller_data.ani.digits, *str == 'P' ? "private" : "unknown");
							zap_set_string(zchan->caller_data.cid_name, zchan->caller_data.ani.digits);
						}
						break;
					case MDMF_PHONE_NAME:
						{
							if (mlen > sizeof(zchan->caller_data.cid_name)) {
								mlen = sizeof(zchan->caller_data.cid_name);
							}
							zap_set_string(zchan->caller_data.cid_name, str);
						}
						break;
					case MDMF_NO_NAME:
						{
							zap_set_string(zchan->caller_data.cid_name, *str == 'P' ? "private" : "unknown");
						}
					case MDMF_DATETIME:
						{
							if (mlen > sizeof(zchan->caller_data.cid_date)) {
								mlen = sizeof(zchan->caller_data.cid_date);
							}
							zap_set_string(zchan->caller_data.cid_date, str);
						}
						break;
					}
				}
				zap_channel_command(zchan, ZAP_COMMAND_DISABLE_CALLERID_DETECT, NULL);
			}
		}

		if (zap_test_flag(zchan, ZAP_CHANNEL_PROGRESS_DETECT) && !zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_PROGRESS)) {
			uint32_t i;

			for (i = 1; i < ZAP_TONEMAP_INVALID; i++) {
				if (zchan->span->tone_finder[i].tone_count) {
					if (zchan->needed_tones[i] && teletone_multi_tone_detect(&zchan->span->tone_finder[i], sln, (int)slen)) {
						if (++zchan->detected_tones[i]) {
							zchan->needed_tones[i] = 0;
							zchan->detected_tones[0]++;
						}
					}
				}
			}
		}
	
		
		if (zap_test_flag(zchan, ZAP_CHANNEL_DTMF_DETECT) && !zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_DTMF_DETECT)) {
			teletone_dtmf_detect(&zchan->dtmf_detect, sln, (int)slen);
			teletone_dtmf_get(&zchan->dtmf_detect, digit_str, sizeof(digit_str));

			if(*digit_str) {
				zio_event_cb_t event_callback = NULL;

				if (zchan->state == ZAP_CHANNEL_STATE_CALLWAITING && (*digit_str == 'D' || *digit_str == 'A')) {
					zchan->detected_tones[ZAP_TONEMAP_CALLWAITING_ACK]++;
				} else {
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
				}
			}
		}
	}

	if (zchan->skip_read_frames > 0 || zap_test_flag(zchan, ZAP_CHANNEL_MUTE)) {
		
		zap_mutex_lock(zchan->pre_buffer_mutex);
		if (zchan->pre_buffer && zap_buffer_inuse(zchan->pre_buffer)) {
			zap_buffer_zero(zchan->pre_buffer);
		}
		zap_mutex_unlock(zchan->pre_buffer_mutex);


		memset(data, 255, *datalen);

		if (zchan->skip_read_frames > 0) {
			zchan->skip_read_frames--;
		}
	}  else	{
		zap_mutex_lock(zchan->pre_buffer_mutex);
		if (zchan->pre_buffer_size && zchan->pre_buffer) {
			zap_buffer_write(zchan->pre_buffer, data, *datalen);
			if (zap_buffer_inuse(zchan->pre_buffer) >= zchan->pre_buffer_size) {
				zap_buffer_read(zchan->pre_buffer, data, *datalen);
			} else {
				memset(data, 255, *datalen);
			}
		}
		zap_mutex_unlock(zchan->pre_buffer_mutex);
	}


	return status;
}


OZ_DECLARE(zap_status_t) zap_channel_write(zap_channel_t *zchan, void *data, zap_size_t datasize, zap_size_t *datalen)
{
	zap_status_t status = ZAP_FAIL;
	zio_codec_t codec_func = NULL;
	zap_size_t max = datasize;
	unsigned i = 0;

	assert(zchan != NULL);
	assert(zchan->zio != NULL);

	if (!zchan->buffer_delay && 
		((zchan->dtmf_buffer && zap_buffer_inuse(zchan->dtmf_buffer)) ||
		 (zchan->fsk_buffer && zap_buffer_inuse(zchan->fsk_buffer)))) {
		/* read size writing DTMF ATM */
		return ZAP_SUCCESS;
	}


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
	
	if (zchan->fds[1] > -1) {
		int dlen = (int) *datalen;
		if ((write(zchan->fds[1], data, dlen)) != dlen) {
			snprintf(zchan->last_error, sizeof(zchan->last_error), "file write error!");
			return ZAP_FAIL;
		}
	}

	if (zap_test_flag(zchan, ZAP_CHANNEL_USE_TX_GAIN) 
		&& (zchan->native_codec == ZAP_CODEC_ALAW || zchan->native_codec == ZAP_CODEC_ULAW)) {
		unsigned char *wdata = data;
		for (i = 0; i < *datalen; i++) {
			wdata[i] = zchan->txgain_table[wdata[i]];
		}
	}
    status = zchan->zio->write(zchan, data, datalen);

	return status;
}

OZ_DECLARE(zap_status_t) zap_channel_clear_vars(zap_channel_t *zchan)
{
	if(zchan->variable_hash) {
		hashtable_destroy(zchan->variable_hash);
	}
	zchan->variable_hash = create_hashtable(16, zap_hash_hashfromstring, zap_hash_equalkeys);

	if(!zchan->variable_hash)
		return ZAP_FAIL;
	
	return ZAP_SUCCESS;
}

OZ_DECLARE(zap_status_t) zap_channel_add_var(zap_channel_t *zchan, const char *var_name, const char *value)
{
	char *t_name = 0, *t_val = 0;

	if(!zchan->variable_hash || !var_name || !value)
	{
		return ZAP_FAIL;
	}

	t_name = strdup(var_name);
	t_val = strdup(value);

	if(hashtable_insert(zchan->variable_hash, t_name, t_val, HASHTABLE_FLAG_FREE_KEY | HASHTABLE_FLAG_FREE_VALUE)) {
		return ZAP_SUCCESS;
	}
	return ZAP_FAIL;
}

OZ_DECLARE(const char *) zap_channel_get_var(zap_channel_t *zchan, const char *var_name)
{
	if(!zchan->variable_hash || !var_name)
	{
		return NULL;
	}
	return (const char *) hashtable_search(zchan->variable_hash, (void *)var_name);
}

static struct {
	zap_io_interface_t *pika_interface;
} interfaces;


OZ_DECLARE(char *) zap_api_execute(const char *type, const char *cmd)
{
	zap_io_interface_t *zio = NULL;
	char *dup = NULL, *p;
	char *rval = NULL;

	if (type && !cmd) {
		dup = strdup(type);
		if ((p = strchr(dup, ' '))) {
			*p++ = '\0';
			cmd = p;
		}

		type = dup;
	}
	
	zap_mutex_lock(globals.mutex);
	if (!(zio = (zap_io_interface_t *) hashtable_search(globals.interface_hash, (void *)type))) {
		zap_load_module_assume(type);
		if ((zio = (zap_io_interface_t *) hashtable_search(globals.interface_hash, (void *)type))) {
			zap_log(ZAP_LOG_INFO, "auto-loaded '%s'\n", type);
		}
	}
	zap_mutex_unlock(globals.mutex);

	if (zio && zio->api) {
		zap_stream_handle_t stream = { 0 };
		zap_status_t status;
		ZAP_STANDARD_STREAM(stream);
		status = zio->api(&stream, cmd);
		
		if (status != ZAP_SUCCESS) {
			zap_safe_free(stream.data);
		} else {
			rval = (char *) stream.data;
		}
	}

	zap_safe_free(dup);
	
	return rval;
}

static void zap_set_channels_gains(zap_span_t *span, int currindex, float rxgain, float txgain)
{
	unsigned chan_index = 0;

	if (!span->chan_count) {
		return;
	}

	for (chan_index = currindex+1; chan_index <= span->chan_count; chan_index++) {
		if (!ZAP_IS_VOICE_CHANNEL(span->channels[chan_index])) {
			continue;
		}
		zap_channel_command(span->channels[chan_index], ZAP_COMMAND_SET_RX_GAIN, &rxgain);
		zap_channel_command(span->channels[chan_index], ZAP_COMMAND_SET_TX_GAIN, &txgain);
	}
}

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
	zap_analog_start_type_t tmp;
	float rxgain = 0.0;
	float txgain = 0.0;
	int chanindex = 0;

	if (!zap_config_open_file(&cfg, cfg_name)) {
		return ZAP_FAIL;
	}
	
	while (zap_config_next_pair(&cfg, &var, &val)) {
		if (*cfg.category == '#') {
			if (cfg.catno != catno) {
				zap_log(ZAP_LOG_DEBUG, "Skipping %s\n", cfg.category);
				catno = cfg.catno;
			}
		} else if (!strncasecmp(cfg.category, "span", 4)) {
			if (cfg.catno != catno) {
				char *type = cfg.category + 4;
				char *name;
				
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

				if ((name = strchr(type, ' '))) {
					*name++ = '\0';
				}

				zap_mutex_lock(globals.mutex);
				if (!(zio = (zap_io_interface_t *) hashtable_search(globals.interface_hash, type))) {
					zap_load_module_assume(type);
					if ((zio = (zap_io_interface_t *) hashtable_search(globals.interface_hash, type))) {
						zap_log(ZAP_LOG_INFO, "auto-loaded '%s'\n", type);
					}
				}
				zap_mutex_unlock(globals.mutex);

				if (!zio) {
					zap_log(ZAP_LOG_CRIT, "failure creating span, no such type '%s'\n", type);
					span = NULL;
					continue;
				}

				if (!zio->configure_span) {
					zap_log(ZAP_LOG_CRIT, "failure creating span, no configure_span method for '%s'\n", type);
					span = NULL;
					continue;
				}

				if (zap_span_create(zio, &span, name) == ZAP_SUCCESS) {
					span->type = strdup(type);
					d = 0;

					zap_log(ZAP_LOG_DEBUG, "created span %d (%s) of type %s\n", span->span_id, span->name, type);
					
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
			} else if (!strcasecmp(var, "analog-start-type")) {
				if (span->trunk_type == ZAP_TRUNK_FXS || span->trunk_type == ZAP_TRUNK_FXO || span->trunk_type == ZAP_TRUNK_EM) {
					if ((tmp = zap_str2zap_analog_start_type(val)) != ZAP_ANALOG_START_NA) {
						span->start_type = tmp;
						zap_log(ZAP_LOG_DEBUG, "changing start type to '%s'\n", zap_analog_start_type2str(span->start_type)); 
					}
				} else {
					zap_log(ZAP_LOG_ERROR, "This option is only valid on analog trunks!\n");
				}
			} else if (!strcasecmp(var, "fxo-channel")) {
				if (span->trunk_type == ZAP_TRUNK_NONE) {
					span->trunk_type = ZAP_TRUNK_FXO;										
					zap_log(ZAP_LOG_DEBUG, "setting trunk type to '%s' start(%s)\n", zap_trunk_type2str(span->trunk_type), 
							zap_analog_start_type2str(span->start_type));
				}
				if (span->trunk_type == ZAP_TRUNK_FXO) {
					chanindex = span->chan_count;
					configured += zio->configure_span(span, val, ZAP_CHAN_TYPE_FXO, name, number);
					zap_set_channels_gains(span, chanindex, rxgain, txgain);
				} else {
					zap_log(ZAP_LOG_WARNING, "Cannot add FXO channels to an FXS trunk!\n");
				}
			} else if (!strcasecmp(var, "fxs-channel")) {
				if (span->trunk_type == ZAP_TRUNK_NONE) {
					span->trunk_type = ZAP_TRUNK_FXS;
					zap_log(ZAP_LOG_DEBUG, "setting trunk type to '%s' start(%s)\n", zap_trunk_type2str(span->trunk_type), 
							zap_analog_start_type2str(span->start_type));
				}
				if (span->trunk_type == ZAP_TRUNK_FXS) {
					chanindex = span->chan_count;
					configured += zio->configure_span(span, val, ZAP_CHAN_TYPE_FXS, name, number);
					zap_set_channels_gains(span, chanindex, rxgain, txgain);
				} else {
					zap_log(ZAP_LOG_WARNING, "Cannot add FXS channels to an FXO trunk!\n");
				}
			} else if (!strcasecmp(var, "em-channel")) {
				if (span->trunk_type == ZAP_TRUNK_NONE) {
					span->trunk_type = ZAP_TRUNK_EM;
					zap_log(ZAP_LOG_DEBUG, "setting trunk type to '%s' start(%s)\n", zap_trunk_type2str(span->trunk_type), 
							zap_analog_start_type2str(span->start_type));
				}
				if (span->trunk_type == ZAP_TRUNK_EM) {
					chanindex = span->chan_count;
					configured += zio->configure_span(span, val, ZAP_CHAN_TYPE_EM, name, number);
					zap_set_channels_gains(span, chanindex, rxgain, txgain);
				} else {
					zap_log(ZAP_LOG_WARNING, "Cannot add EM channels to a non-EM trunk!\n");
				}
			} else if (!strcasecmp(var, "b-channel")) {
				chanindex = span->chan_count;
				configured += zio->configure_span(span, val, ZAP_CHAN_TYPE_B, name, number);
				zap_set_channels_gains(span, chanindex, rxgain, txgain);
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
			} else if (!strcasecmp(var, "cas-channel")) {
				chanindex = span->chan_count;
				configured += zio->configure_span(span, val, ZAP_CHAN_TYPE_CAS, name, number);	
				zap_set_channels_gains(span, chanindex, rxgain, txgain);
			} else if (!strcasecmp(var, "dtmf_hangup")) {
				span->dtmf_hangup = strdup(val);
				span->dtmf_hangup_len = strlen(val);
			} else if (!strcasecmp(var, "txgain")) {
				if (sscanf(val, "%f", &txgain) != 1) {
					zap_log(ZAP_LOG_ERROR, "invalid txgain: '%s'\n", val);
				}
			} else if (!strcasecmp(var, "rxgain")) {
				if (sscanf(val, "%f", &rxgain) != 1) {
					zap_log(ZAP_LOG_ERROR, "invalid rxgain: '%s'\n", val);
				}
			} else {
				zap_log(ZAP_LOG_ERROR, "unknown span variable '%s'\n", var);
			}
		} else if (!strncasecmp(cfg.category, "general", 7)) {
			if (!strncasecmp(var, "cpu_monitoring_interval", 24)) {
				if (atoi(val) > 0) {
					globals.cpu_monitor.interval = atoi(val);
				} else {
					zap_log(ZAP_LOG_ERROR, "Invalid cpu monitoring interval %s\n", val);
				}
			} else	if (!strncasecmp(var, "cpu_set_alarm_threshold", 22)) {
				if (atoi(val) > 0 && atoi(val) < 100) {
					globals.cpu_monitor.set_alarm_threshold = (uint8_t)atoi(val);
				} else {
					zap_log(ZAP_LOG_ERROR, "Invalid cpu alarm set threshold %s\n", val);
				}
			} else if (!strncasecmp(var, "cpu_reset_alarm_threshold", 22)) {
				if (atoi(val) > 0 && atoi(val) < 100) {
					globals.cpu_monitor.reset_alarm_threshold = (uint8_t)atoi(val);
					if (globals.cpu_monitor.reset_alarm_threshold > globals.cpu_monitor.set_alarm_threshold) {
						globals.cpu_monitor.reset_alarm_threshold = globals.cpu_monitor.set_alarm_threshold-10;
						zap_log(ZAP_LOG_ERROR, "Cpu alarm reset threshold must be lower than set threshold, set threshold to %d\n", globals.cpu_monitor.reset_alarm_threshold);
					}
				} else {
					zap_log(ZAP_LOG_ERROR, "Invalid cpu alarm reset threshold %s\n", val);
				}
			} else if (!strncasecmp(var, "cpu_alarm_action", 16)) {
				char* p = val;
				do {
					if (!strncasecmp(p, "reject", 6)) {
						globals.cpu_monitor.alarm_action_flags |= ZAP_CPU_ALARM_ACTION_REJECT;
					} else if (!strncasecmp(p, "warn", 4)) {
						globals.cpu_monitor.alarm_action_flags |= ZAP_CPU_ALARM_ACTION_WARN;
					}
					p = strchr(p, ',');
					if (p) {
						while(++p) if (*p != 0x20) break;
					}
				} while (p);
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
		zap_log(ZAP_LOG_DEBUG, "Module %s does not support configuration.\n", zio->name);	
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

OZ_DECLARE(int) zap_load_module(const char *name)
{
	zap_dso_lib_t lib;
	int count = 0, x = 0;
	char path[128] = "";
	char *err;
	zap_module_t *mod;

#ifdef WIN32
    const char *ext = ".dll";
    //const char *EXT = ".DLL";
#define ZAP_MOD_DIR "." //todo
#elif defined (MACOSX) || defined (DARWIN)
    const char *ext = ".dylib";
    //const char *EXT = ".DYLIB";
#else
    const char *ext = ".so";
    //const char *EXT = ".SO";
#endif
	
	if (*name == *ZAP_PATH_SEPARATOR) {
		snprintf(path, sizeof(path), "%s%s", name, ext);
	} else {
		snprintf(path, sizeof(path), "%s%s%s%s", ZAP_MOD_DIR, ZAP_PATH_SEPARATOR, name, ext);
	}
	
	if (!(lib = zap_dso_open(path, &err))) {
		zap_log(ZAP_LOG_ERROR, "Error loading %s [%s]\n", path, err);
		zap_safe_free(err);
		return 0;
	}
	
	if (!(mod = (zap_module_t *) zap_dso_func_sym(lib, "zap_module", &err))) {
		zap_log(ZAP_LOG_ERROR, "Error loading %s [%s]\n", path, err);
		zap_safe_free(err);
		return 0;
	}

	if (mod->io_load) {
		zap_io_interface_t *interface1 = NULL; /* name conflict w/windows here */

		if (mod->io_load(&interface1) != ZAP_SUCCESS || !interface1 || !interface1->name) {
			zap_log(ZAP_LOG_ERROR, "Error loading %s\n", path);
		} else {
			zap_log(ZAP_LOG_INFO, "Loading IO from %s [%s]\n", path, interface1->name);
			zap_mutex_lock(globals.mutex);
			if (hashtable_search(globals.interface_hash, (void *)interface1->name)) {
				zap_log(ZAP_LOG_ERROR, "Interface %s already loaded!\n", interface1->name);
			} else {
				hashtable_insert(globals.interface_hash, (void *)interface1->name, interface1, HASHTABLE_FLAG_NONE);
				process_module_config(interface1);
				x++;
			}
			zap_mutex_unlock(globals.mutex);
		}
	}

	if (mod->sig_load) {
		if (mod->sig_load() != ZAP_SUCCESS) {
			zap_log(ZAP_LOG_ERROR, "Error loading %s\n", path);
		} else {
			zap_log(ZAP_LOG_INFO, "Loading SIG from %s\n", path);
			x++;
		}
	}

	if (x) {
		char *p;
		mod->lib = lib;
		zap_set_string(mod->path, path);
		if (mod->name[0] == '\0') {
			if (!(p = strrchr(path, *ZAP_PATH_SEPARATOR))) {
				p = path;
			}
			zap_set_string(mod->name, p);
		}

		zap_mutex_lock(globals.mutex);
		if (hashtable_search(globals.module_hash, (void *)mod->name)) {
			zap_log(ZAP_LOG_ERROR, "Module %s already loaded!\n", mod->name);
			zap_dso_destroy(&lib);
		} else {
			hashtable_insert(globals.module_hash, (void *)mod->name, mod, HASHTABLE_FLAG_NONE);
			count++;
		}
		zap_mutex_unlock(globals.mutex);
	} else {
		zap_log(ZAP_LOG_ERROR, "Unloading %s\n", path);
		zap_dso_destroy(&lib);
	}
	
	return count;
}

OZ_DECLARE(int) zap_load_module_assume(const char *name)
{
	char buf[256] = "";

	snprintf(buf, sizeof(buf), "ozmod_%s", name);
	return zap_load_module(buf);
}

OZ_DECLARE(int) zap_load_modules(void)
{
	char cfg_name[] = "modules.conf";
	zap_config_t cfg;
	char *var, *val;
	int count = 0;

	if (!zap_config_open_file(&cfg, cfg_name)) {
        return ZAP_FAIL;
    }

	while (zap_config_next_pair(&cfg, &var, &val)) {
        if (!strcasecmp(cfg.category, "modules")) {
			if (!strcasecmp(var, "load")) {
				count += zap_load_module(val);
			}
		}
	}
			
	return count;
}

OZ_DECLARE(zap_status_t) zap_unload_modules(void)
{
	zap_hash_iterator_t *i;
	zap_dso_lib_t lib;

	for (i = hashtable_first(globals.module_hash); i; i = hashtable_next(i)) {
		const void *key;
		void *val;

		hashtable_this(i, &key, NULL, &val);
		
		if (key && val) {
			zap_module_t *mod = (zap_module_t *) val;

			if (!mod) {
				continue;
			}

			if (mod->io_unload) {
				if (mod->io_unload() == ZAP_SUCCESS) {
					zap_log(ZAP_LOG_INFO, "Unloading IO %s\n", mod->name);
				} else {
					zap_log(ZAP_LOG_ERROR, "Error unloading IO %s\n", mod->name);
				}
			} 

			if (mod->sig_unload) {
				if (mod->sig_unload() == ZAP_SUCCESS) {
					zap_log(ZAP_LOG_INFO, "Unloading SIG %s\n", mod->name);
				} else {
					zap_log(ZAP_LOG_ERROR, "Error unloading SIG %s\n", mod->name);
				}
			} 
			

			zap_log(ZAP_LOG_INFO, "Unloading %s\n", mod->path);
			lib = mod->lib;
			zap_dso_destroy(&lib);
			
		}
	}

	return ZAP_SUCCESS;
}

OZ_DECLARE(zap_status_t) zap_configure_span(const char *type, zap_span_t *span, zio_signal_cb_t sig_cb, ...)
{
	zap_module_t *mod = (zap_module_t *) hashtable_search(globals.module_hash, (void *)type);
	zap_status_t status = ZAP_FAIL;

	if (!mod) {
		zap_load_module_assume(type);
		if ((mod = (zap_module_t *) hashtable_search(globals.module_hash, (void *)type))) {
			zap_log(ZAP_LOG_INFO, "auto-loaded '%s'\n", type);
		}
	}

	if (mod && mod->sig_configure) {
		va_list ap;
		va_start(ap, sig_cb);
		status = mod->sig_configure(span, sig_cb, ap);
		va_end(ap);
	} else {
		zap_log(ZAP_LOG_ERROR, "can't find '%s'\n", type);
		status = ZAP_FAIL;
	}

	return status;
}

OZ_DECLARE(zap_status_t) zap_span_start(zap_span_t *span)
{
	if (span->start) {
		return span->start(span);
	}

	return ZAP_FAIL;
}

OZ_DECLARE(zap_status_t) zap_span_send_signal(zap_span_t *span, zap_sigmsg_t *sigmsg)
{
	zap_status_t status = ZAP_FAIL;

	if (span->signal_cb) {

		if (sigmsg->channel) {
			zap_mutex_lock(sigmsg->channel->mutex);
		}

		status = span->signal_cb(sigmsg);

		if (sigmsg->channel) {
			zap_mutex_unlock(sigmsg->channel->mutex);
		}
	}

	return status;
}


OZ_DECLARE(zap_status_t) zap_global_init(void)
{
	int modcount;
	
	memset(&globals, 0, sizeof(globals));

	time_init();
	
	zap_thread_override_default_stacksize(ZAP_THREAD_STACKSIZE);

	memset(&interfaces, 0, sizeof(interfaces));
	globals.interface_hash = create_hashtable(16, zap_hash_hashfromstring, zap_hash_equalkeys);
	globals.module_hash = create_hashtable(16, zap_hash_hashfromstring, zap_hash_equalkeys);
	globals.span_hash = create_hashtable(16, zap_hash_hashfromstring, zap_hash_equalkeys);
	modcount = 0;
	zap_mutex_create(&globals.mutex);
	zap_mutex_create(&globals.span_mutex);
	
	modcount = zap_load_modules();
	zap_log(ZAP_LOG_NOTICE, "Modules configured: %d \n", modcount);

	globals.cpu_monitor.interval = 1000;
	globals.cpu_monitor.alarm_action_flags = ZAP_CPU_ALARM_ACTION_WARN | ZAP_CPU_ALARM_ACTION_REJECT;
	globals.cpu_monitor.set_alarm_threshold = 80;
	globals.cpu_monitor.reset_alarm_threshold = 70;

	if (load_config() != ZAP_SUCCESS) {
		zap_log(ZAP_LOG_ERROR, "No modules configured!\n");
		return ZAP_FAIL;
	}

	globals.running = 1;
	if (!zap_cpu_monitor_disabled) {
		if (zap_cpu_monitor_start() != ZAP_SUCCESS) {
			return ZAP_FAIL;
		}
	}
	return ZAP_SUCCESS;
}

OZ_DECLARE(uint32_t) zap_running(void)
{
	return globals.running;
}


OZ_DECLARE(zap_status_t) zap_global_destroy(void)
{
	unsigned int j;
	zap_span_t *sp;

	time_end();

	globals.running = 0;
	zap_cpu_monitor_stop();
	zap_span_close_all();
	zap_sleep(1000);
	
	zap_mutex_lock(globals.span_mutex);
	for (sp = globals.spans; sp;) {
		zap_span_t *cur_span = sp;
		sp = sp->next;

		if (cur_span) {
			if (zap_test_flag(cur_span, ZAP_SPAN_CONFIGURED)) {
				zap_mutex_lock(cur_span->mutex);
				zap_clear_flag(cur_span, ZAP_SPAN_CONFIGURED);
				for(j = 1; j <= cur_span->chan_count && cur_span->channels[j]; j++) {
					zap_channel_t *cur_chan = cur_span->channels[j];
					if (cur_chan) {
						if (zap_test_flag(cur_chan, ZAP_CHANNEL_CONFIGURED)) {
							zap_channel_destroy(cur_chan);
						}
						free(cur_chan);
						cur_chan = NULL;
					}
				}
				zap_mutex_unlock(cur_span->mutex);

				if (cur_span->mutex) {
					zap_mutex_destroy(&cur_span->mutex);
				}

				zap_safe_free(cur_span->signal_data);
				zap_span_destroy(cur_span);
			}

			hashtable_remove(globals.span_hash, (void *)cur_span->name);
			zap_safe_free(cur_span->type);
			zap_safe_free(cur_span->name);
			free(cur_span);
			cur_span = NULL;
		}
	}
	globals.spans = NULL;
	zap_mutex_unlock(globals.span_mutex);

	globals.span_index = 0;
	
	zap_unload_modules();

	zap_mutex_lock(globals.mutex);
	hashtable_destroy(globals.interface_hash);
	hashtable_destroy(globals.module_hash);
	hashtable_destroy(globals.span_hash);
	zap_mutex_unlock(globals.mutex);
	zap_mutex_destroy(&globals.mutex);
	zap_mutex_destroy(&globals.span_mutex);
	memset(&globals, 0, sizeof(globals));
	return ZAP_SUCCESS;
}


OZ_DECLARE(uint32_t) zap_separate_string(char *buf, char delim, char **array, int arraylen)
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

OZ_DECLARE(void) zap_bitstream_init(zap_bitstream_t *bsp, uint8_t *data, uint32_t datalen, zap_endian_t endian, uint8_t ss)
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

OZ_DECLARE(int8_t) zap_bitstream_get_bit(zap_bitstream_t *bsp)
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

OZ_DECLARE(void) print_hex_bytes(uint8_t *data, zap_size_t dlen, char *buf, zap_size_t blen)
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

OZ_DECLARE(void) print_bits(uint8_t *b, int bl, char *buf, int blen, zap_endian_t e, uint8_t ss)
{
	zap_bitstream_t bs;
	int j = 0, c = 0;
	int8_t bit;
	uint32_t last;

	if (blen < (bl * 10) + 2) {
        return;
    }

	zap_bitstream_init(&bs, b, bl, e, ss);
	last = bs.byte_index;	
	while((bit = zap_bitstream_get_bit(&bs)) > -1) {
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



OZ_DECLARE_NONSTD(zap_status_t) zap_console_stream_raw_write(zap_stream_handle_t *handle, uint8_t *data, zap_size_t datalen)
{
	zap_size_t need = handle->data_len + datalen;
	
	if (need >= handle->data_size) {
		void *new_data;
		need += handle->alloc_chunk;

		if (!(new_data = realloc(handle->data, need))) {
			return ZAP_MEMERR;
		}

		handle->data = new_data;
		handle->data_size = need;
	}

	memcpy((uint8_t *) (handle->data) + handle->data_len, data, datalen);
	handle->data_len += datalen;
	handle->end = (uint8_t *) (handle->data) + handle->data_len;
	*(uint8_t *)handle->end = '\0';

	return ZAP_SUCCESS;
}

OZ_DECLARE(int) zap_vasprintf(char **ret, const char *fmt, va_list ap) /* code from switch_apr.c */
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

	if (len > 0 && (buf = malloc((buflen = (size_t) (len + 1)))) != NULL) {
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

OZ_DECLARE_NONSTD(zap_status_t) zap_console_stream_write(zap_stream_handle_t *handle, const char *fmt, ...)
{
	va_list ap;
	char *buf = handle->data;
	char *end = handle->end;
	int ret = 0;
	char *data = NULL;

	if (handle->data_len >= handle->data_size) {
		return ZAP_FAIL;
	}

	va_start(ap, fmt);
	ret = zap_vasprintf(&data, fmt, ap);
	va_end(ap);

	if (data) {
		zap_size_t remaining = handle->data_size - handle->data_len;
		zap_size_t need = strlen(data) + 1;

		if ((remaining < need) && handle->alloc_len) {
			zap_size_t new_len;
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
				zap_log(ZAP_LOG_CRIT, "Memory Error!\n");
				free(data);
				return ZAP_FAIL;
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
		free(data);
	}

	return ret ? ZAP_FAIL : ZAP_SUCCESS;
}

static void *zap_cpu_monitor_run(zap_thread_t *me, void *obj)
{
#ifndef WIN32
	cpu_monitor_t *monitor = (cpu_monitor_t *)obj;
	struct zap_cpu_monitor_stats *cpu_stats = zap_new_cpu_monitor();
	if (!cpu_stats) {
		return NULL;
	}
	monitor->running = 1;

	while(zap_running()) {
		double time;
		if (zap_cpu_get_system_idle_time(cpu_stats, &time)) {
			break;
		}

		if (monitor->alarm) {
			if ((int)time >= (100-monitor->set_alarm_threshold)) {
				zap_log(ZAP_LOG_DEBUG, "CPU alarm OFF (idle:%d)\n", (int) time);
				monitor->alarm = 0;
			}
			if (monitor->alarm_action_flags & ZAP_CPU_ALARM_ACTION_WARN) {
				zap_log(ZAP_LOG_WARNING, "CPU alarm is ON (cpu usage:%d)\n", (int) (100-time));
			}
		} else {
			if ((int)time <= (100-monitor->reset_alarm_threshold)) {
				zap_log(ZAP_LOG_DEBUG, "CPU alarm ON (idle:%d)\n", (int) time);
				monitor->alarm = 1;
			}
		}
		zap_interrupt_wait(monitor->interrupt, monitor->interval);
	}
	zap_delete_cpu_monitor(cpu_stats);
	monitor->running = 0;
#else
	UNREFERENCED_PARAMETER(me);
	UNREFERENCED_PARAMETER(obj);
#endif

	return NULL;
}


static zap_status_t zap_cpu_monitor_start(void)
{
	if (zap_interrupt_create(&globals.cpu_monitor.interrupt, ZAP_INVALID_SOCKET) != ZAP_SUCCESS) {
		zap_log(ZAP_LOG_CRIT, "Failed to create CPU monitor interrupt\n");
		return ZAP_FAIL;
	}
	
	if (zap_thread_create_detached(zap_cpu_monitor_run, &globals.cpu_monitor) != ZAP_SUCCESS) {
		zap_log(ZAP_LOG_CRIT, "Failed to create cpu monitor thread!!\n");
		return ZAP_FAIL;
	}
	return ZAP_SUCCESS;
}

static void zap_cpu_monitor_stop(void)
{
	if (!globals.cpu_monitor.interrupt) {
		return;
	}

	if (!globals.cpu_monitor.running) {
		return;
	}

	if (zap_interrupt_signal(globals.cpu_monitor.interrupt) != ZAP_SUCCESS) {
		zap_log(ZAP_LOG_CRIT, "Failed to stop CPU monitor\n");
		return;
	}

	while(globals.cpu_monitor.running) {
		zap_sleep(10);
	}
	
	zap_interrupt_destroy(&globals.cpu_monitor.interrupt);
}

OZ_DECLARE(void) zap_cpu_monitor_disable(void)
{
	zap_cpu_monitor_disabled = 1;
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
