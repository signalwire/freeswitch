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

#ifdef __sun
#include <unistd.h>
#include <stropts.h>
#endif
#include "openzap.h"
#include <poll.h>
#include <sys/socket.h>
#include "libsangoma.h"

typedef enum {
	WP_RINGING = (1 << 0)
} wp_flag_t;

static struct {
	uint32_t codec_ms;
	uint32_t wink_ms;
	uint32_t flash_ms;
	uint32_t ring_on_ms;
	uint32_t ring_off_ms;
} wp_globals;

/* a bunch of this stuff should go into the wanpipe_tdm_api_iface.h */

ZIO_SPAN_POLL_EVENT_FUNCTION(wanpipe_poll_event);
ZIO_SPAN_NEXT_EVENT_FUNCTION(wanpipe_next_event);

#define WP_INVALID_SOCKET -1 

/* a cross platform way to poll on an actual pollset (span and/or list of spans) will probably also be needed for analog */
/* so we can have one analong handler thread that will deal with all the idle analog channels for events */
/* the alternative would be for the driver to provide one socket for all of the oob events for all analog channels */
static __inline__ int tdmv_api_wait_socket(sng_fd_t fd, int timeout, int *flags)
{
	
#ifdef LIBSANGOMA_VERSION
	int err;
	sangoma_wait_obj_t sangoma_wait_obj;

 	sangoma_init_wait_obj(&sangoma_wait_obj, fd, 1, 1, *flags, SANGOMA_WAIT_OBJ);

	err=sangoma_socket_waitfor_many(&sangoma_wait_obj,1 , timeout);
	if (err > 0) {
		*flags=sangoma_wait_obj.flags_out;
	}
	return err;

#else
 	struct pollfd pfds[1];
    int res;

    memset(&pfds[0], 0, sizeof(pfds[0]));
    pfds[0].fd = fd;
    pfds[0].events = *flags;
    res = poll(pfds, 1, timeout);
	*flags = 0;

	if (pfds[0].revents & POLLERR) {
		res = -1;
	}

	if (res > 0) {
		*flags = pfds[0].revents;
	}

    return res;
#endif
	
}

static __inline__ sng_fd_t tdmv_api_open_span_chan(int span, int chan) 
{
	return sangoma_open_tdmapi_span_chan(span, chan);
}            



static zap_io_interface_t wanpipe_interface;

static unsigned char wanpipe_swap_bits(unsigned char cas_bits)
{
	unsigned char swapped_bits = 0x0;
	if (cas_bits & 0x8) {
		swapped_bits |= 0x1;
	}
	if (cas_bits & 0x4) {
		swapped_bits |= 0x2;
	}
	if (cas_bits & 0x2) {
		swapped_bits |= 0x4;
	}
	if (cas_bits & 0x1) {
		swapped_bits |= 0x8;
	}
	return swapped_bits;
}

