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
	uint32_t codec_ms;
	uint32_t wink_ms;
	uint32_t flash_ms;
} wp_globals;

static zap_io_interface_t wanpipe_interface;

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

static unsigned wp_open_range(zap_span_t *span, unsigned spanno, unsigned start, unsigned end, zap_chan_type_t type, char *name, char *number)
{
	unsigned configured = 0, x;

	for(x = start; x < end; x++) {
		zap_channel_t *chan;
		zap_socket_t sockfd = WP_INVALID_SOCKET;
		
		sockfd = tdmv_api_open_span_chan(spanno, x);
		
		if (sockfd != WP_INVALID_SOCKET && zap_span_add_channel(span, sockfd, type, &chan) == ZAP_SUCCESS) {
			zap_log(ZAP_LOG_INFO, "configuring device s%dc%d as OpenZAP device %d:%d fd:%d\n", spanno, x, chan->span_id, chan->chan_id, sockfd);
			if (type == ZAP_CHAN_TYPE_FXS || type == ZAP_CHAN_TYPE_FXO) {
				wanpipe_tdm_api_t tdm_api;

				tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_ENABLE_RXHOOK_EVENTS;
				wp_tdm_cmd_exec(chan, &tdm_api);

				if (type == ZAP_CHAN_TYPE_FXS) {
					tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_ENABLE_RING_DETECT_EVENTS;
					wp_tdm_cmd_exec(chan, &tdm_api);
				}

				tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_GET_HW_CODING;
				wp_tdm_cmd_exec(chan, &tdm_api);
				if (tdm_api.wp_tdm_cmd.hw_tdm_coding) {
					chan->native_codec = chan->effective_codec = ZAP_CODEC_ALAW;
				} else {
					chan->native_codec = chan->effective_codec = ZAP_CODEC_ULAW;
				}
			}
			if (!zap_strlen_zero(name)) {
				zap_copy_string(chan->chan_name, name, sizeof(chan->chan_name));
			}
			if (!zap_strlen_zero(number)) {
				zap_copy_string(chan->chan_number, number, sizeof(chan->chan_number));
			}
			configured++;
		} else {
			zap_log(ZAP_LOG_ERROR, "failure configuring device s%dc%d\n", spanno, x);
		}
	}
	
	return configured;
}

static ZIO_CONFIGURE_FUNCTION(wanpipe_configure)
{
	int num;

	if (!strcasecmp(category, "defaults")) {
		if (!strcasecmp(var, "codec_ms")) {
			num = atoi(val);
			if (num < 10 || num > 60) {
				zap_log(ZAP_LOG_WARNING, "invalid codec ms at line %d\n", lineno);
			} else {
				wp_globals.codec_ms = num;
			}
		} else if (!strcasecmp(var, "wink_ms")) {
			num = atoi(val);
			if (num < 500 || num > 2000) {
				zap_log(ZAP_LOG_WARNING, "invalid wink ms at line %d\n", lineno);
			} else {
				wp_globals.wink_ms = num;
			}
		} else if (!strcasecmp(var, "flash_ms")) {
			num = atoi(val);
			if (num < 500 || num > 2000) {
				zap_log(ZAP_LOG_WARNING, "invalid flash ms at line %d\n", lineno);
			} else {
				wp_globals.flash_ms = num;
			}
		}
	}

	return ZAP_SUCCESS;
}

static ZIO_CONFIGURE_SPAN_FUNCTION(wanpipe_configure_span)
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
			zap_log(ZAP_LOG_ERROR, "Invalid input\n");
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

		configured += wp_open_range(span, spanno, channo, top, type, name, number);

	}
	
	free(mydata);

	return configured;
}

static ZIO_OPEN_FUNCTION(wanpipe_open) 
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
		zchan->native_codec = zchan->effective_codec;

	}

	return ZAP_SUCCESS;
}

static ZIO_CLOSE_FUNCTION(wanpipe_close)
{
	return ZAP_SUCCESS;
}

