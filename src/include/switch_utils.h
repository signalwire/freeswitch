/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 * Seven Du <dujinfang@gmail.com>
 *
 *
 * switch_utils.h -- Compatability and Helper Code
 *
 */
/*! \file switch_utils.h
    \brief Compatability and Helper Code

	Just a miscelaneaous set of general utility/helper functions.

*/
#ifndef SWITCH_UTILS_H
#define SWITCH_UTILS_H

#include <switch.h>
#include <math.h>

SWITCH_BEGIN_EXTERN_C 

#define SWITCH_URL_UNSAFE "\r\n #%&+:;<=>?@[\\]^`{|}\""


static inline uint32_t switch_round_to_step(uint32_t num, uint32_t step)
{
	uint32_t r;
	uint32_t x;

	if (!num) return 0;
	
	r = (num % step);
	x = num - r;
	
	if (r > step / 2) {
		x += step;
	}
	
	return x;
}

/* https://code.google.com/p/stringencoders/wiki/PerformanceAscii 
   http://www.azillionmonkeys.com/qed/asmexample.html
*/
static inline uint32_t switch_toupper(uint32_t eax)
{
uint32_t ebx = (0x7f7f7f7ful & eax) + 0x05050505ul;
ebx = (0x7f7f7f7ful & ebx) + 0x1a1a1a1aul;
 ebx = ((ebx & ~eax) >> 2 ) & 0x20202020ul;
 return eax - ebx;
}

/* https://code.google.com/p/stringencoders/wiki/PerformanceAscii 
   http://www.azillionmonkeys.com/qed/asmexample.html
*/
static inline uint32_t switch_tolower(uint32_t eax)
{
	uint32_t ebx = (0x7f7f7f7ful & eax) + 0x25252525ul;
	ebx = (0x7f7f7f7ful & ebx) + 0x1a1a1a1aul;
	ebx = ((ebx & ~eax) >> 2)  & 0x20202020ul;
	return eax + ebx;
}


#ifdef FS_64BIT

/* https://code.google.com/p/stringencoders/wiki/PerformanceAscii 
   http://www.azillionmonkeys.com/qed/asmexample.html
*/
static inline uint64_t switch_toupper64(uint64_t eax)
{
uint64_t ebx = (0x7f7f7f7f7f7f7f7full & eax) + 0x0505050505050505ull;
 ebx = (0x7f7f7f7f7f7f7f7full & ebx) + 0x1a1a1a1a1a1a1a1aull;
 ebx = ((ebx & ~eax) >> 2 ) & 0x2020202020202020ull;
 return eax - ebx;
}

/* https://code.google.com/p/stringencoders/wiki/PerformanceAscii 
   http://www.azillionmonkeys.com/qed/asmexample.html
*/
static inline uint64_t switch_tolower64(uint64_t eax)
{
	uint64_t ebx = (0x7f7f7f7f7f7f7f7full & eax) + 0x2525252525252525ull;
	ebx = (0x7f7f7f7f7f7f7f7full & ebx) + 0x1a1a1a1a1a1a1a1aull;
	ebx = ((ebx & ~eax) >> 2)  & 0x2020202020202020ull;
	return eax + ebx;
}

static inline void switch_toupper_max(char *s)
{
	uint64_t *b,*p;
	char *c;
	size_t l;

	l = strlen(s);

	p = (uint64_t *) s;

	while (l > 8) {
		b = p;
		*b = (uint64_t) switch_toupper64(*b);
		b++;
		p++;
		l -= 8;
	}

	c = (char *)p;

	while(l > 0) {
		*c = (char) switch_toupper(*c);
		c++;
		l--;
	}

}

static inline void switch_tolower_max(char *s)
{
	uint64_t *b,*p;
	char *c;
	size_t l;

	l = strlen(s);

	p = (uint64_t *) s;

	while (l > 8) {
		b = p;
		*b = (uint64_t) switch_tolower64(*b);
		b++;
		p++;
		l -= 8;
	}

	c = (char *)p;

	while(l > 0) {
		*c = (char) switch_tolower(*c);
		c++;
		l--;
	}

}

#else 

static inline void switch_toupper_max(char *s)
{
	uint32_t *b,*p;
	char *c;
	size_t l;

	l = strlen(s);

	p = (uint32_t *) s;

	while (l > 4) {
		b = p;
		*b = (uint32_t) switch_toupper(*b);
		b++;
		p++;
		l -= 4;
	}

	c = (char *)p;

	while(l > 0) {
		*c = (char) switch_toupper(*c);
		c++;
		l--;
	}
	
}

static inline void switch_tolower_max(char *s)
{
	uint32_t *b,*p;
	char *c;
	size_t l;

	l = strlen(s);

	p = (uint32_t *) s;

	while (l > 4) {
		b = p;
		*b = (uint32_t) switch_tolower(*b);
		b++;
		p++;
		l -= 4;
	}

	c = (char *)p;

	while(l > 0) {
		*c = (char) switch_tolower(*c);
		c++;
		l--;
	}
	
}
#endif




SWITCH_DECLARE(int) old_switch_toupper(int c);
SWITCH_DECLARE(int) old_switch_tolower(int c);
SWITCH_DECLARE(int) switch_isalnum(int c);
SWITCH_DECLARE(int) switch_isalpha(int c);
SWITCH_DECLARE(int) switch_iscntrl(int c);
SWITCH_DECLARE(int) switch_isdigit(int c);
SWITCH_DECLARE(int) switch_isgraph(int c);
SWITCH_DECLARE(int) switch_islower(int c);
SWITCH_DECLARE(int) switch_isprint(int c);
SWITCH_DECLARE(int) switch_ispunct(int c);
SWITCH_DECLARE(int) switch_isspace(int c);
SWITCH_DECLARE(int) switch_isupper(int c);
SWITCH_DECLARE(int) switch_isxdigit(int c);

