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
 * Christian Marangi <ansuelsmth@gmail.com> # PCRE2 conversion
 *
 *
 * switch_regex.c -- PCRE2 wrapper
 *
 */

#include <switch.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

SWITCH_DECLARE(switch_regex_t *) switch_regex_compile(const char *pattern,
													  int options, int *errorcode, unsigned int *erroroffset, switch_regex_compile_context_t *ccontext)
{

	return (switch_regex_t *)pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED, options, errorcode, (PCRE2_SIZE *)erroroffset, ccontext);
}

SWITCH_DECLARE(int) switch_regex_copy_substring(switch_regex_match_t *match_data, int stringnumber, char *buffer, size_t *size)
{
	return pcre2_substring_copy_bynumber(match_data, stringnumber, (PCRE2_UCHAR *)buffer, (PCRE2_SIZE *)size);
}

SWITCH_DECLARE(void) switch_regex_match_free(void *data)
{
	pcre2_match_data_free(data);
}

SWITCH_DECLARE(void) switch_regex_free(void *data)
{
	pcre2_code_free(data);

}

SWITCH_DECLARE(int) switch_regex_perform(const char *field, const char *expression, switch_regex_t **new_re, switch_regex_match_t **new_match_data)
{
	int error_code = 0;
	PCRE2_UCHAR error_str[128];
	PCRE2_SIZE error_offset = 0;
	pcre2_code *re = NULL;
	pcre2_match_data *match_data;
	int match_count = 0;
	char *tmp = NULL;
	uint32_t flags = 0;
	char abuf[256] = "";

	if (!(field && expression)) {
		return 0;
	}

	if (*expression == '_') {
		if (switch_ast2regex(expression + 1, abuf, sizeof(abuf))) {
			expression = abuf;
		}
	}

	if (*expression == '/') {
		char *opts = NULL;
		tmp = strdup(expression + 1);
		switch_assert(tmp);
		if ((opts = strrchr(tmp, '/'))) {
			*opts++ = '\0';
		} else {
			/* Note our error */
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
							  "Regular Expression Error expression[%s] missing ending '/' delimeter\n", expression);
			goto end;
		}
		expression = tmp;
		if (*opts) {
			if (strchr(opts, 'i')) {
				flags |= PCRE2_CASELESS;
			}
			if (strchr(opts, 's')) {
				flags |= PCRE2_DOTALL;
			}
		}
	}

	re = pcre2_compile((PCRE2_SPTR)expression,	/* the pattern */
					  PCRE2_ZERO_TERMINATED,
					  flags,	/* default options */
					  &error_code,	/* for error code */
					  &error_offset,	/* for error offset */
					  NULL);	/* use default character tables */
	if (!re) {
		pcre2_get_error_message(error_code, error_str, 128);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "COMPILE ERROR: %zu [%s][%s]\n", error_offset, error_str, expression);
		goto end;
	}

	match_data = pcre2_match_data_create_from_pattern(re, NULL);

	match_count = pcre2_match(re,	/* result of pcre_compile() */
							(PCRE2_SPTR)field,	/* the subject string */
							(int) strlen(field),	/* the length of the subject string */
							0,	/* start at offset 0 in the subject */
							0,	/* default options */
							match_data,	/* vector of integers for substring information */
							NULL);	/* number of elements (NOT size in bytes) */


	if (match_count <= 0) {
		switch_regex_match_safe_free(match_data);
		switch_regex_safe_free(re);
		match_count = 0;
	}

	if (!new_re || !new_match_data) {
		switch_regex_match_safe_free(match_data);
		switch_regex_safe_free(re);
	}

	if (new_re) {
		*new_re = (switch_regex_t *)re;
	}

	if (new_match_data) {
		*new_match_data = (switch_regex_match_t *)match_data;
	}

  end:
	switch_safe_free(tmp);

	return match_count;
}

SWITCH_DECLARE(void) switch_perform_substitution(switch_regex_match_t *match_data, const char *data,
												 char *substituted, switch_size_t len)
{
	char index[10] = "";
	const char *replace = NULL;
	PCRE2_SIZE replace_size;
	switch_size_t x, y = 0, z = 0;
	int num = 0;
	int brace;

	for (x = 0; y < (len - 1) && x < strlen(data);) {
		if (data[x] == '$') {
			x++;

			brace = data[x] == '{';
			if (brace) {
				x++;
			}

			if (!(data[x] > 47 && data[x] < 58)) {
				x -= brace;
				substituted[y++] = data[x - 1];
				continue;
			}

			while (data[x] > 47 && data[x] < 58 && z < sizeof(index) - 1) {
				index[z++] = data[x];
				x++;
			}
			if (brace) {
				if (data[x] != '}') {
					x -= z - 1;
					substituted[y++] = data[x - 1];
					continue;
				}
				else {
					x++;
				}
			}
			index[z++] = '\0';
			z = 0;
			num = atoi(index);

			if (num < 0 || num > 256) {
				num = -1;
			}

			if (pcre2_substring_get_bynumber(match_data, num, (PCRE2_UCHAR **)&replace, &replace_size) >= 0) {
				if (replace) {
					switch_size_t r;

					for (r = 0; r < strlen(replace) && y < (len - 1); r++) {
						substituted[y++] = replace[r];
					}

					pcre2_substring_free((PCRE2_UCHAR *)replace);
				}
			}
		} else {
			substituted[y++] = data[x];
			x++;
		}
	}
	substituted[y++] = '\0';
}


