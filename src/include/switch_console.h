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
 * switch_console.h -- Simple Console
 *
 */
/*! \file switch_console.h
    \brief Simple Console

	This module implements a basic console i/o and by basic I mean, um yeah, basic
	Right now the primary function of this portion of the program is to keep it from exiting.
*/

#ifndef SWITCH_CONSOLE_H
#define SWITCH_CONSOLE_H

#include <switch.h>

SWITCH_BEGIN_EXTERN_C
#define SWITCH_CMD_CHUNK_LEN 1024
#define SWITCH_STANDARD_STREAM(s) memset(&s, 0, sizeof(s)); s.data = malloc(SWITCH_CMD_CHUNK_LEN); \
	switch_assert(s.data);												\
	memset(s.data, 0, SWITCH_CMD_CHUNK_LEN);							\
	s.end = s.data;														\
	s.data_size = SWITCH_CMD_CHUNK_LEN;									\
	s.write_function = switch_console_stream_write;						\
	s.raw_write_function = switch_console_stream_raw_write;				\
	s.alloc_len = SWITCH_CMD_CHUNK_LEN;									\
	s.alloc_chunk = SWITCH_CMD_CHUNK_LEN

/*!
  \brief A simple comand loop that reads input from the terminal
*/
SWITCH_DECLARE(void) switch_console_loop(void);

/*!
  \brief A method akin to printf that allows you to redirect output to a specific console "channel"
*/
SWITCH_DECLARE(void) switch_console_printf(switch_text_channel_t channel, const char *file, const char *func, int line,
										   const char *fmt, ...) PRINTF_FUNCTION(5, 6);

SWITCH_DECLARE_NONSTD(switch_status_t) switch_console_stream_raw_write(switch_stream_handle_t *handle, uint8_t *data, switch_size_t datalen);

/*!
  \brief A method akin to printf for dealing with api streams
*/
SWITCH_DECLARE_NONSTD(switch_status_t) switch_console_stream_write(switch_stream_handle_t *handle, const char *fmt, ...) PRINTF_FUNCTION(2, 3);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
