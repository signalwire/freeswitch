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
#include <stdarg.h>
#ifdef ZAP_WANPIPE_SUPPORT
#include "zap_wanpipe.h"
#endif
#ifdef ZAP_ZT_SUPPORT
#include "zap_zt.h"
#endif

static struct {
	zap_hash_t *interface_hash;
} globals;

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

static unsigned hashfromstring(void *ky)
{
	unsigned char *str = (unsigned char *) ky;
	unsigned hash = 0;
    int c;
	
	while ((c = *str++)) {
        hash = c + (hash << 6) + (hash << 16) - hash;
	}

    return hash;
}

zap_status_t zap_span_create(zap_software_interface_t *zint, zap_span_t **span)
{
	zap_span_t *new_span = NULL;

	assert(zint != NULL);

	if (zint->span_index < ZAP_MAX_SPANS_INTERFACE) {
		new_span = &zint->spans[++zint->span_index];
		memset(new_span, 0, sizeof(*new_span));
		zap_set_flag(new_span, ZAP_SPAN_CONFIGURED);
		new_span->span_id = zint->span_index;
		new_span->zint = zint;
		*span = new_span;
		return ZAP_SUCCESS;
	}
	
	return ZAP_FAIL;
}

zap_status_t zap_span_add_channel(zap_span_t *span, zap_socket_t sockfd, zap_chan_type_t type, zap_channel_t **chan)
{
	if (span->chan_count < ZAP_MAX_CHANNELS_SPAN) {
		zap_channel_t *new_chan;
		new_chan = &span->channels[++span->chan_count];
		new_chan->type = type;
		new_chan->sockfd = sockfd;
		new_chan->zint = span->zint;
		new_chan->span_id = span->span_id;
		new_chan->chan_id = span->chan_count;
		new_chan->span = span;
		zap_set_flag(new_chan, ZAP_CHANNEL_CONFIGURED | ZAP_CHANNEL_READY);
		*chan = new_chan;
		return ZAP_SUCCESS;
	}

	return ZAP_FAIL;
}

zap_status_t zap_span_find(const char *name, unsigned id, zap_span_t **span)
{
	zap_software_interface_t *zint = (zap_software_interface_t *) hashtable_search(globals.interface_hash, (char *)name);
	zap_span_t *fspan;

	if (!zint) {
		return ZAP_FAIL;
	}

	if (id > ZAP_MAX_SPANS_INTERFACE) {
		return ZAP_FAIL;
	}

	fspan = &zint->spans[id];

	if (!zap_test_flag(fspan, ZAP_SPAN_CONFIGURED)) {
		return ZAP_FAIL;
	}

	*span = fspan;

	return ZAP_SUCCESS;
	
}

zap_status_t zap_span_set_event_callback(zap_span_t *span, zint_event_cb_t event_callback)
{
	span->event_callback = event_callback;
	return ZAP_SUCCESS;
}

zap_status_t zap_channel_set_event_callback(zap_channel_t *zchan, zint_event_cb_t event_callback)
{
	zchan->event_callback = event_callback;
	return ZAP_SUCCESS;
}

