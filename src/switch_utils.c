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

	if (WSAIoctl(tmp_socket,
				 SIO_ROUTING_INTERFACE_QUERY,
				 address_info->ai_addr,
				 (DWORD) address_info->ai_addrlen,
				 &l_address, sizeof(l_address), (LPDWORD) & l_address_len, NULL, NULL)) {

		closesocket(tmp_socket);
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


SWITCH_DECLARE(switch_time_t) switch_str_time(char *in)
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

SWITCH_DECLARE(char *) switch_priority_name(switch_priority_t priority)
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

SWITCH_DECLARE(char *) switch_escape_char(switch_memory_pool_t *pool, char *in, char *delim, char esc)
{
	char *data, *p, *d;
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
	char qc = '"';
	char *e;
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

	/* strip quotes */
	for (x = 0; x < argc; x++) {
		if (*(array[x]) == qc) {
			(array[x])++;
			if ((e = strchr(array[x], qc))) {
				*e = '\0';
			}
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


SWITCH_DECLARE(switch_status_t) switch_string_match(const char *string, size_t string_len, const char *search,
													size_t search_len)
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

SWITCH_DECLARE(int) switch_socket_waitfor(switch_pollfd_t *poll, int ms)
{
	int nsds = 0;

	switch_poll(poll, 1, &nsds, ms);

	return nsds;
}


SWITCH_DECLARE(size_t) switch_url_encode(char *url, char *buf, size_t len)
{
	char *p;
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
