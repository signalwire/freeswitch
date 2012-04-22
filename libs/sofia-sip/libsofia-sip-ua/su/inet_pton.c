/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"

#include <sys/types.h>
#include <string.h>
#include <errno.h>

#include "sofia-sip/su.h"

/*
 * WARNING: Don't even consider trying to compile this on a system where
 * sizeof(int) < 4.  sizeof(int) > 4 is fine; all the world's not a VAX.
 */

static int	inet_pton4(const char *src, unsigned char *dst);
#if HAVE_SIN6
static int	inet_pton6(const char *src, unsigned char *dst);
#endif

/* int
 * inet_pton(af, src, dst)
 *	convert from presentation format (which usually means ASCII printable)
 *	to network format (which is usually some kind of binary format).
 * return:
 *	1 if the address was valid for the specified address family
 *	0 if the address wasn't valid (`dst' is untouched in this case)
 *	-1 if some other error occurred (`dst' is untouched in this case, too)
 * author:
 *	Paul Vixie, 1996.
 */

/** inet_pton() replacement.
 *
 * Convert from presentation format in @a src (which usually means ASCII printable)
 * to network format in @a dst (which is usually some kind of binary format).
 *
 * @param af[in] address family
 * @param src[in] string containing address to convert
 * @param dst[out] return-value network address
 *                 (struct in_addr or struct in_addr6)
 *
 * @retval 1 if the address was valid for the specified address family
 * @retval 0 if the address wasn't valid (`dst' is untouched in this case)
 * @retval -1 if some other error occurred (`dst' is untouched in this case, too)
 *
 * @author Paul Vixie, 1996.
 *
 * @NEW_1_12_9
 */
int
su_inet_pton(int af, const char * src, void * dst)
{
	switch (af) {
	case AF_INET:
		return (inet_pton4(src, dst));
#if HAVE_SIN6
	case AF_INET6:
		return (inet_pton6(src, dst));
#endif
	default:
		su_seterrno(EAFNOSUPPORT);
		return (-1);
	}
	/* NOTREACHED */
}

/* int
 * inet_pton4(src, dst)
 *	like inet_aton() but without all the hexadecimal and shorthand.
 * return:
 *	1 if `src' is a valid dotted quad, else 0.
 * notice:
 *	does not touch `dst' unless it's returning 1.
 * author:
 *	Paul Vixie, 1996.
 */
static int
inet_pton4(const char *src, unsigned char *dst)
{
	int saw_digit, octets, ch;
	unsigned char tmp[4], *tp;

	saw_digit = 0;
	octets = 0;
	*(tp = tmp) = 0;
	while ((ch = *src++) != '\0') {
		if ('0' <= ch && ch <= '9') {
			unsigned octet = *tp * 10 + ch - '0';

			if (saw_digit && *tp == 0)
				return (0);
			if (octet > 255)
				return (0);
			*tp = octet;
			if (!saw_digit) {
				if (++octets > 4)
					return (0);
				saw_digit = 1;
			}
		} else if (ch == '.' && saw_digit) {
			if (octets == 4)
				return (0);
			*++tp = 0;
			saw_digit = 0;
		} else
			return (0);
	}
	if (octets < 4)
		return (0);
	memcpy(dst, tmp, 4);
	return (1);
}

#if HAVE_SIN6

/* int
 * inet_pton6(src, dst)
 *	convert presentation level address to network order binary form.
 * return:
 *	1 if `src' is a valid [RFC1884 2.2] address, else 0.
 * notice:
 *	(1) does not touch `dst' unless it's returning 1.
 *	(2) :: in a full address is silently ignored.
 * credit:
 *	inspired by Mark Andrews.
 * author:
 *	Paul Vixie, 1996.
 */
static int
inet_pton6(const char *src, unsigned char *dst)
{
	uint8_t tmp[16], *tp, *endp, *colonp;
	const char *curtok;
	int ch, saw_xdigit;
	unsigned val;

	memset((tp = tmp), '\0', sizeof tmp);
	endp = tp + sizeof tmp;
	colonp = NULL;
	/* Leading :: requires some special handling. */
	if (*src == ':')
		if (*++src != ':')
			return (0);
	curtok = src;
	saw_xdigit = 0;
	val = 0;
	while ((ch = *src++) != '\0') {
		if (ch == ':') {
			curtok = src;
			if (!saw_xdigit) {
				if (colonp)
					return (0);
				colonp = tp;
				continue;
			} else if (*src == '\0') {
				return (0);
			}
			if (tp + 2 > endp)
				return (0);
			*tp++ = (unsigned char) (val >> 8) & 0xff;
			*tp++ = (unsigned char) val & 0xff;
			saw_xdigit = 0;
			val = 0;
			continue;
		}
		if (ch == '.' && ((tp + 4) <= endp) &&
		    inet_pton4(curtok, tp) > 0) {
			tp += 4;
			saw_xdigit = 0;
			break;	/* '\0' was seen by inet_pton4(). */
		}

		if ('0' <= ch && ch <= '9')
			ch = ch - '0';
		else if ('A' <= ch && ch <= 'F')
			ch = ch - 'A' + 10;
		else if ('a' <= ch && ch <= 'f')
			ch = ch - 'a' + 10;
		else
			return (0);
		val <<= 4;
		val |= ch;
		if (val > 0xffff)
			return (0);
		saw_xdigit = 1;
	}
	if (saw_xdigit) {
		if (tp + 2 > endp)
			return (0);
		*tp++ = (unsigned char) (val >> 8) & 0xff;
		*tp++ = (unsigned char) val & 0xff;
	}
	if (colonp != NULL) {
		/*
		 * Since some memmove()'s erroneously fail to handle
		 * overlapping regions, we'll do the shift by hand.
		 */
		const int n = tp - colonp;
		int i;

		if (tp == endp)
			return (0);
		for (i = 1; i <= n; i++) {
			endp[- i] = colonp[n - i];
			colonp[n - i] = 0;
		}
		tp = endp;
	}
	if (tp != endp)
		return 0;
	memcpy(dst, tmp, sizeof tmp);
	return 1;
}

#endif
