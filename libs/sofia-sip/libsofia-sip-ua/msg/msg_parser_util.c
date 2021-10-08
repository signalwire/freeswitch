/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

/**@ingroup msg_parser
 * @CFILE msg_parser_util.c
 *
 * Text-message parser helper functions.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Aug 28 16:26:34 2001 ppessi
 *
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <limits.h>

#include <stdarg.h>
#include <sofia-sip/su_tagarg.h>

#include <sofia-sip/su.h>
#include <sofia-sip/su_alloc.h>
#include <sofia-sip/su_string.h>

#include "msg_internal.h"
#include "sofia-sip/msg_parser.h"
#include "sofia-sip/bnf.h"

#include "sofia-sip/url.h"

static issize_t msg_comma_scanner(char *start);

/**
 * Parse first line.
 *
 * Splits the first line from a message into three whitespace-separated
 * parts.
 */
int msg_firstline_d(char *s, char **return_part2, char **return_part3)
{
  char *s1 = s, *s2, *s3;
  size_t n;

  /* Split line into three segments separated by whitespace */
  if (s1[n = span_non_ws(s1)]) {
    s1[n] = '\0';
    s2 = s1 + n + 1;
    while (IS_WS(*s2))
      s2++;
  }
  else {
    /* Hopeless - no WS in first line */
    return -1;
  }

  n = span_non_ws(s2);

  if (s2[n]) {
    s2[n++] = '\0';
    while (IS_WS(s2[n]))
      n++;
  }

  s3 = s2 + n;

  *return_part2 = s2;

  *return_part3 = s3;

  return 0;
}

/**Parse a token.
 *
 * Parses a token from string pointed by @a *ss. It stores the token value
 * in @a return_token, and updates the @a ss to the end of token and
 * possible whitespace.
 */
issize_t msg_token_d(char **ss, char const **return_token)
{
  char *s = *ss;
  size_t n = span_token(s);
  if (n) {
    for (; IS_LWS(s[n]); n++)
      s[n] = '\0';
    *return_token = s;
    *ss = s + n;
    return n;
  }
  else
    return -1;
}

/** Parse a 32-bit unsigned int.
 *
 * The function msg_uint32_d() parses a 32-bit unsigned integer in string
 * pointed by @a *ss. It stores the value in @a return_token and updates the
 * @a ss to the end of integer and possible whitespace.
 *
 * @retval length of parsed integer, or -1 upon an error.
 */
issize_t msg_uint32_d(char **ss, uint32_t *return_value)
{
  char const *s = *ss, *s0 = s;
  uint32_t value;
  unsigned digit;

  if (!IS_DIGIT(*s))
    return -1;

  for (value = 0; IS_DIGIT(*s); s++) {
    digit = *s - '0';
    if (value > 429496729U)
      return -1;
    else if (value == 429496729U && digit > 5)
      return -1;
    value = 10 * value + digit;
  }

  if (*s) {
    if (!IS_LWS(*s))
      return (issize_t)-1;
    skip_lws(&s);
  }

  *ss = (char *)s;
  *return_value = value;

  return s - s0;
}


/** Parse any list.
 *
 * Parses a list of items, separated by @a sep. The parsed string is passed
 * in @a *ss, which is updated to point to the first non-linear-whitespace
 * character after the list. The function modifies the string as it parses
 * it.
 *
 * The parsed items are appended to the list @a *append_list. If there the
 * list in @a *append_list is NULL, allocate a new list and return it in @a
 * *append_list. Empty list items are ignored, and are not appended to the
 * list.
 *
 * The function @b must be passed a scanning function @a scanner. The
 * scanning function scans for a legitimate list item, for example, a token.
 * It should also compact the list item, for instance, if the item consists
 * of @c name=value parameter definitions it should remove whitespace around
 * "=" sign. The scanning function returns the length of the scanned item,
 * including any linear whitespace after it.
 *
 * @param[in]     home    memory home for allocating list pointers
 * @param[in,out] ss      pointer to pointer to string to be parsed
 * @param[in,out] append_list  pointer to list
 *                             where parsed list items are appended
 * @param[in]     sep     separator character
 * @param[in]     scanner pointer to function for scanning a single item
 *
 * @retval 0  if successful.
 * @retval -1 upon an error.
 */
issize_t msg_any_list_d(su_home_t *home,
			char **ss,
			msg_param_t **append_list,
			issize_t (*scanner)(char *s),
			int sep)
{
  char const *stack[MSG_N_PARAMS];
  char const **list = stack, **re_list;
  size_t N = MSG_N_PARAMS, n = 0;
  issize_t tlen;
  char *s = *ss;
  char const **start;

  if (!scanner)
    return -1;

  if (*append_list) {
    list = *append_list;
    while (list[n])
      n++;
    N = MSG_PARAMS_NUM(n + 1);
  }

  start = &list[n];

  skip_lws(&s);

  while (*s) {
    tlen = scanner(s);

    if (tlen < 0 || (s[tlen] && s[tlen] != sep && s[tlen] != ','))
      goto error;

    if (tlen > 0) {
      if (n + 1 == N) {		/* Reallocate list? */
	N = MSG_PARAMS_NUM(N + 1);
	if (list == stack || list == *append_list) {
	  re_list = su_alloc(home, N * sizeof(*list));
	  if (re_list)
	    memcpy(re_list, list, n * sizeof(*list));
	}
	else
	  re_list = su_realloc(home, list, N * sizeof(*list));
	if (!re_list)
	  goto error;
	list = re_list;
      }

      list[n++] = s;
      s += tlen;
    }

    if (*s == sep) {
      *s++ = '\0';
      skip_lws(&s);
    }
    else if (*s)
      break;
  }

  *ss = s;

  if (n == 0) {
    *append_list = NULL;
    return 0;
  }

  if (list == stack) {
    size_t size = sizeof(*list) * MSG_PARAMS_NUM(n + 1);
    list = su_alloc(home, size);
    if (!list) return -1;
    memcpy((void *)list, stack, n * sizeof(*list));
  }

  list[n] = NULL;
  *append_list = list;
  return 0;

 error:
  *start = NULL;
  if (list != stack && list != *append_list)
    su_free(home, list);
  return -1;
}

/** Scan an attribute (name [= value]) pair.
 *
 * The attribute consists of name (a token) and optional value, separated by
 * equal sign. The value can be a token or quoted string.
 *
 * This function compacts the scanned value. It removes the
 * whitespace around equal sign "=" by moving the equal sign character and
 * value towards name.
 *
 * If there is whitespace within the scanned value or after it,
 * NUL-terminates the scanned attribute.
 *
 * @retval > 0 number of characters scanned,
 *             including the whitespace within the value
 * @retval -1 upon an error
 */
