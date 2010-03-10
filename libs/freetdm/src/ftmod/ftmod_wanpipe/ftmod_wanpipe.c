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
 * David Yat Sin <davidy@sangoma.com>
 * Nenad Corbic <ncorbic@sangoma.com>
 *
 */

#ifdef __sun
#include <unistd.h>
#include <stropts.h>
#endif
#include "freetdm.h"
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

/*! Starting with libsangoma 3 we can use the new libsangoma waitable API, the poor souls of those using a release where LIBSANGOMA version
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

FIO_SPAN_POLL_EVENT_FUNCTION(wanpipe_poll_event);
FIO_SPAN_NEXT_EVENT_FUNCTION(wanpipe_next_event);

#define WP_INVALID_SOCKET -1 

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
static __inline__ int tdmv_api_wait_socket(ftdm_channel_t *ftdmchan, int timeout, int *flags)
{
	
#ifdef LIBSANGOMA_VERSION
	int err;
    uint32_t inflags = *flags;
    uint32_t outflags = 0;
	sangoma_wait_obj_t *sangoma_wait_obj = ftdmchan->mod_data;

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
    pfds[0].fd = ftdmchan->sockfd;
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

static ftdm_io_interface_t wanpipe_interface;

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
 * \param span FreeTDM span
 * \param spanno Wanpipe span number
 * \param start Initial wanpipe channel number
 * \param end Final wanpipe channel number
 * \param type FreeTDM channel type
 * \param name FreeTDM span name
 * \param number FreeTDM span number
 * \param cas_bits CAS bits
 * \return number of spans configured
 */
