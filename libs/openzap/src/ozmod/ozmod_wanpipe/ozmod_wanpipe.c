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

#ifdef __sun
#include <unistd.h>
#include <stropts.h>
#endif
#include "openzap.h"
#ifndef __WINDOWS__
#include <poll.h>
#include <sys/socket.h>
#endif
#include "libsangoma.h"

#if defined(__WINDOWS__)
/*! Backward compatible defines - current code is all using the old names*/ 
#define sangoma_open_tdmapi_span_chan sangoma_open_api_span_chan
#define sangoma_open_tdmapi_span sangoma_open_api_span
#define sangoma_open_tdmapi_ctrl sangoma_open_api_ctrl
#define sangoma_tdm_get_fe_status sangoma_get_fe_status
#define sangoma_socket_close sangoma_close
#define sangoma_tdm_get_hw_coding sangoma_get_hw_coding
#define sangoma_tdm_set_fe_status sangoma_set_fe_status
#define sangoma_tdm_get_link_status sangoma_get_link_status
#define sangoma_tdm_flush_bufs sangoma_flush_bufs
#define sangoma_tdm_cmd_exec sangoma_cmd_exec
#define sangoma_tdm_read_event sangoma_read_event
#define sangoma_readmsg_tdm sangoma_readmsg
#define sangoma_readmsg_socket sangoma_readmsg
#define sangoma_sendmsg_socket sangoma_writemsg
#define sangoma_writemsg_tdm sangoma_writemsg
#define sangoma_create_socket_intr sangoma_open_api_span_chan
#endif

/*! Starting with libsangoma 3 we can use the new libsangoma waitable API, the poor souls of those using a release were LIBSANGOMA version
 * is defined but the version is not higher or equal to 3.0.0 will be forced to upgrade
 * */
#ifdef LIBSANGOMA_VERSION 
#if LIBSANGOMA_VERSION_CODE < LIBSANGOMA_VERSION(3,0,0)
#undef LIBSANGOMA_VERSION
#endif
#endif

/**
 * \brief Wanpipe flags
 */
typedef enum {
	WP_RINGING = (1 << 0)
} wp_flag_t;

/**
 * \brief Wanpipe globals
 */
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

/**
 * \brief Poll for event on a wanpipe socket
 * \param fd Wanpipe socket descriptor
 * \param timeout Time to wait for event
 * \param flags Sangoma event flags
 * \return -1 on failure, wanpipe event flags on success
 *
 * a cross platform way to poll on an actual pollset (span and/or list of spans) will probably also be needed for analog
 * so we can have one analong handler thread that will deal with all the idle analog channels for events
 * the alternative would be for the driver to provide one socket for all of the oob events for all analog channels
 */