typedef union{
    uint32_t v4;
    struct in6_addr v6;
} ip_t;

SWITCH_DECLARE(switch_bool_t) switch_testv6_subnet(ip_t _ip, ip_t _net, ip_t _mask);


SWITCH_DECLARE(char *) switch_print_host(switch_sockaddr_t *addr, char *buf, switch_size_t len);

#define switch_goto_status(_status, _label) status = _status; goto _label
#define switch_goto_int(_n, _i, _label) _n = _i; goto _label
#define switch_samples_per_packet(rate, interval) ((uint32_t)((float)rate / (1000.0f / (float)interval)))
#define SWITCH_SMAX 32767
#define SWITCH_SMIN -32768
#define switch_normalize_to_16bit(n) if (n > SWITCH_SMAX) n = SWITCH_SMAX; else if (n < SWITCH_SMIN) n = SWITCH_SMIN;
#define switch_codec2str(codec,buf,len) snprintf(buf, len, "%s@%uh@%ui", \
                                                 codec->implementation->iananame, \
                                                 codec->implementation->samples_per_second, \
                                                 codec->implementation->microseconds_per_packet / 1000)



/*!
  \brief Test for NULL or zero length string
  \param s the string to test
  \return true value if the string is NULL or zero length
*/
_Check_return_ static inline int _zstr(_In_opt_z_ const char *s)
{
	return !s || *s == '\0';
}
#ifdef _PREFAST_
#define zstr(x) (_zstr(x) ? 1 : __analysis_assume(x),0)
#else
#define zstr(x) _zstr(x)
#endif
#define switch_strlen_zero(x) zstr(x)
#define switch_strlen_zero_buf(x) zstr_buf(x)
#define zstr_buf(s) (*(s) == '\0')
static inline switch_bool_t switch_is_moh(const char *s)
{
	if (zstr(s) || !strcasecmp(s, "silence") || !strcasecmp(s, "indicate_hold")) {
		return SWITCH_FALSE;
	}
	return SWITCH_TRUE;
}


#define zset(_a, _b) if (!zstr(_b)) _a = _b


/* find a character (find) in a string (in) and return a pointer to that point in the string where the character was found 
   using the array (allowed) as allowed non-matching characters, when (allowed) is NULL, behaviour should be identical to strchr()
 */
static inline char *switch_strchr_strict(const char *in, char find, const char *allowed)
{
	const char *p;

	switch_assert(in);

	p = in;

	if (!*p) return NULL;

	while(p && *p) {
		const char *a = allowed;
		int acceptable = 0;

		if (*p == find) break;

		if (!a) {
			acceptable = 1;
		} else {

			while(a && *a) {

				if (*p == *a) {
					acceptable = 1;
					break;
				}
			
				a++;
			}

		}
		
		if (!acceptable) return NULL;

		p++;
	}

	return (char *) p;
}

#define switch_arraylen(_a) (sizeof(_a) / sizeof(_a[0]))
#define switch_split(_data, _delim, _array) switch_separate_string(_data, _delim, _array, switch_arraylen(_array))

#define switch_is_valid_rate(_tmp) (_tmp == 8000 || _tmp == 12000 || _tmp == 16000 || _tmp == 24000 || _tmp == 32000 || _tmp == 11025 || _tmp == 22050 || _tmp == 44100 || _tmp == 48000)


#ifdef _MSC_VER
#pragma warning(disable:6011)
#endif
static inline int switch_string_has_escaped_data(const char *in)
{
	const char *i;

	switch_assert(in);

	i = strchr(in, '\\');

	while (i && *i == '\\') {
		i++;
		if (*i == '\\' || *i == 'n' || *i == 's' || *i == 't' || *i == '\'') {
			return 1;
		}
		i = strchr(i, '\\');
	}

	return 0;
}
#ifdef _MSC_VER
#pragma warning(default:6011)
#endif

SWITCH_DECLARE(switch_status_t) switch_b64_encode(unsigned char *in, switch_size_t ilen, unsigned char *out, switch_size_t olen);
SWITCH_DECLARE(switch_size_t) switch_b64_decode(char *in, char *out, switch_size_t olen);
SWITCH_DECLARE(char *) switch_amp_encode(char *s, char *buf, switch_size_t len);



static inline char *switch_print_bits(const unsigned char *byte, char *buf, switch_size_t buflen)
{

	int i, j = 0, k = 0, l = 0;

	while(k < buflen) {
		l = 0;
		for (i = 7; i >= 0; i--) {
			buf[j++] = (*byte & (1 << i)) ? '1' : '0';
			if (++l % 4 == 0) {
				buf[j++] = ' ';
			}
		}
		k++;
		byte++;
	}
	
	if (buf[j-1] == ' ') j--;
	buf[j++] = '\0';
	return buf;
}




static inline switch_bool_t switch_is_digit_string(const char *s)
{

	while (s && *s) {
		if (*s < 48 || *s > 57) {
			return SWITCH_FALSE;
		}
		s++;
	}

	return SWITCH_TRUE;
}

