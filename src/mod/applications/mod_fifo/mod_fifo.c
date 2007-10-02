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
 * mod_fifo.c -- FIFO
 *
 */
#include <switch.h>

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_fifo_shutdown);
SWITCH_MODULE_LOAD_FUNCTION(mod_fifo_load);
SWITCH_MODULE_DEFINITION(mod_fifo, mod_fifo_load, mod_fifo_shutdown, NULL);



static switch_status_t on_dtmf(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{
    switch_core_session_t *bleg;
    switch_channel_t *channel;

    bleg = (switch_core_session_t *) buf;

	switch (itype) {
	case SWITCH_INPUT_TYPE_DTMF:
        {
            char *dtmf = (char *) input;
            if (*dtmf == '*') {
                channel = switch_core_session_get_channel(session);
                if (switch_channel_test_flag(channel, CF_ORIGINATOR)) {
                    channel = switch_core_session_get_channel(bleg);
                    switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
                    return SWITCH_STATUS_BREAK;
                }
            }
        }
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}


#define check_string(s) if (!switch_strlen_zero(s) && !strcasecmp(s, "undef")) { s = NULL; }


static switch_status_t read_frame_callback(switch_core_session_t *session, switch_frame_t *frame, void *user_data)
{
    switch_queue_t *fifo = (switch_queue_t *) user_data;
    if (switch_queue_size(fifo)) {
        return SWITCH_STATUS_FALSE;
    }
    return SWITCH_STATUS_SUCCESS;
}

static struct {
    switch_hash_t *fifo_hash;
    switch_memory_pool_t *pool;
} globals;

#define FIFO_DESC "Fifo for stacking parked calls."
#define FIFO_USAGE "<fifo name> [in [<announce file>|undef] [<music file>|undef] | out [wait|nowait] [<announce file>|undef] [<music file>|undef]]"
SWITCH_STANDARD_APP(fifo_function)
{
    int argc;
    char *mydata = NULL, *argv[5] = { 0 };
    switch_queue_t *fifo;
    switch_channel_t *channel;
    int nowait = 0;
    char *moh = NULL;
    char *announce = NULL;


    if (switch_strlen_zero(data)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Args\n");
        return;
    }

    mydata = switch_core_session_strdup(session, data);
    assert(mydata);
    if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) < 2) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "USAGE %s\n", FIFO_USAGE);
        return;
    }

    if (!(fifo = switch_core_hash_find(globals.fifo_hash, argv[0]))) {
        switch_queue_create(&fifo, SWITCH_CORE_QUEUE_LEN, globals.pool);
        assert(fifo);
        switch_core_hash_insert(globals.fifo_hash, argv[0], fifo);
    }

    channel = switch_core_session_get_channel(session);
    moh = switch_channel_get_variable(channel, "fifo_music");
    announce = switch_channel_get_variable(channel, "fifo_announce");

    if (argc > 2) {
        nowait = !strcasecmp(argv[2], "nowait");
    }

    if (!strcasecmp(argv[1], "in")) {
        char *uuid = strdup(switch_core_session_get_uuid(session));

        switch_channel_answer(channel);

        if (argc > 2) {
            announce = argv[2];
        }

        if (argc > 3) {
            moh = argv[3];
        }

        check_string(announce);
        check_string(moh);

        
        if (moh) {
            switch_ivr_broadcast(uuid, moh, SMF_LOOP | SMF_ECHO_ALEG);
        }
        
        switch_channel_set_flag(channel, CF_TAGGED);
        
        switch_queue_push(fifo, uuid);
        switch_ivr_park(session, NULL);
        
        if (switch_channel_ready(channel)) {
            if (announce) {
                switch_ivr_play_file(session, NULL, announce, NULL);
            }
        }

        switch_channel_clear_flag(channel, CF_TAGGED);

        if (switch_channel_ready(channel)) {
            switch_channel_set_state(channel, CS_HIBERNATE);
        }
        
        return;
    } else if (!strcasecmp(argv[1], "out")) {
        void *pop;
        switch_frame_t *read_frame;
        switch_status_t status;
        char *uuid;
        int done = 0;
        switch_core_session_t *other_session;
        switch_input_args_t args = { 0 };

        if (argc > 3) {
            announce = argv[3];
        }

        if (argc > 4) {
            moh = argv[4];
        }
        
        check_string(announce);
        check_string(moh);
                            
        if (!nowait) {
            switch_channel_answer(channel);
        }
        

        for (;;) {
            if (moh) {
                args.read_frame_callback = read_frame_callback;
                args.user_data = fifo;
                switch_ivr_play_file(session, NULL, moh, &args);
            }

            if (switch_queue_trypop(fifo, &pop) != SWITCH_STATUS_SUCCESS) {
                if (nowait) {
                    return;
                }
                status = switch_core_session_read_frame(session, &read_frame, -1, 0);
                if (!SWITCH_READ_ACCEPTABLE(status)) {
                    return;
                }
                continue;
            }
            if (!pop) {
                return;
            }

            uuid = (char *) pop;

            if ((other_session = switch_core_session_locate(uuid))) {
                switch_channel_t *other_channel = switch_core_session_get_channel(other_session);
                switch_caller_profile_t *cloned_profile;
                
                if (announce) {
                    switch_ivr_play_file(session, NULL, announce, NULL);
                } else {
                    switch_ivr_sleep(session, 500);
                }
                switch_channel_clear_flag(other_channel, CF_CONTROLLED);
                switch_channel_clear_flag(other_channel, CF_BROADCAST);
                switch_channel_set_flag(other_channel, CF_BREAK);
                switch_core_session_kill_channel(other_session, SWITCH_SIG_BREAK);
                while (switch_channel_test_flag(other_channel, CF_TAGGED)) {
                    status = switch_core_session_read_frame(session, &read_frame, -1, 0);
                    if (!SWITCH_READ_ACCEPTABLE(status)) {
                        break;
                    }
                }

                switch_channel_answer(channel);
				cloned_profile = switch_caller_profile_clone(session, switch_channel_get_caller_profile(channel));
                assert(cloned_profile);
                switch_channel_set_originator_caller_profile(other_channel, cloned_profile);

				cloned_profile = switch_caller_profile_clone(other_session, switch_channel_get_caller_profile(channel));
                assert(cloned_profile);
                switch_channel_set_originatee_caller_profile(channel, cloned_profile);
				
                switch_ivr_multi_threaded_bridge(session, other_session, on_dtmf, other_session, session);
                switch_core_session_rwunlock(other_session);
                if (nowait) {
                    done = 1;
                }
            }

            switch_safe_free(uuid);

            if (done) {
                break;
            }
        }
    } else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "USAGE %s\n", FIFO_USAGE);
    }

}

SWITCH_MODULE_LOAD_FUNCTION(mod_fifo_load)
{
	switch_application_interface_t *app_interface;

    switch_core_new_memory_pool(&globals.pool);
    switch_core_hash_init(&globals.fifo_hash, globals.pool);

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_APP(app_interface, "fifo", "Park with FIFO", FIFO_DESC, fifo_function, FIFO_USAGE, SAF_NONE);


	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down 
*/
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_fifo_shutdown)
{
    switch_hash_index_t *hi;
    void *val, *pop;
    switch_queue_t *fifo;

    /* Cleanup*/
    for (hi = switch_hash_first(NULL, globals.fifo_hash); hi; hi = switch_hash_next(hi)) {
        switch_hash_this(hi, NULL, NULL, &val);
        fifo = (switch_queue_t *) val;
        while (switch_queue_trypop(fifo, &pop) == SWITCH_STATUS_SUCCESS) {
            free(pop);
        }
    }
    switch_core_hash_destroy(&globals.fifo_hash);
    switch_core_destroy_memory_pool(&globals.pool);
	return SWITCH_STATUS_SUCCESS;
}


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
