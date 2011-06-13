struct dtmf_ss {
	char dtmf_stored[128];
	int dtmf_received;
	char dtmf_accepted[16][128];
	int result;
	switch_bool_t audio_stopped;
	switch_bool_t recorded_audio;
	const char *potentialMatch;
	int potentialMatchCount;
	const char *completeMatch;
	char terminate_key;
};
typedef struct dtmf_ss dtmf_ss_t;

#define RES_WAITFORMORE 0
#define RES_FOUND 1
#define RES_INVALID 3
#define RES_TIMEOUT 4
#define RES_BREAK 5
#define RES_RECORD 6
#define RES_BUFFER_OVERFLOW 99

#define MAX_DTMF_SIZE_OPTION 32

switch_status_t captureMenu(switch_core_session_t *session, dtmf_ss_t *loc, const char *macro_name,  const char *data, switch_event_t *event, const char *lang, int timeout);
switch_status_t captureMenuRecord(switch_core_session_t *session, dtmf_ss_t *loc, switch_event_t *event, const char *file_path, switch_file_handle_t *fh, int max_record_len);
switch_status_t captureMenuInitialize(dtmf_ss_t *loc, char **dtmf_accepted);

switch_status_t playbackBufferDTMF(switch_core_session_t *session, const char *macro_name,  const char *data, switch_event_t *event, const char *lang, int timeout);

