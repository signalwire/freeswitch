/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 *
 * switch_apr.c -- apr wrappers and extensions
 *
 */

#include <switch.h>
#ifndef WIN32
#include <switch_private.h>
#endif
#include "private/switch_core_pvt.h"

/* apr headers*/
#include <apr.h>
#include <apr_atomic.h>
#include <apr_pools.h>
#include <apr_hash.h>
#include <apr_network_io.h>
#include <apr_errno.h>
#include <apr_thread_proc.h>
#include <apr_portable.h>
#include <apr_thread_mutex.h>
#include <apr_thread_cond.h>
#include <apr_thread_rwlock.h>
#include <apr_file_io.h>
#include <apr_poll.h>
#include <apr_dso.h>
#include <apr_strings.h>
#define APR_WANT_STDIO
#define APR_WANT_STRFUNC
#include <apr_want.h>
#include <apr_file_info.h>
#include <apr_fnmatch.h>
#include <apr_tables.h>

/* apr_vformatter_buff_t definition*/
#include <apr_lib.h>

/* apr-util headers */
#include <apr_queue.h>
#include <apr_uuid.h>
#include <apr_md5.h>

/* apr stubs */

SWITCH_DECLARE(int) switch_status_is_timeup(int status)
{
	return APR_STATUS_IS_TIMEUP(status);
}

/* Memory Pools */

SWITCH_DECLARE(switch_thread_id_t) switch_thread_self(void)
{
#ifndef WIN32
	return apr_os_thread_current();
#else
	return (switch_thread_id_t) (GetCurrentThreadId());
#endif
}

SWITCH_DECLARE(int) switch_thread_equal(switch_thread_id_t tid1, switch_thread_id_t tid2)
{
#ifdef WIN32
	return (tid1 == tid2);
#else
	return apr_os_thread_equal(tid1, tid2);
#endif

}

SWITCH_DECLARE(unsigned int) switch_ci_hashfunc_default(const char *char_key, switch_ssize_t *klen)
{
	unsigned int hash = 0;
	const unsigned char *key = (const unsigned char *) char_key;
	const unsigned char *p;
	apr_ssize_t i;

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
	return apr_hashfunc_default(key, klen);
}

/* DSO functions */

SWITCH_DECLARE(switch_status_t) switch_dso_load(switch_dso_handle_t ** res_handle, const char *path, switch_memory_pool_t *ctx)
{
	return apr_dso_load(res_handle, path, ctx);
}

SWITCH_DECLARE(switch_status_t) switch_dso_unload(switch_dso_handle_t *handle)
{
	return apr_dso_unload(handle);
}

SWITCH_DECLARE(switch_status_t) switch_dso_sym(switch_dso_handle_sym_t *ressym, switch_dso_handle_t *handle, const char *symname)
{
	return apr_dso_sym(ressym, handle, symname);
}

SWITCH_DECLARE(const char *) switch_dso_error(switch_dso_handle_t *dso, char *buf, size_t bufsize)
{
	return apr_dso_error(dso, buf, bufsize);
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

	return apr_strftime(s, retsize, max, format, (apr_time_exp_t *) tm);
}

SWITCH_DECLARE(switch_status_t) switch_strftime_nocheck(char *s, switch_size_t *retsize, switch_size_t max, const char *format, switch_time_exp_t *tm)
{
	return apr_strftime(s, retsize, max, format, (apr_time_exp_t *) tm);
}

SWITCH_DECLARE(int) switch_snprintf(char *buf, switch_size_t len, const char *format, ...)
{
	va_list ap;
	int ret;
	va_start(ap, format);
	ret = apr_vsnprintf(buf, len, format, ap);
	va_end(ap);
	return ret;
}

SWITCH_DECLARE(int) switch_vsnprintf(char *buf, switch_size_t len, const char *format, va_list ap)
{
	return apr_vsnprintf(buf, len, format, ap);
}

SWITCH_DECLARE(char *) switch_copy_string(char *dst, const char *src, switch_size_t dst_size)
{
	if (!dst)
		return NULL;
	if (!src) {
		*dst = '\0';
		return dst;
	}
	return apr_cpystrn(dst, src, dst_size);
}

/* thread read write lock functions */

