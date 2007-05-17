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

static zap_software_interface_t wanpipe_interface;

struct wanpipe_channel {
	struct zap_channel zchan;
	int x;
};

static ZINT_CONFIGURE_FUNCTION(wanpipe_configure)
{
	zap_config_t cfg;
	char *var, *val;
	int catno = -1;
	zap_span_t *span = NULL;

	ZINT_CONFIGURE_MUZZLE;

	zap_log(ZAP_LOG_DEBUG, "configuring wanpipe\n");
	if (!zap_config_open_file(&cfg, "wanpipe.conf")) {
		return ZAP_FAIL;
	}
	
	while (zap_config_next_pair(&cfg, &var, &val)) {
		if (!strcasecmp(cfg.category, "span")) {
			if (cfg.catno != catno) {
				zap_log(ZAP_LOG_DEBUG, "found config for span %d\n", cfg.catno);
				catno = cfg.catno;
				if (zap_span_create(&wanpipe_interface, &span) == ZAP_SUCCESS) {
					zap_log(ZAP_LOG_DEBUG, "created span %d\n", span->span_id);
				} else {
					zap_log(ZAP_LOG_CRIT, "failure creating span\n");
					span = NULL;
				}
				
				continue;
			}
			
			if (!span) {
				continue;
			}

			zap_log(ZAP_LOG_DEBUG, "span %d [%s]=[%s]\n", span->span_id, var, val);
		}
	}
	zap_config_close_file(&cfg);

	return ZAP_FAIL;
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
	return ZAP_FAIL;
}
