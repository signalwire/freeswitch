/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2015, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/F
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Michael Jerris <mike@jerris.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Michael Jerris <mike@jerris.com>
 * Eliot Gable <egable@gmail.com>
 * William King <william.king@quentustech.com>
 *
 * switch_apr.c -- apr wrappers and extensions
 *
 */

#include <switch.h>
#ifndef WIN32
#include <switch_private.h>
#endif
#include "private/switch_core_pvt.h"
#include "private/switch_apr_pvt.h"

/* apr headers*/
#include <fspr.h>
#include <fspr_atomic.h>
#include <fspr_pools.h>
#include <fspr_hash.h>
#include <fspr_network_io.h>
#include <fspr_errno.h>
#include <fspr_thread_proc.h>
#include <fspr_portable.h>
#include <fspr_thread_mutex.h>

/* c-ares for async DNS resolution */
#ifdef HAVE_CARES
#include <ares.h>
#include <arpa/inet.h>
#endif
#include <fspr_thread_cond.h>
#include <fspr_thread_rwlock.h>
#include <fspr_file_io.h>
#include <fspr_poll.h>
#include <fspr_strings.h>
#define APR_WANT_STDIO
#define APR_WANT_STRFUNC
#include <fspr_want.h>
#include <fspr_file_info.h>
#include <fspr_fnmatch.h>
#include <fspr_tables.h>

#ifdef WIN32
#include "fspr_arch_networkio.h"
/* Missing socket symbols */
#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif
#endif

/* fspr_vformatter_buff_t definition*/
#include <fspr_lib.h>

#if (defined(HAVE_LIBMD5) || defined(HAVE_LIBMD) || defined(HAVE_MD5INIT))
#include <md5.h>
#elif defined(HAVE_LIBCRYPTO)
	#ifndef OPENSSL_VERSION_NUMBER
		#include <openssl/opensslv.h>
	#endif
	#if OPENSSL_VERSION_NUMBER < 0x30000000
		#include <openssl/md5.h>
	#else
		#include <openssl/evp.h>
	#endif
#else
	#include <apr_md5.h>
#endif

#ifndef WIN32
#include <uuid/uuid.h>
#endif

/* apr stubs */

SWITCH_DECLARE(int) switch_status_is_timeup(int status)
{
	return APR_STATUS_IS_TIMEUP(status);
}

/* Memory Pools */

SWITCH_DECLARE(switch_thread_id_t) switch_thread_self(void)
{
#ifndef WIN32
	return fspr_os_thread_current();
#else
	return (switch_thread_id_t) (GetCurrentThreadId());
#endif
}

SWITCH_DECLARE(int) switch_thread_equal(switch_thread_id_t tid1, switch_thread_id_t tid2)
{
#ifdef WIN32
	return (tid1 == tid2);
#else
	return fspr_os_thread_equal(tid1, tid2);
#endif

}

SWITCH_DECLARE(unsigned int) switch_ci_hashfunc_default(const char *char_key, switch_ssize_t *klen)
{
	unsigned int hash = 0;
	const unsigned char *key = (const unsigned char *) char_key;
	const unsigned char *p;
	fspr_ssize_t i;

	if (*klen == APR_HASH_KEY_STRING) {
		for (p = key; *p; p++) {
			hash = hash * 33 + tolower(*p);
		}
		*klen = p - key;
	} else {
		for (p = key, i = *klen; i; i--, p++) {
			hash = hash * 33 + tolower(*p);
		}
	}

	return hash;
}


SWITCH_DECLARE(unsigned int) switch_hashfunc_default(const char *key, switch_ssize_t *klen)
{
	return fspr_hashfunc_default(key, klen);
}

/* string functions */

SWITCH_DECLARE(switch_status_t) switch_strftime(char *s, switch_size_t *retsize, switch_size_t max, const char *format, switch_time_exp_t *tm)
{
	const char *p = format;

	if (!p)
		return SWITCH_STATUS_FALSE;

	while (*p) {
		if (*p == '%') {
			switch (*(++p)) {
			case 'C':
			case 'D':
			case 'r':
			case 'R':
			case 'T':
			case 'e':
			case 'a':
			case 'A':
			case 'b':
			case 'B':
			case 'c':
			case 'd':
			case 'H':
			case 'I':
			case 'j':
			case 'm':
			case 'M':
			case 'p':
			case 'S':
			case 'U':
			case 'w':
			case 'W':
			case 'x':
			case 'X':
			case 'y':
			case 'Y':
			case 'z':
			case 'Z':
			case '%':
				p++;
				continue;
			case '\0':
			default:
				return SWITCH_STATUS_FALSE;
			}
		}
		p++;
	}

	return fspr_strftime(s, retsize, max, format, (fspr_time_exp_t *) tm);
}

SWITCH_DECLARE(switch_status_t) switch_strftime_nocheck(char *s, switch_size_t *retsize, switch_size_t max, const char *format, switch_time_exp_t *tm)
{
	return fspr_strftime(s, retsize, max, format, (fspr_time_exp_t *) tm);
}

SWITCH_DECLARE(int) switch_snprintf(char *buf, switch_size_t len, const char *format, ...)
{
	va_list ap;
	int ret;
	va_start(ap, format);
	ret = fspr_vsnprintf(buf, len, format, ap);
	va_end(ap);
	return ret;
}

SWITCH_DECLARE(int) switch_vsnprintf(char *buf, switch_size_t len, const char *format, va_list ap)
{
	return fspr_vsnprintf(buf, len, format, ap);
}

SWITCH_DECLARE(char *) switch_copy_string(char *dst, const char *src, switch_size_t dst_size)
{
	if (!dst)
		return NULL;
	if (!src) {
		*dst = '\0';
		return dst;
	}
	return fspr_cpystrn(dst, src, dst_size);
}

/* thread read write lock functions */

SWITCH_DECLARE(switch_status_t) switch_thread_rwlock_create(switch_thread_rwlock_t ** rwlock, switch_memory_pool_t *pool)
{
	return fspr_thread_rwlock_create(rwlock, pool);
}

SWITCH_DECLARE(switch_status_t) switch_thread_rwlock_destroy(switch_thread_rwlock_t *rwlock)
{
	return fspr_thread_rwlock_destroy(rwlock);
}

SWITCH_DECLARE(switch_memory_pool_t *) switch_thread_rwlock_pool_get(switch_thread_rwlock_t *rwlock)
{
	return fspr_thread_rwlock_pool_get(rwlock);
}

SWITCH_DECLARE(switch_status_t) switch_thread_rwlock_rdlock(switch_thread_rwlock_t *rwlock)
{
	return fspr_thread_rwlock_rdlock(rwlock);
}

SWITCH_DECLARE(switch_status_t) switch_thread_rwlock_tryrdlock(switch_thread_rwlock_t *rwlock)
{
	return fspr_thread_rwlock_tryrdlock(rwlock);
}

SWITCH_DECLARE(switch_status_t) switch_thread_rwlock_wrlock(switch_thread_rwlock_t *rwlock)
{
	return fspr_thread_rwlock_wrlock(rwlock);
}

SWITCH_DECLARE(switch_status_t) switch_thread_rwlock_trywrlock(switch_thread_rwlock_t *rwlock)
{
	return fspr_thread_rwlock_trywrlock(rwlock);
}

