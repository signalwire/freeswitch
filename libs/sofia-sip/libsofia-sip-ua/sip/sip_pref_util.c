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

/**@CFILE sip_pref_util.c
 *
 * SIP callercaps and callerprefs utility functions.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Tue Nov  2 16:39:33 EET 2004 ppessi
 */

#include "config.h"

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <float.h>

#include "sofia-sip/sip_parser.h"
#include <sofia-sip/sip_header.h>
#include <sofia-sip/sip_util.h>
#include <sofia-sip/sip_status.h>

static double parse_number(char const *str, char **return_end);

/** Parse a single preference */
int sip_prefs_parse(union sip_pref *sp,
		    char const **in_out_s,
		    int *return_negation)
{
  char const *s;
  size_t n;
  enum sp_type old_type;

  assert(sp && in_out_s && *in_out_s && return_negation);

  old_type = sp->sp_type;
  sp->sp_type = sp_error;

  s = *in_out_s;
  if (!s)
    return 0;

  if (old_type == sp_init) {
    if (s[0] == '\0' ||
	su_casematch(s, "TRUE") ||
	su_casematch(s, "\"TRUE\"")) {
      /* Boolean */
      sp->sp_type = sp_literal;
      sp->sp_literal.spl_value = "TRUE";
      sp->sp_literal.spl_length = 4;
      *return_negation = 0;
      *in_out_s = s + strlen(s);
      return 1;
    } else if (su_casematch(s, "FALSE") ||
	       su_casematch(s, "\"FALSE\"")) {
      /* Boolean */
      sp->sp_type = sp_literal;
      sp->sp_literal.spl_value = "FALSE";
      sp->sp_literal.spl_length = 5;
      *return_negation = 0;
      *in_out_s = s + strlen(s);
      return 1;
    } else if (s[0] == '"' && s[1] != '\0') {
      for (s++; IS_LWS(s[0]); s++)
        {}
    } else
      old_type = sp_error;
  } else if (!s[0]) {
    sp->sp_type = sp_init;
    return 0;
  }

  if (old_type == sp_error)
    return 0;

  if ((*return_negation = s[0] == '!'))
      for (s++; IS_LWS(s[0]); s++)
        {}

  if (*s == '#') {
    /* Numeric */
    double n1, n2;
    char s0, *e;

    for (s++; IS_LWS(s[0]); s++)
      {}

    s0 = s[0];

    if (s0 == '=')
      sp->sp_type = sp_range, n1 = n2 = parse_number(s = s + 1, &e);
    else if (s0 == '<' && s[1] == '=')
      sp->sp_type = sp_range, n1 = -DBL_MAX, n2 = parse_number(s = s + 2, &e);
    else if (s0 == '>' && s[1] == '=')
      sp->sp_type = sp_range, n1 = parse_number(s = s + 2, &e), n2 = DBL_MAX;
    else if (((n1 = parse_number(s, &e)) != 0.0 || s != e) && e[0] == ':')
      sp->sp_type = sp_range, n2 = parse_number(s = e + 1, &e);
    else
      /* Error in conversion */
      sp->sp_type = sp_error, n1 = DBL_MAX, n2 = -DBL_MAX;

    if (s == e && (n1 == 0.0 || n2 == 0.0))
      sp->sp_type = sp_error;      /* Error in conversion */

    sp->sp_range.spr_lower = n1;
    sp->sp_range.spr_upper = n2;

    s = e;
  } else if (*s == '<') {
    /* Quoted string */
    n = strcspn(++s, ">");
    sp->sp_type = sp_string;
    sp->sp_string.sps_value = s;
    sp->sp_string.sps_length = n;
    s += n + 1;
  } else if ((n = span_token(s))) {
    /* Literal */
    sp->sp_type = sp_literal;
    sp->sp_literal.spl_value = s;
    sp->sp_literal.spl_length = n;
    s += n;
  }

  for (; IS_LWS(s[0]); s++)
    {}

  if (s[0] == ',' || (s[0] == '"' && s[1] == '\0'))
    for (s++; IS_LWS(s[0]); s++)
      {}
  else
    old_type = sp_error;

  if (old_type != sp_init && old_type != sp->sp_type)
    sp->sp_type = sp_error;

  *in_out_s = s;

  return sp->sp_type != sp_error;
}