static inline char switch_itodtmf(char i)
{
	char r = i;

	if (i > 9 && i < 14) {
		r += 55;
	} else {
		r += 48;
	}

	return r;
}

static inline int switch_dtmftoi(char *s)
{
	int r;

	switch_assert(s);

	if (!(r = atoi(s))) {
		int l = tolower((unsigned char)*s);
		if (l > 96 && l < 101) {
			r = l - 87;
		}
	}

	return r;
}

static inline uint32_t switch_known_bitrate(switch_payload_t payload)
{
	switch(payload) {
	case 0: /* PCMU */ return 64000;
	case 3: /* GSM */ return 13200;
	case 4: /* G723 */ return 6300;
	case 7: /* LPC */ return 2400;
	case 8: /* PCMA */ return 64000;
	case 9: /* G722 */ return 64000;
	case 18: /* G729 */ return 8000;
	default: break;
	}

	return 0;
}

SWITCH_DECLARE(switch_size_t) switch_fd_read_line(int fd, char *buf, switch_size_t len);
SWITCH_DECLARE(switch_size_t) switch_fd_read_dline(int fd, char **buf, switch_size_t *len);
SWITCH_DECLARE(switch_size_t) switch_fp_read_dline(FILE *fd, char **buf, switch_size_t *len);

SWITCH_DECLARE(switch_status_t) switch_frame_alloc(switch_frame_t **frame, switch_size_t size);
SWITCH_DECLARE(switch_status_t) switch_frame_dup(switch_frame_t *orig, switch_frame_t **clone);
SWITCH_DECLARE(switch_status_t) switch_frame_free(switch_frame_t **frame);
SWITCH_DECLARE(switch_bool_t) switch_is_number(const char *str);
SWITCH_DECLARE(switch_bool_t) switch_is_leading_number(const char *str);
SWITCH_DECLARE(char *) switch_find_parameter(const char *str, const char *param, switch_memory_pool_t *pool);

/*!
  \brief Evaluate the truthfullness of a string expression
  \param expr a string expression
  \return true or false 
*/
static inline int switch_true(const char *expr)
{
	return ((expr && ( !strcasecmp(expr, "yes") ||
					   !strcasecmp(expr, "on") ||
					   !strcasecmp(expr, "true") ||
					   !strcasecmp(expr, "t") ||
					   !strcasecmp(expr, "enabled") ||
					   !strcasecmp(expr, "active") ||
					   !strcasecmp(expr, "allow") ||
					   (switch_is_number(expr) && atoi(expr)))) ? SWITCH_TRUE : SWITCH_FALSE);
}

static inline switch_byte_t switch_true_byte(const char *expr)
{
	return (switch_byte_t)switch_true(expr);
}

#define switch_true_buf(expr)\
((( !strcasecmp(expr, "yes") ||\
!strcasecmp(expr, "on") ||\
!strcasecmp(expr, "true") ||\
!strcasecmp(expr, "t") ||\
!strcasecmp(expr, "enabled") ||\
!strcasecmp(expr, "active") ||\
!strcasecmp(expr, "allow") ||\
(switch_is_number(expr) && atoi(expr)))) ? SWITCH_TRUE : SWITCH_FALSE)

/*!
  \brief Evaluate the falsefullness of a string expression
  \param expr a string expression
  \return true or false 
*/
static inline int switch_false(const char *expr)
{
	return ((expr && ( !strcasecmp(expr, "no") ||
					   !strcasecmp(expr, "off") ||
					   !strcasecmp(expr, "false") ||
					   !strcasecmp(expr, "f") ||
					   !strcasecmp(expr, "disabled") ||
					   !strcasecmp(expr, "inactive") ||
					   !strcasecmp(expr, "disallow") ||
					   (switch_is_number(expr) && !atoi(expr)))) ? SWITCH_TRUE : SWITCH_FALSE);
}


SWITCH_DECLARE(switch_status_t) switch_resolve_host(const char *host, char *buf, size_t buflen);


/*!
  \brief find local ip of the box
  \param buf the buffer to write the ip address found into
  \param len the length of the buf
  \param mask the CIDR found (AF_INET only)
  \param family the address family to return (AF_INET or AF_INET6)
  \return SWITCH_STATUS_SUCCESSS for success, otherwise failure
*/
SWITCH_DECLARE(switch_status_t) switch_find_local_ip(_Out_opt_bytecapcount_(len)
													 char *buf, _In_ int len, _In_opt_ int *mask, _In_ int family);

/*!
  \brief find primary ip of the specified interface
  \param buf the buffer to write the ip address found into
  \param len the length of the buf
  \param mask the CIDR found (AF_INET only)
  \param ifname interface name to check
  \param family the address family to return (AF_INET or AF_INET6)
  \return SWITCH_STATUS_SUCCESSS for success, otherwise failure
*/
SWITCH_DECLARE(switch_status_t) switch_find_interface_ip(_Out_opt_bytecapcount_(len)
													 char *buf, _In_ int len, _In_opt_ int *mask, _In_ const char *ifname, _In_ int family);

/*!
  \brief find the char representation of an ip adress
  \param buf the buffer to write the ip adress found into
  \param len the length of the buf
  \param sa the struct sockaddr * to get the adress from
  \param salen the length of sa
  \return the ip adress string
*/
SWITCH_DECLARE(char *) get_addr(char *buf, switch_size_t len, struct sockaddr *sa, socklen_t salen);
SWITCH_DECLARE(char *) get_addr6(char *buf, switch_size_t len, struct sockaddr_in6 *sa, socklen_t salen);