static ZIO_COMMAND_FUNCTION(wanpipe_command)
{
	wanpipe_tdm_api_t tdm_api;
	int err = 0;

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

static ZIO_READ_FUNCTION(wanpipe_read)
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

static ZIO_WRITE_FUNCTION(wanpipe_write)
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


static ZIO_WAIT_FUNCTION(wanpipe_wait)
{
	int32_t inflags = 0;
	int result;

	if (*flags & ZAP_READ) {
		inflags |= POLLIN;
	}

	if (*flags & ZAP_WRITE) {
		inflags |= POLLOUT;
	}

	if (*flags & ZAP_EVENTS) {
		inflags |= POLLPRI;
	}

	result = tdmv_api_wait_socket(zchan->sockfd, to, &inflags);

	*flags = ZAP_NO_FLAGS;

	if (result < 0){
		snprintf(zchan->last_error, sizeof(zchan->last_error), "Poll failed");
		return ZAP_FAIL;
	}

	if (result == 0) {
		return ZAP_TIMEOUT;
	}

	if (inflags & POLLIN) {
		*flags |= ZAP_READ;
	}

	if (inflags & POLLOUT) {
		*flags |= ZAP_WRITE;
	}

	if (inflags & POLLPRI) {
		*flags |= ZAP_EVENTS;
	}

	return ZAP_SUCCESS;
}

#ifndef WIN32
ZIO_SPAN_POLL_EVENT_FUNCTION(wanpipe_poll_event)
{
	struct pollfd pfds[ZAP_MAX_CHANNELS_SPAN];
	int i, j = 0, k = 0, l = 0, r;
	
	for(i = 1; i <= span->chan_count; i++) {
		memset(&pfds[j], 0, sizeof(pfds[j]));
		pfds[j].fd = span->channels[i].sockfd;
		pfds[j].events = POLLPRI;
		if (zap_test_flag((&span->channels[i]), ZAP_CHANNEL_WINK) || zap_test_flag((&span->channels[i]), ZAP_CHANNEL_FLASH)) {
			l++;
		}
		j++;
	}

	if (l) {
		ms = 5;
	}
	
    r = poll(pfds, j, ms);
	
	if (r == 0) {
		return l ? ZAP_SUCCESS : ZAP_TIMEOUT;
	} else if (r < 0) {
		snprintf(span->last_error, sizeof(span->last_error), "%s", strerror(errno));
		return ZAP_FAIL;
	}
	
	for(i = 1; i <= span->chan_count; i++) {
		if (pfds[i-1].revents & POLLPRI) {
			zap_set_flag((&span->channels[i]), ZAP_CHANNEL_EVENT);
			span->channels[i].last_event_time = zap_current_time_in_ms();
			k++;
		}
	}
	
	return k ? ZAP_SUCCESS : ZAP_FAIL;
}

#endif

ZIO_SPAN_NEXT_EVENT_FUNCTION(wanpipe_next_event)
{
	uint32_t i;
	zap_oob_event_t event_id;
	
	for(i = 1; i <= span->chan_count; i++) {
		if (span->channels[i].last_event_time && !zap_test_flag((&span->channels[i]), ZAP_CHANNEL_EVENT)) {
			uint32_t diff = (uint32_t)(zap_current_time_in_ms() - span->channels[i].last_event_time);
			/* XX printf("%u %u %u\n", diff, (unsigned)zap_current_time_in_ms(), (unsigned)span->channels[i].last_event_time); */
			if (zap_test_flag((&span->channels[i]), ZAP_CHANNEL_WINK)) {
				if (diff > wp_globals.wink_ms) {
					zap_clear_flag_locked((&span->channels[i]), ZAP_CHANNEL_WINK);
					zap_clear_flag_locked((&span->channels[i]), ZAP_CHANNEL_FLASH);
					event_id = ZAP_OOB_OFFHOOK;
					goto event;
				}
			}

			if (zap_test_flag((&span->channels[i]), ZAP_CHANNEL_FLASH)) {
				if (diff > wp_globals.flash_ms) {
					zap_clear_flag_locked((&span->channels[i]), ZAP_CHANNEL_FLASH);
					zap_clear_flag_locked((&span->channels[i]), ZAP_CHANNEL_WINK);
					event_id = ZAP_OOB_ONHOOK;
					goto event;
				}
			}
		}
		if (zap_test_flag((&span->channels[i]), ZAP_CHANNEL_EVENT)) {
			wanpipe_tdm_api_t tdm_api;
			zap_clear_flag((&span->channels[i]), ZAP_CHANNEL_EVENT);

			tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_READ_EVENT;
			if (wp_tdm_cmd_exec(&span->channels[i], &tdm_api) != ZAP_SUCCESS) {
				snprintf(span->last_error, sizeof(span->last_error), "%s", strerror(errno));
				return ZAP_FAIL;
			}
			
			switch(tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_type) {
			case WP_TDM_EVENT_RXHOOK:
				{
					event_id = tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_rxhook_state & WP_TDMAPI_EVENT_RXHOOK_OFF ? ZAP_OOB_OFFHOOK : ZAP_OOB_ONHOOK;
					
					if (event_id == ZAP_OOB_OFFHOOK) {
						if (zap_test_flag((&span->channels[i]), ZAP_CHANNEL_FLASH)) {
							zap_clear_flag_locked((&span->channels[i]), ZAP_CHANNEL_FLASH);
							zap_clear_flag_locked((&span->channels[i]), ZAP_CHANNEL_WINK);
							event_id = ZAP_OOB_FLASH;
							goto event;
						} else {
							zap_set_flag_locked((&span->channels[i]), ZAP_CHANNEL_WINK);
						}
					} else {
						if (zap_test_flag((&span->channels[i]), ZAP_CHANNEL_WINK)) {
							zap_clear_flag_locked((&span->channels[i]), ZAP_CHANNEL_WINK);
							zap_clear_flag_locked((&span->channels[i]), ZAP_CHANNEL_FLASH);
							event_id = ZAP_OOB_WINK;
							goto event;
						} else {
							zap_set_flag_locked((&span->channels[i]), ZAP_CHANNEL_FLASH);
						}
					}
					continue;
				}
				break;
			case WP_TDM_EVENT_RING_DETECT:
				{
					event_id = tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_ring_state & WP_TDMAPI_EVENT_RING_PRESENT ? ZAP_OOB_RING_START : ZAP_OOB_RING_STOP;
					tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_DISABLE_RING_DETECT_EVENTS;
					wp_tdm_cmd_exec(&span->channels[i], &tdm_api);
				}
				break;
			default:
				{
					zap_log(ZAP_LOG_WARNING, "Unhandled event %d\n", tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_type);
					event_id = ZAP_OOB_INVALID;
				}
				break;
			}

		event:
			span->channels[i].last_event_time = 0;
			span->event_header.e_type = ZAP_EVENT_OOB;
			span->event_header.enum_id = event_id;
			span->event_header.channel = &span->channels[i];
			*event = &span->event_header;
			return ZAP_SUCCESS;
		}
	}

	return ZAP_FAIL;
	
}

static ZIO_DESTROY_CHANNEL_FUNCTION(wanpipe_destroy_channel)
{
	tdmv_api_close_socket(&zchan->sockfd);
	zchan->sockfd = WP_INVALID_SOCKET;

	return ZAP_SUCCESS;
}

zap_status_t wanpipe_init(zap_io_interface_t **zio)
{
	assert(zio != NULL);
	memset(&wanpipe_interface, 0, sizeof(wanpipe_interface));

	wp_globals.codec_ms = 20;
	wp_globals.wink_ms = 150;
	wp_globals.flash_ms = 750;
	wanpipe_interface.name = "wanpipe";
	wanpipe_interface.configure_span = wanpipe_configure_span;
	wanpipe_interface.configure = wanpipe_configure;
	wanpipe_interface.open = wanpipe_open;
	wanpipe_interface.close = wanpipe_close;
	wanpipe_interface.command = wanpipe_command;
	wanpipe_interface.wait = wanpipe_wait;
	wanpipe_interface.read = wanpipe_read;
	wanpipe_interface.write = wanpipe_write;
#ifndef WIN32
	wanpipe_interface.poll_event = wanpipe_poll_event;
#endif
	wanpipe_interface.next_event = wanpipe_next_event;
	wanpipe_interface.destroy_channel = wanpipe_destroy_channel;
	*zio = &wanpipe_interface;

	return ZAP_SUCCESS;
}

zap_status_t wanpipe_destroy(void)
{
	memset(&wanpipe_interface, 0, sizeof(wanpipe_interface));
	return ZAP_SUCCESS;
}
