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
#include "wanpipe_tdm_api_iface.h"

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
/* on windows right now, there is no way to specify if we want to read events here or not, we allways get them here */
/* we need some what to select if we are reading regular tdm msgs or events */
/* need to either have 2 functions, 1 for events, 1 for regural read, or a flag on this function to choose */
/* 2 functions preferred.  Need implementation for the event function for both nix and windows that is threadsafe */
static __inline__ int tdmv_api_readmsg_tdm(sng_fd_t fd, void *hdrbuf, int hdrlen, void *databuf, int datalen)
{
	/* What do we need to do here to avoid having to do all */
	/* the memcpy's on windows and still maintain api compat with nix */
	uint32_t rx_len=0;
#if defined(__WINDOWS__)
	static RX_DATA_STRUCT	rx_data;
	api_header_t			*pri;
	wp_tdm_api_rx_hdr_t		*tdm_api_rx_hdr;
	wp_tdm_api_rx_hdr_t		*user_buf = (wp_tdm_api_rx_hdr_t*)hdrbuf;
	DWORD ln;

	if (hdrlen != sizeof(wp_tdm_api_rx_hdr_t)){
		return -1;
	}

	if (!DeviceIoControl(
						 fd,
						 IoctlReadCommand,
						 (LPVOID)NULL,
						 0L,
						 (LPVOID)&rx_data,
						 sizeof(RX_DATA_STRUCT),
						 (LPDWORD)(&ln),
						 (LPOVERLAPPED)NULL
						 )){
		return -1;
	}

	pri = &rx_data.api_header;
	tdm_api_rx_hdr = (wp_tdm_api_rx_hdr_t*)rx_data.data;

	user_buf->wp_tdm_api_event_type = pri->operation_status;

	switch(pri->operation_status)
		{
		case SANG_STATUS_RX_DATA_AVAILABLE:
			if (pri->data_length > datalen){
				break;
			}
			memcpy(databuf, rx_data.data, pri->data_length);
			rx_len = pri->data_length;
			break;

		default:
			break;
		}

#else
	struct msghdr msg;
	struct iovec iov[2];

	memset(&msg,0,sizeof(struct msghdr));

	iov[0].iov_len=hdrlen;
	iov[0].iov_base=hdrbuf;

	iov[1].iov_len=datalen;
	iov[1].iov_base=databuf;

	msg.msg_iovlen=2;
	msg.msg_iov=iov;

	rx_len = read(fd,&msg,datalen+hdrlen);

	if (rx_len <= sizeof(wp_tdm_api_rx_hdr_t)){
		return -EINVAL;
	}

	rx_len-=sizeof(wp_tdm_api_rx_hdr_t);
#endif
    return rx_len;
}                    

static __inline__ int tdmv_api_writemsg_tdm(sng_fd_t fd, void *hdrbuf, int hdrlen, void *databuf, unsigned short datalen)
{
	/* What do we need to do here to avoid having to do all */
	/* the memcpy's on windows and still maintain api compat with nix */
	int bsent = 0;
#if defined(__WINDOWS__)
	static TX_DATA_STRUCT	local_tx_data;
	api_header_t			*pri;
	DWORD ln;

	/* Are these really not needed or used???  What about for nix?? */
	(void)hdrbuf;
	(void)hdrlen;

	pri = &local_tx_data.api_header;

	pri->data_length = datalen;
	memcpy(local_tx_data.data, databuf, pri->data_length);

	if (!DeviceIoControl(
						 fd,
						 IoctlWriteCommand,
						 (LPVOID)&local_tx_data,
						 (ULONG)sizeof(TX_DATA_STRUCT),
						 (LPVOID)&local_tx_data,
						 sizeof(TX_DATA_STRUCT),
						 (LPDWORD)(&ln),
						 (LPOVERLAPPED)NULL
						 )){
		return -1;
	}

	if (local_tx_data.api_header.operation_status == SANG_STATUS_SUCCESS) {
		bsent = datalen;
	}
#else
	struct msghdr msg;
	struct iovec iov[2];

	memset(&msg,0,sizeof(struct msghdr));

	iov[0].iov_len = hdrlen;
	iov[0].iov_base = hdrbuf;

	iov[1].iov_len = datalen;
	iov[1].iov_base = databuf;

	msg.msg_iovlen = 2;
	msg.msg_iov = iov;

	bsent = write(fd, &msg, datalen + hdrlen);
	if (bsent > 0){
		bsent -= sizeof(wp_tdm_api_tx_hdr_t);
	}
#endif
	return bsent;
}

