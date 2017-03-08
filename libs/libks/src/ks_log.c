/*
 * Copyright (c) 2007-2014, Anthony Minessale II
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

#include <ks.h>

static void null_logger(const char *file, const char *func, int line, int level, const char *fmt, ...)
{
	if (file && func && line && level && fmt) {
		return;
	}
	return;
}


static const char *LEVEL_NAMES[] = {
	"EMERG",
	"ALERT",
	"CRIT",
	"ERROR",
	"WARNING",
	"NOTICE",
	"INFO",
	"DEBUG",
	NULL
};

static int ks_log_level = 7;
static ks_log_prefix_t ks_log_prefix = KS_LOG_PREFIX_ALL;

static const char *cut_path(const char *in)
{
	const char *p, *ret = in;
	char delims[] = "/\\";
	char *i;

	for (i = delims; *i; i++) {
		p = in;
		while ((p = strchr(p, *i)) != 0) {
			ret = ++p;
		}
	}
	return ret;
}


static void default_logger(const char *file, const char *func, int line, int level, const char *fmt, ...)
{
	const char *fp;
	char *data;
	va_list ap;
	int ret;
	char buf[1024];
	//int remaining = sizeof(buf) - 1;
	int used = 0;

	if (level < 0 || level > 7) {
		level = 7;
	}
	if (level > ks_log_level) {
		return;
	}

	fp = cut_path(file);

	va_start(ap, fmt);

	ret = ks_vasprintf(&data, fmt, ap);

	if (ret != -1) {
		buf[0] = '\0';
		used += 1;

		if (ks_log_prefix & KS_LOG_PREFIX_LEVEL) {
			used += snprintf(buf + used - 1, sizeof(buf) - used, "[%s] ", LEVEL_NAMES[level]);
		}
		if (ks_log_prefix & KS_LOG_PREFIX_TIME) {
			used += snprintf(buf + used - 1, sizeof(buf) - used, "@%lld ", (long long int)ks_time_now());
		}
		if (ks_log_prefix & KS_LOG_PREFIX_THREAD) {
			used += snprintf(buf + used - 1, sizeof(buf) - used, "#%d ", (int32_t)ks_thread_self_id());
		}
		if (ks_log_prefix & KS_LOG_PREFIX_FILE) {
			used += snprintf(buf + used - 1, sizeof(buf) - used, fp);
			if (ks_log_prefix & KS_LOG_PREFIX_LINE) {
				used += snprintf(buf + used - 1, sizeof(buf) - used, ":%d", line);
			}
			used += snprintf(buf + used - 1, sizeof(buf) - used, " ");
		}
		if (ks_log_prefix & KS_LOG_PREFIX_FUNC) {
			used += snprintf(buf + used - 1, sizeof(buf) - used, "%s() ", func);
		}

		used += snprintf(buf + used - 1, sizeof(buf) - used, data);

		//fprintf(stderr, "[%s] %s:%d %s() %s", LEVEL_NAMES[level], fp, line, func, data);
		fprintf(stderr, buf);
		free(data);
	}

	va_end(ap);

}

ks_logger_t ks_log = null_logger;

KS_DECLARE(void) ks_global_set_logger(ks_logger_t logger)
{
	if (logger) {
		ks_log = logger;
	} else {
		ks_log = null_logger;
	}
}

KS_DECLARE(void) ks_global_set_default_logger(int level)
{
	if (level < 0 || level > 7) {
		level = 7;
	}

	ks_log = default_logger;
	ks_log_level = level;
}

KS_DECLARE(void) ks_global_set_default_logger_prefix(ks_log_prefix_t prefix)
{
	ks_log_prefix = prefix;
}