SWITCH_DECLARE(int) get_addr_int(switch_sockaddr_t *sa);
SWITCH_DECLARE(int) switch_cmp_addr(switch_sockaddr_t *sa1, switch_sockaddr_t *sa2);
SWITCH_DECLARE(int) switch_cp_addr(switch_sockaddr_t *sa1, switch_sockaddr_t *sa2);

/*!
  \brief get the port number of an ip address
  \param sa the struct sockaddr * to get the port from
  \return the ip adress string
*/
SWITCH_DECLARE(unsigned short) get_port(struct sockaddr *sa);

/*!
  \brief flags to be used with switch_build_uri()
 */
	 enum switch_uri_flags {
		 SWITCH_URI_NUMERIC_HOST = 1,
		 SWITCH_URI_NUMERIC_PORT = 2,
		 SWITCH_URI_NO_SCOPE = 4
	 };

/*!
  \brief build a URI string from components
  \param uri output string
  \param size maximum size of output string (including trailing null)
  \param scheme URI scheme
  \param user user part or null if none
  \param sa host address
  \param flags logical OR-ed combination of flags from \ref switch_uri_flags
  \return number of characters printed (not including the trailing null)
 */
SWITCH_DECLARE(int) switch_build_uri(char *uri, switch_size_t size, const char *scheme, const char *user, const switch_sockaddr_t *sa, int flags);

#define SWITCH_STATUS_IS_BREAK(x) (x == SWITCH_STATUS_BREAK || x == 730035 || x == 35 || x == SWITCH_STATUS_INTR)


#ifdef _MSC_VER

#define switch_errno() WSAGetLastError()

static inline int switch_errno_is_break(int errcode)
{
	return errcode == WSAEWOULDBLOCK || errcode == WSAEINPROGRESS || errcode == WSAEINTR;
}

#else

#define switch_errno() errno

static inline int switch_errno_is_break(int errcode)
{
  return errcode == EAGAIN || errcode == EWOULDBLOCK || errcode == EINPROGRESS || errcode == EINTR || errcode == ETIMEDOUT;
}

#endif


/*!
  \brief Return a printable name of a switch_priority_t
  \param priority the priority to get the name of
  \return the printable form of the priority
*/
SWITCH_DECLARE(const char *) switch_priority_name(switch_priority_t priority);

/*!
  \brief Return the RFC2833 character based on an event id
  \param event the event id to convert
  \return the character represented by the event or null for an invalid event
*/
SWITCH_DECLARE(char) switch_rfc2833_to_char(int event);

/*!
  \brief Return the RFC2833 event based on an key character
  \param key the charecter to encode
  \return the event id for the specified character or -1 on an invalid input
*/
SWITCH_DECLARE(unsigned char) switch_char_to_rfc2833(char key);

/*!
  \brief determine if a character is a valid DTMF key
  \param key the key to test
  \return TRUE or FALSE
 */
#define is_dtmf(key)  ((key > 47 && key < 58) || (key > 64 && key < 69) || (key > 96 && key < 101) || key == 35 || key == 42 || key == 87 || key == 119 || key == 70 || key == 102)

#define end_of(_s) *(*_s == '\0' ? _s : _s + strlen(_s) - 1)
#define end_of_p(_s) (*_s == '\0' ? _s : _s + strlen(_s) - 1)
/*!
  \brief Test for the existance of a flag on an arbitary object
  \param obj the object to test
  \param flag the or'd list of flags to test
  \return true value if the object has the flags defined
*/
#define switch_test_flag(obj, flag) ((obj)->flags & flag)

/*!
  \brief Set a flag on an arbitrary object
  \param obj the object to set the flags on
  \param flag the or'd list of flags to set
*/
#define switch_set_flag(obj, flag) (obj)->flags |= (flag)

/*!
  \brief Set a flag on an arbitrary object while locked
  \param obj the object to set the flags on
  \param flag the or'd list of flags to set
*/
#define switch_set_flag_locked(obj, flag) assert((obj)->flag_mutex != NULL); \
switch_mutex_lock((obj)->flag_mutex);								\
(obj)->flags |= (flag);\
switch_mutex_unlock((obj)->flag_mutex);

/*!
  \brief Clear a flag on an arbitrary object
  \param obj the object to test
  \param flag the or'd list of flags to clear
*/
#define switch_clear_flag_locked(obj, flag) switch_mutex_lock((obj)->flag_mutex); (obj)->flags &= ~(flag); switch_mutex_unlock((obj)->flag_mutex);

/*!
  \brief Clear a flag on an arbitrary object while locked
  \param obj the object to test
  \param flag the or'd list of flags to clear
*/
#define switch_clear_flag(obj, flag) (obj)->flags &= ~(flag)

/*!
  \brief Copy flags from one arbitrary object to another
  \param dest the object to copy the flags to
  \param src the object to copy the flags from
  \param flags the flags to copy
*/
#define switch_copy_flags(dest, src, flags) (dest)->flags &= ~(flags);	(dest)->flags |= ((src)->flags & (flags))

#define switch_set_string(_dst, _src) switch_copy_string(_dst, _src, sizeof(_dst))


static inline char *switch_sanitize_number(char *number)
{
	char *p = number, *q;
	char warp[] = "/:";
	int i;

	switch_assert(number);

	if (!(strchr(p, '/') || strchr(p, ':') || strchr(p, '@'))) {
		return number;
	}

	while ((q = strrchr(p, '@')))
		*q = '\0';
	
	for (i = 0; i < (int) strlen(warp); i++) {
		while (p && (q = strchr(p, warp[i])))
			p = q + 1;
	}

	return p;
}

