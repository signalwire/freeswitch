/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2011, Anthony Minessale II <anthm@freeswitch.org>
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
 *
 * Clarence <xjh.azzbcc@gmail.com>
 *
 * test_sofia_profile.c -- tests of sofia profile
 *
 */
#include <mod_sofia.h>
#include <switch.h>
#include <test/switch_test.h>

#define KEY_NAME       "sofia.conf"
#define SECTION        "configuration"
#define FAILED_PROFILE "failed_profile"

static switch_xml_t dynamic_sofia_profile(const char *section, const char *tag_name, const char *key_name,
                                          const char *key_value, switch_event_t *params, void *user_data) {
    /**
     * which start failed on same port is in use with normal profile.
     */
    char *xml_string = "<document type=\"freeswitch/xml\">"
                       "  <section name=\"configuration\">"
                       "    <configuration name=\"sofia.conf\">"
                       "      <profiles>"
                       "        <profile name=\"" FAILED_PROFILE "\">"
                       "          <settings/>"
                       "        </profile>"
                       "      </profiles>"
                       "    </configuration>"
                       "  </section>"
                       "</document>";
    if (zstr(tag_name) || !strstr(tag_name, SECTION) || zstr(key_value) || !strstr(key_value, KEY_NAME)) {
        return NULL;
    }
    return switch_xml_parse_str_dynamic(xml_string, SWITCH_TRUE);
}

FST_CORE_DB_BEGIN("./profile")
FST_MODULE_BEGIN(mod_sofia, sofia)

FST_SETUP_BEGIN() {}
FST_SETUP_END()

FST_TEARDOWN_BEGIN() {}
FST_TEARDOWN_END()

/**
 * unexpected empty profile name
 */
FST_TEST_BEGIN(test_empty_name) {
    sofia_profile_t *profile = sofia_glue_find_profile("empty_name");
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "profile: %p\n", (void *) profile);
    fst_requires(profile != NULL);
    fst_check_string_equals(profile->name, "unnamed");
    sofia_glue_release_profile(profile);
}
FST_TEST_END()

/**
 * sometimes `stop wait` cost too much time.
 */
FST_TEST_BEGIN(test_profile_stop) {
    int sanity;
    sofia_profile_t *profile;
    switch_stream_handle_t stream = { 0 };
    const char *profile_name = "normal";
    SWITCH_STANDARD_STREAM(stream);

    // normal profile is necessary.
    profile = sofia_glue_find_profile(profile_name);
    fst_requires(profile != NULL);

    // profile must be up for at least 10 seconds.
    for (; switch_epoch_time_now(NULL) < profile->started + 10;) {
        switch_sleep(500000);
    }
    sofia_glue_release_profile(profile);

    // command block 20s even profile have stop already.
    switch_api_execute("sofia", "profile normal stop wait", NULL, &stream);
    fst_requires(sofia_glue_find_profile(profile_name) == NULL);

    /**
     * XXX
     *   profile haven't release from the global hash yet, so that we can't start the profile immediately
     *   `sofia_glue_add_profile` will lost the new profile,
     */
    for (sanity = 0; sanity < 20 && sofia_glue_profile_exists(profile_name); sanity++) {
        switch_sleep(500000);
    }

    // start normal profile.
    switch_api_execute("sofia", "profile normal start", NULL, &stream);

    switch_safe_free(stream.data);
}
FST_TEST_END()

/**
 * check memory when profile start failed
 */
FST_TEST_BEGIN(test_port_in_use) {
    int sanity;
    sofia_profile_t *profile;
    const char *profile_name = "normal";
    switch_stream_handle_t stream = { 0 };

    // profile haven't started yet.
    for (sanity = 0; sanity < 20 && !sofia_glue_profile_exists(profile_name); sanity++) {
        profile = sofia_glue_find_profile(profile_name);
        if (profile) {
            sofia_glue_release_profile(profile);
            break;
        }
        switch_sleep(500000);
    }

    // bind profile search function
    switch_xml_bind_search_function(dynamic_sofia_profile, switch_xml_parse_section_string(SECTION), NULL);

    // load failed profile
    SWITCH_STANDARD_STREAM(stream);
    switch_api_execute("sofia", "profile " FAILED_PROFILE " start", NULL, &stream);
    fst_check_string_has(stream.data, "successfully");

    // release bind function
    switch_xml_unbind_search_function_ptr(dynamic_sofia_profile);
    switch_safe_free(stream.data);
}
FST_TEST_END()

/**
 * finally unload mod_sofia
 */
FST_TEST_BEGIN(test_unload_mod_sofia) {
    const char *err = NULL;
    switch_status_t status;

    switch_sleep(30000000);
    status = switch_loadable_module_unload_module(SWITCH_GLOBAL_dirs.mod_dir, "mod_sofia", SWITCH_FALSE, &err);
    fst_requires(status == SWITCH_STATUS_SUCCESS);
}
FST_TEST_END()

FST_MODULE_END()
FST_CORE_END()
