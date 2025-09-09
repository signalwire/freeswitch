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
 * Michael Jerris <mike@jerris.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Michael Jerris <mike@jerris.com>
 *
 * switch_regex.h -- pcre2 wrapper and extensions Header
 *
 */
/*! \file switch_regex.h
    \brief Regex Header
*/
#ifndef SWITCH_REGEX_H
#define SWITCH_REGEX_H

SWITCH_BEGIN_EXTERN_C
/**
 * @defgroup switch_regex Regular Expressions
 * @ingroup FREESWITCH
 * @{
 */

#define SWITCH_PCRE2 1

typedef struct pcre2_real_code switch_regex_t;
typedef struct pcre2_real_match_data_8 switch_regex_match_t;
typedef struct pcre2_real_compile_context_8 switch_regex_compile_context_t;

SWITCH_DECLARE(switch_regex_t *) switch_regex_compile(const char *pattern, int options, int *errorcode, unsigned int *erroroffset,
													  switch_regex_compile_context_t *ccontext);

SWITCH_DECLARE(int) switch_regex_copy_substring(switch_regex_match_t *match_data, int stringnumber, char *buffer, size_t *size);

SWITCH_DECLARE(void) switch_regex_match_free(void *data);
SWITCH_DECLARE(void) switch_regex_free(void *data);

SWITCH_DECLARE(int) switch_regex_perform(const char *field, const char *expression, switch_regex_t **new_re, switch_regex_match_t **new_match_data);
#define switch_regex(field, expression) switch_regex_perform(field, expression, NULL, NULL)

SWITCH_DECLARE(void) switch_perform_substitution(switch_regex_match_t *match_data, const char *data,
												 char *substituted, switch_size_t len);

/*!
 \brief Function to evaluate an expression against a string
 \param target The string to find a match in
 \param expression The regular expression to run against the string
 \return Boolean if a match was found or not
*/
SWITCH_DECLARE(switch_status_t) switch_regex_match(const char *target, const char *expression);

/*!
 \brief Function to evaluate an expression against a string
 \param target The string to find a match in
 \param expression The regular expression to run against the string
 \param partial_match If non-zero returns SUCCESS if the target is a partial match, on successful return, this is set to non-zero if the match was partial and zero if it was a full match
 \return Boolean if a match was found or not
*/
SWITCH_DECLARE(switch_status_t) switch_regex_match_partial(const char *target, const char *expression, int *partial_match);

SWITCH_DECLARE(void) switch_capture_regex(switch_regex_match_t *match_data, int match_count,
										  const char *var, switch_cap_callback_t callback, void *user_data);

SWITCH_DECLARE_NONSTD(void) switch_regex_set_var_callback(const char *var, const char *val, void *user_data);
SWITCH_DECLARE_NONSTD(void) switch_regex_set_event_header_callback(const char *var, const char *val, void *user_data);

#define switch_regex_match_safe_free(match_data)	if (match_data) {\
				switch_regex_match_free(match_data);\
				match_data = NULL;\
			}

#define switch_regex_safe_free(re)	if (re) {\
				switch_regex_free(re);\
				re = NULL;\
			}


/** @} */

SWITCH_END_EXTERN_C
#endif
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