issize_t msg_attribute_value_scanner(char *s)
{
  char *p = s;
  size_t tlen;

  skip_token(&s);

  if (s == p)		/* invalid parameter name */
    return -1;

  tlen = s - p;

  if (IS_LWS(*s)) { *s++ = '\0'; skip_lws(&s); }

  if (*s == '=') {
    char *v;
    s++;
    skip_lws(&s);

    /* get value */
    if (*s == '"') {
      size_t qlen = span_quoted(s);
      if (!qlen)
	return -1;
      v = s; s += qlen;
    }
    else {
      v = s;
      skip_param(&s);
      if (s == v)
	return -1;
    }

    if (p + tlen + 1 != v) {
      memmove(p + tlen + 1, v, s - v);
      p[tlen] = '=';
      p[tlen + 1 + (s - v)] = '\0';
    }
  }

  if (IS_LWS(*s)) { *s++ = '\0'; skip_lws(&s); }

  return s - p;
}

/**Parse an attribute-value list.
 *
 * Parses an attribute-value list, which has syntax as follows:
 * @code
 *  av-list = (av-pair *(";" av-pair)
 *  av-pair = token ["=" ( value / quoted-string) ]        ; optional value
 * @endcode
 *
 * @param[in]     home      pointer to a memory home
 * @param[in,out] ss        pointer to string at the start of parameter list
 * @param[in,out] append_list  pointer to list
 *                             where parsed list items are appended
 *
 * @retval >= 0 if successful
 * @retval -1 upon an error
 */
issize_t msg_avlist_d(su_home_t *home,
		      char **ss,
		      msg_param_t const **append_list)
{
  char const *stack[MSG_N_PARAMS];
  char const **params;
  size_t n = 0, N;
  char *s = *ss;

  if (!*s)
    return -1;

  if (*append_list) {
    params = (char const **)*append_list;
    for (n = 0; params[n]; n++)
      ;
    N = MSG_PARAMS_NUM(n + 1);
  }
  else {
    params = stack;
    N = MSG_PARAMS_NUM(1);
  }

  for (;;) {
    char *p;
    size_t tlen;

    /* XXX - we should handle also quoted parameters */

    skip_lws(&s);
    p = s;
    skip_token(&s);
    tlen = s - p;
    if (!tlen)		/* invalid parameter name */
      goto error;

    if (IS_LWS(*s)) { *s++ = '\0'; skip_lws(&s); }

    if (*s == '=') {
      char *v;
      s++;
      skip_lws(&s);

      /* get value */
      if (*s == '"') {
	size_t qlen = span_quoted(s);
	if (!qlen)
	  goto error;
	v = s; s += qlen;
      }
      else {
	v = s;
	skip_param(&s);
	if (s == v)
	  goto error;
      }
      if (p + tlen + 1 != v) {
	p = memmove(v - tlen - 1, p, tlen);
	p[tlen] = '=';
      }

    }

    if (IS_LWS(*s)) { *s++ = '\0'; skip_lws(&s); }

    if (n == N) {
      /* Reallocate params */
      char const **nparams = su_realloc(home, (void*)(params != stack ? params : NULL),
					(N = MSG_PARAMS_NUM(N + 1)) * sizeof(*params));
      if (!nparams) {
	goto error;
      }
      if (params == stack)
	memcpy(nparams, stack, n * sizeof(*params));
      params = nparams;
    }

    params[n++] = p;

    if (*s != ';')
      break;

    *s++ = '\0';
  }

  *ss = s;

  if (params == stack) {
    size_t size = sizeof(*params) * MSG_PARAMS_NUM(n + 1);
    params = su_alloc(home, size);
    if (!params) return -1;
    memcpy((void *)params, stack, n * sizeof(*params));
  }
  else if (n == N) {
    /* Reallocate params */
    char const **nparams = su_realloc(home, (void*)(params != stack ? params : NULL),
				      (N = MSG_PARAMS_NUM(N + 1)) * sizeof(*params));
    if (!nparams) {
      goto error;
    }
    if (params == stack)
      memcpy(nparams, stack, n * sizeof(*params));
    params = nparams;
  }

  params[n] = NULL;

  *append_list = params;

  return 0;

 error:
  if (params != stack)
    su_free(home, params);
  return -1;
}

/**Parse a semicolon-separated parameter list starting with semicolon.
 *
 * Parse a parameter list, which has syntax as follows:
 * @code
 *  *(";" token [ "=" (token | quoted-string)]).
 * @endcode
 *
 * @param[in]     home      pointer to a memory home
 * @param[in,out] ss        pointer to string at the start of parameter list
 * @param[in,out] append_list  pointer to list
 *                             where parsed list items are appended
 *
 * @retval >= 0 if successful
 * @retval -1 upon an error
 *
 * @sa msg_avlist_d()
 */
issize_t msg_params_d(su_home_t *home,
		      char **ss,
		      msg_param_t const **append_list)
{
  if (**ss == ';') {
    *(*ss)++ = '\0';
    *append_list = NULL;
    return msg_avlist_d(home, ss, append_list);
  }

  if (IS_LWS(**ss)) {
    *(*ss)++ = '\0'; skip_lws(ss);
  }

  return 0;
}

/** Encode a list of parameters */
isize_t msg_params_e(char b[], isize_t bsiz, msg_param_t const pparams[])
{
  int i;
  char *end = b + bsiz, *b0 = b;
  msg_param_t p;

  if (pparams)
    for (i = 0; (p = pparams[i]); i++) {
      if (p[0]) {
	MSG_CHAR_E(b, end, ';');
	MSG_STRING_E(b, end, p);
      }
    }

  return b - b0;
}

/** Duplicate a parameter list */
char *msg_params_dup(msg_param_t const **d, msg_param_t const s[],
		     char *b, isize_t xtra)
{
  char *end = b + xtra;
  char **pp;
  int i;
  isize_t n;

  n = msg_params_count(s);

  if (n == 0) {
    *d = NULL;
    return b;
  }

  MSG_STRUCT_ALIGN(b);
  pp = (char **)b;

  b += sizeof(*pp) * MSG_PARAMS_NUM(n + 1);

  for (i = 0; s[i]; i++) {
    MSG_STRING_DUP(b, pp[i], s[i]);
  }
  pp[i] = NULL;

  assert(b <= end); (void)end;

  *d = (msg_param_t const *)pp;

  return b;
}


