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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>



SWITCH_DECLARE(int) switch_perform_regex(char *field, char *expression, pcre **new_re, int *ovector, uint32_t olen)
{
	const char *error = NULL;
	int erroffset = 0;
	pcre *re = NULL;
	int match_count = 0;
	
	if (!(field && expression)) {
		return 0;
	}

	re = pcre_compile(expression, /* the pattern */
					  0,		  /* default options */
					  &error,	  /* for error message */
					  &erroffset, /* for error offset */
					  NULL);	  /* use default character tables */
	if (error) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "COMPILE ERROR: %d [%s]\n", erroffset, error);
		switch_clean_re(re);
		return 0;
	}

	match_count = pcre_exec(re,	/* result of pcre_compile() */
							NULL,	/* we didn't study the pattern */
							field,	/* the subject string */
							(int) strlen(field), /* the length of the subject string */
							0,	/* start at offset 0 in the subject */
							0,	/* default options */
							ovector,	/* vector of integers for substring information */
							olen); /* number of elements (NOT size in bytes) */

	if (match_count <= 0) {
		switch_clean_re(re);
		match_count = 0;
	}

	*new_re = re;

	return match_count;
}


SWITCH_DECLARE(void) switch_perform_substitution(pcre *re, int match_count, char *data, char *field_data, char *substituted, uint32_t len, int *ovector)
{
	char index[10] = "";
	char replace[1024] = "";
	uint32_t x, y = 0, z = 0, num = 0;

	for (x = 0; x < (len-1) && x < strlen(data);) {
		if (data[x] == '$') {
			x++;
			
			if (!(data[x] > 47 && data[x] < 58)) {
				substituted[y++] = data[x-1];
				continue;
			}

			while (data[x] > 47 && data[x] < 58) {
				index[z++] = data[x];
				x++;
			}
			index[z++] = '\0';
			z = 0;
			num = atoi(index);
			
			if (pcre_copy_substring(field_data,
									ovector,
									match_count,
									num,
									replace,
									sizeof(replace)) > 0) {
				unsigned int r;
				for (r = 0; r < strlen(replace); r++) {
					substituted[y++] = replace[r];
				}
			}
		} else {
			substituted[y++] = data[x];
			x++;
		}
	}
	substituted[y++] = '\0';
}



SWITCH_DECLARE(switch_time_t) switch_str_time(char *in)
{
    switch_time_exp_t tm = {0};
    int proceed = 0, ovector[30];
    pcre *re = NULL;
    char replace[1024] = "";
    switch_time_t ret = 0;
    char *pattern = "^(\\d+)-(\\d+)-(\\d+)\\s*(\\d*):{0,1}(\\d*):{0,1}(\\d*)";

    switch_time_exp_lt(&tm, switch_time_now());
    tm.tm_year = tm.tm_mon = tm.tm_mday = tm.tm_hour = tm.tm_min = tm.tm_sec = 0;

    if ((proceed = switch_perform_regex(in, pattern, &re, ovector, sizeof(ovector) / sizeof(ovector[0])))) {

        if (proceed > 1) {
            pcre_copy_substring(in, ovector, proceed, 1, replace, sizeof(replace));
            tm.tm_year = atoi(replace) - 1900;
        }

        if (proceed > 2) {
            pcre_copy_substring(in, ovector, proceed, 2, replace, sizeof(replace));
            tm.tm_mon = atoi(replace) - 1;
        }

        if (proceed > 3) {
            pcre_copy_substring(in, ovector, proceed, 3, replace, sizeof(replace));
            tm.tm_mday = atoi(replace)-1;
        }
        
        if (proceed > 4) {
            pcre_copy_substring(in, ovector, proceed, 4, replace, sizeof(replace));
            tm.tm_hour = atoi(replace);
        }

        if (proceed > 5) {
            pcre_copy_substring(in, ovector, proceed, 5, replace, sizeof(replace));
            tm.tm_min = atoi(replace);
        }

        if (proceed > 6) {
            pcre_copy_substring(in, ovector, proceed, 6, replace, sizeof(replace));
            tm.tm_sec = atoi(replace);
        }

        apr_time_exp_gmt_get(&ret, &tm);
        return ret;
    } /* possible else with more patterns later */
    
    return ret;

}