/** Parse number:
 * number          =  [ "+" / "-" ] 1*DIGIT ["." 0*DIGIT]
 */
static double parse_number(char const *str, char **return_end)
{
  double value = 0.0;
  double decimal = 0.1;
  char d, sign = '+';

  if (return_end)
    *return_end = (char *)str;

  d = *str;

  if (d == '+' || d == '-')
    sign = d, d = *++str;

  if (!('0' <= d && d <= '9'))
    return value;

  for (; '0' <= d && d <= '9'; d = *++str)
    value = value * 10 + (d - '0');

  if (d == '.') for (d = *++str; '0' <= d && d <= '9'; d = *++str) {
    value += (d - '0') * decimal; decimal *= 0.1;
  }

  if (value > DBL_MAX)
    value = DBL_MAX;

  if (sign == '-')
    value = -value;

  if (return_end)
    *return_end = (char *)str;

  return value;
}


/** Return true if preferences match */
int sip_prefs_match(union sip_pref const *a,
		    union sip_pref const *b)
{
  if (!a || !b)
    return 0;
  if (a->sp_type != b->sp_type)
    return 0;
  switch (a->sp_type) {
  default:
  case sp_error:
    return 0;
  case sp_literal:
    return
      a->sp_literal.spl_length == b->sp_literal.spl_length &&
      su_casenmatch(a->sp_literal.spl_value, b->sp_literal.spl_value,
		  a->sp_literal.spl_length);
  case sp_string:
    return
      a->sp_string.sps_length == b->sp_string.sps_length &&
      strncmp(a->sp_string.sps_value, b->sp_string.sps_value,
	      a->sp_string.sps_length) == 0;
  case sp_range:
    return
      a->sp_range.spr_lower <= b->sp_range.spr_upper &&
      a->sp_range.spr_upper >= b->sp_range.spr_lower;
  }
}

/**Find a matching parameter-value pair from a parameter list.
 *
 * Check if the given feature values match with each other.
 *
 * @param pvalue first feature parameter
 * @param nvalue second feature parameter
 * @param return_parse_error return-value parameter for error (may be NULL)
 *
 * @retval 1 if given feature parameters match
 * @retval 0 if there is no match or a parse or type error occurred.
 *
 * If there is a parsing or type error, 0 is returned and @a
 * *return_parse_error is set to -1.
 *
 * @sa sip_prefs_parse(), sip_prefs_match(), union #sip_pref.
 */
int sip_prefs_matching(char const *pvalue,
		       char const *nvalue,
		       int *return_parse_error)
{
  int error;
  char const *p;
  union sip_pref np[1], pp[1];
  int n_negated, p_negated;

  if (!return_parse_error)
    return_parse_error = &error;

  if (!pvalue || !nvalue)
    return 0;

  memset(np, 0, sizeof np);

  /* Usually nvalue is from Accept/Reject-Contact,
     pvalue is from Contact */
  while (sip_prefs_parse(np, &nvalue, &n_negated)) {
    memset(pp, 0, sizeof pp);
    p = pvalue;

    while (sip_prefs_parse(pp, &p, &p_negated)) {
      if (pp->sp_type != np->sp_type) /* Types do not match */
	return 0;

      if (sip_prefs_match(np, pp) /* We found matching value */
	  ? !p_negated	/* without negative */
	  : p_negated)	/* Negative did not match */
	break;
    }

    if (pp->sp_type == sp_error)
      return *return_parse_error = -1, 0;

    if (pp->sp_type != sp_init /* We found matching value */
	? !n_negated		/* and we expected one */
	: n_negated)		/* We found none and expected none */
      return 1;
  }

  if (np->sp_type == sp_error)
    *return_parse_error = -1;

  return 0;
}

/** Check if the parameter is a valid feature tag.
 *
 * A feature tag is a parameter starting with a single plus, or a well-known
 * feature tag listed in @RFC3841: "audio", "automata", "application",
 * "class", "control", "duplex", "data", "description", "events", "isfocus",
 * "language", "mobility", "methods", "priority", "schemes", "type", or
 * "video". However, well-known feature tag can not start with plus. So,
 * "+alarm" or "audio" is a feature tag, "alarm", "++alarm", or "+audio" are
 * not.
 *
 * @retval 1 if string is a feature tag parameter
 * @retval 0 otherwise
 */