static inline switch_bool_t switch_string_var_check(char *s, switch_bool_t disable)
{
	char *p;
	char *dol = NULL;

	for (p = s; p && *p; p++) {
		if (*p == '$') {
			dol = p;
		} else if (dol) {
			if (*p == '{') {
				if (disable) {
					*dol = '%';
					dol = NULL;
				} else {
					return SWITCH_TRUE;
				}
			} else if (*p != '\\') {
				dol = NULL;
			}
		}
	}
	return SWITCH_FALSE;
}


static inline switch_bool_t switch_string_var_check_const(const char *s)
{
	const char *p;
	int dol = 0;

	for (p = s; p && *p; p++) {
		if (*p == '$') {
			dol = 1;
		} else if (dol) {
			if (*p == '{') {
				return SWITCH_TRUE;
			} else if (*p != '\\') {
				dol = 0;
			}
		}
	}
	return SWITCH_FALSE;
}

static inline char *switch_var_clean_string(char *s)
{
	switch_string_var_check(s, SWITCH_TRUE);
	return s;
}

static inline char *switch_clean_string(char *s)
{
	char *p;
	for (p = s; p && *p; p++) {
		uint8_t x = (uint8_t) * p;
		if ((x < 32) && x != '\n' && x != '\r') {
			*p = ' ';
		}
	}

	return s;
}


static inline char *switch_clean_name_string(char *s)
{
	char *p;
	for (p = s; p && *p; p++) {
		uint8_t x = (uint8_t) * p;
		if ((x < 32) ||	x == '\'' || x == '"' || x == '<' || x == '>' || x == '\\' || x == ':' || x == '@' || x == '/') {
			*p = ' ';
		}
		if ( (p == s) && (*p == ' ') ) {
			s++;
		}
	}

	return s;
}



/*!
  \brief Turn a string into a number (default if NULL)
  \param nptr the string
  \param dft the default
  \return the number version of the string or the default
*/
static inline int switch_safe_atoi(const char *nptr, int dft)
{
	return nptr ? atoi(nptr) : dft;
}


/*!
  \brief Free a pointer and set it to NULL unless it already is NULL
  \param it the pointer
*/
#define switch_safe_free(it) if (it) {free(it);it=NULL;}

static inline char *switch_safe_strdup(const char *it)
{
	if (it) {
		return strdup(it);
	}

	return NULL;
}


#ifdef _MSC_VER
#pragma warning(disable:6011)
#endif
static inline char *switch_lc_strdup(const char *it)
{
	char *dup;
	char *p;

	if (it) {
		dup = strdup(it);
		for (p = dup; p && *p; p++) {
			*p = (char) switch_tolower(*p);
		}
		return dup;
	}

	return NULL;
}


static inline char *switch_uc_strdup(const char *it)
{
	char *dup;
	char *p;

	if (it) {
		dup = strdup(it);
		for (p = dup; p && *p; p++) {
			*p = (char) switch_toupper(*p);
		}
		return dup;
	}

	return NULL;
}
#ifdef _MSC_VER
#pragma warning(default:6011)
#endif


/*!
  \brief Test if one string is inside another with extra case checking
  \param s the inner string
  \param q the big string
  \return SWITCH_TRUE or SWITCH_FALSE
*/
static inline switch_bool_t switch_strstr(char *s, char *q)
{
	char *p, *S = NULL, *Q = NULL;
	switch_bool_t tf = SWITCH_FALSE;

	if (!s || !q) {
		return SWITCH_FALSE;
	}

	if (strstr(s, q)) {
		return SWITCH_TRUE;
	}

	S = strdup(s);

	switch_assert(S != NULL);

	for (p = S; p && *p; p++) {
		*p = (char) switch_toupper(*p);
	}

	if (strstr(S, q)) {
		tf = SWITCH_TRUE;
		goto done;
	}

	Q = strdup(q);
	switch_assert(Q != NULL);

	for (p = Q; p && *p; p++) {
		*p = (char) switch_toupper(*p);
	}

	if (strstr(s, Q)) {
		tf = SWITCH_TRUE;
		goto done;
	}

	if (strstr(S, Q)) {
		tf = SWITCH_TRUE;
		goto done;
	}

  done:
	switch_safe_free(S);
	switch_safe_free(Q);

	return tf;
}


/*!
  \brief Make a null string a blank string instead
  \param s the string to test
  \return the original string or blank string.
*/
#define switch_str_nil(s) (s ? s : "")

/*!
  \brief Wait a desired number of microseconds and yield the CPU
*/
#define switch_yield(ms) switch_sleep(ms);

/*!
  \brief Converts a string representation of a date into a switch_time_t
  \param in the string
  \return the epoch time in usec
*/
SWITCH_DECLARE(switch_time_t) switch_str_time(const char *in);
#define switch_time_from_sec(sec)   ((switch_time_t)(sec) * 1000000)

/*!
  \brief Declares a function designed to set a dynamic global string
  \param fname the function name to declare
  \param vname the name of the global pointer to modify with the new function
*/
#define SWITCH_DECLARE_GLOBAL_STRING_FUNC(fname, vname) static void fname(const char *string) { if (!string) return;\
		if (vname) {free(vname); vname = NULL;}vname = strdup(string);} static void fname(const char *string)