SWITCH_DECLARE(switch_status_t) switch_thread_rwlock_trywrlock_timeout(switch_thread_rwlock_t *rwlock, int timeout)
{
	int sanity = timeout * 2;

	while (sanity) {
		if (switch_thread_rwlock_trywrlock(rwlock) == SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_SUCCESS;
		}
		sanity--;
		switch_yield(500000);
	}

	return SWITCH_STATUS_FALSE;
}


SWITCH_DECLARE(switch_status_t) switch_thread_rwlock_unlock(switch_thread_rwlock_t *rwlock)
{
	return fspr_thread_rwlock_unlock(rwlock);
}

/* thread mutex functions */

SWITCH_DECLARE(switch_status_t) switch_mutex_init(switch_mutex_t ** lock, unsigned int flags, switch_memory_pool_t *pool)
{
#ifdef WIN32
	/* Old version of APR misunderstands mutexes. On Windows, mutexes are cross-process.
	   APR has no reason to not use critical sections instead of mutexes. */
	if (flags == SWITCH_MUTEX_NESTED) flags = SWITCH_MUTEX_DEFAULT;
#endif
	return fspr_thread_mutex_create(lock, flags, pool);
}

SWITCH_DECLARE(switch_status_t) switch_mutex_destroy(switch_mutex_t *lock)
{
	return fspr_thread_mutex_destroy(lock);
}

SWITCH_DECLARE(switch_status_t) switch_mutex_lock(switch_mutex_t *lock)
{
	return fspr_thread_mutex_lock(lock);
}

SWITCH_DECLARE(switch_status_t) switch_mutex_unlock(switch_mutex_t *lock)
{
	return fspr_thread_mutex_unlock(lock);
}

SWITCH_DECLARE(switch_status_t) switch_mutex_trylock(switch_mutex_t *lock)
{
	return fspr_thread_mutex_trylock(lock);
}

/* time function stubs */

SWITCH_DECLARE(switch_time_t) switch_time_now(void)
{
#if defined(HAVE_CLOCK_GETTIME) && defined(SWITCH_USE_CLOCK_FUNCS)
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return ts.tv_sec * APR_USEC_PER_SEC + (ts.tv_nsec / 1000);
#else
	return (switch_time_t) fspr_time_now();
#endif
}

SWITCH_DECLARE(switch_status_t) switch_time_exp_gmt_get(switch_time_t *result, switch_time_exp_t *input)
{
	return fspr_time_exp_gmt_get((fspr_time_t *) result, (fspr_time_exp_t *) input);
}

SWITCH_DECLARE(switch_status_t) switch_time_exp_get(switch_time_t *result, switch_time_exp_t *input)
{
	return fspr_time_exp_get((fspr_time_t *) result, (fspr_time_exp_t *) input);
}

SWITCH_DECLARE(switch_status_t) switch_time_exp_lt(switch_time_exp_t *result, switch_time_t input)
{
	return fspr_time_exp_lt((fspr_time_exp_t *) result, input);
}

SWITCH_DECLARE(switch_status_t) switch_time_exp_tz(switch_time_exp_t *result, switch_time_t input, switch_int32_t offs)
{
	return fspr_time_exp_tz((fspr_time_exp_t *) result, input, (fspr_int32_t) offs);
}

SWITCH_DECLARE(switch_status_t) switch_time_exp_gmt(switch_time_exp_t *result, switch_time_t input)
{
	return fspr_time_exp_gmt((fspr_time_exp_t *) result, input);
}

SWITCH_DECLARE(switch_status_t) switch_rfc822_date(char *date_str, switch_time_t t)
{
	return fspr_rfc822_date(date_str, t);
}

SWITCH_DECLARE(switch_time_t) switch_time_make(switch_time_t sec, int32_t usec)
{
	return ((switch_time_t) (sec) * APR_USEC_PER_SEC + (switch_time_t) (usec));
}

/* Thread condition locks */

SWITCH_DECLARE(switch_status_t) switch_thread_cond_create(switch_thread_cond_t ** cond, switch_memory_pool_t *pool)
{
	return fspr_thread_cond_create(cond, pool);
}

SWITCH_DECLARE(switch_status_t) switch_thread_cond_wait(switch_thread_cond_t *cond, switch_mutex_t *mutex)
{
	return fspr_thread_cond_wait(cond, mutex);
}

SWITCH_DECLARE(switch_status_t) switch_thread_cond_timedwait(switch_thread_cond_t *cond, switch_mutex_t *mutex, switch_interval_time_t timeout)
{
	fspr_status_t st = fspr_thread_cond_timedwait(cond, mutex, timeout);

	if (st == APR_TIMEUP) {
		st = SWITCH_STATUS_TIMEOUT;
	}

	return st;
}

SWITCH_DECLARE(switch_status_t) switch_thread_cond_signal(switch_thread_cond_t *cond)
{
	return fspr_thread_cond_signal(cond);
}

SWITCH_DECLARE(switch_status_t) switch_thread_cond_broadcast(switch_thread_cond_t *cond)
{
	return fspr_thread_cond_broadcast(cond);
}

SWITCH_DECLARE(switch_status_t) switch_thread_cond_destroy(switch_thread_cond_t *cond)
{
	return fspr_thread_cond_destroy(cond);
}

/* file i/o stubs */

SWITCH_DECLARE(switch_status_t) switch_file_open(switch_file_t ** newf, const char *fname, int32_t flag, switch_fileperms_t perm,
												 switch_memory_pool_t *pool)
{
	return fspr_file_open(newf, fname, flag, perm, pool);
}

SWITCH_DECLARE(switch_status_t) switch_file_seek(switch_file_t *thefile, switch_seek_where_t where, int64_t *offset)
{
	fspr_status_t rv;
	fspr_off_t off = (fspr_off_t) (*offset);
	rv = fspr_file_seek(thefile, where, &off);
	*offset = (int64_t) off;
	return rv;
}

SWITCH_DECLARE(switch_status_t) switch_file_copy(const char *from_path, const char *to_path, switch_fileperms_t perms, switch_memory_pool_t *pool)
{
	return fspr_file_copy(from_path, to_path, perms, pool);
}


SWITCH_DECLARE(switch_status_t) switch_file_close(switch_file_t *thefile)
{
	return fspr_file_close(thefile);
}

SWITCH_DECLARE(switch_status_t) switch_file_trunc(switch_file_t *thefile, int64_t offset)
{
	return fspr_file_trunc(thefile, offset);
}

SWITCH_DECLARE(switch_status_t) switch_file_lock(switch_file_t *thefile, int type)
{
	return fspr_file_lock(thefile, type);
}

SWITCH_DECLARE(switch_status_t) switch_file_rename(const char *from_path, const char *to_path, switch_memory_pool_t *pool)
{
	return fspr_file_rename(from_path, to_path, pool);
}

SWITCH_DECLARE(switch_status_t) switch_file_remove(const char *path, switch_memory_pool_t *pool)
{
	return fspr_file_remove(path, pool);
}

SWITCH_DECLARE(switch_status_t) switch_file_read(switch_file_t *thefile, void *buf, switch_size_t *nbytes)
{
	return fspr_file_read(thefile, buf, nbytes);
}

SWITCH_DECLARE(switch_status_t) switch_file_write(switch_file_t *thefile, const void *buf, switch_size_t *nbytes)
{
	return fspr_file_write(thefile, buf, nbytes);
}