static unsigned wp_open_range(zap_span_t *span, unsigned spanno, unsigned start, unsigned end, zap_chan_type_t type, char *name, char *number, unsigned char cas_bits)
{
	unsigned configured = 0, x;

	if (type == ZAP_CHAN_TYPE_CAS) {
		zap_log(ZAP_LOG_DEBUG, "Configuring CAS channels with abcd == 0x%X\n", cas_bits);
	}	
	for(x = start; x < end; x++) {
		zap_channel_t *chan;
		zap_socket_t sockfd = WP_INVALID_SOCKET;
		const char *dtmf = "none";

		sockfd = tdmv_api_open_span_chan(spanno, x);
		
		if (sockfd != WP_INVALID_SOCKET && zap_span_add_channel(span, sockfd, type, &chan) == ZAP_SUCCESS) {
			wanpipe_tdm_api_t tdm_api;
			memset(&tdm_api,0,sizeof(tdm_api));
			
			chan->physical_span_id = spanno;
			chan->physical_chan_id = x;
			chan->rate = 8000;

			if (type == ZAP_CHAN_TYPE_FXS || type == ZAP_CHAN_TYPE_FXO || type == ZAP_CHAN_TYPE_B) {
				int err;
				
				dtmf = "software";

				/* FIXME: Handle Error Conditino Check for return code */
				err= sangoma_tdm_get_hw_coding(chan->sockfd, &tdm_api);

				if (tdm_api.wp_tdm_cmd.hw_tdm_coding) {
					chan->native_codec = chan->effective_codec = ZAP_CODEC_ALAW;
				} else {
					chan->native_codec = chan->effective_codec = ZAP_CODEC_ULAW;
				}

				err = sangoma_tdm_get_hw_dtmf(chan->sockfd, &tdm_api);
				if (err > 0) {
					err = sangoma_tdm_enable_dtmf_events(chan->sockfd, &tdm_api);
					if (err == 0) {
						zap_channel_set_feature(chan, ZAP_CHANNEL_FEATURE_DTMF_DETECT);
						dtmf = "hardware";
					}
				}
			}

#if 0
            if (type == ZAP_CHAN_TYPE_FXS || type == ZAP_CHAN_TYPE_FXO) {
                /* Enable FLASH/Wink Events */
                int err=sangoma_set_rm_rxflashtime(chan->sockfd, &tdm_api, wp_globals.flash_ms);
                if (err == 0) {
			        zap_log(ZAP_LOG_ERROR, "flash enabled s%dc%d\n", spanno, x);
                } else {
			        zap_log(ZAP_LOG_ERROR, "flash disabled s%dc%d\n", spanno, x);
                }
            }
#endif

			if (type == ZAP_CHAN_TYPE_CAS) {
#ifdef LIBSANGOMA_VERSION
				sangoma_tdm_write_rbs(chan->sockfd,&tdm_api,chan->physical_chan_id,wanpipe_swap_bits(cas_bits));
#else
				sangoma_tdm_write_rbs(chan->sockfd,&tdm_api,wanpipe_swap_bits(cas_bits));
#endif
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

		zap_log(ZAP_LOG_INFO, "configuring device s%dc%d as OpenZAP device %d:%d fd:%d DTMF: %s\n", 
				spanno, x, chan->span_id, chan->chan_id, sockfd, dtmf);
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
			if (num < 50 || num > 3000) {
				zap_log(ZAP_LOG_WARNING, "invalid wink ms at line %d\n", lineno);
			} else {
				wp_globals.wink_ms = num;
			}
		} else if (!strcasecmp(var, "flash_ms")) {
			num = atoi(val);
			if (num < 50 || num > 3000) {
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
	unsigned char cas_bits = 0;
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
		if (ZAP_CHAN_TYPE_CAS == type && zap_config_get_cas_bits(ch, &cas_bits)) {
			zap_log(ZAP_LOG_ERROR, "Failed to get CAS bits in CAS channel\n");
			continue;
		}
		configured += wp_open_range(span, spanno, channo, top, type, name, number, cas_bits);

	}
	
	free(mydata);

	return configured;
}

static ZIO_OPEN_FUNCTION(wanpipe_open) 
{

	wanpipe_tdm_api_t tdm_api;

	memset(&tdm_api,0,sizeof(tdm_api));

	if (zchan->type == ZAP_CHAN_TYPE_DQ921 || zchan->type == ZAP_CHAN_TYPE_DQ931) {
		zchan->native_codec = zchan->effective_codec = ZAP_CODEC_NONE;
	} else {
		zchan->effective_codec = zchan->native_codec;
		
		sangoma_tdm_set_usr_period(zchan->sockfd, &tdm_api, wp_globals.codec_ms);

		zap_channel_set_feature(zchan, ZAP_CHANNEL_FEATURE_INTERVAL);
		zchan->effective_interval = zchan->native_interval = wp_globals.codec_ms;
		zchan->packet_len = zchan->native_interval * 8;
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
	case ZAP_COMMAND_OFFHOOK:
		{
			err=sangoma_tdm_txsig_offhook(zchan->sockfd,&tdm_api);
			if (err) {
				snprintf(zchan->last_error, sizeof(zchan->last_error), "OFFHOOK Failed");
				return ZAP_FAIL;
			}
			zap_set_flag_locked(zchan, ZAP_CHANNEL_OFFHOOK);
		}
		break;
	case ZAP_COMMAND_ONHOOK:
		{
			err=sangoma_tdm_txsig_onhook(zchan->sockfd,&tdm_api);
			if (err) {
				snprintf(zchan->last_error, sizeof(zchan->last_error), "ONHOOK Failed");
				return ZAP_FAIL;
			}
			zap_clear_flag_locked(zchan, ZAP_CHANNEL_OFFHOOK);
		}
		break;
	case ZAP_COMMAND_GENERATE_RING_ON:
		{
			err=sangoma_tdm_txsig_start(zchan->sockfd,&tdm_api);
			if (err) {
				snprintf(zchan->last_error, sizeof(zchan->last_error), "Ring Failed");
				return ZAP_FAIL;
			}
			zap_set_flag_locked(zchan, ZAP_CHANNEL_RINGING);
			zap_set_pflag_locked(zchan, WP_RINGING);
			zchan->ring_time = zap_current_time_in_ms() + wp_globals.ring_on_ms;
		}
		break;
	case ZAP_COMMAND_GENERATE_RING_OFF:
		{
			err=sangoma_tdm_txsig_offhook(zchan->sockfd,&tdm_api);
			if (err) {
				snprintf(zchan->last_error, sizeof(zchan->last_error), "Ring-off Failed");
				return ZAP_FAIL;
			}
			zap_clear_pflag_locked(zchan, WP_RINGING);
			zap_clear_flag_locked(zchan, ZAP_CHANNEL_RINGING);
		}
		break;
	case ZAP_COMMAND_GET_INTERVAL:
		{
			err=sangoma_tdm_get_usr_period(zchan->sockfd, &tdm_api);
			if (err > 0 ) {
				ZAP_COMMAND_OBJ_INT = err;
				err=0;
			}
		}
		break;
	case ZAP_COMMAND_SET_INTERVAL: 
		{
			err=sangoma_tdm_set_usr_period(zchan->sockfd, &tdm_api, ZAP_COMMAND_OBJ_INT);
			zchan->packet_len = zchan->native_interval * (zchan->effective_codec == ZAP_CODEC_SLIN ? 16 : 8);
		}
		break;
	case ZAP_COMMAND_SET_CAS_BITS:
		{
#ifdef LIBSANGOMA_VERSION
			err=sangoma_tdm_write_rbs(zchan->sockfd,&tdm_api,zchan->physical_chan_id,wanpipe_swap_bits(ZAP_COMMAND_OBJ_INT));
#else
			err=sangoma_tdm_write_rbs(zchan->sockfd,&tdm_api,wanpipe_swap_bits(ZAP_COMMAND_OBJ_INT));
#endif
		}
		break;
	case ZAP_COMMAND_GET_CAS_BITS:
		{
			/* wanpipe does not has a command to get the CAS bits so we emulate it */
			ZAP_COMMAND_OBJ_INT = zchan->cas_bits;
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

	rx_len = sangoma_readmsg_tdm(zchan->sockfd, &hdrframe, (int)sizeof(hdrframe), data, (int)*datalen,0);
	*datalen = rx_len;

	if (rx_len == 0 || rx_len == -17) {
		return ZAP_TIMEOUT;
	}

	if (rx_len < 0) {
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
	bsent = sangoma_writemsg_tdm(zchan->sockfd, &hdrframe, (int)sizeof(hdrframe), data, (unsigned short)(*datalen),0);

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

ZIO_SPAN_POLL_EVENT_FUNCTION(wanpipe_poll_event)
{
#ifdef LIBSANGOMA_VERSION
	sangoma_wait_obj_t pfds[ZAP_MAX_CHANNELS_SPAN];
#else
	struct pollfd pfds[ZAP_MAX_CHANNELS_SPAN];
#endif

	uint32_t i, j = 0, k = 0, l = 0;
	int objects=0;
	int r;
	
	for(i = 1; i <= span->chan_count; i++) {
		zap_channel_t *zchan = span->channels[i];

#ifdef LIBSANGOMA_VERSION
 		sangoma_init_wait_obj(&pfds[j], zchan->sockfd , 1, 1, POLLPRI, SANGOMA_WAIT_OBJ);
#else
		memset(&pfds[j], 0, sizeof(pfds[j]));
		pfds[j].fd = span->channels[i]->sockfd;
		pfds[j].events = POLLPRI;
#endif
		objects++;
		/* The driver probably should be able to do this wink/flash/ringing by itself this is sort of a hack to make it work! */

		if (zap_test_flag(zchan, ZAP_CHANNEL_WINK) || zap_test_flag(zchan, ZAP_CHANNEL_FLASH)) {
			l = 5;
		}

		j++;

		if (zap_test_flag(zchan, ZAP_CHANNEL_RINGING)) {
			l = 5;
		}

		if (zap_test_flag(zchan, ZAP_CHANNEL_RINGING) && zap_current_time_in_ms() >= zchan->ring_time) {
			wanpipe_tdm_api_t tdm_api;
			int err;
			memset(&tdm_api, 0, sizeof(tdm_api));
			if (zap_test_pflag(zchan, WP_RINGING)) {
				err=sangoma_tdm_txsig_offhook(zchan->sockfd,&tdm_api);
				if (err) {
					snprintf(zchan->last_error, sizeof(zchan->last_error), "Ring-off Failed");
					return ZAP_FAIL;
				}
				zap_clear_pflag_locked(zchan, WP_RINGING);
				zchan->ring_time = zap_current_time_in_ms() + wp_globals.ring_off_ms;
			} else {
				err=sangoma_tdm_txsig_start(zchan->sockfd,&tdm_api);
				if (err) {
					snprintf(zchan->last_error, sizeof(zchan->last_error), "Ring Failed");
					return ZAP_FAIL;
				}
				zap_set_pflag_locked(zchan, WP_RINGING);
				zchan->ring_time = zap_current_time_in_ms() + wp_globals.ring_on_ms;
			}
		}
	}

	if (l) {
		ms = l;
	}
#ifdef LIBSANGOMA_VERSION
	r = sangoma_socket_waitfor_many(pfds,objects,ms);
#else
	r = poll(pfds, j, ms);
#endif
	
	if (r == 0) {
		return l ? ZAP_SUCCESS : ZAP_TIMEOUT;
	} else if (r < 0) {
		snprintf(span->last_error, sizeof(span->last_error), "%s", strerror(errno));
		return ZAP_FAIL;
	}
	
	for(i = 1; i <= span->chan_count; i++) {
		zap_channel_t *zchan = span->channels[i];

#ifdef LIBSANGOMA_VERSION
		if (pfds[i-1].flags_out & POLLPRI) {
#else
		if (pfds[i-1].revents & POLLPRI) {
#endif
			zap_set_flag(zchan, ZAP_CHANNEL_EVENT);
			zchan->last_event_time = zap_current_time_in_ms();
			k++;
		}
	}
	

	return k ? ZAP_SUCCESS : ZAP_FAIL;
}


static ZIO_GET_ALARMS_FUNCTION(wanpipe_get_alarms)
{
	wanpipe_tdm_api_t tdm_api;
	unsigned int alarms = 0;
	int err;

	memset(&tdm_api,0,sizeof(tdm_api));

#ifdef LIBSANGOMA_VERSION
	if ((err = sangoma_tdm_get_fe_alarms(zchan->sockfd, &tdm_api, &alarms))) {
        snprintf(zchan->last_error, sizeof(zchan->last_error), "ioctl failed (%s)", strerror(errno));
        snprintf(zchan->span->last_error, sizeof(zchan->span->last_error), "ioctl failed (%s)", strerror(errno));
        return ZAP_FAIL;		
	}
#else
	if ((err = sangoma_tdm_get_fe_alarms(zchan->sockfd, &tdm_api)) < 0){
        snprintf(zchan->last_error, sizeof(zchan->last_error), "ioctl failed (%s)", strerror(errno));
        snprintf(zchan->span->last_error, sizeof(zchan->span->last_error), "ioctl failed (%s)", strerror(errno));
        return ZAP_FAIL;		
	}
	alarms = tdm_api.wp_tdm_cmd.fe_alarms;
#endif

	
    zchan->alarm_flags = alarms ? ZAP_ALARM_RED : ZAP_ALARM_NONE;

    return ZAP_SUCCESS;
}


ZIO_SPAN_NEXT_EVENT_FUNCTION(wanpipe_next_event)
{
	uint32_t i,err;
	zap_oob_event_t event_id;
	
	for(i = 1; i <= span->chan_count; i++) {
		if (span->channels[i]->last_event_time && !zap_test_flag(span->channels[i], ZAP_CHANNEL_EVENT)) {
			uint32_t diff = (uint32_t)(zap_current_time_in_ms() - span->channels[i]->last_event_time);
			/* XX printf("%u %u %u\n", diff, (unsigned)zap_current_time_in_ms(), (unsigned)span->channels[i]->last_event_time); */
			if (zap_test_flag(span->channels[i], ZAP_CHANNEL_WINK)) {
				if (diff > wp_globals.wink_ms) {
					zap_clear_flag_locked(span->channels[i], ZAP_CHANNEL_WINK);
					zap_clear_flag_locked(span->channels[i], ZAP_CHANNEL_FLASH);
					zap_set_flag_locked(span->channels[i], ZAP_CHANNEL_OFFHOOK);
					event_id = ZAP_OOB_OFFHOOK;
					goto event;
				}
			}

			if (zap_test_flag(span->channels[i], ZAP_CHANNEL_FLASH)) {
				if (diff > wp_globals.flash_ms) {
					zap_clear_flag_locked(span->channels[i], ZAP_CHANNEL_FLASH);
					zap_clear_flag_locked(span->channels[i], ZAP_CHANNEL_WINK);
					zap_clear_flag_locked(span->channels[i], ZAP_CHANNEL_OFFHOOK);
					event_id = ZAP_OOB_ONHOOK;

					if (span->channels[i]->type == ZAP_CHAN_TYPE_FXO) {
						zap_channel_t *zchan = span->channels[i];
						wanpipe_tdm_api_t tdm_api;
						memset(&tdm_api, 0, sizeof(tdm_api));

						sangoma_tdm_txsig_onhook(zchan->sockfd,&tdm_api);
					}
					goto event;
				}
			}
		} 
		if (zap_test_flag(span->channels[i], ZAP_CHANNEL_EVENT)) {
			wanpipe_tdm_api_t tdm_api;
			zap_channel_t *zchan = span->channels[i];
			memset(&tdm_api, 0, sizeof(tdm_api));
			zap_clear_flag(span->channels[i], ZAP_CHANNEL_EVENT);

			err=sangoma_tdm_read_event(zchan->sockfd,&tdm_api);
			if (err != ZAP_SUCCESS) {
				snprintf(span->last_error, sizeof(span->last_error), "%s", strerror(errno));
				return ZAP_FAIL;
			}
			
			switch(tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_type) {

			case WP_TDMAPI_EVENT_LINK_STATUS:
				{
					switch(tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_link_status) {
					case WP_TDMAPI_EVENT_LINK_STATUS_CONNECTED:
						event_id = ZAP_OOB_ALARM_CLEAR;
						break;
					default:
						event_id = ZAP_OOB_ALARM_TRAP;
						break;
					};
				}
				break;

			case WP_TDMAPI_EVENT_RXHOOK:
				{
					if (span->channels[i]->type == ZAP_CHAN_TYPE_FXS) {
						event_id = tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_hook_state & WP_TDMAPI_EVENT_RXHOOK_OFF ? ZAP_OOB_OFFHOOK : ZAP_OOB_ONHOOK;
						if (event_id == ZAP_OOB_OFFHOOK) {
							if (zap_test_flag(span->channels[i], ZAP_CHANNEL_FLASH)) {
								zap_clear_flag_locked(span->channels[i], ZAP_CHANNEL_FLASH);
								zap_clear_flag_locked(span->channels[i], ZAP_CHANNEL_WINK);
								event_id = ZAP_OOB_FLASH;
								goto event;
							} else {
								zap_set_flag_locked(span->channels[i], ZAP_CHANNEL_WINK);
							}
						} else {
							if (zap_test_flag(span->channels[i], ZAP_CHANNEL_WINK)) {
								zap_clear_flag_locked(span->channels[i], ZAP_CHANNEL_WINK);
								zap_clear_flag_locked(span->channels[i], ZAP_CHANNEL_FLASH);
								event_id = ZAP_OOB_WINK;
								goto event;
							} else {
								zap_set_flag_locked(span->channels[i], ZAP_CHANNEL_FLASH);
							}
						}					
						continue;
					} else {
						int err;
						zap_channel_t *zchan = span->channels[i];
						err=sangoma_tdm_txsig_onhook(zchan->sockfd,&tdm_api);
						if (err) {
							snprintf(span->channels[i]->last_error, sizeof(span->channels[i]->last_error), "ONHOOK Failed");
							return ZAP_FAIL;
						}
						event_id = tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_hook_state & WP_TDMAPI_EVENT_RXHOOK_OFF ? ZAP_OOB_ONHOOK : ZAP_OOB_NOOP;	
					}
				}
				break;
			case WP_TDMAPI_EVENT_RING_DETECT:
				{
					event_id = tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_ring_state == WP_TDMAPI_EVENT_RING_PRESENT ? ZAP_OOB_RING_START : ZAP_OOB_RING_STOP;
				}
				break;
			case WP_TDMAPI_EVENT_RING_TRIP_DETECT:
				{
					event_id = tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_ring_state == WP_TDMAPI_EVENT_RING_PRESENT ? ZAP_OOB_ONHOOK : ZAP_OOB_OFFHOOK;
				}
				break;
			case WP_TDMAPI_EVENT_RBS:
				{
					event_id = ZAP_OOB_CAS_BITS_CHANGE;
					/* save the CAS bits, user should retrieve it with ZAP_COMMAND_GET_CAS_BITS 
					   is there a best play to store this? instead of adding cas_bits member to zap_chan? */
					span->channels[i]->cas_bits = wanpipe_swap_bits(tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_rbs_bits);
				}
				break;
            case WP_TDMAPI_EVENT_DTMF:
                {
                    char tmp_dtmf[2] = { tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_dtmf_digit, 0 };
					event_id = ZAP_OOB_NOOP;

					//zap_log(ZAP_LOG_DEBUG, "%d:%d queue hardware dtmf %s\n", zchan->span_id, zchan->chan_id, tmp_dtmf);
                    if (tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_dtmf_type == WAN_EC_TONE_STOP) {
                        zap_channel_queue_dtmf(zchan, tmp_dtmf);
                    } 
                }
                break;
			case WP_TDMAPI_EVENT_ALARM:
				event_id = ZAP_OOB_NOOP;
				break;
			default:
				{
					zap_log(ZAP_LOG_WARNING, "Unhandled event %d\n", tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_type);
					event_id = ZAP_OOB_INVALID;
				}
				break;
			}

		event:

			span->channels[i]->last_event_time = 0;
			span->event_header.e_type = ZAP_EVENT_OOB;
			span->event_header.enum_id = event_id;
			span->event_header.channel = span->channels[i];
			*event = &span->event_header;
			return ZAP_SUCCESS;
		}
	}

	return ZAP_FAIL;
	
}

static ZIO_CHANNEL_DESTROY_FUNCTION(wanpipe_channel_destroy)
{
	if (zchan->sockfd > -1) {
		close(zchan->sockfd);
		zchan->sockfd = WP_INVALID_SOCKET;
	}

	return ZAP_SUCCESS;
}

static ZIO_IO_LOAD_FUNCTION(wanpipe_init)
{
	assert(zio != NULL);
	memset(&wanpipe_interface, 0, sizeof(wanpipe_interface));

	wp_globals.codec_ms = 20;
	wp_globals.wink_ms = 150;
	wp_globals.flash_ms = 750;
	wp_globals.ring_on_ms = 2000;
	wp_globals.ring_off_ms = 4000;
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
	wanpipe_interface.channel_destroy = wanpipe_channel_destroy;
	wanpipe_interface.get_alarms = wanpipe_get_alarms;
	*zio = &wanpipe_interface;

	return ZAP_SUCCESS;
}

static ZIO_IO_UNLOAD_FUNCTION(wanpipe_destroy)
{
	memset(&wanpipe_interface, 0, sizeof(wanpipe_interface));
	return ZAP_SUCCESS;
}


zap_module_t zap_module = { 
	"wanpipe",
	wanpipe_init,
	wanpipe_destroy,
};

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
