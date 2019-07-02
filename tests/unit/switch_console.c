/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2018, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Konstantin Molchanov <molchanov.kv@gmail.com>
 *
 *
 * switch_utils.c -- tests switch_utils
 *
 */

#include <stdio.h>
#include <switch.h>
#include <test/switch_test.h>

FST_MINCORE_BEGIN()

FST_SUITE_BEGIN(SWITCH_STANDARD_STREAM)

FST_SETUP_BEGIN()
{
}
FST_SETUP_END()

FST_TEARDOWN_BEGIN()
{
}
FST_TEARDOWN_END()

FST_TEST_BEGIN(benchmark)
{
	switch_stream_handle_t stream = { 0 };
	SWITCH_STANDARD_STREAM(stream);

	char expected_result[] = {'A', 0x00, 0x01, 0x02, 'B'};
	char raw_data[] = {0x00, 0x01, 0x02};

	stream.write_function(&stream, "%s", "A");
	stream.raw_write_function(&stream, (uint8_t *) raw_data, sizeof(raw_data));
	stream.write_function(&stream, "B");

	fst_requires(stream.data_len == sizeof(expected_result));
	fst_requires(memcmp(stream.data, expected_result, sizeof(expected_result)) == 0);

	switch_safe_free(stream.data);
}
FST_TEST_END()

FST_SUITE_END()

FST_MINCORE_END()

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