/*!
  \brief Separate a string into an array based on a character delimiter
  \param buf the string to parse
  \param delim the character delimiter
  \param array the array to split the values into
  \param arraylen the max number of elements in the array
  \return the number of elements added to the array
*/
SWITCH_DECLARE(unsigned int) switch_separate_string(_In_ char *buf, char delim, _Post_count_(return) char **array, unsigned int arraylen);
SWITCH_DECLARE(unsigned int) switch_separate_string_string(char *buf, char *delim, _Post_count_(return) char **array, unsigned int arraylen);


SWITCH_DECLARE(char *) switch_strip_spaces(char *str, switch_bool_t dup);
SWITCH_DECLARE(char *) switch_strip_whitespace(const char *str);
SWITCH_DECLARE(char *) switch_strip_commas(char *in, char *out, switch_size_t len);
SWITCH_DECLARE(char *) switch_strip_nonnumerics(char *in, char *out, switch_size_t len);
SWITCH_DECLARE(char *) switch_separate_paren_args(char *str);
SWITCH_DECLARE(const char *) switch_stristr(const char *instr, const char *str);
SWITCH_DECLARE(switch_bool_t) switch_is_lan_addr(const char *ip);
SWITCH_DECLARE(char *) switch_replace_char(char *str, char from, char to, switch_bool_t dup);
SWITCH_DECLARE(switch_bool_t) switch_ast2regex(const char *pat, char *rbuf, size_t len);

/*!
  \brief Escape a string by prefixing a list of characters with an escape character
  \param pool a memory pool to use
  \param in the string
  \param delim the list of characters to escape
  \param esc the escape character
  \return the escaped string
*/
SWITCH_DECLARE(char *) switch_escape_char(switch_memory_pool_t *pool, char *in, const char *delim, char esc);

SWITCH_DECLARE(char *) switch_escape_string(const char *in, char *out, switch_size_t outlen);
SWITCH_DECLARE(char *) switch_escape_string_pool(const char *in, switch_memory_pool_t *pool);

/*!
  \brief Wait for a socket
  \param poll the pollfd to wait on
  \param ms the number of milliseconds to wait
  \return the requested condition
*/
SWITCH_DECLARE(int) switch_socket_waitfor(switch_pollfd_t *poll, int ms);

/*!
  \brief Create a pointer to the file name in a given file path eliminating the directory name
  \return the pointer to the next character after the final / or \\ characters
*/
SWITCH_DECLARE(const char *) switch_cut_path(const char *in);

SWITCH_DECLARE(char *) switch_string_replace(const char *string, const char *search, const char *replace);
SWITCH_DECLARE(switch_status_t) switch_string_match(const char *string, size_t string_len, const char *search, size_t search_len);
SWITCH_DECLARE(int) switch_strcasecmp_any(const char *str, ...);

/*!
  \brief Quote shell argument
  \param string the string to quote (example: a ' b"' c)
  \return the quoted string (gives: 'a '\'' b"'\'' c' for unices, "a ' b ' c" for MS Windows), should be freed
*/
SWITCH_DECLARE(char *) switch_util_quote_shell_arg(const char *string);

/*!
  \brief Quote shell argument, allocating from pool if provided
  \param string the string to quote (example: a ' b"' c)
  \param pool a memory pool to use
  \return the quoted string (gives: 'a '\'' b"'\'' c' for unices, "a ' b ' c" for MS Windows), if pool not provided, returned value should be freed
*/
SWITCH_DECLARE(char *) switch_util_quote_shell_arg_pool(const char *string, switch_memory_pool_t *pool);


#define SWITCH_READ_ACCEPTABLE(status) (status == SWITCH_STATUS_SUCCESS || status == SWITCH_STATUS_BREAK || status == SWITCH_STATUS_INUSE)


static inline int32_t switch_calc_bitrate(int w, int h, int quality, double fps)
{
	int r;

	/* KUSH GAUGE*/

	if (!fps) fps = 15;
	
	r = (int32_t)((double)(w * h * fps * (quality ? quality : 1)) * 0.07) / 1000;

	if (!quality) r /= 2;

	return r;

}

static inline int32_t switch_parse_bandwidth_string(const char *bwv)
{
	float bw = 0;

	if (!bwv) return 0;

	if (!strcasecmp(bwv, "auto")) {
		return -1;
	}	

	if ((bw = (float) atof(bwv))) {
		if (bw < 0) return 0;

		if (strstr(bwv, "KB")) {
			bw *= 8;
		} else if (strstr(bwv, "mb")) {
			bw *= 1024;
		} else if (strstr(bwv, "MB")) {
			bw *= 8192;
		}
	}

	return (int32_t) roundf(bw);
}

static inline int switch_needs_url_encode(const char *s)
{
	const char hex[] = "0123456789ABCDEF";
	const char *p, *e = end_of_p(s);

	
	for(p = s; p && *p; p++) {
		if (*p == '%' && e-p > 1) {
			if (strchr(hex, *(p+1)) && strchr(hex, *(p+2))) {
				p++;
				continue;
			}
		}

		if (strchr(SWITCH_URL_UNSAFE, *p)) {
			return 1;
		}
	}

	return 0;
}

SWITCH_DECLARE(char *) switch_url_encode_opt(const char *url, char *buf, size_t len, switch_bool_t double_encode);
SWITCH_DECLARE(char *) switch_url_encode(const char *url, char *buf, size_t len);
SWITCH_DECLARE(char *) switch_url_decode(char *s);
SWITCH_DECLARE(switch_bool_t) switch_simple_email(const char *to,
												  const char *from,
												  const char *headers,
												  const char *body, const char *file, const char *convert_cmd, const char *convert_ext);