/** Parse a comma-separated list.
 *
 * Parses a comma-separated list. The parsed string is passed in @a *ss,
 * which is updated to point to the first non-linear-whitespace character
 * after the list. The function modifies the string as it parses it.
 *
 * A pointer to the resulting list is returned in @a *retval. If there
 * already is a list in @a *retval, new items are appended. Empty list items
 * are ignored, and are not included in the list.
 *
 * The function can be passed an optional scanning function. The scanning
 * function scans for a legitimate list item, for example, a token. It also
 * compacts the list item, for instance, if the item consists of @c
 * name=value parameter definitions.  The scanning function returns the
 * length of the scanned item, including any linear whitespace after it.
 *
 * By default, the scanning function accepts tokens, quoted strings or
 * separators (except comma, of course).
 *
 * @param[in]     home    memory home for allocating list pointers
 * @param[in,out] ss      pointer to pointer to string to be parsed
 * @param[in,out] append_list  pointer to list
 *                             where parsed list items are appended
 * @param[in]     scanner pointer to function scanning a single item
 *                        (optional)
 *
 * @retval 0  if successful.
 * @retval -1 upon an error.
 */
issize_t msg_commalist_d(su_home_t *home,
			 char **ss,
			 msg_param_t **append_list,
			 issize_t (*scanner)(char *s))
{
  scanner = scanner ? scanner : msg_comma_scanner;
  return msg_any_list_d(home, ss, append_list, scanner, ',');
}

/** Token scanner for msg_commalist_d() accepting also empty entries. */
issize_t msg_token_scan(char *start)
{
  char *s = start;
  skip_token(&s);

  if (IS_LWS(*s))
    *s++ = '\0';
  skip_lws(&s);

  return s - start;
}

/** Scan and compact a comma-separated item */
static
issize_t msg_comma_scanner(char *start)
{
  size_t tlen;
  char *s, *p;

  s = p = start;

  if (s[0] == ',')
    return 0;

  for (;;) {
    /* Grab next section - token, quoted string, or separator character */
    char c = *s;

    if (IS_TOKEN(c))
      tlen = span_token(s);
    else if (c == '"')
      tlen = span_quoted(s);
    else /* if (IS_SEPARATOR(c)) */
      tlen = 1;

    if (tlen == 0)
      return -1;

    if (p != s)
      memmove(p, s, tlen);	/* Move section to end of paramexter */
    p += tlen; s += tlen;

    skip_lws(&s);		/* Skip possible LWS */

    if (*s == '\0' || *s == ',') {		/* Test for possible end */
      if (p != s) *p = '\0';
      return s - start;
    }

    if (IS_TOKEN(c) && IS_TOKEN(*s))
      *p++ = ' ';		/* Two tokens must be separated by LWS */
  }
}

/** Parse a comment.
 *
 * Parses a multilevel comment. The comment assigned to return-value
 * parameter @a return_comment is NUL-terminated. The string at return-value
 * parameter @a ss is updated to point to first non-linear-whitespace
 * character after the comment.
 */
issize_t msg_comment_d(char **ss, char const **return_comment)
{
  /* skip comment */
  int level = 1;
  char *s = *ss;

  assert(s[0] == '(');

  if (*s != '(')
    return -1;

  *s++ = '\0';

  if (return_comment)
    *return_comment = s;

  while (level) switch (*s++) {
  case '(': level++; break;
  case ')': level--; break;
  case '\0': /* ERROR */ return -1;
  }

  assert(s[-1] == ')');

  s[-1] = '\0';
  skip_lws(&s);
  *ss = s;

  return 0;
}

/** Parse a quoted string */
issize_t msg_quoted_d(char **ss, char **return_quoted)
{
  char *s= *ss, *s0 = s;
  ssize_t n = span_quoted(s);

  if (n <= 0)
    return -1;

  *return_quoted = s;
  s += n;
  if (IS_LWS(*s)) {
    *s++ = '\0';
    skip_lws(&s);		/* skip linear whitespace */
  }

  *ss = s;

  return s - s0;
}

#if 0
/** Calculate length of string when quoted. */
int msg_quoted_len(char const *u)
{
  int rv;

  if (!u)
    return 0;

  rv = span_token_lws(u);
  if (u[rv]) {
    /* We need to quote string */
    int n;
    int extra = 2; /* quote chars */

    /* Find all characters to quote */
    for (n = strcspn(u + rv, "\\\""); u[rv + n]; rv += n)
      extra++;

    rv += extra;
  }

  return rv;
}
#endif

/**Parse @e host[":"port] pair.
 *
 * Parses a @e host[":"port] pair. The caller passes function a pointer to a
 * string via @a ss, and pointers to which parsed host and port are assigned
 * via @a hhost and @a pport, respectively. The value-result parameter @a
 * *pport must be initialized to default value (e.g., NULL).
 *
 * @param ss    pointer to pointer to string to be parsed
 * @param return_host value-result parameter for @e host
 * @param return_port value-result parameter for @e port

 * @return
 * Returns zero when successful, and a negative value upon an error. The
 * parsed values for host and port are assigned via @a return_host and @a
 * return_port, respectively. The function also updates the pointer @a *ss,
 * so if call is successful, the @a *ss points to first
 * non-linear-whitespace character after @e host[":"port] pair.
 *
 * @note
 * If there is no whitespace after @e port, the value in @a *pport may not be
 * NUL-terminated.  The calling function @b must NUL terminate the value by
 * setting the @a **ss to NUL after first examining its value.
 */
int msg_hostport_d(char **ss,
		   char const **return_host,
		   char const **return_port)
{
  char *host, *s = *ss;
  char *port = NULL;

  /* Host name */
  host = s;
  if (s[0] != '[') {
    skip_token(&s); if (host == s) return -1;
  }
  else {
    /* IPv6 */
    size_t n = strspn(++s, HEX ":.");
    if (s[n] != ']') return -1;
    s += n + 1;
  }

  if (IS_LWS(*s)) { *s++ = '\0'; skip_lws(&s); }

  if (s[0] == ':') {
    unsigned long nport;
    *s++ = '\0'; skip_lws(&s);
    if (!IS_DIGIT(*s))
      return -1;
    port = s;
    nport = strtoul(s, &s, 10);
    if (nport > 65535)
      return -1;
    if (IS_LWS(*s)) {
      *s++ = '\0';
      skip_lws(&s);
    }
  }

  *return_host = host;
  *return_port = port;

  *ss = s;

  return 0;
}

