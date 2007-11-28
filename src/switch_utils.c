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
 * switch_utils.c -- Compatability and Helper Code
 *
 */
#include <switch.h>
#ifndef WIN32
#include <arpa/inet.h>
#endif
#include "private/switch_core_pvt.h"


switch_size_t switch_fd_read_line(int fd, char *buf, switch_size_t len)
{
	char c, *p;
	int cur;
	switch_size_t total = 0;

	p = buf;
	while (total + sizeof(c) < len && (cur = read(fd, &c, sizeof(c))) > 0) {
		total += cur;
		*p++ = c;
		if (c == '\r' || c == '\n') {
			break;
		}
	}

	*p++ = '\0';
	return total;
}



static const char switch_b64_table[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
#define B64BUFFLEN 1024
SWITCH_DECLARE(switch_status_t) switch_b64_encode(unsigned char *in, switch_size_t ilen, unsigned char *out, switch_size_t olen)
{
    int y = 0, bytes = 0;
    size_t x = 0;
    unsigned int b = 0,l = 0;

    for(x = 0; x < ilen; x++) {
        b = (b<<8) + in[x];
        l += 8;
        while (l >= 6) {
            out[bytes++] = switch_b64_table[(b>>(l-=6))%64];
            if(++y != 72) {
                continue;
            }
            //out[bytes++] = '\n';
            y=0;
        }
    }

    if (l > 0) {
        out[bytes++] = switch_b64_table[((b%16)<<(6-l))%64];
    }
    if (l != 0) {
		while (l < 6) {
			out[bytes++] = '=', l += 2;
		}
	}

    return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status_t) switch_b64_decode(char *in, char *out, switch_size_t olen)
{

	char l64[256];
	int b = 0, c, l = 0, i;
	char *ip, *op = out;
	size_t ol = 0;

	for (i=0; i<256; i++) {
		l64[i] = -1;
	}

	for (i=0; i<64; i++) {
		l64[(int)switch_b64_table[i]] = i;
	}

	for (ip = in; ip && *ip; ip++) {
		c = l64[(int)*ip];
		if (c == -1) {
			continue;
		}

		b = (b << 6) + c;
		l += 6;

		while (l >= 8) {
			op[ol++] = (b >> (l -= 8)) % 256;
			if (ol >= olen -2) {
				goto end;
			}
		}
	}

 end:

	op[ol++] = '\0';

	return SWITCH_STATUS_SUCCESS;
}


static int write_buf(int fd, const char *buf)
{

	int len = (int) strlen(buf);
	if (fd && write(fd, buf, len) != len) {
		close(fd);
		return 0;
	}

	return 1;
}

SWITCH_DECLARE(switch_bool_t) switch_simple_email(const char *to, const char *from, const char *headers, const char *body, const char *file)
{
	char *bound = "XXXX_boundary_XXXX";
	const char *mime_type = "audio/inline";
	char filename[80], buf[B64BUFFLEN];
	int fd = 0, ifd = 0;
	int x = 0, y = 0, bytes = 0, ilen = 0;
	unsigned int b = 0, l = 0;
	unsigned char in[B64BUFFLEN];
	unsigned char out[B64BUFFLEN + 512];

    snprintf(filename, 80, "%smail.%d%04x", SWITCH_GLOBAL_dirs.temp_dir, (int)time(NULL), rand() & 0xffff);
    
    if ((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644))) {
        if (file) {
            if ((ifd = open(file, O_RDONLY)) < 1) {
                return SWITCH_FALSE;
            }

            snprintf(buf, B64BUFFLEN, "MIME-Version: 1.0\nContent-Type: multipart/mixed; boundary=\"%s\"\n", bound);
            if (!write_buf(fd, buf)) {
                return SWITCH_FALSE;
            }
        }

        if (headers && !write_buf(fd, headers))
            return SWITCH_FALSE;

        if (!write_buf(fd, "\n\n"))
            return SWITCH_FALSE;

        if (file) {
			if (body && switch_stristr("content-type", body)) {
				snprintf(buf, B64BUFFLEN, "--%s\n", bound);
			} else {
				snprintf(buf, B64BUFFLEN, "--%s\nContent-Type: text/plain\n\n", bound);
			}
            if (!write_buf(fd, buf))
                return SWITCH_FALSE;
        }

        if (body) {
            if (!write_buf(fd, body)) {
                return SWITCH_FALSE;
            }
        }

        if (file) {
			const char *filename = switch_cut_path(file);
			const char *new_type;
			char *ext;

			if ((ext = strrchr(filename, '.'))) {
				ext++;
				if ((new_type = switch_core_mime_ext2type(ext))) {
					mime_type = new_type;
				}
			}

			snprintf(buf, B64BUFFLEN,
					 "\n\n--%s\nContent-Type: %s; name=\"%s\"\n"
					 "Content-Transfer-Encoding: base64\n"
					 "Content-Description: Sound attachment.\n"
					 "Content-Disposition: attachment; filename=\"%s\"\n\n",
					 bound, mime_type, filename, filename);
            if (!write_buf(fd, buf))
                return SWITCH_FALSE;

            while ((ilen = read(ifd, in, B64BUFFLEN))) {
                for (x = 0; x < ilen; x++) {
                    b = (b << 8) + in[x];
                    l += 8;
                    while (l >= 6) {
                        out[bytes++] = switch_b64_table[(b >> (l -= 6)) % 64];
                        if (++y != 72)
                            continue;
                        out[bytes++] = '\n';
                        y = 0;
                    }
                }
                if (write(fd, &out, bytes) != bytes) {
                    return -1;
                } else
                    bytes = 0;

            }

            if (l > 0) {
                out[bytes++] = switch_b64_table[((b % 16) << (6 - l)) % 64];
            }
            if (l != 0)
                while (l < 6) {
                    out[bytes++] = '=', l += 2;
                }
            if (write(fd, &out, bytes) != bytes) {
                return -1;
            }

        }



        if (file) {
            snprintf(buf, B64BUFFLEN, "\n\n--%s--\n.\n", bound);
            if (!write_buf(fd, buf))
                return SWITCH_FALSE;
        }
    }

    if (fd) {
        close(fd);
    }
    if (ifd) {
        close(ifd);
    }
    snprintf(buf, B64BUFFLEN, "/bin/cat %s | %s %s %s", filename, runtime.mailer_app, runtime.mailer_app_args, to);
    if(system(buf)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to execute command: %s\n", buf);
    }

    unlink(filename);


    if (file) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Emailed file [%s] to [%s]\n", filename, to);
    } else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Emailed data to [%s]\n", to);
    }

    return SWITCH_TRUE;
}