static unsigned wp_open_range(ftdm_span_t *span, unsigned spanno, unsigned start, unsigned end, ftdm_chan_type_t type, char *name, char *number, unsigned char cas_bits)
{
	unsigned configured = 0, x;
#ifdef LIBSANGOMA_VERSION
	sangoma_status_t sangstatus;
	sangoma_wait_obj_t *sangoma_wait_obj;
#endif

	if (type == FTDM_CHAN_TYPE_CAS) {
		ftdm_log(FTDM_LOG_DEBUG, "Configuring Wanpipe CAS channels with abcd == 0x%X\n", cas_bits);
	}	
	for(x = start; x < end; x++) {
		ftdm_channel_t *chan;
		ftdm_socket_t sockfd = WP_INVALID_SOCKET;
		const char *dtmf = "none";
		if (!strncasecmp(span->name, "smg_prid_nfas", 8) && span->trunk_type == FTDM_TRUNK_T1 && x == 24) {
#ifdef LIBSANGOMA_VERSION
			sockfd = __tdmv_api_open_span_chan(spanno, x);
#else
			ftdm_log(FTDM_LOG_ERROR, "span %d channel %d cannot be configured as smg_prid_nfas, you need to compile freetdm with newer libsangoma\n", spanno, x);
#endif
		} else {
			sockfd = tdmv_api_open_span_chan(spanno, x);
		}

		if (sockfd == WP_INVALID_SOCKET) {
			ftdm_log(FTDM_LOG_ERROR, "Failed to open wanpipe device span %d channel %d\n", spanno, x);
			continue;
		}
		
		if (ftdm_span_add_channel(span, sockfd, type, &chan) == FTDM_SUCCESS) {
			wanpipe_tdm_api_t tdm_api;
			memset(&tdm_api, 0, sizeof(tdm_api));
#ifdef LIBSANGOMA_VERSION
			/* we need SANGOMA_DEVICE_WAIT_OBJ_SIG and not SANGOMA_DEVICE_WAIT_OBJ alone because we need to call 
			 * sangoma_wait_obj_sig to wake up any I/O waiters when closing the channel (typically on ftdm shutdown)
			 * this adds an extra pair of file descriptors to the waitable object
			 * */
			sangstatus = sangoma_wait_obj_create(&sangoma_wait_obj, sockfd, SANGOMA_DEVICE_WAIT_OBJ_SIG);
			if (sangstatus != SANG_STATUS_SUCCESS) {
				ftdm_log(FTDM_LOG_ERROR, "failure create waitable object for s%dc%d\n", spanno, x);
				continue;
			}
			chan->mod_data = sangoma_wait_obj;
#endif
			
			chan->physical_span_id = spanno;
			chan->physical_chan_id = x;
			chan->rate = 8000;
			
			if (type == FTDM_CHAN_TYPE_FXS || type == FTDM_CHAN_TYPE_FXO || type == FTDM_CHAN_TYPE_B) {
				int err;
				
				dtmf = "software";

				/* FIXME: Handle Error Condition Check for return code */
				err = sangoma_tdm_get_hw_coding(chan->sockfd, &tdm_api);

				if (tdm_api.wp_tdm_cmd.hw_tdm_coding) {
					chan->native_codec = chan->effective_codec = FTDM_CODEC_ALAW;
				} else {
					chan->native_codec = chan->effective_codec = FTDM_CODEC_ULAW;
				}

				//err = sangoma_tdm_get_hw_dtmf(chan->sockfd, &tdm_api);
				//if (err > 0) {
					err = sangoma_tdm_enable_dtmf_events(chan->sockfd, &tdm_api);
					if (err == 0) {
						ftdm_channel_set_feature(chan, FTDM_CHANNEL_FEATURE_DTMF_DETECT);
						dtmf = "hardware";
					}
				//}
			}

#ifdef LIBSANGOMA_VERSION
			if (type == FTDM_CHAN_TYPE_FXS) {
				if (sangoma_tdm_disable_ring_trip_detect_events(chan->sockfd, &tdm_api)) {
					/* we had problems of on-hook/off-hook detection due to how ring trip events were handled
					 * if this fails, I believe we will still work ok as long as we dont handle them incorrectly */
					ftdm_log(FTDM_LOG_WARNING, "Failed to disable ring trip events in channel s%dc%d\n", spanno, x);
				}
			}
#endif
#if 0
            if (type == FTDM_CHAN_TYPE_FXS || type == FTDM_CHAN_TYPE_FXO) {
                /* Enable FLASH/Wink Events */
                int err=sangoma_set_rm_rxflashtime(chan->sockfd, &tdm_api, wp_globals.flash_ms);
                if (err == 0) {
			        ftdm_log(FTDM_LOG_ERROR, "flash enabled s%dc%d\n", spanno, x);
                } else {
			        ftdm_log(FTDM_LOG_ERROR, "flash disabled s%dc%d\n", spanno, x);
                }
            }
#endif

			if (type == FTDM_CHAN_TYPE_CAS || type == FTDM_CHAN_TYPE_EM) {
#ifdef LIBSANGOMA_VERSION
				sangoma_tdm_write_rbs(chan->sockfd,&tdm_api,chan->physical_chan_id, wanpipe_swap_bits(cas_bits));

				/* this should probably be done for old libsangoma but I am not sure if the API is available and I'm lazy to check,
				   The poll rate is hard coded to 100 per second (done in the driver, is the max rate of polling allowed by wanpipe)
				 */
				if (sangoma_tdm_enable_rbs_events(chan->sockfd, &tdm_api, 100)) {
					ftdm_log(FTDM_LOG_ERROR, "Failed to enable RBS/CAS events in device %d:%d fd:%d\n", chan->span_id, chan->chan_id, sockfd);
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
					ftdm_log(FTDM_LOG_ERROR, "Failed to enable RBS/CAS events in device %d:%d fd:%d\n", chan->span_id, chan->chan_id, sockfd);
				}
				/* probably done by the driver but lets write defensive code this time */
				sangoma_tdm_flush_bufs(chan->sockfd, &tdm_api);
				sangoma_tdm_write_rbs(chan->sockfd,&tdm_api, wanpipe_swap_bits(cas_bits));
#endif
			}
			
			if (!ftdm_strlen_zero(name)) {
				ftdm_copy_string(chan->chan_name, name, sizeof(chan->chan_name));
			}

			if (!ftdm_strlen_zero(number)) {
				ftdm_copy_string(chan->chan_number, number, sizeof(chan->chan_number));
			}
			configured++;
			ftdm_log(FTDM_LOG_INFO, "configuring device s%dc%d as FreeTDM device %d:%d fd:%d DTMF: %s\n",
				spanno, x, chan->span_id, chan->chan_id, sockfd, dtmf);

		} else {
			ftdm_log(FTDM_LOG_ERROR, "ftdm_span_add_channel failed for wanpipe span %d channel %d\n", spanno, x);
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
static FIO_CONFIGURE_FUNCTION(wanpipe_configure)
{
	int num;

	if (!strcasecmp(category, "defaults")) {
		if (!strcasecmp(var, "codec_ms")) {
			num = atoi(val);
			if (num < 10 || num > 60) {
				ftdm_log(FTDM_LOG_WARNING, "invalid codec ms at line %d\n", lineno);
			} else {
				wp_globals.codec_ms = num;
			}
		} else if (!strcasecmp(var, "wink_ms")) {
			num = atoi(val);
			if (num < 50 || num > 3000) {
				ftdm_log(FTDM_LOG_WARNING, "invalid wink ms at line %d\n", lineno);
			} else {
				wp_globals.wink_ms = num;
			}
		} else if (!strcasecmp(var, "flash_ms")) {
			num = atoi(val);
			if (num < 50 || num > 3000) {
				ftdm_log(FTDM_LOG_WARNING, "invalid flash ms at line %d\n", lineno);
			} else {
				wp_globals.flash_ms = num;
			}
		} else if (!strcasecmp(var, "ring_on_ms")) {
			num = atoi(val);
			if (num < 500 || num > 5000) {
				ftdm_log(FTDM_LOG_WARNING, "invalid ring_on_ms at line %d (valid range 500 to 5000)\n", lineno);
			} else {
				wp_globals.ring_on_ms = num;
			}
		} else if (!strcasecmp(var, "ring_off_ms")) {
			num = atoi(val);
			if (num < 500 || num > 5000) {
				ftdm_log(FTDM_LOG_WARNING, "invalid ring_off_ms at line %d (valid range 500 to 5000)\n", lineno);
			} else {
				wp_globals.ring_off_ms = num;
			}
		}
	}

	return FTDM_SUCCESS;
}

/**
 * \brief Initialises an freetdm Wanpipe span from a configuration string
 * \param span FreeTDM span
 * \param str Configuration string
 * \param type FreeTDM span type
 * \param name FreeTDM span name
 * \param number FreeTDM span number
 * \return Success or failure
 */
static FIO_CONFIGURE_SPAN_FUNCTION(wanpipe_configure_span)
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
	

	mydata = ftdm_strdup(str);
	assert(mydata != NULL);


	items = ftdm_separate_string(mydata, ',', item_list, (sizeof(item_list) / sizeof(item_list[0])));

	for(i = 0; i < items; i++) {
		sp = item_list[i];
		if ((ch = strchr(sp, ':'))) {
			*ch++ = '\0';
		}

		if (!(sp && ch)) {
			ftdm_log(FTDM_LOG_ERROR, "No valid wanpipe span and channel was specified\n");
			continue;
		}

		channo = atoi(ch);
		spanno = atoi(sp);

		if (channo < 0) {
			ftdm_log(FTDM_LOG_ERROR, "Invalid channel number %d\n", channo);
			continue;
		}

		if (spanno < 0) {
			ftdm_log(FTDM_LOG_ERROR, "Invalid span number %d\n", channo);
			continue;
		}
		
		if ((mx = strchr(ch, '-'))) {
			mx++;
			top = atoi(mx) + 1;
		} else {
			top = channo + 1;
		}
		
		
		if (top < 0) {
			ftdm_log(FTDM_LOG_ERROR, "Invalid range number %d\n", top);
			continue;
		}
		if (FTDM_CHAN_TYPE_CAS == type && ftdm_config_get_cas_bits(ch, &cas_bits)) {
			ftdm_log(FTDM_LOG_ERROR, "Failed to get CAS bits in CAS channel\n");
			continue;
		}
		configured += wp_open_range(span, spanno, channo, top, type, name, number, cas_bits);

	}
	
	ftdm_safe_free(mydata);

	return configured;
}

/**
 * \brief Opens Wanpipe channel
 * \param ftdmchan Channel to open
 * \return Success or failure
 */
static FIO_OPEN_FUNCTION(wanpipe_open) 
{

	wanpipe_tdm_api_t tdm_api;

	memset(&tdm_api,0,sizeof(tdm_api));
	sangoma_tdm_flush_bufs(ftdmchan->sockfd, &tdm_api);
#ifdef LIBSANGOMA_VERSION
	sangoma_flush_event_bufs(ftdmchan->sockfd, &tdm_api);
#endif

	if (ftdmchan->type == FTDM_CHAN_TYPE_DQ921 || ftdmchan->type == FTDM_CHAN_TYPE_DQ931) {
		ftdmchan->native_codec = ftdmchan->effective_codec = FTDM_CODEC_NONE;
	} else {
		ftdmchan->effective_codec = ftdmchan->native_codec;
		
		sangoma_tdm_set_usr_period(ftdmchan->sockfd, &tdm_api, wp_globals.codec_ms);

		ftdm_channel_set_feature(ftdmchan, FTDM_CHANNEL_FEATURE_INTERVAL);
		ftdmchan->effective_interval = ftdmchan->native_interval = wp_globals.codec_ms;
		ftdmchan->packet_len = ftdmchan->native_interval * 8;
	}

	return FTDM_SUCCESS;
}

/**
 * \brief Closes Wanpipe channel
 * \param ftdmchan Channel to close
 * \return Success
 */
static FIO_CLOSE_FUNCTION(wanpipe_close)
{
#ifdef LIBSANGOMA_VERSION
	sangoma_wait_obj_t *waitobj = ftdmchan->mod_data;
	/* kick any I/O waiters */
	sangoma_wait_obj_signal(waitobj);
#endif
	return FTDM_SUCCESS;
}

/**
 * \brief Executes an FreeTDM command on a Wanpipe channel
 * \param ftdmchan Channel to execute command on
 * \param command FreeTDM command to execute
 * \param obj Object (unused)
 * \return Success or failure
 */
static FIO_COMMAND_FUNCTION(wanpipe_command)
{
	wanpipe_tdm_api_t tdm_api;
	int err = 0;

	memset(&tdm_api, 0, sizeof(tdm_api));

	switch(command) {
	case FTDM_COMMAND_OFFHOOK:
		{
			err=sangoma_tdm_txsig_offhook(ftdmchan->sockfd,&tdm_api);
			if (err) {
				snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "OFFHOOK Failed");
				return FTDM_FAIL;
			}
			ftdm_set_flag_locked(ftdmchan, FTDM_CHANNEL_OFFHOOK);
		}
		break;
	case FTDM_COMMAND_ONHOOK:
		{
			err=sangoma_tdm_txsig_onhook(ftdmchan->sockfd,&tdm_api);
			if (err) {
				snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "ONHOOK Failed");
				return FTDM_FAIL;
			}
			ftdm_clear_flag_locked(ftdmchan, FTDM_CHANNEL_OFFHOOK);
		}
		break;
	case FTDM_COMMAND_GENERATE_RING_ON:
		{
			err=sangoma_tdm_txsig_start(ftdmchan->sockfd,&tdm_api);
			if (err) {
				snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "Ring Failed");
				return FTDM_FAIL;
			}
			ftdm_set_flag_locked(ftdmchan, FTDM_CHANNEL_RINGING);
			ftdm_set_pflag_locked(ftdmchan, WP_RINGING);
			ftdmchan->ring_time = ftdm_current_time_in_ms() + wp_globals.ring_on_ms;
		}
		break;
	case FTDM_COMMAND_GENERATE_RING_OFF:
		{
			err=sangoma_tdm_txsig_offhook(ftdmchan->sockfd,&tdm_api);
			if (err) {
				snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "Ring-off Failed");
				return FTDM_FAIL;
			}
			ftdm_clear_pflag_locked(ftdmchan, WP_RINGING);
			ftdm_clear_flag_locked(ftdmchan, FTDM_CHANNEL_RINGING);
		}
		break;
	case FTDM_COMMAND_GET_INTERVAL:
		{
			err=sangoma_tdm_get_usr_period(ftdmchan->sockfd, &tdm_api);
			if (err > 0 ) {
				FTDM_COMMAND_OBJ_INT = err;
				err=0;
			}
		}
		break;
	case FTDM_COMMAND_ENABLE_ECHOCANCEL:
		{
			err=sangoma_tdm_enable_hwec(ftdmchan->sockfd, &tdm_api);
			if (err) {
             	snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "HWEC Enable Failed");
				return FTDM_FAIL;
			}
		}
		break;
	case FTDM_COMMAND_DISABLE_ECHOCANCEL:
		{
			err=sangoma_tdm_disable_hwec(ftdmchan->sockfd, &tdm_api);
			if (err) {
             	snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "HWEC Disable Failed");
				return FTDM_FAIL;
			}
		}
		break;
	case FTDM_COMMAND_ENABLE_LOOP:
		{
#ifdef WP_API_FEATURE_LOOP
         	err=sangoma_tdm_enable_loop(ftdmchan->sockfd, &tdm_api);
			if (err) {
				snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "Loop Enable Failed");
				return FTDM_FAIL;
			}
