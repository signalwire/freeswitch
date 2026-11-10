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
 * Module Contributor(s):
 *  Konstantin Alexandrin <akscfx@gmail.com>
 *
 *
 */
#include "mod_google_tts.h"

char *lang2bcp47(const char *lng) {
    if(strcasecmp(lng, "en") == 0) { return "en-gb"; }
    if(strcasecmp(lng, "de") == 0) { return "de-de"; }
    if(strcasecmp(lng, "es") == 0) { return "es-es"; }
    if(strcasecmp(lng, "it") == 0) { return "it-it"; }
    if(strcasecmp(lng, "ru") == 0) { return "ru-ru"; }
    return (char *)lng;
}

char *fmt_gender(const char *gender) {
    if(strcasecmp(gender, "male") == 0)     { return "MALE"; }
    if(strcasecmp(gender, "female") == 0)   { return "FEMALE"; }
    return (char *)gender;
}

char *fmt_encode(const char *fmt) {
    if(strcasecmp(fmt, "mp3") == 0)  { return "MP3"; }
    if(strcasecmp(fmt, "wav") == 0)  { return "LINEAR16"; }
    if(strcasecmp(fmt, "ulaw") == 0) { return "MULAW"; }
    if(strcasecmp(fmt, "alaw") == 0) { return "ALAW"; }
    return (char *)fmt;
}

char *fmt_enct2fext(const char *fmt) {
    if(strcasecmp(fmt, "mp3") == 0)      { return "mp3"; }
    if(strcasecmp(fmt, "linear16") == 0) { return "wav"; }
    if(strcasecmp(fmt, "mulaw") == 0)    { return "ulaw"; }
    if(strcasecmp(fmt, "alaw") == 0)     { return "alaw"; }
    return (char *)fmt;
}

char *escape_squotes(const char *string) {
    size_t string_len = strlen(string);
    size_t i;
    size_t n = 0;
    size_t dest_len = 0;
    char *dest;

    dest_len = strlen(string) + 1;
    for (i = 0; i < string_len; i++) {
        switch (string[i]) {
            case '\'': dest_len += 1; break;
        }
    }

    dest = (char *) malloc(sizeof(char) * dest_len);
    switch_assert(dest);

    for (i = 0; i < string_len; i++) {
        switch (string[i]) {
            case '\'':
                dest[n++] = '\\';
                dest[n++] = '\'';
            break;
            default:
                dest[n++] = string[i];
        }
    }
    dest[n++] = '\0';

    switch_assert(n == dest_len);
    return dest;
}

/*-
 * Copyright (c) 2001 Mike Barcroft <mike@FreeBSD.org>
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 */
char *strnstr(const char *s, const char *find, size_t slen) {
	char c, sc;
	size_t len;

	if ((c = *find++) != '\0') {
		len = strlen(find);
		do {
			do {
				if (slen-- < 1 || (sc = *s++) == '\0')
					return (NULL);
			} while (sc != c);
			if (len > slen)
				return (NULL);
		} while (strncmp(s, find, len) != 0);
		s--;
	}

	return ((char *)s);
}