/** Find a header parameter.
 *
 * Searches for given parameter @a name from the header. If parameter is
 * found, it returns a non-NULL pointer to the parameter value. If there is
 * no value for the name (in form "name" or "name=value"), the returned pointer
 * points to a NUL character.
 *
 * @param h     pointer to header structure
 * @param name  parameter name (with or without "=" sign)
 *
 * @return
 * A pointer to parameter value, or NULL if parameter was not found.
 */
char const *msg_header_find_param(msg_common_t const *h, char const *name)
{
  if (h && h->h_class->hc_params) {
    msg_param_t const **params = (msg_param_t const **)
      ((char *)h + h->h_class->hc_params);
    return msg_params_find(*params, name);
  }

  return NULL;
}

/**Modify a parameter value or list item in a header.
 *
 * A header parameter @a param can be just a C-string (@a is_item > 0), or
 * it can have internal format <i>name [ "=" value]</i>. In the latter case,
 * the value part following = is ignored when replacing or removing the
 * parameter.
 *
 * @param home      memory home used to re-allocate parameter list in header
 * @param h         pointer to a header
 * @param param     parameter to be replaced or added
 * @param is_item   how to interpret @a param:
 *                  - 1 case-sensitive, no structure
 *                  - 0 case-insensitive, <i>name [ "=" value ]</i>
 * @param remove_replace_add  what operation to do:
 *                  - -1 remove
 *                  - 0 replace
 *                  - 1 add
 *
 * @retval 1 if parameter was replaced or removed successfully
 * @retval 0 if parameter was added successfully,
 *           or there was nothing to remove
 * @retval -1 upon an error
 */
static
int msg_header_param_modify(su_home_t *home, msg_common_t *h,
			    char const *param,
			    int is_item,
			    int remove_replace_add)
{
  msg_param_t *params, **pointer_to_params;
  size_t plen, n;

  if (!h || !h->h_class->hc_params || !param)
    return -1;

  pointer_to_params = (msg_param_t **)((char *)h + h->h_class->hc_params);
  params = *pointer_to_params;

  plen = is_item > 0 ? strlen(param) : strcspn(param, "=");
  n = 0;

  if (params) {
    /* Existing list, try to replace or remove  */
    for (; params[n]; n++) {
      char const *maybe = params[n];

      if (remove_replace_add > 0)
	continue;

      if (is_item > 0) {
	if (strcmp(maybe, param) == 0) {
	  if (remove_replace_add == 0)
	    return 1;
	}
      }
      else {
	if (su_casenmatch(maybe, param, plen) &&
	    (maybe[plen] == '=' || maybe[plen] == 0))
	  break;
      }
    }
  }

  /* Not found? */
  if (!params || !params[n]) {
    if (remove_replace_add < 0)
      return 0;		/* Nothing to remove */
    else
      remove_replace_add = 1;	/* Add instead of replace */
  }

  if (remove_replace_add < 0) { /* Remove */
    for (; params[n]; n++)
      params[n] = params[n + 1];
  }
  else {
    if (remove_replace_add > 0) { /* Add */
      size_t m_before = MSG_PARAMS_NUM(n + 1);
      size_t m_after =  MSG_PARAMS_NUM(n + 2);

      assert(!params || !params[n]);

      if (m_before != m_after || !params) {
	msg_param_t *p;
	/* XXX - we should know when to do realloc */
	p = su_alloc(home, m_after * sizeof(*p));
	if (!p) return -1;
	if (n > 0)
	  memcpy(p, params, n * sizeof(p[0]));
	*pointer_to_params = params = p;
      }
      params[n + 1] = NULL;
    }

    params[n] = param;	/* Add .. or replace */
  }

  msg_fragment_clear(h);

  if (h->h_class->hc_update) {
    /* Update shortcuts */
    size_t namelen;
    char const *name, *value;

    name = param;
    namelen = strcspn(name, "=");

    if (remove_replace_add < 0)
      value = NULL;
    else
      value = param + namelen + (name[namelen] == '=');

    h->h_class->hc_update(h, name, namelen, value);
  }

  return remove_replace_add <= 0; /* 0 when added, 1 otherwise */
}

/** Add a parameter to a header.
 *
 * You should use this function only when the header accepts multiple
 * parameters (or list items) with the same name. If that is not the case,
 * you should use msg_header_replace_param().
 *
 * @note This function @b does @b not duplicate @p param. The caller should
 * have allocated the @a param from the memory home associated with header
 * @a h.
 *
 * The possible shortcuts to parameter values are updated. For example, the
 * "received" parameter in @Via header has shortcut in structure #sip_via_t,
 * the @ref sip_via_s::v_received "v_received" field. The shortcut is usully
 * a pointer to the parameter value. If the parameter was
 * "received=127.0.0.1" the @ref sip_via_s::v_received "v_received" field
 * would be a pointer to "127.0.0.1". If the parameter was "received=" or
 * "received", the shortcut would be a pointer to an empty string, "".
 *
 * @param home      memory home used to re-allocate parameter list in header
 * @param h         pointer to a header
 * @param param     parameter to be replaced or added
 *
 * @retval 0 if parameter was added
 * @retval -1 upon an error
 *
 * @sa msg_header_replace_param(), msg_header_remove_param(),
 * msg_header_update_params(),
 * #msg_common_t, #msg_header_t,
 * #msg_hclass_t, msg_hclass_t::hc_params, msg_hclass_t::hc_update
 */
int msg_header_add_param(su_home_t *home, msg_common_t *h, char const *param)
{
  return msg_header_param_modify(home, h, param,
				 0 /* case-insensitive name=value */,
				 1 /* add */);
}



/** Replace or add a parameter to a header.
 *
 * A header parameter @a param is a string of format <i>name "=" value</i>
 * or just name. The value part following "=" is ignored when selecting a
 * parameter to replace.
 *
 * @note This function @b does @b not duplicate @p param. The caller should
 * have allocated the @a param from the memory home associated with header
 * @a h.
 *
 * The possible shortcuts to parameter values are updated. For example, the
 * "received" parameter in @Via header has shortcut in structure #sip_via_t,
 * the @ref sip_via_s::v_received "v_received" field.
 *
 * @param home      memory home used to re-allocate parameter list in header
 * @param h         pointer to a header
 * @param param     parameter to be replaced or added
 *
 * @retval 0 if parameter was added
 * @retval 1 if parameter was replaced
 * @retval -1 upon an error
 *
 * @sa msg_header_add_param(), msg_header_remove_param(),
 * msg_header_update_params(),
 * #msg_common_t, #msg_header_t,
 * #msg_hclass_t, msg_hclass_t::hc_params, msg_hclass_t::hc_update
 */
