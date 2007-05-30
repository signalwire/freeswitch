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
#include "zap_zt.h"


static struct {
	uint32_t codec_ms;
	uint32_t wink_ms;
	uint32_t flash_ms;
} zt_globals;

#define ZT_INVALID_SOCKET -1

static unsigned zt_open_range(zap_span_t *span, unsigned start, unsigned end, zap_chan_type_t type, char *name, char *number)
{
	unsigned configured = 0, x;
	char path[128] = "";
	zt_params_t ztp = {0};

	for(x = start; x < end; x++) {
		zap_channel_t *chan;
		zap_socket_t sockfd = ZT_INVALID_SOCKET;
		int command;
		int len = zt_globals.codec_ms * 8;

		snprintf(path, sizeof(path), "/dev/zap/%d", x);
		sockfd = open(path, O_RDWR);
		
		if (sockfd != ZT_INVALID_SOCKET && zap_span_add_channel(span, sockfd, type, &chan) == ZAP_SUCCESS) {
			command = ZT_START;
#if 0

			if (ioctl(sockfd, ZT_HOOK, &command)) {
				zap_log(ZAP_LOG_INFO, "failure configuring device %s as OpenZAP device %d:%d fd:%d err:%s\n", 
						path, chan->span_id, chan->chan_id, sockfd, strerror(errno));

				continue;
			}
#endif
	
			if (ioctl(chan->sockfd, ZT_SET_BLOCKSIZE, &len)) {
				zap_log(ZAP_LOG_INFO, "failure configuring device %s as OpenZAP device %d:%d fd:%d err:%s\n", 
						path, chan->span_id, chan->chan_id, sockfd, strerror(errno));
				close(sockfd);
				continue;
			} else {
                chan->packet_len = len;
                chan->effective_interval = chan->native_interval = chan->packet_len / 8;
				
                if (chan->effective_codec == ZAP_CODEC_SLIN) {
                    chan->packet_len *= 2;
                }
			}
			
			
			if (ioctl(sockfd, ZT_GET_PARAMS, &ztp) < 0) {
				close(sockfd);
				zap_log(ZAP_LOG_INFO, "failure configuring device %s as OpenZAP device %d:%d fd:%d\n", path, chan->span_id, chan->chan_id, sockfd);
				continue;
			}
			zap_log(ZAP_LOG_INFO, "configuring device %s as OpenZAP device %d:%d fd:%d\n", path, chan->span_id, chan->chan_id, sockfd);

			if (type == ZAP_CHAN_TYPE_FXS || type == ZAP_CHAN_TYPE_FXO) {
				if (ztp.g711_type == ZT_G711_ALAW) {
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
			zap_log(ZAP_LOG_ERROR, "failure configuring device %s\n", path);
		}
	}
	
	return configured;
}

static ZIO_CONFIGURE_SPAN_FUNCTION(zt_configure_span)
{

	int items, i;
    char *mydata, *item_list[10];
	char *ch, *mx;
	int channo;
	int top = 0;
	unsigned configured = 0;

	assert(str != NULL);
	

	mydata = strdup(str);
	assert(mydata != NULL);


	items = zap_separate_string(mydata, ',', item_list, (sizeof(item_list) / sizeof(item_list[0])));

	for(i = 0; i < items; i++) {
		ch = item_list[i];

		if (!(ch)) {
			zap_log(ZAP_LOG_ERROR, "Invalid input\n");
			continue;
		}

		channo = atoi(ch);
		
		if (channo < 0) {
			zap_log(ZAP_LOG_ERROR, "Invalid channel number %d\n", channo);
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

		configured += zt_open_range(span, channo, top, type, name, number);

	}
	
	free(mydata);

	return configured;

}

static ZIO_CONFIGURE_FUNCTION(zt_configure)
{
	if (!strcasecmp(category, "defaults")) {
		if (!strcasecmp(var, "codec_ms")) {
			unsigned codec_ms = atoi(val);
			if (codec_ms < 10 || codec_ms > 60) {
				zap_log(ZAP_LOG_WARNING, "invalid codec ms at line %d\n", lineno);
			} else {
				zt_globals.codec_ms = codec_ms;
			}
		}
	}
	
	return ZAP_SUCCESS;
}

static ZIO_OPEN_FUNCTION(zt_open) 
{
	ZIO_OPEN_MUZZLE;
	return ZAP_SUCCESS;
}

static ZIO_CLOSE_FUNCTION(zt_close)
{
	ZIO_CLOSE_MUZZLE;
	return ZAP_SUCCESS;
}

static ZIO_COMMAND_FUNCTION(zt_command)
{
	zt_params_t ztp = {0};
	int err = 0;

	ZIO_COMMAND_MUZZLE;
	
	switch(command) {
	case ZAP_COMMAND_GET_INTERVAL:
		{
			if (!(err = ioctl(zchan->sockfd, ZT_GET_BLOCKSIZE, &zchan->packet_len))) {
				zchan->native_interval = zchan->packet_len / 8;
				if (zchan->effective_codec == ZAP_CODEC_SLIN) {
					zchan->packet_len *= 2;
				}
				ZAP_COMMAND_OBJ_INT = zchan->native_interval;
			}

		}
		break;
	case ZAP_COMMAND_SET_INTERVAL: 
		{
			int interval = ZAP_COMMAND_OBJ_INT;
			int len = interval * 8;

			if (!(err = ioctl(zchan->sockfd, ZT_SET_BLOCKSIZE, &len))) {
				zchan->packet_len = len;
				zchan->effective_interval = zchan->native_interval = zchan->packet_len / 8;

                if (zchan->effective_codec == ZAP_CODEC_SLIN) {
                    zchan->packet_len *= 2;
                }

			}
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

static ZIO_WAIT_FUNCTION(zt_wait)
{
	int32_t inflags = 0;
	int result;
    struct pollfd pfds[1];

	if (*flags & ZAP_READ) {
		inflags |= POLLIN;
	}

	if (*flags & ZAP_WRITE) {
		inflags |= POLLOUT;
	}

	if (*flags & ZAP_EVENTS) {
		inflags |= POLLPRI;
	}


    memset(&pfds[0], 0, sizeof(pfds[0]));
    pfds[0].fd = zchan->sockfd;
    pfds[0].events = inflags;
    result = poll(pfds, 1, to);
	*flags = 0;

	if (pfds[0].revents & POLLERR) {
		result = -1;
	}

	if (result > 0) {
		*flags = pfds[0].revents;
	}


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

ZIO_SPAN_POLL_EVENT_FUNCTION(zt_poll_event)
{
	struct pollfd pfds[ZAP_MAX_CHANNELS_SPAN];
	int i, j = 0, k = 0, r, e;
	
	for(i = 1; i <= span->chan_count; i++) {
		e = ZT_IOMUX_SIGEVENT;
		memset(&pfds[j], 0, sizeof(pfds[j]));
		pfds[j].fd = span->channels[i].sockfd;
		pfds[j].events = POLLPRI;
		ioctl(span->channels[i].sockfd ,ZT_IOMUX, &e);
		j++;
	}

    r = poll(pfds, j, ms);

	if (r == 0) {
		return ZAP_TIMEOUT;
	} else if (r < 0 || (pfds[i-1].revents & POLLERR)) {
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

ZIO_SPAN_NEXT_EVENT_FUNCTION(zt_next_event)
{
	uint32_t i, event_id;
	zap_oob_event_t zt_event_id;

	for(i = 1; i <= span->chan_count; i++) {
		if (zap_test_flag((&span->channels[i]), ZAP_CHANNEL_EVENT)) {
			zap_clear_flag((&span->channels[i]), ZAP_CHANNEL_EVENT);
			if (ioctl(span->channels[i].sockfd, ZT_GETEVENT, &zt_event_id) == -1) {
				snprintf(span->last_error, sizeof(span->last_error), "%s", strerror(errno));
				return ZAP_FAIL;
			}

			switch(zt_event_id) {
			case ZT_EVENT_ONHOOK:
				{
					event_id = ZAP_OOB_ONHOOK;
				}
				break;
			case ZT_EVENT_RINGOFFHOOK:
				{
					if (span->channels[i].type == ZAP_CHAN_TYPE_FXS) {
						event_id = ZAP_OOB_OFFHOOK;
					} else if (span->channels[i].type == ZAP_CHAN_TYPE_FXO) {
						event_id = ZAP_OOB_RING_START;
					}
				}
				break;
			default:
				{
					zap_log(ZAP_LOG_WARNING, "Unhandled event %d\n", zt_event_id);
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


static ZIO_READ_FUNCTION(zt_read)
{
	zap_ssize_t r = 0;

	*datalen = zchan->packet_len;
	r = read(zchan->sockfd, data, *datalen);
	
	if (r >= 0) {
		*datalen = r;
		return ZAP_SUCCESS;
	}

	return ZAP_FAIL;
}

static ZIO_WRITE_FUNCTION(zt_write)
{
	zap_ssize_t w = 0;

	w = write(zchan->sockfd, data, *datalen);

	if (w >= 0) {
		*datalen = w;
		return ZAP_SUCCESS;
	}

	return ZAP_FAIL;
}

static zap_io_interface_t zt_interface;

zap_status_t zt_init(zap_io_interface_t **zio)
{
	assert(zio != NULL);
	memset(&zt_interface, 0, sizeof(zt_interface));
	memset(&zt_globals, 0, sizeof(zt_globals));

	zt_globals.codec_ms = 20;
	zt_globals.wink_ms = 150;
	zt_globals.flash_ms = 750;

	zt_interface.name = "zt";
	zt_interface.configure = zt_configure;
	zt_interface.configure_span = zt_configure_span;
	zt_interface.open = zt_open;
	zt_interface.close = zt_close;
	zt_interface.wait = zt_wait;
	zt_interface.read = zt_read;
	zt_interface.write = zt_write;
	zt_interface.poll_event = zt_poll_event;
	zt_interface.next_event = zt_next_event;

	*zio = &zt_interface;

	return ZAP_SUCCESS;
}

zap_status_t zt_destroy(void)
{
	return ZAP_FAIL;
}