#endif		
		}
	case FTDM_COMMAND_DISABLE_LOOP:
		{
#ifdef WP_API_FEATURE_LOOP
         	err=sangoma_tdm_disable_loop(ftdmchan->sockfd, &tdm_api);
			if (err) {
				snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "Loop Disable Failed");
				return FTDM_FAIL;
			}
#endif	 
		}
	case FTDM_COMMAND_SET_INTERVAL: 
		{
			err=sangoma_tdm_set_usr_period(ftdmchan->sockfd, &tdm_api, FTDM_COMMAND_OBJ_INT);
			ftdmchan->packet_len = ftdmchan->native_interval * (ftdmchan->effective_codec == FTDM_CODEC_SLIN ? 16 : 8);
		}
		break;
	case FTDM_COMMAND_SET_CAS_BITS:
		{
#ifdef LIBSANGOMA_VERSION
			err = sangoma_tdm_write_rbs(ftdmchan->sockfd,&tdm_api, ftdmchan->physical_chan_id, wanpipe_swap_bits(FTDM_COMMAND_OBJ_INT));
#else
			err = sangoma_tdm_write_rbs(ftdmchan->sockfd, &tdm_api, wanpipe_swap_bits(FTDM_COMMAND_OBJ_INT));
#endif
		}
		break;
	case FTDM_COMMAND_GET_CAS_BITS:
		{
#ifdef LIBSANGOMA_VERSION
			unsigned char rbsbits;
			err = sangoma_tdm_read_rbs(ftdmchan->sockfd, &tdm_api, ftdmchan->physical_chan_id, &rbsbits);
			if (!err) {
				FTDM_COMMAND_OBJ_INT = wanpipe_swap_bits(rbsbits);
			}
#else
			// does sangoma_tdm_read_rbs is available here?
			FTDM_COMMAND_OBJ_INT = ftdmchan->rx_cas_bits;
#endif
		}
		break;
	case FTDM_COMMAND_SET_LINK_STATUS:
		{
			ftdm_channel_hw_link_status_t status = FTDM_COMMAND_OBJ_INT;
			char sangoma_status = status == FTDM_HW_LINK_CONNECTED ? FE_CONNECTED : FE_DISCONNECTED;
			err = sangoma_tdm_set_fe_status(ftdmchan->sockfd, &tdm_api, sangoma_status);
		}
		break;
	case FTDM_COMMAND_GET_LINK_STATUS:
		{
			unsigned char sangoma_status = 0;
			err = sangoma_tdm_get_fe_status(ftdmchan->sockfd, &tdm_api, &sangoma_status);
			if (!err) {
				FTDM_COMMAND_OBJ_INT = sangoma_status == FE_CONNECTED ? FTDM_HW_LINK_CONNECTED : FTDM_HW_LINK_DISCONNECTED;
			}
		}
		break;
	default:
		break;
	};

	if (err) {
		snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "%s", strerror(errno));
		return FTDM_FAIL;
	}


	return FTDM_SUCCESS;
}

