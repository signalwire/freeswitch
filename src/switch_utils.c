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
 * Juan Jose Comellas <juanjo@comellas.org>
 *
 *
 * switch_utils.c -- Compatibility and Helper Code
 *
 */

#include <switch.h>
#ifndef WIN32
#include <arpa/inet.h>
#endif
#include "private/switch_core_pvt.h"
#define ESCAPE_META '\\'

struct switch_network_node {
	uint32_t ip;
	uint32_t mask;
	uint32_t bits;
	switch_bool_t ok;
	char *token;
	struct switch_network_node *next;
};
typedef struct switch_network_node switch_network_node_t;

struct switch_network_list {
	struct switch_network_node *node_head;
	switch_bool_t default_type;
	switch_memory_pool_t *pool;
};

#ifndef WIN32
int switch_inet_pton(int af, const char *src, void *dst)
{
	return inet_pton(af, src, dst);
}
#endif

SWITCH_DECLARE(switch_status_t) switch_network_list_create(switch_network_list_t **list, switch_bool_t default_type, switch_memory_pool_t *pool)
{
	switch_network_list_t *new_list;

	if (!pool) {
		switch_core_new_memory_pool(&pool);
	}

	new_list = switch_core_alloc(pool, sizeof(**list));
	new_list->pool = pool;
	new_list->default_type = default_type;

	*list = new_list;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_bool_t) switch_network_list_validate_ip_token(switch_network_list_t *list, uint32_t ip, const char **token)
{
	switch_network_node_t *node;
	switch_bool_t ok = list->default_type;
	uint32_t bits = 0;

	for (node = list->node_head; node; node = node->next) {
		if (node->bits > bits && switch_test_subnet(ip, node->ip, node->mask)) {
			if (node->ok) {
				ok = SWITCH_TRUE;
			} else {
				ok = SWITCH_FALSE;
			}

			bits = node->bits;

			if (token) {
				*token = node->token;
			}
		}
	}

	return ok;
}

SWITCH_DECLARE(switch_status_t) switch_network_list_add_cidr_token(switch_network_list_t *list, const char *cidr_str, switch_bool_t ok, const char *token)
{
	uint32_t ip, mask, bits;
	switch_network_node_t *node;

	if (switch_parse_cidr(cidr_str, &ip, &mask, &bits)) {
		return SWITCH_STATUS_GENERR;
	}

	node = switch_core_alloc(list->pool, sizeof(*node));

	node->ip = ip;
	node->mask = mask;
	node->ok = ok;
	node->bits = bits;

	if (!switch_strlen_zero(token)) {
		node->token = switch_core_strdup(list->pool, token);
	}
	
	node->next = list->node_head;
	list->node_head = node;

	return SWITCH_STATUS_SUCCESS;	
}