SWITCH_DECLARE(switch_bool_t) switch_is_lan_addr(const char *ip)
{


	return (
			strncmp(ip, "10.", 3) &&
			strncmp(ip, "192.168.", 8) &&
			strncmp(ip, "127.", 4) &&
			strncmp(ip, "255.", 4) &&
			strncmp(ip, "0.", 2) &&
			strncmp(ip, "1.", 2) &&
			strncmp(ip, "2.", 2) &&
			strncmp(ip, "172.16.", 7) &&
			strncmp(ip, "172.17.", 7) &&
			strncmp(ip, "172.18.", 7) &&
			strncmp(ip, "172.19.", 7) &&
			strncmp(ip, "172.2", 5) &&
			strncmp(ip, "172.30.", 7) &&
			strncmp(ip, "172.31.", 7) &&
			strncmp(ip, "192.0.2.", 8) &&
			strncmp(ip, "169.254.", 8)
			) ? SWITCH_FALSE : SWITCH_TRUE;
	
}

SWITCH_DECLARE(switch_bool_t) switch_ast2regex(char *pat, char *rbuf, size_t len)
{
	char *p = pat;
	memset(rbuf, 0, len);
	
	*(rbuf + strlen(rbuf)) = '^';

	while(p && *p) {
		if (*p == 'N') {
			strncat(rbuf, "[2-9]", len - strlen(rbuf));
		} else if (*p == 'X') {
			strncat(rbuf, "[0-9]", len - strlen(rbuf));
		} else if (*p == 'Z') {
			strncat(rbuf, "[1-9]", len - strlen(rbuf));
		} else if (*p == '.') {
			strncat(rbuf, ".*", len - strlen(rbuf));
		} else if (strlen(rbuf) < len - 1) {
			*(rbuf + strlen(rbuf)) = *p;
		}
		p++;
	}
	*(rbuf + strlen(rbuf)) = '$';

	return strcmp(pat,rbuf) ? SWITCH_TRUE : SWITCH_FALSE;
}

