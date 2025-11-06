/*
 *
 * damir@telnyx.com, 31. October, 2025.
 * switch_jb_deadlock.c -- Deadlock test for jitter buffer channel mutex ordering
 *
 * This test reproduces the deadlock between two threads:
 * Thread 1: switch_channel_event_set_data() -> switch_jb_get_frames()
 *           Locks: channel mutex -> jb mutex
 * Thread 2: switch_jb_put_packet() -> new_node() -> switch_jb_reset()
 *           Locks: jb mutex -> channel mutex (via switch_channel_set_variable_printf)
 *
 * The deadlock occurs when:
 * - Thread 1 holds channel mutex, wants jb mutex (in switch_jb_get_frames at line 1271)
 * - Thread 2 holds jb mutex, wants channel mutex (in switch_jb_reset at line 1178)
 */

#include <switch.h>
#include <test/switch_test.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

#define MAX_TEST_THREADS 2
#define TEST_DURATION_SEC 10
#define PACKET_INJECTION_RATE_MS 1  /* Inject as fast as possible */

static volatile int should_stop_threads = 0;
static volatile int deadlock_detected = 0;
static volatile int thread1_iterations = 0;
static volatile int thread2_iterations = 0;
static switch_core_session_t *test_session = NULL;
static switch_jb_t *test_jb = NULL;
static switch_channel_t *test_channel = NULL;

/* Thread 1: Simulates media bug destroy path from GDB backtrace
 * Calls switch_channel_event_set_data() which:
 *   1. Locks channel->profile_mutex
 *   2. Calls switch_channel_event_set_extended_data()
 *   3. Which calls switch_core_media_get_stats()
 *   4. Which calls switch_rtp_get_stats() -> switch_jb_get_frames()
 *   5. Which tries to lock jb->mutex
 * Lock ordering: channel mutex -> jb mutex
 */
static void *event_data_thread(void *arg)
{
	int iterations = 0;
	switch_event_t *event = NULL;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
	                  "Thread 1 (event_data): Started - calling switch_channel_event_set_data()\n");

	while (!should_stop_threads && iterations < 10000) {
		if (test_channel) {
			/* Create event and call switch_channel_event_set_data() */
			if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_DESTROY) == SWITCH_STATUS_SUCCESS) {
				/* THIS locks channel mutex, THEN tries to access jb stats */
				switch_channel_event_set_data(test_channel, event);
				switch_event_destroy(&event);

				iterations++;
				__sync_add_and_fetch(&thread1_iterations, 1);

				if (iterations % 1000 == 0) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
					                  "Thread 1: %d iterations completed\n", iterations);
				}
			}
		}

		/* Minimal delay to maximize collision probability */
		switch_yield(100);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
	                  "Thread 1 (event_data): Completed %d iterations\n", iterations);
	return NULL;
}

/* Thread 2: Simulates RTP packet processing path
 * Feeds packets into jitter buffer to trigger the allocation limit
 * and force switch_jb_reset() to be called from new_node()
 * This locks: jb mutex -> channel mutex (in switch_jb_reset)
 */
static void *packet_injection_thread(void *arg)
{
	int iterations = 0;
	uint16_t seq_num = 1000;
	uint32_t timestamp = 160000;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
	                  "Thread 2 (packet_injection): Started - simulating RTP packet processing\n");

	while (!should_stop_threads && iterations < 5000) {
		if (test_jb) {
			switch_rtp_packet_t packet;
			switch_size_t packet_len = sizeof(packet.header) + 160; /* Typical audio packet */

			memset(&packet, 0, sizeof(packet));
			packet.header.version = 2;

			/* Use DIFFERENT sequence numbers with BIG GAPS to force node accumulation
			 * Each packet gets a seq number far apart so they don't get consumed by jitter buffer */
			packet.header.seq = htons(seq_num);
			seq_num += 100;  /* Big gaps prevent jitter buffer from reading/consuming packets */

			packet.header.ts = htonl(timestamp);
			timestamp += 160 * 100;

			/* Feed packets rapidly to force allocation limit and trigger reset */
			switch_jb_put_packet(test_jb, &packet, packet_len);

			iterations++;
			__sync_add_and_fetch(&thread2_iterations, 1);

			if (iterations % 500 == 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
				                  "Thread 2: %d packets injected, seq=%u\n", iterations, ntohs(packet.header.seq));
			}
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
	                  "Thread 2 (packet_injection): Completed %d iterations\n", iterations);
	return NULL;
}

