/*
 * Test case for race condition in switch_core_session_read_frame.
 * This test reproduces a crash in switch_core_session_read_frame that occurs near the end,
 * when switch_mutex_unlock(session->read_codec->mutex); is called.
 *
 */

#include <switch.h>
#include <test/switch_test.h>

static switch_core_session_t *g_session = NULL;
static volatile int return_inuse = 0;
static volatile int unset_done = 0;
static switch_io_routines_t original_io_routines = {0};
static switch_io_routines_t patched_io_routines = {0};

/* Mock read_frame that returns SWITCH_STATUS_INUSE */
static switch_status_t mock_read_frame_inuse(switch_core_session_t *session, switch_frame_t **frame,
                                              switch_io_flag_t flags, int stream_id)
{
    if (return_inuse) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
                          "[TEST] mock_read_frame: Returning SWITCH_STATUS_INUSE (first call)\n");
        return_inuse = 0; /* Only return INUSE once, then fall through to original */
        switch_yield(100000); /* Give time for race to happen */
        return SWITCH_STATUS_INUSE;
    }

    /* Call original */
    if (original_io_routines.read_frame) {
        return original_io_routines.read_frame(session, frame, flags, stream_id);
    }

    return SWITCH_STATUS_FALSE;
}

/* Thread that unsets read_codec */
static void *unset_codec_thread(switch_thread_t *thread, void *obj)
{
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
                      "[Thread B] Waiting before unsetting codec...\n");

    /* Wait for Thread A to unlock codec_read_mutex and jump to cnt_with_cng
     * Mock waits 100ms, so we wait 10ms to hit the window after unlock but before final unlock */
    switch_yield(10000); /* 10ms */

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
                      "[Thread B] Unsetting read_codec to NULL\n");

    switch_core_session_unset_read_codec(g_session);
    unset_done = 1;

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
                      "[Thread B] Codec unset - crash should happen at line 1058\n");

    return NULL;
}

/* Bug callback */
static switch_bool_t bug_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
    return SWITCH_TRUE;
}

FST_CORE_BEGIN("./conf")
{
    FST_SUITE_BEGIN(switch_core_io_inuse_race)
    {
        FST_SETUP_BEGIN()
        {
            fst_requires_module("mod_loopback");
        }
        FST_SETUP_END()

        FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

        FST_TEST_BEGIN(test_inuse_race_condition)
        {
            switch_core_session_t *session = NULL;
            switch_call_cause_t cause;
            switch_channel_t *channel = NULL;
            switch_status_t status;
            switch_frame_t *frame = NULL;
            switch_media_bug_t *bug = NULL;
            switch_thread_t *thread = NULL;
            switch_threadattr_t *thd_attr = NULL;
            switch_status_t thread_status;
            switch_endpoint_interface_t *endpoint_interface = NULL;

            return_inuse = 0;
            unset_done = 0;

            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
                              "========== INUSE RACE CONDITION TEST ==========\n");

            /* Get null endpoint interface (that's what we're using in the dial string) */
            endpoint_interface = switch_loadable_module_get_endpoint_interface("null");
            fst_requires(endpoint_interface);

            /* Backup original io_routines and patch BEFORE creating session */
            if (endpoint_interface->io_routines) {
                memcpy(&original_io_routines, endpoint_interface->io_routines, sizeof(switch_io_routines_t));
                memcpy(&patched_io_routines, endpoint_interface->io_routines, sizeof(switch_io_routines_t));
                patched_io_routines.read_frame = mock_read_frame_inuse;

                /* Patch endpoint BEFORE creating session */
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
                                  "[TEST] Patching endpoint to return INUSE\n");
                endpoint_interface->io_routines = &patched_io_routines;
            }

            /* Enable mock to return INUSE */
            return_inuse = 1;

            /* Create session with patched routines */
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
                              "[TEST] Creating session with patched endpoint\n");

            status = switch_ivr_originate(NULL, &session, &cause,
                                         "null/+15553334444",
                                         0, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);

            /* DON'T restore yet - keep it patched during read_frame call */

            if (status != SWITCH_STATUS_SUCCESS || !session) {
                /* Restore on error path */
                endpoint_interface->io_routines = &original_io_routines;
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
                                  "[TEST] Failed to create session: status=%d, cause=%d\n",
                                  status, cause);
                fst_requires(0);
            }

            g_session = session;
            channel = switch_core_session_get_channel(session);

            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
                              "[TEST] Session created with patched routines, codec=%p\n",
                              (void*)switch_core_session_get_read_codec(session));

            /* Add media bug with CONTINUE_ON_HOLD flag - this is REQUIRED */
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
                              "[TEST] Adding media bug with CONTINUE_ON_HOLD flag\n");

            status = switch_core_media_bug_add(session, "test_bug", NULL,
                                               bug_callback, NULL, 0,
                                               SMBF_CONTINUE_ON_HOLD | SMBF_READ_STREAM,
                                               &bug);
            fst_requires(status == SWITCH_STATUS_SUCCESS);

            /* Start Thread B */
            switch_threadattr_create(&thd_attr, fst_pool);
            switch_thread_create(&thread, thd_attr, unset_codec_thread, NULL, fst_pool);

            /* This should crash */
            status = switch_core_session_read_frame(session, &frame, SWITCH_IO_FLAG_NONE, 0);

            /* NOW restore original after read_frame completed */
            endpoint_interface->io_routines = &original_io_routines;

            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
                              "[TEST] read_frame returned %d\n", status);

            if (unset_done) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
                                  "[TEST] !!! NO CRASH - Bug fixed or timing issue\n");
            } else {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
                                  "[TEST] !!! Codec wasn't unset - timing issue\n");
            }

            /* Cleanup */
            return_inuse = 0;

            /* Join thread before cleaning up session */
            switch_thread_join(&thread_status, thread);

            if (bug) {
                switch_core_media_bug_remove(session, &bug);
            }

            switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);

            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
                              "========== TEST COMPLETED ==========\n\n");
        }
        FST_TEST_END()
    }
    FST_SUITE_END()
}
FST_CORE_END()