/**
 * \brief Reads data from a Wanpipe channel
 * \param ftdmchan Channel to read from
 * \param data Data buffer
 * \param datalen Size of data buffer
 * \return Success, failure or timeout
 */
static FIO_READ_FUNCTION(wanpipe_read)
{
	int rx_len = 0;
	wp_tdm_api_rx_hdr_t hdrframe;

	memset(&hdrframe, 0, sizeof(hdrframe));

	rx_len = sangoma_readmsg_tdm(ftdmchan->sockfd, &hdrframe, (int)sizeof(hdrframe), data, (int)*datalen,0);
	*datalen = rx_len;

	if (rx_len == 0 || rx_len == -17) {
		return FTDM_TIMEOUT;
	}

	if (rx_len < 0) {
		snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "%s", strerror(errno));
		return FTDM_FAIL;
	} 


	return FTDM_SUCCESS;
}

/**
 * \brief Writes data to a Wanpipe channel
 * \param ftdmchan Channel to write to
 * \param data Data buffer
 * \param datalen Size of data buffer
 * \return Success or failure
 */
static FIO_WRITE_FUNCTION(wanpipe_write)
{
	int bsent;
	wp_tdm_api_tx_hdr_t hdrframe;

	/* Do we even need the headerframe here? on windows, we don't even pass it to the driver */
	memset(&hdrframe, 0, sizeof(hdrframe));
	if (*datalen == 0) {
		return FTDM_SUCCESS;
	}
	bsent = sangoma_writemsg_tdm(ftdmchan->sockfd, &hdrframe, (int)sizeof(hdrframe), data, (unsigned short)(*datalen),0);

	/* should we be checking if bsent == *datalen here? */
	if (bsent > 0) {
		*datalen = bsent;
		return FTDM_SUCCESS;
	}

	return FTDM_FAIL;
}