SWITCH_DECLARE(switch_status_t) switch_network_list_add_host_mask(switch_network_list_t *list, const char *host, const char *mask_str, switch_bool_t ok)
{
	int ip, mask;
	switch_network_node_t *node;

	switch_inet_pton(AF_INET, host, &ip);
	switch_inet_pton(AF_INET, mask_str, &mask);

	node = switch_core_alloc(list->pool, sizeof(*node));

	node->ip = ip;
	node->mask = mask;
	node->ok = ok;

	/* http://graphics.stanford.edu/~seander/bithacks.html */
	mask = mask - ((mask >> 1) & 0x55555555);
	mask = (mask & 0x33333333) + ((mask >> 2) & 0x33333333);
	node->bits = (((mask + (mask >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;

	node->next = list->node_head;
	list->node_head = node;

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(int) switch_parse_cidr(const char *string, uint32_t *ip, uint32_t *mask, uint32_t *bitp)
{
	char host[128];
	char *bit_str;
	int32_t bits;

	switch_copy_string(host, string, sizeof(host));
	bit_str = strchr(host, '/');

	if (!bit_str) {
		return -1;
	}

	*bit_str++ = '\0';
	bits = atoi(bit_str);

	if (bits < 0 || bits > 32) {
		return -2;
	}

	bits = atoi(bit_str);
	switch_inet_pton(AF_INET, host, ip);
	*ip = htonl(*ip);

	*mask = 0xFFFFFFFF & ~(0xFFFFFFFF >> bits);

	*bitp = bits;

	return 0;
}


SWITCH_DECLARE(char *) switch_find_end_paren(const char *s, char open, char close)
{
	const char *e = NULL;
	int depth = 0;

	while (s && *s && *s == ' ') {
		s++;
	}

	if (s && *s == open) {
		depth++;
		for (e = s + 1; e && *e; e++) {
			if (*e == open) {
				depth++;
			} else if (*e == close) {
				depth--;
				if (!depth) {
					break;
				}
			}
		}
	}

	return (char *) e;
}

SWITCH_DECLARE(switch_size_t) switch_fd_read_line(int fd, char *buf, switch_size_t len)
{
	char c, *p;
	int cur;
	switch_size_t total = 0;

	p = buf;
	while (total + 2 < len && (cur = read(fd, &c, 1)) == 1) {
		total += cur;
		*p++ = c;
		if (c == '\r' || c == '\n') {
			break;
		}
	}

	*p++ = '\0';
	assert(total < len);
	return total;
}

SWITCH_DECLARE(char *) switch_amp_encode(char *s, char *buf, switch_size_t len)
{
	char *p, *q;
	switch_size_t x = 0;
	switch_assert(s);

	q = buf;

	for (p = s; x < len; p++) {
		switch (*p) {
		case '<':
			if (x + 4 > len - 1) {
				goto end;
			}
			*q++ = '&';
			*q++ = 'l';
			*q++ = 't';
			*q++ = ';';
			x += 4;
			break;
		case '>':
			if (x + 4 > len - 1) {
				goto end;
			}
			*q++ = '&';
			*q++ = 'g';
			*q++ = 't';
			*q++ = ';';
			x += 4;
			break;
		default:
			if (x + 1 > len - 1) {
				goto end;
			}
			*q++ = *p;
			x++;
			if (*p == '\0') {
				goto end;
			}
			break;
		}
	}

  end:

	return buf;
}

static const char switch_b64_table[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
#define B64BUFFLEN 1024
SWITCH_DECLARE(switch_status_t) switch_b64_encode(unsigned char *in, switch_size_t ilen, unsigned char *out, switch_size_t olen)
{
	int y = 0, bytes = 0;
	size_t x = 0;
	unsigned int b = 0, l = 0;

	for (x = 0; x < ilen; x++) {
		b = (b << 8) + in[x];
		l += 8;
		while (l >= 6) {
			out[bytes++] = switch_b64_table[(b >> (l -= 6)) % 64];
			if (++y != 72) {
				continue;
			}
			//out[bytes++] = '\n';
			y = 0;
		}
	}

	if (l > 0) {
		out[bytes++] = switch_b64_table[((b % 16) << (6 - l)) % 64];
	}
	if (l != 0) {
		while (l < 6) {
			out[bytes++] = '=', l += 2;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_size_t) switch_b64_decode(char *in, char *out, switch_size_t olen)
{

	char l64[256];
	int b = 0, c, l = 0, i;
	char *ip, *op = out;
	size_t ol = 0;

	for (i = 0; i < 256; i++) {
		l64[i] = -1;
	}

	for (i = 0; i < 64; i++) {
		l64[(int) switch_b64_table[i]] = (char) i;
	}

	for (ip = in; ip && *ip; ip++) {
		c = l64[(int) *ip];
		if (c == -1) {
			continue;
		}

		b = (b << 6) + c;
		l += 6;

		while (l >= 8) {
			op[ol++] = (char) ((b >> (l -= 8)) % 256);
			if (ol >= olen - 2) {
				goto end;
			}
		}
	}

  end:

	op[ol++] = '\0';

	return ol;
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

	switch_snprintf(filename, 80, "%smail.%d%04x", SWITCH_GLOBAL_dirs.temp_dir, (int) switch_timestamp(NULL), rand() & 0xffff);

	if ((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644))) {
		if (file) {
			if ((ifd = open(file, O_RDONLY | O_BINARY)) < 1) {
				return SWITCH_FALSE;
			}
		}
		switch_snprintf(buf, B64BUFFLEN, "MIME-Version: 1.0\nContent-Type: multipart/mixed; boundary=\"%s\"\n", bound);
		if (!write_buf(fd, buf)) {
			return SWITCH_FALSE;
		}

		if (headers && !write_buf(fd, headers))
			return SWITCH_FALSE;

		if (!write_buf(fd, "\n\n"))
			return SWITCH_FALSE;

		if (body && switch_stristr("content-type", body)) {
			switch_snprintf(buf, B64BUFFLEN, "--%s\n", bound);
		} else {
			switch_snprintf(buf, B64BUFFLEN, "--%s\nContent-Type: text/plain\n\n", bound);
		}
		if (!write_buf(fd, buf))
			return SWITCH_FALSE;

		if (body) {
			if (!write_buf(fd, body)) {
				return SWITCH_FALSE;
			}
		}

		if (file) {
			const char *stipped_file = switch_cut_path(file);
			const char *new_type;
			char *ext;

			if ((ext = strrchr(stipped_file, '.'))) {
				ext++;
				if ((new_type = switch_core_mime_ext2type(ext))) {
					mime_type = new_type;
				}
			}

			switch_snprintf(buf, B64BUFFLEN,
							"\n\n--%s\nContent-Type: %s; name=\"%s\"\n"
							"Content-ID: <ATTACHED@freeswitch.org>\n"
							"Content-Transfer-Encoding: base64\n"
							"Content-Description: Sound attachment.\n"
							"Content-Disposition: attachment; filename=\"%s\"\n\n", bound, mime_type, stipped_file, stipped_file);
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

		switch_snprintf(buf, B64BUFFLEN, "\n\n--%s--\n.\n", bound);
		if (!write_buf(fd, buf))
			return SWITCH_FALSE;
	}

	if (fd) {
		close(fd);
	}
	if (ifd) {
		close(ifd);
	}
#ifdef WIN32
	switch_snprintf(buf, B64BUFFLEN, "type %s | %s %s %s", filename, runtime.mailer_app, runtime.mailer_app_args, to);
#else
	switch_snprintf(buf, B64BUFFLEN, "/bin/cat %s | %s %s %s", filename, runtime.mailer_app, runtime.mailer_app_args, to);
#endif
	if (switch_system(buf, SWITCH_TRUE) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to execute command: %s\n", buf);
	}

	if (unlink(filename) != 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to delete file [%s]\n", filename);
	}

	if (file) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Emailed file [%s] to [%s]\n", filename, to);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Emailed data to [%s]\n", to);
	}

	return SWITCH_TRUE;
}

SWITCH_DECLARE(switch_bool_t) switch_is_lan_addr(const char *ip)
{
	if (switch_strlen_zero(ip))
		return SWITCH_FALSE;

	return (strncmp(ip, "10.", 3) &&
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
			strncmp(ip, "172.2", 5) && strncmp(ip, "172.30.", 7) && strncmp(ip, "172.31.", 7) && strncmp(ip, "192.0.2.", 8) && strncmp(ip, "169.254.", 8)
		)? SWITCH_FALSE : SWITCH_TRUE;
}

SWITCH_DECLARE(switch_bool_t) switch_ast2regex(char *pat, char *rbuf, size_t len)
{
	char *p = pat;

	if (!pat) {
		return SWITCH_FALSE;
	}

	memset(rbuf, 0, len);

	*(rbuf + strlen(rbuf)) = '^';

	while (p && *p) {
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

	return strcmp(pat, rbuf) ? SWITCH_TRUE : SWITCH_FALSE;
}

SWITCH_DECLARE(char *) switch_replace_char(char *str, char from, char to, switch_bool_t dup)
{
	char *p;

	if (dup) {
		p = strdup(str);
		switch_assert(p);
	} else {
		p = str;
	}

	for (; p && *p; p++) {
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

	if (!sp)
		return NULL;

	while (*sp == ' ') {
		sp++;
	}

	s = strdup(sp);

	if (!s)
		return NULL;

	p = s + (strlen(s) - 1);

	while (*p == ' ') {
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
		while (*e == ' ') {
			*e-- = '\0';
		}
		e = args;
		br = 1;
		while (e && *e) {
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
		for (; ((*start) && (switch_toupper(*start) != switch_toupper(*instr))); start++);

		if (!*start)
			return NULL;

		pptr = instr;
		sptr = start;

		while (switch_toupper(*sptr) == switch_toupper(*pptr)) {
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

	if (len < 16) {
		return status;
	}

	
	switch (family) {
	case AF_INET:
		switch_copy_string(buf, "127.0.0.1", len);
		base = "82.45.148.209";
		break;
	case AF_INET6:
		switch_copy_string(buf, "::1", len);
		base = "2001:503:BA3E::2:30"; // DNS Root server A 
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
								  address_info->ai_addr, (DWORD) address_info->ai_addrlen, &l_address, sizeof(l_address), (LPDWORD) & l_address_len, NULL,
								  NULL)) {

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

			switch_copy_string(buf, get_addr(abuf, sizeof(abuf), (struct sockaddr*)&iface_out, sizeof(iface_out)), len);
			status = SWITCH_STATUS_SUCCESS;
		}
		break;
	case AF_INET6:
		{
			struct sockaddr_in6 iface_out;
			struct sockaddr_in6 remote;
			memset(&remote, 0, sizeof(struct sockaddr_in6));

			remote.sin6_family = AF_INET6;
			switch_inet_pton(AF_INET6, base, &remote.sin6_addr);
			remote.sin6_port = htons(4242);

			memset(&iface_out, 0, sizeof(iface_out));
			tmp_socket = socket(AF_INET6, SOCK_DGRAM, 0);

			if (connect(tmp_socket, (struct sockaddr *) &remote, sizeof(remote)) == -1) {
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

	switch_time_exp_lt(&tm, switch_timestamp_now());
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

#ifndef _MSC_VER
#define switch_inet_ntop inet_ntop
#else
/* Copyright (c) 1996 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>

/*
 * WARNING: Don't even consider trying to compile this on a system where
 * sizeof(int) < 4.  sizeof(int) > 4 is fine; all the world's not a VAX.
 */

static const char *switch_inet_ntop4(const unsigned char *src, char *dst, size_t size);
#if HAVE_SIN6
static const char *switch_inet_ntop6(const unsigned char *src, char *dst, size_t size);
#endif

/* char *
 * inet_ntop(af, src, dst, size)
 *	convert a network format address to presentation format.
 * return:
 *	pointer to presentation format address (`dst'), or NULL (see errno).
 * author:
 *	Paul Vixie, 1996.
 */
const char *switch_inet_ntop(int af, void const *src, char *dst, size_t size)
{

	switch (af) {
	case AF_INET:
		return switch_inet_ntop4(src, dst, size);
#if HAVE_SIN6
	case AF_INET6:
		return switch_inet_ntop6(src, dst, size);
#endif
	default:
		return NULL;
	}
	/* NOTREACHED */
}

/* const char *
 * inet_ntop4(src, dst, size)
 *	format an IPv4 address, more or less like inet_ntoa()
 * return:
 *	`dst' (as a const)
 * notes:
 *	(1) uses no statics
 *	(2) takes a unsigned char* not an in_addr as input
 * author:
 *	Paul Vixie, 1996.
 */
static const char *switch_inet_ntop4(const unsigned char *src, char *dst, size_t size)
{
	static const char fmt[] = "%u.%u.%u.%u";
	char tmp[sizeof "255.255.255.255"];

	if (switch_snprintf(tmp, sizeof tmp, fmt, src[0], src[1], src[2], src[3]) >= (int) size) {
		return NULL;
	}

	return strcpy(dst, tmp);
}

#if HAVE_SIN6
/* const char *
 * inet_ntop6(src, dst, size)
 *	convert IPv6 binary address into presentation (printable) format
 * author:
 *	Paul Vixie, 1996.
 */
static const char *switch_inet_ntop6(unsigned char const *src, char *dst, size_t size)
{
	/*
	 * Note that int32_t and int16_t need only be "at least" large enough
	 * to contain a value of the specified size.  On some systems, like
	 * Crays, there is no such thing as an integer variable with 16 bits.
	 * Keep this in mind if you think this function should have been coded
	 * to use pointer overlays.  All the world's not a VAX.
	 */
	char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"], *tp;
	struct {
		int base, len;
	} best = {
	-1, 0}, cur = {
	-1, 0};
	unsigned int words[8];
	int i;

	/*
	 * Preprocess:
	 *  Copy the input (bytewise) array into a wordwise array.
	 *  Find the longest run of 0x00's in src[] for :: shorthanding.
	 */
	for (i = 0; i < 16; i += 2)
		words[i / 2] = (src[i] << 8) | (src[i + 1]);
	best.base = -1;
	cur.base = -1;
	for (i = 0; i < 8; i++) {
		if (words[i] == 0) {
			if (cur.base == -1)
				cur.base = i, cur.len = 1;
			else
				cur.len++;
		} else {
			if (cur.base != -1) {
				if (best.base == -1 || cur.len > best.len)
					best = cur;
				cur.base = -1;
			}
		}
	}
	if (cur.base != -1) {
		if (best.base == -1 || cur.len > best.len)
			best = cur;
	}
	if (best.base != -1 && best.len < 2)
		best.base = -1;

	/*
	 * Format the result.
	 */
	tp = tmp;
	for (i = 0; i < 8; i++) {
		/* Are we inside the best run of 0x00's? */
		if (best.base != -1 && i >= best.base && i < (best.base + best.len)) {
			if (i == best.base)
				*tp++ = ':';
			continue;
		}
		/* Are we following an initial run of 0x00s or any real hex? */
		if (i != 0)
			*tp++ = ':';
		/* Is this address an encapsulated IPv4? */
		if (i == 6 && best.base == 0 && (best.len == 6 || (best.len == 5 && words[5] == 0xffff))) {
			if (!inet_ntop4(src + 12, tp, sizeof tmp - (tp - tmp)))
				return (NULL);
			tp += strlen(tp);
			break;
		}
		tp += sprintf(tp, "%x", words[i]);
	}
	/* Was it a trailing run of 0x00's? */
	if (best.base != -1 && (best.base + best.len) == 8)
		*tp++ = ':';
	*tp++ = '\0';

	/*
	 * Check for overflow, copy, and we're done.
	 */
	if ((size_t) (tp - tmp) >= size) {
		return NULL;
	}

	return strcpy(dst, tmp);
}
#endif

#endif

SWITCH_DECLARE(char *) get_addr(char *buf, switch_size_t len, struct sockaddr *sa, socklen_t salen)
{
	switch_assert(buf);
	*buf = '\0';
	if (sa) {
		getnameinfo(sa, salen, buf, (socklen_t)len, NULL, 0, NI_NUMERICHOST);
	}
	return buf;
}

SWITCH_DECLARE(unsigned short) get_port(struct sockaddr *sa)
{
	unsigned short port = 0;
	if (sa) {
		switch (sa->sa_family) {
			case AF_INET:
				port = ntohs(((struct sockaddr_in*)sa)->sin_port);
				break;
			case AF_INET6:
				port = ntohs(((struct sockaddr_in6*)sa)->sin6_port);
				break;
		}
	}
	return port;
}

SWITCH_DECLARE(int) switch_build_uri(char *uri,
									 switch_size_t size,
									 const char *scheme,
									 const char *user,
									 const switch_sockaddr_t *sa,
									 int flags)
{
	char host[NI_MAXHOST], serv[NI_MAXSERV];
	struct sockaddr_storage ss;
	const struct sockaddr *addr;
	const char *colon;

	if (flags & SWITCH_URI_NO_SCOPE && sa->family == AF_INET6) {
		memcpy(&ss, &sa->sa, sa->salen);
		((struct sockaddr_in6*) &ss)->sin6_scope_id = 0;
		addr = (const struct sockaddr*) &ss;
	} else {
		addr = (const struct sockaddr*) &sa->sa;
	}

	if (getnameinfo(addr, sa->salen, host, sizeof(host), serv, sizeof(serv),
		((flags & SWITCH_URI_NUMERIC_HOST) ? NI_NUMERICHOST : 0) | ((flags & SWITCH_URI_NUMERIC_PORT) ? NI_NUMERICSERV : 0)) != 0) {
			return 0;
	}

	colon = strchr(host, ':');

	return switch_snprintf(uri, size, "%s:%s%s%s%s%s%s%s", scheme,
		user ? user : "", user ? "@" : "",
		colon ? "[" : "", host, colon ? "]" : "",
		serv[0] ? ":" : "", serv[0] ? serv : "");
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

	key = (char) switch_toupper(key);
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

/* Helper function used when separating strings to unescape a character. The
   supported characters are:

   \n  linefeed
   \r  carriage return
   \t  tab
   \s  space

   Any other character is returned as it was received. */
static char unescape_char(char escaped)
{
	char unescaped;

	switch (escaped) {
	case 'n':
		unescaped = '\n';
		break;
	case 'r':
		unescaped = '\r';
		break;
	case 't':
		unescaped = '\t';
		break;
	case 's':
		unescaped = ' ';
		break;
	default:
		unescaped = escaped;
	}
	return unescaped;
}

/* Helper function used when separating strings to remove quotes, leading /
   trailing spaces, and to convert escaped characters. */
static char *cleanup_separated_string(char *str, char delim)
{
	char *ptr;
	char *dest;
	char *start;
	char *end = NULL;
	int inside_quotes = 0;

	/* Skip initial whitespace */
	for (ptr = str; *ptr == ' '; ++ptr) {
	}

	for (start = dest = ptr; *ptr; ++ptr) {
		char e;
		int esc = 0;

		if (*ptr == ESCAPE_META) {
			e = *(ptr + 1);
			if (e == '\'' || e == '"' || (delim && e == delim) || (e = unescape_char(*(ptr + 1))) != *(ptr + 1)) {
				++ptr;
				*dest++ = e;
				end = dest;
				esc++;
			}
		}
		if (!esc) {
			if (*ptr == '\'') {
				inside_quotes = (1 - inside_quotes);
			} else {
				*dest++ = *ptr;
				if (*ptr != ' ' || inside_quotes) {
					end = dest;
				}
			}
		}
	}
	if (end) {
		*end = '\0';
	}
	return start;
}

/* Separate a string using a delimiter that is not a space */
static unsigned int separate_string_char_delim(char *buf, char delim, char **array, unsigned int arraylen)
{
	enum tokenizer_state {
		START,
		FIND_DELIM
	} state = START;

	unsigned int count = 0;
	char *ptr = buf;
	int inside_quotes = 0;
	unsigned int i;

	while (*ptr && count < arraylen) {
		switch (state) {
		case START:
			array[count++] = ptr;
			state = FIND_DELIM;
			break;

		case FIND_DELIM:
			/* escaped characters are copied verbatim to the destination string */
			if (*ptr == ESCAPE_META) {
				++ptr;
			} else if (*ptr == '\'') {
				inside_quotes = (1 - inside_quotes);
			} else if (*ptr == delim && !inside_quotes) {
				*ptr = '\0';
				state = START;
			}
			++ptr;
			break;
		}
	}
	/* strip quotes, escaped chars and leading / trailing spaces */
	for (i = 0; i < count; ++i) {
		array[i] = cleanup_separated_string(array[i], delim);
	}
	return count;
}

/* Separate a string using a delimiter that is a space */
static unsigned int separate_string_blank_delim(char *buf, char **array, unsigned int arraylen)
{
	enum tokenizer_state {
		START,
		SKIP_INITIAL_SPACE,
		FIND_DELIM,
		SKIP_ENDING_SPACE
	} state = START;

	unsigned int count = 0;
	char *ptr = buf;
	int inside_quotes = 0;
	unsigned int i;

	while (*ptr && count < arraylen) {
		switch (state) {
		case START:
			array[count++] = ptr;
			state = SKIP_INITIAL_SPACE;
			break;

		case SKIP_INITIAL_SPACE:
			if (*ptr == ' ') {
				++ptr;
			} else {
				state = FIND_DELIM;
			}
			break;

		case FIND_DELIM:
			if (*ptr == ESCAPE_META) {
				++ptr;
			} else if (*ptr == '\'') {
				inside_quotes = (1 - inside_quotes);
			} else if (*ptr == ' ' && !inside_quotes) {
				*ptr = '\0';
				state = SKIP_ENDING_SPACE;
			}
			++ptr;
			break;

		case SKIP_ENDING_SPACE:
			if (*ptr == ' ') {
				++ptr;
			} else {
				state = START;
			}
			break;
		}
	}
	/* strip quotes, escaped chars and leading / trailing spaces */
	for (i = 0; i < count; ++i) {
		array[i] = cleanup_separated_string(array[i], 0);
	}
	return count;
}

SWITCH_DECLARE(unsigned int) switch_separate_string(char *buf, char delim, char **array, unsigned int arraylen)
{
	if (!buf || !array || !arraylen) {
		return 0;
	}

	memset(array, 0, arraylen * sizeof(*array));

	return (delim == ' ' ? separate_string_blank_delim(buf, array, arraylen) : separate_string_char_delim(buf, delim, array, arraylen));
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
	char *dest, *tmp;

	dest = (char *) malloc(sizeof(char));
	switch_assert(dest);

	for (i = 0; i < string_len; i++) {
		if (switch_string_match(string + i, string_len - i, search, search_len) == SWITCH_STATUS_SUCCESS) {
			for (n = 0; n < replace_len; n++) {
				dest[dest_len] = replace[n];
				dest_len++;
				tmp = (char *) realloc(dest, sizeof(char) * (dest_len + 1));
				switch_assert(tmp);
				dest = tmp;
			}
			i += search_len - 1;
		} else {
			dest[dest_len] = string[i];
			dest_len++;
			tmp = (char *) realloc(dest, sizeof(char) * (dest_len + 1));
			switch_assert(tmp);
			dest = tmp;
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

SWITCH_DECLARE(size_t) switch_url_encode(const char *url, char *buf, size_t len)
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


/* Written by Marc Espie, public domain */
#define SWITCH_CTYPE_NUM_CHARS       256

const short _switch_C_toupper_[1 + SWITCH_CTYPE_NUM_CHARS] = {
	EOF,
	0x00,	0x01,	0x02,	0x03,	0x04,	0x05,	0x06,	0x07,
	0x08,	0x09,	0x0a,	0x0b,	0x0c,	0x0d,	0x0e,	0x0f,
	0x10,	0x11,	0x12,	0x13,	0x14,	0x15,	0x16,	0x17,
	0x18,	0x19,	0x1a,	0x1b,	0x1c,	0x1d,	0x1e,	0x1f,
	0x20,	0x21,	0x22,	0x23,	0x24,	0x25,	0x26,	0x27,
	0x28,	0x29,	0x2a,	0x2b,	0x2c,	0x2d,	0x2e,	0x2f,
	0x30,	0x31,	0x32,	0x33,	0x34,	0x35,	0x36,	0x37,
	0x38,	0x39,	0x3a,	0x3b,	0x3c,	0x3d,	0x3e,	0x3f,
	0x40,	0x41,	0x42,	0x43,	0x44,	0x45,	0x46,	0x47,
	0x48,	0x49,	0x4a,	0x4b,	0x4c,	0x4d,	0x4e,	0x4f,
	0x50,	0x51,	0x52,	0x53,	0x54,	0x55,	0x56,	0x57,
	0x58,	0x59,	0x5a,	0x5b,	0x5c,	0x5d,	0x5e,	0x5f,
	0x60,	'A',	'B',	'C',	'D',	'E',	'F',	'G',
	'H',	'I',	'J',	'K',	'L',	'M',	'N',	'O',
	'P',	'Q',	'R',	'S',	'T',	'U',	'V',	'W',
	'X',	'Y',	'Z',	0x7b,	0x7c,	0x7d,	0x7e,	0x7f,
	0x80,	0x81,	0x82,	0x83,	0x84,	0x85,	0x86,	0x87,
	0x88,	0x89,	0x8a,	0x8b,	0x8c,	0x8d,	0x8e,	0x8f,
	0x90,	0x91,	0x92,	0x93,	0x94,	0x95,	0x96,	0x97,
	0x98,	0x99,	0x9a,	0x9b,	0x9c,	0x9d,	0x9e,	0x9f,
	0xa0,	0xa1,	0xa2,	0xa3,	0xa4,	0xa5,	0xa6,	0xa7,
	0xa8,	0xa9,	0xaa,	0xab,	0xac,	0xad,	0xae,	0xaf,
	0xb0,	0xb1,	0xb2,	0xb3,	0xb4,	0xb5,	0xb6,	0xb7,
	0xb8,	0xb9,	0xba,	0xbb,	0xbc,	0xbd,	0xbe,	0xbf,
	0xc0,	0xc1,	0xc2,	0xc3,	0xc4,	0xc5,	0xc6,	0xc7,
	0xc8,	0xc9,	0xca,	0xcb,	0xcc,	0xcd,	0xce,	0xcf,
	0xd0,	0xd1,	0xd2,	0xd3,	0xd4,	0xd5,	0xd6,	0xd7,
	0xd8,	0xd9,	0xda,	0xdb,	0xdc,	0xdd,	0xde,	0xdf,
	0xe0,	0xe1,	0xe2,	0xe3,	0xe4,	0xe5,	0xe6,	0xe7,
	0xe8,	0xe9,	0xea,	0xeb,	0xec,	0xed,	0xee,	0xef,
	0xf0,	0xf1,	0xf2,	0xf3,	0xf4,	0xf5,	0xf6,	0xf7,
	0xf8,	0xf9,	0xfa,	0xfb,	0xfc,	0xfd,	0xfe,	0xff
};

const short *_switch_toupper_tab_ = _switch_C_toupper_;

SWITCH_DECLARE(int) switch_toupper(int c)
{
	if ((unsigned int)c > 255)
		return(c);
	if (c < -1)
		return EOF;
	return((_switch_toupper_tab_ + 1)[c]);
}

const short _switch_C_tolower_[1 + SWITCH_CTYPE_NUM_CHARS] = {
	EOF,
	0x00,	0x01,	0x02,	0x03,	0x04,	0x05,	0x06,	0x07,
	0x08,	0x09,	0x0a,	0x0b,	0x0c,	0x0d,	0x0e,	0x0f,
	0x10,	0x11,	0x12,	0x13,	0x14,	0x15,	0x16,	0x17,
	0x18,	0x19,	0x1a,	0x1b,	0x1c,	0x1d,	0x1e,	0x1f,
	0x20,	0x21,	0x22,	0x23,	0x24,	0x25,	0x26,	0x27,
	0x28,	0x29,	0x2a,	0x2b,	0x2c,	0x2d,	0x2e,	0x2f,
	0x30,	0x31,	0x32,	0x33,	0x34,	0x35,	0x36,	0x37,
	0x38,	0x39,	0x3a,	0x3b,	0x3c,	0x3d,	0x3e,	0x3f,
	0x40,	'a',	'b',	'c',	'd',	'e',	'f',	'g',
	'h',	'i',	'j',	'k',	'l',	'm',	'n',	'o',
	'p',	'q',	'r',	's',	't',	'u',	'v',	'w',
	'x',	'y',	'z',	0x5b,	0x5c,	0x5d,	0x5e,	0x5f,
	0x60,	0x61,	0x62,	0x63,	0x64,	0x65,	0x66,	0x67,
	0x68,	0x69,	0x6a,	0x6b,	0x6c,	0x6d,	0x6e,	0x6f,
	0x70,	0x71,	0x72,	0x73,	0x74,	0x75,	0x76,	0x77,
	0x78,	0x79,	0x7a,	0x7b,	0x7c,	0x7d,	0x7e,	0x7f,
	0x80,	0x81,	0x82,	0x83,	0x84,	0x85,	0x86,	0x87,
	0x88,	0x89,	0x8a,	0x8b,	0x8c,	0x8d,	0x8e,	0x8f,
	0x90,	0x91,	0x92,	0x93,	0x94,	0x95,	0x96,	0x97,
	0x98,	0x99,	0x9a,	0x9b,	0x9c,	0x9d,	0x9e,	0x9f,
	0xa0,	0xa1,	0xa2,	0xa3,	0xa4,	0xa5,	0xa6,	0xa7,
	0xa8,	0xa9,	0xaa,	0xab,	0xac,	0xad,	0xae,	0xaf,
	0xb0,	0xb1,	0xb2,	0xb3,	0xb4,	0xb5,	0xb6,	0xb7,
	0xb8,	0xb9,	0xba,	0xbb,	0xbc,	0xbd,	0xbe,	0xbf,
	0xc0,	0xc1,	0xc2,	0xc3,	0xc4,	0xc5,	0xc6,	0xc7,
	0xc8,	0xc9,	0xca,	0xcb,	0xcc,	0xcd,	0xce,	0xcf,
	0xd0,	0xd1,	0xd2,	0xd3,	0xd4,	0xd5,	0xd6,	0xd7,
	0xd8,	0xd9,	0xda,	0xdb,	0xdc,	0xdd,	0xde,	0xdf,
	0xe0,	0xe1,	0xe2,	0xe3,	0xe4,	0xe5,	0xe6,	0xe7,
	0xe8,	0xe9,	0xea,	0xeb,	0xec,	0xed,	0xee,	0xef,
	0xf0,	0xf1,	0xf2,	0xf3,	0xf4,	0xf5,	0xf6,	0xf7,
	0xf8,	0xf9,	0xfa,	0xfb,	0xfc,	0xfd,	0xfe,	0xff
};

const short *_switch_tolower_tab_ = _switch_C_tolower_;

SWITCH_DECLARE(int) switch_tolower(int c)
{
	if ((unsigned int)c > 255)
		return(c);
	if (c < -1)
		return EOF;
	return((_switch_tolower_tab_ + 1)[c]);
}

/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#undef _U
#undef _L
#undef _N
#undef _S
#undef _P
#undef _C
#undef _X
#undef _B

#define	_U	0x01
#define	_L	0x02
#define	_N	0x04
#define	_S	0x08
#define	_P	0x10
#define	_C	0x20
#define	_X	0x40
#define	_B	0x80

const char _switch_C_ctype_[1 + SWITCH_CTYPE_NUM_CHARS] = {
	0,
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C,
	_C,	_C|_S,	_C|_S,	_C|_S,	_C|_S,	_C|_S,	_C,	_C,
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C,
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C,
   _S|_B,	_P,	_P,	_P,	_P,	_P,	_P,	_P,
	_P,	_P,	_P,	_P,	_P,	_P,	_P,	_P,
	_N,	_N,	_N,	_N,	_N,	_N,	_N,	_N,
	_N,	_N,	_P,	_P,	_P,	_P,	_P,	_P,
	_P,	_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U,
	_U,	_U,	_U,	_U,	_U,	_U,	_U,	_U,
	_U,	_U,	_U,	_U,	_U,	_U,	_U,	_U,
	_U,	_U,	_U,	_P,	_P,	_P,	_P,	_P,
	_P,	_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L,
	_L,	_L,	_L,	_L,	_L,	_L,	_L,	_L,
	_L,	_L,	_L,	_L,	_L,	_L,	_L,	_L,
/* determine printability based on the IS0 8859 8-bit standard */
	_L,	_L,	_L,	_P,	_P,	_P,	_P,	_C,

	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C, /* 80 */
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C, /* 88 */
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C, /* 90 */
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C, /* 98 */
	_P,	_P,	_P,	_P,	_P,	_P,	_P,	_P, /* A0 */
	_P,	_P,	_P,	_P,	_P,	_P,	_P,	_P, /* A8 */
	_P,	_P,	_P,	_P,	_P,	_P,	_P,	_P, /* B0 */
	_P,	_P,	_P,	_P,	_P,	_P,	_P,	_P, /* B8 */
	_P,	_P,	_P,	_P,	_P,	_P,	_P,	_P, /* C0 */
	_P,	_P,	_P,	_P,	_P,	_P,	_P,	_P, /* C8 */
	_P,	_P,	_P,	_P,	_P,	_P,	_P,	_P, /* D0 */
	_P,	_P,	_P,	_P,	_P,	_P,	_P,	_P, /* D8 */
	_P,	_P,	_P,	_P,	_P,	_P,	_P,	_P, /* E0 */
	_P,	_P,	_P,	_P,	_P,	_P,	_P,	_P, /* E8 */
	_P,	_P,	_P,	_P,	_P,	_P,	_P,	_P, /* F0 */
	_P,	_P,	_P,	_P,	_P,	_P,	_P,	_P  /* F8 */
};

const char *_switch_ctype_ = _switch_C_ctype_;

SWITCH_DECLARE(int) switch_isalnum(int c)
{
	return (c < 0 ? 0 : c > 255 ? 0 : ((_switch_ctype_ + 1)[(unsigned char)c] & (_U|_L|_N)));
}

SWITCH_DECLARE(int) switch_isalpha(int c)
{
	return (c < 0 ? 0 : c > 255 ? 0 : ((_switch_ctype_ + 1)[(unsigned char)c] & (_U|_L)));
}

SWITCH_DECLARE(int) switch_iscntrl(int c)
{
	return (c < 0 ? 0 : c > 255 ? 0 : ((_switch_ctype_ + 1)[(unsigned char)c] & _C));
}

SWITCH_DECLARE(int) switch_isdigit(int c)
{
	return (c < 0 ? 0 : c > 255 ? 0 : ((_switch_ctype_ + 1)[(unsigned char)c] & _N));
}

SWITCH_DECLARE(int) switch_isgraph(int c)
{
	return (c < 0 ? 0 : c > 255 ? 0 : ((_switch_ctype_ + 1)[(unsigned char)c] & (_P|_U|_L|_N)));
}

SWITCH_DECLARE(int) switch_islower(int c)
{
	return (c < 0 ? 0 : c > 255 ? 0 : ((_switch_ctype_ + 1)[(unsigned char)c] & _L));
}

SWITCH_DECLARE(int) switch_isprint(int c)
{
	return (c < 0 ? 0 : c > 255 ? 0 : ((_switch_ctype_ + 1)[(unsigned char)c] & (_P|_U|_L|_N|_B)));
}

SWITCH_DECLARE(int) switch_ispunct(int c)
{
	return (c < 0 ? 0 : c > 255 ? 0 : ((_switch_ctype_ + 1)[(unsigned char)c] & _P));
}

SWITCH_DECLARE(int) switch_isspace(int c)
{
	return (c < 0 ? 0 : c > 255 ? 0 : ((_switch_ctype_ + 1)[(unsigned char)c] & _S));
}

SWITCH_DECLARE(int) switch_isupper(int c)
{
	return (c < 0 ? 0 : c > 255 ? 0 : ((_switch_ctype_ + 1)[(unsigned char)c] & _U));
}

SWITCH_DECLARE(int) switch_isxdigit(int c)
{
	return (c < 0 ? 0 : c > 255 ? 0 : ((_switch_ctype_ + 1)[(unsigned char)c] & (_N|_X)));
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