int msg_header_replace_param(su_home_t *home,
			     msg_common_t *h,
			     char const *param)
{
  return msg_header_param_modify(home, h, param,
				 0 /* case-insensitive name=value */,
				 0 /* replace */);
}

/** Remove a parameter from header.
 *
 * The parameter name is given as token optionally followed by "=" sign and
 * value. The "=" and value after it are ignored when selecting a parameter
 * to remove.
 *
 * The possible shortcuts to parameter values are updated. For example, the
 * "received" parameter in @Via header has shortcut in structure #sip_via_t,
 * the @ref sip_via_s::v_received "v_received" field. The shortcut to
 * removed parameter would be set to NULL.
 *
 * @param h         pointer to a header
 * @param name      name of parameter to be removed
 *
 * @retval 1 if a parameter was removed
 * @retval 0 if no parameter was not removed
 * @retval -1 upon an error
 *
 * @sa msg_header_add_param(), msg_header_replace_param(),
 * msg_header_update_params(),
 * #msg_common_t, #msg_header_t,
 * #msg_hclass_t, msg_hclass_t::hc_params, msg_hclass_t::hc_update
 */
int msg_header_remove_param(msg_common_t *h, char const *name)
{
  return msg_header_param_modify(NULL, h, name,
				 0 /* case-insensitive name=value */,
				 -1 /* remove */);
}

/** Update shortcuts to parameter values.
 *
 * Update the shortcuts to parameter values in parameter list. For example,
 * the "received" parameter in @Via header has shortcut in structure
 * #sip_via_t, the @ref sip_via_s::v_received "v_received" field. The
 * shortcut is usully a pointer to the parameter value. If the parameter was
 * "received=127.0.0.1" the @ref sip_via_s::v_received "v_received" field
 * would be a pointer to "127.0.0.1". If the parameter was "received=" or
 * "received", the shortcut would be a pointer to an empty string, "".
 *
 * @retval 0 when successful
 * @retval -1 upon an error
 *
 * @sa msg_header_add_param(), msg_header_replace_param(),
 * msg_header_update_params(),
 * #msg_common_t, #msg_header_t,
 * #msg_hclass_t, msg_hclass_t::hc_params, msg_hclass_t::hc_update
 */
int msg_header_update_params(msg_common_t *h, int clear)
{
  msg_hclass_t *hc;
  unsigned char offset;
  msg_update_f *update;
  int retval;
  msg_param_t const *params;
  size_t n;
  char const *p, *v;

  if (h == NULL)
    return errno = EFAULT, -1;

  hc = h->h_class; offset = hc->hc_params; update = hc->hc_update;

  if (offset == 0 || update == NULL)
    return 0;

  if (clear)
    update(h, NULL, 0, NULL);

  params = *(msg_param_t **)((char *)h + offset);
  if (params == NULL)
    return 0;

  retval = 0;

  for (p = *params; p; p = *++params) {
    n = strcspn(p, "=");
    v = p + n + (p[n] == '=');
    if (update(h, p, n, v) < 0)
      retval = -1;
  }

  return retval;
}


/** Find a header item.
 *
 * Searches for given item @a name from the header. If item is found, the
 * function returns a non-NULL pointer to the item.
 *
 * @param h     pointer to header structure
 * @param item  item
 *
 * @return
 * A pointer to item, or NULL if it was not found.
 *
 * @since New in @VERSION_1_12_4
 *
 * @sa msg_header_replace_item(), msg_header_remove_item(),
 * @Allow, @AllowEvents
 */
char const *msg_header_find_item(msg_common_t const *h, char const *item)
{
  if (h && h->h_class->hc_params) {
    char const * const * items =
      *(char const * const * const *)
      ((char *)h + h->h_class->hc_params);

    if (items)
      for (; *items; items++) {
	if (strcmp(item, *items) == 0) {
	  return *items;
	}
      }
  }

  return NULL;
}


/**Add an item to a header.
 *
 * This function treats a #msg_header_t as set of C strings. The @a item is
 * a C string. If no identical string is found from the list, it is added to
 * the list.
 *
 * The shortcuts, if any, to item values are updated accordingly.
 *
 * @param home      memory home used to re-allocate list in header
 * @param h         pointer to a header
 * @param item      item to be removed
 *
 * @retval 0 if item was added
 * @retval 1 if item was replaced
 * @retval -1 upon an error
 *
 * @since New in @VERSION_1_12_4.
 *
 * @sa msg_header_remove_item(), @Allow, @AllowEvents,
 * msg_header_replace_param(), msg_header_remove_param(),
 * #msg_common_t, #msg_header_t, #msg_list_t
 * #msg_hclass_t, msg_hclass_t::hc_params
 */
int msg_header_replace_item(su_home_t *home,
			    msg_common_t *h,
			    char const *item)
{
  return msg_header_param_modify(home, h, item,
				 1 /* string item */,
				 0 /* replace */);
}

/**Remove an item from a header.
 *
 * This function treats a #msg_header_t as set of C strings. The @a item is a
 * C string. If identical string is found from the list, it is removed.
 *
 * The shortcuts, if any, to item values are updated accordingly.
 *
 * @param h        pointer to a header
 * @param name     item to be removed
 *
 * @retval 0 if item was added
 * @retval 1 if item was replaced
 * @retval -1 upon an error
 *
 * @since New in @VERSION_1_12_4.
 *
 * @sa msg_header_replace_item(), @Allow, @AllowEvents,
 * msg_header_replace_param(), msg_header_remove_param(),
 * #msg_common_t, #msg_header_t, #msg_list_t
 * #msg_hclass_t, msg_hclass_t::hc_params
 */
int msg_header_remove_item(msg_common_t *h, char const *name)
{
  return msg_header_param_modify(NULL, h, name,
				 1 /* item */,
				 -1 /* remove */);
}


/** Find a parameter from a parameter list.
 *
 * Searches for given parameter @a token from the parameter list. If
 * parameter is found, it returns a non-NULL pointer to the parameter value.
 * If there is no value for the parameter (the parameter is of form "name"
 * or "name="), the returned pointer points to a NUL character.
 *
 * @param params list (or vector) of parameters
 * @param token  parameter name (with or without "=" sign)
 *
 * @return
 * A pointer to parameter value, or NULL if parameter was not found.
 */
msg_param_t msg_params_find(msg_param_t const params[], msg_param_t token)
{
  if (params && token) {
    size_t i, n = strcspn(token, "=");

    assert(n > 0);

    for (i = 0; params[i]; i++) {
      msg_param_t param = params[i];
      if (su_casenmatch(param, token, n)) {
	if (param[n] == '=')
	  return param + n + 1;
        else if (param[n] == 0)
	  return param + n;
      }
    }
  }

  return NULL;
}