SWITCH_DECLARE(int) switch_file_printf(switch_file_t *thefile, const char *format, ...)
{
	va_list ap;
	int ret;
	char *data;

	va_start(ap, format);

	if ((ret = switch_vasprintf(&data, format, ap)) != -1) {
		switch_size_t bytes = strlen(data);
		switch_file_write(thefile, data, &bytes);
		free(data);
	}

	va_end(ap);

	return ret;
}

SWITCH_DECLARE(switch_status_t) switch_file_mktemp(switch_file_t ** thefile, char *templ, int32_t flags, switch_memory_pool_t *pool)
{
	return fspr_file_mktemp(thefile, templ, flags, pool);
}

SWITCH_DECLARE(switch_size_t) switch_file_get_size(switch_file_t *thefile)
{
	struct fspr_finfo_t finfo;
	return fspr_file_info_get(&finfo, APR_FINFO_SIZE, thefile) == SWITCH_STATUS_SUCCESS ? (switch_size_t) finfo.size : 0;
}

SWITCH_DECLARE(switch_status_t) switch_directory_exists(const char *dirname, switch_memory_pool_t *pool)
{
	fspr_dir_t *dir_handle;
	switch_memory_pool_t *our_pool = NULL;
	switch_status_t status;

	if (!pool) {
		switch_core_new_memory_pool(&our_pool);
		pool = our_pool;
	}

	if ((status = fspr_dir_open(&dir_handle, dirname, pool)) == APR_SUCCESS) {
		fspr_dir_close(dir_handle);
	}

	if (our_pool) {
		switch_core_destroy_memory_pool(&our_pool);
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_file_exists(const char *filename, switch_memory_pool_t *pool)
{
	int32_t wanted = APR_FINFO_TYPE;
	switch_memory_pool_t *our_pool = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	fspr_finfo_t info = { 0 };

	if (zstr(filename)) {
		return status;
	}

	if (!pool) {
		switch_core_new_memory_pool(&our_pool);
	}

	fspr_stat(&info, filename, wanted, pool ? pool : our_pool);
	if (info.filetype != APR_NOFILE) {
		status = SWITCH_STATUS_SUCCESS;
	}

	if (our_pool) {
		switch_core_destroy_memory_pool(&our_pool);
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_dir_make(const char *path, switch_fileperms_t perm, switch_memory_pool_t *pool)
{
	return fspr_dir_make(path, perm, pool);
}

SWITCH_DECLARE(switch_status_t) switch_dir_make_recursive(const char *path, switch_fileperms_t perm, switch_memory_pool_t *pool)
{
	return fspr_dir_make_recursive(path, perm, pool);
}

struct switch_dir {
	fspr_dir_t *dir_handle;
	fspr_finfo_t finfo;
};

SWITCH_DECLARE(switch_status_t) switch_dir_open(switch_dir_t ** new_dir, const char *dirname, switch_memory_pool_t *pool)
{
	switch_status_t status;
	switch_dir_t *dir = malloc(sizeof(*dir));

	if (!dir) {
		*new_dir = NULL;
		return SWITCH_STATUS_FALSE;
	}

	memset(dir, 0, sizeof(*dir));
	if ((status = fspr_dir_open(&(dir->dir_handle), dirname, pool)) == APR_SUCCESS) {
		*new_dir = dir;
	} else {
		free(dir);
		*new_dir = NULL;
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_dir_close(switch_dir_t *thedir)
{
	switch_status_t status = fspr_dir_close(thedir->dir_handle);

	free(thedir);
	return status;
}

SWITCH_DECLARE(uint32_t) switch_dir_count(switch_dir_t *thedir)
{
	const char *name;
	fspr_int32_t finfo_flags = APR_FINFO_DIRENT | APR_FINFO_TYPE | APR_FINFO_NAME;
	uint32_t count = 0;

	fspr_dir_rewind(thedir->dir_handle);

	while (fspr_dir_read(&(thedir->finfo), finfo_flags, thedir->dir_handle) == SWITCH_STATUS_SUCCESS) {

		if (thedir->finfo.filetype != APR_REG && thedir->finfo.filetype != APR_LNK) {
			continue;
		}

		if (!(name = thedir->finfo.fname)) {
			name = thedir->finfo.name;
		}

		if (name) {
			count++;
		}
	}

	fspr_dir_rewind(thedir->dir_handle);

	return count;
}

SWITCH_DECLARE(const char *) switch_dir_next_file(switch_dir_t *thedir, char *buf, switch_size_t len)
{
	const char *fname = NULL;
	fspr_int32_t finfo_flags = APR_FINFO_DIRENT | APR_FINFO_TYPE | APR_FINFO_NAME;
	const char *name;

	while (fspr_dir_read(&(thedir->finfo), finfo_flags, thedir->dir_handle) == SWITCH_STATUS_SUCCESS) {

		if (thedir->finfo.filetype != APR_REG && thedir->finfo.filetype != APR_LNK) {
			continue;
		}

		if (!(name = thedir->finfo.fname)) {
			name = thedir->finfo.name;
		}

		if (name) {
			switch_copy_string(buf, name, len);
			fname = buf;
			break;
		} else {
			continue;
		}
	}
	return fname;
}

/* thread stubs */

#ifndef WIN32
struct fspr_threadattr_t {
	fspr_pool_t *pool;
	pthread_attr_t attr;
	int priority;
};
#else
/* this needs to be revisited when apr for windows supports thread priority settings */
/* search for WIN32 in this file */
struct fspr_threadattr_t {
	fspr_pool_t *pool;
	fspr_int32_t detach;
	fspr_size_t stacksize;
	int priority;
};
#endif


SWITCH_DECLARE(switch_status_t) switch_threadattr_create(switch_threadattr_t ** new_attr, switch_memory_pool_t *pool)
{
	switch_status_t status;

	if ((status = fspr_threadattr_create(new_attr, pool)) == SWITCH_STATUS_SUCCESS) {

		(*new_attr)->priority = SWITCH_PRI_LOW;

	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_threadattr_detach_set(switch_threadattr_t *attr, int32_t on)
{
	return fspr_threadattr_detach_set(attr, on);
}

SWITCH_DECLARE(switch_status_t) switch_threadattr_stacksize_set(switch_threadattr_t *attr, switch_size_t stacksize)
{
	return fspr_threadattr_stacksize_set(attr, stacksize);
}

SWITCH_DECLARE(switch_status_t) switch_threadattr_priority_set(switch_threadattr_t *attr, switch_thread_priority_t priority)
{

	attr->priority = priority;

	return SWITCH_STATUS_SUCCESS;
}

static char TT_KEY[] = "1";

SWITCH_DECLARE(switch_status_t) switch_thread_create(switch_thread_t ** new_thread, switch_threadattr_t *attr,
													 switch_thread_start_t func, void *data, switch_memory_pool_t *cont)
{
	switch_core_memory_pool_set_data(cont, "_in_thread", TT_KEY);
	return fspr_thread_create(new_thread, attr, func, data, cont);
}

SWITCH_DECLARE(switch_interval_time_t) switch_interval_time_from_timeval(struct timeval *tvp)
{
	return ((switch_interval_time_t)tvp->tv_sec * 1000000) + tvp->tv_usec / 1000;
}

/* socket stubs */

SWITCH_DECLARE(switch_status_t) switch_os_sock_get(switch_os_socket_t *thesock, switch_socket_t *sock)
{
	return fspr_os_sock_get(thesock, sock);
}

SWITCH_DECLARE(switch_status_t) switch_os_sock_put(switch_socket_t **sock, switch_os_socket_t *thesock, switch_memory_pool_t *pool)
{
	return fspr_os_sock_put(sock, thesock, pool);
}

SWITCH_DECLARE(switch_status_t) switch_socket_addr_get(switch_sockaddr_t ** sa, switch_bool_t remote, switch_socket_t *sock)
{
	return fspr_socket_addr_get(sa, (fspr_interface_e) remote, sock);
}

SWITCH_DECLARE(switch_status_t) switch_socket_create(switch_socket_t ** new_sock, int family, int type, int protocol, switch_memory_pool_t *pool)
{
	return fspr_socket_create(new_sock, family, type, protocol, pool);
}

SWITCH_DECLARE(switch_status_t) switch_socket_shutdown(switch_socket_t *sock, switch_shutdown_how_e how)
{
	return fspr_socket_shutdown(sock, (fspr_shutdown_how_e) how);
}

SWITCH_DECLARE(switch_status_t) switch_socket_close(switch_socket_t *sock)
{
	return fspr_socket_close(sock);
}

SWITCH_DECLARE(switch_status_t) switch_socket_bind(switch_socket_t *sock, switch_sockaddr_t *sa)
{
	return fspr_socket_bind(sock, sa);
}

SWITCH_DECLARE(switch_status_t) switch_socket_listen(switch_socket_t *sock, int32_t backlog)
{
	return fspr_socket_listen(sock, backlog);
}

SWITCH_DECLARE(switch_status_t) switch_socket_accept(switch_socket_t ** new_sock, switch_socket_t *sock, switch_memory_pool_t *pool)
{
	return fspr_socket_accept(new_sock, sock, pool);
}

SWITCH_DECLARE(switch_status_t) switch_socket_connect(switch_socket_t *sock, switch_sockaddr_t *sa)
{
	return fspr_socket_connect(sock, sa);
}

SWITCH_DECLARE(switch_status_t) switch_socket_send(switch_socket_t *sock, const char *buf, switch_size_t *len)
{
	int status = SWITCH_STATUS_SUCCESS;
	switch_size_t req = *len, wrote = 0, need = *len;
	int to_count = 0;

	while ((wrote < req && status == SWITCH_STATUS_SUCCESS) || (need == 0 && status == SWITCH_STATUS_BREAK) || status == 730035 || status == 35) {
		need = req - wrote;
		status = fspr_socket_send(sock, buf + wrote, &need);
		if (status == SWITCH_STATUS_BREAK || status == 730035 || status == 35) {
			if (++to_count > 60000) {
				status = SWITCH_STATUS_FALSE;
				break;
			}
			switch_yield(10000);
		} else {
			to_count = 0;
		}
		wrote += need;
	}

	*len = wrote;
	return (switch_status_t)status;
}

SWITCH_DECLARE(switch_status_t) switch_socket_send_nonblock(switch_socket_t *sock, const char *buf, switch_size_t *len)
{
	if (!sock || !buf || !len) {
		return SWITCH_STATUS_GENERR;
	}

	return fspr_socket_send(sock, buf, len);
}

SWITCH_DECLARE(switch_status_t) switch_socket_sendto(switch_socket_t *sock, switch_sockaddr_t *where, int32_t flags, const char *buf,
													 switch_size_t *len)
{
	if (!where || !buf || !len || !*len) {
		return SWITCH_STATUS_GENERR;
	}
	return fspr_socket_sendto(sock, where, flags, buf, len);
}

SWITCH_DECLARE(switch_status_t) switch_socket_recv(switch_socket_t *sock, char *buf, switch_size_t *len)
{
	int r;

	r = fspr_socket_recv(sock, buf, len);

	if (r == 35 || r == 730035) {
		r = SWITCH_STATUS_BREAK;
	}

	return (switch_status_t)r;
}

SWITCH_DECLARE(switch_status_t) switch_sockaddr_create(switch_sockaddr_t **sa, switch_memory_pool_t *pool)
{
	switch_sockaddr_t *new_sa;
	unsigned short family = APR_INET;

	new_sa = fspr_pcalloc(pool, sizeof(fspr_sockaddr_t));
	switch_assert(new_sa);
	new_sa->pool = pool;

	new_sa->family = family;
	new_sa->sa.sin.sin_family = family;

	new_sa->salen = sizeof(struct sockaddr_in);
	new_sa->addr_str_len = 16;
	new_sa->ipaddr_ptr = &(new_sa->sa.sin.sin_addr);
	new_sa->ipaddr_len = sizeof(struct in_addr);

	*sa = new_sa;
	return SWITCH_STATUS_SUCCESS;
}

#ifdef HAVE_CARES

/**
 * Internal state structure for async DNS resolution
 * Holds c-ares channel and results during resolution process
 */
struct dns_resolve_state {
	ares_channel dns_channel;
	struct ares_addrinfo *resolved_info;
	int resolution_status;
	switch_memory_pool_t *mem_pool;
};

/**
 * Callback invoked by c-ares when DNS resolution completes
 * Stores the result and status in the state structure
 */
static void dns_completion_callback(void *user_data, int status, int timeouts, struct ares_addrinfo *result)
{
	struct dns_resolve_state *resolve_state = (struct dns_resolve_state *)user_data;

	(void)timeouts; /* Unused parameter */

	if (status == ARES_SUCCESS && result != NULL) {
		resolve_state->resolved_info = result;
	}
	resolve_state->resolution_status = status;
}

/**
 * Convert c-ares addrinfo results to FreeSWITCH's sockaddr format
 * Creates a linked list of fspr_sockaddr_t structures from c-ares results
 */
static switch_status_t convert_cares_to_sockaddr(const struct ares_addrinfo *ai_head, fspr_sockaddr_t **sa,
												  fspr_port_t port, switch_memory_pool_t *pool)
{
	const struct ares_addrinfo_node *ai_node;
	fspr_sockaddr_t *head_addr = NULL;
	fspr_sockaddr_t *tail_addr = NULL;
	const char *canonical_name = ai_head->name;

	if (!ai_head || !ai_head->nodes || !sa || !pool) {
		return SWITCH_STATUS_GENERR;
	}

	/* Iterate through c-ares result nodes */
	for (ai_node = ai_head->nodes; ai_node != NULL; ai_node = ai_node->ai_next) {
		fspr_sockaddr_t *new_sockaddr;
		fspr_socklen_t sockaddr_size;
		int addr_str_len;
		int ipaddr_len;

		/* Only handle IPv4 and IPv6 */
		if (ai_node->ai_family == AF_INET) {
			sockaddr_size = sizeof(struct sockaddr_in);
			addr_str_len = 16;  /* xxx.xxx.xxx.xxx\0 */
			ipaddr_len = sizeof(struct in_addr);
		} else if (ai_node->ai_family == AF_INET6) {
			sockaddr_size = sizeof(struct sockaddr_in6);
			addr_str_len = 46;  /* INET6_ADDRSTRLEN */
			ipaddr_len = sizeof(struct in6_addr);
		} else {
			/* Skip unsupported address families */
			continue;
		}

		/* Validate c-ares provided valid address data */
		if (!ai_node->ai_addr || ai_node->ai_addrlen < (int)sockaddr_size) {
			continue;
		}

		/* Allocate new sockaddr structure from pool */
		new_sockaddr = fspr_pcalloc(pool, sizeof(fspr_sockaddr_t));
		if (!new_sockaddr) {
			return SWITCH_STATUS_MEMERR;
		}

		/* Populate basic fields */
		new_sockaddr->pool = pool;
		new_sockaddr->family = ai_node->ai_family;
		new_sockaddr->salen = sockaddr_size;
		new_sockaddr->ipaddr_len = ipaddr_len;
		new_sockaddr->addr_str_len = addr_str_len;
		new_sockaddr->next = NULL;

		/* Copy sockaddr data and set up pointer to IP address within sockaddr union */
		if (ai_node->ai_family == AF_INET) {
			memcpy(&new_sockaddr->sa.sin, ai_node->ai_addr, sockaddr_size);
			new_sockaddr->sa.sin.sin_port = htons(port);
			new_sockaddr->ipaddr_ptr = &(new_sockaddr->sa.sin.sin_addr);
			new_sockaddr->port = port;
		} else {  /* AF_INET6 */
			memcpy(&new_sockaddr->sa.sin6, ai_node->ai_addr, sockaddr_size);
			new_sockaddr->sa.sin6.sin6_port = htons(port);
			new_sockaddr->ipaddr_ptr = &(new_sockaddr->sa.sin6.sin6_addr);
			new_sockaddr->port = port;
		}

		/* Store canonical name in first entry only */
		if (canonical_name != NULL) {
			new_sockaddr->hostname = fspr_pstrdup(pool, canonical_name);
			canonical_name = NULL;  /* Only set in first entry */
		}

		/* Build linked list */
		if (head_addr == NULL) {
			head_addr = new_sockaddr;
		}
		if (tail_addr != NULL) {
			tail_addr->next = new_sockaddr;
		}
		tail_addr = new_sockaddr;
	}

	if (head_addr == NULL) {
		return SWITCH_STATUS_NOTFOUND;
	}

	*sa = head_addr;
	return SWITCH_STATUS_SUCCESS;
}

/**
 * Resolve hostname using c-ares asynchronous DNS resolver
 * Blocks until resolution completes, then converts results to FreeSWITCH format
 */
static switch_status_t resolve_hostname_cares(fspr_sockaddr_t **sa, const char *hostname, fspr_int32_t family,
											   fspr_port_t port, fspr_int32_t flags, switch_memory_pool_t *pool)
{
	struct dns_resolve_state resolve_state;
	struct ares_options ares_opts;
	struct ares_addrinfo_hints hints;
	int ares_optmask = 0;
	int init_status;
	switch_status_t result_status;

	memset(&resolve_state, 0, sizeof(resolve_state));
	memset(&ares_opts, 0, sizeof(ares_opts));
	memset(&hints, 0, sizeof(hints));

	resolve_state.mem_pool = pool;
	resolve_state.resolution_status = ARES_ETIMEOUT;

	/* Configure c-ares to use internal event thread for async processing */
	ares_optmask |= ARES_OPT_EVENT_THREAD | ARES_OPT_TIMEOUT | ARES_OPT_TRIES;
	ares_opts.evsys = ARES_EVSYS_DEFAULT;
	ares_opts.timeout = runtime.ares_dns_timeout;
	ares_opts.tries = 2;

	/* Initialize c-ares channel */
	init_status = ares_init_options(&resolve_state.dns_channel, &ares_opts, ares_optmask);
	if (init_status != ARES_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
						 "c-ares initialization failed: %s\n", ares_strerror(init_status));
		return SWITCH_STATUS_GENERR;
	}

	/* Set ai_family based on flags, falling back to family parameter */
	if ((flags & APR_IPV4_ADDR_OK) && !(flags & APR_IPV6_ADDR_OK)) {
		hints.ai_family = AF_INET;
	} else if ((flags & APR_IPV6_ADDR_OK) && !(flags & APR_IPV4_ADDR_OK)) {
		hints.ai_family = AF_INET6;
	} else {
		hints.ai_family = family;
	}
	hints.ai_socktype = SOCK_DGRAM;  /* VoIP typically uses UDP */
	hints.ai_protocol = IPPROTO_UDP;

	/* Start asynchronous DNS query */
	ares_getaddrinfo(resolve_state.dns_channel, hostname, NULL, &hints,
					dns_completion_callback, &resolve_state);

	/* Block until the async DNS resolution completes or ares_dns_timeout expires (c-ares processes it internally) */
	ares_queue_wait_empty(resolve_state.dns_channel, runtime.ares_dns_timeout);

	/* Map c-ares status to FreeSWITCH status codes */
	switch (resolve_state.resolution_status) {
		case ARES_SUCCESS:
			/* Convert results to FreeSWITCH sockaddr format */
			result_status = convert_cares_to_sockaddr(resolve_state.resolved_info, sa, port, pool);

			/* Free c-ares allocated result */
			if (resolve_state.resolved_info) {
				ares_freeaddrinfo(resolve_state.resolved_info);
			}
			break;

		case ARES_ENOTFOUND:
		case ARES_ENODATA:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
							 "DNS resolution failed for %s: not found\n", hostname);
			result_status = SWITCH_STATUS_NOTFOUND;
			break;

		case ARES_ENOMEM:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
							 "DNS resolution failed for %s: out of memory\n", hostname);
			result_status = SWITCH_STATUS_MEMERR;
			break;

		case ARES_ETIMEOUT:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
							 "DNS resolution failed for %s: timeout\n", hostname);
			result_status = SWITCH_STATUS_TIMEOUT;
			break;

		default:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
							 "DNS resolution failed for %s: %s\n", hostname,
							 ares_strerror(resolve_state.resolution_status));
			result_status = SWITCH_STATUS_GENERR;
			break;
	}

	/* Cleanup c-ares channel */
	ares_destroy(resolve_state.dns_channel);

	return result_status;
}

/**
 * Helper function to detect if a string is a numeric IP address
 * Returns 1 for numeric IPs, 0 for hostnames that need DNS resolution
 */
static int is_numeric_address(const char *hostname, fspr_int32_t family)
{
	unsigned char buf[sizeof(struct in6_addr)];

	if (!hostname) {
		return 0;
	}

	/* Try IPv4 first if family allows */
	if (family == APR_UNSPEC || family == APR_INET) {
		if (inet_pton(AF_INET, hostname, buf) == 1) {
			return 1;
		}
	}

	/* Try IPv6 if family allows */
	if (family == APR_UNSPEC || family == APR_INET6) {
		/* Handle bracketed IPv6 addresses like [::1] */
		if (hostname[0] == '[') {
			char stripped[APRMAXHOSTLEN];
			char *bracket;
			size_t len = strlen(hostname + 1);
			if (len < sizeof(stripped)) {
				strcpy(stripped, hostname + 1);
				bracket = strchr(stripped, ']');
				if (bracket) {
					*bracket = '\0';
					if (inet_pton(AF_INET6, stripped, buf) == 1) {
						return 1;
					}
				}
			}
		} else {
			if (inet_pton(AF_INET6, hostname, buf) == 1) {
				return 1;
			}
		}
	}

	return 0;
}

#endif /* HAVE_CARES */

SWITCH_DECLARE(switch_status_t) switch_sockaddr_info_get(switch_sockaddr_t ** sa, const char *hostname, int32_t family,
														 switch_port_t port, int32_t flags, switch_memory_pool_t *pool)
{
#ifdef HAVE_CARES
	/* Fast path: NULL hostname means wildcard bind (0.0.0.0/::) - use APR */
	if (hostname == NULL) {
		return fspr_sockaddr_info_get(sa, hostname, family, port, flags, pool);
	}

	/* Fast path: numeric IP addresses don't need DNS resolution - use APR */
	if (is_numeric_address(hostname, family)) {
		return fspr_sockaddr_info_get(sa, hostname, family, port, flags, pool);
	}

	/* Use c-ares for actual DNS hostname resolution */
	return resolve_hostname_cares(sa, hostname, family, port, flags, pool);
#else
	/* Fall back to APR when c-ares is not available */
	return fspr_sockaddr_info_get(sa, hostname, family, port, flags, pool);
#endif
}

SWITCH_DECLARE(switch_status_t) switch_sockaddr_new(switch_sockaddr_t ** sa, const char *ip, switch_port_t port, switch_memory_pool_t *pool)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	fspr_sockaddr_t *new_sa;
	int family;

	if (!sa || !pool || !ip) {
		switch_goto_status(SWITCH_STATUS_GENERR, end);
	}

	new_sa = fspr_pcalloc(pool, sizeof(fspr_sockaddr_t));
	switch_assert(new_sa);

	new_sa->pool = pool;

#if APR_HAVE_IPV6
	if (strchr(ip, ':')) {
		struct sockaddr_in6 sa6 = { 0 };

		family = APR_INET6;
		inet_pton(family, ip, &(sa6.sin6_addr));
		memcpy(&new_sa->sa, &sa6, sizeof(struct sockaddr_in6));
	} else
#endif
	{
		struct sockaddr_in sa4 = { 0 };

		family = APR_INET;
		inet_pton(family, ip, &(sa4.sin_addr));
		memcpy(&new_sa->sa, &sa4, sizeof(struct sockaddr_in));
	}

	new_sa->hostname = fspr_pstrdup(pool, ip);
	new_sa->family = family;
	new_sa->sa.sin.sin_family = family;
	if (port) {
		/* XXX IPv6: assumes sin_port and sin6_port at same offset */
		new_sa->sa.sin.sin_port = htons(port);
		new_sa->port = port;
	}

	if (family == APR_INET) {
		new_sa->salen = sizeof(struct sockaddr_in);
		new_sa->addr_str_len = 16;
		new_sa->ipaddr_ptr = &(new_sa->sa.sin.sin_addr);
		new_sa->ipaddr_len = sizeof(struct in_addr);
	}
#if APR_HAVE_IPV6
	else if (family == APR_INET6) {
		new_sa->salen = sizeof(struct sockaddr_in6);
		new_sa->addr_str_len = 46;
		new_sa->ipaddr_ptr = &(new_sa->sa.sin6.sin6_addr);
		new_sa->ipaddr_len = sizeof(struct in6_addr);
	}
#endif

	*sa = new_sa;

end:
	return status;
}

SWITCH_DECLARE(switch_status_t) switch_socket_opt_set(switch_socket_t *sock, int32_t opt, int32_t on)
{
	if (opt == SWITCH_SO_TCP_KEEPIDLE) {
#if defined(TCP_KEEPIDLE)
		int r = -10;
		r = setsockopt(sock->socketdes, SOL_TCP, TCP_KEEPIDLE, (void *)&on, sizeof(on));
		return r ? SWITCH_STATUS_FALSE : SWITCH_STATUS_SUCCESS;
#else
		return SWITCH_STATUS_NOTIMPL;
#endif
	}

	if (opt == SWITCH_SO_TCP_KEEPINTVL) {
#if defined(TCP_KEEPINTVL)
		int r = -10;
		r = setsockopt(sock->socketdes, SOL_TCP, TCP_KEEPINTVL, (void *)&on, sizeof(on));
		return r ? SWITCH_STATUS_FALSE : SWITCH_STATUS_SUCCESS;
#else
		return SWITCH_STATUS_NOTIMPL;
#endif
	}

	return fspr_socket_opt_set(sock, opt, on);
}

SWITCH_DECLARE(switch_status_t) switch_socket_timeout_get(switch_socket_t *sock, switch_interval_time_t *t)
{
	fspr_interval_time_t at = 0;
	switch_status_t status = fspr_socket_timeout_get(sock, &at);
	*t = at;

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_socket_timeout_set(switch_socket_t *sock, switch_interval_time_t t)
{
	return fspr_socket_timeout_set(sock, t);
}

SWITCH_DECLARE(switch_status_t) switch_sockaddr_ip_get(char **addr, switch_sockaddr_t *sa)
{
	return fspr_sockaddr_ip_get(addr, sa);
}

SWITCH_DECLARE(int) switch_sockaddr_equal(const switch_sockaddr_t *sa1, const switch_sockaddr_t *sa2)
{
	return fspr_sockaddr_equal(sa1, sa2);
}

SWITCH_DECLARE(switch_status_t) switch_mcast_join(switch_socket_t *sock, switch_sockaddr_t *join, switch_sockaddr_t *iface, switch_sockaddr_t *source)
{
	return fspr_mcast_join(sock, join, iface, source);
}

SWITCH_DECLARE(switch_status_t) switch_mcast_hops(switch_socket_t *sock, uint8_t ttl)
{
	return fspr_mcast_hops(sock, ttl);
}

SWITCH_DECLARE(switch_status_t) switch_mcast_loopback(switch_socket_t *sock, uint8_t opt)
{
	return fspr_mcast_loopback(sock, opt);
}

SWITCH_DECLARE(switch_status_t) switch_mcast_interface(switch_socket_t *sock, switch_sockaddr_t *iface)
{
	return fspr_mcast_interface(sock, iface);
}


/* socket functions */

SWITCH_DECLARE(const char *) switch_get_addr(char *buf, switch_size_t len, switch_sockaddr_t *in)
{
	if (!in) {
		return SWITCH_BLANK_STRING;
	}

	memset(buf, 0, len);

	if (in->family == AF_INET) {
		get_addr(buf, len, (struct sockaddr *) &in->sa, in->salen);
		return buf;
	}

	get_addr6(buf, len, (struct sockaddr_in6 *) &in->sa, in->salen);
	return buf;
}

SWITCH_DECLARE(int) switch_socket_fd_get(switch_socket_t *sock)
{
	return fspr_socket_fd_get(sock);
}

SWITCH_DECLARE(uint16_t) switch_sockaddr_get_port(switch_sockaddr_t *sa)
{
	return sa->port;
}

SWITCH_DECLARE(int32_t) switch_sockaddr_get_family(switch_sockaddr_t *sa)
{
	return sa->family;
}

SWITCH_DECLARE(switch_status_t) switch_getnameinfo(char **hostname, switch_sockaddr_t *sa, int32_t flags)
{
	return fspr_getnameinfo(hostname, sa, flags);
}

SWITCH_DECLARE(switch_status_t) switch_socket_atmark(switch_socket_t *sock, int *atmark)
{
	return fspr_socket_atmark(sock, atmark);
}

SWITCH_DECLARE(switch_status_t) switch_socket_recvfrom(switch_sockaddr_t *from, switch_socket_t *sock, int32_t flags, char *buf, size_t *len)
{
	int r = SWITCH_STATUS_GENERR;

	if (from && sock && (r = fspr_socket_recvfrom(from, sock, flags, buf, len)) == APR_SUCCESS) {
		from->port = ntohs(from->sa.sin.sin_port);
		/* from->ipaddr_ptr = &(from->sa.sin.sin_addr);
		 * from->ipaddr_ptr = inet_ntoa(from->sa.sin.sin_addr);
		 */
	}

	if (r == 35 || r == 730035) {
		r = SWITCH_STATUS_BREAK;
	}

	return (switch_status_t)r;
}

/* poll stubs */

SWITCH_DECLARE(switch_status_t) switch_pollset_create(switch_pollset_t ** pollset, uint32_t size, switch_memory_pool_t *pool, uint32_t flags)
{
	return fspr_pollset_create(pollset, size, pool, flags);
}

SWITCH_DECLARE(switch_status_t) switch_pollset_add(switch_pollset_t *pollset, const switch_pollfd_t *descriptor)
{
	if (!pollset || !descriptor) {
		return SWITCH_STATUS_FALSE;
	}

	return fspr_pollset_add((fspr_pollset_t *) pollset, (const fspr_pollfd_t *) descriptor);
}

SWITCH_DECLARE(switch_status_t) switch_pollset_remove(switch_pollset_t *pollset, const switch_pollfd_t *descriptor)
{
	if (!pollset || !descriptor) {
		return SWITCH_STATUS_FALSE;
	}

	return fspr_pollset_remove((fspr_pollset_t *) pollset, (const fspr_pollfd_t *) descriptor);
}

SWITCH_DECLARE(switch_status_t) switch_socket_create_pollfd(switch_pollfd_t **pollfd, switch_socket_t *sock, int16_t flags, void *client_data, switch_memory_pool_t *pool)
{
	if (!pollfd || !sock) {
		return SWITCH_STATUS_FALSE;
	}

	if ((*pollfd = (switch_pollfd_t*)fspr_palloc(pool, sizeof(switch_pollfd_t))) == 0) {
		return SWITCH_STATUS_MEMERR;
	}

	memset(*pollfd, 0, sizeof(switch_pollfd_t));

	(*pollfd)->desc_type = (switch_pollset_type_t) APR_POLL_SOCKET;
	(*pollfd)->reqevents = flags;
	(*pollfd)->desc.s = sock;
	(*pollfd)->client_data = client_data;

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status_t) switch_pollset_poll(switch_pollset_t *pollset, switch_interval_time_t timeout, int32_t *num, const switch_pollfd_t **descriptors)
{
	fspr_status_t st = SWITCH_STATUS_FALSE;

	if (pollset) {
		st = fspr_pollset_poll((fspr_pollset_t *) pollset, timeout, num, (const fspr_pollfd_t **) descriptors);

		if (st == APR_TIMEUP) {
			st = SWITCH_STATUS_TIMEOUT;
		}
	}

	return st;
}

SWITCH_DECLARE(switch_status_t) switch_poll(switch_pollfd_t *aprset, int32_t numsock, int32_t *nsds, switch_interval_time_t timeout)
{
	fspr_status_t st = SWITCH_STATUS_FALSE;

	if (aprset) {
		st = fspr_poll((fspr_pollfd_t *) aprset, numsock, nsds, timeout);

		if (numsock == 1 && ((aprset[0].rtnevents & APR_POLLERR) || (aprset[0].rtnevents & APR_POLLHUP) || (aprset[0].rtnevents & APR_POLLNVAL))) {
			st = SWITCH_STATUS_GENERR;
		} else if (st == APR_TIMEUP) {
			st = SWITCH_STATUS_TIMEOUT;
		}
	}

	return st;
}

SWITCH_DECLARE(switch_status_t) switch_socket_create_pollset(switch_pollfd_t ** poll, switch_socket_t *sock, int16_t flags, switch_memory_pool_t *pool)
{
	switch_pollset_t *pollset;

	if (switch_pollset_create(&pollset, 1, pool, 0) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_GENERR;
	}

	if (switch_socket_create_pollfd(poll, sock, flags, sock, pool) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_GENERR;
	}

	if (switch_pollset_add(pollset, *poll) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_GENERR;
	}

	return SWITCH_STATUS_SUCCESS;
}

/* apr-util stubs */

/* UUID Handling (apr-util) */

SWITCH_DECLARE(void) switch_uuid_format(char *buffer, const switch_uuid_t *uuid)
{
#ifndef WIN32
	uuid_unparse_lower(uuid->data, buffer);
#else
	RPC_CSTR buf;
	UuidToString((const UUID *) uuid, &buf);
	strcpy(buffer, (const char *) buf);
	RpcStringFree(&buf);
#endif
}

SWITCH_DECLARE(void) switch_uuid_get(switch_uuid_t *uuid)
{
	switch_mutex_lock(runtime.uuid_mutex);
#ifndef WIN32
	uuid_generate(uuid->data);
#else
	UuidCreate((UUID *) uuid);
#endif
	switch_mutex_unlock(runtime.uuid_mutex);
}

SWITCH_DECLARE(switch_status_t) switch_uuid_parse(switch_uuid_t *uuid, const char *uuid_str)
{
#ifndef WIN32
	if (uuid_parse(uuid_str, uuid->data)) {
		return SWITCH_STATUS_FALSE;
	}
	return SWITCH_STATUS_SUCCESS;
#else
	return UuidFromString((RPC_CSTR) uuid_str, (UUID *) uuid);
#endif
}

SWITCH_DECLARE(switch_status_t) switch_md5(unsigned char digest[SWITCH_MD5_DIGESTSIZE], const void *input, switch_size_t inputLen)
{
#if (defined(HAVE_LIBMD5) || defined(HAVE_LIBMD) || defined(HAVE_MD5INIT))
	MD5_CTX md5_context;

	MD5Init(&md5_context);
	MD5Update(&md5_context, input, inputLen);
	MD5Final(digest, &md5_context);

	return SWITCH_STATUS_SUCCESS;
#elif defined(HAVE_LIBCRYPTO)
	#if OPENSSL_VERSION_NUMBER < 0x30000000
		MD5_CTX md5_context;

		MD5_Init(&md5_context);
		MD5_Update(&md5_context, input, inputLen);
		MD5_Final(digest, &md5_context);
	#else
		EVP_MD_CTX *md5_context;

		/* MD5_Init */
		md5_context = EVP_MD_CTX_new();
		EVP_DigestInit_ex(md5_context, EVP_md5(), NULL);
		/* MD5_Update */
		EVP_DigestUpdate(md5_context, input, inputLen);
		/* MD5_Final */
		EVP_DigestFinal_ex(md5_context, digest, NULL);
		EVP_MD_CTX_free(md5_context);
	#endif

	return SWITCH_STATUS_SUCCESS;
#else
	return SWITCH_STATUS_NOTIMPL;
#endif
}

SWITCH_DECLARE(switch_status_t) switch_md5_string(char digest_str[SWITCH_MD5_DIGEST_STRING_SIZE], const void *input, switch_size_t inputLen)
{
	unsigned char digest[SWITCH_MD5_DIGESTSIZE];
	switch_status_t status = switch_md5(digest, input, inputLen);
	short i, x;
	uint8_t b;

	digest_str[SWITCH_MD5_DIGEST_STRING_SIZE - 1] = '\0';

	for (x = i = 0; x < SWITCH_MD5_DIGESTSIZE; x++) {
		b = (digest[x] >> 4) & 15;
		digest_str[i++] = b + (b > 9 ? 'a' - 10 : '0');
		b = digest[x] & 15;
		digest_str[i++] = b + (b > 9 ? 'a' - 10 : '0');
	}
	digest_str[i] = '\0';

	return status;
}

/* FIFO queues (apr-util) */

SWITCH_DECLARE(switch_status_t) switch_queue_create(switch_queue_t ** queue, unsigned int queue_capacity, switch_memory_pool_t *pool)
{
	return switch_apr_queue_create(queue, queue_capacity, pool);
}

SWITCH_DECLARE(unsigned int) switch_queue_size(switch_queue_t *queue)
{
	return switch_apr_queue_size(queue);
}

SWITCH_DECLARE(switch_status_t) switch_queue_pop(switch_queue_t *queue, void **data)
{
	return switch_apr_queue_pop(queue, data);
}

SWITCH_DECLARE(switch_status_t) switch_queue_pop_timeout(switch_queue_t *queue, void **data, switch_interval_time_t timeout)
{
	return switch_apr_queue_pop_timeout(queue, data, timeout);
}

SWITCH_DECLARE(switch_status_t) switch_queue_push(switch_queue_t *queue, void *data)
{
	fspr_status_t s;

	do {
		s = switch_apr_queue_push(queue, data);
	} while (s == APR_EINTR);

	return s;
}

SWITCH_DECLARE(switch_status_t) switch_queue_trypop(switch_queue_t *queue, void **data)
{
	return switch_apr_queue_trypop(queue, data);
}

SWITCH_DECLARE(switch_status_t) switch_queue_interrupt_all(switch_queue_t *queue)
{
	return switch_apr_queue_interrupt_all(queue);
}

SWITCH_DECLARE(switch_status_t) switch_queue_term(switch_queue_t *queue)
{
	return switch_apr_queue_term(queue);
}

SWITCH_DECLARE(switch_status_t) switch_queue_trypush(switch_queue_t *queue, void *data)
{
	fspr_status_t s;

	do {
		s = switch_apr_queue_trypush(queue, data);
	} while (s == APR_EINTR);

	return s;
}

SWITCH_DECLARE(int) switch_vasprintf(char **ret, const char *fmt, va_list ap)
{
#ifdef HAVE_VASPRINTF
	return vasprintf(ret, fmt, ap);
#else
	char *buf;
	int len;
	size_t buflen;
	va_list ap2;
	char *tmp = NULL;

#ifdef _MSC_VER
#if _MSC_VER >= 1500
	/* hack for incorrect assumption in msvc header files for code analysis */
	__analysis_assume(tmp);
#endif
	ap2 = ap;
#else
	va_copy(ap2, ap);
#endif

	len = vsnprintf(tmp, 0, fmt, ap2);

	if (len > 0 && (buf = malloc((buflen = (size_t) (len + 1)))) != NULL) {
		len = vsnprintf(buf, buflen, fmt, ap);
		*ret = buf;
	} else {
		*ret = NULL;
		len = -1;
	}

	va_end(ap2);
	return len;
#endif
}

SWITCH_DECLARE(switch_status_t) switch_match_glob(const char *pattern, switch_array_header_t ** result, switch_memory_pool_t *pool)
{
	return fspr_match_glob(pattern, (fspr_array_header_t **) result, pool);
}

/**
 * Create an anonymous pipe.
 * @param in The file descriptor to use as input to the pipe.
 * @param out The file descriptor to use as output from the pipe.
 * @param pool The pool to operate on.
 */
SWITCH_DECLARE(switch_status_t) switch_file_pipe_create(switch_file_t ** in, switch_file_t ** out, switch_memory_pool_t *pool)
{
	return fspr_file_pipe_create((fspr_file_t **) in, (fspr_file_t **) out, pool);
}

/**
 * Get the timeout value for a pipe or manipulate the blocking state.
 * @param thepipe The pipe we are getting a timeout for.
 * @param timeout The current timeout value in microseconds.
 */
SWITCH_DECLARE(switch_status_t) switch_file_pipe_timeout_get(switch_file_t *thepipe, switch_interval_time_t *timeout)
{
	return fspr_file_pipe_timeout_get((fspr_file_t *) thepipe, (fspr_interval_time_t *) timeout);
}

/**
 * Set the timeout value for a pipe or manipulate the blocking state.
 * @param thepipe The pipe we are setting a timeout on.
 * @param timeout The timeout value in microseconds.  Values < 0 mean wait
 *        forever, 0 means do not wait at all.
 */
SWITCH_DECLARE(switch_status_t) switch_file_pipe_timeout_set(switch_file_t *thepipe, switch_interval_time_t timeout)
{
	return fspr_file_pipe_timeout_set((fspr_file_t *) thepipe, (fspr_interval_time_t) timeout);
}


/**
 * stop the current thread
 * @param thd The thread to stop
 * @param retval The return value to pass back to any thread that cares
 */
SWITCH_DECLARE(switch_status_t) switch_thread_exit(switch_thread_t *thd, switch_status_t retval)
{
	return fspr_thread_exit((fspr_thread_t *) thd, retval);
}


/**
 * block until the desired thread stops executing.
 * @param retval The return value from the dead thread.
 * @param thd The thread to join
 */
SWITCH_DECLARE(switch_status_t) switch_thread_join(switch_status_t *retval, switch_thread_t *thd)
{
	if ( !thd ) {
		char *bt = switch_print_backtrace();
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ERROR: Attempting to join thread that does not exist, backtrace is:%s\n", bt);
		if (bt) free(bt);
		return SWITCH_STATUS_FALSE;
	}

	return fspr_thread_join((fspr_status_t *) retval, (fspr_thread_t *) thd);
}


SWITCH_DECLARE(switch_status_t) switch_atomic_init(switch_memory_pool_t *pool)
{
	return fspr_atomic_init((fspr_pool_t *) pool);
}

SWITCH_DECLARE(uint32_t) switch_atomic_read(volatile switch_atomic_t *mem)
{
#ifdef fspr_atomic_t
	return fspr_atomic_read((fspr_atomic_t *)mem);
#else
	return fspr_atomic_read32((fspr_uint32_t *)mem);
#endif
}

SWITCH_DECLARE(void) switch_atomic_set(volatile switch_atomic_t *mem, uint32_t val)
{
#ifdef fspr_atomic_t
	fspr_atomic_set((fspr_atomic_t *)mem, val);
#else
	fspr_atomic_set32((fspr_uint32_t *)mem, val);
#endif
}

SWITCH_DECLARE(void) switch_atomic_add(volatile switch_atomic_t *mem, uint32_t val)
{
#ifdef fspr_atomic_t
	fspr_atomic_add((fspr_atomic_t *)mem, val);
#else
	fspr_atomic_add32((fspr_uint32_t *)mem, val);
#endif
}

SWITCH_DECLARE(void) switch_atomic_inc(volatile switch_atomic_t *mem)
{
#ifdef fspr_atomic_t
	fspr_atomic_inc((fspr_atomic_t *)mem);
#else
	fspr_atomic_inc32((fspr_uint32_t *)mem);
#endif
}

SWITCH_DECLARE(int) switch_atomic_dec(volatile switch_atomic_t *mem)
{
#ifdef fspr_atomic_t
	return fspr_atomic_dec((fspr_atomic_t *)mem);
#else
	return fspr_atomic_dec32((fspr_uint32_t *)mem);
#endif
}

SWITCH_DECLARE(char *) switch_strerror(switch_status_t statcode, char *buf, switch_size_t bufsize)
{
	return fspr_strerror(statcode, buf, bufsize);
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