SWITCH_DECLARE(char *) switch_replace_char(char *str, char from, char to, switch_bool_t dup)
{
	char *p;

	if (dup) {
		p = strdup(str);
		assert(p);
	} else {
		p = str;
	}

	for(;p && *p; p++) {
		if (*p == from) {
			*p = to;
		}
	}

	return p;
}

SWITCH_DECLARE(char *) switch_strip_spaces(const char *str)
{
	const char *sp = str;
	char *p, *s = NULL;
	
	while(sp && *sp && *sp == ' ') {
		sp++;
	}
	
	s = strdup(sp);

	p = s + (strlen(s) - 1);

	while(*p == ' ') {
		*p-- = '\0';
	}
	
	return s;
}


SWITCH_DECLARE(char *) switch_separate_paren_args(char *str)
{
	char *e, *args;
	switch_size_t br;

	if ((args = strchr(str, '('))) {
		e = args - 1;
		*args++ = '\0';
		while(*e == ' ') {
			*e-- = '\0';
		}
		e = args;
		br = 1;
		while(e && *e) {
			if (*e == '(') {
				br++;
			} else if (br > 1 && *e == ')') {
				br--;
			} else if (br == 1 && *e == ')') {
				*e = '\0';
				break;
			}
			e++;
		}
	}

	return args;
}

SWITCH_DECLARE(switch_bool_t) switch_is_number(const char *str)
{
	const char *p;
	switch_bool_t r = SWITCH_TRUE;

	for (p = str; p && *p; p++) {
		if (!(*p == '.' || (*p > 47 && *p < 58))) {
			r = SWITCH_FALSE;
			break;
		}
	}

	return r;
}


SWITCH_DECLARE(const char *) switch_stristr(const char *instr, const char *str)
{
/*
** Rev History:  16/07/97  Greg Thayer		Optimized
**               07/04/95  Bob Stout		ANSI-fy
**               02/03/94  Fred Cole		Original
**               09/01/03  Bob Stout		Bug fix (lines 40-41) per Fred Bulback
**
** Hereby donated to public domain.
*/

	const char *pptr, *sptr, *start;

	if (!str || !instr)
		return NULL;

	for (start = str; *start; start++) {
		/* find start of pattern in string */
		for ( ; ((*start) && (toupper(*start) != toupper(*instr))); start++);

		if (!*start)
			return NULL;

		pptr = instr;
		sptr = start;

		while (toupper(*sptr) == toupper(*pptr)) {
			sptr++;
			pptr++;

			/* if end of pattern then pattern was found */
			if (!*pptr)
				return (start);

			if (!*sptr)
				return NULL;
		}
	}
	return NULL;
}


SWITCH_DECLARE(switch_status_t) switch_find_local_ip(char *buf, int len, int family)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *base;

#ifdef WIN32
	SOCKET tmp_socket;
	SOCKADDR_STORAGE l_address;
	int l_address_len;
	struct addrinfo *address_info;
#else
#ifdef __Darwin__
	int ilen;
#else
	unsigned int ilen;
#endif
	int tmp_socket = -1, on = 1;
	char abuf[25] = "";