zap_status_t zap_channel_open_any(const char *name, unsigned span_id, zap_direction_t direction, zap_channel_t **zchan)
{
	zap_software_interface_t *zint = (zap_software_interface_t *) hashtable_search(globals.interface_hash, (char *)name);
	zap_status_t status = ZAP_FAIL;
	zap_channel_t *check;
	unsigned i,j;
	zap_span_t *span;
	unsigned span_max;

	if (!zint) {
		zap_log(ZAP_LOG_ERROR, "Invalid interface name!\n");
		return ZAP_FAIL;
	}

	if (span_id) {
		span_max = span_id;
	} else {
		span_max = zint->span_index;
	}

	if (direction == ZAP_TOP_DOWN) {
		j = 1;
	} else {
		j = span_max;
	}

	

	for(;;) {
		span = &zint->spans[j];

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

			if (zap_test_flag(check, ZAP_CHANNEL_READY) && !zap_test_flag(check, ZAP_CHANNEL_OPEN)) {

				status = check->zint->open(check);

				if (status == ZAP_SUCCESS) {
					zap_set_flag(check, ZAP_CHANNEL_OPEN);
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

	return status;
}

zap_status_t zap_channel_open(const char *name, unsigned span_id, unsigned chan_id, zap_channel_t **zchan)
{
	zap_software_interface_t *zint = (zap_software_interface_t *) hashtable_search(globals.interface_hash, (char *)name);
	zap_status_t status = ZAP_FAIL;

	if (span_id < ZAP_MAX_SPANS_INTERFACE && chan_id < ZAP_MAX_CHANNELS_SPAN && zint) {
		zap_channel_t *check;
		check = &zint->spans[span_id].channels[chan_id];
		if (zap_test_flag(check, ZAP_CHANNEL_READY) && ! zap_test_flag(check, ZAP_CHANNEL_OPEN)) {
			status = check->zint->open(check);
			if (status == ZAP_SUCCESS) {
				zap_set_flag(check, ZAP_CHANNEL_OPEN);
				*zchan = check;
			}
			return status;
		}
	}

	return status;
}

zap_status_t zap_channel_close(zap_channel_t **zchan)
{
	zap_channel_t *check;
	zap_status_t status = ZAP_FAIL;

	assert(zchan != NULL);
	check = *zchan;
	assert(check != NULL);
	*zchan = NULL;
	
	if (zap_test_flag(check, ZAP_CHANNEL_OPEN)) {
		status = check->zint->close(check);
		if (status == ZAP_SUCCESS) {
			zap_clear_flag(check, ZAP_CHANNEL_OPEN);
			check->event_callback = NULL;
			zap_clear_flag(check, ZAP_CHANNEL_DTMF_DETECT);
			zap_clear_flag(check, ZAP_CHANNEL_SUPRESS_DTMF);
			*zchan = NULL;
		}
	}
	
	return status;
}


zap_status_t zap_channel_command(zap_channel_t *zchan, zap_command_t command, void *obj)
{
	assert(zchan != NULL);
	assert(zchan->zint != NULL);

    if (!zap_test_flag(zchan, ZAP_CHANNEL_OPEN)) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "channel not open");
        return ZAP_FAIL;
    }

	switch(command) {
	case ZAP_COMMAND_ENABLE_TONE_DETECT:
		{
			/* if they don't have thier own, use ours */
			if (!zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_DTMF_DETECT)) {
				zap_tone_type_t tt = ZAP_COMMAND_OBJ_INT;
				if (tt == ZAP_TONE_DTMF) {
					teletone_dtmf_detect_init (&zchan->dtmf_detect, 8000);
					zap_set_flag(zchan, ZAP_CHANNEL_DTMF_DETECT);
					zap_set_flag(zchan, ZAP_CHANNEL_SUPRESS_DTMF);
					return ZAP_SUCCESS;
				} else {
					snprintf(zchan->last_error, sizeof(zchan->last_error), "invalid command");
					return ZAP_FAIL;
				}
			}
		}
		break;
	case ZAP_COMMAND_DISABLE_TONE_DETECT:
		{
			if (!zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_DTMF_DETECT)) {
				zap_tone_type_t tt = ZAP_COMMAND_OBJ_INT;
                if (tt == ZAP_TONE_DTMF) {
                    teletone_dtmf_detect_init (&zchan->dtmf_detect, 8000);
                    zap_clear_flag(zchan, ZAP_CHANNEL_DTMF_DETECT);
					zap_clear_flag(zchan, ZAP_CHANNEL_SUPRESS_DTMF);
                    return ZAP_SUCCESS;
                } else {
                    snprintf(zchan->last_error, sizeof(zchan->last_error), "invalid command");
                    return ZAP_FAIL;
                }
			}
		}
		break;
	default:
		break;
	}

	if (!zchan->zint->command) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "method not implemented");
		return ZAP_FAIL;
	}

    return zchan->zint->command(zchan, command, obj);

}

zap_status_t zap_channel_wait(zap_channel_t *zchan, zap_wait_flag_t *flags, unsigned to)
{
	assert(zchan != NULL);
	assert(zchan->zint != NULL);

    if (!zap_test_flag(zchan, ZAP_CHANNEL_OPEN)) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "channel not open");
        return ZAP_FAIL;
    }

	if (!zchan->zint->wait) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "method not implemented");
		return ZAP_FAIL;
	}

    return zchan->zint->wait(zchan, flags, to);

}