/**
 * \brief Waits for an event on a Wanpipe channel
 * \param ftdmchan Channel to open
 * \param flags Type of event to wait for
 * \param to Time to wait (in ms)
 * \return Success, failure or timeout
 */

static FIO_WAIT_FUNCTION(wanpipe_wait)
{
	int32_t inflags = 0;
	int result;

	if (*flags & FTDM_READ) {
		inflags |= POLLIN;
	}

	if (*flags & FTDM_WRITE) {
		inflags |= POLLOUT;
	}

	if (*flags & FTDM_EVENTS) {
		inflags |= POLLPRI;
	}

	result = tdmv_api_wait_socket(ftdmchan, to, &inflags);

	*flags = FTDM_NO_FLAGS;

	if (result < 0){
		snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "Poll failed");
		return FTDM_FAIL;
	}

	if (result == 0) {
		return FTDM_TIMEOUT;
	}

	if (inflags & POLLIN) {
		*flags |= FTDM_READ;
	}

	if (inflags & POLLOUT) {
		*flags |= FTDM_WRITE;
	}

	if (inflags & POLLPRI) {
		*flags |= FTDM_EVENTS;
	}

	return FTDM_SUCCESS;
}

/**
 * \brief Checks for events on a Wanpipe span
 * \param span Span to check for events
 * \param ms Time to wait for event
 * \return Success if event is waiting or failure if not
 */
