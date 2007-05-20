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

#define WANPIPE_TDM_API 1

#include "openzap.h"
#include "zap_wanpipe.h"

#include <sangoma_tdm_api.h>

static struct {
	unsigned codec_ms;
} wp_globals;

static zap_software_interface_t wanpipe_interface;

static zap_status_t wp_tdm_cmd_exec(zap_channel_t *zchan, wanpipe_tdm_api_t *tdm_api)
{
	int err;

	err = tdmv_api_ioctl(zchan->sockfd, tdm_api);
	
	if (err) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "%s", strerror(errno));
		return ZAP_FAIL;
	}

	return ZAP_SUCCESS;
}

static unsigned wp_open_range(zap_span_t *span, unsigned spanno, unsigned start, unsigned end, zap_chan_type_t type)
{
	unsigned configured = 0, x;

	for(x = start; x < end; x++) {
		zap_channel_t *chan;
		zap_socket_t sockfd = WP_INVALID_SOCKET;
		
		sockfd = tdmv_api_open_span_chan(spanno, x);
		
		if (sockfd != WP_INVALID_SOCKET && zap_span_add_channel(span, sockfd, type, &chan) == ZAP_SUCCESS) {
			zap_log(ZAP_LOG_INFO, "configuring device s%dc%d as OpenZAP device %d:%d fd:%d\n", spanno, x, chan->span_id, chan->chan_id, sockfd);
			configured++;
		} else {
			zap_log(ZAP_LOG_ERROR, "failure configuring device s%dc%d\n", spanno, x);
		}
	}
	
	return configured;
}

static unsigned wp_configure_channel(zap_config_t *cfg, const char *str, zap_span_t *span, zap_chan_type_t type)
{
	int items, i;
    char *mydata, *item_list[10];
	char *sp, *ch, *mx;
	int channo;
	int spanno;
	int top = 0;
	unsigned configured = 0;

	assert(str != NULL);
	

	mydata = strdup(str);
	assert(mydata != NULL);


	items = zap_separate_string(mydata, ',', item_list, (sizeof(item_list) / sizeof(item_list[0])));

	for(i = 0; i < items; i++) {
		sp = item_list[i];
		if ((ch = strchr(sp, ':'))) {
			*ch++ = '\0';
		}

		if (!(sp && ch)) {
			zap_log(ZAP_LOG_ERROR, "Invalid input on line %d\n", cfg->lineno);
			continue;
		}


		channo = atoi(ch);
		spanno = atoi(sp);


		if (channo < 0) {
			zap_log(ZAP_LOG_ERROR, "Invalid channel number %d\n", channo);
			continue;
		}

		if (spanno < 0) {
			zap_log(ZAP_LOG_ERROR, "Invalid span number %d\n", channo);
			continue;
		}
		
		if ((mx = strchr(ch, '-'))) {
			mx++;
			top = atoi(mx) + 1;
		} else {
			top = channo + 1;
		}
		
		
		if (top < 0) {
			zap_log(ZAP_LOG_ERROR, "Invalid range number %d\n", top);
			continue;
		}

		configured += wp_open_range(span, spanno, channo, top, type);

	}
	
	free(mydata);

	return configured;
}