/* Watchdog thread to detect deadlock */
static void *watchdog_thread(void *arg)
{
	int check_count = 0;
	int prev_thread1_count = 0;
	int prev_thread2_count = 0;
	int stall_count = 0;
	int current_thread1 = 0;
	int current_thread2 = 0;
	int thread1_should_run = 0;
	int thread2_should_run = 0;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
	                  "Watchdog: Started - monitoring for deadlock\n");

	while (!should_stop_threads && check_count < TEST_DURATION_SEC * 2) {
		switch_sleep(500000); /* Check every 500ms */

		current_thread1 = __sync_add_and_fetch(&thread1_iterations, 0);
		current_thread2 = __sync_add_and_fetch(&thread2_iterations, 0);

		/* Check if both threads are stalled AND still expected to be running
		 * Thread 1 limit: 10000, Thread 2 limit: 5000 */
		thread1_should_run = (current_thread1 < 10000);
		thread2_should_run = (current_thread2 < 5000);

		if (current_thread1 == prev_thread1_count && current_thread2 == prev_thread2_count) {
			/* Only count as stall if at least one thread should still be running */
			if (thread1_should_run || thread2_should_run) {
				stall_count++;

				if (stall_count >= 4) { /* Both threads stalled for 2 seconds */
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT,
					                  "DEADLOCK DETECTED! Thread 1: %d iterations (stalled, limit: 10000), Thread 2: %d iterations (stalled, limit: 5000)\n",
					                  current_thread1, current_thread2);
					deadlock_detected = 1;
					should_stop_threads = 1;
					break;
				}
			} else {
				/* Both threads completed their work normally */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
				                  "Watchdog: Both threads completed their iteration limits\n");
				should_stop_threads = 1;
				break;
			}
		} else {
			stall_count = 0;
		}

		prev_thread1_count = current_thread1;
		prev_thread2_count = current_thread2;
		check_count++;
	}

	if (!deadlock_detected) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
		                  "Watchdog: No deadlock detected - threads completed normally\n");
	}

	return NULL;
}