SWITCH_DECLARE(char *) switch_find_end_paren(const char *s, char open, char close);

static inline void switch_separate_file_params(const char *file, char **file_portion, char **params_portion)
{
	char *e = NULL;
	char *space = strdup(file);

	file = space;

	*file_portion = NULL;
	*params_portion = NULL;
	
	while (*file == '{') {
		e = switch_find_end_paren(file, '{', '}');
		file = e + 1;
		while(*file == ' ') file++;
	}


	if (e) {
		*file_portion = strdup((char *)file);
		*++e = '\0';
		*params_portion = (char *)space;
	} else {
		*file_portion = (char *)space;
	}
	
	return;
}

static inline switch_bool_t switch_is_file_path(const char *file)
{
	const char *e;
	int r;

	if (zstr(file)) {
		return SWITCH_FALSE;
	}

	while(*file == '{') {
		if ((e = switch_find_end_paren(file, '{', '}'))) {
			file = e + 1;
			while(*file == ' ') file++;
		}
	}

#ifdef WIN32
	r = (*file == '\\' || *(file + 1) == ':' || *file == '/' || strstr(file, SWITCH_URL_SEPARATOR));
#else
	r = ((*file == '/') || strstr(file, SWITCH_URL_SEPARATOR));
#endif

	return r ? SWITCH_TRUE : SWITCH_FALSE;
}


static inline const char *switch_parse_audio_col(switch_audio_col_t col)
{
	const char *field = NULL;

	switch (col) {
	case SWITCH_AUDIO_COL_STR_TITLE:
		field = "title";
		break;
	case SWITCH_AUDIO_COL_STR_COMMENT:
		field = "comment";
		break;
	case SWITCH_AUDIO_COL_STR_ARTIST:
		field = "artist";
		break;
	case SWITCH_AUDIO_COL_STR_DATE:
		field = "date";
		break;
	case SWITCH_AUDIO_COL_STR_SOFTWARE:
		field = "software";
		break;
	case SWITCH_AUDIO_COL_STR_COPYRIGHT:
		field = "copyright";
		break;
	default:
		break;
	}

	return field;
}

SWITCH_DECLARE(int) switch_parse_cidr(const char *string, ip_t *ip, ip_t *mask, uint32_t *bitp);
SWITCH_DECLARE(switch_status_t) switch_network_list_create(switch_network_list_t **list, const char *name, switch_bool_t default_type,
														   switch_memory_pool_t *pool);
SWITCH_DECLARE(switch_status_t) switch_network_list_add_cidr_token(switch_network_list_t *list, const char *cidr_str, switch_bool_t ok, const char *token);
#define switch_network_list_add_cidr(_list, _cidr_str, _ok) switch_network_list_add_cidr_token(_list, _cidr_str, _ok, NULL)

SWITCH_DECLARE(char *) switch_network_ipv4_mapped_ipv6_addr(const char* ip_str);
SWITCH_DECLARE(switch_status_t) switch_network_list_add_host_mask(switch_network_list_t *list, const char *host, const char *mask_str, switch_bool_t ok);
SWITCH_DECLARE(switch_bool_t) switch_network_list_validate_ip_token(switch_network_list_t *list, uint32_t ip, const char **token);
SWITCH_DECLARE(switch_bool_t) switch_network_list_validate_ip6_token(switch_network_list_t *list, ip_t ip, const char **token);
#define switch_network_list_validate_ip(_list, _ip) switch_network_list_validate_ip_token(_list, _ip, NULL);

#define switch_test_subnet(_ip, _net, _mask) (_mask ? ((_net & _mask) == (_ip & _mask)) : _net ? _net == _ip : 1)

SWITCH_DECLARE(int) switch_inet_pton(int af, const char *src, void *dst);

SWITCH_DECLARE(const char *) switch_dow_int2str(int val);
SWITCH_DECLARE(int) switch_dow_str2int(const char *exp);
SWITCH_DECLARE(switch_bool_t) switch_dow_cmp(const char *exp, int val);
SWITCH_DECLARE(int) switch_number_cmp(const char *exp, int val);
SWITCH_DECLARE(int) switch_tod_cmp(const char *exp, int val);

SWITCH_DECLARE(int) switch_fulldate_cmp(const char *exp, switch_time_t *ts);
SWITCH_DECLARE(void) switch_split_date(const char *exp, int *year, int *month, int *day);
SWITCH_DECLARE(void) switch_split_time(const char *exp, int *hour, int *min, int *sec);

/*!
  \brief Split a user@domain string as user and domain
  \param in the input string
  \param user the string to put the user into
  \param domain the string to put the domain into
  \return 1 if successfull
  \note Extended formats protocol:user@domain:port (Example: sip:toto@example.org)
*/
SWITCH_DECLARE(int) switch_split_user_domain(char *in, char **user, char **domain);

