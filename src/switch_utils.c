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
#include <switch_utils.h>

SWITCH_DECLARE(unsigned int) switch_separate_string(char *buf, char delim, char **array, int arraylen)
{
	int argc;
	char *scan;
	int paren = 0;

	if (!buf || !array || !arraylen) {
		return 0;
	}

	memset(array, 0, arraylen * sizeof(*array));

	scan = buf;

	for (argc = 0; *scan && (argc < arraylen - 1); argc++) {
		array[argc] = scan;
		for (; *scan; scan++) {
			if (*scan == '(')
				paren++;
			else if (*scan == ')') {
				if (paren)
					paren--;
			} else if ((*scan == delim) && !paren) {
				*scan++ = '\0';
				break;
			}
		}
	}

	if (*scan) {
		array[argc++] = scan;
	}

	return argc;
}

SWITCH_DECLARE(char *) switch_cut_path(char *in)
{
	char *p, *ret = in;
	char delims[] = "/\\";
	char *i;

	for (i = delims; *i; i++) {
		p = in;
		while ((p = strchr(p, *i)) != 0) {
			ret = ++p;
		}
	}
	return ret;
}

SWITCH_DECLARE(switch_status) switch_socket_create_pollfd(switch_pollfd_t *poll, switch_socket_t *sock,
														  switch_int16_t flags, switch_memory_pool *pool)
{
	switch_pollset_t *pollset;
	switch_status status;

	if ((status = switch_pollset_create(&pollset, 1, pool, flags)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	poll->desc_type = SWITCH_POLL_SOCKET;
	poll->reqevents = flags;
	poll->desc.s = sock;
	poll->client_data = sock;

	return switch_pollset_add(pollset, poll);
}


SWITCH_DECLARE(int) switch_socket_waitfor(switch_pollfd_t *poll, int ms)
{
	switch_status status;
	int nsds = 0;

	if ((status = switch_poll(poll, 1, &nsds, ms)) != SWITCH_STATUS_SUCCESS) {
		return -1;
	}

	return nsds;
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