/*******************************/
ZINT_CODEC_FUNCTION(zint_slin2ulaw)
{
	int16_t sln_buf[512] = {0}, *sln = sln_buf;
	uint8_t *lp = data;
	unsigned i;
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


ZINT_CODEC_FUNCTION(zint_ulaw2slin)
{
	int16_t *sln = data;
	uint8_t law[1024] = {0}, *lp = law;
	unsigned i;
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

ZINT_CODEC_FUNCTION(zint_slin2alaw)
{
	int16_t sln_buf[512] = {0}, *sln = sln_buf;
	uint8_t *lp = data;
	unsigned i;
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


ZINT_CODEC_FUNCTION(zint_alaw2slin)
{
	int16_t *sln = data;
	uint8_t law[1024] = {0}, *lp = law;
	unsigned i;
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

ZINT_CODEC_FUNCTION(zint_ulaw2alaw)
{
	zap_size_t len = *datalen;
	unsigned i;
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

ZINT_CODEC_FUNCTION(zint_alaw2ulaw)
{
	zap_size_t len = *datalen;
	unsigned i;
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

zap_status_t zap_channel_read(zap_channel_t *zchan, void *data, zap_size_t *datalen)
{
	zap_status_t status = ZAP_FAIL;
	zint_codec_t codec_func;
	zap_size_t max = *datalen;

	assert(zchan != NULL);
	assert(zchan->zint != NULL);
	assert(zchan->zint != NULL);
	
    if (!zap_test_flag(zchan, ZAP_CHANNEL_OPEN)) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "channel not open");
        return ZAP_FAIL;
    }

	if (!zchan->zint->read) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "method not implemented");
		return ZAP_FAIL;
	}

    status = zchan->zint->read(zchan, data, datalen);

	if (status == ZAP_SUCCESS && zchan->effective_codec != zchan->native_codec) {
		if (zchan->native_codec == ZAP_CODEC_ULAW && zchan->effective_codec == ZAP_CODEC_SLIN) {
			codec_func = zint_ulaw2slin;
		} else if (zchan->native_codec == ZAP_CODEC_ULAW && zchan->effective_codec == ZAP_CODEC_ALAW) {
			codec_func = zint_ulaw2alaw;
		} else if (zchan->native_codec == ZAP_CODEC_ALAW && zchan->effective_codec == ZAP_CODEC_SLIN) {
			codec_func = zint_alaw2slin;
		} else if (zchan->native_codec == ZAP_CODEC_ALAW && zchan->effective_codec == ZAP_CODEC_ULAW) {
			codec_func = zint_alaw2ulaw;
		}

		if (codec_func) {
			status = codec_func(data, max, datalen);
		} else {
			snprintf(zchan->last_error, sizeof(zchan->last_error), "codec error!");
			status = ZAP_FAIL;
		}
	}

	if (zap_test_flag(zchan, ZAP_CHANNEL_DTMF_DETECT)) {
		int16_t sln_buf[1024], *sln = sln_buf;
		zap_size_t slen;
		char digit_str[80] = "";

		if (zchan->effective_codec == ZAP_CODEC_SLIN) {
			sln = data;
			slen = *datalen / 2;
		} else {
			zap_size_t len = *datalen;
			unsigned i;
			uint8_t *lp = data;
			slen = max;
			
			if (slen > len) {
				slen = len;
			}

			if (zchan->effective_codec == ZAP_CODEC_ULAW) {
				for(i = 0; i < max; i++) {
					*sln++ = ulaw_to_linear(*lp++);
				} 
			} else if (zchan->effective_codec == ZAP_CODEC_ALAW) {
				for(i = 0; i < max; i++) {
					*sln++ = alaw_to_linear(*lp++);
				} 
			}

			sln = sln_buf;
		}

		teletone_dtmf_detect (&zchan->dtmf_detect, sln, slen);
		teletone_dtmf_get(&zchan->dtmf_detect, digit_str, sizeof(digit_str));
		if(digit_str[0]) {
			zint_event_cb_t event_callback = NULL;
			if (zchan->span->event_callback) {
				event_callback = zchan->span->event_callback;
			} else if (zchan->event_callback) {
				event_callback = zchan->event_callback;
			}

			if (event_callback) {
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

	return status;
}


zap_status_t zap_channel_write(zap_channel_t *zchan, void *data, zap_size_t *datalen)
{
	zap_status_t status = ZAP_FAIL;
	zint_codec_t codec_func;
	zap_size_t max = *datalen;

	assert(zchan != NULL);
	assert(zchan->zint != NULL);

    if (!zap_test_flag(zchan, ZAP_CHANNEL_OPEN)) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "channel not open");
        return ZAP_FAIL;
    }

	if (!zchan->zint->write) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "method not implemented");
		return ZAP_FAIL;
	}
	
	
	if (zchan->effective_codec != zchan->native_codec) {
		if (zchan->native_codec == ZAP_CODEC_ULAW && zchan->effective_codec == ZAP_CODEC_SLIN) {
			codec_func = zint_slin2ulaw;
		} else if (zchan->native_codec == ZAP_CODEC_ULAW && zchan->effective_codec == ZAP_CODEC_ALAW) {
			codec_func = zint_alaw2ulaw;
		} else if (zchan->native_codec == ZAP_CODEC_ALAW && zchan->effective_codec == ZAP_CODEC_SLIN) {
			codec_func = zint_slin2alaw;
		} else if (zchan->native_codec == ZAP_CODEC_ALAW && zchan->effective_codec == ZAP_CODEC_ULAW) {
			codec_func = zint_ulaw2alaw;
		}

		if (codec_func) {
			status = codec_func(data, max, datalen);
		} else {
			snprintf(zchan->last_error, sizeof(zchan->last_error), "codec error!");
			status = ZAP_FAIL;
		}
	}

    status = zchan->zint->write(zchan, data, datalen);

	return status;
}

zap_status_t zap_global_init(void)
{
	zap_config_t cfg;
	char *var, *val;
	unsigned configured = 0;
	zap_software_interface_t *zint;
	int modcount;

	globals.interface_hash = create_hashtable(16, hashfromstring, equalkeys);
	zint = NULL;
	modcount = 0;

#ifdef ZAP_WANPIPE_SUPPORT
	if (wanpipe_init(&zint) == ZAP_SUCCESS) {
		hashtable_insert(globals.interface_hash, (void *)zint->name, zint);
		modcount++;
	} else {
		zap_log(ZAP_LOG_ERROR, "Error initilizing wanpipe.\n");	
	}
#endif
	zint = NULL;
#ifdef ZAP_ZT_SUPPORT
	if (zt_init(&zint) == ZAP_SUCCESS) {
		hashtable_insert(globals.interface_hash, (void *)zint->name, zint);
		modcount++;
	} else {
		zap_log(ZAP_LOG_ERROR, "Error initilizing zt.\n");	
	}
#endif

	if (!modcount) {
		zap_log(ZAP_LOG_ERROR, "Error initilizing anything.\n");	
		return ZAP_FAIL;
	}

	if (!zap_config_open_file(&cfg, "openzap.conf")) {
		return ZAP_FAIL;
	}
	
	while (zap_config_next_pair(&cfg, &var, &val)) {
		if (!strcasecmp(cfg.category, "openzap")) {
			if (!strcmp(var, "load")) {
				zap_software_interface_t *zint;
		
				zint = (zap_software_interface_t *) hashtable_search(globals.interface_hash, val);
				if (zint) {
					if (zint->configure(zint) == ZAP_SUCCESS) {
						configured++;
					}
				} else {
					zap_log(ZAP_LOG_WARNING, "Attempted to load Non-Existant module '%s'\n", val);
				}
			}
		}
	}

	zap_config_close_file(&cfg);

	if (configured) {
		return ZAP_SUCCESS;
	}

	zap_log(ZAP_LOG_ERROR, "No modules configured!\n");
	return ZAP_FAIL;
}

zap_status_t zap_global_destroy(void)
{

#ifdef ZAP_ZT_SUPPORT
	zt_destroy();
#endif
#ifdef ZAP_WANPIPE_SUPPORT
	wanpipe_destroy();
#endif

	hashtable_destroy(globals.interface_hash, 0);

	return ZAP_SUCCESS;
}


unsigned zap_separate_string(char *buf, char delim, char **array, int arraylen)
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