int sip_is_callerpref(char const *param)
{
#define MATCH(s) \
  (su_casenmatch(param + 1, s + 1, strlen(s) - 1) && \
   (param[strlen(s)] == '=' || param[strlen(s)] == '\0'))

  int xor = 0, base = 0;

  if (!param || !param[0])
    return 0;

  if (param[0] == '+')
    param++, xor = 1;

  switch (param[0]) {
  case 'a': case 'A':
    base = MATCH("audio") || MATCH("automata") || MATCH("application") ||
      MATCH("actor");
    break;
  case 'c': case 'C':
    base = MATCH("class") || MATCH("control");
    break;
  case 'd': case 'D':
    base = MATCH("duplex") || MATCH("data") || MATCH("description");
    break;
  case 'e': case 'E':
    base = MATCH("events");
    break;
  case 'i': case 'I':
    base = MATCH("isfocus");
    break;
  case 'l': case 'L':
    base = MATCH("language");
    break;
  case 'm': case 'M':
    base = MATCH("mobility") || MATCH("methods");
    break;
  case 'p': case 'P':
    base = MATCH("priority");
    break;
  case 's': case 'S':
    base = MATCH("schemes");
    break;
  case 't': case 'T':
    base = MATCH("type");
    break;
  case 'v': case 'V':
    base = MATCH("video");
    break;
  default:
    base = 0;
    break;
  }
#undef MATCH

  return base ^ xor;
}

/** Check if @Contact is immune to callerprefs. */
int sip_contact_is_immune(sip_contact_t const *m)
{
  unsigned i;

  if (m->m_params)
    for (i = 0; m->m_params[i]; i++) {
      if (sip_is_callerpref(m->m_params[i]))
	return 0;
    }

  return 1;
}

/**Check if @Contact matches by @AcceptContact.
 *
 * Matching @AcceptContact and @Contact headers is done as explained in
 * @RFC3841 section 7.2.4. The caller score can be calculated from the
 * returned S and N values.
 *
 * @par Matching
 * The @AcceptContact header contains number of feature tag parameters. The
 * count of feature tags is returned in @a return_N. For each feature tag in
 * @AcceptContact, the feature tag with same name is searched from the
 * @Contact header. If both headers contain the feature tag with same name,
 * their values are compared. If the value in @AcceptContact does not match
 * with the value in @Contact, there is mismatch and 0 is returned. If they
 * match, S is increased by 1.
 *
 * @param m   pointer to @Contact header structure
 * @param cp   pointer to @AcceptContact header structure
 * @param return_N   return-value parameter for number of
 *                   feature tags in @AcceptContact
 * @param return_S   return-value parameter for number of
 *                   matching feature tags
 * @param return_error   return-value parameter for parsing error
 *
 * For example,
 * @code
 * if (sip_contact_accept(contact, accept_contact, &S, &N, &error)) {
 *   if (N == 0)
 *     score == 1.0;
 *   else
 *     score = (double)S / (double)N;
 *   if (accept_contact->cp_explicit) {
 *     if (accept_contact->cp_require)
 *       goto drop;
 *     else
 *       score = 0.0;
 *   }
 * }
 * else if (!error) {
 *   score = 0.0;
 * }
 * @endcode
 *
 * @retval 1 if @Contact matches
 * @return @a return_S contains number of matching feature tags
 * @return @a return_N contains number of feature tags in @AcceptContact
 * @retval 0 if @Contact does not match
 * @return @a return_error contains -1 if feature tag value was malformed
 *
 * @sa @RFC3841 section 7.2.4, sip_contact_score(), sip_contact_reject(),
 * sip_contact_is_immune(), sip_contact_immunize(), sip_is_callerpref(),
 * sip_prefs_matching().
 */