SWITCH_DECLARE(switch_status_t) switch_thread_rwlock_create(switch_thread_rwlock_t ** rwlock, switch_memory_pool_t *pool)
{
	return apr_thread_rwlock_create(rwlock, pool);
}

SWITCH_DECLARE(switch_status_t) switch_thread_rwlock_destroy(switch_thread_rwlock_t *rwlock)
{
	return apr_thread_rwlock_destroy(rwlock);
}

SWITCH_DECLARE(switch_memory_pool_t *) switch_thread_rwlock_pool_get(switch_thread_rwlock_t *rwlock)
{
	return apr_thread_rwlock_pool_get(rwlock);
}

SWITCH_DECLARE(switch_status_t) switch_thread_rwlock_rdlock(switch_thread_rwlock_t *rwlock)
{
	return apr_thread_rwlock_rdlock(rwlock);
}

SWITCH_DECLARE(switch_status_t) switch_thread_rwlock_tryrdlock(switch_thread_rwlock_t *rwlock)
{
	return apr_thread_rwlock_tryrdlock(rwlock);
}

SWITCH_DECLARE(switch_status_t) switch_thread_rwlock_wrlock(switch_thread_rwlock_t *rwlock)
{
	return apr_thread_rwlock_wrlock(rwlock);
}

SWITCH_DECLARE(switch_status_t) switch_thread_rwlock_trywrlock(switch_thread_rwlock_t *rwlock)
{
	return apr_thread_rwlock_trywrlock(rwlock);
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
	return apr_thread_rwlock_unlock(rwlock);
}

/* thread mutex functions */

SWITCH_DECLARE(switch_status_t) switch_mutex_init(switch_mutex_t ** lock, unsigned int flags, switch_memory_pool_t *pool)
{
	return apr_thread_mutex_create(lock, flags, pool);
}

SWITCH_DECLARE(switch_status_t) switch_mutex_destroy(switch_mutex_t *lock)
{
	return apr_thread_mutex_destroy(lock);
}

SWITCH_DECLARE(switch_status_t) switch_mutex_lock(switch_mutex_t *lock)
{
	return apr_thread_mutex_lock(lock);
}

SWITCH_DECLARE(switch_status_t) switch_mutex_unlock(switch_mutex_t *lock)
{
	return apr_thread_mutex_unlock(lock);
}

SWITCH_DECLARE(switch_status_t) switch_mutex_trylock(switch_mutex_t *lock)
{
	return apr_thread_mutex_trylock(lock);
}

/* time function stubs */

SWITCH_DECLARE(switch_time_t) switch_time_now(void)
{
#if defined(HAVE_CLOCK_GETTIME) && defined(SWITCH_USE_CLOCK_FUNCS)
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return ts.tv_sec * APR_USEC_PER_SEC + (ts.tv_nsec / 1000);
#else
	return (switch_time_t) apr_time_now();
#endif
}

SWITCH_DECLARE(switch_status_t) switch_time_exp_gmt_get(switch_time_t *result, switch_time_exp_t *input)
{
	return apr_time_exp_gmt_get((apr_time_t *) result, (apr_time_exp_t *) input);
}

SWITCH_DECLARE(switch_status_t) switch_time_exp_get(switch_time_t *result, switch_time_exp_t *input)
{
	return apr_time_exp_get((apr_time_t *) result, (apr_time_exp_t *) input);
}

SWITCH_DECLARE(switch_status_t) switch_time_exp_lt(switch_time_exp_t *result, switch_time_t input)
{
	return apr_time_exp_lt((apr_time_exp_t *) result, input);
}

SWITCH_DECLARE(switch_status_t) switch_time_exp_tz(switch_time_exp_t *result, switch_time_t input, switch_int32_t offs)
{
	return apr_time_exp_tz((apr_time_exp_t *) result, input, (apr_int32_t) offs);
}

SWITCH_DECLARE(switch_status_t) switch_time_exp_gmt(switch_time_exp_t *result, switch_time_t input)
{
	return apr_time_exp_gmt((apr_time_exp_t *) result, input);
}

SWITCH_DECLARE(switch_status_t) switch_rfc822_date(char *date_str, switch_time_t t)
{
	return apr_rfc822_date(date_str, t);
}

