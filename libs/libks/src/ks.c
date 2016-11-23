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

static ks_pool_t *pool = NULL;



KS_DECLARE(void) ks_random_string(char *buf, uint16_t len, char *set)
{
	char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	int max;
	uint16_t x;

	if (!set) {
		set = chars;
	}

	max = (int) strlen(set);

	for (x = 0; x < len; x++) {
		int j = (int) (max * 1.0 * rand() / (RAND_MAX + 1.0));
		buf[x] = set[j];
	}
}


KS_DECLARE(ks_status_t) ks_global_set_cleanup(ks_pool_cleanup_fn_t fn, void *arg)
{
	return ks_pool_set_cleanup(ks_global_pool(), NULL, arg, 0, fn);
}
	
KS_DECLARE(ks_status_t) ks_init(void)
{

	srand(getpid() * (intptr_t)&pool + time(NULL));
	ks_ssl_init_ssl_locks();
	ks_global_pool();
	ks_rng_init();
	
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_shutdown(void)
{
	ks_status_t status = KS_STATUS_SUCCESS;

	ks_ssl_destroy_ssl_locks();
	//ks_rng_shutdown();

	if (pool) {
		status = ks_pool_close(&pool);
	}
	
	return status;
}

KS_DECLARE(ks_pool_t *) ks_global_pool(void)
{

	ks_status_t status;

	if (!pool) {
		if ((status = ks_pool_open(&pool)) != KS_STATUS_SUCCESS) {
			abort();
		}
	}

	return pool;
}

KS_ENUM_NAMES(STATUS_NAMES, STATUS_STRINGS)
KS_STR2ENUM(ks_str2ks_status, ks_status2str, ks_status_t, STATUS_NAMES, KS_STATUS_COUNT)

KS_DECLARE(size_t) ks_url_encode(const char *url, char *buf, size_t len)
{
	const char *p;
	size_t x = 0;
	const char urlunsafe[] = "\r\n \"#%&+:;<=>?@[\\]^`{|}";
	const char hex[] = "0123456789ABCDEF";

	if (!buf) {
		return 0;
	}

	if (!url) {
		return 0;
	}

	len--;

	for (p = url; *p; p++) {
		if (x >= len) {
			break;
		}
		if (*p < ' ' || *p > '~' || strchr(urlunsafe, *p)) {
			if ((x + 3) >= len) {
				break;
			}
			buf[x++] = '%';
			buf[x++] = hex[*p >> 4];
			buf[x++] = hex[*p & 0x0f];
		} else {
			buf[x++] = *p;
		}
	}
	buf[x] = '\0';

	return x;
}

KS_DECLARE(char *) ks_url_decode(char *s)
{
	char *o;
	unsigned int tmp;

	for (o = s; *s; s++, o++) {
		if (*s == '%' && strlen(s) > 2 && sscanf(s + 1, "%2x", &tmp) == 1) {
			*o = (char) tmp;
			s += 2;
		} else {
			*o = *s;
		}
	}
	*o = '\0';
	return s;
}

KS_DECLARE(int) ks_cpu_count(void)
{
	int cpu_count;

#ifndef WIN32
	cpu_count = sysconf (_SC_NPROCESSORS_ONLN);
#else
	{
		SYSTEM_INFO sysinfo;
		GetSystemInfo( &sysinfo );
		cpu_count = sysinfo.dwNumberOfProcessors;
	}
#endif
	
	return cpu_count;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