#endif

	switch_copy_string(buf, "127.0.0.1", len);

	switch (family) {
	case AF_INET:
		base = "82.45.148.209";
		break;
	case AF_INET6:
		base = "52.2d.94.d1";
		break;
	default:
		base = "127.0.0.1";
		break;
	}


#ifdef WIN32
	tmp_socket = socket(family, SOCK_DGRAM, 0);

	getaddrinfo(base, NULL, NULL, &address_info);

	if (!address_info || WSAIoctl(tmp_socket,
				 SIO_ROUTING_INTERFACE_QUERY,
				 address_info->ai_addr, (DWORD) address_info->ai_addrlen, &l_address, sizeof(l_address), (LPDWORD) & l_address_len, NULL, NULL)) {

		closesocket(tmp_socket);
		if (address_info)
			freeaddrinfo(address_info);
		return status;
	}

	closesocket(tmp_socket);
	freeaddrinfo(address_info);

	if (!getnameinfo((const struct sockaddr *) &l_address, l_address_len, buf, len, NULL, 0, NI_NUMERICHOST)) {

		status = SWITCH_STATUS_SUCCESS;

	}
#else

	switch (family) {
	case AF_INET:
		{
			struct sockaddr_in iface_out;
			struct sockaddr_in remote;
			memset(&remote, 0, sizeof(struct sockaddr_in));

			remote.sin_family = AF_INET;
			remote.sin_addr.s_addr = inet_addr(base);
			remote.sin_port = htons(4242);

			memset(&iface_out, 0, sizeof(iface_out));
			tmp_socket = socket(AF_INET, SOCK_DGRAM, 0);

			if (setsockopt(tmp_socket, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) == -1) {
				goto doh;
			}

			if (connect(tmp_socket, (struct sockaddr *) &remote, sizeof(struct sockaddr_in)) == -1) {
				goto doh;
			}

			ilen = sizeof(iface_out);
			if (getsockname(tmp_socket, (struct sockaddr *) &iface_out, &ilen) == -1) {
				goto doh;
			}

			if (iface_out.sin_addr.s_addr == 0) {
				goto doh;
			}

			switch_copy_string(buf, get_addr(abuf, sizeof(abuf), &iface_out.sin_addr), len);
			status = SWITCH_STATUS_SUCCESS;
		}
		break;
	case AF_INET6:
		{
			struct sockaddr_in6 iface_out;
			struct sockaddr_in6 remote;
			memset(&remote, 0, sizeof(struct sockaddr_in6));

			remote.sin6_family = AF_INET6;
			inet_pton(AF_INET6, buf, &remote.sin6_addr);
			remote.sin6_port = htons(4242);

			memset(&iface_out, 0, sizeof(iface_out));
			tmp_socket = socket(AF_INET6, SOCK_DGRAM, 0);

			if (setsockopt(tmp_socket, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) == -1) {
				goto doh;
			}

			if (connect(tmp_socket, (struct sockaddr *) &remote, sizeof(struct sockaddr_in)) == -1) {
				goto doh;
			}

			ilen = sizeof(iface_out);
			if (getsockname(tmp_socket, (struct sockaddr *) &iface_out, &ilen) == -1) {
				goto doh;
			}

			if (iface_out.sin6_addr.s6_addr == 0) {
				goto doh;
			}

			inet_ntop(AF_INET6, (const void *) &iface_out.sin6_addr, buf, len - 1);
			status = SWITCH_STATUS_SUCCESS;
		}
		break;
	}


  doh:
	if (tmp_socket > 0) {
		close(tmp_socket);
		tmp_socket = -1;
	}
#endif

	return status;

}