/* a cross platform way to poll on an actual pollset (span and/or list of spans) will probably also be needed for analog */
/* so we can have one analong handler thread that will deal with all the idle analog channels for events */
/* the alternative would be for the driver to provide one socket for all of the oob events for all analog channels */
static __inline__ int tdmv_api_wait_socket(sng_fd_t fd, int timeout, int *flags)
{
#if defined(__WINDOWS__)
	DWORD ln;
	API_POLL_STRUCT	api_poll;

	memset(&api_poll, 0x00, sizeof(API_POLL_STRUCT));
	
	api_poll.user_flags_bitmap = *flags;
	api_poll.timeout = timeout;

	if (!DeviceIoControl(
						 fd,
						 IoctlApiPoll,
						 (LPVOID)NULL,
						 0L,
						 (LPVOID)&api_poll,
						 sizeof(API_POLL_STRUCT),
						 (LPDWORD)(&ln),
						 (LPOVERLAPPED)NULL)) {
		return -1;
	}

	*flags = 0;

	switch(api_poll.operation_status)
		{
		case SANG_STATUS_RX_DATA_AVAILABLE:
			break;

		case SANG_STATUS_RX_DATA_TIMEOUT:
			return 0;

		default:
			return -1;
		}

	if (api_poll.poll_events_bitmap == 0){
		return -1;
	}

	if (api_poll.poll_events_bitmap & POLL_EVENT_TIMEOUT) {
		return 0;
	}

	*flags = api_poll.poll_events_bitmap;

	return 1;
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

#define FNAME_LEN 128
static __inline__ sng_fd_t tdmv_api_open_span_chan(int span, int chan) 
{
   	char fname[FNAME_LEN];
	sng_fd_t fd = WP_INVALID_SOCKET;
#if defined(__WINDOWS__)
	DWORD			ln;
	wan_udp_hdr_t	wan_udp;

	/* NOTE: under Windows Interfaces are zero based but 'chan' is 1 based. */
	/*		Subtract 1 from 'chan'. */
	_snprintf(fname , FNAME_LEN, "\\\\.\\WANPIPE%d_IF%d", span, chan - 1);

	fd = CreateFile(	fname, 
						GENERIC_READ | GENERIC_WRITE, 
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						(LPSECURITY_ATTRIBUTES)NULL, 
						OPEN_EXISTING,
						FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
						(HANDLE)NULL
						);

	/* make sure that we are the only ones who have this chan open */
	/* is this a threadsafe way to make sure that we are ok and will */
	/* never return a valid handle to more than one thread for the same channel? */

	wan_udp.wan_udphdr_command = GET_OPEN_HANDLES_COUNTER; 
	wan_udp.wan_udphdr_data_len = 0;

	DeviceIoControl(
					fd,
					IoctlManagementCommand,
					(LPVOID)&wan_udp,
					sizeof(wan_udp_hdr_t),
					(LPVOID)&wan_udp,
					sizeof(wan_udp_hdr_t),
					(LPDWORD)(&ln),
					(LPOVERLAPPED)NULL
					);

	if ((wan_udp.wan_udphdr_return_code) || (*(int*)&wan_udp.wan_udphdr_data[0] != 1)){
		/* somone already has this channel, or somthing else is not right. */
		tdmv_api_close_socket(&fd);
	}

#else
	/* Does this fail if another thread already has this chan open? */
	/* if not, we need to add some code to make sure it does */
	snprintf(fname, FNAME_LEN, "/dev/wptdm_s%dc%d",span,chan);

	fd = open(fname, O_RDWR);

	if (fd < 0) {
		fd = WP_INVALID_SOCKET;
	}

#endif
	return fd;  
}            



static zap_io_interface_t wanpipe_interface;

static zap_status_t wp_tdm_cmd_exec(zap_channel_t *zchan, wanpipe_tdm_api_t *tdm_api)
{
	int err;

	/* I'm told the 2nd arg is ignored but i send it as the cmd anyway for good measure */
	err = ioctl(zchan->sockfd, tdm_api->wp_tdm_cmd.cmd, &tdm_api->wp_tdm_cmd);

	if (err) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "%s", strerror(errno));
		return ZAP_FAIL;
	}

	return ZAP_SUCCESS;
}

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
		
		sockfd = tdmv_api_open_span_chan(spanno, x);
		
		if (sockfd != WP_INVALID_SOCKET && zap_span_add_channel(span, sockfd, type, &chan) == ZAP_SUCCESS) {
			wanpipe_tdm_api_t tdm_api;
			zap_log(ZAP_LOG_INFO, "configuring device s%dc%d as OpenZAP device %d:%d fd:%d\n", spanno, x, chan->span_id, chan->chan_id, sockfd);
			chan->physical_span_id = spanno;
			chan->physical_chan_id = x;
			chan->rate = 8000;

			if (type == ZAP_CHAN_TYPE_FXS || type == ZAP_CHAN_TYPE_FXO) {
				

#if 1
				if (type == ZAP_CHAN_TYPE_FXO) {
					tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_SET_EVENT;
					tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_type = WP_TDMAPI_EVENT_TXSIG_ONHOOK;
					tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_mode = WP_TDMAPI_EVENT_ENABLE;
					wp_tdm_cmd_exec(chan, &tdm_api);
				}
#endif

				tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_SET_EVENT;
				tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_type = WP_TDMAPI_EVENT_RING_DETECT;
				tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_mode = WP_TDMAPI_EVENT_ENABLE;
				wp_tdm_cmd_exec(chan, &tdm_api);
#if 1
				tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_SET_EVENT;
				tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_type = WP_TDMAPI_EVENT_RING_TRIP_DETECT;
				tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_mode = WP_TDMAPI_EVENT_ENABLE;
				wp_tdm_cmd_exec(chan, &tdm_api);
#endif
				tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_SET_EVENT;
				tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_type = WP_TDMAPI_EVENT_RXHOOK;
				tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_mode = WP_TDMAPI_EVENT_ENABLE;
				wp_tdm_cmd_exec(chan, &tdm_api);

			}

			if (type == ZAP_CHAN_TYPE_FXS || type == ZAP_CHAN_TYPE_FXO || type == ZAP_CHAN_TYPE_B) {
				tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_GET_HW_CODING;
				wp_tdm_cmd_exec(chan, &tdm_api);
				if (tdm_api.wp_tdm_cmd.hw_tdm_coding) {
					chan->native_codec = chan->effective_codec = ZAP_CODEC_ALAW;
				} else {
					chan->native_codec = chan->effective_codec = ZAP_CODEC_ULAW;
				}
			}

			if (type == ZAP_CHAN_TYPE_CAS) {
				tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_WRITE_RBS_BITS;
				tdm_api.wp_tdm_cmd.rbs_tx_bits = wanpipe_swap_bits(cas_bits);
				wp_tdm_cmd_exec(chan, &tdm_api);
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

	if (zchan->type == ZAP_CHAN_TYPE_DQ921 || zchan->type == ZAP_CHAN_TYPE_DQ931) {
		zchan->native_codec = zchan->effective_codec = ZAP_CODEC_NONE;
	} else {
		zchan->effective_codec = zchan->native_codec;
		tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_SET_CODEC;
		tdm_api.wp_tdm_cmd.tdm_codec = 0;
		wp_tdm_cmd_exec(zchan, &tdm_api);		
		tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_SET_USR_PERIOD;
		tdm_api.wp_tdm_cmd.usr_period = wp_globals.codec_ms;
		wp_tdm_cmd_exec(zchan, &tdm_api);
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
			tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_SET_EVENT;
			tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_type = zchan->span->start_type == ZAP_ANALOG_START_KEWL ? 
				WP_TDMAPI_EVENT_TXSIG_START : WP_TDMAPI_EVENT_TXSIG_KEWL;
			tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_mode = WP_TDMAPI_EVENT_ENABLE;
			if ((err = wp_tdm_cmd_exec(zchan, &tdm_api))) {
				snprintf(zchan->last_error, sizeof(zchan->last_error), "OFFHOOK Failed");
				return ZAP_FAIL;
			}
			zap_set_flag_locked(zchan, ZAP_CHANNEL_OFFHOOK);
		}
		break;
	case ZAP_COMMAND_ONHOOK:
		{
			tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_SET_EVENT;
			tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_type = WP_TDMAPI_EVENT_TXSIG_ONHOOK;
			tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_mode = WP_TDMAPI_EVENT_ENABLE;
			if ((err = wp_tdm_cmd_exec(zchan, &tdm_api))) {
				snprintf(zchan->last_error, sizeof(zchan->last_error), "ONHOOK Failed");
				return ZAP_FAIL;
			}
			zap_clear_flag_locked(zchan, ZAP_CHANNEL_OFFHOOK);
		}
		break;
	case ZAP_COMMAND_GENERATE_RING_ON:
		{
			tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_SET_EVENT;
			tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_type = WP_TDMAPI_EVENT_RING;
			tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_mode = WP_TDMAPI_EVENT_ENABLE;
			if ((err = wp_tdm_cmd_exec(zchan, &tdm_api))) {
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
			tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_SET_EVENT;
			tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_type = WP_TDMAPI_EVENT_RING;
			tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_mode = WP_TDMAPI_EVENT_DISABLE;
			if ((err = wp_tdm_cmd_exec(zchan, &tdm_api))) {
				snprintf(zchan->last_error, sizeof(zchan->last_error), "Ring-off Failed");
				return ZAP_FAIL;
			}
			zap_clear_pflag_locked(zchan, WP_RINGING);
			zap_clear_flag_locked(zchan, ZAP_CHANNEL_RINGING);
		}
		break;
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
	case ZAP_COMMAND_SET_CAS_BITS:
		{
			tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_WRITE_RBS_BITS;
			tdm_api.wp_tdm_cmd.rbs_tx_bits = wanpipe_swap_bits(ZAP_COMMAND_OBJ_INT);
			err = wp_tdm_cmd_exec(zchan, &tdm_api);
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
	uint32_t i, j = 0, k = 0, l = 0;
	int r;
	
	for(i = 1; i <= span->chan_count; i++) {
		zap_channel_t *zchan = span->channels[i];
		memset(&pfds[j], 0, sizeof(pfds[j]));
		pfds[j].fd = span->channels[i]->sockfd;
		pfds[j].events = POLLPRI;

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
				tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_SET_EVENT;
				tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_type = WP_TDMAPI_EVENT_RING;
				tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_mode = WP_TDMAPI_EVENT_DISABLE;
				if ((err = wp_tdm_cmd_exec(zchan, &tdm_api))) {
					snprintf(zchan->last_error, sizeof(zchan->last_error), "Ring-off Failed");
					return ZAP_FAIL;
				}
				zap_clear_pflag_locked(zchan, WP_RINGING);
				zchan->ring_time = zap_current_time_in_ms() + wp_globals.ring_off_ms;
			} else {
				tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_SET_EVENT;
				tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_type = WP_TDMAPI_EVENT_RING;
				tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_mode = WP_TDMAPI_EVENT_ENABLE;
				if ((err = wp_tdm_cmd_exec(zchan, &tdm_api))) {
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
	
    r = poll(pfds, j, ms);
	
	if (r == 0) {
		return l ? ZAP_SUCCESS : ZAP_TIMEOUT;
	} else if (r < 0) {
		snprintf(span->last_error, sizeof(span->last_error), "%s", strerror(errno));
		return ZAP_FAIL;
	}
	
	for(i = 1; i <= span->chan_count; i++) {
		zap_channel_t *zchan = span->channels[i];

		if (pfds[i-1].revents & POLLPRI) {
			zap_set_flag(zchan, ZAP_CHANNEL_EVENT);
			zchan->last_event_time = zap_current_time_in_ms();
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
						wanpipe_tdm_api_t tdm_api;
						memset(&tdm_api, 0, sizeof(tdm_api));
						tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_SET_EVENT;
						tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_type = WP_TDMAPI_EVENT_TXSIG_ONHOOK;
						tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_mode = WP_TDMAPI_EVENT_ENABLE;
						wp_tdm_cmd_exec(span->channels[i], &tdm_api);
					}
					goto event;
				}
			}
		}
		if (zap_test_flag(span->channels[i], ZAP_CHANNEL_EVENT)) {
			wanpipe_tdm_api_t tdm_api;
			memset(&tdm_api, 0, sizeof(tdm_api));
			zap_clear_flag(span->channels[i], ZAP_CHANNEL_EVENT);

			tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_READ_EVENT;
			if (wp_tdm_cmd_exec(span->channels[i], &tdm_api) != ZAP_SUCCESS) {
				snprintf(span->last_error, sizeof(span->last_error), "%s", strerror(errno));
				return ZAP_FAIL;
			}
			
			switch(tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_type) {
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
						
						tdm_api.wp_tdm_cmd.cmd = SIOC_WP_TDM_SET_EVENT;
						tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_type = WP_TDMAPI_EVENT_TXSIG_ONHOOK;
						tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_mode = WP_TDMAPI_EVENT_ENABLE;
						if ((err = wp_tdm_cmd_exec(span->channels[i], &tdm_api))) {
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