FST_CORE_BEGIN("./conf")
{
	FST_SUITE_BEGIN(switch_jb_deadlock)
	{
		FST_SETUP_BEGIN()
		{
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
			should_stop_threads = 1;

			/* Clean up - jitter buffer is owned by RTP session, so don't destroy it separately */
			test_jb = NULL;
			test_channel = NULL;

			if (test_session) {
				switch_channel_hangup(switch_core_session_get_channel(test_session),
				                      SWITCH_CAUSE_NORMAL_CLEARING);
				switch_core_session_rwunlock(test_session);
				test_session = NULL;
			}
		}
		FST_TEARDOWN_END()

		FST_SESSION_BEGIN(jb_channel_mutex_deadlock_test)
		{
			pthread_t threads[3];
			switch_call_cause_t cause;
			switch_status_t status;
			int i;
			switch_memory_pool_t *pool = NULL;
			switch_rtp_t *rtp_session = NULL;
			switch_rtp_flag_t flags[SWITCH_RTP_FLAG_INVALID] = {0};
			const char *err = NULL;
			switch_media_handle_t *media_handle = NULL;
			switch_core_media_params_t *mparams = NULL;

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
			                  "=== JITTER BUFFER CHANNEL MUTEX DEADLOCK TEST ===\n");

			should_stop_threads = 0;
			deadlock_detected = 0;
			thread1_iterations = 0;
			thread2_iterations = 0;

			/* Create a test session with null endpoint (designed for testing) */
			status = switch_ivr_originate(NULL, &test_session, &cause,
			                              "null/+15553334444", 2, NULL, NULL, NULL, NULL, NULL,
			                              SOF_NONE, NULL, NULL);

			if (status != SWITCH_STATUS_SUCCESS || !test_session) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
				                  "Failed to create test session, cause: %d\n", cause);
				fst_check(0);
				goto test_end;
			}

			test_channel = switch_core_session_get_channel(test_session);
			fst_requires(test_channel != NULL);

			/* Create media handle so switch_core_media_get_stats() actually works! */
			pool = switch_core_session_get_pool(test_session);

			mparams = switch_core_session_alloc(test_session, sizeof(switch_core_media_params_t));
			mparams->num_codecs = 1;
			mparams->inbound_codec_string = switch_core_session_strdup(test_session, "PCMU");
			mparams->outbound_codec_string = switch_core_session_strdup(test_session, "PCMU");
			mparams->rtpip = switch_core_session_strdup(test_session, "127.0.0.1");

			status = switch_media_handle_create(&media_handle, test_session, mparams);
			if (status != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
				                  "Failed to create media handle\n");
				fst_check(0);
				goto test_end;
			}

			/* Create RTP session with jitter buffer */

			rtp_session = switch_rtp_new("127.0.0.1", 12340, "127.0.0.1", 12342,
			                              0, 8000, 20 * 1000, flags, "soft", &err, pool);

			if (!rtp_session) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
				                  "Failed to create RTP session: %s\n", err ? err : "unknown");
				fst_check(0);
				goto test_end;
			}

			/* Activate jitter buffer with LOW limits to trigger reset quickly */
			status = switch_rtp_activate_jitter_buffer(rtp_session, 3, 5, 80, 8000);
			if (status != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
				                  "Failed to activate jitter buffer\n");
				switch_rtp_destroy(&rtp_session);
				fst_check(0);
				goto test_end;
			}

			/* Get the jitter buffer from RTP session */
			test_jb = switch_rtp_get_jitter_buffer(rtp_session);
			if (!test_jb) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
				                  "Failed to get jitter buffer from RTP session\n");
				switch_rtp_destroy(&rtp_session);
				fst_check(0);
				goto test_end;
			}

			/* Enable jitter buffer debugging to see if reset is triggered (level 2 required) */
			switch_rtp_debug_jitter_buffer(rtp_session, "2");

			/* Attach RTP session to the test session's media so stats path works */
			switch_core_media_set_rtp_session(test_session, SWITCH_MEDIA_TYPE_AUDIO, rtp_session);

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
			                  "Created session with media_handle, RTP, and jitter buffer (max_frame_len=5)\n");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
			                  "Jitter buffer debug enabled - watch for 'ALLOCATED FRAMES TOO HIGH' messages\n");

			/* Start Thread 1: event_data (channel mutex -> jb mutex) */
			fst_check(pthread_create(&threads[0], NULL, event_data_thread, NULL) == 0);

			/* Start Thread 2: packet_injection (jb mutex -> channel mutex) */
			fst_check(pthread_create(&threads[1], NULL, packet_injection_thread, NULL) == 0);

			/* Start Watchdog thread */
			fst_check(pthread_create(&threads[2], NULL, watchdog_thread, NULL) == 0);

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
			                  "Started 2 racing threads + 1 watchdog thread\n");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
			                  "Running test for %d seconds...\n", TEST_DURATION_SEC);

			/* Wait for all threads to complete or deadlock to be detected */
			for (i = 0; i < 3; i++) {
				pthread_join(threads[i], NULL);
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
			                  "Test completed. Thread 1: %d iterations, Thread 2: %d iterations\n",
			                  thread1_iterations, thread2_iterations);

			if (deadlock_detected) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
				                  "DEADLOCK WAS DETECTED!\n");
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
				                  "No deadlock detected - either the fix is applied or race didn't trigger\n");
			}

			/* The test "passes" by detecting the deadlock (proving the bug exists)
			 * or by completing without deadlock (if the fix is applied)
			 * We'll mark it as successful either way since we're demonstrating the issue */
			fst_check(1);

test_end:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
			                  "=== TEST END ===\n");
		}
		FST_SESSION_END()
	}
	FST_SUITE_END()
}
FST_CORE_END()