static __inline__ int tdmv_api_wait_socket(zap_channel_t *zchan, int timeout, int *flags)
{
	
#ifdef LIBSANGOMA_VERSION
	int err;
    uint32_t inflags = *flags;
    uint32_t outflags = 0;
	sangoma_wait_obj_t *sangoma_wait_obj = zchan->mod_data;

	err = sangoma_waitfor(sangoma_wait_obj, inflags, &outflags, timeout);
	*flags = 0;
    if (err == SANG_STATUS_SUCCESS) {
        *flags = outflags;
        err = 1; /* ideally should be the number of file descriptors with something to read */
    }
    if (err == SANG_STATUS_APIPOLL_TIMEOUT) {
        err = 0;
    }
    return err;
#else
 	struct pollfd pfds[1];
    int res;

    memset(&pfds[0], 0, sizeof(pfds[0]));
    pfds[0].fd = zchan->sockfd;
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

/**
 * \brief Opens a sangoma channel socket (TDM API)
 * \param span Span number
 * \param chan Channel number
 * \return 0 on success, wanpipe error code on failure
 */
static __inline__ sng_fd_t tdmv_api_open_span_chan(int span, int chan) 
{
	return sangoma_open_tdmapi_span_chan(span, chan);
}

#ifdef LIBSANGOMA_VERSION
static __inline__ sng_fd_t __tdmv_api_open_span_chan(int span, int chan) 
{ 
	return  __sangoma_open_tdmapi_span_chan(span, chan);
}                        
#endif

static zap_io_interface_t wanpipe_interface;

/**
 * \brief Inverts bit string
 * \param cas_bits CAS bit string
 * \return Swapped bits
 */
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

/**
 * \brief Initialises a range of wanpipe channels
 * \param span Openzap span
 * \param spanno Wanpipe span number
 * \param start Initial wanpipe channel number
 * \param end Final wanpipe channel number
 * \param type Openzap channel type
 * \param name Openzap span name
 * \param number Openzap span number
 * \param cas_bits CAS bits
 * \return number of spans configured
 */
static unsigned wp_open_range(zap_span_t *span, unsigned spanno, unsigned start, unsigned end, zap_chan_type_t type, char *name, char *number, unsigned char cas_bits)
{
	unsigned configured = 0, x;
#ifdef LIBSANGOMA_VERSION
	sangoma_status_t sangstatus;
	sangoma_wait_obj_t *sangoma_wait_obj;
#endif

	if (type == ZAP_CHAN_TYPE_CAS) {
		zap_log(ZAP_LOG_DEBUG, "Configuring Wanpipe CAS channels with abcd == 0x%X\n", cas_bits);
	}	
	for(x = start; x < end; x++) {
		zap_channel_t *chan;
		zap_socket_t sockfd = ZAP_INVALID_SOCKET;
		const char *dtmf = "none";
		if (!strncasecmp(span->name, "smg_prid_nfas", 8) && span->trunk_type == ZAP_TRUNK_T1 && x == 24) {
#ifdef LIBSANGOMA_VERSION
			sockfd = __tdmv_api_open_span_chan(spanno, x);
#else
			zap_log(ZAP_LOG_ERROR, "span %d channel %d cannot be configured as smg_prid_nfas, you need to compile openzap with newer libsangoma\n", spanno, x);
#endif
		} else {
			sockfd = tdmv_api_open_span_chan(spanno, x);
		}

		if (sockfd == ZAP_INVALID_SOCKET) {
			zap_log(ZAP_LOG_ERROR, "Failed to open wanpipe device span %d channel %d\n", spanno, x);
			continue;
		}
		
		if (zap_span_add_channel(span, sockfd, type, &chan) == ZAP_SUCCESS) {
			wanpipe_tdm_api_t tdm_api;
			memset(&tdm_api, 0, sizeof(tdm_api));
#ifdef LIBSANGOMA_VERSION
			sangstatus = sangoma_wait_obj_create(&sangoma_wait_obj, sockfd, SANGOMA_DEVICE_WAIT_OBJ);
			if (sangstatus != SANG_STATUS_SUCCESS) {
				zap_log(ZAP_LOG_ERROR, "failure create waitable object for s%dc%d\n", spanno, x);
				continue;
			}
			chan->mod_data = sangoma_wait_obj;
#endif
			
			chan->physical_span_id = spanno;
			chan->physical_chan_id = x;
			chan->rate = 8000;
			
			if (type == ZAP_CHAN_TYPE_FXS || type == ZAP_CHAN_TYPE_FXO || type == ZAP_CHAN_TYPE_B) {
				int err;
				
				dtmf = "software";

				/* FIXME: Handle Error Condition Check for return code */
				err = sangoma_tdm_get_hw_coding(chan->sockfd, &tdm_api);

				if (tdm_api.wp_tdm_cmd.hw_tdm_coding) {
					chan->native_codec = chan->effective_codec = ZAP_CODEC_ALAW;
				} else {
					chan->native_codec = chan->effective_codec = ZAP_CODEC_ULAW;
				}

				err = sangoma_tdm_get_hw_dtmf(chan->sockfd, &tdm_api);
				if (err > 0) {
					zap_channel_set_feature(chan, ZAP_CHANNEL_FEATURE_DTMF_DETECT);
					dtmf = "hardware";
				}
			}

#ifdef LIBSANGOMA_VERSION
			if (type == ZAP_CHAN_TYPE_FXS) {
				if (sangoma_tdm_disable_ring_trip_detect_events(chan->sockfd, &tdm_api)) {
					/* we had problems of on-hook/off-hook detection due to how ring trip events were handled
					 * if this fails, I believe we will still work ok as long as we dont handle them incorrectly */
					zap_log(ZAP_LOG_WARNING, "Failed to disable ring trip events in channel s%dc%d\n", spanno, x);
				}
			}
#endif
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

			if (type == ZAP_CHAN_TYPE_CAS || type == ZAP_CHAN_TYPE_EM) {
#ifdef LIBSANGOMA_VERSION
				sangoma_tdm_write_rbs(chan->sockfd,&tdm_api,chan->physical_chan_id, wanpipe_swap_bits(cas_bits));

				/* this should probably be done for old libsangoma but I am not sure if the API is available and I'm lazy to check,
				   The poll rate is hard coded to 100 per second (done in the driver, is the max rate of polling allowed by wanpipe)
				 */
				if (sangoma_tdm_enable_rbs_events(chan->sockfd, &tdm_api, 100)) {
					zap_log(ZAP_LOG_ERROR, "Failed to enable RBS/CAS events in device %d:%d fd:%d\n", chan->span_id, chan->chan_id, sockfd);
					continue;
				}
				/* probably done by the driver but lets write defensive code this time */
				sangoma_flush_bufs(chan->sockfd, &tdm_api);
#else
				/* 
				 * With wanpipe 3.4.4.2 I get failure even though the events are enabled, /var/log/messages said:
				 * wanpipe4: WARNING: Event type 9 is already pending!
				 * wanpipe4: Failed to add new fe event 09 ch_map=FFFFFFFF!
				 * may be we should not send an error until that is fixed in the driver
				 */
				if (sangoma_tdm_enable_rbs_events(chan->sockfd, &tdm_api, 100)) {
					zap_log(ZAP_LOG_ERROR, "Failed to enable RBS/CAS events in device %d:%d fd:%d\n", chan->span_id, chan->chan_id, sockfd);
				}
				/* probably done by the driver but lets write defensive code this time */
				sangoma_tdm_flush_bufs(chan->sockfd, &tdm_api);
				sangoma_tdm_write_rbs(chan->sockfd,&tdm_api, wanpipe_swap_bits(cas_bits));
#endif
			}
			
			if (!zap_strlen_zero(name)) {
				zap_copy_string(chan->chan_name, name, sizeof(chan->chan_name));
			}

			if (!zap_strlen_zero(number)) {
				zap_copy_string(chan->chan_number, number, sizeof(chan->chan_number));
			}
			configured++;
			zap_log(ZAP_LOG_INFO, "configuring device s%dc%d as OpenZAP device %d:%d fd:%d DTMF: %s\n",
				spanno, x, chan->span_id, chan->chan_id, sockfd, dtmf);

		} else {
			zap_log(ZAP_LOG_ERROR, "zap_span_add_channel failed for wanpipe span %d channel %d\n", spanno, x);
		}
	}
	
	return configured;
}

/**
 * \brief Process configuration variable for a Wanpipe profile
 * \param category Wanpipe profile name
 * \param var Variable name
 * \param val Variable value
 * \param lineno Line number from configuration file
 * \return Success
 */
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
		} else if (!strcasecmp(var, "ring_on_ms")) {
			num = atoi(val);
			if (num < 500 || num > 5000) {
				zap_log(ZAP_LOG_WARNING, "invalid ring_on_ms at line %d (valid range 500 to 5000)\n", lineno);
			} else {
				wp_globals.ring_on_ms = num;
			}
		} else if (!strcasecmp(var, "ring_off_ms")) {
			num = atoi(val);
			if (num < 500 || num > 5000) {
				zap_log(ZAP_LOG_WARNING, "invalid ring_off_ms at line %d (valid range 500 to 5000)\n", lineno);
			} else {
				wp_globals.ring_off_ms = num;
			}
		}
	}

	return ZAP_SUCCESS;
}