/** Find a slot for parameter from a parameter list.
 *
 * Searches for given parameter @a token from the parameter list. If
 * parameter is found, it returns a non-NULL pointer to the item containing
 * the parameter.
 *
 * @param params list (or vector) of parameters
 * @param token  parameter name (with or without "=" sign)
 *
 * @return
 * A pointer to parameter slot, or NULL if parameter was not found.
 */
msg_param_t *msg_params_find_slot(msg_param_t params[], msg_param_t token)
{
  if (params && token) {
    int i;
	size_t n = strlen(token);

    assert(n > 0);

    for (i = 0; params[i]; i++) {
      msg_param_t param = params[i];
      if (su_casenmatch(param, token, n)) {
	if (param[n] == '=')
	  return params + i;
        else if (param[n] == 0 || token[n - 1] == '=')
	  return params + i;
      }
    }

  }

  return NULL;
}

/** Replace or add a parameter from a list.
 *
 * A non-NULL parameter list must have been created by msg_params_d()
 * or by msg_params_dup().
 *
 * @note This function does not duplicate @p param.
 *
 * @param home      memory home
 * @param inout_params   pointer to pointer to parameter list
 * @param param     parameter to be replaced or added
 *
 * @retval 0 if parameter was added
 * @retval 1 if parameter was replaced
 * @retval -1 upon an error
 */
int msg_params_replace(su_home_t *home,
		       msg_param_t **inout_params,
		       msg_param_t param)
{
  msg_param_t *params;
  size_t i, n;

  assert(inout_params);

  if (param == NULL || param[0] == '=' || param[0] == '\0')
    return -1;

  params = *inout_params;

  n = strcspn(param, "=");

  if (params) {
    /* Existing list, try to replace or remove  */
    for (i = 0; params[i]; i++) {
      msg_param_t maybe = params[i];

      if (su_casenmatch(maybe, param, n)) {
	if (maybe[n] == '=' || maybe[n] == 0) {
	  params[i] = param;
	  return 1;
	}
      }
    }
  }

  /* Not found on list */
  return msg_params_add(home, inout_params, param);
}

/** Remove a parameter from a list.
 *
 * @retval 1 if parameter was removed
 * @retval 0 if parameter was not found
 * @retval -1 upon an error
 */
int msg_params_remove(msg_param_t *params, msg_param_t param)
{
  size_t i, n;

  if (!params || !param || !param[0])
    return -1;

  n = strcspn(param, "=");
  assert(n > 0);

  for (i = 0; params[i]; i++) {
    msg_param_t maybe = params[i];

    if (su_casenmatch(maybe, param, n)) {
      if (maybe[n] == '=' || maybe[n] == 0) {
	/* Remove */
	do {
	  params[i] = params[i + 1];
	} while (params[i++]);
	return 1;
      }
    }
  }

  return 0;
}

/** Calculate number of parameters in a parameter list */
size_t msg_params_length(char const * const * params)
{
  size_t len;

  if (!params)
    return 0;

  for (len = 0; params[len]; len++)
    ;

  return len;
}


/**
 * Add a parameter to a list.
 *
 * Add a parameter to the list; the list must have been created by @c
 * msg_params_d() or by @c msg_params_dup() (or it may contain only @c
 * NULL).
 *
 * @note This function does not duplicate @p param.
 *
 * @param home      memory home
 * @param inout_params   pointer to pointer to parameter list
 * @param param     parameter to be added
 *
 * @retval 0 if parameter was added
 * @retval -1 upon an error
 */
int msg_params_add(su_home_t *home,
		   msg_param_t **inout_params,
		   msg_param_t param)
{
  size_t n, m_before, m_after;
  msg_param_t *p = *inout_params;

  if (param == NULL)
    return -1;

  /* Count number of parameters */
  for (n = 0; p && p[n]; n++)
    ;

  m_before = MSG_PARAMS_NUM(n + 1);
  m_after =  MSG_PARAMS_NUM(n + 2);

  if (m_before != m_after || !p) {
    p = su_alloc(home, m_after * sizeof(*p));
    assert(p); if (!p) return -1;
    if (n)
      memcpy(p, *inout_params, n * sizeof(*p));
    *inout_params = p;
  }

  p[n] = param;
  p[n + 1] = NULL;

  return 0;
}

static
int msg_param_prune(msg_param_t const d[], msg_param_t p, unsigned prune)
{
  size_t i, nlen;

  if (prune == 1)
    nlen = strcspn(p, "=");
  else
    nlen = 0;

  for (i = 0; d[i]; i++) {
    if ((prune == 1 &&
	 su_casenmatch(p, d[i], nlen)
	 && (d[i][nlen] == '=' || d[i][nlen] == '\0'))
	||
	(prune == 2 && su_casematch(p, d[i]))
	||
	(prune == 3 && strcmp(p, d[i]) == 0))
      return 1;
  }

  return 0;
}

/**Join two parameter lists.
 *
 * The function @c msg_params_join() joins two parameter lists; the
 * first list must have been created by @c msg_params_d() or by @c
 * msg_params_dup() (or it may contain only @c NULL).
 *
 * @param home    memory home
 * @param dst     pointer to pointer to destination parameter list
 * @param src     source list
 * @param prune   prune duplicates
 * @param dup     duplicate parameters in src list
 *
 * @par Pruning
 * <table>
 * <tr><td>0<td>do not prune</tr>
 * <tr><td>1<td>prune parameters with identical names</tr>
 * <tr><td>2<td>case-insensitive values</tr>
 * <tr><td>3<td>case-sensitive values</tr>
 * </table>
 *
 * @return
 * @retval >= 0 when successful
 * @retval -1 upon an error
 */