SWITCH_DECLARE(char *) switch_priority_name(switch_priority_t priority)
{
	switch(priority) { /*lol*/
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



SWITCH_DECLARE(char *) switch_get_addr(char *buf, switch_size_t len, switch_sockaddr_t *in)
{
	uint8_t x, *i;
	char *p = buf;


	i = (uint8_t *) &in->sa.sin.sin_addr;

	memset(buf, 0, len);
	for(x =0; x < 4; x++) {
		sprintf(p, "%u%s", i[x], x == 3 ? "" : ".");
		p = buf + strlen(buf);
	}
	return buf;
}

SWITCH_DECLARE(apr_status_t) switch_socket_recvfrom(apr_sockaddr_t *from, apr_socket_t *sock,
									apr_int32_t flags, char *buf, 
									apr_size_t *len)
{
	apr_status_t r;

	if ((r = apr_socket_recvfrom(from, sock, flags, buf, len)) == APR_SUCCESS) {
		from->port = ntohs(from->sa.sin.sin_port);
		//from->ipaddr_ptr = &(from->sa.sin.sin_addr);
		//from->ipaddr_ptr = inet_ntoa(from->sa.sin.sin_addr);
	}

	return r;

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

    for (c = RFC2833_CHARS; *c ; c++) {
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
    while(*p) {
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
    while(*p) {
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
	for(x = 0; x < argc; x++) {
		if (*(array[x]) == qc) {
			(array[x])++;
			if ((e = strchr(array[x], qc))) {
				*e = '\0';
			}
		}
	}

	return argc;
}

SWITCH_DECLARE(char *) switch_cut_path(char *in)
{
	char *p, *ret = in;
	char delims[] = "/\\";
	char *i;

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

SWITCH_DECLARE(switch_status_t) switch_socket_create_pollfd(switch_pollfd_t *poll, switch_socket_t *sock,
														  switch_int16_t flags, switch_memory_pool_t *pool)
{
	switch_pollset_t *pollset;

	if (switch_pollset_create(&pollset, 1, pool, flags) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_GENERR;
	}

	poll->desc_type = SWITCH_POLL_SOCKET;
	poll->reqevents = flags;
	poll->desc.s = sock;
	poll->client_data = sock;

	if (switch_pollset_add(pollset, poll) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_GENERR;
	}

	return SWITCH_STATUS_SUCCESS;
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
  
	dest = (char *)malloc(sizeof(char));

	for (i = 0; i < string_len; i++) {
		if (switch_string_match(string + i, string_len - i, search, search_len) == SWITCH_STATUS_SUCCESS) {
			for (n = 0; n < replace_len; n++) {
				dest[dest_len] = replace[n];
				dest_len++;
				dest = (char *)realloc(dest, sizeof(char)*(dest_len+1));
			}
			i += search_len-1;
		} else {
			dest[dest_len] = string[i];
			dest_len++;
			dest = (char *)realloc(dest, sizeof(char)*(dest_len+1));
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

    memset(buf, 0, len);
    for( p = url ; *p ; p++) {
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
			*o = (char)tmp;
			s += 2;
		} else {
			*o = *s;
		}
	}
	*o = '\0';
	return s;
}


#ifdef WIN32
//this forces certain symbols to not be optimized out of the dll
void include_me(void)
{
	apr_socket_shutdown(NULL, 0);
	apr_socket_recvfrom(NULL, NULL, 0, NULL, NULL);
	apr_mcast_join(NULL, NULL, NULL, NULL);
	apr_socket_opt_set(NULL, 0, 0);
}
#endif

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