static ZINT_CONFIGURE_FUNCTION(wanpipe_configure)
{
	zap_config_t cfg;
	char *var, *val;
	int catno = -1;
	zap_span_t *span = NULL;
	int new_span = 0;
	unsigned configured = 0, d = 0;

	ZINT_CONFIGURE_MUZZLE;

	zap_log(ZAP_LOG_DEBUG, "configuring wanpipe\n");
	if (!zap_config_open_file(&cfg, "wanpipe.conf")) {
		return ZAP_FAIL;
	}
	
	while (zap_config_next_pair(&cfg, &var, &val)) {
		if (!strcasecmp(cfg.category, "defaults")) {
			if (!strcasecmp(var, "codec_ms")) {
				unsigned codec_ms = atoi(val);
				if (codec_ms < 10 || codec_ms > 60) {
					zap_log(ZAP_LOG_WARNING, "invalid codec ms at line %d\n", cfg.lineno);
				} else {
					wp_globals.codec_ms = codec_ms;
				}
			}
		} else if (!strcasecmp(cfg.category, "span")) {
			if (cfg.catno != catno) {
				zap_log(ZAP_LOG_DEBUG, "found config for span\n");
				catno = cfg.catno;
				new_span = 1;				
				span = NULL;
			}
			
			if (new_span) {
				if (!strcasecmp(var, "enabled") && ! zap_true(val)) {
					zap_log(ZAP_LOG_DEBUG, "span (disabled)\n");
				} else {
					if (zap_span_create(&wanpipe_interface, &span) == ZAP_SUCCESS) {
						zap_log(ZAP_LOG_DEBUG, "created span %d\n", span->span_id);
					} else {
						zap_log(ZAP_LOG_CRIT, "failure creating span\n");
						span = NULL;
					}
				}
				new_span = 0;
				continue;
			}

			if (!span) {
				continue;
			}

			zap_log(ZAP_LOG_DEBUG, "span %d [%s]=[%s]\n", span->span_id, var, val);
			
			if (!strcasecmp(var, "enabled")) {
				zap_log(ZAP_LOG_WARNING, "'enabled' command ignored when it's not the first command in a [span]\n");
			} else if (!strcasecmp(var, "b-channel")) {
				configured += wp_configure_channel(&cfg, val, span, ZAP_CHAN_TYPE_B);
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
					configured += wp_configure_channel(&cfg, val, span, qtype);
					d++;
				}
			}
		}
	}
	zap_config_close_file(&cfg);

	zap_log(ZAP_LOG_INFO, "wanpipe configured %u channel(s)\n", configured);
	
	return configured ? ZAP_SUCCESS : ZAP_FAIL;
}

static ZINT_OPEN_FUNCTION(wanpipe_open) 
{

	wanpipe_tdm_api_t tdm_api;

	if (zchan->type == ZAP_CHAN_TYPE_DQ921 || zchan->type == ZAP_CHAN_TYPE_DQ931) {
		zchan->native_codec = zchan->effective_codec = ZAP_CODEC_NONE;
	} else {
		tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_SET_CODEC;
		tdm_api.wp_tdm_cmd.tdm_codec = WP_NONE;
		wp_tdm_cmd_exec(zchan, &tdm_api);

		tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_SET_USR_PERIOD;
		tdm_api.wp_tdm_cmd.usr_period = wp_globals.codec_ms;
		wp_tdm_cmd_exec(zchan, &tdm_api);
		zap_channel_set_feature(zchan, ZAP_CHANNEL_FEATURE_INTERVAL);
		zchan->effective_interval = zchan->native_interval = wp_globals.codec_ms;
		zchan->packet_len = zchan->native_interval * 8;
		
		tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_GET_HW_CODING;
		if (tdm_api.wp_tdm_cmd.hw_tdm_coding) {
			zchan->native_codec = zchan->effective_codec = ZAP_CODEC_ULAW;
		} else {
			zchan->native_codec = zchan->effective_codec = ZAP_CODEC_ALAW;
		}
	}

	return ZAP_SUCCESS;
}

static ZINT_CLOSE_FUNCTION(wanpipe_close)
{
	ZINT_CLOSE_MUZZLE;
	return ZAP_SUCCESS;
}

static ZINT_COMMAND_FUNCTION(wanpipe_command)
{
	wanpipe_tdm_api_t tdm_api;
	int err = 0;

	ZINT_COMMAND_MUZZLE;
	
	memset(&tdm_api, 0, sizeof(tdm_api));
	
	switch(command) {
	case ZAP_COMMAND_GET_INTERVAL:
		{
			tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_GET_USR_PERIOD;

			if (!(err = wp_tdm_cmd_exec(zchan, &tdm_api))) {
				ZAP_COMMAND_OBJ_INT = tdm_api.wp_tdm_cmd.usr_period;
			}

		}
		break;
	case ZAP_COMMAND_SET_INTERVAL: 
		{
			tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_SET_USR_PERIOD;
			tdm_api.wp_tdm_cmd.usr_period = ZAP_COMMAND_OBJ_INT;
			err = wp_tdm_cmd_exec(zchan, &tdm_api);
			zchan->packet_len = zchan->native_interval * (zchan->effective_codec == ZAP_CODEC_SLIN ? 16 : 8);
		}
		break;
	default:
		break;
	};

	if (err) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "%s", strerror(errno));
		return ZAP_FAIL;
	}


	return ZAP_SUCCESS;
}