SWITCH_DECLARE(switch_time_t) switch_str_time(const char *in)
{
	switch_time_exp_t tm = { 0 };
	int proceed = 0, ovector[30];
	switch_regex_t *re = NULL;
	char replace[1024] = "";
	switch_time_t ret = 0;
	char *pattern = "^(\\d+)-(\\d+)-(\\d+)\\s*(\\d*):{0,1}(\\d*):{0,1}(\\d*)";

	switch_time_exp_lt(&tm, switch_time_now());
	tm.tm_year = tm.tm_mon = tm.tm_mday = tm.tm_hour = tm.tm_min = tm.tm_sec = 0;

	if ((proceed = switch_regex_perform(in, pattern, &re, ovector, sizeof(ovector) / sizeof(ovector[0])))) {

		if (proceed > 1) {
			switch_regex_copy_substring(in, ovector, proceed, 1, replace, sizeof(replace));
			tm.tm_year = atoi(replace) - 1900;
		}

		if (proceed > 2) {
			switch_regex_copy_substring(in, ovector, proceed, 2, replace, sizeof(replace));
			tm.tm_mon = atoi(replace) - 1;
		}

		if (proceed > 3) {
			switch_regex_copy_substring(in, ovector, proceed, 3, replace, sizeof(replace));
			tm.tm_mday = atoi(replace);
		}

		if (proceed > 4) {
			switch_regex_copy_substring(in, ovector, proceed, 4, replace, sizeof(replace));
			tm.tm_hour = atoi(replace);
		}

		if (proceed > 5) {
			switch_regex_copy_substring(in, ovector, proceed, 5, replace, sizeof(replace));
			tm.tm_min = atoi(replace);
		}

		if (proceed > 6) {
			switch_regex_copy_substring(in, ovector, proceed, 6, replace, sizeof(replace));
			tm.tm_sec = atoi(replace);
		}

		switch_time_exp_gmt_get(&ret, &tm);
		return ret;
	}
	/* possible else with more patterns later */
	return ret;

}

SWITCH_DECLARE(const char *) switch_priority_name(switch_priority_t priority)
{
	switch (priority) {			/*lol */
	case SWITCH_PRIORITY_NORMAL:
		return "NORMAL";
	case SWITCH_PRIORITY_LOW:
		return "LOW";
	case SWITCH_PRIORITY_HIGH:
		return "HIGH";
	default:
		return "INVALID";
	}
}

static char RFC2833_CHARS[] = "0123456789*#ABCDF";

SWITCH_DECLARE(char *) get_addr(char *buf, switch_size_t len, struct in_addr *in)
{
	uint8_t x, *i;
	char *p = buf;


	i = (uint8_t *) in;

	memset(buf, 0, len);
	for (x = 0; x < 4; x++) {
		sprintf(p, "%u%s", i[x], x == 3 ? "" : ".");
		p = buf + strlen(buf);
	}
	return buf;
}

SWITCH_DECLARE(char) switch_rfc2833_to_char(int event)
{
	if (event > -1 && event < (int32_t) sizeof(RFC2833_CHARS)) {
		return RFC2833_CHARS[event];
	}
	return '\0';
}

SWITCH_DECLARE(unsigned char) switch_char_to_rfc2833(char key)
{
	char *c;
	unsigned char counter = 0;

	key = (char) toupper(key);
	for (c = RFC2833_CHARS; *c; c++) {
		if (*c == key) {
			return counter;
		}
		counter++;
	}
	return '\0';
}

SWITCH_DECLARE(char *) switch_escape_char(switch_memory_pool_t *pool, char *in, const char *delim, char esc)
{
	char *data;
	const char *p, *d;
	int count = 1, i = 0;

	p = in;
	while (*p) {
		d = delim;
		while (*d) {
			if (*p == *d) {
				count++;
			}
			d++;
		}
		p++;
	}

	if (count == 1) {
		return in;
	}

	data = switch_core_alloc(pool, strlen(in) + count);

	p = in;
	while (*p) {
		d = delim;
		while (*d) {
			if (*p == *d) {
				data[i++] = esc;
			}
			d++;
		}
		data[i++] = *p;
		p++;
	}
	return data;
}


