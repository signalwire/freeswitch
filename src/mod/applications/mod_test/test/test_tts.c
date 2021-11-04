/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2020, Anthony Minessale II <anthm@freeswitch.org>
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
 * Seven Du <dujinfang@gmail.com>
 *
 *
 * test_tts.c -- tests for mock test tts interface
 *
 */
#include <switch.h>
#include <test/switch_test.h>

FST_CORE_BEGIN(".")

FST_MODULE_BEGIN(mod_test, test_asr)

FST_SETUP_BEGIN()
{
	fst_requires_module("mod_tone_stream");
	fst_requires_module("mod_sndfile");
	fst_requires_module("mod_dptools");
}
FST_SETUP_END()

FST_TEARDOWN_BEGIN()
{
}
FST_TEARDOWN_END()


FST_TEST_BEGIN(tts_8000)
{
    switch_speech_handle_t sh = { 0 };
    switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;
    switch_status_t status;
    int sample_rate = 8000;
    int interval = 20;
    uint8_t data[320];
    size_t datalen = sizeof(data);
    int read = 0;

    status = switch_core_speech_open(&sh, "test", "default", sample_rate, interval, 1, &flags, NULL);
    fst_requires(status == SWITCH_STATUS_SUCCESS);
    status = switch_core_speech_feed_tts(&sh, "text", &flags);
    fst_requires(status == SWITCH_STATUS_SUCCESS);
    do {
        status = switch_core_speech_read_tts(&sh, data, &datalen, &flags);
        read++;
    } while (status == SWITCH_STATUS_SUCCESS);

    fst_check(read = sample_rate / interval); // samples of 1 second
    switch_core_speech_close(&sh, &flags);
}
FST_TEST_END()

FST_TEST_BEGIN(tts_48000)
{
    switch_speech_handle_t sh = { 0 };
    switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;
    switch_status_t status;
    int sample_rate = 48000;
    int interval = 20;
    uint8_t data[320];
    size_t datalen = sizeof(data);
    int read = 0;

    status = switch_core_speech_open(&sh, "test", "default", sample_rate, interval, 1, &flags, NULL);
    fst_requires(status == SWITCH_STATUS_SUCCESS);
    status = switch_core_speech_feed_tts(&sh, "text", &flags);
    fst_requires(status == SWITCH_STATUS_SUCCESS);
    do {
        status = switch_core_speech_read_tts(&sh, data, &datalen, &flags);
        read++;
    } while (status == SWITCH_STATUS_SUCCESS);

    fst_check(read = sample_rate / interval); // samples of 1 second
    switch_core_speech_close(&sh, &flags);
}
FST_TEST_END()

FST_TEST_BEGIN(tts_48000_2)
{
    switch_speech_handle_t sh = { 0 };
    switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;
    switch_status_t status;
    int sample_rate = 48000;
    int interval = 20;
    uint8_t data[320];
    size_t datalen = sizeof(data);
    int read = 0;

    status = switch_core_speech_open(&sh, "test", "default", sample_rate, interval, 2, &flags, NULL);
    fst_requires(status == SWITCH_STATUS_SUCCESS);
    status = switch_core_speech_feed_tts(&sh, "text", &flags);
    fst_requires(status == SWITCH_STATUS_SUCCESS);
    do {
        status = switch_core_speech_read_tts(&sh, data, &datalen, &flags);
        read++;
    } while (status == SWITCH_STATUS_SUCCESS);

    fst_check(read = sample_rate / interval); // samples of 1 second
    switch_core_speech_close(&sh, &flags);
}
FST_TEST_END()

FST_TEST_BEGIN(tts_time)
{
    switch_speech_handle_t sh = { 0 };
    switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;
    switch_status_t status;
    int sample_rate = 8000;
    int interval = 20;
    uint8_t data[320];
    size_t datalen = sizeof(data);
    int read = 0;

    status = switch_core_speech_open(&sh, "test", "default", sample_rate, interval, 1, &flags, NULL);
    fst_requires(status == SWITCH_STATUS_SUCCESS);
    status = switch_core_speech_feed_tts(&sh, "silence://3000", &flags);
    fst_requires(status == SWITCH_STATUS_SUCCESS);
    do {
        status = switch_core_speech_read_tts(&sh, data, &datalen, &flags);
        read++;
        switch_yield(interval * 1000);
    } while (status == SWITCH_STATUS_SUCCESS);

    fst_check(read = sample_rate / interval * 3); // samples of 3 second
    fst_check_duration(3000, 500);
    switch_core_speech_close(&sh, &flags);
}
FST_TEST_END()

FST_TEST_BEGIN(unload_test)
{
	const char *err = NULL;
	switch_sleep(1000000);
	fst_check(switch_loadable_module_unload_module(SWITCH_GLOBAL_dirs.mod_dir, (char *)"mod_test", SWITCH_FALSE, &err) == SWITCH_STATUS_SUCCESS);
}
FST_TEST_END()


FST_MODULE_END()

FST_CORE_END()
