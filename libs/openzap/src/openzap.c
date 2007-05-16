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
#ifdef ZAP_WANPIPE_SUPPORT
#include "zap_wanpipe.h"
#endif
#ifdef ZAP_ZT_SUPPORT
#include "zap_zt.h"
#endif

static struct {
	zap_hash_t *interface_hash;
} globals;


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
		zap_set_flag(new_chan, ZAP_CHANNEL_CONFIGURED | ZAP_CHANNEL_READY);
		*chan = new_chan;
		return ZAP_SUCCESS;
	}

	return ZAP_FAIL;
}

zap_status_t zap_span_destroy(zap_span_t **span);

zap_status_t zap_channel_open(const char *name, unsigned span_id, unsigned chan_id, zap_channel_t **zchan)
{
	zap_software_interface_t *zint;

	if (span_id < ZAP_MAX_SPANS_INTERFACE && chan_id < ZAP_MAX_CHANNELS_SPAN && 
		(zint = (zap_software_interface_t *) hashtable_search(globals.interface_hash, (void *)name))) {
		zap_channel_t *check;
		check = &zint->spans[span_id].channels[chan_id];
		if (zap_test_flag(check, ZAP_CHANNEL_READY) && ! zap_test_flag(check, ZAP_CHANNEL_OPEN)) {
			zap_set_flag(check, ZAP_CHANNEL_OPEN);
			*zchan = check;
			return ZAP_SUCCESS;
		}
	}

	return ZAP_FAIL;
}

zap_status_t zap_channel_close(zap_channel_t **zchan)
{
	zap_channel_t *check;
	*zchan = NULL;
	
	assert(zchan != NULL);
	check = *zchan;
	assert(check != NULL);
	
	if (zap_test_flag(check, ZAP_CHANNEL_OPEN)) {
		zap_clear_flag(check, ZAP_CHANNEL_OPEN);
		return ZAP_SUCCESS;
	}
	
	return ZAP_FAIL;
}


zap_status_t zap_channel_set_codec(zap_channel_t *zchan, zap_codec_t codec)
{
	assert(zchan != NULL);

	if (!zap_test_flag(zchan, ZAP_CHANNEL_OPEN)) {
		return ZAP_FAIL;
	}

	return zchan->zint->set_codec(zchan, codec);
	
}

zap_status_t zap_channel_set_interval(zap_channel_t *zchan, unsigned ms)
{
	assert(zchan != NULL);

    if (!zap_test_flag(zchan, ZAP_CHANNEL_OPEN)) {
        return ZAP_FAIL;
    }

    return zchan->zint->set_interval(zchan, ms);

}

zap_status_t zap_channel_wait(zap_channel_t *zchan, zap_wait_flag_t flags)
{
	assert(zchan != NULL);

    if (!zap_test_flag(zchan, ZAP_CHANNEL_OPEN)) {
        return ZAP_FAIL;
    }

    return zchan->zint->wait(zchan, flags);

}


zap_status_t zap_channel_read(zap_channel_t *zchan, void *data, zap_size_t *datalen)
{
	assert(zchan != NULL);

    if (!zap_test_flag(zchan, ZAP_CHANNEL_OPEN)) {
        return ZAP_FAIL;
    }

    return zchan->zint->read(zchan, data, datalen);
	
}


zap_status_t zap_channel_write(zap_channel_t *zchan, void *data, zap_size_t *datalen)
{
	assert(zchan != NULL);

    if (!zap_test_flag(zchan, ZAP_CHANNEL_OPEN)) {
        return ZAP_FAIL;
    }

    return zchan->zint->write(zchan, data, datalen);
	
}

zap_status_t zap_global_init(void)
{
	zap_config_t cfg;
	char *var, *val;
	unsigned configured = 0;
	zap_software_interface_t *zint;
	
	assert(zint = NULL);
	
	globals.interface_hash = create_hashtable(16, hashfromstring, equalkeys);

#ifdef ZAP_WANPIPE_SUPPORT
	if (wanpipe_init(&zint) == ZAP_SUCCESS) {
		hashtable_insert(globals.interface_hash, (void *)zint->name, zint);
	}
#endif
#ifdef ZAP_ZT_SUPPORT
	if (zt_init(&zint) == ZAP_SUCCESS) {
		hashtable_insert(globals.interface_hash, (void *)zint->name, zint);
	}
#endif

	
	if (!zap_config_open_file(&cfg, "openzap.conf")) {
		return ZAP_FAIL;
	}

	while (zap_config_next_pair(&cfg, &var, &val)) {
		if (!strcasecmp(cfg.category, "openzap")) {
			if (!strcmp(var, "load")) {
				zap_software_interface_t *zint;
				
				if ((zint = (zap_software_interface_t *) hashtable_search(globals.interface_hash, val))) {
					if (zint->configure(zint) == ZAP_SUCCESS) {
						configured++;
					}
				}
			}
		}
	}

	return configured ? ZAP_SUCCESS : ZAP_FAIL;
}

zap_status_t zap_global_destroy(void)
{
	hashtable_destroy(globals.interface_hash, 1);

#ifdef ZAP_ZT_SUPPORT
	zt_destroy();
#endif
#ifdef ZAP_WANPIPE_SUPPORT
	wanpipe_destroy();
#endif
	return ZAP_SUCCESS;
}
