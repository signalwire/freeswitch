/*
 * Copyright 2008-2015 Arsen Chaloyan
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <apr_general.h>
#include <sofia-sip/su_log.h>
#include "mrcp_sofiasip_logger.h"

APT_LOG_SOURCE_IMPLEMENT(MRCP, sip_log_source, "SOFIASIP")

SOFIAPUBVAR su_log_t tport_log[];      /* Transport event debug */
SOFIAPUBVAR su_log_t nea_log[];        /* Event engine debug */
SOFIAPUBVAR su_log_t nta_log[];        /* Transaction engine debug */
SOFIAPUBVAR su_log_t nua_log[];        /* User Agent engine debug */
SOFIAPUBVAR su_log_t soa_log[];        /* SDP Offer/Answer engine debug */
SOFIAPUBVAR su_log_t su_log_default[]; /* Default debug */

static void mrcp_sofiasip_log(void *stream, char const *format, va_list arg_ptr)
{
	if(format) {
		/* use generic vsnprintf() since apr_vformatter doesn't support 
		the format %p widely used by SofiaSIP. */
		char buf[4096];
		int len = vsnprintf(buf, sizeof(buf), format, arg_ptr);
		if(len <= 0)
			return;
		if(buf[len-1] == '\n') {
			/* remove trailing '\n' since apt logger appends it anyway */
			len--;
			buf[len] = '\0';
		}
		apt_log(SIP_LOG_MARK, APT_PRIO_DEBUG, "%.*s", len, buf);
	}
}

static su_log_t* mrcp_sofiasip_logger_get(const char *name)
{
	if (!strcasecmp(name,"tport"))
		return tport_log;
	else if (!strcasecmp(name,"nea"))
		return nea_log;
	else if (!strcasecmp(name,"nta"))
		return nta_log;
	else if (!strcasecmp(name,"nua"))
		return nua_log;
	else if (!strcasecmp(name,"soa"))
		return soa_log;
	else if (!strcasecmp(name,"default"))
		return su_log_default;
	return NULL;
}

MRCP_DECLARE(apt_bool_t) mrcp_sofiasip_log_init(const char *name, const char *level_str, apt_bool_t redirect)
{
	su_log_t *logger = mrcp_sofiasip_logger_get(name);
	if(!logger) {
		apt_log(SIP_LOG_MARK,APT_PRIO_WARNING,"Unknown SofiaSIP Logger <%s>",name);
		return FALSE;
	}

	apt_log(SIP_LOG_MARK,APT_PRIO_DEBUG,"Init SofiaSIP Logger [%s] level:%s redirect:%d",
			name, level_str, redirect);
	su_log_init(logger);

	if(redirect == TRUE) {
		su_log_redirect(logger, mrcp_sofiasip_log, NULL);
	}

	if(level_str) {
		int level = atoi(level_str);
		if(level >=0 && level < 10) {
			su_log_set_level(logger, level);
		}
		else {
			apt_log(SIP_LOG_MARK,APT_PRIO_WARNING,"Unknown SofiaSIP Log Level [%s]: must be in range [0..9]",level_str);
		}
	}
	return TRUE;
}

MRCP_DECLARE(void) mrcp_sofiasip_logsource_init()
{
	sip_log_source_init();
}
