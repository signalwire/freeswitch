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
 * Seven Du <dujinfang@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 *
 * switch_utf8.h UTf8
 *
 */


/*
  Basic UTF-8 manipulation routines
  by Jeff Bezanson
  placed in the public domain Fall 2005

  This code is designed to provide the utilities you need to manipulate
  UTF-8 as an internal string encoding. These functions do not perform the
  error checking normally needed when handling UTF-8 data, so if you happen
  to be from the Unicode Consortium you will want to flay me alive.
  I do this because error checking can be performed at the boundaries (I/O),
  with these routines reserved for higher performance on data known to be
  valid.
*/

#include <switch.h>

/* is c the start of a utf8 sequence? */
#define isutf(c) (((c)&0xC0)!=0x80)

/* convert UTF-8 data to wide character */
SWITCH_DECLARE(int) switch_u8_toucs(uint32_t *dest, int sz, char *src, int srcsz);

/* the opposite conversion */
SWITCH_DECLARE(int) switch_u8_toutf8(char *dest, int sz, uint32_t *src, int srcsz);

/* single character to UTF-8 */
SWITCH_DECLARE(int) switch_u8_wc_toutf8(char *dest, uint32_t ch);

/* character number to byte offset */
SWITCH_DECLARE(int) switch_u8_offset(char *str, int charnum);

/* byte offset to character number */
SWITCH_DECLARE(int) switch_u8_charnum(char *s, int offset);

/* return next character, updating an index variable */
SWITCH_DECLARE(uint32_t) switch_u8_nextchar(char *s, int *i);

/* move to next character */
SWITCH_DECLARE(void) switch_u8_inc(char *s, int *i);

/* move to previous character */
SWITCH_DECLARE(void) switch_u8_dec(char *s, int *i);

/* returns length of next utf-8 sequence */
SWITCH_DECLARE(int) switch_u8_seqlen(char *s);

/* assuming src points to the character after a backslash, read an
   escape sequence, storing the result in dest and returning the number of
   input characters processed */
SWITCH_DECLARE(int) switch_u8_read_escape_sequence(char *src, uint32_t *dest);

/* given a wide character, convert it to an ASCII escape sequence stored in
   buf, where buf is "sz" bytes. returns the number of characters output.*/
SWITCH_DECLARE(int) switch_u8_escape_wchar(char *buf, int sz, uint32_t ch);

/* convert a string "src" containing escape sequences to UTF-8 */
SWITCH_DECLARE(int) switch_u8_unescape(char *buf, int sz, char *src);

/* convert UTF-8 "src" to ASCII with escape sequences.
   if escape_quotes is nonzero, quote characters will be preceded by
   backslashes as well. */
SWITCH_DECLARE(int) switch_u8_escape(char *buf, int sz, char *src, int escape_quotes);

/* utility predicates used by the above */
int octal_digit(char c);
int hex_digit(char c);

/* return a pointer to the first occurrence of ch in s, or NULL if not
   found. character index of found character returned in *charn. */
SWITCH_DECLARE(char *) switch_u8_strchr(char *s, uint32_t ch, int *charn);

/* same as the above, but searches a buffer of a given size instead of
   a NUL-terminated string. */
SWITCH_DECLARE(char *) switch_u8_memchr(char *s, uint32_t ch, size_t sz, int *charn);

/* count the number of characters in a UTF-8 string */
SWITCH_DECLARE(int) switch_u8_strlen(char *s);

SWITCH_DECLARE(int) switch_u8_is_locale_utf8(char *locale);

SWITCH_DECLARE(uint32_t) switch_u8_get_char(char *s, int *i);