/**
 * \brief Initialises an openzap Wanpipe span from a configuration string
 * \param span Openzap span
 * \param str Configuration string
 * \param type Openzap span type
 * \param name Openzap span name
 * \param number Openzap span number
 * \return Success or failure
 */
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
			zap_log(ZAP_LOG_ERROR, "No valid wanpipe span and channel was specified\n");
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

/**
 * \brief Opens Wanpipe channel
 * \param zchan Channel to open
 * \return Success or failure
 */
static ZIO_OPEN_FUNCTION(wanpipe_open) 
{

	wanpipe_tdm_api_t tdm_api;

	memset(&tdm_api,0,sizeof(tdm_api));
	sangoma_tdm_flush_bufs(zchan->sockfd, &tdm_api);
#ifdef LIBSANGOMA_VERSION
	sangoma_flush_event_bufs(zchan->sockfd, &tdm_api);
#endif

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

/**
 * \brief Closes Wanpipe channel
 * \param zchan Channel to close
 * \return Success
 */
static ZIO_CLOSE_FUNCTION(wanpipe_close)
{
	return ZAP_SUCCESS;
}

/**
 * \brief Executes an Openzap command on a Wanpipe channel
 * \param zchan Channel to execute command on
 * \param command Openzap command to execute
 * \param obj Object (unused)
 * \return Success or failure
 */
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
	case ZAP_COMMAND_ENABLE_ECHOCANCEL:
		{
			err=sangoma_tdm_enable_hwec(zchan->sockfd, &tdm_api);
			if (err) {
             			snprintf(zchan->last_error, sizeof(zchan->last_error), "HWEC Enable Failed");
				return ZAP_FAIL;
			}
		}
		break;
	case ZAP_COMMAND_DISABLE_ECHOCANCEL:
		{
			err=sangoma_tdm_disable_hwec(zchan->sockfd, &tdm_api);
			if (err) {
             			snprintf(zchan->last_error, sizeof(zchan->last_error), "HWEC Disable Failed");
				return ZAP_FAIL;
			}
		}
		break;
	case ZAP_COMMAND_ENABLE_DTMF_DETECT:
		{
#ifdef WP_API_FEATURE_DTMF_EVENTS
			err = sangoma_tdm_enable_dtmf_events(zchan->sockfd, &tdm_api);
			if (err) {
				zap_log(ZAP_LOG_WARNING, "Enabling of Sangoma HW DTMF failed\n");
             			snprintf(zchan->last_error, sizeof(zchan->last_error), "HW DTMF Enable Failed");
				return ZAP_FAIL;
			}
			zap_log(ZAP_LOG_DEBUG, "Enabled DTMF events on chan %d:%d\n", zchan->span_id, zchan->chan_id);
#else
			return ZAP_NOTIMPL;
#endif
		}
		break;
	case ZAP_COMMAND_DISABLE_DTMF_DETECT:
		{
#ifdef WP_API_FEATURE_DTMF_EVENTS
			err = sangoma_tdm_disable_dtmf_events(zchan->sockfd, &tdm_api);
			if (err) {
				zap_log(ZAP_LOG_WARNING, "Disabling of Sangoma HW DTMF failed\n");
             			snprintf(zchan->last_error, sizeof(zchan->last_error), "HW DTMF Disable Failed");
				return ZAP_FAIL;
			}
			zap_log(ZAP_LOG_DEBUG, "Disabled DTMF events on chan %d:%d\n", zchan->span_id, zchan->chan_id);
#else
			return ZAP_NOTIMPL;
#endif
		}
		break;
	case ZAP_COMMAND_ENABLE_LOOP:
		{
#ifdef WP_API_FEATURE_LOOP
         	err=sangoma_tdm_enable_loop(zchan->sockfd, &tdm_api);
			if (err) {
				snprintf(zchan->last_error, sizeof(zchan->last_error), "Loop Enable Failed");
				return ZAP_FAIL;
			}
#endif		
		}
		break;
	case ZAP_COMMAND_DISABLE_LOOP:
		{
#ifdef WP_API_FEATURE_LOOP
         	err=sangoma_tdm_disable_loop(zchan->sockfd, &tdm_api);
			if (err) {
				snprintf(zchan->last_error, sizeof(zchan->last_error), "Loop Disable Failed");
				return ZAP_FAIL;
			}
#endif	 
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
			err = sangoma_tdm_write_rbs(zchan->sockfd,&tdm_api, zchan->physical_chan_id, wanpipe_swap_bits(ZAP_COMMAND_OBJ_INT));
#else
			err = sangoma_tdm_write_rbs(zchan->sockfd, &tdm_api, wanpipe_swap_bits(ZAP_COMMAND_OBJ_INT));
#endif
		}
		break;
	case ZAP_COMMAND_GET_CAS_BITS:
		{
#ifdef LIBSANGOMA_VERSION
            unsigned char rbsbits;
            err = sangoma_tdm_read_rbs(zchan->sockfd, &tdm_api, zchan->physical_chan_id, &rbsbits);
            if (!err) {
                ZAP_COMMAND_OBJ_INT = wanpipe_swap_bits(rbsbits);
            }
#else
            // does sangoma_tdm_read_rbs is available here?
			ZAP_COMMAND_OBJ_INT = zchan->rx_cas_bits;
#endif
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

/**
 * \brief Reads data from a Wanpipe channel
 * \param zchan Channel to read from
 * \param data Data buffer
 * \param datalen Size of data buffer
 * \return Success, failure or timeout
 */
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

/**
 * \brief Writes data to a Wanpipe channel
 * \param zchan Channel to write to
 * \param data Data buffer
 * \param datalen Size of data buffer
 * \return Success or failure
 */
static ZIO_WRITE_FUNCTION(wanpipe_write)
{
	int bsent;
	wp_tdm_api_tx_hdr_t hdrframe;

	/* Do we even need the headerframe here? on windows, we don't even pass it to the driver */
	memset(&hdrframe, 0, sizeof(hdrframe));
	if (*datalen == 0) {
		return ZAP_SUCCESS;
	}
	bsent = sangoma_writemsg_tdm(zchan->sockfd, &hdrframe, (int)sizeof(hdrframe), data, (unsigned short)(*datalen),0);

	/* should we be checking if bsent == *datalen here? */
	if (bsent > 0) {
		*datalen = bsent;
		return ZAP_SUCCESS;
	}

	return ZAP_FAIL;
}

/**
 * \brief Waits for an event on a Wanpipe channel
 * \param zchan Channel to open
 * \param flags Type of event to wait for
 * \param to Time to wait (in ms)
 * \return Success, failure or timeout
 */

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

	result = tdmv_api_wait_socket(zchan, to, &inflags);

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

/**
 * \brief Checks for events on a Wanpipe span
 * \param span Span to check for events
 * \param ms Time to wait for event
 * \return Success if event is waiting or failure if not
 */
ZIO_SPAN_POLL_EVENT_FUNCTION(wanpipe_poll_event)
{
#ifdef LIBSANGOMA_VERSION
    sangoma_status_t sangstatus;
	sangoma_wait_obj_t *pfds[ZAP_MAX_CHANNELS_SPAN] = { 0 };
    uint32_t inflags[ZAP_MAX_CHANNELS_SPAN];
    uint32_t outflags[ZAP_MAX_CHANNELS_SPAN];
#else
	struct pollfd pfds[ZAP_MAX_CHANNELS_SPAN];
#endif

	uint32_t i, j = 0, k = 0, l = 0;
	int r;
	
	for(i = 1; i <= span->chan_count; i++) {
		zap_channel_t *zchan = span->channels[i];

		if (!strncasecmp(span->name, "smg_prid_nfas", 8) && span->trunk_type == ZAP_TRUNK_T1 && zchan->physical_chan_id == 24) {
			continue;
		} 

#ifdef LIBSANGOMA_VERSION
		if (!zchan->mod_data) {
			continue; /* should never happen but happens when shutting down */
		}
		pfds[j] = zchan->mod_data;
        inflags[j] = POLLPRI;
#else
		memset(&pfds[j], 0, sizeof(pfds[j]));
		pfds[j].fd = span->channels[i]->sockfd;
		pfds[j].events = POLLPRI;
#endif

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
	sangstatus = sangoma_waitfor_many(pfds, inflags, outflags, j, ms);
    if (SANG_STATUS_APIPOLL_TIMEOUT == sangstatus) {
        r = 0;
    } else if (SANG_STATUS_SUCCESS == sangstatus) {
        r = 1; /* hopefully we never need how many changed -_- */
    } else {
        r = -1;
    }
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
		if (outflags[i-1] & POLLPRI) {
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

/**
 * \brief Gets alarms from a Wanpipe Channel
 * \param zchan Channel to get alarms from
 * \return Success or failure
 */
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
	zchan->alarm_flags = ZAP_ALARM_NONE;

	if (alarms & WAN_TE_BIT_ALARM_RED) {
		zchan->alarm_flags |= ZAP_ALARM_RED;
		alarms &= ~WAN_TE_BIT_ALARM_RED;
	}

	if (alarms & WAN_TE_BIT_ALARM_AIS) {
		zchan->alarm_flags |= ZAP_ALARM_AIS;
		zchan->alarm_flags |= ZAP_ALARM_BLUE;
		alarms &= ~WAN_TE_BIT_ALARM_AIS;
	}

	if (alarms & WAN_TE_BIT_ALARM_RAI) {
		zchan->alarm_flags |= ZAP_ALARM_RAI;
		zchan->alarm_flags |= ZAP_ALARM_YELLOW;
		alarms &= ~WAN_TE_BIT_ALARM_RAI;
	}

	/* still missing to map:
	 * ZAP_ALARM_RECOVER
	 * ZAP_ALARM_LOOPBACK
	 * ZAP_ALARM_NOTOPEN
	 * */

	/* if we still have alarms that we did not map, set the general alarm */
	if (alarms) {
		zap_log(ZAP_LOG_DEBUG, "Unmapped wanpipe alarms: %d\n", alarms);
		zchan->alarm_flags |= ZAP_ALARM_GENERAL;
	}

	return ZAP_SUCCESS;
}

/**
 * \brief Retrieves an event from a wanpipe span
 * \param span Span to retrieve event from
 * \param event Openzap event to return
 * \return Success or failure
 */
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
					zap_log(ZAP_LOG_DEBUG, "%d:%d Returning fake ONHOOK\n", span->channels[i]->span_id, span->channels[i]->chan_id);
					goto event;
				}
			}
		} 
		if (zap_test_flag(span->channels[i], ZAP_CHANNEL_EVENT)) {
			wanpipe_tdm_api_t tdm_api;
			zap_channel_t *zchan = span->channels[i];
			memset(&tdm_api, 0, sizeof(tdm_api));
			zap_clear_flag(span->channels[i], ZAP_CHANNEL_EVENT);

			err = sangoma_tdm_read_event(zchan->sockfd, &tdm_api);
			if (err != ZAP_SUCCESS) {
				snprintf(span->last_error, sizeof(span->last_error), "%s", strerror(errno));
				return ZAP_FAIL;
			}
			
			zap_log(ZAP_LOG_DEBUG, "%d:%d wanpipe returned event %d\n", span->channels[i]->span_id, span->channels[i]->chan_id, tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_type);
			switch(tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_type) {

			case WP_TDMAPI_EVENT_LINK_STATUS:
				{
					zap_sigmsg_t sigmsg;
					memset(&sigmsg, 0, sizeof(sigmsg));
					switch(tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_link_status) {
					case WP_TDMAPI_EVENT_LINK_STATUS_CONNECTED:
						event_id = ZAP_OOB_ALARM_CLEAR;
						break;
					default:
						event_id = ZAP_OOB_ALARM_TRAP;
						break;
					};
					sigmsg.chan_id = zchan->chan_id;
					sigmsg.span_id = zchan->span_id;
					sigmsg.channel = zchan;
					sigmsg.event_id = (event_id == ZAP_OOB_ALARM_CLEAR) ? ZAP_SIGEVENT_ALARM_CLEAR : ZAP_SIGEVENT_ALARM_TRAP;
					zap_span_send_signal(zchan->span, &sigmsg);
				}
				break;

			case WP_TDMAPI_EVENT_RXHOOK:
				{
					zap_log(ZAP_LOG_DEBUG, "%d:%d rxhook, state %d\n", span->channels[i]->span_id, span->channels[i]->chan_id, 
							tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_hook_state);
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
				/*
				disabled this ones when configuring, we don't need them, do we?
			case WP_TDMAPI_EVENT_RING_TRIP_DETECT:
				{
					event_id = tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_ring_state == WP_TDMAPI_EVENT_RING_PRESENT ? ZAP_OOB_ONHOOK : ZAP_OOB_OFFHOOK;
				}
				break;
				*/
			case WP_TDMAPI_EVENT_RBS:
				{
					event_id = ZAP_OOB_CAS_BITS_CHANGE;
					span->channels[i]->rx_cas_bits = wanpipe_swap_bits(tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_rbs_bits);
				}
				break;
            case WP_TDMAPI_EVENT_DTMF:
                {
                    char tmp_dtmf[2] = { tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_dtmf_digit, 0 };
					event_id = ZAP_OOB_NOOP;

					//zap_log(ZAP_LOG_WARNING, "%d:%d queue hardware dtmf %s %s\n", zchan->span_id, zchan->chan_id, tmp_dtmf, 
					//tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_dtmf_type == WAN_EC_TONE_PRESENT ? "on" : "off");
					if (tmp_dtmf[0] == 'f') {
						if (zap_test_flag(zchan, ZAP_CHANNEL_INUSE)) {
							zap_channel_queue_dtmf(zchan, tmp_dtmf);
						}
						break;
					}

                    if (tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_dtmf_type == WAN_EC_TONE_PRESENT) {
						zap_set_flag_locked(zchan, ZAP_CHANNEL_MUTE);
					}

                    if (tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_dtmf_type == WAN_EC_TONE_STOP) {
						zap_clear_flag_locked(zchan, ZAP_CHANNEL_MUTE);
						if (zap_test_flag(zchan, ZAP_CHANNEL_INUSE)) {
							zap_channel_queue_dtmf(zchan, tmp_dtmf);
						}
                    } 
                }
                break;
			case WP_TDMAPI_EVENT_ALARM:
				{
					zap_sigmsg_t sigmsg;
					zap_log(ZAP_LOG_DEBUG, "Got wanpipe alarms %d\n", tdm_api.wp_tdm_cmd.event.wp_api_event_alarm);
					memset(&sigmsg, 0, sizeof(sigmsg));
					event_id = ZAP_OOB_ALARM_TRAP;
					sigmsg.chan_id = zchan->chan_id;
					sigmsg.span_id = zchan->span_id;
					sigmsg.channel = zchan;
					sigmsg.event_id = (event_id == ZAP_OOB_ALARM_CLEAR) ? ZAP_SIGEVENT_ALARM_CLEAR : ZAP_SIGEVENT_ALARM_TRAP;
					zap_span_send_signal(zchan->span, &sigmsg);
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

/**
 * \brief Destroys a Wanpipe Channel
 * \param zchan Channel to destroy
 * \return Success
 */
static ZIO_CHANNEL_DESTROY_FUNCTION(wanpipe_channel_destroy)
{
#ifdef LIBSANGOMA_VERSION
	if (zchan->mod_data) {
	    sangoma_wait_obj_t *sangoma_wait_obj;
		sangoma_wait_obj = zchan->mod_data;
		zchan->mod_data = NULL;
		sangoma_wait_obj_delete(&sangoma_wait_obj);
	}
#endif
	if (zchan->sockfd != ZAP_INVALID_SOCKET) {
		/* enable HW DTMF. As odd as it seems. Why enable when the channel is being destroyed and won't be used anymore?
		* because that way we can transfer the DTMF state back to the driver, if we're being restarted we will set again
		* the FEATURE_DTMF flag and use HW DTMF, if we don't enable here, then on module restart we won't see
		* HW DTMF available and will use software */
		if (zap_channel_test_feature(zchan, ZAP_CHANNEL_FEATURE_DTMF_DETECT)) {
			wanpipe_tdm_api_t tdm_api;
			int err;
			memset(&tdm_api, 0, sizeof(tdm_api));
			err = sangoma_tdm_enable_dtmf_events(zchan->sockfd, &tdm_api);
			if (err) {
				zap_log(ZAP_LOG_WARNING, "Failed to enable Sangoma HW DTMF on channel %d:%d at destroy\n", 
						zchan->span_id, zchan->chan_id);
			}
		}
		sangoma_close(&zchan->sockfd);
	}

	return ZAP_SUCCESS;
}

/**
 * \brief Loads wanpipe IO module
 * \param zio Openzap IO interface
 * \return Success
 */
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
	wanpipe_interface.poll_event = wanpipe_poll_event;
	wanpipe_interface.next_event = wanpipe_next_event;
	wanpipe_interface.channel_destroy = wanpipe_channel_destroy;
	wanpipe_interface.get_alarms = wanpipe_get_alarms;
	*zio = &wanpipe_interface;

	return ZAP_SUCCESS;
}

/**
 * \brief Unloads wanpipe IO module
 * \return Success
 */
static ZIO_IO_UNLOAD_FUNCTION(wanpipe_destroy)
{
	memset(&wanpipe_interface, 0, sizeof(wanpipe_interface));
	return ZAP_SUCCESS;
}

/**
 * \brief Openzap wanpipe IO module definition
 */
EX_DECLARE_DATA zap_module_t zap_module = { 
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet 
 */