FIO_SPAN_POLL_EVENT_FUNCTION(wanpipe_poll_event)
{
#ifdef LIBSANGOMA_VERSION
	sangoma_status_t sangstatus;
	sangoma_wait_obj_t *pfds[FTDM_MAX_CHANNELS_SPAN] = { 0 };
	uint32_t inflags[FTDM_MAX_CHANNELS_SPAN];
	uint32_t outflags[FTDM_MAX_CHANNELS_SPAN];
#else
	struct pollfd pfds[FTDM_MAX_CHANNELS_SPAN];
#endif
	uint32_t i, j = 0, k = 0, l = 0;
	int r;
	
	for(i = 1; i <= span->chan_count; i++) {
		ftdm_channel_t *ftdmchan = span->channels[i];
#ifdef LIBSANGOMA_VERSION
		if (!ftdmchan->mod_data) {
			continue; /* should never happen but happens when shutting down */
		}
		pfds[j] = ftdmchan->mod_data;
		inflags[j] = POLLPRI;
#else
		memset(&pfds[j], 0, sizeof(pfds[j]));
		pfds[j].fd = span->channels[i]->sockfd;
		pfds[j].events = POLLPRI;
#endif

		/* The driver probably should be able to do this wink/flash/ringing by itself this is sort of a hack to make it work! */
		if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_WINK) || ftdm_test_flag(ftdmchan, FTDM_CHANNEL_FLASH)) {
			l = 5;
		}

		j++;

		if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_RINGING)) {
			l = 5;
		}

		if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_RINGING) && ftdm_current_time_in_ms() >= ftdmchan->ring_time) {
			wanpipe_tdm_api_t tdm_api;
			int err;
			memset(&tdm_api, 0, sizeof(tdm_api));
			if (ftdm_test_pflag(ftdmchan, WP_RINGING)) {
				err = sangoma_tdm_txsig_offhook(ftdmchan->sockfd,&tdm_api);
				if (err) {
					snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "Ring-off Failed");
					ftdm_log(FTDM_LOG_ERROR, "sangoma_tdm_txsig_offhook failed\n");
					return FTDM_FAIL;
				}
				ftdm_clear_pflag_locked(ftdmchan, WP_RINGING);
				ftdmchan->ring_time = ftdm_current_time_in_ms() + wp_globals.ring_off_ms;
			} else {
				err=sangoma_tdm_txsig_start(ftdmchan->sockfd,&tdm_api);
				if (err) {
					snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "Ring Failed");
					ftdm_log(FTDM_LOG_ERROR, "sangoma_tdm_txsig_start failed\n");
					return FTDM_FAIL;
				}
				ftdm_set_pflag_locked(ftdmchan, WP_RINGING);
				ftdmchan->ring_time = ftdm_current_time_in_ms() + wp_globals.ring_on_ms;
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
		ftdm_log(FTDM_LOG_ERROR, "sangoma_waitfor_many failed: %d, %s\n", sangstatus, strerror(errno));
		r = -1;
	}
#else
	r = poll(pfds, j, ms);
#endif
	
	if (r == 0) {
		return l ? FTDM_SUCCESS : FTDM_TIMEOUT;
	} else if (r < 0) {
		snprintf(span->last_error, sizeof(span->last_error), "%s", strerror(errno));
		return FTDM_FAIL;
	}
	
	for(i = 1; i <= span->chan_count; i++) {
		ftdm_channel_t *ftdmchan = span->channels[i];

#ifdef LIBSANGOMA_VERSION
		if (outflags[i-1] & POLLPRI) {
#else
		if (pfds[i-1].revents & POLLPRI) {
#endif
			ftdm_set_flag(ftdmchan, FTDM_CHANNEL_EVENT);
			ftdmchan->last_event_time = ftdm_current_time_in_ms();
			k++;
		}
	}
	/* when k is 0 it might be that an async wanpipe device signal was delivered */	
	return FTDM_SUCCESS;
}

/**
 * \brief Gets alarms from a Wanpipe Channel
 * \param ftdmchan Channel to get alarms from
 * \return Success or failure
 */
static FIO_GET_ALARMS_FUNCTION(wanpipe_get_alarms)
{
	wanpipe_tdm_api_t tdm_api;
	unsigned int alarms = 0;
	int err;

	memset(&tdm_api,0,sizeof(tdm_api));

#ifdef LIBSANGOMA_VERSION
	if ((err = sangoma_tdm_get_fe_alarms(ftdmchan->sockfd, &tdm_api, &alarms))) {
		snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "ioctl failed (%s)", strerror(errno));
		snprintf(ftdmchan->span->last_error, sizeof(ftdmchan->span->last_error), "ioctl failed (%s)", strerror(errno));
		return FTDM_FAIL;		
	}
#else
	if ((err = sangoma_tdm_get_fe_alarms(ftdmchan->sockfd, &tdm_api)) < 0){
		snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "ioctl failed (%s)", strerror(errno));
		snprintf(ftdmchan->span->last_error, sizeof(ftdmchan->span->last_error), "ioctl failed (%s)", strerror(errno));
		return FTDM_FAIL;		
	}
	alarms = tdm_api.wp_tdm_cmd.fe_alarms;
#endif
	ftdmchan->alarm_flags = FTDM_ALARM_NONE;

	if (alarms & WAN_TE_BIT_ALARM_RED) {
		ftdmchan->alarm_flags |= FTDM_ALARM_RED;
		alarms &= ~WAN_TE_BIT_ALARM_RED;
	}

	if (alarms & WAN_TE_BIT_ALARM_AIS) {
		ftdmchan->alarm_flags |= FTDM_ALARM_AIS;
		ftdmchan->alarm_flags |= FTDM_ALARM_BLUE;
		alarms &= ~WAN_TE_BIT_ALARM_AIS;
	}

	if (alarms & WAN_TE_BIT_ALARM_RAI) {
		ftdmchan->alarm_flags |= FTDM_ALARM_RAI;
		ftdmchan->alarm_flags |= FTDM_ALARM_YELLOW;
		alarms &= ~WAN_TE_BIT_ALARM_RAI;
	}

	/* still missing to map:
	 * FTDM_ALARM_RECOVER
	 * FTDM_ALARM_LOOPBACK
	 * FTDM_ALARM_NOTOPEN
	 * */

	/* if we still have alarms that we did not map, set the general alarm */
	if (alarms) {
		ftdm_log(FTDM_LOG_DEBUG, "Unmapped wanpipe alarms: %d\n", alarms);
		ftdmchan->alarm_flags |= FTDM_ALARM_GENERAL;
	}

	return FTDM_SUCCESS;
}