SWITCH_DECLARE(switch_time_t) switch_time_make(switch_time_t sec, int32_t usec)
{
	return ((switch_time_t) (sec) * APR_USEC_PER_SEC + (switch_time_t) (usec));
}

/* Thread condition locks */

SWITCH_DECLARE(switch_status_t) switch_thread_cond_create(switch_thread_cond_t ** cond, switch_memory_pool_t *pool)
{
	return apr_thread_cond_create(cond, pool);
}

SWITCH_DECLARE(switch_status_t) switch_thread_cond_wait(switch_thread_cond_t *cond, switch_mutex_t *mutex)
{
	return apr_thread_cond_wait(cond, mutex);
}

SWITCH_DECLARE(switch_status_t) switch_thread_cond_timedwait(switch_thread_cond_t *cond, switch_mutex_t *mutex, switch_interval_time_t timeout)
{
	apr_status_t st = apr_thread_cond_timedwait(cond, mutex, timeout);

	if (st == APR_TIMEUP) {
		st = SWITCH_STATUS_TIMEOUT;
	}

	return st;
}

SWITCH_DECLARE(switch_status_t) switch_thread_cond_signal(switch_thread_cond_t *cond)
{
	return apr_thread_cond_signal(cond);
}

SWITCH_DECLARE(switch_status_t) switch_thread_cond_broadcast(switch_thread_cond_t *cond)
{
	return apr_thread_cond_broadcast(cond);
}

SWITCH_DECLARE(switch_status_t) switch_thread_cond_destroy(switch_thread_cond_t *cond)
{
	return apr_thread_cond_destroy(cond);
}

/* file i/o stubs */

SWITCH_DECLARE(switch_status_t) switch_file_open(switch_file_t ** newf, const char *fname, int32_t flag, switch_fileperms_t perm,
												 switch_memory_pool_t *pool)
{
	return apr_file_open(newf, fname, flag, perm, pool);
}

SWITCH_DECLARE(switch_status_t) switch_file_seek(switch_file_t *thefile, switch_seek_where_t where, int64_t *offset)
{
	apr_status_t rv;
	apr_off_t off = (apr_off_t) (*offset);
	rv = apr_file_seek(thefile, where, &off);
	*offset = (int64_t) off;
	return rv;
}

SWITCH_DECLARE(switch_status_t) switch_file_copy(const char *from_path, const char *to_path, switch_fileperms_t perms, switch_memory_pool_t *pool)
{
	return apr_file_copy(from_path, to_path, perms, pool);
}


SWITCH_DECLARE(switch_status_t) switch_file_close(switch_file_t *thefile)
{
	return apr_file_close(thefile);
}

SWITCH_DECLARE(switch_status_t) switch_file_trunc(switch_file_t *thefile, int64_t offset)
{
	return apr_file_trunc(thefile, offset);
}

SWITCH_DECLARE(switch_status_t) switch_file_lock(switch_file_t *thefile, int type)
{
	return apr_file_lock(thefile, type);
}

SWITCH_DECLARE(switch_status_t) switch_file_rename(const char *from_path, const char *to_path, switch_memory_pool_t *pool)
{
	return apr_file_rename(from_path, to_path, pool);
}

SWITCH_DECLARE(switch_status_t) switch_file_remove(const char *path, switch_memory_pool_t *pool)
{
	return apr_file_remove(path, pool);
}

SWITCH_DECLARE(switch_status_t) switch_file_read(switch_file_t *thefile, void *buf, switch_size_t *nbytes)
{
	return apr_file_read(thefile, buf, nbytes);
}

SWITCH_DECLARE(switch_status_t) switch_file_write(switch_file_t *thefile, const void *buf, switch_size_t *nbytes)
{
	return apr_file_write(thefile, buf, nbytes);
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
	return apr_file_mktemp(thefile, templ, flags, pool);
}

SWITCH_DECLARE(switch_size_t) switch_file_get_size(switch_file_t *thefile)
{
	struct apr_finfo_t finfo;
	return apr_file_info_get(&finfo, APR_FINFO_SIZE, thefile) == SWITCH_STATUS_SUCCESS ? (switch_size_t) finfo.size : 0;
}

