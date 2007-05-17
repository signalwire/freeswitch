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

#ifndef __WINDOWS__
#if defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32)
#define __WINDOWS__
#endif
#endif

#include "openzap.h"
#include "zap_wanpipe.h"

#ifdef __WINDOWS__
#ifdef _MSC_VER
/* disable warning for zero length array in a struct */
/* this will cause errors on c99 and ansi compliant compilers and will need to be fixed in the wanpipe header files */
#pragma warning(disable:4200 4201 4214)
#endif
#include <windows.h>
#include <winioctl.h>
#include <conio.h>
#include <stddef.h>
typedef unsigned __int16 u_int16_t;
typedef unsigned __int32 u_int32_t;
#endif

#include <wanpipe_defines.h>
#include <wanpipe_cfg.h>
#include <wanpipe_tdm_api.h>
#ifdef __WINDOWS__
#include <sang_status_defines.h>
#include <sang_api.h>
#endif
#include <sdla_te1_pmc.h>
#include <sdla_te1.h>
#include <sdla_56k.h>
#include <sdla_remora.h>
#include <sdla_te3.h>	
#include <sdla_front_end.h>
#include <sdla_aft_te1.h>
#include <sys/socket.h>

static zap_software_interface_t wanpipe_interface;

struct wanpipe_channel {
	struct zap_channel zchan;
	int x;
};



unsigned wp_open_lapd(zap_span_t *span, const char *name)
{
	struct wan_sockaddr_ll  sa;
	zap_socket_t sock = -1;

	errno = 0;
	sock = socket(AF_WANPIPE, SOCK_RAW, 0);
	memset(&sa,0,sizeof(struct wan_sockaddr_ll));

	zap_log(ZAP_LOG_DEBUG, "attempting to open lapd device socket %s\n", name);

	if( sock < 0 ) {
		zap_log(ZAP_LOG_ERROR, "socket error!\n");
		return 0;
	} 

	zap_copy_string(sa.sll_device, name, sizeof(sa.sll_device));
	zap_copy_string(sa.sll_card, "wanpipe1", sizeof(sa.sll_card));

	sa.sll_protocol = htons(PVC_PROT);
	sa.sll_family = AF_WANPIPE;

	if(bind(sock, (struct sockaddr *)&sa, sizeof(struct wan_sockaddr_ll)) < 0){
		zap_log(ZAP_LOG_ERROR, "socket bind error!\n");
        return 0;
	}

	{
		unsigned customer_id;
		int err = ioctl(sock,SIOC_AFT_CUSTOMER_ID,&customer_id);
		if (err){
			perror("Customer ID: ");
		}else{
			printf("Customer ID 0x%X\n",customer_id);
			return 1;
		}


	}

	
	return 0;
}


static zap_socket_t wp_open_device(int span, int chan) 
{
   	char fname[256];
#if defined(WIN32)

	_snprintf(fname , FNAME_LEN, "\\\\.\\WANPIPE%d_IF%d", span, chan - 1);

	return CreateFile(	fname, 
						GENERIC_READ | GENERIC_WRITE, 
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						(LPSECURITY_ATTRIBUTES)NULL, 
						OPEN_EXISTING,
						FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
						(HANDLE)NULL
						);
#else
  	int fd=-1;

	sprintf(fname, "/dev/wptdm_s%dc%d", span, chan);

	fd = open(fname, O_RDWR);

	return fd;  
#endif
}            