/**
 * \brief Retrieves an event from a wanpipe span
 * \param span Span to retrieve event from
 * \param event FreeTDM event to return
 * \return Success or failure
 */
FIO_SPAN_NEXT_EVENT_FUNCTION(wanpipe_next_event)
{
	uint32_t i,err;
	ftdm_oob_event_t event_id;
	for(i = 1; i <= span->chan_count; i++) {
		if (span->channels[i]->last_event_time && !ftdm_test_flag(span->channels[i], FTDM_CHANNEL_EVENT)) {
			uint32_t diff = (uint32_t)(ftdm_current_time_in_ms() - span->channels[i]->last_event_time);
			/* XX printf("%u %u %u\n", diff, (unsigned)ftdm_current_time_in_ms(), (unsigned)span->channels[i]->last_event_time); */
			if (ftdm_test_flag(span->channels[i], FTDM_CHANNEL_WINK)) {
				if (diff > wp_globals.wink_ms) {
					ftdm_clear_flag_locked(span->channels[i], FTDM_CHANNEL_WINK);
					ftdm_clear_flag_locked(span->channels[i], FTDM_CHANNEL_FLASH);
					ftdm_set_flag_locked(span->channels[i], FTDM_CHANNEL_OFFHOOK);
					event_id = FTDM_OOB_OFFHOOK;
					goto event;
				}
			}

			if (ftdm_test_flag(span->channels[i], FTDM_CHANNEL_FLASH)) {
				if (diff > wp_globals.flash_ms) {
					ftdm_clear_flag_locked(span->channels[i], FTDM_CHANNEL_FLASH);
					ftdm_clear_flag_locked(span->channels[i], FTDM_CHANNEL_WINK);
					ftdm_clear_flag_locked(span->channels[i], FTDM_CHANNEL_OFFHOOK);
					event_id = FTDM_OOB_ONHOOK;

					if (span->channels[i]->type == FTDM_CHAN_TYPE_FXO) {
						ftdm_channel_t *ftdmchan = span->channels[i];
						wanpipe_tdm_api_t tdm_api;
						memset(&tdm_api, 0, sizeof(tdm_api));

						sangoma_tdm_txsig_onhook(ftdmchan->sockfd,&tdm_api);
					}
					goto event;
				}
			}
		} 
		if (ftdm_test_flag(span->channels[i], FTDM_CHANNEL_EVENT)) {
			wanpipe_tdm_api_t tdm_api;
			ftdm_channel_t *ftdmchan = span->channels[i];
			memset(&tdm_api, 0, sizeof(tdm_api));
			ftdm_clear_flag(span->channels[i], FTDM_CHANNEL_EVENT);

			err = sangoma_tdm_read_event(ftdmchan->sockfd, &tdm_api);
			if (err != FTDM_SUCCESS) {
				snprintf(span->last_error, sizeof(span->last_error), "%s", strerror(errno));
				return FTDM_FAIL;
			}
			
			ftdm_log(FTDM_LOG_DEBUG, "read wanpipe event %d\n", tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_type);
			switch(tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_type) {

			case WP_TDMAPI_EVENT_LINK_STATUS:
				{
					switch(tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_link_status) {
					case WP_TDMAPI_EVENT_LINK_STATUS_CONNECTED:
						event_id = FTDM_OOB_ALARM_CLEAR;
						break;
					default:
						event_id = FTDM_OOB_ALARM_TRAP;
						break;
					};
				}
				break;

			case WP_TDMAPI_EVENT_RXHOOK:
				{
					if (span->channels[i]->type == FTDM_CHAN_TYPE_FXS) {
						event_id = tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_hook_state & WP_TDMAPI_EVENT_RXHOOK_OFF ? FTDM_OOB_OFFHOOK : FTDM_OOB_ONHOOK;
						if (event_id == FTDM_OOB_OFFHOOK) {
							if (ftdm_test_flag(span->channels[i], FTDM_CHANNEL_FLASH)) {
								ftdm_clear_flag_locked(span->channels[i], FTDM_CHANNEL_FLASH);
								ftdm_clear_flag_locked(span->channels[i], FTDM_CHANNEL_WINK);
								event_id = FTDM_OOB_FLASH;
								goto event;
							} else {
								ftdm_set_flag_locked(span->channels[i], FTDM_CHANNEL_WINK);
							}
						} else {
							if (ftdm_test_flag(span->channels[i], FTDM_CHANNEL_WINK)) {
								ftdm_clear_flag_locked(span->channels[i], FTDM_CHANNEL_WINK);
								ftdm_clear_flag_locked(span->channels[i], FTDM_CHANNEL_FLASH);
								event_id = FTDM_OOB_WINK;
								goto event;
							} else {
								ftdm_set_flag_locked(span->channels[i], FTDM_CHANNEL_FLASH);
							}
						}					
						continue;
					} else {
						int err;
						ftdm_channel_t *ftdmchan = span->channels[i];
						err=sangoma_tdm_txsig_onhook(ftdmchan->sockfd,&tdm_api);
						if (err) {
							snprintf(span->channels[i]->last_error, sizeof(span->channels[i]->last_error), "ONHOOK Failed");
							return FTDM_FAIL;
						}
						event_id = tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_hook_state & WP_TDMAPI_EVENT_RXHOOK_OFF ? FTDM_OOB_ONHOOK : FTDM_OOB_NOOP;	
					}
				}
				break;
			case WP_TDMAPI_EVENT_RING_DETECT:
				{
					event_id = tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_ring_state == WP_TDMAPI_EVENT_RING_PRESENT ? FTDM_OOB_RING_START : FTDM_OOB_RING_STOP;
				}
				break;
				/*
				disabled this ones when configuring, we don't need them, do we?
			case WP_TDMAPI_EVENT_RING_TRIP_DETECT:
				{
					event_id = tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_ring_state == WP_TDMAPI_EVENT_RING_PRESENT ? FTDM_OOB_ONHOOK : FTDM_OOB_OFFHOOK;
				}
				break;
				*/
			case WP_TDMAPI_EVENT_RBS:
				{
					event_id = FTDM_OOB_CAS_BITS_CHANGE;
					span->channels[i]->rx_cas_bits = wanpipe_swap_bits(tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_rbs_bits);
				}
				break;
			case WP_TDMAPI_EVENT_DTMF:
				{
					char tmp_dtmf[2] = { tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_dtmf_digit, 0 };
					event_id = FTDM_OOB_NOOP;

					if (tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_dtmf_type == WAN_EC_TONE_PRESENT) {
						ftdm_set_flag_locked(ftdmchan, FTDM_CHANNEL_MUTE);
					}

					if (tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_dtmf_type == WAN_EC_TONE_STOP) {
						ftdm_clear_flag_locked(ftdmchan, FTDM_CHANNEL_MUTE);
						if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_INUSE)) {
							ftdm_log(FTDM_LOG_DEBUG, "Queuing wanpipe DTMF: %c\n", tmp_dtmf[0]);
							ftdm_channel_queue_dtmf(ftdmchan, tmp_dtmf);
						}
					} 
				}
				break;
			case WP_TDMAPI_EVENT_ALARM:
				{
					ftdm_log(FTDM_LOG_DEBUG, "Got wanpipe alarms %d\n", tdm_api.wp_tdm_cmd.event.wp_api_event_alarm);
					event_id = FTDM_OOB_ALARM_TRAP;
				}
				break;
			default:
				{
					ftdm_log(FTDM_LOG_WARNING, "Unhandled wanpipe event %d\n", tdm_api.wp_tdm_cmd.event.wp_tdm_api_event_type);
					event_id = FTDM_OOB_INVALID;
				}
				break;
			}

		event:

			span->channels[i]->last_event_time = 0;
			span->event_header.e_type = FTDM_EVENT_OOB;
			span->event_header.enum_id = event_id;
			span->event_header.channel = span->channels[i];
			*event = &span->event_header;
			return FTDM_SUCCESS;
		}
	}

	return FTDM_FAIL;
	
}