SWITCH_DECLARE(void) switch_capture_regex(switch_regex_match_t *match_data, int match_count,
										  const char *var, switch_cap_callback_t callback, void *user_data)

{


	const char *replace;
	PCRE2_SIZE replace_size;
	int i;

	for (i = 0; i < match_count; i++) {
		if (pcre2_substring_get_bynumber(match_data, i, (PCRE2_UCHAR **)&replace, &replace_size) >= 0) {
			if (replace) {
				callback(var, (const char *)replace, user_data);
				pcre2_substring_free((PCRE2_UCHAR *)replace);
			}
		}
	}
}

SWITCH_DECLARE(switch_status_t) switch_regex_match_partial(const char *target, const char *expression, int *partial)
{
	PCRE2_UCHAR error[128]; /* Used to hold any errors                                           */
	int error_code = 0;	/* Holds the code of an error                                           */
	PCRE2_SIZE error_offset = 0;		/* Holds the offset of an error                                      */
	pcre2_code *pcre_prepared = NULL;	/* Holds the compiled regex                                          */
	int match_count = 0;		/* Number of times the regex was matched                             */
	pcre2_match_data *match_data;
	int pcre2_flags = 0;
	uint32_t flags = 0;
	char *tmp = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (*expression == '/') {
		char *opts = NULL;
		tmp = strdup(expression + 1);
		switch_assert(tmp);
		if ((opts = strrchr(tmp, '/'))) {
			*opts++ = '\0';
		} else {
			/* Note our error */
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
							  "Regular Expression Error expression[%s] missing ending '/' delimeter\n", expression);
			goto end;
		}
		expression = tmp;
		if (*opts) {
			if (strchr(opts, 'i')) {
				flags |= PCRE2_CASELESS;
			}
			if (strchr(opts, 's')) {
				flags |= PCRE2_DOTALL;
			}
		}
	}

	/* Compile the expression */
	pcre_prepared = pcre2_compile((PCRE2_SPTR)expression, PCRE2_ZERO_TERMINATED, flags, &error_code, &error_offset, NULL);

	/* See if there was an error in the expression */
	if (!pcre_prepared) {
		pcre2_get_error_message(error_code, error, 128);

		/* Note our error */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
						  "Regular Expression Error expression[%s] error[%s] location[%zu]\n", expression, error, error_offset);

		/* We definitely didn't match anything */
		goto end;
	}

	if (*partial) {
		pcre2_flags = PCRE2_PARTIAL_SOFT;
	}

	/* So far so good, run the regex */
	match_data = pcre2_match_data_create_from_pattern(pcre_prepared, NULL);

	match_count =
		pcre2_match(pcre_prepared, (PCRE2_SPTR)target, (int) strlen(target), 0, pcre2_flags, match_data, NULL);

	pcre2_match_data_free(match_data);

	/* Clean up */
	if (pcre_prepared) {
		pcre2_code_free(pcre_prepared);
		pcre_prepared = NULL;
	}

	/* switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "number of matches: %d\n", match_count); */

	/* Was it a match made in heaven? */
	if (match_count > 0) {
		*partial = 0;
		switch_goto_status(SWITCH_STATUS_SUCCESS, end);
	} else if (match_count == PCRE2_ERROR_PARTIAL) {
		/* yes it is already set, but the code is clearer this way */
		*partial = 1;
		switch_goto_status(SWITCH_STATUS_SUCCESS, end);
	} else {
		goto end;
	}
 end:
	switch_safe_free(tmp);
	return status;
}

SWITCH_DECLARE(switch_status_t) switch_regex_match(const char *target, const char *expression)
{
	int partial = 0;
	return switch_regex_match_partial(target, expression, &partial);
}

SWITCH_DECLARE_NONSTD(void) switch_regex_set_var_callback(const char *var, const char *val, void *user_data)
{
	switch_core_session_t *session = (switch_core_session_t *) user_data;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_add_variable_var_check(channel, var, val, SWITCH_FALSE, SWITCH_STACK_PUSH);
}

SWITCH_DECLARE_NONSTD(void) switch_regex_set_event_header_callback(const char *var, const char *val, void *user_data)
{

	switch_event_t *event = (switch_event_t *) user_data;
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, var, val);
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