static ZINT_READ_FUNCTION(wanpipe_read)
{
	int rx_len = 0;
	wp_tdm_api_rx_hdr_t hdrframe;

	memset(&hdrframe, 0, sizeof(hdrframe));

	rx_len = tdmv_api_readmsg_tdm(zchan->sockfd, &hdrframe, (int)sizeof(hdrframe), data, (int)*datalen);
	
	*datalen = rx_len;

	if (rx_len <= 0) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "%s", strerror(errno));
		return ZAP_FAIL;
	}

	return ZAP_SUCCESS;
}

static ZINT_WRITE_FUNCTION(wanpipe_write)
{
	int bsent;
	wp_tdm_api_tx_hdr_t hdrframe;

	/* Do we even need the headerframe here? on windows, we don't even pass it to the driver */
	memset(&hdrframe, 0, sizeof(hdrframe));
	bsent = tdmv_api_writemsg_tdm(zchan->sockfd, &hdrframe, (int)sizeof(hdrframe), data, (unsigned short)(*datalen));

	/* should we be checking if bsent == *datalen here? */
	if (bsent > 0) {
		*datalen = bsent;
		return ZAP_SUCCESS;
	}

	return ZAP_FAIL;
}


static ZINT_WAIT_FUNCTION(wanpipe_wait)
{
	zap_wait_flag_t inflags = *flags;
	int result;
	
	result = tdmv_api_wait_socket(zchan->sockfd, to, inflags);

	*flags = ZAP_NO_FLAGS;

	if(result < 0){
		snprintf(zchan->last_error, sizeof(zchan->last_error), "Poll failed");
		return ZAP_FAIL;
	}

	if (result == 0) {
		return ZAP_TIMEOUT;
	}

	if (result & POLLIN) {
		*flags |= ZAP_READ;
	}

	if (result & POLLOUT) {
		*flags |= ZAP_WRITE;
	}

	if (result & POLLPRI) {
		*flags |= ZAP_ERROR;
	}

	return ZAP_SUCCESS;
}

zap_status_t wanpipe_init(zap_software_interface_t **zint)
{
	assert(zint != NULL);
	memset(&wanpipe_interface, 0, sizeof(wanpipe_interface));

	wp_globals.codec_ms = 20;
	wanpipe_interface.name = "wanpipe";
	wanpipe_interface.configure =  wanpipe_configure;
	wanpipe_interface.open = wanpipe_open;
	wanpipe_interface.close = wanpipe_close;
	wanpipe_interface.command = wanpipe_command;
	wanpipe_interface.wait = wanpipe_wait;
	wanpipe_interface.read = wanpipe_read;
	wanpipe_interface.write = wanpipe_write;
	*zint = &wanpipe_interface;

	return ZAP_SUCCESS;
}

zap_status_t wanpipe_destroy(void)
{
	unsigned int i,j;

	for(i = 1; i <= wanpipe_interface.span_index; i++) {
		zap_span_t *cur_span = &wanpipe_interface.spans[i];

		if (zap_test_flag(cur_span, ZAP_SPAN_CONFIGURED)) {
			for(j = 1; j <= cur_span->chan_count; j++) {
				zap_channel_t *cur_chan = &cur_span->channels[j];
				if (zap_test_flag(cur_chan, ZAP_CHANNEL_CONFIGURED)) {
					zap_log(ZAP_LOG_INFO, "Closing channel %u:%u fd:%d\n", cur_chan->span_id, cur_chan->chan_id, cur_chan->sockfd);
					tdmv_api_close_socket(&cur_chan->sockfd);
				}
			}
		}
	}

	memset(&wanpipe_interface, 0, sizeof(wanpipe_interface));
	
	return ZAP_SUCCESS;
}
