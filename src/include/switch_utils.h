/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 *
 * switch_utils.h -- Compatability and Helper Code
 *
 */
#ifndef SWITCH_UTILS_H
#define SWITCH_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <switch.h>

#ifndef snprintf
#define snprintf apr_snprintf
#endif
#ifndef vsnprintf
#define vsnprintf apr_vsnprintf
#endif

#ifdef HAVE_TIMEVAL_STRUCT
extern struct timeval tv;
#ifdef WIN32
typedef long switch_timeval_t;
typedef long switch_suseconds_t;
#else
typedef typeof(tv.tv_sec) switch_timeval_t;
typedef typeof(tv.tv_usec) switch_suseconds_t;
#endif
#endif

#define switch_copy_string apr_cpystrn
#define switch_test_flag(obj, flag) ((obj)->flags & flag)
#define switch_set_flag(obj, flag) (obj)->flags |= (flag)
#define switch_clear_flag(obj, flag) (obj)->flags &= ~(flag)
#define switch_copy_flags(dest, src, flags) (dest)->flags &= ~(flags);	(dest)->flags |= ((src)->flags & (flags))
#define switch_strlen_zero(s) (s && *s != '\0') ? 0 : 1
#define switch_yield(ms) apr_sleep(ms * 10); apr_thread_yield();
#define SWITCH_DECLARE_GLOBAL_STRING_FUNC(fname, vname) static void fname(char *string) { if (vname) {free(vname); vname = NULL;}vname = strdup(string);}

SWITCH_DECLARE(unsigned int) switch_separate_string(char *buf, char delim, char **array, int arraylen);
SWITCH_DECLARE(switch_status) switch_socket_create_pollfd(switch_pollfd_t *poll, switch_socket_t *sock, unsigned int flags, switch_memory_pool *pool);
SWITCH_DECLARE(int) switch_socket_waitfor(switch_pollfd_t *poll, int ms);
SWITCH_DECLARE(void) switch_swap_linear(int16_t *buf, int len);
SWITCH_DECLARE(char *) switch_cut_path(char *in);
SWITCH_DECLARE(int) switch_float_to_short(float *f, short *s, int len);
SWITCH_DECLARE(int) switch_char_to_float(char *c, float *f, int len);
SWITCH_DECLARE(int) switch_float_to_char(float *f, char *c, int len);
SWITCH_DECLARE(int) switch_short_to_float(short *s, float *f, int len);

#if !defined(switch_strdupa) && defined(__GNUC__)
# define switch_strdupa(s)									\
  (__extension__										\
    ({													\
      __const char *__old = (s);						\
      size_t __len = strlen (__old) + 1;				\
      char *__new = (char *) __builtin_alloca (__len);	\
      (char *) memcpy (__new, __old, __len);			\
    }))
#endif


#ifdef HAVE_TIMEVAL_STRUCT
struct timeval switch_tvadd(struct timeval a, struct timeval b);
struct timeval switch_tvsub(struct timeval a, struct timeval b);


/*static struct timeval switch_tvnow(void)
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return t;
}
*/

static struct timeval switch_tv(switch_timeval_t sec, switch_suseconds_t usec)
{
	struct timeval t;
    t.tv_sec = sec;
    t.tv_usec = usec;
    return t;

}

static int switch_tvdiff_ms(struct timeval end, struct timeval start)
{
	/* the offset by 1,000,000 below is intentional...
	   it avoids differences in the way that division
	   is handled for positive and negative numbers, by ensuring
	   that the divisor is always positive
	*/
	return	((end.tv_sec - start.tv_sec) * 1000) +
		(((1000000 + end.tv_usec - start.tv_usec) / 1000) - 1000);
}

#endif

#ifdef __cplusplus
}

#endif

#endif
