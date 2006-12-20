/* iksemel (XML parser for Jabber)
** Copyright (C) 2000-2003 Gurer Ozen <madcat@e-kolay.net>
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU Lesser General Public License.
*/

#include "common.h"
#include "iksemel.h"

static char base64_charset[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


char *iks_base64_decode(const char *buf)
{
	char *res, *save, *foo, val;
	const char *end;
	int index;

	if (!buf)
		return NULL;

	index = iks_strlen(buf)*6 / 8 + 1;
	save = res = iks_malloc(index);
	memset(res, 0, index);
	index = 0;

	if (!save)
		return NULL;

	end = buf + iks_strlen(buf);

	while (*buf && buf < end) {
		if (!(foo = strchr(base64_charset, *buf)))
			foo = base64_charset;
		val = (int)(foo - base64_charset);
		buf++;
		switch (index) {
			case 0:
				*res |= val << 2;
				break;
			case 1:
				*res++ |= val >> 4;
				*res |= val << 4;
				break;
			case 2:
				*res++ |= val >> 2;
				*res |= val << 6;
				break;
			case 3:
				*res++ |= val;
				break;
		}
		index++;
		index %= 4;
	}
	*res = 0;

	return save;
}

char *iks_base64_encode(const char *buf, int len)
{
	char *res, *save;
	int k, t;

	len = (len > 0) ? (len) : (iks_strlen(buf));
	save = res = iks_malloc((len*8) / 6 + 4);

	for (k = 0; k < len/3; ++k) {
		*res++ = base64_charset[*buf >> 2];
		t = ((*buf & 0x03) << 4);
		buf++;
		*res++ = base64_charset[t | (*buf >> 4)];
		t = ((*buf & 0x0F) << 2);
		buf++;
		*res++ = base64_charset[t | (*buf >> 6)];
		*res++ = base64_charset[*buf++ & 0x3F];
	}

	switch (len % 3) {
		case 2:
			*res++ = base64_charset[*buf >> 2];
			t =  ((*buf & 0x03) << 4);
			buf++;
			*res++ = base64_charset[t | (*buf >> 4)];
			*res++ = base64_charset[((*buf++ & 0x0F) << 2)];
			break;
		case 1:
			*res++ = base64_charset[*buf >> 2];
			*res++ = base64_charset[(*buf++ & 0x03) << 4];
			break;
	}
	*res++ = '=';
	*res++ = '=';
	*res = 0;
	return save;
}