/* malloc or DIE macros */
#ifdef NDEBUG
#define switch_malloc(ptr, len) (void)( (!!(ptr = malloc(len))) || (fprintf(stderr,"ABORT! Malloc failure at: %s:%d", __FILE__, __LINE__),abort(), 0), ptr )
#define switch_zmalloc(ptr, len) (void)( (!!(ptr = calloc(1, (len)))) || (fprintf(stderr,"ABORT! Malloc failure at: %s:%d", __FILE__, __LINE__),abort(), 0), ptr)
#if (_MSC_VER >= 1500)			// VC9+
#define switch_strdup(ptr, s) (void)( (!!(ptr = _strdup(s))) || (fprintf(stderr,"ABORT! Malloc failure at: %s:%d", __FILE__, __LINE__),abort(), 0), ptr)
#else
#define switch_strdup(ptr, s) (void)( (!!(ptr = strdup(s))) || (fprintf(stderr,"ABORT! Malloc failure at: %s:%d", __FILE__, __LINE__),abort(), 0), ptr)
#endif
#else
#if (_MSC_VER >= 1500)			// VC9+
#define switch_malloc(ptr, len) (void)(assert(((ptr) = malloc((len)))),ptr);__analysis_assume( ptr )
#define switch_zmalloc(ptr, len) (void)(assert((ptr = calloc(1, (len)))),ptr);__analysis_assume( ptr )
#define switch_strdup(ptr, s) (void)(assert(((ptr) = _strdup(s))),ptr);__analysis_assume( ptr )
#else
#define switch_malloc(ptr, len) (void)(assert(((ptr) = malloc((len)))),ptr)
#define switch_zmalloc(ptr, len) (void)(assert((ptr = calloc(1, (len)))),ptr)
#define switch_strdup(ptr, s) (void)(assert(((ptr) = strdup((s)))),ptr)
#endif
#endif

#define DUMP_EVENT(_e) 	{char *event_str;switch_event_serialize(_e, &event_str, SWITCH_FALSE);switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "DUMP\n%s\n", event_str);free(event_str);}

#ifndef _MSC_VER
#define switch_inet_ntop inet_ntop
#else
SWITCH_DECLARE(const char *) switch_inet_ntop(int af, void const *src, char *dst, size_t size);
#endif

SWITCH_DECLARE(char *) switch_uuid_str(char *buf, switch_size_t len);
SWITCH_DECLARE(char *) switch_format_number(const char *num);

SWITCH_DECLARE(unsigned int) switch_atoui(const char *nptr);
SWITCH_DECLARE(unsigned long) switch_atoul(const char *nptr);

/**
 * Portable version of strerror_r(), work around for the incompatible
 * return type of GNU and XSI variants.
 * \param[in]	errnum	Error number
 * \param[both]	buf	Buffer for error message
 * \param[in]	buflen	Size of message buffer
 * \return	Pointer to message buffer, returning error message or "Unknown error xxx" if none found
 */
SWITCH_DECLARE(char *) switch_strerror_r(int errnum, char *buf, switch_size_t buflen);
SWITCH_DECLARE(int) switch_wait_sock(switch_os_socket_t sock, uint32_t ms, switch_poll_t flags);
SWITCH_DECLARE(int) switch_wait_socklist(switch_waitlist_t *waitlist, uint32_t len, uint32_t ms);

typedef struct switch_http_request_s {
	const char *method;        /* GET POST PUT DELETE OPTIONS PATCH HEAD */
	const char *uri;
	const char *qs;            /* query string*/
	const char *host;
	switch_port_t port;
	const char *from;
	const char *user_agent;
	const char *referer;
	const char *user;
	switch_bool_t keepalive;
	const char *content_type;
	switch_size_t content_length;
	switch_size_t bytes_header;
	switch_size_t bytes_read;
	switch_size_t bytes_buffered;
	switch_event_t *headers;
	void *user_data;           /* private user data */

	/* private members used by the parser internally */
	char *_buffer;
	switch_bool_t _destroy_headers;
} switch_http_request_t;

/**
 * parse http headers in a buffer
 * return status of success or not
 * \param[in]	buffer the buffer start from the very begining of the http request, e.g. 'GET '
 * \param[in]	datalen the buffer length
 * \param[out]	the http request pointer or null, need destroy later if got non-NULL pointer
 * \return	SWITCH_STATUS_SUCCESS | SWITCH_STATUS_FALSE
 */
SWITCH_DECLARE(switch_status_t) switch_http_parse_header(char *buffer, uint32_t datalen, switch_http_request_t *request);
SWITCH_DECLARE(void) switch_http_free_request(switch_http_request_t *request);
SWITCH_DECLARE(void) switch_http_dump_request(switch_http_request_t *request);
/**
 * parse http query string
 * \param[in]	request the http request object
 * \param[in]	qs the query string buffer
 *
 * qs buffer will be modified, so be sure to dup the qs before passing into this function if you want to keep the original string untouched
 * if qs is NULL, the it will parse request->qs, request->qs will be duplicated before parse to avoid being modified
 */

SWITCH_DECLARE(void) switch_http_parse_qs(switch_http_request_t *request, char *qs);

SWITCH_DECLARE(switch_status_t) switch_frame_buffer_free(switch_frame_buffer_t *fb, switch_frame_t **frameP);
SWITCH_DECLARE(switch_status_t) switch_frame_buffer_dup(switch_frame_buffer_t *fb, switch_frame_t *orig, switch_frame_t **clone);
SWITCH_DECLARE(switch_status_t) switch_frame_buffer_destroy(switch_frame_buffer_t **fbP);
SWITCH_DECLARE(switch_status_t) switch_frame_buffer_create(switch_frame_buffer_t **fbP);

typedef struct {
	int64_t userms;
	int64_t kernelms;
} switch_cputime;
/**
/ Return used CPU time in this process for user and kernel code
**/
SWITCH_DECLARE(void) switch_getcputime(switch_cputime *t);

SWITCH_END_EXTERN_C
#endif
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