issize_t msg_params_join(su_home_t *home,
			 msg_param_t **dst,
			 msg_param_t const *src,
			 unsigned prune,
			 int dup)
{
  size_t n, m, n_before, n_after, pruned, total = 0;
  msg_param_t *d = *dst;

  if (prune > 3)
    return -1;

  if (src == NULL || *src == NULL)
    return 0;

  /* Count number of parameters */
  for (n = 0; d && d[n]; n++)
    ;

  n_before = MSG_PARAMS_NUM(n + 1);

  for (m = 0, pruned = 0; src[m]; m++) {
    if (n > 0 && prune > 0 && msg_param_prune(d, src[m], prune)) {
      pruned++;
      if (prune > 1)
	continue;
    }
    if (dup)
      total += strlen(src[m]) + 1;
  }

  n_after = MSG_PARAMS_NUM(n + m - pruned + 1);

  if (n_before != n_after || !d) {
    d = su_alloc(home, n_after * sizeof(*d));
    assert(d); if (!d) return -1;
    if (n)
      memcpy(d, *dst, n * sizeof(*d));
    *dst = d;
  }

  for (m = 0; src[m]; m++) {
    if (pruned && msg_param_prune(d, src[m], prune)) {
      pruned--;
      if (prune > 1)
	continue;
    }

    if (dup)
      d[n++] = su_strdup(home, src[m]);	/* XXX */
    else
      d[n++] = src[m];
  }

  d[n] = NULL;

  return 0;
}

/**Join header item lists.
 *
 * Join items from a source header to the destination header. The item are
 * compared with the existing ones. If a match is found, it is not added to
 * the list. If @a duplicate is true, the entries are duplicated while they
 * are added to the list.
 *
 * @param home       memory home
 * @param dst        destination header
 * @param src        source header
 * @param duplicate  if true, allocate and copy items that are added
 *
 * @return
 * @retval >= 0 when successful
 * @retval -1 upon an error
 *
 * @NEW_1_12_5.
 */
int msg_header_join_items(su_home_t *home,
			  msg_common_t *dst,
			  msg_common_t const *src,
			  int duplicate)
{
  size_t N, m, M, i, n_before, n_after, total;
  char *dup = NULL;
  msg_param_t *d, **dd, *s;
  msg_param_t t, *temp, temp0[16];
  size_t *len, len0[(sizeof temp0)/(sizeof temp0[0])];
  msg_update_f *update = NULL;

  if (dst == NULL || dst->h_class->hc_params == 0 ||
      src == NULL || src->h_class->hc_params == 0)
    return -1;

  s = *(msg_param_t **)((char *)src + src->h_class->hc_params);
  if (s == NULL)
    return 0;

  for (M = 0; s[M]; M++)
    {}

  if (M == 0)
    return 0;

  if (M <= (sizeof temp0) / (sizeof temp0[0])) {
    temp = temp0, len = len0;
  }
  else {
    temp = malloc(M * (sizeof *temp) + M * (sizeof *len));
    if (!temp) return -1;
    len = (void *)(temp + M);
  }

  dd = (msg_param_t **)((char *)dst + dst->h_class->hc_params);
  d = *dd;

  for (N = 0; d && d[N]; N++)
    {}

  for (m = 0, M = 0, total = 0; s[m]; m++) {
    t = s[m];
    for (i = 0; i < N; i++)
      if (strcmp(t, d[i]) == 0)
	break;
    if (i < N)
      continue;

    for (i = 0; i < M; i++)
      if (strcmp(t, temp[i]) == 0)
	break;
    if (i < M)
      continue;

    temp[M] = t;
    len[M] = strlen(t);
    total += len[M++] + 1;
  }

  if (M == 0)
    goto success;

  dup = su_alloc(home, total); if (!dup) goto error;

  n_before = MSG_PARAMS_NUM(N + 1);
  n_after = MSG_PARAMS_NUM(N + M + 1);

  if (d == NULL || n_before != n_after) {
    d = su_alloc(home, n_after * sizeof(*d)); if (!d) goto error;
    if (N)
      memcpy(d, *dd, N * sizeof(*d));
    *dd = d;
  }

  update = dst->h_class->hc_update;

  for (m = 0; m < M; m++) {
    d[N++] = memcpy(dup, temp[m], len[m] + 1);

    if (update)
      update(dst, dup, len[m], dup + len[m]);

    dup += len[m] + 1;
  }

  d[N] = NULL;

 success:
  if (temp != temp0)
    free(temp);
  return 0;

 error:
  if (temp != temp0)
    free(temp);
  su_free(home, dup);
  return -1;
}

/**Compare parameter lists.
 *
 * Compares parameter lists.
 *
 * @param a pointer to a parameter list
 * @param b pointer to a parameter list
 *
 * @retval an integer less than zero if @a is less than @a b
 * @retval an integer zero if @a match with @a b
 * @retval an integer greater than zero if @a is greater than @a b
 */
int msg_params_cmp(char const * const a[], char const * const b[])
{
  int c;
  size_t nlen;

  if (a == NULL || b == NULL)
    return (a != NULL) - (b != NULL);

  for (;;) {
    if (*a == NULL || *b == NULL)
      return (*a != NULL) - (*b != NULL);
    nlen = strcspn(*a, "=");
    if ((c = su_strncasecmp(*a, *b, nlen)))
      return c;
    if ((c = strcmp(*a + nlen, *b + nlen)))
      return c;
    a++, b++;
  }
}


/** Unquote string
 *
 * Duplicates the string @a q in unquoted form.
 */
char *msg_unquote_dup(su_home_t *home, char const *q)
{
  char *d;
  size_t total, n, m;

  /* First, easy case */
  if (q[0] == '"')
    q++;
  n = strcspn(q, "\"\\");
  if (q[n] == '\0' || q[n] == '"')
    return su_strndup(home, q, n);

  /* Hairy case - backslash-escaped chars */
  total = n;
  for (;;) {
    if (q[n] == '\0' || q[n] == '"' || q[n + 1] == '\0')
      break;
    m = strcspn(q + n + 2, "\"\\");
    total += m + 1;
    n += m + 2;
  }

  if (!(d = su_alloc(home, total + 1)))
    return NULL;

  for (n = 0;;) {
    m = strcspn(q, "\"\\");
    memcpy(d + n, q, m);
    n += m, q += m;
    if (q[0] == '\0' || q[0] == '"' || q[1] == '\0')
      break;
    d[n++] = q[1];
    q += 2;
  }
  assert(total == n);
  d[n] = '\0';

  return d;
}

/** Unquote string */
char *msg_unquote(char *dst, char const *s)
{
  int copy = dst != NULL;
  char *d = dst;

  if (*s++ != '"')
    return NULL;

  for (;;) {
    size_t n = strcspn(s, "\"\\");
    if (copy)
      memmove(d, s, n);
    s += n;
    d += n;

    if (*s == '\0')
      return NULL;
    else if (*s == '"') {
      if (copy) *d = '\0';
      return dst;
    }
    else {
      /* Copy quoted char */
      if ((copy ? (*d++ = *++s) : *++s) == '\0')
	return NULL;
      s++;
    }
  }
}