/**
 * \brief Destroys a Wanpipe Channel
 * \param ftdmchan Channel to destroy
 * \return Success
 */
static FIO_CHANNEL_DESTROY_FUNCTION(wanpipe_channel_destroy)
{
#ifdef LIBSANGOMA_VERSION
	if (ftdmchan->mod_data) {
		sangoma_wait_obj_t *sangoma_wait_obj;
		sangoma_wait_obj = ftdmchan->mod_data;
		ftdmchan->mod_data = NULL;
		sangoma_wait_obj_delete(&sangoma_wait_obj);
	}
#endif

	if (ftdmchan->sockfd > -1) {
		close(ftdmchan->sockfd);
		ftdmchan->sockfd = WP_INVALID_SOCKET;
	}

	return FTDM_SUCCESS;
}

/**
 * \brief Loads wanpipe IO module
 * \param fio FreeTDM IO interface
 * \return Success
 */
static FIO_IO_LOAD_FUNCTION(wanpipe_init)
{
	assert(fio != NULL);
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
	*fio = &wanpipe_interface;

	return FTDM_SUCCESS;
}

/**
 * \brief Unloads wanpipe IO module
 * \return Success
 */
static FIO_IO_UNLOAD_FUNCTION(wanpipe_destroy)
{
	memset(&wanpipe_interface, 0, sizeof(wanpipe_interface));
	return FTDM_SUCCESS;
}

/**
 * \brief FreeTDM wanpipe IO module definition
 */
EX_DECLARE_DATA ftdm_module_t ftdm_module = { 
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 
 */
