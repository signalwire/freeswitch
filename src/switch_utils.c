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
	ip_t ip;
	ip_t mask;
	uint32_t bits;
	int family;
	switch_bool_t ok;
	char *token;
	char *str;
	struct switch_network_node *next;
};
typedef struct switch_network_node switch_network_node_t;

struct switch_network_list {
	struct switch_network_node *node_head;
	switch_bool_t default_type;
	switch_memory_pool_t *pool;
	char *name;
};

#ifndef WIN32
SWITCH_DECLARE(int) switch_inet_pton(int af, const char *src, void *dst)
{
	return inet_pton(af, src, dst);
}
#endif

SWITCH_DECLARE(char *) switch_print_host(switch_sockaddr_t *addr, char *buf, switch_size_t len)
{
	switch_port_t port;

	switch_get_addr(buf, len, addr);
	port = switch_sockaddr_get_port(addr);

	snprintf(buf + strlen(buf), len - strlen(buf), ":%d", port);
	return buf;
}

SWITCH_DECLARE(switch_status_t) switch_frame_alloc(switch_frame_t **frame, switch_size_t size)
{
	switch_frame_t *new_frame;

	switch_zmalloc(new_frame, sizeof(*new_frame));

	switch_set_flag(new_frame, SFF_DYNAMIC);
	new_frame->buflen = (uint32_t)size;
	new_frame->data = malloc(size);
	switch_assert(new_frame->data);

	*frame = new_frame;

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status_t) switch_frame_dup(switch_frame_t *orig, switch_frame_t **clone)
{
	switch_frame_t *new_frame;

	if (!orig) {
		return SWITCH_STATUS_FALSE;
	}

	switch_assert(orig->buflen);

	new_frame = malloc(sizeof(*new_frame));

	switch_assert(new_frame);

	*new_frame = *orig;
	switch_set_flag(new_frame, SFF_DYNAMIC);

	new_frame->data = malloc(new_frame->buflen);
	switch_assert(new_frame->data);

	memcpy(new_frame->data, orig->data, orig->datalen);
	new_frame->codec = NULL;
	new_frame->pmap = NULL;
	*clone = new_frame;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_frame_free(switch_frame_t **frame)
{
	if (!frame || !*frame || !switch_test_flag((*frame), SFF_DYNAMIC)) {
		return SWITCH_STATUS_FALSE;
	}

	switch_safe_free((*frame)->data);
	free(*frame);
	*frame = NULL;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(int) switch_strcasecmp_any(const char *str, ...)
{
	va_list ap;
	const char *next_str = 0;
	int r = 0;

	va_start(ap, str);

	while ((next_str = va_arg(ap, const char *))) {
		if (!strcasecmp(str, next_str)) {
			r = 1;
			break;
		}
	}
	
	va_end(ap);

	return r;
}


SWITCH_DECLARE(char *) switch_find_parameter(const char *str, const char *param, switch_memory_pool_t *pool)
{
	char *e, *r = NULL, *ptr = NULL, *next = NULL;
	size_t len;

	ptr = (char *) str;

	while (ptr) {
		len = strlen(param);
		e = ptr+len;
		next = strchr(ptr, ';');

		if (!strncasecmp(ptr, param, len) && *e == '=') {
			size_t mlen;

			ptr = ++e;

			if (next) {
				e = next;
			} else {
				e = ptr + strlen(ptr);
			}
			
			mlen = (e - ptr) + 1;

			if (pool) {
				r = switch_core_alloc(pool, mlen);
			} else {
				r = malloc(mlen);
			}

			switch_snprintf(r, mlen, "%s", ptr);

			break;
		}

		if (next) {
			ptr = next + 1;
		} else break;
	}

	return r;
}

SWITCH_DECLARE(switch_status_t) switch_network_list_create(switch_network_list_t **list, const char *name, switch_bool_t default_type,
														   switch_memory_pool_t *pool)
{
	switch_network_list_t *new_list;

	if (!pool) {
		switch_core_new_memory_pool(&pool);
	}

	new_list = switch_core_alloc(pool, sizeof(**list));
	new_list->pool = pool;
	new_list->default_type = default_type;
	new_list->name = switch_core_strdup(new_list->pool, name);

	*list = new_list;

	return SWITCH_STATUS_SUCCESS;
}

#define IN6_AND_MASK(result, ip, mask) \
	((uint32_t *) (result))[0] =((const uint32_t *) (ip))[0] & ((const uint32_t *)(mask))[0]; \
	((uint32_t *) (result))[1] =((const uint32_t *) (ip))[1] & ((const uint32_t *)(mask))[1]; \
	((uint32_t *) (result))[2] =((const uint32_t *) (ip))[2] & ((const uint32_t *)(mask))[2]; \
	((uint32_t *) (result))[3] =((const uint32_t *) (ip))[3] & ((const uint32_t *)(mask))[3];
SWITCH_DECLARE(switch_bool_t) switch_testv6_subnet(ip_t _ip, ip_t _net, ip_t _mask) {
		if (!IN6_IS_ADDR_UNSPECIFIED(&_mask.v6)) {
			struct in6_addr a, b;
			IN6_AND_MASK(&a, &_net, &_mask);
			IN6_AND_MASK(&b, &_ip, &_mask);
			return !memcmp(&a,&b, sizeof(struct in6_addr));
		} else {
			if (!IN6_IS_ADDR_UNSPECIFIED(&_net.v6)) {
				return !memcmp(&_net,&_ip,sizeof(struct in6_addr));
			}
			else return SWITCH_TRUE;
		}
}
SWITCH_DECLARE(switch_bool_t) switch_network_list_validate_ip6_token(switch_network_list_t *list, ip_t ip, const char **token)
{
	switch_network_node_t *node;
	switch_bool_t ok = list->default_type;
	uint32_t bits = 0;

	for (node = list->node_head; node; node = node->next) {
		if (node->family == AF_INET) continue;
		if (node->bits > bits && switch_testv6_subnet(ip, node->ip, node->mask)) {
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

SWITCH_DECLARE(switch_bool_t) switch_network_list_validate_ip_token(switch_network_list_t *list, uint32_t ip, const char **token)
{
	switch_network_node_t *node;
	switch_bool_t ok = list->default_type;
	uint32_t bits = 0;

	for (node = list->node_head; node; node = node->next) {
		if (node->family == AF_INET6) continue; /* want AF_INET */
		if (node->bits > bits && switch_test_subnet(ip, node->ip.v4, node->mask.v4)) {
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

SWITCH_DECLARE(switch_status_t) switch_network_list_perform_add_cidr_token(switch_network_list_t *list, const char *cidr_str, switch_bool_t ok,
																		   const char *token)
{
	ip_t ip, mask;
	uint32_t bits;
	switch_network_node_t *node;

	if (switch_parse_cidr(cidr_str, &ip, &mask, &bits)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Adding %s (%s) [%s] to list %s\n",
						  cidr_str, ok ? "allow" : "deny", switch_str_nil(token), list->name);
		return SWITCH_STATUS_GENERR;
	}

	node = switch_core_alloc(list->pool, sizeof(*node));

	node->ip = ip;
	node->mask = mask;
	node->ok = ok;
	node->bits = bits;
	node->str = switch_core_strdup(list->pool, cidr_str);

	if (strchr(cidr_str,':')) {
		node->family = AF_INET6;
	} else {
		node->family = AF_INET;
	}

	if (!zstr(token)) {
		node->token = switch_core_strdup(list->pool, token);
	}

	node->next = list->node_head;
	list->node_head = node;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Adding %s (%s) [%s] to list %s\n",
					  cidr_str, ok ? "allow" : "deny", switch_str_nil(token), list->name);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_network_list_add_cidr_token(switch_network_list_t *list, const char *cidr_str, switch_bool_t ok, const char *token)
{
	char *cidr_str_dup = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (strchr(cidr_str, ',')) {
		char *argv[32] = { 0 };
		int i, argc;
		cidr_str_dup = strdup(cidr_str);

		switch_assert(cidr_str_dup);
		if ((argc = switch_separate_string(cidr_str_dup, ',', argv, (sizeof(argv) / sizeof(argv[0]))))) {
			for (i = 0; i < argc; i++) {
				switch_status_t this_status;
				if ((this_status = switch_network_list_perform_add_cidr_token(list, argv[i], ok, token)) != SWITCH_STATUS_SUCCESS) {
					status = this_status;
				}
			}
		}
	} else {
		status = switch_network_list_perform_add_cidr_token(list, cidr_str, ok, token);
	}

	switch_safe_free(cidr_str_dup);
	return status;
}

SWITCH_DECLARE(switch_status_t) switch_network_list_add_host_mask(switch_network_list_t *list, const char *host, const char *mask_str, switch_bool_t ok)
{
	ip_t ip, mask;
	switch_network_node_t *node;

	switch_inet_pton(AF_INET, host, &ip);
	switch_inet_pton(AF_INET, mask_str, &mask);

	node = switch_core_alloc(list->pool, sizeof(*node));

	node->ip.v4 = ntohl(ip.v4);
	node->mask.v4 = ntohl(mask.v4);
	node->ok = ok;

	/* http://graphics.stanford.edu/~seander/bithacks.html */
	mask.v4 = mask.v4 - ((mask.v4 >> 1) & 0x55555555);
	mask.v4 = (mask.v4 & 0x33333333) + ((mask.v4 >> 2) & 0x33333333);
	node->bits = (((mask.v4 + (mask.v4 >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;

	node->str = switch_core_sprintf(list->pool, "%s:%s", host, mask_str);

	node->next = list->node_head;
	list->node_head = node;

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(int) switch_parse_cidr(const char *string, ip_t *ip, ip_t *mask, uint32_t *bitp)
{
	char host[128];
	char *bit_str;
	int32_t bits;
	const char *ipv6;
	ip_t *maskv = mask;
	ip_t *ipv = ip;

	switch_copy_string(host, string, sizeof(host)-1);
	bit_str = strchr(host, '/');

	if (!bit_str) {
		return -1;
	}

	*bit_str++ = '\0';
	bits = atoi(bit_str);
	ipv6 = strchr(string, ':');
	if (ipv6) {
		int i,n;
		if (bits < 0 || bits > 128) {
			return -2;
		}
		bits = atoi(bit_str);
		switch_inet_pton(AF_INET6, host, (unsigned char *)ip);
		for (n=bits,i=0 ;i < 16; i++){
			if (n >= 8) {
				maskv->v6.s6_addr[i] = 0xFF;
				n -= 8;
			} else if (n < 8) {
				maskv->v6.s6_addr[i] = 0xFF & ~(0xFF >> n);
				n -= n;
			} else if (n == 0) {
				maskv->v6.s6_addr[i] = 0x00;
			}
		}
	} else {
		if (bits < 0 || bits > 32) {
			return -2;
		}

		bits = atoi(bit_str);
		switch_inet_pton(AF_INET, host, (unsigned char *)ip);
		ipv->v4 = htonl(ipv->v4);

		maskv->v4 = 0xFFFFFFFF & ~(0xFFFFFFFF >> bits);
	}
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
			if (*e == open && open != close) {
				depth++;
			} else if (*e == close) {
				depth--;
				if (!depth) {
					break;
				}
			}
		}
	}

	return (e && *e == close) ? (char *) e : NULL;
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

#define DLINE_BLOCK_SIZE 1024
#define DLINE_MAX_SIZE 1048576
SWITCH_DECLARE(switch_size_t) switch_fd_read_dline(int fd, char **buf, switch_size_t *len)
{
	char c, *p;
	int cur;
	switch_size_t total = 0;
	char *data = *buf;
	switch_size_t ilen = *len;

	if (!data) {
		*len = ilen = DLINE_BLOCK_SIZE;
		data = malloc(ilen);
		memset(data, 0, ilen);
	}

	p = data;
	while ((cur = read(fd, &c, 1)) == 1) {

		if (total + 2 >= ilen) {
			if (ilen + DLINE_BLOCK_SIZE > DLINE_MAX_SIZE) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Single line limit reached!\n");	
				break;
			}

			ilen += DLINE_BLOCK_SIZE;
			data = realloc(data, ilen);
			switch_assert(data);
			p = data + total;

		}

		total += cur;
		*p++ = c;

		if (c == '\r' || c == '\n') {
			break;
		}
	}

	*p++ = '\0';

	*len = ilen;
	*buf = data;

	return total;
}



SWITCH_DECLARE(switch_size_t) switch_fp_read_dline(FILE *fd, char **buf, switch_size_t *len)
{
	char c, *p;
	switch_size_t total = 0;
	char *data = *buf;
	switch_size_t ilen = *len;

	if (!data) {
		*len = ilen = DLINE_BLOCK_SIZE;
		data = malloc(ilen);
		memset(data, 0, ilen);
	}

	p = data;
	//while ((c = fgetc(fd)) != EOF) {

	while (fread(&c, 1, 1, fd) == 1) {
		
		if (total + 2 >= ilen) {
			if (ilen + DLINE_BLOCK_SIZE > DLINE_MAX_SIZE) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Single line limit reached!\n");	
				break;
			}

			ilen += DLINE_BLOCK_SIZE;
			data = realloc(data, ilen);
			switch_assert(data);
			p = data + total;

		}

		total++;
		*p++ = c;

		if (c == '\r' || c == '\n') {
			break;
		}
	}

	*p++ = '\0';

	*len = ilen;
	*buf = data;

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

		case '"':
			if (x + 6 > len - 1) {
				goto end;
			}
			*q++ = '&';
			*q++ = 'q';
			*q++ = 'u';
			*q++ = 'o';
			*q++ = 't';
			*q++ = ';';
			x += 6;
			break;
		case '\'':
			if (x + 6 > len - 1) {
				goto end;
			}
			*q++ = '&';
			*q++ = 'a';
			*q++ = 'p';
			*q++ = 'o';
			*q++ = 's';
			*q++ = ';';
			x += 6;
			break;
		case '&':
			if (x + 5 > len - 1) {
				goto end;
			}
			*q++ = '&';
			*q++ = 'a';
			*q++ = 'm';
			*q++ = 'p';
			*q++ = ';';
			x += 5;
			break;
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
			if (bytes >= (int)olen - 1) {
				goto end;
			}
			if (++y != 72) {
				continue;
			}
			/* out[bytes++] = '\n'; */
			y = 0;
		}
	}

	if (l > 0) {
		out[bytes++] = switch_b64_table[((b % 16) << (6 - l)) % 64];
	}
	if (l != 0) {
		while (l < 6 && bytes < (int)olen - 1) {
			out[bytes++] = '=', l += 2;
		}
	}

  end:

	out[bytes] = '\0';

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

SWITCH_DECLARE(switch_bool_t) switch_simple_email(const char *to,
												  const char *from,
												  const char *headers,
												  const char *body, const char *file, const char *convert_cmd, const char *convert_ext)
{
	char *bound = "XXXX_boundary_XXXX";
	const char *mime_type = "audio/inline";
	char filename[80], buf[B64BUFFLEN];
	int fd = -1, ifd = -1;
	int x = 0, y = 0, bytes = 0, ilen = 0;
	unsigned int b = 0, l = 0;
	unsigned char in[B64BUFFLEN];
	unsigned char out[B64BUFFLEN + 512];
	char *dupfile = NULL, *ext = NULL;
	char *newfile = NULL;
	switch_bool_t rval = SWITCH_FALSE;
	const char *err = NULL;

	if (!zstr(file) && !zstr(convert_cmd) && !zstr(convert_ext)) {
		if ((ext = strrchr(file, '.'))) {
			dupfile = strdup(file);
			if ((ext = strrchr(dupfile, '.'))) {
				*ext++ = '\0';
				newfile = switch_mprintf("%s.%s", dupfile, convert_ext);
			}
		}

		if (newfile) {
			char cmd[1024] = "";
			switch_snprintf(cmd, sizeof(cmd), "%s %s %s", convert_cmd, file, newfile);
			switch_system(cmd, SWITCH_TRUE);
			if (strcmp(file, newfile)) {
				file = newfile;
			} else {
				switch_safe_free(newfile);
			}
		}

		switch_safe_free(dupfile);
	}

	switch_snprintf(filename, 80, "%s%smail.%d%04x", SWITCH_GLOBAL_dirs.temp_dir, SWITCH_PATH_SEPARATOR, (int) switch_epoch_time_now(NULL), rand() & 0xffff);

	if ((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644)) > -1) {
		if (file) {
			if ((ifd = open(file, O_RDONLY | O_BINARY)) < 0) {
				rval = SWITCH_FALSE;
				err = "Cannot open tmp file\n";
				goto end;
			}
		}
		switch_snprintf(buf, B64BUFFLEN, "MIME-Version: 1.0\nContent-Type: multipart/mixed; boundary=\"%s\"\n", bound);
		if (!write_buf(fd, buf)) {
			rval = SWITCH_FALSE;
			err = "write error.";
			goto end;
		}

		if (headers && !write_buf(fd, headers)) {
			rval = SWITCH_FALSE;
			err = "write error.";
			goto end;
		}

		if (!write_buf(fd, "\n\n")) {
			rval = SWITCH_FALSE;
			err = "write error.";
			goto end;
		}

		if (body && switch_stristr("content-type", body)) {
			switch_snprintf(buf, B64BUFFLEN, "--%s\n", bound);
		} else {
			switch_snprintf(buf, B64BUFFLEN, "--%s\nContent-Type: text/plain\n\n", bound);
		}
		if (!write_buf(fd, buf)) {
			rval = SWITCH_FALSE;
			err = "write error.";
			goto end;
		}

		if (body) {
			if (!write_buf(fd, body)) {
				rval = SWITCH_FALSE;
				err = "write error.";
				goto end;
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
			if (!write_buf(fd, buf)) {
				rval = SWITCH_FALSE;
				err = "write error.";
				goto end;
			}

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
					rval = -1;
					break;
				} else {
					bytes = 0;
				}

			}

			if (l > 0) {
				out[bytes++] = switch_b64_table[((b % 16) << (6 - l)) % 64];
			}
			if (l != 0)
				while (l < 6) {
					out[bytes++] = '=', l += 2;
				}
			if (write(fd, &out, bytes) != bytes) {
				rval = -1;
			}

		}

		switch_snprintf(buf, B64BUFFLEN, "\n\n--%s--\n.\n", bound);

		if (!write_buf(fd, buf)) {
			rval = SWITCH_FALSE;
			err = "write error.";
			goto end;
		}
	}

	if (fd > -1) {
		close(fd);
		fd = -1;
	}

	if (zstr(from)) {
		from = "freeswitch";
	}

	{
		char *to_arg = switch_util_quote_shell_arg(to);
		char *from_arg = switch_util_quote_shell_arg(from);
#ifdef WIN32
		switch_snprintf(buf, B64BUFFLEN, "\"\"%s\" -f %s %s %s < \"%s\"\"", runtime.mailer_app, from_arg, runtime.mailer_app_args, to_arg, filename);
#else
		switch_snprintf(buf, B64BUFFLEN, "/bin/cat %s | %s -f %s %s %s", filename, runtime.mailer_app, from_arg, runtime.mailer_app_args, to_arg);
#endif
		switch_safe_free(to_arg); switch_safe_free(from_arg);
	}
	if (switch_system(buf, SWITCH_TRUE) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to execute command: %s\n", buf);
		err = "execute error";
		rval = SWITCH_FALSE;
	}

	if (zstr(err)) {
		if (file) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Emailed file [%s] to [%s]\n", filename, to);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Emailed data to [%s]\n", to);
		}

		rval = SWITCH_TRUE;
	}

  end:

	if (fd > -1) {
		close(fd);
	}

	if (unlink(filename) != 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to delete file [%s]\n", filename);
	}

	if (ifd > -1) {
		close(ifd);
	}


	if (newfile) {
		unlink(newfile);
		free(newfile);
	}

	if (rval != SWITCH_TRUE) {
		if (zstr(err)) err = "Unknown Error";

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "EMAIL NOT SENT, error [%s]\n", err);
	}

	return rval;
}

SWITCH_DECLARE(switch_bool_t) switch_is_lan_addr(const char *ip)
{
	if (zstr(ip))
		return SWITCH_FALSE;

	return (strncmp(ip, "10.", 3) &&       /* 10.0.0.0        -   10.255.255.255  (10/8 prefix)       */
			strncmp(ip, "192.168.", 8) &&  /* 192.168.0.0     -   192.168.255.255 (192.168/16 prefix) */
			strncmp(ip, "127.", 4) &&      /* 127.0.0.0       -   127.255.255.255 (127/8 prefix)      */
			strncmp(ip, "255.", 4) &&
			strncmp(ip, "0.", 2) &&       
			strncmp(ip, "1.", 2) &&
			strncmp(ip, "2.", 2) &&
			strncmp(ip, "172.16.", 7) &&   /* 172.16.0.0      -   172.31.255.255  (172.16/12 prefix)  */
			strncmp(ip, "172.17.", 7) &&
			strncmp(ip, "172.18.", 7) &&
			strncmp(ip, "172.19.", 7) &&
			strncmp(ip, "172.20.", 7) &&
			strncmp(ip, "172.21.", 7) &&
			strncmp(ip, "172.22.", 7) &&
			strncmp(ip, "172.23.", 7) &&
			strncmp(ip, "172.24.", 7) &&
			strncmp(ip, "172.25.", 7) &&
			strncmp(ip, "172.26.", 7) &&
			strncmp(ip, "172.27.", 7) &&
			strncmp(ip, "172.28.", 7) &&
			strncmp(ip, "172.29.", 7) &&
			strncmp(ip, "172.30.", 7) &&
			strncmp(ip, "172.31.", 7) &&
			strncmp(ip, "192.0.2.", 8) &&  /* 192.0.2.0       -   192.0.2.255      (192.0.2/24 prefix) */
			strncmp(ip, "169.254.", 8)     /* 169.254.0.0     -   169.254.255.255  (169.254/16 prefix) */
		)? SWITCH_FALSE : SWITCH_TRUE;
}

SWITCH_DECLARE(switch_bool_t) switch_ast2regex(const char *pat, char *rbuf, size_t len)
{
	const char *p = pat;

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

SWITCH_DECLARE(char *) switch_strip_whitespace(const char *str)
{
	const char *sp = str;
	char *p, *s = NULL;
	size_t len;

	if (zstr(sp)) {
		return strdup(SWITCH_BLANK_STRING);
	}

	while ((*sp == 13 ) || (*sp == 10 ) || (*sp == 9 ) || (*sp == 32) || (*sp == 11) ) {
		sp++;
	}
	
	if (zstr(sp)) {
		return strdup(SWITCH_BLANK_STRING);
	}

	s = strdup(sp);
	switch_assert(s);

	if ((len = strlen(s)) > 0) {
		p = s + (len - 1);

		while ((p >= s) && ((*p == 13 ) || (*p == 10 ) || (*p == 9 ) || (*p == 32) || (*p == 11))) {
			*p-- = '\0';
		}
	}

	return s;
}

SWITCH_DECLARE(char *) switch_strip_spaces(char *str, switch_bool_t dup)
{
	char *sp = str;
	char *p, *s = NULL;
	size_t len;

	if (zstr(sp)) {
		return dup ? strdup(SWITCH_BLANK_STRING) : sp;
	}

	while (*sp == ' ') {
		sp++;
	}

	if (dup) {
		s = strdup(sp);
		switch_assert(s);
	} else {
		s = sp;
	}

	if (zstr(s)) {
		return s;
	}

	if ((len = strlen(s)) > 0) {
		p = s + (len - 1);

		while (p && *p && (p >= s) && *p == ' ') {
			*p-- = '\0';
		}
	}

	return s;
}

SWITCH_DECLARE(char *) switch_strip_commas(char *in, char *out, switch_size_t len)
{
	char *p = in, *q = out;
	char *ret = out;
	switch_size_t x = 0;

	for (; p && *p; p++) {
		if ((*p > 47 && *p < 58)) {
			*q++ = *p;
		} else if (*p != ',') {
			ret = NULL;
			break;
		}

		if (++x > len) {
			ret = NULL;
			break;
		}
	}

	return ret;
}

SWITCH_DECLARE(char *) switch_strip_nonnumerics(char *in, char *out, switch_size_t len)
{
	char *p = in, *q = out;
	char *ret = out;
	switch_size_t x = 0;
	/* valid are 0 - 9, period (.), minus (-), and plus (+) - remove all others */
	for (; p && *p; p++) {
		if ((*p > 47 && *p < 58) || *p == '.' || *p == '-' || *p == '+') {
			*q++ = *p;
		}

		if (++x > len) {
			ret = NULL;
			break;
		}
	}

	return ret;
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

	if (*str == '-' || *str == '+') {
		str++;
	}

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

#ifdef HAVE_GETIFADDRS
#include <ifaddrs.h>
static int get_netmask(struct sockaddr_in *me, int *mask)
{
	struct ifaddrs *ifaddrs, *i = NULL;

	if (!me || getifaddrs(&ifaddrs) < 0) {
		return -1;
	}

	for (i = ifaddrs; i; i = i->ifa_next) {
		struct sockaddr_in *s = (struct sockaddr_in *) i->ifa_addr;
		struct sockaddr_in *m = (struct sockaddr_in *) i->ifa_netmask;

		if (s && m && s->sin_family == AF_INET && s->sin_addr.s_addr == me->sin_addr.s_addr) {
			*mask = m->sin_addr.s_addr;
			freeifaddrs(ifaddrs);
			return 0;
		}
	}

	freeifaddrs(ifaddrs);

	return -2;
}
#elif defined(__linux__)

#include <sys/ioctl.h>
#include <net/if.h>
static int get_netmask(struct sockaddr_in *me, int *mask)
{

	static struct ifreq ifreqs[20] = { {{{0}}} };
	struct ifconf ifconf;
	int nifaces, i;
	int sock;
	int r = -1;

	memset(&ifconf, 0, sizeof(ifconf));
	ifconf.ifc_buf = (char *) (ifreqs);
	ifconf.ifc_len = sizeof(ifreqs);


	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		goto end;
	}

	if (ioctl(sock, SIOCGIFCONF, (char *) &ifconf) < 0) {
		goto end;
	}

	nifaces = ifconf.ifc_len / sizeof(struct ifreq);

	for (i = 0; i < nifaces; i++) {
		struct sockaddr_in *sin = NULL;
		struct in_addr ip;

		ioctl(sock, SIOCGIFADDR, &ifreqs[i]);
		sin = (struct sockaddr_in *) &ifreqs[i].ifr_addr;
		ip = sin->sin_addr;

		if (ip.s_addr == me->sin_addr.s_addr) {
			ioctl(sock, SIOCGIFNETMASK, &ifreqs[i]);
			sin = (struct sockaddr_in *) &ifreqs[i].ifr_addr;
			/* mask = sin->sin_addr; */
			*mask = sin->sin_addr.s_addr;
			r = 0;
			break;
		}

	}

  end:

	close(sock);
	return r;

}

#elif defined(WIN32)

static int get_netmask(struct sockaddr_in *me, int *mask)
{
	SOCKET sock = WSASocket(AF_INET, SOCK_DGRAM, 0, 0, 0, 0);
	INTERFACE_INFO interfaces[20];
	unsigned long bytes;
	int interface_count, x;
	int r = -1;

	*mask = 0;

	if (sock == SOCKET_ERROR) {
		return -1;
	}

	if (WSAIoctl(sock, SIO_GET_INTERFACE_LIST, 0, 0, &interfaces, sizeof(interfaces), &bytes, 0, 0) == SOCKET_ERROR) {
		r = -1;
		goto end;
	}

	interface_count = bytes / sizeof(INTERFACE_INFO);

	for (x = 0; x < interface_count; ++x) {
		struct sockaddr_in *addr = (struct sockaddr_in *) &(interfaces[x].iiAddress);

		if (addr->sin_addr.s_addr == me->sin_addr.s_addr) {
			struct sockaddr_in *netmask = (struct sockaddr_in *) &(interfaces[x].iiNetmask);
			*mask = netmask->sin_addr.s_addr;
			r = 0;
			break;
		}
	}

  end:
	closesocket(sock);
	return r;
}

#else

static int get_netmask(struct sockaddr_in *me, int *mask)
{
	return -1;
}

#endif


SWITCH_DECLARE(switch_status_t) switch_resolve_host(const char *host, char *buf, size_t buflen)
{

	struct addrinfo *ai;
	int err;

	if ((err = getaddrinfo(host, 0, 0, &ai))) {
		return SWITCH_STATUS_FALSE;
	}

	get_addr(buf, buflen, ai->ai_addr, sizeof(*ai->ai_addr));

	freeaddrinfo(ai);

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status_t) switch_find_local_ip(char *buf, int len, int *mask, int family)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *base;
	char *force_local_ip_v4 = switch_core_get_variable_dup("force_local_ip_v4");
	char *force_local_ip_v6 = switch_core_get_variable_dup("force_local_ip_v6");

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

	switch (family) {
	case AF_INET:
		if (force_local_ip_v4) {
			switch_copy_string(buf, force_local_ip_v4, len);
			switch_safe_free(force_local_ip_v4);
			switch_safe_free(force_local_ip_v6);
			return SWITCH_STATUS_SUCCESS;
		}
	case AF_INET6:
		if (force_local_ip_v6) {
			switch_copy_string(buf, force_local_ip_v6, len);
			switch_safe_free(force_local_ip_v4);
			switch_safe_free(force_local_ip_v6);
			return SWITCH_STATUS_SUCCESS;
		}
	default:
		switch_safe_free(force_local_ip_v4);
		switch_safe_free(force_local_ip_v6);
		break;
	}


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
		base = "2001:503:BA3E::2:30";	/* DNS Root server A */
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
		if (mask) {
			get_netmask((struct sockaddr_in *) &l_address, mask);
		}
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
			if ( (tmp_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1 ) {
				goto doh;
			}

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

			switch_copy_string(buf, get_addr(abuf, sizeof(abuf), (struct sockaddr *) &iface_out, sizeof(iface_out)), len);
			if (mask) {
				get_netmask((struct sockaddr_in *) &iface_out, mask);
			}

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
			if ( (tmp_socket = socket(AF_INET6, SOCK_DGRAM, 0)) == -1 ) {
				goto doh;
			}

			if (connect(tmp_socket, (struct sockaddr *) &remote, sizeof(remote)) == -1) {
				goto doh;
			}

			ilen = sizeof(iface_out);
			if (getsockname(tmp_socket, (struct sockaddr *) &iface_out, &ilen) == -1) {
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

#ifdef HAVE_GETIFADDRS
# include <ifaddrs.h>
# include <net/if.h>
#endif
SWITCH_DECLARE(switch_status_t) switch_find_interface_ip(char *buf, int len, int *mask, const char *ifname, int family)
{
        switch_status_t status = SWITCH_STATUS_FALSE;

#ifdef HAVE_GETIFADDRS

	struct ifaddrs *addrs, *addr;

	getifaddrs(&addrs);
	for(addr = addrs; addr; addr = addr->ifa_next)
	{
		if (!(addr->ifa_flags & IFF_UP)) continue; // Address is not UP
		if (!addr->ifa_addr) continue; // No address set
		if (!addr->ifa_netmask) continue; // No netmask set
		if (family != AF_UNSPEC && addr->ifa_addr->sa_family != family) continue; // Not the address family we're looking for
		if (strcmp(addr->ifa_name, ifname)) continue; // Not the interface we're looking for

		switch(addr->ifa_addr->sa_family) {
		case AF_INET:
			inet_ntop(AF_INET, &( ((struct sockaddr_in*)(addr->ifa_addr))->sin_addr ), buf, len - 1);
			break;
		case AF_INET6:
			inet_ntop(AF_INET6, &( ((struct sockaddr_in6*)(addr->ifa_addr))->sin6_addr ), buf, len - 1);
			break;
		default:
			continue;
		}

		if (mask && addr->ifa_netmask->sa_family == AF_INET) {
			*mask = ((struct sockaddr_in*)(addr->ifa_addr))->sin_addr.s_addr;
		}

		status = SWITCH_STATUS_SUCCESS;
		break;
	}
	freeifaddrs(addrs);

#elif defined(__linux__)

	// TODO Not implemented, contributions welcome.

#elif defined(WIN32)

	// TODO Not implemented, contributions welcome.

#endif

	return status;
}


SWITCH_DECLARE(switch_time_t) switch_str_time(const char *in)
{
	switch_time_exp_t tm = { 0 }, local_tm = { 0 };
	int proceed = 0, ovector[30];
	switch_regex_t *re = NULL;
	char replace[1024] = "";
	switch_time_t ret = 0, local_time = 0;
	char *pattern = "^(\\d+)-(\\d+)-(\\d+)\\s*(\\d*):{0,1}(\\d*):{0,1}(\\d*)";
	char *pattern2 = "^(\\d{4})(\\d{2})(\\d{2})(\\d{2})(\\d{2})(\\d{2})";

	switch_time_exp_lt(&tm, switch_micro_time_now());
	tm.tm_year = tm.tm_mon = tm.tm_mday = tm.tm_hour = tm.tm_min = tm.tm_sec = tm.tm_usec = 0;

	if (!(proceed = switch_regex_perform(in, pattern, &re, ovector, sizeof(ovector) / sizeof(ovector[0])))) {
		switch_regex_safe_free(re);
		proceed = switch_regex_perform(in, pattern2, &re, ovector, sizeof(ovector) / sizeof(ovector[0]));
	}
	
	if (proceed) {

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

		switch_regex_safe_free(re);

		switch_time_exp_get(&local_time, &tm);
		switch_time_exp_lt(&local_tm, local_time);
		tm.tm_isdst = local_tm.tm_isdst;
		tm.tm_gmtoff = local_tm.tm_gmtoff;

		switch_time_exp_gmt_get(&ret, &tm);
		return ret;
	}

	switch_regex_safe_free(re);

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

#ifdef _MSC_VER
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
SWITCH_DECLARE(const char *) switch_inet_ntop(int af, void const *src, char *dst, size_t size)
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

#if HAVE_SIN6 || defined(NTDDI_VERSION)
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
			if (!switch_inet_ntop4(src + 12, tp, sizeof tmp - (tp - tmp)))
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

SWITCH_DECLARE(int) get_addr_int(switch_sockaddr_t *sa)
{
	struct sockaddr_in *s = (struct sockaddr_in *) &sa->sa;

	return ntohs((unsigned short) s->sin_addr.s_addr);
}

SWITCH_DECLARE(int) switch_cmp_addr(switch_sockaddr_t *sa1, switch_sockaddr_t *sa2)
{
	struct sockaddr_in *s1;
	struct sockaddr_in *s2;

	struct sockaddr_in6 *s16;
	struct sockaddr_in6 *s26;

	struct sockaddr *ss1;
	struct sockaddr *ss2;

	if (!(sa1 && sa2))
		return 0;

	s1 = (struct sockaddr_in *) &sa1->sa;
	s2 = (struct sockaddr_in *) &sa2->sa;

	s16 = (struct sockaddr_in6 *) &sa1->sa;
	s26 = (struct sockaddr_in6 *) &sa2->sa;

	ss1 = (struct sockaddr *) &sa1->sa;
	ss2 = (struct sockaddr *) &sa2->sa;

	if (ss1->sa_family != ss2->sa_family)
		return 0;

	switch (ss1->sa_family) {
	case AF_INET:
		return (s1->sin_addr.s_addr == s2->sin_addr.s_addr && s1->sin_port == s2->sin_port);
	case AF_INET6:
		if (s16->sin6_addr.s6_addr && s26->sin6_addr.s6_addr) {
			int i;

			if (s16->sin6_port != s26->sin6_port)
				return 0;

			for (i = 0; i < 4; i++) {
				if (*((int32_t *) s16->sin6_addr.s6_addr + i) != *((int32_t *) s26->sin6_addr.s6_addr + i))
					return 0;
			}

			return 1;
		}
	}

	return 0;
}

SWITCH_DECLARE(char *) get_addr6(char *buf, switch_size_t len, struct sockaddr_in6 *sa, socklen_t salen)
{
	switch_assert(buf);
	*buf = '\0';

	if (sa) {
#if defined(NTDDI_VERSION)
			switch_inet_ntop6((unsigned char*)&(sa->sin6_addr), buf, len);
#else
		inet_ntop(AF_INET6, &(sa->sin6_addr), buf, len);
#endif
	}

	return buf;
}

SWITCH_DECLARE(char *) get_addr(char *buf, switch_size_t len, struct sockaddr *sa, socklen_t salen)
{
	switch_assert(buf);
	*buf = '\0';

	if (sa) {
		getnameinfo(sa, salen, buf, (socklen_t) len, NULL, 0, NI_NUMERICHOST);
	}
	return buf;
}

SWITCH_DECLARE(unsigned short) get_port(struct sockaddr *sa)
{
	unsigned short port = 0;
	if (sa) {
		switch (sa->sa_family) {
		case AF_INET:
			port = ntohs(((struct sockaddr_in *) sa)->sin_port);
			break;
		case AF_INET6:
			port = ntohs(((struct sockaddr_in6 *) sa)->sin6_port);
			break;
		}
	}
	return port;
}

SWITCH_DECLARE(int) switch_build_uri(char *uri, switch_size_t size, const char *scheme, const char *user, const switch_sockaddr_t *sa, int flags)
{
	char host[NI_MAXHOST], serv[NI_MAXSERV];
	struct sockaddr_in6 si6;
	const struct sockaddr *addr;
	const char *colon;

	if (flags & SWITCH_URI_NO_SCOPE && sa->family == AF_INET6) {
		memcpy(&si6, &sa->sa, sa->salen);
		si6.sin6_scope_id = 0;

		addr = (const struct sockaddr *) &si6;
	} else {
		addr = (const struct sockaddr *) (intptr_t) & sa->sa;
	}

	if (getnameinfo(addr, sa->salen, host, sizeof(host), serv, sizeof(serv),
					((flags & SWITCH_URI_NUMERIC_HOST) ? NI_NUMERICHOST : 0) | ((flags & SWITCH_URI_NUMERIC_PORT) ? NI_NUMERICSERV : 0)) != 0) {
		return 0;
	}

	colon = strchr(host, ':');

	return switch_snprintf(uri, size, "%s:%s%s%s%s%s%s%s", scheme,
						   user ? user : "", user ? "@" : "", colon ? "[" : "", host, colon ? "]" : "", serv[0] ? ":" : "", serv[0] ? serv : "");
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

SWITCH_DECLARE(char *) switch_escape_string(const char *in, char *out, switch_size_t outlen)
{
	const char *p;
	char *o = out;

	for (p = in; *p; p++) {
		switch (*p) {
		case '\n':
			*o++ = '\\';
			*o++ = 'n';
			break;
		case '\r':
			*o++ = '\\';
			*o++ = 'r';
			break;
		case '\t':
			*o++ = '\\';
			*o++ = 't';
			break;
		case ' ':
			*o++ = '\\';
			*o++ = 's';
			break;
		case '$':
			*o++ = '\\';
			*o++ = '$';
			break;
		default:
			*o++ = *p;
			break;
		}
	}

	*o++ = '\0';

	return out;
}

SWITCH_DECLARE(char *) switch_escape_string_pool(const char *in, switch_memory_pool_t *pool)
{
	size_t len = strlen(in) * 2 + 1;
	char *buf = switch_core_alloc(pool, len);
	return switch_escape_string(in, buf, len);
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
			if (e == '\'' || e == '"' || (delim && e == delim) || e == ESCAPE_META || (e = unescape_char(*(ptr + 1))) != *(ptr + 1)) {
				++ptr;
				*dest++ = e;
				end = dest;
				esc++;
			}
		}
		if (!esc) {
			if (*ptr == '\'' && (inside_quotes || ((ptr+1) && strchr(ptr+1, '\'')))) {
				if ((inside_quotes = (1 - inside_quotes))) {
					end = dest;
				}
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

SWITCH_DECLARE(unsigned int) switch_separate_string_string(char *buf, char *delim, char **array, unsigned int arraylen)
{
	unsigned int count = 0;
	char *d;
	size_t dlen = strlen(delim);

	array[count++] = buf;

	while (count < arraylen && array[count - 1]) {
		if ((d = strstr(array[count - 1], delim))) {
			*d = '\0';
			d += dlen;
			array[count++] = d;
		} else
			break;
	}

	return count;
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
			} else if (*ptr == '\'' && (inside_quotes || ((ptr+1) && strchr(ptr+1, '\'')))) {
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


	if (*buf == '^' && *(buf+1) == '^') {
		char *p = buf + 2;
		
		if (p && *p && *(p+1)) {
			buf = p;
			delim = *buf++;
		}
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

SWITCH_DECLARE(char *) switch_util_quote_shell_arg(const char *string)
{
	return switch_util_quote_shell_arg_pool(string, NULL);
}

SWITCH_DECLARE(char *) switch_util_quote_shell_arg_pool(const char *string, switch_memory_pool_t *pool)
{
	size_t string_len = strlen(string);
	size_t i;
	size_t n = 0;
	size_t dest_len = 0;
	char *dest;

	/* first pass through, figure out how large to make the allocation */
	dest_len = strlen(string) + 1; /* string + null */
	dest_len += 1; /* opening quote */
	for (i = 0; i < string_len; i++) {
		switch (string[i]) {
#ifndef WIN32
		case '\'':
			/* We replace ' by sq backslace sq sq, so need 3 additional bytes */
			dest_len += 3;
			break;
#endif
		}
	}
	dest_len += 1; /* closing quote */

	/* if we're given a pool, allocate from it, otherwise use malloc */
	if (pool) {
		dest = switch_core_alloc(pool, sizeof(char) * dest_len);
	} else {
		dest = (char *) malloc(sizeof(char) * dest_len);
	}
	switch_assert(dest);

#ifdef WIN32
	dest[n++] = '"';
#else
	dest[n++] = '\'';
#endif

	for (i = 0; i < string_len; i++) {
		switch (string[i]) {
#ifdef WIN32
		case '"':
		case '%':
			dest[n++] = ' ';
			break;
#else
		case '\'':
			/* We replace ' by sq backslash sq sq */
			dest[n++] = '\'';
			dest[n++] = '\\';
			dest[n++] = '\'';
			dest[n++] = '\'';
			break;
#endif
		default:
			dest[n++] = string[i];
		}
	}

#ifdef WIN32
	dest[n++] = '"';
#else
	dest[n++] = '\'';
#endif
	dest[n++] = 0;
	switch_assert(n == dest_len);
	return dest;
}



#ifdef HAVE_POLL
#include <poll.h>
SWITCH_DECLARE(int) switch_wait_sock(switch_os_socket_t sock, uint32_t ms, switch_poll_t flags)
{
	struct pollfd pfds[2] = { { 0 } };
	int s = 0, r = 0;

	if (sock == SWITCH_SOCK_INVALID) {
		return SWITCH_SOCK_INVALID;
	}	

	pfds[0].fd = sock;


	if ((flags & SWITCH_POLL_READ)) {
		pfds[0].events |= POLLIN;
	}

	if ((flags & SWITCH_POLL_WRITE)) {
		pfds[0].events |= POLLOUT;
	}

	if ((flags & SWITCH_POLL_ERROR)) {
		pfds[0].events |= POLLERR;
	}

	if ((flags & SWITCH_POLL_HUP)) {
		pfds[0].events |= POLLHUP;
	}

	if ((flags & SWITCH_POLL_RDNORM)) {
		pfds[0].events |= POLLRDNORM;
	}

	if ((flags & SWITCH_POLL_RDBAND)) {
		pfds[0].events |= POLLRDBAND;
	}

	if ((flags & SWITCH_POLL_PRI)) {
		pfds[0].events |= POLLPRI;
	}
	
	s = poll(pfds, 1, ms);

	if (s < 0) {
		r = s;
	} else if (s > 0) {
		if ((pfds[0].revents & POLLIN)) {
			r |= SWITCH_POLL_READ;
		}
		if ((pfds[0].revents & POLLOUT)) {
			r |= SWITCH_POLL_WRITE;
		}
		if ((pfds[0].revents & POLLERR)) {
			r |= SWITCH_POLL_ERROR;
		}
		if ((pfds[0].revents & POLLHUP)) {
			r |= SWITCH_POLL_HUP;
		}
		if ((pfds[0].revents & POLLRDNORM)) {
			r |= SWITCH_POLL_RDNORM;
		}
		if ((pfds[0].revents & POLLRDBAND)) {
			r |= SWITCH_POLL_RDBAND;
		}
		if ((pfds[0].revents & POLLPRI)) {
			r |= SWITCH_POLL_PRI;
		}
		if ((pfds[0].revents & POLLNVAL)) {
			r |= SWITCH_POLL_INVALID;
		}
	}

	return r;

}

SWITCH_DECLARE(int) switch_wait_socklist(switch_waitlist_t *waitlist, uint32_t len, uint32_t ms)
{
	struct pollfd *pfds;
	int s = 0, r = 0, i;

	pfds = calloc(len, sizeof(struct pollfd));
	
	for (i = 0; i < len; i++) {
		if (waitlist[i].sock == SWITCH_SOCK_INVALID) {
			break;
		}

		pfds[i].fd = waitlist[i].sock;
		
		if ((waitlist[i].events & SWITCH_POLL_READ)) {
			pfds[i].events |= POLLIN;
		}

		if ((waitlist[i].events & SWITCH_POLL_WRITE)) {
			pfds[i].events |= POLLOUT;
		}

		if ((waitlist[i].events & SWITCH_POLL_ERROR)) {
			pfds[i].events |= POLLERR;
		}

		if ((waitlist[i].events & SWITCH_POLL_HUP)) {
			pfds[i].events |= POLLHUP;
		}

		if ((waitlist[i].events & SWITCH_POLL_RDNORM)) {
			pfds[i].events |= POLLRDNORM;
		}

		if ((waitlist[i].events & SWITCH_POLL_RDBAND)) {
			pfds[i].events |= POLLRDBAND;
		}

		if ((waitlist[i].events & SWITCH_POLL_PRI)) {
			pfds[i].events |= POLLPRI;
		}
	}
	
	s = poll(pfds, len, ms);

	if (s < 0) {
		r = s;
	} else if (s > 0) {
		for (i = 0; i < len; i++) {
			if ((pfds[i].revents & POLLIN)) {
				r |= SWITCH_POLL_READ;
				waitlist[i].revents |= SWITCH_POLL_READ;
			}
			if ((pfds[i].revents & POLLOUT)) {
				r |= SWITCH_POLL_WRITE;
				waitlist[i].revents |= SWITCH_POLL_WRITE;
			}
			if ((pfds[i].revents & POLLERR)) {
				r |= SWITCH_POLL_ERROR;
				waitlist[i].revents |= SWITCH_POLL_ERROR;
			}
			if ((pfds[i].revents & POLLHUP)) {
				r |= SWITCH_POLL_HUP;
				waitlist[i].revents |= SWITCH_POLL_HUP;
			}
			if ((pfds[i].revents & POLLRDNORM)) {
				r |= SWITCH_POLL_RDNORM;
				waitlist[i].revents |= SWITCH_POLL_RDNORM;
			}
			if ((pfds[i].revents & POLLRDBAND)) {
				r |= SWITCH_POLL_RDBAND;
				waitlist[i].revents |= SWITCH_POLL_RDBAND;
			}
			if ((pfds[i].revents & POLLPRI)) {
				r |= SWITCH_POLL_PRI;
				waitlist[i].revents |= SWITCH_POLL_PRI;
			}
			if ((pfds[i].revents & POLLNVAL)) {
				r |= SWITCH_POLL_INVALID;
				waitlist[i].revents |= SWITCH_POLL_INVALID;
			}
		}
	}

	free(pfds);

	return r;

}

#else
/* use select instead of poll */
SWITCH_DECLARE(int) switch_wait_sock(switch_os_socket_t sock, uint32_t ms, switch_poll_t flags)
{
	int s = 0, r = 0;
	fd_set *rfds;
	fd_set *wfds;
	fd_set *efds;
	struct timeval tv;

	if (sock == SWITCH_SOCK_INVALID) {
		return SWITCH_SOCK_INVALID;
	}

	rfds = malloc(sizeof(fd_set));
	wfds = malloc(sizeof(fd_set));
	efds = malloc(sizeof(fd_set));

	FD_ZERO(rfds);
	FD_ZERO(wfds);
	FD_ZERO(efds);

#ifndef WIN32
	/* Wouldn't you rather know?? */
	assert(sock <= FD_SETSIZE);
#endif
	
	if ((flags & SWITCH_POLL_READ)) {

#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 4127 )
	FD_SET(sock, rfds);
#pragma warning( pop ) 
#else
	FD_SET(sock, rfds);
#endif
	}

	if ((flags & SWITCH_POLL_WRITE)) {

#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 4127 )
	FD_SET(sock, wfds);
#pragma warning( pop ) 
#else
	FD_SET(sock, wfds);
#endif
	}

	if ((flags & SWITCH_POLL_ERROR)) {

#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 4127 )
	FD_SET(sock, efds);
#pragma warning( pop ) 
#else
	FD_SET(sock, efds);
#endif
	}

	tv.tv_sec = ms / 1000;
	tv.tv_usec = (ms % 1000) * ms;
	
	s = select(sock + 1, (flags & SWITCH_POLL_READ) ? rfds : NULL, (flags & SWITCH_POLL_WRITE) ? wfds : NULL, (flags & SWITCH_POLL_ERROR) ? efds : NULL, &tv);

	if (s < 0) {
		r = s;
	} else if (s > 0) {
		if ((flags & SWITCH_POLL_READ) && FD_ISSET(sock, rfds)) {
			r |= SWITCH_POLL_READ;
		}

		if ((flags & SWITCH_POLL_WRITE) && FD_ISSET(sock, wfds)) {
			r |= SWITCH_POLL_WRITE;
		}

		if ((flags & SWITCH_POLL_ERROR) && FD_ISSET(sock, efds)) {
			r |= SWITCH_POLL_ERROR;
		}
	}

	free(rfds);
	free(wfds);
	free(efds);

	return r;

}

SWITCH_DECLARE(int) switch_wait_socklist(switch_waitlist_t *waitlist, uint32_t len, uint32_t ms)
{
	int s = 0, r = 0;
	fd_set *rfds;
	fd_set *wfds;
	fd_set *efds;
	struct timeval tv;
	unsigned int i;
	switch_os_socket_t max_fd = 0;
	int flags = 0;

	rfds = malloc(sizeof(fd_set));
	wfds = malloc(sizeof(fd_set));
	efds = malloc(sizeof(fd_set));

	FD_ZERO(rfds);
	FD_ZERO(wfds);
	FD_ZERO(efds);

	for (i = 0; i < len; i++) {
		if (waitlist[i].sock == SWITCH_SOCK_INVALID) {
			break;
		}

		if (waitlist[i].sock > max_fd) {
			max_fd = waitlist[i].sock;
		}

#ifndef WIN32
		/* Wouldn't you rather know?? */
		assert(waitlist[i].sock <= FD_SETSIZE);
#endif
		flags |= waitlist[i].events;
	
		if ((waitlist[i].events & SWITCH_POLL_READ)) {

#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 4127 )
			FD_SET(waitlist[i].sock, rfds);
#pragma warning( pop ) 
#else
			FD_SET(waitlist[i].sock, rfds);
#endif
		}

		if ((waitlist[i].events & SWITCH_POLL_WRITE)) {

#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 4127 )
			FD_SET(waitlist[i].sock, wfds);
#pragma warning( pop ) 
#else
			FD_SET(waitlist[i].sock, wfds);
#endif
		}

		if ((waitlist[i].events & SWITCH_POLL_ERROR)) {

#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 4127 )
			FD_SET(waitlist[i].sock, efds);
#pragma warning( pop ) 
#else
			FD_SET(waitlist[i].sock, efds);
#endif
		}
	}

	tv.tv_sec = ms / 1000;
	tv.tv_usec = (ms % 1000) * ms;
	
	s = select(max_fd + 1, (flags & SWITCH_POLL_READ) ? rfds : NULL, (flags & SWITCH_POLL_WRITE) ? wfds : NULL, (flags & SWITCH_POLL_ERROR) ? efds : NULL, &tv);

	if (s < 0) {
		r = s;
	} else if (s > 0) {
		for (i = 0; i < len; i++) {
			if ((waitlist[i].events & SWITCH_POLL_READ) && FD_ISSET(waitlist[i].sock, rfds)) {
				r |= SWITCH_POLL_READ;
				waitlist[i].revents |= SWITCH_POLL_READ;
			}

			if ((waitlist[i].events & SWITCH_POLL_WRITE) && FD_ISSET(waitlist[i].sock, wfds)) {
				r |= SWITCH_POLL_WRITE;
				waitlist[i].revents |= SWITCH_POLL_WRITE;
			}

			if ((waitlist[i].events & SWITCH_POLL_ERROR) && FD_ISSET(waitlist[i].sock, efds)) {
				r |= SWITCH_POLL_ERROR;
				waitlist[i].revents |= SWITCH_POLL_ERROR;
			}
		}
	}

	free(rfds);
	free(wfds);
	free(efds);

	return r;

}
#endif

SWITCH_DECLARE(int) switch_socket_waitfor(switch_pollfd_t *poll, int ms)
{
	int nsds = 0;

	switch_poll(poll, 1, &nsds, ms);

	return nsds;
}

SWITCH_DECLARE(char *) switch_url_encode(const char *url, char *buf, size_t len)
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
			if ((x + 3) > len) {
				break;
			}
			buf[x++] = '%';
			buf[x++] = hex[(*p >> 4) & 0x0f];
			buf[x++] = hex[*p & 0x0f];
		} else {
			buf[x++] = *p;
		}
	}
	buf[x] = '\0';

	return buf;
}

SWITCH_DECLARE(char *) switch_url_decode(char *s)
{
	char *o;
	unsigned int tmp;

	if (zstr(s)) {
		return s;
	}

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

SWITCH_DECLARE(void) switch_split_time(const char *exp, int *hour, int *min, int *sec)
{
	char *dup = strdup(exp);
	char *shour = NULL;
	char *smin = NULL;
	char *ssec = NULL;

	switch_assert(dup);

	shour = dup;
	if ((smin=strchr(dup, ':'))) {
		*smin++ = '\0';
		if ((ssec=strchr(smin, ':'))) {
			*ssec++ = '\0';
		} else {
			ssec = "00";
		}
		if (hour && shour) {
			*hour = atol(shour);
		}
		if (min && smin) {
			*min = atol(smin);
		}
		if (sec && ssec) {
			*sec = atol(ssec);
		}

	}
	switch_safe_free(dup);
	return;

}

SWITCH_DECLARE(void) switch_split_date(const char *exp, int *year, int *month, int *day)
{
	char *dup = strdup(exp);
	char *syear = NULL;
	char *smonth = NULL;
	char *sday = NULL;

	switch_assert(dup);

	syear = dup;
	if ((smonth=strchr(dup, '-'))) {
		*smonth++ = '\0';
		if ((sday=strchr(smonth, '-'))) {
			*sday++ = '\0';
			if (year && syear) {
				*year = atol(syear);
			}
			if (month && smonth) {
				*month = atol(smonth);
			}
			if (day && sday) {
				*day = atol(sday);
			}
		}
	}
	switch_safe_free(dup);
	return;

}

/* Ex exp value "2009-10-10 14:33:22~2009-11-10 17:32:31" */
SWITCH_DECLARE(int) switch_fulldate_cmp(const char *exp, switch_time_t *ts)
{
	char *dup = strdup(exp);
	char *sStart;
	char *sEnd;

	switch_assert(dup);

	sStart = dup;
	if ((sEnd=strchr(dup, '~'))) {
		char *sDate = sStart;
		char *sTime;
		*sEnd++ = '\0';
		if ((sTime=strchr(sStart, ' '))) {
			switch_time_t tsStart;
			struct tm tmTmp;
			int year = 1970, month = 1, day = 1;
			int hour = 0, min = 0, sec = 0;
			*sTime++ = '\0';

			memset(&tmTmp, 0, sizeof(tmTmp));
			switch_split_date(sDate, &year, &month, &day);
			switch_split_time(sTime, &hour, &min, &sec);
			tmTmp.tm_year = year-1900;
			tmTmp.tm_mon = month-1;
			tmTmp.tm_mday = day;

			tmTmp.tm_hour = hour;
			tmTmp.tm_min = min;
			tmTmp.tm_sec = sec;
			tmTmp.tm_isdst = 0;
			tsStart = mktime(&tmTmp);

			sDate = sEnd;
			if ((sTime=strchr(sEnd, ' '))) {
				switch_time_t tsEnd;
				struct tm tmTmp;
				int year = 1970, month = 1, day = 1;
				int hour = 0, min = 0, sec = 0;
				*sTime++ = '\0';

				memset(&tmTmp, 0, sizeof(tmTmp));
				switch_split_date(sDate, &year, &month, &day);
				switch_split_time(sTime, &hour, &min, &sec);
				tmTmp.tm_year = year-1900;
				tmTmp.tm_mon = month-1;
				tmTmp.tm_mday = day;

				tmTmp.tm_hour = hour;
				tmTmp.tm_min = min;
				tmTmp.tm_sec = sec;
				tmTmp.tm_isdst = 0;
				tsEnd = mktime(&tmTmp);

				if (tsStart <= *ts/1000000 && tsEnd > *ts/1000000) {
					switch_safe_free(dup);
					return 1;
				}
			}
		}
	}
	switch_safe_free(dup);
	return 0;

}


/* Written by Marc Espie, public domain */
#define SWITCH_CTYPE_NUM_CHARS       256

const short _switch_C_toupper_[1 + SWITCH_CTYPE_NUM_CHARS] = {
	EOF,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
	0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
	0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
	0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

const short *_switch_toupper_tab_ = _switch_C_toupper_;

SWITCH_DECLARE(int) old_switch_toupper(int c)
{
	if ((unsigned int) c > 255)
		return (c);
	if (c < -1)
		return EOF;
	return ((_switch_toupper_tab_ + 1)[c]);
}

const short _switch_C_tolower_[1 + SWITCH_CTYPE_NUM_CHARS] = {
	EOF,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
	'x', 'y', 'z', 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
	0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
	0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

const short *_switch_tolower_tab_ = _switch_C_tolower_;

SWITCH_DECLARE(int) old_switch_tolower(int c)
{
	if ((unsigned int) c > 255)
		return (c);
	if (c < -1)
		return EOF;
	return ((_switch_tolower_tab_ + 1)[c]);
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

const int _switch_C_ctype_[1 + SWITCH_CTYPE_NUM_CHARS] = {
	0,
	_C, _C, _C, _C, _C, _C, _C, _C,
	_C, _C | _S, _C | _S, _C | _S, _C | _S, _C | _S, _C, _C,
	_C, _C, _C, _C, _C, _C, _C, _C,
	_C, _C, _C, _C, _C, _C, _C, _C,
	_S | _B, _P, _P, _P, _P, _P, _P, _P,
	_P, _P, _P, _P, _P, _P, _P, _P,
	_N, _N, _N, _N, _N, _N, _N, _N,
	_N, _N, _P, _P, _P, _P, _P, _P,
	_P, _U | _X, _U | _X, _U | _X, _U | _X, _U | _X, _U | _X, _U,
	_U, _U, _U, _U, _U, _U, _U, _U,
	_U, _U, _U, _U, _U, _U, _U, _U,
	_U, _U, _U, _P, _P, _P, _P, _P,
	_P, _L | _X, _L | _X, _L | _X, _L | _X, _L | _X, _L | _X, _L,
	_L, _L, _L, _L, _L, _L, _L, _L,
	_L, _L, _L, _L, _L, _L, _L, _L,
/* determine printability based on the IS0 8859 8-bit standard */
	_L, _L, _L, _P, _P, _P, _P, _C,

	_C, _C, _C, _C, _C, _C, _C, _C,	/* 80 */
	_C, _C, _C, _C, _C, _C, _C, _C,	/* 88 */
	_C, _C, _C, _C, _C, _C, _C, _C,	/* 90 */
	_C, _C, _C, _C, _C, _C, _C, _C,	/* 98 */
	_P, _P, _P, _P, _P, _P, _P, _P,	/* A0 */
	_P, _P, _P, _P, _P, _P, _P, _P,	/* A8 */
	_P, _P, _P, _P, _P, _P, _P, _P,	/* B0 */
	_P, _P, _P, _P, _P, _P, _P, _P,	/* B8 */
	_P, _P, _P, _P, _P, _P, _P, _P,	/* C0 */
	_P, _P, _P, _P, _P, _P, _P, _P,	/* C8 */
	_P, _P, _P, _P, _P, _P, _P, _P,	/* D0 */
	_P, _P, _P, _P, _P, _P, _P, _P,	/* D8 */
	_P, _P, _P, _P, _P, _P, _P, _P,	/* E0 */
	_P, _P, _P, _P, _P, _P, _P, _P,	/* E8 */
	_P, _P, _P, _P, _P, _P, _P, _P,	/* F0 */
	_P, _P, _P, _P, _P, _P, _P, _P	/* F8 */
};

const int *_switch_ctype_ = _switch_C_ctype_;

SWITCH_DECLARE(int) switch_isalnum(int c)
{
	return (c < 0 ? 0 : c > 255 ? 0 : ((_switch_ctype_ + 1)[(unsigned char) c] & (_U | _L | _N)));
}

SWITCH_DECLARE(int) switch_isalpha(int c)
{
	return (c < 0 ? 0 : c > 255 ? 0 : ((_switch_ctype_ + 1)[(unsigned char) c] & (_U | _L)));
}

SWITCH_DECLARE(int) switch_iscntrl(int c)
{
	return (c < 0 ? 0 : c > 255 ? 0 : ((_switch_ctype_ + 1)[(unsigned char) c] & _C));
}

SWITCH_DECLARE(int) switch_isdigit(int c)
{
	return (c < 0 ? 0 : c > 255 ? 0 : ((_switch_ctype_ + 1)[(unsigned char) c] & _N));
}

SWITCH_DECLARE(int) switch_isgraph(int c)
{
	return (c < 0 ? 0 : c > 255 ? 0 : ((_switch_ctype_ + 1)[(unsigned char) c] & (_P | _U | _L | _N)));
}

SWITCH_DECLARE(int) switch_islower(int c)
{
	return (c < 0 ? 0 : c > 255 ? 0 : ((_switch_ctype_ + 1)[(unsigned char) c] & _L));
}

SWITCH_DECLARE(int) switch_isprint(int c)
{
	return (c < 0 ? 0 : c > 255 ? 0 : ((_switch_ctype_ + 1)[(unsigned char) c] & (_P | _U | _L | _N | _B)));
}

SWITCH_DECLARE(int) switch_ispunct(int c)
{
	return (c < 0 ? 0 : c > 255 ? 0 : ((_switch_ctype_ + 1)[(unsigned char) c] & _P));
}

SWITCH_DECLARE(int) switch_isspace(int c)
{
	return (c < 0 ? 0 : c > 255 ? 0 : ((_switch_ctype_ + 1)[(unsigned char) c] & _S));
}

SWITCH_DECLARE(int) switch_isupper(int c)
{
	return (c < 0 ? 0 : c > 255 ? 0 : ((_switch_ctype_ + 1)[(unsigned char) c] & _U));
}

SWITCH_DECLARE(int) switch_isxdigit(int c)
{
	return (c < 0 ? 0 : c > 255 ? 0 : ((_switch_ctype_ + 1)[(unsigned char) c] & (_N | _X)));
}
static const char *DOW[] = {
	"sun",
	"mon",
	"tue",
	"wed",
	"thu",
	"fri",
	"sat"
};

SWITCH_DECLARE(const char *) switch_dow_int2str(int val) {
	if (val >= switch_arraylen(DOW)) {
		val = val % switch_arraylen(DOW);
	}
	return DOW[val];
}

SWITCH_DECLARE(int) switch_dow_str2int(const char *exp) {
	int ret = -1;
	int x;
	
	for (x = 0; x < switch_arraylen(DOW); x++) {
		if (!strncasecmp(DOW[x], exp, 3)) {
			ret = x + 1;
			break;
		}
	}
	return ret;
}

typedef enum {
	DOW_ERR = -2,
	DOW_EOF = -1,
	DOW_SUN = 1,
	DOW_MON,
	DOW_TUE,
	DOW_WED,
	DOW_THU,
	DOW_FRI,
	DOW_SAT,
	DOW_HYPHEN = '-',
	DOW_COMA = ','
} dow_t;

static inline dow_t _dow_read_token(const char **s) 
{
	int i;
	
	if (**s == '-') {
		(*s)++;
		return DOW_HYPHEN;
	} else if (**s == ',') {
		(*s)++;
		return DOW_COMA;
	} else if (**s >= '1' && **s <= '7') {
		dow_t r = **s - '0';
		(*s)++;
		return r;
	} else if ((i = switch_dow_str2int(*s)) && i != -1) {
		(*s) += 3;
		return i;
	} else if (!**s) {
		return DOW_EOF;
	} else {
		return DOW_ERR;
	}
}

SWITCH_DECLARE(switch_bool_t) switch_dow_cmp(const char *exp, int val)
{
	dow_t cur, prev = DOW_EOF, range_start = DOW_EOF;
	const char *p = exp;
		
	while ((cur = _dow_read_token(&p)) != DOW_EOF) {
		if (cur == DOW_COMA) {
			/* Reset state */
			cur = prev = DOW_EOF;	
		} else if (cur == DOW_HYPHEN) {
			/* Save the previous token and move to the next one */
			range_start = prev;
		} else if (cur == DOW_ERR) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse error for [%s] at position %ld (%.6s)\n", exp, (long) (p - exp), p);
			break;
		} else {
			/* Valid day found */
			if (range_start != DOW_EOF) { /* Evaluating a range */
				if (val >= range_start && val <= cur) {
					return SWITCH_TRUE;
				}
				range_start = DOW_EOF;
			} else if (val == cur) {
				return SWITCH_TRUE;
			}
		}
		
		prev = cur;
	}

	return SWITCH_FALSE;
}

SWITCH_DECLARE(int) switch_number_cmp(const char *exp, int val)
{
	char *p;

	if ((p = strchr(exp, '-'))) {
		int min;
		int max;

		min = atol(exp);
		p++;
		max = atol(p);

		if (val >= min && val <= max) {
			return 1;
		}
	} else if ((p = strchr(exp, ','))) {
		const char *cur = exp;
		p++;
		while (cur) {
			if (atol(cur) == val) {
				return 1;
			}

			cur = p;
			if (p && p + 1) {
				if ((p = strchr((p + 1), ','))) {
					p++;
				}
			}
		}
	} else {
		if (atol(exp) == val) {
			return 1;
		}
	}

	return 0;

}

SWITCH_DECLARE(int) switch_tod_cmp(const char *exp, int val)
{
	char *dup = strdup(exp);
	char *minh;
	char *minm;
	char *mins;
	char *maxh;
	char *maxm;
	char *maxs;

	switch_assert(dup);

	minh = dup;
	if ((minm=strchr(dup, ':'))) {
		*minm++ = '\0';
		if ((maxh=strchr(minm, '-'))) {
			if ((maxm=strchr(maxh, ':'))) {
				*maxh++ = '\0';
				*maxm++ = '\0';
				/* Check if min/max seconds are present */
				if ((mins=strchr(minm, ':'))) {
					*mins++ = '\0';
				} else {
					mins = "00";
				}
				if ((maxs=strchr(maxm, ':'))) {
					*maxs++ = '\0';
				} else {
					maxs = "00";
				}

				if (val >= (atol(minh) * 60 * 60) + (atol(minm) * 60) + atol(mins) && val < (atol(maxh) * 60 * 60) + (atol(maxm) * 60) + atol(maxs)) {
					switch_safe_free(dup);
					return 1;
				}
			}
		}
	}
	switch_safe_free(dup);
	return 0;

}

SWITCH_DECLARE(int) switch_split_user_domain(char *in, char **user, char **domain)
{
	char *p = NULL, *h = NULL, *u = NULL;

	if (!in) return 0;

	/* Remove URL scheme */
	if (!strncasecmp(in, "sip:", 4)) in += 4;
	else if (!strncasecmp(in, "sips:", 5)) in += 5;

	/* Isolate the host part from the user part */
	if ((h = in, p = strchr(h, '@'))) *p = '\0', u = in, h = p+1;

	/* Clean out the host part of any suffix */
	for (p = h; *p; p++)
		if (*p == ':' || *p == ';' || *p == ' ') {
			*p = '\0'; break;
		}

	if (user) *user = u;
	if (domain) *domain = h;
	return 1;
}


SWITCH_DECLARE(char *) switch_uuid_str(char *buf, switch_size_t len)
{
	switch_uuid_t uuid;

	if (len < (SWITCH_UUID_FORMATTED_LENGTH + 1)) {
		switch_snprintf(buf, len, "INVALID");
	} else {
		switch_uuid_get(&uuid);
		switch_uuid_format(buf, &uuid);
	}

	return buf;
}


SWITCH_DECLARE(char *) switch_format_number(const char *num)
{
	char *r;
	size_t len;
	const char *p = num;

	if (!p) {
		return (char*)p;
	}

	if (zstr(p)) {
		return strdup(p);
	}

	if (*p == '+') {
		p++;
	}

	if (!switch_is_number(p)) {
		return strdup(p);
	}

	len = strlen(p);
	
	/* region 1, TBD add more....*/
	if (len == 11 && p[0] == '1') {
		r = switch_mprintf("%c (%c%c%c) %c%c%c-%c%c%c%c", p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],p[8],p[9],p[10]);
	} else if (len == 10) {
		r = switch_mprintf("1 (%c%c%c) %c%c%c-%c%c%c%c", p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],p[8],p[9]);
	} else {
		r = strdup(num);
	}

	return r;
}


SWITCH_DECLARE(unsigned int) switch_atoui(const char *nptr)
{
	int tmp = atoi(nptr);
	if (tmp < 0) return 0;
	else return (unsigned int) tmp;
}

SWITCH_DECLARE(unsigned long) switch_atoul(const char *nptr)
{
	long tmp = atol(nptr);
	if (tmp < 0) return 0;
	else return (unsigned long) tmp;
}


SWITCH_DECLARE(char *) switch_strerror_r(int errnum, char *buf, switch_size_t buflen)
{
#ifdef HAVE_STRERROR_R
#ifdef STRERROR_R_CHAR_P
	/* GNU variant returning char *, avoids warn-unused-result error */
	return strerror_r(errnum, buf, buflen);
#else
	/*
	 * XSI variant returning int, with GNU compatible error string,
	 * if no message could be found
	 */
	if (strerror_r(errnum, buf, buflen)) {
		switch_snprintf(buf, buflen, "Unknown error %d", errnum);
	}
	return buf;
#endif /* STRERROR_R_CHAR_P */
#elif defined(WIN32)
	/* WIN32 variant */
	if (strerror_s(buf, buflen, errnum)) {
		switch_snprintf(buf, buflen, "Unknown error %d", errnum);
	}
	return buf;
#else
	/* Fallback, copy string into private buffer */
	switch_copy_string(buf, strerror(errnum), buflen);
	return buf;
#endif
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