static unsigned wp_open_range(zap_span_t *span, unsigned spanno, unsigned start, unsigned end, zap_chan_type_t type)
{
	unsigned configured = 0, x;

	for(x = start; x < end; x++) {
		zap_channel_t *chan;
		zap_socket_t sockfd = -1;
		
		sockfd = wp_open_device(spanno, x);
		
		if (sockfd > -1 && zap_span_add_channel(span, sockfd, type, &chan) == ZAP_SUCCESS) {
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
	int x;
	unsigned configured = 0;

	assert(str != NULL);
	
	if (type == ZAP_CHAN_TYPE_DQ921) {
		char *flag, *name;
		
		flag = str;
		if ((name = strchr(flag, ':'))) {
			name++;
		} else {
			name = flag;
			flag = NULL;
		}

		if (flag) {
			if (strstr(flag, "q931")) {
				type = ZAP_CHAN_TYPE_DQ931;
				configured += wp_open_lapd(span, name);
				return configured;
			}

		}
	}

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
		if (!strcasecmp(cfg.category, "span")) {
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
				zap_log(ZAP_LOG_WARNING, "'enabled' param ignored when it's not the first param in a [span]\n");
			} else if (!strcasecmp(var, "b-channel")) {
				configured += wp_configure_channel(&cfg, val, span, ZAP_CHAN_TYPE_B);
			} else if (!strcasecmp(var, "d-channel")) {
				if (d) {
					zap_log(ZAP_LOG_WARNING, "ignoring extra d-channel\n");
				} else {
					configured += wp_configure_channel(&cfg, val, span, ZAP_CHAN_TYPE_DQ921);
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
	ZINT_OPEN_MUZZLE;
	return ZAP_FAIL;
}

static ZINT_CLOSE_FUNCTION(wanpipe_close)
{
	ZINT_CLOSE_MUZZLE;
	return ZAP_FAIL;
}

static ZINT_SET_CODEC_FUNCTION(wanpipe_set_codec)
{
	ZINT_SET_CODEC_MUZZLE;
	return ZAP_FAIL;
}

static ZINT_SET_INTERVAL_FUNCTION(wanpipe_set_interval)
{
	ZINT_SET_INTERVAL_MUZZLE;
	return ZAP_FAIL;
}

static ZINT_WAIT_FUNCTION(wanpipe_wait)
{
	ZINT_WAIT_MUZZLE;
	return ZAP_FAIL;
}

static ZINT_READ_FUNCTION(wanpipe_read)
{
	ZINT_READ_MUZZLE;
	return ZAP_FAIL;
}

static ZINT_WRITE_FUNCTION(wanpipe_write)
{
	ZINT_WRITE_MUZZLE;
	return ZAP_FAIL;
}

zap_status_t wanpipe_init(zap_software_interface_t **zint)
{
	assert(zint != NULL);
	memset(&wanpipe_interface, 0, sizeof(wanpipe_interface));

	wanpipe_interface.name = "wanpipe";
	wanpipe_interface.configure =  wanpipe_configure;
	wanpipe_interface.open = wanpipe_open;
	wanpipe_interface.close = wanpipe_close;
	wanpipe_interface.set_codec = wanpipe_set_codec;
	wanpipe_interface.set_interval = wanpipe_set_interval;
	wanpipe_interface.wait = wanpipe_wait;
	wanpipe_interface.read = wanpipe_read;
	wanpipe_interface.write = wanpipe_write;
	*zint = &wanpipe_interface;

	return ZAP_SUCCESS;
}

zap_status_t wanpipe_destroy(void)
{
	int i,j;

	for(i = 1; i <= wanpipe_interface.span_index; i++) {
		zap_span_t *cur_span = &wanpipe_interface.spans[i];

		if (zap_test_flag(cur_span, ZAP_SPAN_CONFIGURED)) {
			for(j = 1; j <= cur_span->chan_count; j++) {
				zap_channel_t *cur_chan = &cur_span->channels[j];
				if (zap_test_flag(cur_chan, ZAP_CHANNEL_CONFIGURED)) {
					zap_log(ZAP_LOG_INFO, "Closing channel %u:%u fd:%d\n", cur_chan->span_id, cur_chan->chan_id, cur_chan->sockfd);
					zap_socket_close(cur_chan->sockfd);
				}
			}
		}
	}

	memset(&wanpipe_interface, 0, sizeof(wanpipe_interface));
	
	return ZAP_SUCCESS;
}