/** Quote string */
issize_t msg_unquoted_e(char *b, isize_t bsiz, char const *s)
{
  isize_t e = 0;

  if (b == NULL)
    bsiz = 0;

  if (0 < bsiz)
    *b = '"';
  e++;

  for (;*s;) {
    size_t n = strcspn(s, "\"\\");

    if (n == 0) {
      if (e + 2 <= bsiz)
	b[e] = '\\', b[e + 1] = s[0];
      e += 2;
      s++;
    }
    else {
      if (e + n <= bsiz)
	memcpy(b + e, s, n);
      e += n;
      s += n;
    }
  }

  if (e < bsiz)
    b[e] = '"';
  e++;

  return e;
}


/** Calculate a simple hash over a string. */
unsigned long msg_hash_string(char const *id)
{
  unsigned long hash = 0;

  if (id)
    for (; *id; id++) {
      hash += (unsigned)*id;
      hash *= 38501U;
    }
  else
    hash *= 38501U;

  if (hash == 0)
    hash = (unsigned long)-1;

  return hash;
}


/** Calculate the size of a duplicate of a header structure. */
isize_t msg_header_size(msg_header_t const *h)
{
  if (h == NULL || h == MSG_HEADER_NONE)
    return 0;
  else
    return h->sh_class->hc_dxtra(h, h->sh_class->hc_size);
}


/** Encode a message to the buffer.
 *
 * The function msg_encode_e encodes a message to a given buffer.
 * It returns the length of the message to be encoded, even if the
 * buffer is too small (just like snprintf() is supposed to do).
 *
 * @param b        buffer (may be NULL)
 * @param size     size of buffer
 * @param mo       public message structure (#sip_t, #http_t)
 * @param flags    see #
 */
issize_t msg_object_e(char b[], isize_t size, msg_pub_t const *mo, int flags)
{
  size_t rv = 0;
  ssize_t n;
  msg_header_t const *h;

  if (mo->msg_request)
    h = mo->msg_request;
  else
    h = mo->msg_status;

  for (; h; h = h->sh_succ) {
    n = msg_header_e(b, size, h, flags);
    if (n < 0)
      return -1;
    if ((size_t)n < size)
      b += n, size -= n;
    else
      b = NULL, size = 0;
    rv += n;
  }

  return rv;
}

/** Encode header contents. */
issize_t msg_header_field_e(char b[], isize_t bsiz, msg_header_t const *h, int flags)
{
  assert(h); assert(h->sh_class);

  return h->sh_class->hc_print(b, bsiz, h, flags);
}

/** Get offset of header @a h from structure @a mo. */
msg_header_t **
msg_header_offset(msg_t const *msg, msg_pub_t const *mo, msg_header_t const *h)
{
  if (h == NULL || h->sh_class == NULL)
    return NULL;

  return msg_hclass_offset(msg->m_class, mo, h->sh_class);
}

/**Get a header from the public message structure.
 *
 * Gets a pointer to header from a message structure.
 *
 * @param pub public message structure from which header is obtained
 * @param hc  header class
 */
msg_header_t *
msg_header_access(msg_pub_t const *pub, msg_hclass_t *hc)
{
  msg_header_t * const * hh;

  if (pub == NULL || hc == NULL)
    return NULL;

  hh = msg_hclass_offset((void *)pub->msg_ident, (msg_pub_t *)pub, hc);

  if (hh)
    return *hh;
  else
    return NULL;
}

#include <sofia-sip/su_uniqueid.h>

/** Generates a random token.
 *
 */
issize_t msg_random_token(char token[], isize_t tlen,
			  void const *rmemp, isize_t rsize)
{
  uint32_t random = 0, rword;
  uint8_t rbyte;
  uint8_t const *rmem = rmemp;
  size_t i;
  ssize_t n;

  static char const token_chars[33] =
    /* Create aesthetically pleasing raNDom capS LooK */
    "aBcDeFgHjKmNpQrStUvXyZ0123456789";

  if (rmem == NULL && rsize == 0)
    rsize = UINT_MAX;

  if (rsize == 0) {
    if (token && tlen > 0)
      strcpy(token, "+");
    return 1;
  }

  if (token == NULL) {
    if (rsize >= tlen * 5 / 8)
      return tlen;
    else
      return rsize / 5 * 8;
  }

  for (i = 0, n = 0; i < tlen;) {
    if (n < 5) {
      if (rsize == 0)
	;
      else if (rmem) {
	rbyte = *rmem++, rsize--;
	random = random | (rbyte << n);
	n += 8;
      } else {
	rword = su_random();
	random = (rword >> 13) & 31;
	n = 6;
      }
    }

    token[i] = token_chars[random & 31];
    random >>= 5;
    i++, n -= 5;

    if (n < 0 || (n == 0 && rsize == 0))
      break;
  }

  token[i] = 0;

  return i;
}


/** Parse a message.
 *
 * Parse a text message with parser @a mc. The @a data is copied and it is
 * not modified or referenced by the parsed message.
 *
 * @par Example
 * Parse a SIP message fragment (e.g., payload of NOTIFY sent in response to
 * REFER):
 * @code
 * msg_t *m = msg_make(sip_default_mclass(), 0, pl->pl_data, pl->pl_len);
 * sip_t *frag = sip_object(m);
 * @endcode
 *
 * @param mc message class (parser table)
 * @param flags message flags (see #msg_flg_user)
 * @param data message text
 * @param len size of message text (if -1, use strlen(data))
 *
 * @retval A pointer to a freshly allocated and parsed message.
 *
 * Upon parsing error, the header structure may be left incomplete. The
 * #MSG_FLG_ERROR is set in @a msg_object(msg)->msg_flags.
 *
 * @since New in @VERSION_1_12_4
 *
 * @sa msg_as_string(), msg_extract()
 */
msg_t *msg_make(msg_mclass_t const *mc, int flags,
		void const *data, ssize_t len)
{
  msg_t *msg;
  msg_iovec_t iovec[2];

  if (len == -1)
    len = strlen(data);
  if (len == 0)
    return NULL;

  msg = msg_create(mc, flags);
  if (msg == NULL)
    return NULL;

  su_home_preload(msg_home(msg), 1, len + 1024);

  if (msg_recv_iovec(msg, iovec, 2, len, 1) < 0) {
    perror("msg_recv_iovec");
  }
  assert((ssize_t)iovec->mv_len == len);
  memcpy(iovec->mv_base, data, len);
  msg_recv_commit(msg, len, 1);

  if (msg_extract(msg) < 0)
    msg->m_object->msg_flags |= MSG_FLG_ERROR;

  return msg;
}