SWITCH_DECLARE(switch_status_t) switch_directory_exists(const char *dirname, switch_memory_pool_t *pool)
{
	apr_dir_t *dir_handle;
	switch_memory_pool_t *our_pool = NULL;
	switch_status_t status;

	if (!pool) {
		switch_core_new_memory_pool(&our_pool);
		pool = our_pool;
	}

	if ((status = apr_dir_open(&dir_handle, dirname, pool)) == APR_SUCCESS) {
		apr_dir_close(dir_handle);
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
	apr_finfo_t info = { 0 };

	if (zstr(filename)) {
		return status;
	}

	if (!pool) {
		switch_core_new_memory_pool(&our_pool);
	}

	apr_stat(&info, filename, wanted, pool ? pool : our_pool);
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
	return apr_dir_make(path, perm, pool);
}

SWITCH_DECLARE(switch_status_t) switch_dir_make_recursive(const char *path, switch_fileperms_t perm, switch_memory_pool_t *pool)
{
	return apr_dir_make_recursive(path, perm, pool);
}

struct switch_dir {
	apr_dir_t *dir_handle;
	apr_finfo_t finfo;
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
	if ((status = apr_dir_open(&(dir->dir_handle), dirname, pool)) == APR_SUCCESS) {
		*new_dir = dir;
	} else {
		free(dir);
		*new_dir = NULL;
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_dir_close(switch_dir_t *thedir)
{
	switch_status_t status = apr_dir_close(thedir->dir_handle);

	free(thedir);
	return status;
}

SWITCH_DECLARE(const char *) switch_dir_next_file(switch_dir_t *thedir, char *buf, switch_size_t len)
{
	const char *fname = NULL;
	apr_int32_t finfo_flags = APR_FINFO_DIRENT | APR_FINFO_TYPE | APR_FINFO_NAME;
	const char *name;

	while (apr_dir_read(&(thedir->finfo), finfo_flags, thedir->dir_handle) == SWITCH_STATUS_SUCCESS) {

		if (thedir->finfo.filetype != APR_REG && thedir->finfo.filetype != APR_LNK) {
			continue;
		}

		if (!(name = thedir->finfo.fname)) {
			name = thedir->finfo.name;
		}

		if (!name) {
			continue;
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

SWITCH_DECLARE(switch_status_t) switch_threadattr_create(switch_threadattr_t ** new_attr, switch_memory_pool_t *pool)
{
	return apr_threadattr_create(new_attr, pool);
}

SWITCH_DECLARE(switch_status_t) switch_threadattr_detach_set(switch_threadattr_t *attr, int32_t on)
{
	return apr_threadattr_detach_set(attr, on);
}

SWITCH_DECLARE(switch_status_t) switch_threadattr_stacksize_set(switch_threadattr_t *attr, switch_size_t stacksize)
{
	return apr_threadattr_stacksize_set(attr, stacksize);
}

#ifndef WIN32
struct apr_threadattr_t {
	apr_pool_t *pool;
	pthread_attr_t attr;
	int priority;
};
#endif

SWITCH_DECLARE(switch_status_t) switch_threadattr_priority_increase(switch_threadattr_t *attr)
{
#ifndef WIN32
	attr->priority = 10;
#endif
	return SWITCH_STATUS_SUCCESS;
}

static char TT_KEY[] = "1";

SWITCH_DECLARE(switch_status_t) switch_thread_create(switch_thread_t ** new_thread, switch_threadattr_t *attr,
													 switch_thread_start_t func, void *data, switch_memory_pool_t *cont)
{
	switch_core_memory_pool_set_data(cont, "_in_thread", TT_KEY);
	return apr_thread_create(new_thread, attr, func, data, cont);
}

/* socket stubs */

SWITCH_DECLARE(switch_status_t) switch_os_sock_get(switch_os_socket_t *thesock, switch_socket_t *sock)
{
	return apr_os_sock_get(thesock, sock);
}

SWITCH_DECLARE(switch_status_t) switch_socket_addr_get(switch_sockaddr_t ** sa, switch_bool_t remote, switch_socket_t *sock)
{
	return apr_socket_addr_get(sa, (apr_interface_e) remote, sock);
}

SWITCH_DECLARE(switch_status_t) switch_socket_create(switch_socket_t ** new_sock, int family, int type, int protocol, switch_memory_pool_t *pool)
{
	return apr_socket_create(new_sock, family, type, protocol, pool);
}

SWITCH_DECLARE(switch_status_t) switch_socket_shutdown(switch_socket_t *sock, switch_shutdown_how_e how)
{
	return apr_socket_shutdown(sock, (apr_shutdown_how_e) how);
}

SWITCH_DECLARE(switch_status_t) switch_socket_close(switch_socket_t *sock)
{
	return apr_socket_close(sock);
}

SWITCH_DECLARE(switch_status_t) switch_socket_bind(switch_socket_t *sock, switch_sockaddr_t *sa)
{
	return apr_socket_bind(sock, sa);
}

SWITCH_DECLARE(switch_status_t) switch_socket_listen(switch_socket_t *sock, int32_t backlog)
{
	return apr_socket_listen(sock, backlog);
}

SWITCH_DECLARE(switch_status_t) switch_socket_accept(switch_socket_t ** new_sock, switch_socket_t *sock, switch_memory_pool_t *pool)
{
	return apr_socket_accept(new_sock, sock, pool);
}

SWITCH_DECLARE(switch_status_t) switch_socket_connect(switch_socket_t *sock, switch_sockaddr_t *sa)
{
	return apr_socket_connect(sock, sa);
}

SWITCH_DECLARE(switch_status_t) switch_socket_send(switch_socket_t *sock, const char *buf, switch_size_t *len)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_size_t req = *len, wrote = 0, need = *len;
	int to_count = 0;

	while ((wrote < req && status == SWITCH_STATUS_SUCCESS) || (need == 0 && status == SWITCH_STATUS_BREAK) || status == 730035 || status == 35) {
		need = req - wrote;
		status = apr_socket_send(sock, buf + wrote, &need);
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
	return status;
}

SWITCH_DECLARE(switch_status_t) switch_socket_send_nonblock(switch_socket_t *sock, const char *buf, switch_size_t *len)
{
	if (!sock || !buf || !len) {
		return SWITCH_STATUS_GENERR;
	}
	
	return apr_socket_send(sock, buf, len);
}

SWITCH_DECLARE(switch_status_t) switch_socket_sendto(switch_socket_t *sock, switch_sockaddr_t *where, int32_t flags, const char *buf,
													 switch_size_t *len)
{
	if (!where || !buf || !len || !*len) {
		return SWITCH_STATUS_GENERR;
	}
	return apr_socket_sendto(sock, where, flags, buf, len);
}

SWITCH_DECLARE(switch_status_t) switch_socket_recv(switch_socket_t *sock, char *buf, switch_size_t *len)
{
	switch_status_t r;

	r = apr_socket_recv(sock, buf, len);

	if (r == 35 || r == 730035) {
		r = SWITCH_STATUS_BREAK;
	}

	return r;
}

SWITCH_DECLARE(switch_status_t) switch_sockaddr_create(switch_sockaddr_t **sa, switch_memory_pool_t *pool)
{
	switch_sockaddr_t *new_sa;
	unsigned short family = APR_INET;

	new_sa = apr_pcalloc(pool, sizeof(apr_sockaddr_t));
	switch_assert(new_sa);
	new_sa->pool = pool;
	memset(new_sa, 0, sizeof(*new_sa));

    new_sa->family = family;
    new_sa->sa.sin.sin_family = family;

    new_sa->salen = sizeof(struct sockaddr_in);
    new_sa->addr_str_len = 16;
    new_sa->ipaddr_ptr = &(new_sa->sa.sin.sin_addr);
    new_sa->ipaddr_len = sizeof(struct in_addr);

	*sa = new_sa;
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_sockaddr_info_get(switch_sockaddr_t ** sa, const char *hostname, int32_t family,
														 switch_port_t port, int32_t flags, switch_memory_pool_t *pool)
{
	return apr_sockaddr_info_get(sa, hostname, family, port, flags, pool);
}

SWITCH_DECLARE(switch_status_t) switch_socket_opt_set(switch_socket_t *sock, int32_t opt, int32_t on)
{
	return apr_socket_opt_set(sock, opt, on);
}

SWITCH_DECLARE(switch_status_t) switch_socket_timeout_set(switch_socket_t *sock, switch_interval_time_t t)
{
	return apr_socket_timeout_set(sock, t);
}

SWITCH_DECLARE(switch_status_t) switch_sockaddr_ip_get(char **addr, switch_sockaddr_t *sa)
{
	return apr_sockaddr_ip_get(addr, sa);
}

SWITCH_DECLARE(int) switch_sockaddr_equal(const switch_sockaddr_t *sa1, const switch_sockaddr_t *sa2)
{
	return apr_sockaddr_equal(sa1, sa2);
}

SWITCH_DECLARE(switch_status_t) switch_mcast_join(switch_socket_t *sock, switch_sockaddr_t *join, switch_sockaddr_t *iface, switch_sockaddr_t *source)
{
	return apr_mcast_join(sock, join, iface, source);
}

SWITCH_DECLARE(switch_status_t) switch_mcast_hops(switch_socket_t *sock, uint8_t ttl)
{
	return apr_mcast_hops(sock, ttl);
}

SWITCH_DECLARE(switch_status_t) switch_mcast_loopback(switch_socket_t *sock, uint8_t opt)
{
	return apr_mcast_loopback(sock, opt);
}

/* socket functions */

SWITCH_DECLARE(const char *) switch_get_addr(char *buf, switch_size_t len, switch_sockaddr_t *in)
{
	if (!in) {
		return SWITCH_BLANK_STRING;
	}

	if (in->family == AF_INET) {
		return get_addr(buf, len, (struct sockaddr *) &in->sa, in->salen);
	}

	return get_addr6(buf, len, (struct sockaddr_in6 *) &in->sa, in->salen);
}

SWITCH_DECLARE(uint16_t) switch_sockaddr_get_port(switch_sockaddr_t *sa)
{
	return sa->port;
}

SWITCH_DECLARE(int32_t) switch_sockaddr_get_family(switch_sockaddr_t *sa)
{
	return sa->family;
}

SWITCH_DECLARE(switch_status_t) switch_socket_atmark(switch_socket_t *sock, int *atmark)
{
	return apr_socket_atmark(sock, atmark);
}

SWITCH_DECLARE(switch_status_t) switch_socket_recvfrom(switch_sockaddr_t *from, switch_socket_t *sock, int32_t flags, char *buf, size_t *len)
{
	apr_status_t r = SWITCH_STATUS_GENERR;

	if (from && sock && (r = apr_socket_recvfrom(from, sock, flags, buf, len)) == APR_SUCCESS) {
		from->port = ntohs(from->sa.sin.sin_port);
		/* from->ipaddr_ptr = &(from->sa.sin.sin_addr);
		 * from->ipaddr_ptr = inet_ntoa(from->sa.sin.sin_addr);
		 */
	}

	if (r == 35 || r == 730035) {
		r = SWITCH_STATUS_BREAK;
	}

	return r;
}

/* poll stubs */

SWITCH_DECLARE(switch_status_t) switch_pollset_create(switch_pollset_t ** pollset, uint32_t size, switch_memory_pool_t *p, uint32_t flags)
{
	return apr_pollset_create(pollset, size, p, flags);
}

SWITCH_DECLARE(switch_status_t) switch_pollset_add(switch_pollset_t *pollset, const switch_pollfd_t *descriptor)
{
	if (!pollset || !descriptor) {
		return SWITCH_STATUS_FALSE;
	}

	return apr_pollset_add((apr_pollset_t *) pollset, (const apr_pollfd_t *) descriptor);
}

SWITCH_DECLARE(switch_status_t) switch_pollset_remove(switch_pollset_t *pollset, const switch_pollfd_t *descriptor)
{
	if (!pollset || !descriptor) {
		return SWITCH_STATUS_FALSE;
	}	
	
	return apr_pollset_remove((apr_pollset_t *) pollset, (const apr_pollfd_t *) descriptor);
}

SWITCH_DECLARE(switch_status_t) switch_socket_create_pollfd(switch_pollfd_t **pollfd, switch_socket_t *sock, int16_t flags, void *client_data, switch_memory_pool_t *pool)
{
	if (!pollfd || !sock) {
		return SWITCH_STATUS_FALSE;
	}
	
	if ((*pollfd = (switch_pollfd_t*)apr_palloc(pool, sizeof(switch_pollfd_t))) == 0) {
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
	apr_status_t st = SWITCH_STATUS_FALSE;
	
	if (pollset) {
		st = apr_pollset_poll((apr_pollset_t *) pollset, timeout, num, (const apr_pollfd_t **) descriptors);
		
		if (st == APR_TIMEUP) {
			st = SWITCH_STATUS_TIMEOUT;
		}
	}
	
	return st;
}

SWITCH_DECLARE(switch_status_t) switch_poll(switch_pollfd_t *aprset, int32_t numsock, int32_t *nsds, switch_interval_time_t timeout)
{
	apr_status_t st = SWITCH_STATUS_FALSE;

	if (aprset) {
		st = apr_poll((apr_pollfd_t *) aprset, numsock, nsds, timeout);

		if (st == APR_TIMEUP) {
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
	apr_uuid_format(buffer, (const apr_uuid_t *) uuid);
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
	apr_uuid_get((apr_uuid_t *) uuid);
#else
	UuidCreate((UUID *) uuid);
#endif
	switch_mutex_unlock(runtime.uuid_mutex);
}

SWITCH_DECLARE(switch_status_t) switch_uuid_parse(switch_uuid_t *uuid, const char *uuid_str)
{
#ifndef WIN32
	return apr_uuid_parse((apr_uuid_t *) uuid, uuid_str);
#else
	return UuidFromString((RPC_CSTR) uuid_str, (UUID *) uuid);
#endif
}

SWITCH_DECLARE(switch_status_t) switch_md5(unsigned char digest[SWITCH_MD5_DIGESTSIZE], const void *input, switch_size_t inputLen)
{
	return apr_md5(digest, input, inputLen);
}

SWITCH_DECLARE(switch_status_t) switch_md5_string(char digest_str[SWITCH_MD5_DIGEST_STRING_SIZE], const void *input, switch_size_t inputLen)
{
	unsigned char digest[SWITCH_MD5_DIGESTSIZE];
	apr_status_t status = apr_md5(digest, input, inputLen);
	int x;

	digest_str[SWITCH_MD5_DIGEST_STRING_SIZE - 1] = '\0';

	for (x = 0; x < SWITCH_MD5_DIGESTSIZE; x++) {
		switch_snprintf(digest_str + (x * 2), 3, "%02x", digest[x]);
	}

	return status;
}

/* FIFO queues (apr-util) */

SWITCH_DECLARE(switch_status_t) switch_queue_create(switch_queue_t ** queue, unsigned int queue_capacity, switch_memory_pool_t *pool)
{
	return apr_queue_create(queue, queue_capacity, pool);
}

SWITCH_DECLARE(unsigned int) switch_queue_size(switch_queue_t *queue)
{
	return apr_queue_size(queue);
}

SWITCH_DECLARE(switch_status_t) switch_queue_pop(switch_queue_t *queue, void **data)
{
	return apr_queue_pop(queue, data);
}

SWITCH_DECLARE(switch_status_t) switch_queue_pop_timeout(switch_queue_t *queue, void **data, switch_interval_time_t timeout)
{
	return apr_queue_pop_timeout(queue, data, timeout);
}


SWITCH_DECLARE(switch_status_t) switch_queue_push(switch_queue_t *queue, void *data)
{
	apr_status_t s;

	do {
		s = apr_queue_push(queue, data);
	} while (s == APR_EINTR);

	return s;
}

SWITCH_DECLARE(switch_status_t) switch_queue_trypop(switch_queue_t *queue, void **data)
{
	return apr_queue_trypop(queue, data);
}

SWITCH_DECLARE(switch_status_t) switch_queue_interrupt_all(switch_queue_t *queue)
{
	return apr_queue_interrupt_all(queue);
}

SWITCH_DECLARE(switch_status_t) switch_queue_trypush(switch_queue_t *queue, void *data)
{
	apr_status_t s;

	do {
		s = apr_queue_trypush(queue, data);
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

SWITCH_DECLARE(switch_status_t) switch_match_glob(const char *pattern, switch_array_header_t ** result, switch_memory_pool_t *p)
{
	return apr_match_glob(pattern, (apr_array_header_t **) result, p);
}

/**
 * Create an anonymous pipe.
 * @param in The file descriptor to use as input to the pipe.
 * @param out The file descriptor to use as output from the pipe.
 * @param pool The pool to operate on.
 */
SWITCH_DECLARE(switch_status_t) switch_file_pipe_create(switch_file_t ** in, switch_file_t ** out, switch_memory_pool_t *p)
{
	return apr_file_pipe_create((apr_file_t **) in, (apr_file_t **) out, p);
}

/**
 * Get the timeout value for a pipe or manipulate the blocking state.
 * @param thepipe The pipe we are getting a timeout for.
 * @param timeout The current timeout value in microseconds. 
 */
SWITCH_DECLARE(switch_status_t) switch_file_pipe_timeout_get(switch_file_t *thepipe, switch_interval_time_t *timeout)
{
	return apr_file_pipe_timeout_get((apr_file_t *) thepipe, (apr_interval_time_t *) timeout);
}

/**
 * Set the timeout value for a pipe or manipulate the blocking state.
 * @param thepipe The pipe we are setting a timeout on.
 * @param timeout The timeout value in microseconds.  Values < 0 mean wait 
 *        forever, 0 means do not wait at all.
 */
SWITCH_DECLARE(switch_status_t) switch_file_pipe_timeout_set(switch_file_t *thepipe, switch_interval_time_t timeout)
{
	return apr_file_pipe_timeout_set((apr_file_t *) thepipe, (apr_interval_time_t) timeout);
}


/**
 * stop the current thread
 * @param thd The thread to stop
 * @param retval The return value to pass back to any thread that cares
 */
SWITCH_DECLARE(switch_status_t) switch_thread_exit(switch_thread_t *thd, switch_status_t retval)
{
	return apr_thread_exit((apr_thread_t *) thd, retval);
}

/**
 * block until the desired thread stops executing.
 * @param retval The return value from the dead thread.
 * @param thd The thread to join
 */
SWITCH_DECLARE(switch_status_t) switch_thread_join(switch_status_t *retval, switch_thread_t *thd)
{
	if ( !thd ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ERROR: Attempting to join thread that does not exist\n");
		return SWITCH_STATUS_FALSE;
	}

	return apr_thread_join((apr_status_t *) retval, (apr_thread_t *) thd);
}


SWITCH_DECLARE(switch_status_t) switch_atomic_init(switch_memory_pool_t *pool)
{
	return apr_atomic_init((apr_pool_t *) pool);
}

SWITCH_DECLARE(uint32_t) switch_atomic_read(volatile switch_atomic_t *mem)
{
#ifdef apr_atomic_t
	return apr_atomic_read((apr_atomic_t *)mem);
#else
	return apr_atomic_read32((apr_uint32_t *)mem);
#endif
}

SWITCH_DECLARE(void) switch_atomic_set(volatile switch_atomic_t *mem, uint32_t val)
{
#ifdef apr_atomic_t
	apr_atomic_set((apr_atomic_t *)mem, val);
#else
	apr_atomic_set32((apr_uint32_t *)mem, val);
#endif
}

SWITCH_DECLARE(void) switch_atomic_add(volatile switch_atomic_t *mem, uint32_t val)
{
#ifdef apr_atomic_t
	apr_atomic_add((apr_atomic_t *)mem, val);
#else
	apr_atomic_add32((apr_uint32_t *)mem, val);
#endif
}

SWITCH_DECLARE(void) switch_atomic_inc(volatile switch_atomic_t *mem)
{
#ifdef apr_atomic_t
	apr_atomic_inc((apr_atomic_t *)mem);
#else
	apr_atomic_inc32((apr_uint32_t *)mem);
#endif
}

SWITCH_DECLARE(int) switch_atomic_dec(volatile switch_atomic_t *mem)
{
#ifdef apr_atomic_t
	return apr_atomic_dec((apr_atomic_t *)mem);
#else
	return apr_atomic_dec32((apr_uint32_t *)mem);
#endif
}

SWITCH_DECLARE(char *) switch_strerror(switch_status_t statcode, char *buf, switch_size_t bufsize)
{
       return apr_strerror(statcode, buf, bufsize);
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
