/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 *
 * Viktor Krykun <v.krikun at zfoneproject.com>
 */

#include "zrtp.h"

/** libzrtp test elements identifier */
typedef uint32_t zrtp_test_id_t;

/** Defines constant for unknown test element identifier */
#define ZRTP_TEST_UNKNOWN_ID 0

/** Default lengths for libzrtp test string buffers */
#define ZRTP_TEST_STR_LEN 128

/** libzrtp test endpoint configuration */
typedef struct {
	zrtp_config_t 			zrtp;				/** libzrtp global configuration parameters */
	unsigned 				generate_traffic;	/** switch On to emulate RTP/RTCP traffic generation. Off by default. */
} zrtp_test_endpoint_cfg_t;

/** ZRTP test session parameters*/
typedef struct {
	zrtp_profile_t 			zrtp;				/** libzrtp session parameters */
	unsigned 				streams_count;		/** number of zrtp streams to be attached to the session */
	zrtp_signaling_role_t	role;				/** signaling role, default is ZRTP_SIGNALING_ROLE_UNKNOWN */
	unsigned				is_enrollment;		/** true if enrollment session should be created */
} zrtp_test_session_cfg_t;

/** ZRTP test stream info */
typedef struct {
	zrtp_stream_info_t 		zrtp;				/** libzrtp stream info */
	unsigned 				zrtp_events_queueu[128]; /** list of received zrtp events*/
	unsigned 				zrtp_events_count; 	/** number of received events */
} zrtp_test_stream_info_t;

/** ZRTP test session state snapshot */
typedef struct {
	zrtp_session_info_t 	zrtp;				/** libzrtp session info*/
	zrtp_test_stream_info_t	streams[ZRTP_MAX_STREAMS_PER_SESSION]; /** array of attached streams info */
	unsigned 				streams_count;		/** number streams attached to the session */
} zrtp_test_session_info_t;

/** *ZRTP test channel state */
typedef struct {
	zrtp_test_stream_info_t left;				/** one-leg zrtp stream */
	zrtp_test_stream_info_t right;				/** second-leg zrtp stream */
	unsigned  char 			is_secure;			/** enabled when both streams in the channel are secure */
} zrtp_test_channel_info_t;


/**
 * Initialize zrtp test endpoint configuration with default values
 * @param cfg	- endpoint config to initialize
 */
void zrtp_test_endpoint_config_defaults(zrtp_test_endpoint_cfg_t *cfg);

/**
 * ZRTP test endpoint constructor
 * One endpoint is created, it starts processing threads and ready to emulate ZRTP exchange.
 *
 * @param cfg 	- endpoint configuration
 * @param name	- endpoint name for debug purposes and cache naming, e.h "Alice", "Bob".
 * @param id	- just created endpoint identifier will be placed here
 *
 * @return zrtp_status_ok on success or some of zrtp_status_t error codes on failure
 */
zrtp_status_t zrtp_test_endpoint_create(zrtp_test_endpoint_cfg_t *cfg,
										const char *name,
										zrtp_test_id_t *id);

/**
 * ZRTP test endpoint destructor
 * zrtp_test_endpoint_destroy() stops processing threads and release all
 * recurses allocated in zrtp_test_endpoint_create().
 *
 * @param id	- endpoint identifier
 * @return zrtp_status_ok on success or some of zrtp_status_t error codes on failure
 */
zrtp_status_t zrtp_test_endpoint_destroy(zrtp_test_id_t id);

/**
 * Enables test session config with default values
 * @param cfg	- session config for initialization
 */
void zrtp_test_session_config_defaults(zrtp_test_session_cfg_t *cfg);

/**
 * Create zrtp test session
 *
 * @param endpoint	- test endpoint creating endpoint should belong to
 * @param cfg		- session parameters
 * @param id		- created session identifier will be placed here
 * @return zrtp_status_ok on success or some of zrtp_status_t error codes on failure
 */
zrtp_status_t zrtp_test_session_create(zrtp_test_id_t endpoint,
									   zrtp_test_session_cfg_t *cfg,
									   zrtp_test_id_t *id);

zrtp_status_t zrtp_test_session_destroy(zrtp_test_id_t id);

zrtp_status_t zrtp_test_session_get(zrtp_test_id_t id, zrtp_test_session_info_t *info);

/**
 * Get stream Id by it's index in zrtp session
 *
 * @param session_id	- zrtp test session id where needed stream should be taken
 * @param idx		- stream index
 * @return found stream id, or ZRTP_TEST_UNKNOWN_ID if idex is out of stream array range
 */
zrtp_test_id_t zrtp_test_session_get_stream_by_idx(zrtp_test_id_t session_id, unsigned idx);

zrtp_status_t zrtp_test_stream_get(zrtp_test_id_t id, zrtp_test_stream_info_t *info);

zrtp_status_t zrtp_test_channel_create(zrtp_test_id_t left_stream, zrtp_test_id_t right_stream, zrtp_test_id_t *id);
zrtp_status_t zrtp_test_channel_create2(zrtp_test_id_t left_session, zrtp_test_id_t right_session, unsigned stream_idx, zrtp_test_id_t *id);
zrtp_status_t zrtp_test_channel_destroy(zrtp_test_id_t id);
zrtp_status_t zrtp_test_channel_start(zrtp_test_id_t id);
zrtp_status_t zrtp_test_channel_get(zrtp_test_id_t id, zrtp_test_channel_info_t *info);

zrtp_stream_t *zrtp_stream_for_test_stream(zrtp_test_id_t stream_id);

unsigned zrtp_stream_did_event_receive(zrtp_test_id_t stream_id, unsigned event);