SWITCH_DECLARE(unsigned int) switch_separate_string(char *buf, char delim, char **array, int arraylen)
{
	int argc;
	char *ptr;
	int quot = 0;
	char qc = '\'';
	int x;

	if (!buf || !array || !arraylen) {
		return 0;
	}

	memset(array, 0, arraylen * sizeof(*array));

	ptr = buf;

	for (argc = 0; *ptr && (argc < arraylen - 1); argc++) {
		array[argc] = ptr;
		for (; *ptr; ptr++) {
			if (*ptr == qc) {
				if (quot) {
					quot--;
				} else {
					quot++;
				}
			} else if ((*ptr == delim) && !quot) {
				*ptr++ = '\0';
				break;
			}
		}
	}

	if (*ptr) {
		array[argc++] = ptr;
	}

	/* strip quotes and leading / trailing spaces */
	for (x = 0; x < argc; x++) {
		char *p;

		while(*(array[x]) == ' ') {
			(array[x])++;
		}
		p = array[x];
		while((p = strchr(array[x], qc))) {
			memmove(p, p+1, strlen(p));
			p++;
		}
		p = array[x] + (strlen(array[x]) - 1);
		while(*p == ' ') {
			*p-- = '\0';
		}
	}

	return argc;
}

SWITCH_DECLARE(const char *) switch_cut_path(const char *in)
{
	const char *p, *ret = in;
	const char delims[] = "/\\";
	const char *i;

	if (in) {
		for (i = delims; *i; i++) {
			p = in;
			while ((p = strchr(p, *i)) != 0) {
				ret = ++p;
			}
		}
		return ret;
	} else {
		return NULL;
	}
}


SWITCH_DECLARE(switch_status_t) switch_string_match(const char *string, size_t string_len, const char *search, size_t search_len)
{
	size_t i;

	for (i = 0; (i < search_len) && (i < string_len); i++) {
		if (string[i] != search[i]) {
			return SWITCH_STATUS_FALSE;
		}
	}

	if (i == search_len) {
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(char *) switch_string_replace(const char *string, const char *search, const char *replace)
{
	size_t string_len = strlen(string);
	size_t search_len = strlen(search);
	size_t replace_len = strlen(replace);
	size_t i, n;
	size_t dest_len = 0;
	char *dest;

	dest = (char *) malloc(sizeof(char));

	for (i = 0; i < string_len; i++) {
		if (switch_string_match(string + i, string_len - i, search, search_len) == SWITCH_STATUS_SUCCESS) {
			for (n = 0; n < replace_len; n++) {
				dest[dest_len] = replace[n];
				dest_len++;
				dest = (char *) realloc(dest, sizeof(char) * (dest_len + 1));
			}
			i += search_len - 1;
		} else {
			dest[dest_len] = string[i];
			dest_len++;
			dest = (char *) realloc(dest, sizeof(char) * (dest_len + 1));
		}
	}

	dest[dest_len] = 0;
	return dest;
}

SWITCH_DECLARE(int) switch_socket_waitfor(switch_pollfd_t * poll, int ms)
{
	int nsds = 0;

	switch_poll(poll, 1, &nsds, ms);

	return nsds;
}


SWITCH_DECLARE(size_t) switch_url_encode(const char *url, char *buf, size_t len)
{
	const char *p;
	size_t x = 0;
	const char urlunsafe[] = "\r\n \"#%&+:;<=>?@[\\]^`{|}";
	const char hex[] = "0123456789ABCDEF";

	if (!buf) {
		return 0;
	}

	memset(buf, 0, len);

	if (!url) {
		return 0;
	}

	for (p = url; *p; p++) {
		if (*p < ' ' || *p > '~' || strchr(urlunsafe, *p)) {
			if ((x + 3) > len) {
				break;
			}
			buf[x++] = '%';
			buf[x++] = hex[*p >> 4];
			buf[x++] = hex[*p & 0x0f];
		} else {
			buf[x++] = *p;
		}
		if (x == len) {
			break;
		}
	}
	return x;
}

SWITCH_DECLARE(char *) switch_url_decode(char *s)
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


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