int sip_contact_accept(sip_contact_t const *m,
		       sip_accept_contact_t const *cp,
		       unsigned *return_S,
		       unsigned *return_N,
		       int *return_error)
{
  char const *cap, *acc;
  unsigned i, S, N;
  size_t eq;

  if (!return_N) return_N = &N;
  if (!return_S) return_S = &S;

  *return_S = 0, *return_N = 0;

  if (!m || !cp || !m->m_params || !cp->cp_params)
    return 1;

  for (i = 0, S = 0, N = 0; cp->cp_params[i]; i++) {
    acc = cp->cp_params[i];
    if (!sip_is_callerpref(acc))
      continue;

    N++;

    cap = msg_params_find(m->m_params, acc);

    if (cap) {
      eq = strcspn(acc, "=");
      acc += eq + (acc[eq] == '=');

      if (!sip_prefs_matching(cap, acc, return_error))
	return 0;

      S++;
    }
  }

  *return_S = S; /* Matched feature tags */
  *return_N = N; /* Number of feature tags in @AcceptContact */

  return 1;
}


/** Check if @Contact is rejected by @RejectContact.
 *
 * @param m pointer to @Contact header
 * @param reject pointer to @RejectContact header
 *
 * @retval 1 when rejecting
 * @retval 0 when @Contact does not match with @RejectContact
 *
 * @sa sip_contact_score(), sip_contact_accept(), sip_contact_immunize(),
 * sip_contact_is_immune(), @RFC3841, @RejectContact, @Contact
 */
int sip_contact_reject(sip_contact_t const *m,
		       sip_reject_contact_t const *reject)
{
  unsigned S, N;
  int error;

  if (!m || !m->m_params || !reject || !reject->cp_params)
    return 0;

  return sip_contact_accept(m, reject, &S, &N, &error) && S == N && N > 0;
}

/**Immunize @Contact to callerprefs.
 *
 * Make a copy of @Contact header @a m and remove all parameters which
 * affect caller preferences.
 *
 * @param home   home object used when allocating copy
 * @param m   pointer to @Contact header structure to immunize
 *
 * @retval pointer to immunized copy if successful
 * @retval NULL upon an error
 *
 * @sa @RFC3841, sip_is_callerpref(), sip_contact_score(),
 * sip_contact_accept(), sip_contact_reject(), @Contact
 */
sip_contact_t *sip_contact_immunize(su_home_t *home, sip_contact_t const *m)
{
  unsigned i, j;
  sip_contact_t m0[1], *m1;
  msg_param_t *params;

  if (!m)
    return NULL;

  *m0 = *m, m0->m_next = NULL;

  m1 = sip_contact_copy(home, m0);

  if (m1 == NULL || !m1->m_params)
    return m1;

  params = (msg_param_t *)m1->m_params;

  for (i = 0, j = 0; params[i]; i++) {
    if (!sip_is_callerpref(params[i]))
      params[j++] = params[i];
  }

  params[j] = NULL;

  return m1;
}

/** Calculate score for contact.
 *
 * The caller preference score is an integer in range of 0 to 1000.
 *
 * @retval -1 if the contact is rejected
 * @retval 1000 if contact is immune to caller preferences
 * @retval 0..1000 reflecting @RFC3841 score in 0.000 - 1.000.
 *
 * @sa sip_q_value(),
 * sip_contact_accept(), sip_contact_reject(), sip_contact_is_immune(),
 * sip_contact_immunize(), sip_is_callerpref(), sip_prefs_matching(),
 * @RFC3841, @AcceptContact, @RejectContact, @Contact
 */
int sip_contact_score(sip_contact_t const *m,
		      sip_accept_contact_t const *ac,
		      sip_reject_contact_t const *rc)
{
  unsigned long S_total = 0;
  unsigned M = 0, scale = 1000;
  int error = 0;

  if (sip_contact_is_immune(m))
    return 1000;		/* Immune */

  for (; rc; rc = rc->cp_next)
    if (sip_contact_reject(m, rc))
      break;
  if (rc)
    return -1;			/* Rejected */

  for (; ac; ac = ac->cp_next) {
    unsigned S, N;

    if (!sip_contact_accept(m, ac, &S, &N, &error)) {
      if (ac->cp_require)
	return 0;		/* Discarded */
      continue;
    }

    M++;

    /* Apply score */
    if (S < N && ac->cp_explicit) {
      S = 0;
      if (ac->cp_require)
	return 0;		/* Dropped */
    }

    if (S > 0 && N > 0)
      S_total += sip_q_value(ac->cp_q) * (scale * S / N + (2 * S >= N));
  }

  if (!M)
    return 0;

  S_total /= M;

  if (S_total < scale * 1000)
    return S_total / scale;
  else
    return 1000;
}
