/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Neal Horman <neal at wanlink dot com>
 * Bret McDanel <trixter at 0xdecafbad dot com>
 * Dale Thatcher <freeswitch at dalethatcher dot com>
 * Chris Danielson <chris at maxpowersoft dot com>
 * Rupa Schomaker <rupa@rupa.com>
 * David Weekly <david@weekly.org>
 * Joao Mesquita <jmesquita@gmail.com>
 * Raymond Chandler <intralanman@freeswitch.org>
 * Seven Du <dujinfang@gmail.com>
 * Emmanuel Schmidbauer <e.schmidbauer@gmail.com>
 *
 * mod_conference.c -- Software Conference Bridge
 *
 */
#include <switch.h>

#ifdef OPENAL_POSITIONING
#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#endif

#define DEFAULT_AGC_LEVEL 1100
#define CONFERENCE_UUID_VARIABLE "conference_uuid"

SWITCH_MODULE_LOAD_FUNCTION(mod_conference_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_conference_shutdown);
SWITCH_MODULE_DEFINITION(mod_conference, mod_conference_load, mod_conference_shutdown, NULL);

struct conf_fps {
	float fps;
	int ms;
	int samples;
};

static struct conf_fps FPS_VALS[] = { 
	{1.0f, 1000, 90},
	{5.0f, 200, 450},
	{10.0f, 100, 900},
	{15.0f, 66, 1364},
	{16.60f, 60, 1500},
	{20.0f, 50, 4500},
	{25.0f, 40, 2250},
	{30.0f, 33, 2700},
	{33.0f, 30, 2790},
	{66.60f, 15, 6000},
	{100.0f, 10, 9000},
	{0,0,0}
};


typedef enum {
	CONF_SILENT_REQ = (1 << 0),
	CONF_SILENT_DONE = (1 << 1)
} conf_app_flag_t;

static const char global_app_name[] = "conference";
static char *global_cf_name = "conference.conf";
static char *cf_pin_url_param_name = "X-ConfPin=";
static char *api_syntax;
static int EC = 0;

/* Size to allocate for audio buffers */
#define CONF_BUFFER_SIZE 1024 * 128
#define CONF_EVENT_MAINT "conference::maintenance"
#define CONF_EVENT_CDR "conference::cdr"
#define CONF_DEFAULT_LEADIN 20

#define CONF_DBLOCK_SIZE CONF_BUFFER_SIZE
#define CONF_DBUFFER_SIZE CONF_BUFFER_SIZE
#define CONF_DBUFFER_MAX 0
#define CONF_CHAT_PROTO "conf"

#ifndef MIN
#define MIN(a, b) ((a)<(b)?(a):(b))
#endif

/* the rate at which the infinite impulse response filter on speaker score will decay. */
#define SCORE_DECAY 0.8
/* the maximum value for the IIR score [keeps loud & longwinded people from getting overweighted] */
#define SCORE_MAX_IIR 25000
/* the minimum score for which you can be considered to be loud enough to now have the floor */
#define SCORE_IIR_SPEAKING_MAX 300
/* the threshold below which you cede the floor to someone loud (see above value). */
#define SCORE_IIR_SPEAKING_MIN 100
/* the FPS of the conference canvas */
#define FPS 30
/* max supported layers in one mcu */
#define MCU_MAX_LAYERS 64

/* video layout scale factor */
#define VIDEO_LAYOUT_SCALE 360.0f

#define CONFERENCE_MUX_DEFAULT_LAYOUT "group:grid"
#define CONFERENCE_MUX_DEFAULT_SUPER_LAYOUT "grid"
#define CONFERENCE_CANVAS_DEFAULT_WIDTH 1280
#define CONFERENCE_CANVAS_DEFAULT_HIGHT 720
#define MAX_CANVASES 20
#define SUPER_CANVAS_ID MAX_CANVASES
#define test_eflag(conference, flag) ((conference)->eflags & flag)

typedef enum {
	FILE_STOP_CURRENT,
	FILE_STOP_ALL,
	FILE_STOP_ASYNC
} file_stop_t;

/* Global Values */
static struct {
	switch_memory_pool_t *conference_pool;
	switch_mutex_t *conference_mutex;
	switch_hash_t *conference_hash;
	switch_mutex_t *id_mutex;
	switch_mutex_t *hash_mutex;
	switch_mutex_t *setup_mutex;
	uint32_t id_pool;
	int32_t running;
	uint32_t threads;
	switch_event_channel_id_t event_channel_id;
} globals;

/* forward declaration for conference_obj and caller_control */
struct conference_member;
typedef struct conference_member conference_member_t;

struct caller_control_actions;

typedef struct caller_control_actions {
	char *binded_dtmf;
	char *data;
	char *expanded_data;
} caller_control_action_t;

typedef struct caller_control_menu_info {
	switch_ivr_menu_t *stack;
	char *name;
} caller_control_menu_info_t;

typedef enum {
	MFLAG_RUNNING,
	MFLAG_CAN_SPEAK,
	MFLAG_CAN_HEAR,
	MFLAG_KICKED,
	MFLAG_ITHREAD,
	MFLAG_NOCHANNEL,
	MFLAG_INTREE,
	MFLAG_NO_MINIMIZE_ENCODING,
	MFLAG_FLUSH_BUFFER,
	MFLAG_ENDCONF,
	MFLAG_HAS_AUDIO,
	MFLAG_TALKING,
	MFLAG_RESTART,
	MFLAG_MINTWO,
	MFLAG_MUTE_DETECT,
	MFLAG_DIST_DTMF,
	MFLAG_MOD,
	MFLAG_INDICATE_MUTE,
	MFLAG_INDICATE_UNMUTE,
	MFLAG_NOMOH,
	MFLAG_VIDEO_BRIDGE,
	MFLAG_INDICATE_MUTE_DETECT,
	MFLAG_PAUSE_RECORDING,
	MFLAG_ACK_VIDEO,
	MFLAG_GHOST,
	MFLAG_JOIN_ONLY,
	MFLAG_POSITIONAL,
	MFLAG_NO_POSITIONAL,
	MFLAG_JOIN_VID_FLOOR,
	MFLAG_RECEIVING_VIDEO,
	MFLAG_CAN_BE_SEEN,
	MFLAG_SECOND_SCREEN,
	MFLAG_SILENT,
	///////////////////////////
	MFLAG_MAX
} member_flag_t;

typedef enum {
	CFLAG_RUNNING,
	CFLAG_DYNAMIC,
	CFLAG_ENFORCE_MIN,
	CFLAG_DESTRUCT,
	CFLAG_LOCKED,
	CFLAG_ANSWERED,
	CFLAG_BRIDGE_TO,
	CFLAG_WAIT_MOD,
	CFLAG_VID_FLOOR,
	CFLAG_WASTE_FLAG,
	CFLAG_OUTCALL,
	CFLAG_INHASH,
	CFLAG_EXIT_SOUND,
	CFLAG_ENTER_SOUND,
	CFLAG_USE_ME,
	CFLAG_AUDIO_ALWAYS,
	CFLAG_ENDCONF_FORCED,
	CFLAG_RFC4579,
	CFLAG_FLOOR_CHANGE,
	CFLAG_VID_FLOOR_LOCK,
	CFLAG_JSON_EVENTS,
	CFLAG_LIVEARRAY_SYNC,
	CFLAG_CONF_RESTART_AUTO_RECORD,
	CFLAG_POSITIONAL,
	CFLAG_TRANSCODE_VIDEO,
	CFLAG_VIDEO_MUXING,
	CFLAG_MINIMIZE_VIDEO_ENCODING,
	CFLAG_MANAGE_INBOUND_VIDEO_BITRATE,
	CFLAG_JSON_STATUS,
	CFLAG_VIDEO_BRIDGE_FIRST_TWO,
	CFLAG_VIDEO_REQUIRED_FOR_CANVAS,
	CFLAG_PERSONAL_CANVAS,
	/////////////////////////////////
	CFLAG_MAX
} conference_flag_t;

typedef struct conference_cdr_node_s {
	switch_caller_profile_t *cp;
	char *record_path;
	switch_time_t join_time;
	switch_time_t leave_time;
	member_flag_t mflags[MFLAG_MAX];
	uint32_t id;
	conference_member_t *member;
	switch_event_t *var_event;
	struct conference_cdr_node_s *next;
} conference_cdr_node_t;

typedef enum {
	CDRR_LOCKED = 1,
	CDRR_PIN,
	CDRR_MAXMEMBERS
} cdr_reject_reason_t;

typedef struct conference_cdr_reject_s {
	switch_caller_profile_t *cp;
	switch_time_t reject_time;
	cdr_reject_reason_t reason;
	struct conference_cdr_reject_s *next;
} conference_cdr_reject_t;

typedef enum {
	CDRE_NONE,
	CDRE_AS_CONTENT,
	CDRE_AS_FILE
} cdr_event_mode_t;


struct call_list {
	char *string;
	int iteration;
	struct call_list *next;
};
typedef struct call_list call_list_t;



typedef enum {
	RFLAG_CAN_SPEAK = (1 << 0),
	RFLAG_CAN_HEAR = (1 << 1),
	RFLAG_CAN_SEND_VIDEO = (1 << 2)
} relation_flag_t;

typedef enum {
	NODE_TYPE_FILE,
	NODE_TYPE_SPEECH
} node_type_t;

typedef enum {
	NFLAG_NONE = (1 << 0),
	NFLAG_PAUSE = (1 << 1)
} node_flag_t;

typedef enum {
	EFLAG_ADD_MEMBER = (1 << 0),
	EFLAG_DEL_MEMBER = (1 << 1),
	EFLAG_ENERGY_LEVEL = (1 << 2),
	EFLAG_VOLUME_LEVEL = (1 << 3),
	EFLAG_GAIN_LEVEL = (1 << 4),
	EFLAG_DTMF = (1 << 5),
	EFLAG_STOP_TALKING = (1 << 6),
	EFLAG_START_TALKING = (1 << 7),
	EFLAG_MUTE_MEMBER = (1 << 8),
	EFLAG_UNMUTE_MEMBER = (1 << 9),
	EFLAG_DEAF_MEMBER = (1 << 10),
	EFLAG_UNDEAF_MEMBER = (1 << 11),
	EFLAG_KICK_MEMBER = (1 << 12),
	EFLAG_DTMF_MEMBER = (1 << 13),
	EFLAG_ENERGY_LEVEL_MEMBER = (1 << 14),
	EFLAG_VOLUME_IN_MEMBER = (1 << 15),
	EFLAG_VOLUME_OUT_MEMBER = (1 << 16),
	EFLAG_PLAY_FILE = (1 << 17),
	EFLAG_PLAY_FILE_MEMBER = (1 << 18),
	EFLAG_SPEAK_TEXT = (1 << 19),
	EFLAG_SPEAK_TEXT_MEMBER = (1 << 20),
	EFLAG_LOCK = (1 << 21),
	EFLAG_UNLOCK = (1 << 22),
	EFLAG_TRANSFER = (1 << 23),
	EFLAG_BGDIAL_RESULT = (1 << 24),
	EFLAG_FLOOR_CHANGE = (1 << 25),
	EFLAG_MUTE_DETECT = (1 << 26),
	EFLAG_RECORD = (1 << 27),
	EFLAG_HUP_MEMBER = (1 << 28),
	EFLAG_PLAY_FILE_DONE = (1 << 29),
	EFLAG_SET_POSITION_MEMBER = (1 << 30)
} event_type_t;

#ifdef OPENAL_POSITIONING
typedef struct al_handle_s {
	switch_mutex_t *mutex;
	ALCdevice *device;
	ALCcontext *context;
	ALuint source;
	ALuint buffer_in[2];
	int setpos;
	ALfloat pos_x;
	ALfloat pos_y;
	ALfloat pos_z;
} al_handle_t;
#else 
typedef struct al_handle_s {
	int unsupported;
	switch_mutex_t *mutex;
} al_handle_t;
#endif
struct conference_obj;

typedef struct conference_file_node {
	switch_file_handle_t fh;
	switch_speech_handle_t *sh;
	node_flag_t flags;
	node_type_t type;
	uint8_t done;
	uint8_t async;
	switch_memory_pool_t *pool;
	uint32_t leadin;
	struct conference_file_node *next;
	char *file;
	switch_bool_t mux;
	uint32_t member_id;
	al_handle_t *al;
	int layer_id;
	int canvas_id;
	struct conference_obj *conference;
} conference_file_node_t;

typedef enum {
	REC_ACTION_STOP = 1,
	REC_ACTION_PAUSE,
	REC_ACTION_RESUME
} recording_action_type_t;

/* conference xml config sections */
typedef struct conf_xml_cfg {
	switch_xml_t profile;
	switch_xml_t controls;
} conf_xml_cfg_t;

struct vid_helper {
	conference_member_t *member_a;
	conference_member_t *member_b;
	int up;
};


typedef struct mcu_layer_geometry_s {
	int x;
	int y;
	int scale;
	int floor;
	int flooronly;
	int fileonly;
	int overlap;
	char *res_id;
	char *audio_position;
} mcu_layer_geometry_t;

typedef struct mcu_layer_def_s {
	char *name;
	mcu_layer_geometry_t layers[MCU_MAX_LAYERS];
} mcu_layer_def_t;

struct mcu_canvas_s;

typedef struct mcu_layer_s {
	mcu_layer_geometry_t geometry;
	int member_id;
	int idx;
	int tagged;
	int bugged;
	int screen_w;
	int screen_h;
	int x_pos;
	int y_pos;
	int banner_patched;
	int mute_patched;
	int avatar_patched;
	int refresh;
	int is_avatar;
	switch_img_position_t logo_pos;
	switch_image_t *img;
	switch_image_t *cur_img;
	switch_image_t *banner_img;
	switch_image_t *logo_img;
	switch_image_t *logo_text_img;
	switch_image_t *mute_img;
	switch_img_txt_handle_t *txthandle;
	conference_file_node_t *fnode;
	struct mcu_canvas_s *canvas;
} mcu_layer_t;

typedef struct video_layout_s {
	char *name;
	char *audio_position;
	mcu_layer_geometry_t images[MCU_MAX_LAYERS];
	int layers;
} video_layout_t;

typedef struct video_layout_node_s {
	video_layout_t *vlayout;
	struct video_layout_node_s *next;
} video_layout_node_t;

typedef struct layout_group_s {
	video_layout_node_t *layouts;
} layout_group_t;

typedef struct mcu_canvas_s {
	int width;
	int height;
	switch_image_t *img;
	mcu_layer_t layers[MCU_MAX_LAYERS];
	int total_layers;
	int layers_used;
	int layout_floor_id;
	int refresh;
	int send_keyframe;
	int play_file;
	switch_rgb_color_t bgcolor;
	switch_rgb_color_t letterbox_bgcolor;
	switch_mutex_t *mutex;
	switch_timer_t timer;
	switch_memory_pool_t *pool;
	video_layout_t *vlayout;
	video_layout_t *new_vlayout;
	int canvas_id;
	struct conference_obj *conference;
	switch_thread_t *video_muxing_thread;
	int video_timer_reset;
	switch_queue_t *video_queue;
	int32_t video_write_bandwidth;
} mcu_canvas_t;

/* Record Node */
typedef struct conference_record {
	struct conference_obj *conference;
	char *path;
	switch_memory_pool_t *pool;
	switch_bool_t autorec;
	struct conference_record *next;
	switch_file_handle_t fh;
} conference_record_t;

typedef enum {
	CONF_VIDEO_MODE_PASSTHROUGH,
	CONF_VIDEO_MODE_TRANSCODE,
	CONF_VIDEO_MODE_MUX
} conf_video_mode_t;

/* Conference Object */
typedef struct conference_obj {
	char *name;
	char *la_name;
	char *la_event_channel;
	char *mod_event_channel;
	char *desc;
	char *timer_name;
	char *tts_engine;
	char *tts_voice;
	char *enter_sound;
	char *exit_sound;
	char *alone_sound;
	char *perpetual_sound;
	char *moh_sound;
	char *muted_sound;
	char *mute_detect_sound;
	char *unmuted_sound;
	char *locked_sound;
	char *is_locked_sound;
	char *is_unlocked_sound;
	char *kicked_sound;
	char *join_only_sound;
	char *caller_id_name;
	char *caller_id_number;
	char *sound_prefix;
	char *special_announce;
	char *auto_record;
	char *record_filename;
	char *outcall_templ;
	char *video_layout_name;
	char *video_layout_group;
	char *video_canvas_bgcolor;
	char *video_super_canvas_bgcolor;
	char *video_letterbox_bgcolor;
	char *no_video_avatar;
	conf_video_mode_t conf_video_mode;
	int members_with_video;
	int members_with_avatar;
	switch_codec_settings_t video_codec_settings;
	uint32_t canvas_width;
	uint32_t canvas_height;
	uint32_t terminate_on_silence;
	uint32_t max_members;
	uint32_t doc_version;
	char *maxmember_sound;
	uint32_t announce_count;
	char *pin;
	char *mpin;
	char *pin_sound;
	char *bad_pin_sound;
	char *profile_name;
	char *domain;
	char *chat_id;
	char *caller_controls;
	char *moderator_controls;
	switch_live_array_t *la;
	conference_flag_t flags[CFLAG_MAX];
	member_flag_t mflags[MFLAG_MAX];
	switch_call_cause_t bridge_hangup_cause;
	switch_mutex_t *flag_mutex;
	uint32_t rate;
	uint32_t interval;
	uint32_t channels;
	switch_mutex_t *mutex;
	conference_member_t *members;
	conference_member_t *floor_holder;
	uint32_t video_floor_holder;
	uint32_t last_video_floor_holder;
	switch_mutex_t *member_mutex;
	conference_file_node_t *fnode;
	conference_file_node_t *async_fnode;
	switch_memory_pool_t *pool;
	switch_thread_rwlock_t *rwlock;
	uint32_t count;
	int32_t energy_level;
	uint8_t min;
	switch_speech_handle_t lsh;
	switch_speech_handle_t *sh;
	switch_byte_t *not_talking_buf;
	uint32_t not_talking_buf_len;
	int pin_retries;
	int broadcast_chat_messages;
	int comfort_noise_level;
	int auto_recording;
	int record_count;
	uint32_t min_recording_participants;
	int ivr_dtmf_timeout;
	int ivr_input_timeout;
	uint32_t eflags;
	uint32_t verbose_events;
	int end_count;
	uint32_t count_ghosts;
	/* allow extra time after 'endconf' member leaves */
	switch_time_t endconf_time;
	int endconf_grace_time;

	uint32_t relationship_total;
	uint32_t score;
	int mux_loop_count;
	int member_loop_count;
	int agc_level;

	uint32_t avg_score;
	uint32_t avg_itt;
	uint32_t avg_tally;
	switch_time_t run_time;
	char *uuid_str;
	uint32_t originating;
	switch_call_cause_t cancel_cause;
	conference_cdr_node_t *cdr_nodes;
	conference_cdr_reject_t *cdr_rejected;
	switch_time_t start_time;
	switch_time_t end_time;
	char *log_dir;
	cdr_event_mode_t cdr_event_mode;
	struct vid_helper vh[2];
	struct vid_helper mh;
	conference_record_t *rec_node_head;
	int last_speech_channels;
	mcu_canvas_t *canvas;
	mcu_canvas_t *canvases[MAX_CANVASES+1];
	int canvas_count;
	int super_canvas_label_layers;
	int super_canvas_show_all_layers;
	int canvas_running_count;
	switch_mutex_t *canvas_mutex;
	switch_hash_t *layout_hash;
	switch_hash_t *layout_group_hash;
	struct conf_fps video_fps;
	int playing_video_file;
	int recording_members;
	uint32_t video_floor_packets;
} conference_obj_t;

/* Relationship with another member */
typedef struct conference_relationship {
	uint32_t id;
	uint32_t flags;
	struct conference_relationship *next;
} conference_relationship_t;

/* Conference Member Object */
struct conference_member {
	uint32_t id;
	switch_core_session_t *session;
	switch_channel_t *channel;
	conference_obj_t *conference;
	switch_memory_pool_t *pool;
	switch_buffer_t *audio_buffer;
	switch_buffer_t *mux_buffer;
	switch_buffer_t *resample_buffer;
	member_flag_t flags[MFLAG_MAX];
	uint32_t score;
	uint32_t last_score;
	uint32_t score_iir;
	switch_mutex_t *flag_mutex;
	switch_mutex_t *write_mutex;
	switch_mutex_t *audio_in_mutex;
	switch_mutex_t *audio_out_mutex;
	switch_mutex_t *read_mutex;
	switch_mutex_t *fnode_mutex;
	switch_thread_rwlock_t *rwlock;
	switch_codec_implementation_t read_impl;
	switch_codec_implementation_t orig_read_impl;
	switch_codec_t read_codec;
	switch_codec_t write_codec;
	char *rec_path;
	switch_time_t rec_time;
	conference_record_t *rec;
	uint8_t *frame;
	uint8_t *last_frame;
	uint32_t frame_size;
	uint8_t *mux_frame;
	uint32_t read;
	uint32_t vol_period;
	int32_t energy_level;
	int32_t agc_volume_in_level;
	int32_t volume_in_level;
	int32_t volume_out_level;
	int32_t agc_concur;
	int32_t nt_tally;
	switch_time_t join_time;
	switch_time_t last_talking;
	uint32_t native_rate;
	switch_audio_resampler_t *read_resampler;
	int16_t *resample_out;
	uint32_t resample_out_len;
	conference_file_node_t *fnode;
	conference_relationship_t *relationships;
	switch_speech_handle_t lsh;
	switch_speech_handle_t *sh;
	uint32_t verbose_events;
	uint32_t avg_score;
	uint32_t avg_itt;
	uint32_t avg_tally;
	struct conference_member *next;
	switch_ivr_dmachine_t *dmachine;
	conference_cdr_node_t *cdr_node;
	char *kicked_sound;
	switch_queue_t *dtmf_queue;
	switch_queue_t *video_queue;
	switch_queue_t *mux_out_queue;
	switch_thread_t *video_muxing_write_thread;
	switch_thread_t *input_thread;
	cJSON *json;
	cJSON *status_field;
	uint8_t loop_loop;
	al_handle_t *al;
	int last_speech_channels;
	int video_layer_id;
	int canvas_id;
	int watching_canvas_id;
	int video_codec_index;
	int video_codec_id;
	char *video_banner_text;
	char *video_logo;
	char *video_mute_png;
	char *video_reservation_id;
	switch_media_flow_t video_flow;
	switch_frame_buffer_t *fb;
	switch_image_t *avatar_png_img;
	switch_image_t *video_mute_img;
	uint32_t floor_packets;
	int blanks;
	int managed_kps;
	int blackouts;
	int good_img;
	int auto_avatar;
	int avatar_patched;
	mcu_canvas_t *canvas;
	switch_image_t *pcanvas_img;
};

typedef enum {
	CONF_API_SUB_ARGS_SPLIT,
	CONF_API_SUB_MEMBER_TARGET,
	CONF_API_SUB_ARGS_AS_ONE
} conference_fntype_t;

typedef void (*void_fn_t) (void);

/* API command parser */
typedef struct api_command {
	char *pname;
	void_fn_t pfnapicmd;
	conference_fntype_t fntype;
	char *pcommand;
	char *psyntax;
} api_command_t;


/* Function Prototypes */
static int setup_media(conference_member_t *member, conference_obj_t *conference);
static uint32_t next_member_id(void);
static conference_relationship_t *member_get_relationship(conference_member_t *member, conference_member_t *other_member);
static conference_member_t *conference_member_get(conference_obj_t *conference, uint32_t id);
static conference_relationship_t *member_add_relationship(conference_member_t *member, uint32_t id);
static switch_status_t member_del_relationship(conference_member_t *member, uint32_t id);
static switch_status_t conference_add_member(conference_obj_t *conference, conference_member_t *member);
static switch_status_t conference_del_member(conference_obj_t *conference, conference_member_t *member);
static void *SWITCH_THREAD_FUNC conference_thread_run(switch_thread_t *thread, void *obj);
static void *SWITCH_THREAD_FUNC conference_video_muxing_thread_run(switch_thread_t *thread, void *obj);
static void *SWITCH_THREAD_FUNC conference_super_video_muxing_thread_run(switch_thread_t *thread, void *obj);
static void conference_loop_output(conference_member_t *member);
static uint32_t conference_stop_file(conference_obj_t *conference, file_stop_t stop);
static switch_status_t conference_play_file(conference_obj_t *conference, char *file, uint32_t leadin, switch_channel_t *channel, uint8_t async);
static void conference_send_all_dtmf(conference_member_t *member, conference_obj_t *conference, const char *dtmf);
static switch_status_t conference_say(conference_obj_t *conference, const char *text, uint32_t leadin);
static void conference_list(conference_obj_t *conference, switch_stream_handle_t *stream, char *delim);
static conference_obj_t *conference_find(char *name, char *domain);
static void member_bind_controls(conference_member_t *member, const char *controls);
static void conference_send_presence(conference_obj_t *conference);
static void conference_set_video_floor_holder(conference_obj_t *conference, conference_member_t *member, switch_bool_t force);
static void canvas_del_fnode_layer(conference_obj_t *conference, conference_file_node_t *fnode);
static void canvas_set_fnode_layer(mcu_canvas_t *canvas, conference_file_node_t *fnode, int idx);


static inline void conference_set_flag(conference_obj_t *conference, conference_flag_t flag)
{
	conference->flags[flag] = 1;
}
static inline void conference_set_flag_locked(conference_obj_t *conference, conference_flag_t flag)
{
	switch_mutex_lock(conference->flag_mutex);
	conference->flags[flag] = 1;
	switch_mutex_unlock(conference->flag_mutex);
}
static inline void conference_clear_flag(conference_obj_t *conference, conference_flag_t flag)
{
	conference->flags[flag] = 0;
}
static inline void conference_clear_flag_locked(conference_obj_t *conference, conference_flag_t flag)
{
	switch_mutex_lock(conference->flag_mutex);
	conference->flags[flag] = 0;
	switch_mutex_unlock(conference->flag_mutex);
}
static inline switch_bool_t conference_test_flag(conference_obj_t *conference, conference_flag_t flag)
{
	return !!conference->flags[flag];
}
static inline void conference_set_mflag(conference_obj_t *conference, member_flag_t mflag)
{
	conference->mflags[mflag] = 1;
}
static inline void conference_clear_mflag(conference_obj_t *conference, member_flag_t mflag)
{
	conference->mflags[mflag] = 0;
}
static inline switch_bool_t conference_test_mflag(conference_obj_t *conference, member_flag_t mflag)
{
	return !!conference->mflags[mflag];
}
static inline switch_bool_t cdr_test_mflag(conference_cdr_node_t *np, member_flag_t mflag)
{
	return !!np->mflags[mflag];
}
static inline void member_set_flag(conference_member_t *member, member_flag_t flag)
{
	member->flags[flag] = 1;
}
static inline void member_set_flag_locked(conference_member_t *member, member_flag_t flag)
{
	switch_mutex_lock(member->flag_mutex);
	member->flags[flag] = 1;
	switch_mutex_unlock(member->flag_mutex);
}
static inline void member_clear_flag(conference_member_t *member, member_flag_t flag)
{
	member->flags[flag] = 0;
}
static inline void member_clear_flag_locked(conference_member_t *member, member_flag_t flag)
{
	switch_mutex_lock(member->flag_mutex);
	member->flags[flag] = 0;
	switch_mutex_unlock(member->flag_mutex);
}
static inline switch_bool_t member_test_flag(conference_member_t *member, member_flag_t flag)
{
	return !!member->flags[flag];
}





SWITCH_STANDARD_API(conf_api_main);


static int conference_set_fps(conference_obj_t *conference, float fps)
{
	int i = 0, j = 0;

	for (i = 0; FPS_VALS[i].ms; i++) {
		if (FPS_VALS[i].fps == fps) {

			conference->video_fps = FPS_VALS[i];

			for (j = 0; j <= conference->canvas_count; j++) {
				if (conference->canvases[j]) {
					conference->canvases[j]->video_timer_reset = 1;
				}
			}

			return 1;
		}
	}

	return 0;
}

static switch_status_t conference_outcall(conference_obj_t *conference,
										  char *conference_name,
										  switch_core_session_t *session,
										  char *bridgeto, uint32_t timeout, 
										  char *flags, 
										  char *cid_name, 
										  char *cid_num,
										  char *profile,
										  switch_call_cause_t *cause,
										  switch_call_cause_t *cancel_cause, switch_event_t *var_event);
static switch_status_t conference_outcall_bg(conference_obj_t *conference,
											 char *conference_name,
											 switch_core_session_t *session, char *bridgeto, uint32_t timeout, const char *flags, const char *cid_name,
											 const char *cid_num, const char *call_uuid, const char *profile, switch_call_cause_t *cancel_cause, switch_event_t **var_event);
SWITCH_STANDARD_APP(conference_function);
static void launch_conference_video_muxing_thread(conference_obj_t *conference, mcu_canvas_t *canvas, int super);
static void launch_conference_thread(conference_obj_t *conference);
static void launch_conference_video_muxing_write_thread(conference_member_t *member);
static void *SWITCH_THREAD_FUNC conference_loop_input(switch_thread_t *thread, void *obj);
static switch_status_t conference_local_play_file(conference_obj_t *conference, switch_core_session_t *session, char *path, uint32_t leadin, void *buf,
												  uint32_t buflen);
static switch_status_t conference_member_play_file(conference_member_t *member, char *file, uint32_t leadin, switch_bool_t mux);
static switch_status_t conference_member_say(conference_member_t *member, char *text, uint32_t leadin);
static uint32_t conference_member_stop_file(conference_member_t *member, file_stop_t stop);
static conference_obj_t *conference_new(char *name, conf_xml_cfg_t cfg, switch_core_session_t *session, switch_memory_pool_t *pool);
static switch_status_t chat_send(switch_event_t *message_event);
								 

static void launch_conference_record_thread(conference_obj_t *conference, char *path, switch_bool_t autorec);

typedef switch_status_t (*conf_api_args_cmd_t) (conference_obj_t *, switch_stream_handle_t *, int, char **);
typedef switch_status_t (*conf_api_member_cmd_t) (conference_member_t *, switch_stream_handle_t *, void *);
typedef switch_status_t (*conf_api_text_cmd_t) (conference_obj_t *, switch_stream_handle_t *, const char *);

static void conference_member_itterator(conference_obj_t *conference, switch_stream_handle_t *stream, uint8_t non_mod, conf_api_member_cmd_t pfncallback, void *data);
static switch_status_t conf_api_sub_mute(conference_member_t *member, switch_stream_handle_t *stream, void *data);
static switch_status_t conf_api_sub_tmute(conference_member_t *member, switch_stream_handle_t *stream, void *data);
static switch_status_t conf_api_sub_unmute(conference_member_t *member, switch_stream_handle_t *stream, void *data);
static switch_status_t conf_api_sub_vmute(conference_member_t *member, switch_stream_handle_t *stream, void *data);
static switch_status_t conf_api_sub_tvmute(conference_member_t *member, switch_stream_handle_t *stream, void *data);
static switch_status_t conf_api_sub_unvmute(conference_member_t *member, switch_stream_handle_t *stream, void *data);
static switch_status_t conf_api_sub_deaf(conference_member_t *member, switch_stream_handle_t *stream, void *data);
static switch_status_t conf_api_sub_undeaf(conference_member_t *member, switch_stream_handle_t *stream, void *data);
static switch_status_t conference_add_event_data(conference_obj_t *conference, switch_event_t *event);
static switch_status_t conference_add_event_member_data(conference_member_t *member, switch_event_t *event);
static switch_status_t conf_api_sub_floor(conference_member_t *member, switch_stream_handle_t *stream, void *data);
static switch_status_t conf_api_sub_vid_floor(conference_member_t *member, switch_stream_handle_t *stream, void *data);
static switch_status_t conf_api_sub_clear_vid_floor(conference_obj_t *conference, switch_stream_handle_t *stream, void *data);
static switch_status_t conf_api_sub_position(conference_member_t *member, switch_stream_handle_t *stream, void *data);

#define lock_member(_member) switch_mutex_lock(_member->write_mutex); switch_mutex_lock(_member->read_mutex)
#define unlock_member(_member) switch_mutex_unlock(_member->read_mutex); switch_mutex_unlock(_member->write_mutex)

//#define lock_member(_member) switch_mutex_lock(_member->write_mutex)
//#define unlock_member(_member) switch_mutex_unlock(_member->write_mutex)

static void conference_parse_layouts(conference_obj_t *conference, int WIDTH, int HEIGHT)
{
	switch_event_t *params;
	switch_xml_t cxml = NULL, cfg = NULL, x_layouts, x_layout, x_layout_settings, x_group, x_groups, x_image;
	char cmd_str[256] = "";

	switch_mutex_lock(globals.setup_mutex);
	if (!conference->layout_hash) {
		switch_core_hash_init(&conference->layout_hash);
	}


	if (!conference->layout_group_hash) {
		switch_core_hash_init(&conference->layout_group_hash);
	}
	switch_mutex_unlock(globals.setup_mutex);

	switch_event_create(&params, SWITCH_EVENT_COMMAND);
	switch_assert(params);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "conf_name", conference->name);
	
	if (!(cxml = switch_xml_open_cfg("conference_layouts.conf", &cfg, params))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", "conference_layouts.conf");
		goto done;
	}

	if ((x_layout_settings = switch_xml_child(cfg, "layout-settings"))) {
		if ((x_layouts = switch_xml_child(x_layout_settings, "layouts"))) {
			for (x_layout = switch_xml_child(x_layouts, "layout"); x_layout; x_layout = x_layout->next) {
				video_layout_t *vlayout;
				const char *val = NULL, *name = NULL;
				switch_bool_t auto_3d = SWITCH_FALSE;
				
				if ((val = switch_xml_attr(x_layout, "name"))) {
					name = val;
				}

				if (!name) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "invalid layout\n");
					continue;
				}

				auto_3d = switch_true(switch_xml_attr(x_layout, "auto-3d-position"));
				
				vlayout = switch_core_alloc(conference->pool, sizeof(*vlayout));
				vlayout->name = switch_core_strdup(conference->pool, name);

				for (x_image = switch_xml_child(x_layout, "image"); x_image; x_image = x_image->next) {
					const char *res_id = NULL, *audio_position = NULL;
					int x = -1, y = -1, scale = -1, floor = 0, flooronly = 0, fileonly = 0, overlap = 0;

					if ((val = switch_xml_attr(x_image, "x"))) {
						x = atoi(val);
					}
					
					if ((val = switch_xml_attr(x_image, "y"))) {
						y = atoi(val);
					}
					
					if ((val = switch_xml_attr(x_image, "scale"))) {
						scale = atoi(val);
					}
					
					if ((val = switch_xml_attr(x_image, "floor"))) {
						floor = switch_true(val);
					}

					if ((val = switch_xml_attr(x_image, "floor-only"))) {
						flooronly = floor = switch_true(val);
					}

					if ((val = switch_xml_attr(x_image, "file-only"))) {
						fileonly = floor = switch_true(val);
					}

					if ((val = switch_xml_attr(x_image, "overlap"))) {
						overlap = switch_true(val);
					}
					
					if ((val = switch_xml_attr(x_image, "reservation_id"))) {
						res_id = val;
					}

					if ((val = switch_xml_attr(x_image, "audio-position"))) {
						audio_position = val;
					}


					if (x < 0 || y < 0 || scale < 0 || !name) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "invalid image\n");
						continue;
					}

				
					vlayout->images[vlayout->layers].x = x;
					vlayout->images[vlayout->layers].y = y;
					vlayout->images[vlayout->layers].scale = scale;
					vlayout->images[vlayout->layers].floor = floor;
					vlayout->images[vlayout->layers].flooronly = flooronly;
					vlayout->images[vlayout->layers].fileonly = fileonly;
					vlayout->images[vlayout->layers].overlap = overlap;
					
					if (res_id) {
						vlayout->images[vlayout->layers].res_id = switch_core_strdup(conference->pool, res_id);
					}

					if (auto_3d || audio_position) {
						if (auto_3d || !strcasecmp(audio_position, "auto")) {
							int x_pos = WIDTH * x / VIDEO_LAYOUT_SCALE;
							int y_pos = HEIGHT * y / VIDEO_LAYOUT_SCALE;
							int width = WIDTH * scale / VIDEO_LAYOUT_SCALE;
							int height = HEIGHT * scale / VIDEO_LAYOUT_SCALE;
							int center_x = x_pos + width / 2;
							int center_y = y_pos + height / 2;
							int half_x = WIDTH / 2;
							int half_y = HEIGHT / 2;
							float xv = 0, yv = 0;

							if (center_x > half_x) {
								xv = (float)(center_x - half_x) / half_x;
							} else {
								xv = (float) -1 - (center_x / half_x) * -1;
							}

							if (center_y > half_y) {
								yv = -1 - ((center_y - half_y) / half_y) * -1;
							} else {
								yv = center_y / half_y;
							}

							vlayout->images[vlayout->layers].audio_position = switch_core_sprintf(conference->pool, "%02f:0.0:%02f", xv, yv);

						} else {
							vlayout->images[vlayout->layers].audio_position = switch_core_strdup(conference->pool, audio_position);
						}
					}

					vlayout->layers++;
				}

				switch_core_hash_insert(conference->layout_hash, name, vlayout);
				switch_snprintf(cmd_str, sizeof(cmd_str), "add conference ::conference::list_conferences vid-layout %s", name);
				switch_console_set_complete(cmd_str);
			}
			
		}

		if ((x_groups = switch_xml_child(x_layout_settings, "groups"))) {
			for (x_group = switch_xml_child(x_groups, "group"); x_group; x_group = x_group->next) {
				const char *name = switch_xml_attr(x_group, "name");
				layout_group_t *lg;
				video_layout_node_t *last_vlnode = NULL;

				x_layout = switch_xml_child(x_group, "layout");
				
				if (!name || !x_group || !x_layout) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "invalid group\n");
					continue;
				}

				lg = switch_core_alloc(conference->pool, sizeof(*lg));
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding layout group %s\n", name);
				switch_core_hash_insert(conference->layout_group_hash, name, lg);

				while(x_layout) {
					const char *nname = x_layout->txt;
					video_layout_t *vlayout;
					video_layout_node_t *vlnode;

					if ((vlayout = switch_core_hash_find(conference->layout_hash, nname))) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding node %s to layout group %s\n", nname, name);

						vlnode = switch_core_alloc(conference->pool, sizeof(*vlnode));
						vlnode->vlayout = vlayout;

						if (last_vlnode) {
							last_vlnode->next = vlnode;
							last_vlnode = last_vlnode->next;
						} else {
							lg->layouts = last_vlnode = vlnode;
						}


					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "invalid group member %s\n", nname);
					}

					x_layout = x_layout->next;
				}
			}
		}
	}


 done:

	if (cxml) {
		switch_xml_free(cxml);
		cxml = NULL;
	}

	switch_event_destroy(&params);	

}

/* do not use this on an img cropped with switch_img_set_rect() */
static void reset_image(switch_image_t *img, switch_rgb_color_t *color)
{
	switch_img_fill(img, 0, 0, img->d_w, img->d_h, color);
}

/* clear layer and reset_layer called inside lock always */

static void clear_layer(mcu_layer_t *layer)
{
	switch_img_fill(layer->canvas->img, layer->x_pos, layer->y_pos, layer->screen_w, layer->screen_h, &layer->canvas->bgcolor);
	layer->banner_patched = 0;
	layer->refresh = 1;
}

static void reset_layer(mcu_layer_t *layer)
{
	layer->tagged = 0;

	switch_img_free(&layer->banner_img);
	switch_img_free(&layer->logo_img);
	switch_img_free(&layer->logo_text_img);

	layer->mute_patched = 0;
	layer->banner_patched = 0;
	layer->is_avatar = 0;
	
	if (layer->geometry.overlap) {
		layer->canvas->refresh = 1;
	}

	switch_img_free(&layer->img);
	layer->img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, layer->screen_w, layer->screen_h, 1);
	switch_assert(layer->img);

	clear_layer(layer);
	switch_img_free(&layer->cur_img);
}

static void scale_and_patch(mcu_layer_t *layer, switch_image_t *ximg, switch_bool_t freeze)
{
	switch_image_t *IMG, *img;

	switch_mutex_lock(layer->canvas->mutex);

	IMG = layer->canvas->img;
	img = ximg ? ximg : layer->cur_img;

	switch_assert(IMG);

	if (!img) {
		switch_mutex_unlock(layer->canvas->mutex);
		return;
	}

	if (layer->refresh) {
		switch_img_fill(layer->canvas->img, layer->x_pos, layer->y_pos, layer->screen_w, layer->screen_h, &layer->canvas->letterbox_bgcolor);
		layer->refresh = 0;
	}

	if (layer->geometry.scale) {
		int img_w = 0, img_h = 0;
		double screen_aspect = 0, img_aspect = 0;
		int x_pos = layer->x_pos;
		int y_pos = layer->y_pos;
		
		img_w = layer->screen_w = IMG->d_w * layer->geometry.scale / VIDEO_LAYOUT_SCALE;
		img_h = layer->screen_h = IMG->d_h * layer->geometry.scale / VIDEO_LAYOUT_SCALE;

		screen_aspect = (double) layer->screen_w / layer->screen_h;
		img_aspect = (double) img->d_w / img->d_h;

		if (freeze) {
			switch_img_free(&layer->img);
		}
		
		if (screen_aspect > img_aspect) {
			img_w = img_aspect * layer->screen_h;
			x_pos += (layer->screen_w - img_w) / 2;
		} else if (screen_aspect < img_aspect) {
			img_h = layer->screen_w / img_aspect;
			y_pos += (layer->screen_h - img_h) / 2;
		}

		if (layer->img && (layer->img->d_w != img_w || layer->img->d_h != img_h)) {
			switch_img_free(&layer->img);
			layer->banner_patched = 0;
			clear_layer(layer);
		}

		if (!layer->img) {
			layer->img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, img_w, img_h, 1);
		}
		
		if (layer->banner_img && !layer->banner_patched) {
			switch_img_fill(layer->canvas->img, layer->x_pos, layer->y_pos, layer->screen_w, layer->screen_h, &layer->canvas->letterbox_bgcolor);
			switch_img_patch(IMG, layer->banner_img, layer->x_pos, layer->y_pos + (layer->screen_h - layer->banner_img->d_h));

			if (!freeze) {
				switch_img_set_rect(layer->img, 0, 0, layer->img->d_w, layer->img->d_h - layer->banner_img->d_h);
			}

			layer->banner_patched = 1;
		}

		switch_assert(layer->img);

		if (switch_img_scale(img, &layer->img, img_w, img_h) == SWITCH_STATUS_SUCCESS) {
			if (layer->bugged && layer->member_id > -1) {
				conference_member_t *member;
				if ((member = conference_member_get(layer->canvas->conference, layer->member_id))) {
					switch_frame_t write_frame = { 0 };
					write_frame.img = layer->img;
					switch_core_media_bug_patch_video(member->session, &write_frame);
					switch_thread_rwlock_unlock(member->rwlock);
				}
			}

			switch_img_patch(IMG, layer->img, x_pos, y_pos);
		}

		if (layer->logo_img) {
			int ew = layer->screen_w, eh = layer->screen_h - (layer->banner_img ? layer->banner_img->d_h : 0);
			int ex = 0, ey = 0;

			switch_img_fit(&layer->logo_img, ew, eh);
			switch_img_find_position(layer->logo_pos, ew, eh, layer->logo_img->d_w, layer->logo_img->d_h, &ex, &ey);
			switch_img_patch(IMG, layer->logo_img, layer->x_pos + ex, layer->y_pos + ey);
			if (layer->logo_text_img) {
				int tx = 0, ty = 0;
				switch_img_find_position(POS_LEFT_BOT, 
										 layer->logo_img->d_w, layer->logo_img->d_h, layer->logo_text_img->d_w, layer->logo_text_img->d_h, &tx, &ty);
				switch_img_patch(IMG, layer->logo_text_img, layer->x_pos + ex + tx, layer->y_pos + ey + ty);
			}

		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "insert at %d,%d\n", 0, 0);
		switch_img_patch(IMG, img, 0, 0);
	}

	switch_mutex_unlock(layer->canvas->mutex);
}

static void set_canvas_bgcolor(mcu_canvas_t *canvas, char *color)
{
	switch_color_set_rgb(&canvas->bgcolor, color);
	reset_image(canvas->img, &canvas->bgcolor);
}

static void set_canvas_letterbox_bgcolor(mcu_canvas_t *canvas, char *color)
{
	switch_color_set_rgb(&canvas->letterbox_bgcolor, color);
}

static void check_used_layers(mcu_canvas_t *canvas)
{
	int i;

	if (!canvas) return;

	canvas->layers_used = 0;
	for (i = 0; i < canvas->total_layers; i++) {
		if (canvas->layers[i].member_id) {
			canvas->layers_used++;
		}
	}
}

static void detach_video_layer(conference_member_t *member)
{
	mcu_layer_t *layer = NULL;
	mcu_canvas_t *canvas = NULL;

	switch_mutex_lock(member->conference->canvas_mutex);
	
	if (member->canvas_id < 0) goto end;

	canvas = member->conference->canvases[member->canvas_id];

	if (!canvas || member->video_layer_id < 0) {
		goto end;
	}

	switch_mutex_lock(canvas->mutex);

	layer = &canvas->layers[member->video_layer_id];

	if (layer->geometry.audio_position) {
		conf_api_sub_position(member, NULL, "0:0:0");
	}

	if (layer->txthandle) {
		switch_img_txt_handle_destroy(&layer->txthandle);
	}

	reset_layer(layer);
	layer->member_id = 0;
	member->video_layer_id = -1;
	//member->canvas_id = 0;
	//member->watching_canvas_id = -1;
	member->avatar_patched = 0;
	check_used_layers(canvas);
	canvas->send_keyframe = 1;
	switch_mutex_unlock(canvas->mutex);

 end:

	switch_mutex_unlock(member->conference->canvas_mutex);

}


static void layer_set_logo(conference_member_t *member, mcu_layer_t *layer, const char *path)
{
	const char *var = NULL;
	char *dup = NULL;
	switch_event_t *params = NULL;
	char *parsed = NULL;
	char *tmp;
	switch_img_position_t pos = POS_LEFT_TOP;

	switch_mutex_lock(layer->canvas->mutex);

	if (!path) {
		path = member->video_logo;
	}
	
	if (!path) {
		goto end;
	}

	if (path) {
		switch_img_free(&layer->logo_img);
		switch_img_free(&layer->logo_text_img);
	}

	if (*path == '{') {
		dup = strdup(path);
		path = dup;

		if (switch_event_create_brackets((char *)path, '{', '}', ',', &params, &parsed, SWITCH_FALSE) != SWITCH_STATUS_SUCCESS || !parsed) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
		} else {
			path = parsed;
		}
	}


	if (zstr(path) || !strcasecmp(path, "reset")) {	
		path = switch_channel_get_variable_dup(member->channel, "video_logo_path", SWITCH_FALSE, -1);
	}
	
	if (zstr(path) || !strcasecmp(path, "clear")) {
		switch_img_free(&layer->banner_img);
		layer->banner_patched = 0;
		
		switch_img_fill(member->conference->canvas->img, layer->x_pos, layer->y_pos, layer->screen_w, layer->screen_h, 
						&member->conference->canvas->letterbox_bgcolor);

		goto end;
	}

	if ((tmp = strchr(path, '}'))) {
		path = tmp + 1;
	}


	if (params) {
		if ((var = switch_event_get_header(params, "position"))) {
			pos = parse_img_position(var);
		}
	}

	if (path && strcasecmp(path, "clear")) {
		layer->logo_img = switch_img_read_png(path, SWITCH_IMG_FMT_ARGB);
	}

	if (layer->logo_img) {
		layer->logo_pos = pos;

		if (params) {
			if ((var = switch_event_get_header(params, "text"))) {
				layer->logo_text_img = switch_img_write_text_img(layer->screen_w, layer->screen_h, SWITCH_FALSE, var);
			}
		}
	}
	
	if (params) switch_event_destroy(&params);

	switch_safe_free(dup);

 end:

	switch_mutex_unlock(layer->canvas->mutex);

}

static void layer_set_banner(conference_member_t *member, mcu_layer_t *layer, const char *text)
{
	switch_rgb_color_t fgcolor, bgcolor;
	int font_scale = 4;
	int font_size = 0;
	const char *fg = "#cccccc";
	const char *bg = "#142e55";
	char *parsed = NULL;
	switch_event_t *params = NULL;
	const char *font_face = NULL;
	const char *var, *tmp = NULL;
	char *dup = NULL;
	
	switch_mutex_lock(layer->canvas->mutex);

	if (!text) {
		text = member->video_banner_text;
	}
	
	if (!text) {
		goto end;
	}

	if (*text == '{') {
		dup = strdup(text);
		text = dup;

		if (switch_event_create_brackets((char *)text, '{', '}', ',', &params, &parsed, SWITCH_FALSE) != SWITCH_STATUS_SUCCESS || !parsed) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
		} else {
			text = parsed;
		}
	}

	if (zstr(text) || !strcasecmp(text, "reset")) {	
		text = switch_channel_get_variable_dup(member->channel, "video_banner_text", SWITCH_FALSE, -1);
	}
	
	if (zstr(text) || !strcasecmp(text, "clear")) {
		switch_img_free(&layer->banner_img);
		layer->banner_patched = 0;
		
		switch_img_fill(member->conference->canvas->img, layer->x_pos, layer->y_pos, layer->screen_w, layer->screen_h, 
						&member->conference->canvas->letterbox_bgcolor);

		goto end;
	}

	if ((tmp = strchr(text, '}'))) {
		text = tmp + 1;
	}

	if (params) {
		if ((var = switch_event_get_header(params, "fg"))) {
			fg = var;
		}

		if ((var = switch_event_get_header(params, "bg"))) {
			bg = var;
		}

		if ((var = switch_event_get_header(params, "font_face"))) {
			font_face = var;
		}

		if ((var = switch_event_get_header(params, "font_scale"))) {
			int tmp = atoi(var);

			if (tmp >= 5 && tmp <= 50) {
				font_scale = tmp;
			}
		}
	}

	font_size = (double)(font_scale / 100.0f) * layer->screen_h;


	switch_color_set_rgb(&fgcolor, fg);
	switch_color_set_rgb(&bgcolor, bg);

	if (layer->txthandle) {
		switch_img_txt_handle_destroy(&layer->txthandle);
	}

	switch_img_txt_handle_create(&layer->txthandle, font_face, fg, bg, font_size, 0, NULL);

	if (!layer->txthandle) {
		switch_img_free(&layer->banner_img);
		layer->banner_patched = 0;

		switch_img_fill(member->conference->canvas->img, layer->x_pos, layer->y_pos, layer->screen_w, layer->screen_h,
						&member->conference->canvas->letterbox_bgcolor);

		goto end;
	}

	switch_img_free(&layer->banner_img);
	switch_img_free(&layer->logo_img);
	switch_img_free(&layer->logo_text_img);
	layer->banner_img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, layer->screen_w, font_size * 2, 1);

	reset_image(layer->banner_img, &bgcolor);
	switch_img_txt_handle_render(layer->txthandle, layer->banner_img, font_size / 2, font_size / 2, text, NULL, fg, bg, 0, 0);

 end:

	if (params) switch_event_destroy(&params);

	switch_safe_free(dup);

	switch_mutex_unlock(layer->canvas->mutex);
}

static void reset_video_bitrate_counters(conference_member_t *member)
{
	member->managed_kps = 0;
	member->blackouts = 0;
	member->good_img = 0;
	member->blanks = 0;
}

static switch_status_t attach_video_layer(conference_member_t *member, mcu_canvas_t *canvas, int idx)
{
	mcu_layer_t *layer = NULL;
	switch_channel_t *channel = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	const char *var = NULL;
	
	if (!member->session) abort();

	channel = switch_core_session_get_channel(member->session);


	if (!switch_channel_test_flag(channel, CF_VIDEO) && !member->avatar_png_img) {
		return SWITCH_STATUS_FALSE;
	}

	if (member->video_flow == SWITCH_MEDIA_FLOW_SENDONLY && !member->avatar_png_img) {
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(member->conference->canvas_mutex);

	switch_mutex_lock(canvas->mutex);

	layer = &canvas->layers[idx];

	layer->tagged = 0;

	if (layer->fnode || layer->geometry.fileonly) {
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	if (layer->geometry.flooronly && member->id != member->conference->video_floor_holder) {
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	if (layer->geometry.res_id) {
		if (!member->video_reservation_id || strcmp(layer->geometry.res_id, member->video_reservation_id)) {
			switch_goto_status(SWITCH_STATUS_FALSE, end);
		}
	}

	if (layer->member_id && layer->member_id == member->id) {
		member->video_layer_id = idx;
		switch_goto_status(SWITCH_STATUS_BREAK, end);
	}
	
	if (layer->geometry.res_id || member->video_reservation_id) {
		if (!layer->geometry.res_id || !member->video_reservation_id || strcmp(layer->geometry.res_id, member->video_reservation_id)) {
			switch_goto_status(SWITCH_STATUS_FALSE, end);
		}
	}
	
	if (member->video_layer_id > -1) {
		detach_video_layer(member);
	}

	reset_layer(layer);
	switch_img_free(&layer->mute_img);

	member->avatar_patched = 0;
		
	if (member->avatar_png_img) {
		layer->is_avatar = 1;
	}
	
	var = NULL;
	if (member->video_banner_text || (var = switch_channel_get_variable_dup(channel, "video_banner_text", SWITCH_FALSE, -1))) {
		layer_set_banner(member, layer, var);
	}

	var = NULL;
	if (member->video_logo || (var = switch_channel_get_variable_dup(channel, "video_logo_path", SWITCH_FALSE, -1))) {
		layer_set_logo(member, layer, var);
	}

	layer->member_id = member->id;
	member->video_layer_id = idx;
	member->canvas_id = canvas->canvas_id;
	canvas->send_keyframe = 1;

	//member->watching_canvas_id = canvas->canvas_id;
	check_used_layers(canvas);

	if (layer->geometry.audio_position) {
		conf_api_sub_position(member, NULL, layer->geometry.audio_position);
	}

	switch_img_fill(canvas->img, layer->x_pos, layer->y_pos, layer->screen_w, layer->screen_h, &canvas->letterbox_bgcolor);
	reset_video_bitrate_counters(member);

 end:

	switch_mutex_unlock(canvas->mutex);

	switch_mutex_unlock(member->conference->canvas_mutex);

	return status;
}

static void init_canvas_layers(conference_obj_t *conference, mcu_canvas_t *canvas, video_layout_t *vlayout)
{
	int i = 0;	

	if (!canvas) return;

	switch_mutex_lock(canvas->mutex);	
	canvas->layout_floor_id = -1;

	if (!vlayout) {
		vlayout = canvas->new_vlayout;
		canvas->new_vlayout = NULL;
	}

	if (!vlayout) {
		switch_mutex_unlock(canvas->mutex);
		return;
	}

	canvas->vlayout = vlayout;
	
	for (i = 0; i < vlayout->layers; i++) {
		mcu_layer_t *layer = &canvas->layers[i];
		layer->geometry.x = vlayout->images[i].x;
		layer->geometry.y = vlayout->images[i].y;
		layer->geometry.scale = vlayout->images[i].scale;
		layer->geometry.floor = vlayout->images[i].floor;
		layer->geometry.overlap = vlayout->images[i].overlap;
		layer->idx = i;
		layer->refresh = 1;

		layer->screen_w = canvas->img->d_w * layer->geometry.scale / VIDEO_LAYOUT_SCALE;
		layer->screen_h = canvas->img->d_h * layer->geometry.scale / VIDEO_LAYOUT_SCALE;

		// if (layer->screen_w % 2) layer->screen_w++; // round to even
		// if (layer->screen_h % 2) layer->screen_h++; // round to even

		layer->x_pos = canvas->img->d_w * layer->geometry.x / VIDEO_LAYOUT_SCALE;
		layer->y_pos = canvas->img->d_h * layer->geometry.y / VIDEO_LAYOUT_SCALE;


		if (layer->geometry.floor) {
			canvas->layout_floor_id = i;
		}

		/* if we ever decided to reload layers config on demand the pointer assignment below  will lead to segs but we
		   only load them once forever per conference so these pointers are valid for the life of the conference */
		layer->geometry.res_id = vlayout->images[i].res_id;
		layer->geometry.audio_position = vlayout->images[i].audio_position;
	}

	reset_image(canvas->img, &canvas->bgcolor);

	for (i = 0; i < MCU_MAX_LAYERS; i++) {
		mcu_layer_t *layer = &canvas->layers[i];

		layer->member_id = 0;
		layer->tagged = 0;
		layer->banner_patched = 0;
		layer->refresh = 1;
		layer->canvas = canvas;
		reset_layer(layer);
	}

	canvas->layers_used = 0;
	canvas->total_layers = vlayout->layers;
	canvas->send_keyframe = 1;

	switch_mutex_unlock(canvas->mutex);	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Canvas position %d applied layout %s\n", canvas->canvas_id, vlayout->name);

}

static switch_status_t attach_canvas(conference_obj_t *conference, mcu_canvas_t *canvas, int super)
{
	if (conference->canvas_count >= MAX_CANVASES + 1) {
		return SWITCH_STATUS_FALSE;
	}

	canvas->canvas_id = conference->canvas_count;

	if (!super) {
		conference->canvas_count++;
		
		if (!conference->canvas) {
			conference->canvas = canvas;
		}
	}
	
	conference->canvases[canvas->canvas_id] = canvas;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Canvas attached to position %d\n", canvas->canvas_id);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t init_canvas(conference_obj_t *conference, video_layout_t *vlayout, mcu_canvas_t **canvasP)
{
	mcu_canvas_t *canvas;

	if (conference->canvas_count >= MAX_CANVASES) {
		return SWITCH_STATUS_FALSE;
	}

	canvas = switch_core_alloc(conference->pool, sizeof(*canvas));
	canvas->conference = conference;
	canvas->pool = conference->pool;
	switch_mutex_init(&canvas->mutex, SWITCH_MUTEX_NESTED, conference->pool);
	canvas->layout_floor_id = -1;

	switch_img_free(&canvas->img);

	canvas->width = conference->canvas_width;
	canvas->height = conference->canvas_height;
	
	canvas->img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, canvas->width, canvas->height, 0);
	switch_queue_create(&canvas->video_queue, 200, canvas->pool);

	switch_assert(canvas->img);

	switch_mutex_lock(canvas->mutex);
	set_canvas_bgcolor(canvas, conference->video_canvas_bgcolor);
	set_canvas_letterbox_bgcolor(canvas, conference->video_letterbox_bgcolor);
	init_canvas_layers(conference, canvas, vlayout);
	switch_mutex_unlock(canvas->mutex);

	canvas->canvas_id = -1;
	*canvasP = canvas;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Layout set to %s\n", vlayout->name);

	return SWITCH_STATUS_SUCCESS;
}

static int flush_video_queue(switch_queue_t *q)
{
	switch_image_t *img;
	void *pop;
	int r = 0;

	if (!q) return 0;

	while (switch_queue_size(q) > 1 && switch_queue_trypop(q, &pop) == SWITCH_STATUS_SUCCESS && pop) {
		img = (switch_image_t *)pop;
		switch_img_free(&img);
		r++;
	}

	return r + switch_queue_size(q);
}


static void destroy_canvas(mcu_canvas_t **canvasP) {
	int i;
	mcu_canvas_t *canvas = *canvasP;

	switch_img_free(&canvas->img);
	flush_video_queue(canvas->video_queue);

	for (i = 0; i < MCU_MAX_LAYERS; i++) {
		switch_img_free(&canvas->layers[i].img);
	}

	*canvasP = NULL;
}

typedef struct codec_set_s {
	switch_codec_t codec;
	switch_frame_t frame;
	uint8_t *packet;
} codec_set_t;

static void write_canvas_image_to_codec_group(conference_obj_t *conference, mcu_canvas_t *canvas, codec_set_t *codec_set,
											  int codec_index, uint32_t timestamp, switch_bool_t need_refresh, 
											  switch_bool_t need_keyframe, switch_bool_t need_reset)

{
	conference_member_t *imember;
	switch_frame_t write_frame = { 0 }, *frame = NULL;
	switch_status_t encode_status = SWITCH_STATUS_FALSE;

	write_frame = codec_set->frame;
	frame = &write_frame;
	frame->img = codec_set->frame.img;
	frame->packet = codec_set->frame.packet;
	frame->packetlen = codec_set->frame.packetlen;

	switch_clear_flag(frame, SFF_SAME_IMAGE);
	frame->m = 0;
	frame->timestamp = timestamp;

	if (need_reset) {
		int type = 1; // sum flags: 1 encoder; 2; decoder
		switch_core_codec_control(&codec_set->codec, SCC_VIDEO_RESET, SCCT_INT, (void *)&type, NULL, NULL);
		need_refresh = SWITCH_TRUE;
	}

	if (need_refresh || need_keyframe) {
		switch_core_codec_control(&codec_set->codec, SCC_VIDEO_REFRESH, SCCT_NONE, NULL, NULL, NULL);
	}

	do {
		
		frame->data = ((unsigned char *)frame->packet) + 12;
		frame->datalen = SWITCH_DEFAULT_VIDEO_SIZE;

		encode_status = switch_core_codec_encode_video(&codec_set->codec, frame);

		if (encode_status == SWITCH_STATUS_SUCCESS || encode_status == SWITCH_STATUS_MORE_DATA) {

			switch_assert((encode_status == SWITCH_STATUS_SUCCESS && frame->m) || !frame->m);

			if (frame->datalen == 0) {
				break;
			}

			if (frame->timestamp) {
				switch_set_flag(frame, SFF_RAW_RTP_PARSE_FRAME);
			}

			frame->packetlen = frame->datalen + 12;

			switch_mutex_lock(conference->member_mutex);
			for (imember = conference->members; imember; imember = imember->next) {
				switch_frame_t *dupframe;

				if (imember->watching_canvas_id != canvas->canvas_id) {
					continue;
				}

				if (member_test_flag(imember, MFLAG_NO_MINIMIZE_ENCODING)) {
					continue;
				}
				
				if (imember->video_codec_index != codec_index) {
					continue;
				}

				if (!imember->session || !switch_channel_test_flag(imember->channel, CF_VIDEO) ||
					switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS) {
					continue;
				}

				//if (need_refresh) {
				//	switch_core_session_request_video_refresh(imember->session);
				//}
				
				//switch_core_session_write_encoded_video_frame(imember->session, frame, 0, 0);
				switch_set_flag(frame, SFF_ENCODED);
				
				if (switch_frame_buffer_dup(imember->fb, frame, &dupframe) == SWITCH_STATUS_SUCCESS) {
					switch_queue_push(imember->mux_out_queue, dupframe);
					dupframe = NULL;
				}

				switch_clear_flag(frame, SFF_ENCODED);

				switch_core_session_rwunlock(imember->session);
			}
			switch_mutex_unlock(conference->member_mutex);
		}

	} while(encode_status == SWITCH_STATUS_MORE_DATA);
}

#define MAX_MUX_CODECS 10

static video_layout_t *find_best_layout(conference_obj_t *conference, layout_group_t *lg, uint32_t count)
{
	video_layout_node_t *vlnode = NULL, *last = NULL;

	if (!count) count = conference->members_with_video + conference->members_with_avatar;

	for (vlnode = lg->layouts; vlnode; vlnode = vlnode->next) {
		if (vlnode->vlayout->layers >= count) {
			break;
		}

		last = vlnode;
	}

	return vlnode? vlnode->vlayout : last ? last->vlayout : NULL;
}

static video_layout_t *get_layout(conference_obj_t *conference, const char *video_layout_name, const char *video_layout_group)
{
	layout_group_t *lg = NULL;
	video_layout_t *vlayout = NULL;

	if (video_layout_group) {
		lg = switch_core_hash_find(conference->layout_group_hash, video_layout_group);
		vlayout = find_best_layout(conference, lg, 0);
	} else {
		vlayout = switch_core_hash_find(conference->layout_hash, video_layout_name);
	}

	return vlayout;
}

static void vmute_snap(conference_member_t *member, switch_bool_t clear)
{


	if (member->canvas_id > -1 && member->video_layer_id > -1) {
		mcu_layer_t *layer = NULL;
		mcu_canvas_t *canvas = NULL;

		canvas = member->conference->canvases[member->canvas_id];

		switch_mutex_lock(canvas->mutex);
		layer = &canvas->layers[member->video_layer_id];
		switch_img_free(&layer->mute_img);
		switch_img_free(&member->video_mute_img);

		if (!clear && layer->cur_img) {
			switch_img_copy(layer->cur_img, &member->video_mute_img);
			switch_img_copy(layer->cur_img, &layer->mute_img);
		}

		switch_mutex_unlock(canvas->mutex);			
	}
}

static void *SWITCH_THREAD_FUNC conference_video_muxing_write_thread_run(switch_thread_t *thread, void *obj)
{
	conference_member_t *member = (conference_member_t *) obj;
	void *pop;
	int loops = 0;

	while(member_test_flag(member, MFLAG_RUNNING) || switch_queue_size(member->mux_out_queue)) {
		switch_frame_t *frame;

		if (member_test_flag(member, MFLAG_RUNNING)) {
			if (switch_queue_pop(member->mux_out_queue, &pop) == SWITCH_STATUS_SUCCESS) {
				if (!pop) continue;

				if (loops == 0 || loops == 50) {
					switch_core_media_gen_key_frame(member->session);
					switch_core_session_request_video_refresh(member->session);
				}

				loops++;
				
				frame = (switch_frame_t *) pop;

				if (switch_test_flag(frame, SFF_ENCODED)) {
					switch_core_session_write_encoded_video_frame(member->session, frame, 0, 0);
				} else {
					switch_core_session_write_video_frame(member->session, frame, SWITCH_IO_FLAG_NONE, 0);
				}
				switch_frame_buffer_free(member->fb, &frame);
			}
		} else {
			if (switch_queue_trypop(member->mux_out_queue, &pop) == SWITCH_STATUS_SUCCESS) {
				if (pop) {
					frame = (switch_frame_t *) pop;
					switch_frame_buffer_free(member->fb, &frame);
				}
			}
		}
	}

	return NULL;
}

static void check_video_recording(conference_obj_t *conference, switch_frame_t *frame)
{
	conference_member_t *imember;
	
	if (!conference->recording_members) {
		return;
	}

	switch_mutex_lock(conference->member_mutex);
		
	for (imember = conference->members; imember; imember = imember->next) {
		if (!imember->rec) {
			continue;
		}			
		if (switch_test_flag((&imember->rec->fh), SWITCH_FILE_OPEN) && switch_core_file_has_video(&imember->rec->fh)) {
			switch_core_file_write_video(&imember->rec->fh, frame);
		}
	}

	switch_mutex_unlock(conference->member_mutex);

}

static void check_avatar(conference_member_t *member, switch_bool_t force)
{
	const char *avatar = NULL, *var = NULL;
	mcu_canvas_t *canvas;

	if (member->canvas_id < 0) {
		return;
	}

	canvas = member->conference->canvases[member->canvas_id];

	if (conference_test_flag(member->conference, CFLAG_VIDEO_REQUIRED_FOR_CANVAS) && 
		(!switch_channel_test_flag(member->channel, CF_VIDEO) || member->video_flow == SWITCH_MEDIA_FLOW_SENDONLY)) {
		return;
	}

	if (canvas) {
		switch_mutex_lock(canvas->mutex);
	}

	member->avatar_patched = 0;
	
	if (!force && switch_channel_test_flag(member->channel, CF_VIDEO) && member->video_flow != SWITCH_MEDIA_FLOW_SENDONLY) {
		member_set_flag_locked(member, MFLAG_ACK_VIDEO);
	} else {
		if (member->conference->no_video_avatar) {
			avatar = member->conference->no_video_avatar;
		}
		
		if ((var = switch_channel_get_variable_dup(member->channel, "video_no_video_avatar_png", SWITCH_FALSE, -1))) {
			avatar = var;
		}
	}
	
	if ((var = switch_channel_get_variable_dup(member->channel, "video_avatar_png", SWITCH_FALSE, -1))) {
		avatar = var;
	}
	
	switch_img_free(&member->avatar_png_img);
	
	if (avatar) {
		member->avatar_png_img = switch_img_read_png(avatar, SWITCH_IMG_FMT_I420);
	}

	if (force && !member->avatar_png_img && member->video_mute_img) {
		switch_img_copy(member->video_mute_img, &member->avatar_png_img);
	}
	
	if (canvas) {
		switch_mutex_unlock(canvas->mutex);
	}
}

static void check_flush(conference_member_t *member)
{
	int flushed;

	if (!member->channel || !switch_channel_test_flag(member->channel, CF_VIDEO)) {
		return;
	}
	
	flushed = flush_video_queue(member->video_queue);
	
	if (flushed && member->auto_avatar) {
		switch_channel_video_sync(member->channel);
		
		switch_img_free(&member->avatar_png_img);
		member->avatar_patched = 0;
		reset_video_bitrate_counters(member);
		member->blanks = 0;
		member->auto_avatar = 0;
	}
}

static void patch_fnode(mcu_canvas_t *canvas, conference_file_node_t *fnode)
{
	if (fnode && fnode->layer_id > -1) {
		mcu_layer_t *layer = &canvas->layers[fnode->layer_id];
		switch_frame_t file_frame = { 0 };
		switch_status_t status = switch_core_file_read_video(&fnode->fh, &file_frame, SVR_FLUSH);

		if (status == SWITCH_STATUS_SUCCESS) {
			switch_img_free(&layer->cur_img);
			layer->cur_img = file_frame.img;
			layer->tagged = 1;
		} else if (status == SWITCH_STATUS_IGNORE) {
			if (canvas && fnode->layer_id > -1 ) {
				canvas_del_fnode_layer(canvas->conference, fnode);
			}
		}
	}
}

static void fnode_check_video(conference_file_node_t *fnode) {
	mcu_canvas_t *canvas = fnode->conference->canvases[fnode->canvas_id];
	
	if (switch_core_file_has_video(&fnode->fh) && switch_core_file_read_video(&fnode->fh, NULL, SVR_CHECK) == SWITCH_STATUS_BREAK) {
		int full_screen = 0;

		if (fnode->fh.params && fnode->conference->canvas_count == 1) {
			full_screen = switch_true(switch_event_get_header(fnode->fh.params, "full-screen"));
		}
		
		if (full_screen) {
			canvas->play_file = 1;
			canvas->conference->playing_video_file = 1;
		} else {
			canvas_set_fnode_layer(canvas, fnode, -1);
		}
	}
}


static switch_status_t find_layer(conference_obj_t *conference, mcu_canvas_t *canvas, conference_member_t *member, mcu_layer_t **layerP)
{
	uint32_t avatar_layers = 0;
	mcu_layer_t *layer = NULL;
	int i;

	switch_mutex_lock(conference->canvas_mutex);

	for (i = 0; i < canvas->total_layers; i++) {
		mcu_layer_t *xlayer = &canvas->layers[i];
					
		if (xlayer->is_avatar && xlayer->member_id != conference->video_floor_holder) {
			avatar_layers++;
		}
	}

	if (!layer && 
		(canvas->layers_used < canvas->total_layers || 
		 (avatar_layers && !member->avatar_png_img) || member_test_flag(member, MFLAG_MOD)) &&
		(member->avatar_png_img || member->video_flow != SWITCH_MEDIA_FLOW_SENDONLY)) {
		/* find an empty layer */
		for (i = 0; i < canvas->total_layers; i++) {
			mcu_layer_t *xlayer = &canvas->layers[i];

			if (xlayer->geometry.res_id) {
				if (member->video_reservation_id && !strcmp(xlayer->geometry.res_id, member->video_reservation_id)) {
					layer = xlayer;
					attach_video_layer(member, canvas, i);
					break;
				}
			} else if (xlayer->geometry.flooronly && !xlayer->fnode) {
				if (member->id == conference->video_floor_holder) {
					layer = xlayer;
					attach_video_layer(member, canvas, i);
					break;
				}
			} else if ((!xlayer->member_id || (!member->avatar_png_img && 
											   xlayer->is_avatar && 
											   xlayer->member_id != conference->video_floor_holder)) &&
					   !xlayer->fnode && !xlayer->geometry.fileonly) {
				switch_status_t lstatus;

				lstatus = attach_video_layer(member, canvas, i);

				if (lstatus == SWITCH_STATUS_SUCCESS || lstatus == SWITCH_STATUS_BREAK) {
					layer = xlayer;
					break;
				}
			}
		}
	}

	switch_mutex_unlock(conference->canvas_mutex);

	if (layer) {
		*layerP = layer;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;

}

static void next_canvas(conference_member_t *imember)
{
	if (imember->canvas_id == imember->conference->canvas_count - 1) {
		imember->canvas_id = 0;
	} else {
		imember->canvas_id++;
	}
}

static void pop_next_image(conference_member_t *member, switch_image_t **imgP)
{
	switch_image_t *img = *imgP;
	int size = 0;
	void *pop;

	if (!member->avatar_png_img && switch_channel_test_flag(member->channel, CF_VIDEO)) {
		do {
			if (switch_queue_trypop(member->video_queue, &pop) == SWITCH_STATUS_SUCCESS && pop) {
				switch_img_free(&img);
				img = (switch_image_t *)pop;
				member->blanks = 0;
			} else {
				break;
			}
			size = switch_queue_size(member->video_queue);
		} while(size > member->conference->video_fps.fps / 2);
		
		if (member_test_flag(member, MFLAG_CAN_BE_SEEN) && member->video_layer_id > -1 && member->video_flow != SWITCH_MEDIA_FLOW_SENDONLY) {
			if (img) {
				member->good_img++;
				if ((member->good_img % (int)(member->conference->video_fps.fps * 10)) == 0) {
					reset_video_bitrate_counters(member);
				}
			} else {
				member->blanks++;
				member->good_img = 0;
						
				if (member->blanks == member->conference->video_fps.fps || (member->blanks % (int)(member->conference->video_fps.fps * 10)) == 0) {
					member->managed_kps = 0;
					switch_core_session_request_video_refresh(member->session);
				}
						
				if (member->blanks == member->conference->video_fps.fps * 5) {
					member->blackouts++;
					check_avatar(member, SWITCH_TRUE);
					member->managed_kps = 0;
							
					if (member->avatar_png_img) {
						//if (layer) {
						//layer->is_avatar = 1;
						//}
								
						member->auto_avatar = 1;
					}
				}
			}
		}
	} else {
		check_flush(member);
	}

	*imgP = img;
}

static void check_auto_bitrate(conference_member_t *member, mcu_layer_t *layer)
{
	if (conference_test_flag(member->conference, CFLAG_MANAGE_INBOUND_VIDEO_BITRATE) && !member->managed_kps) {
		switch_core_session_message_t msg = { 0 };
		int kps;
		int w = 320;
		int h = 240;
				
		if (layer) {
			if (layer->screen_w > 320 && layer->screen_h > 240) {
				w = layer->screen_w;
				h = layer->screen_h;
			}
		}
				
		if (!layer || !member_test_flag(member, MFLAG_CAN_BE_SEEN) || member->avatar_png_img) {
			kps = 200;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "%s auto-setting bitrate to %dkps because user's image is not visible\n", 
							  switch_channel_get_name(member->channel), kps);
		} else {
			kps = switch_calc_bitrate(w, h, 2, member->conference->video_fps.fps);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "%s auto-setting bitrate to %dkps to accomodate %dx%d resolution\n", 
							  switch_channel_get_name(member->channel), kps, layer->screen_w, layer->screen_h);
		}
				
		msg.message_id = SWITCH_MESSAGE_INDICATE_BITRATE_REQ;
		msg.numeric_arg = kps * 1024;
		msg.from = __FILE__;
				
		switch_core_session_receive_message(member->session, &msg);
		member->managed_kps = kps;
	}
}

static void *SWITCH_THREAD_FUNC conference_video_muxing_thread_run(switch_thread_t *thread, void *obj)
{
	mcu_canvas_t *canvas = (mcu_canvas_t *) obj;
	conference_obj_t *conference = canvas->conference;
	conference_member_t *imember;
	switch_codec_t *check_codec = NULL;
	codec_set_t *write_codecs[MAX_MUX_CODECS] = { 0 };
	int buflen = SWITCH_RTP_MAX_BUF_LEN;
	int i = 0;
	uint32_t video_key_freq = 10000000;
	switch_time_t last_key_time = 0;
	mcu_layer_t *layer = NULL;
	switch_frame_t write_frame = { 0 };
	uint8_t *packet = NULL;
	switch_image_t *write_img = NULL, *file_img = NULL;
	uint32_t timestamp = 0;
	//video_layout_t *vlayout = get_layout(conference);
	int members_with_video = 0, members_with_avatar = 0;
	int do_refresh = 0;
	int last_file_count = 0;

	canvas->video_timer_reset = 1;
	
	packet = switch_core_alloc(conference->pool, SWITCH_RTP_MAX_BUF_LEN);

	while (globals.running && !conference_test_flag(conference, CFLAG_DESTRUCT) && conference_test_flag(conference, CFLAG_VIDEO_MUXING)) {
		switch_bool_t need_refresh = SWITCH_FALSE, need_keyframe = SWITCH_FALSE, need_reset = SWITCH_FALSE;
		switch_time_t now;
		int min_members = 0;
		int count_changed = 0;
		int file_count = 0, check_async_file = 0, check_file = 0;
		switch_image_t *async_file_img = NULL, *normal_file_img = NULL, *file_imgs[2] = { 0 };
		switch_frame_t file_frame = { 0 };
		int j = 0;

		switch_mutex_lock(canvas->mutex);
		if (canvas->new_vlayout) {
			init_canvas_layers(conference, canvas, NULL);
		}
		switch_mutex_unlock(canvas->mutex);

		if (canvas->video_timer_reset) {
			canvas->video_timer_reset = 0;

			if (canvas->timer.interval) {
				switch_core_timer_destroy(&canvas->timer);
			}
			
			switch_core_timer_init(&canvas->timer, "soft", conference->video_fps.ms, conference->video_fps.samples, NULL);
			canvas->send_keyframe = 1;
		}

		if (!conference->playing_video_file) {
			switch_core_timer_next(&canvas->timer);
		}

		now = switch_micro_time_now();
		
		if (members_with_video != conference->members_with_video) {
			do_refresh = 100;
			count_changed = 1;
		}
		
		if (members_with_avatar != conference->members_with_avatar) {
			count_changed = 1;
		}

		if (count_changed && !conference_test_flag(conference, CFLAG_PERSONAL_CANVAS)) {
			layout_group_t *lg = NULL;
			video_layout_t *vlayout = NULL;
			int canvas_count = 0;
			
			switch_mutex_lock(conference->member_mutex);
			for (imember = conference->members; imember; imember = imember->next) {
				if (imember->canvas_id == canvas->canvas_id || imember->canvas_id == -1) {
					canvas_count++;
				}
			}
			switch_mutex_unlock(conference->member_mutex);
			
			if (conference->video_layout_group && (lg = switch_core_hash_find(conference->layout_group_hash, conference->video_layout_group))) {
				if ((vlayout = find_best_layout(conference, lg, canvas_count))) {
					switch_mutex_lock(conference->member_mutex);
					conference->canvas->new_vlayout = vlayout;
					switch_mutex_unlock(conference->member_mutex);
				}
			}
		}
		
		if (count_changed) {
			need_refresh = 1;
			need_keyframe = 1;
			do_refresh = 100;
		}
		
		if (conference->async_fnode && switch_core_file_has_video(&conference->async_fnode->fh)) {
			check_async_file = 1;
			file_count++;
		}
		
		if (conference->fnode && switch_core_file_has_video(&conference->fnode->fh)) {
			check_file = 1;
			file_count++;
		}

		if (file_count != last_file_count) {
			count_changed = 1;
		}

		last_file_count = file_count;
		
		if (do_refresh) {
			if ((do_refresh % 50) == 0) {
				switch_mutex_lock(conference->member_mutex);
				
				for (imember = conference->members; imember; imember = imember->next) { 
					if (imember->canvas_id != canvas->canvas_id) continue;

					if (imember->session && switch_channel_test_flag(imember->channel, CF_VIDEO)) {
						switch_core_session_request_video_refresh(imember->session);
						switch_core_media_gen_key_frame(imember->session);
					}
				}
				switch_mutex_unlock(conference->member_mutex);
			}
			do_refresh--;
		}
		
		members_with_video = conference->members_with_video;
		members_with_avatar = conference->members_with_avatar;

		if (conference_test_flag(conference, CFLAG_VIDEO_BRIDGE_FIRST_TWO)) {
			if (conference->members_with_video < 3) {
				switch_yield(20000);
				continue;
			}
		}

		switch_mutex_lock(conference->member_mutex);
		
		for (imember = conference->members; imember; imember = imember->next) {
			switch_image_t *img = NULL;
			int i;
			
			if (!imember->session || (!switch_channel_test_flag(imember->channel, CF_VIDEO) && !imember->avatar_png_img) || 
				conference_test_flag(conference, CFLAG_PERSONAL_CANVAS) || switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS) {
				continue;
			}		

			if (imember->watching_canvas_id == canvas->canvas_id && switch_channel_test_flag(imember->channel, CF_VIDEO_REFRESH_REQ)) {
				switch_channel_clear_flag(imember->channel, CF_VIDEO_REFRESH_REQ);
				need_keyframe = SWITCH_TRUE;
			}

			if (conference_test_flag(conference, CFLAG_MINIMIZE_VIDEO_ENCODING) &&
				imember->watching_canvas_id > -1 && imember->watching_canvas_id == canvas->canvas_id && 
				!member_test_flag(imember, MFLAG_NO_MINIMIZE_ENCODING)) {
				min_members++;
				
				if (switch_channel_test_flag(imember->channel, CF_VIDEO)) {				
					if (imember->video_codec_index < 0 && (check_codec = switch_core_session_get_video_write_codec(imember->session))) {
						for (i = 0; write_codecs[i] && switch_core_codec_ready(&write_codecs[i]->codec) && i < MAX_MUX_CODECS; i++) {
							if (check_codec->implementation->codec_id == write_codecs[i]->codec.implementation->codec_id) {
								imember->video_codec_index = i;
								imember->video_codec_id = check_codec->implementation->codec_id;
								need_refresh = SWITCH_TRUE;
								break;
							}
						}

						if (imember->video_codec_index < 0) {
							write_codecs[i] = switch_core_alloc(conference->pool, sizeof(codec_set_t));
						
							if (switch_core_codec_copy(check_codec, &write_codecs[i]->codec, 
													   &conference->video_codec_settings, conference->pool) == SWITCH_STATUS_SUCCESS) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
												  "Setting up video write codec %s at slot %d\n", write_codecs[i]->codec.implementation->iananame, i);

								imember->video_codec_index = i;
								imember->video_codec_id = check_codec->implementation->codec_id;
								need_refresh = SWITCH_TRUE;
								write_codecs[i]->frame.packet = switch_core_alloc(conference->pool, buflen);
								write_codecs[i]->frame.data = ((uint8_t *)write_codecs[i]->frame.packet) + 12;
								write_codecs[i]->frame.packetlen = buflen;
								write_codecs[i]->frame.buflen = buflen - 12;
								switch_set_flag((&write_codecs[i]->frame), SFF_RAW_RTP);

							}
						}
					}

					if (imember->video_codec_index < 0) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Write Codec Error\n");
						switch_core_session_rwunlock(imember->session);
						continue;
					}
				}
			}

			if (imember->canvas_id > -1 && imember->canvas_id != canvas->canvas_id) {
				switch_core_session_rwunlock(imember->session);
				continue;
			}

			if (conference->playing_video_file) {
				switch_core_session_rwunlock(imember->session);
				continue;
			}

			//VIDFLOOR 
			if (conference->canvas_count == 1 && canvas->layout_floor_id > -1 && imember->id == conference->video_floor_holder && 
				imember->video_layer_id != canvas->layout_floor_id) {
				attach_video_layer(imember, canvas, canvas->layout_floor_id);
			}
			
			pop_next_image(imember, &img);
			layer = NULL;

			switch_mutex_lock(canvas->mutex);
			//printf("MEMBER %d layer_id %d canvas: %d/%d\n", imember->id, imember->video_layer_id,
			//	   canvas->layers_used, canvas->total_layers);
				
			if (imember->video_layer_id > -1) {
				layer = &canvas->layers[imember->video_layer_id];
				if (layer->member_id != imember->id) {
					layer = NULL;
					imember->video_layer_id = -1;
				}
			}

			if (imember->avatar_png_img) {
				if (layer) {
					if (!imember->avatar_patched || !layer->cur_img) {
						layer->tagged = 1;
						//layer->is_avatar = 1;
						switch_img_free(&layer->cur_img);
						switch_img_copy(imember->avatar_png_img, &layer->cur_img);
						imember->avatar_patched = 1;
					}
				}
				switch_img_free(&img);
			}

			if (!layer) {
				if (find_layer(conference, canvas, imember, &layer) != SWITCH_STATUS_SUCCESS) {
					next_canvas(imember);
				}
			}

			check_auto_bitrate(imember, layer);

			if (layer) {
				
				//if (layer->cur_img && layer->cur_img != imember->avatar_png_img) {
				//	switch_img_free(&layer->cur_img);
				//}

				if (member_test_flag(imember, MFLAG_CAN_BE_SEEN)) {
					layer->mute_patched = 0;
				} else {
					switch_image_t *tmp;

					if (img && img != imember->avatar_png_img) {
						switch_img_free(&img);
					}
					
					if (!layer->mute_patched) {

						if (imember->video_mute_img || layer->mute_img) {
							clear_layer(layer);
							
							if (!layer->mute_img && imember->video_mute_img) {
								//layer->mute_img = switch_img_read_png(imember->video_mute_png, SWITCH_IMG_FMT_I420);
								switch_img_copy(imember->video_mute_img, &layer->mute_img);
							}

							if (layer->mute_img) {
								scale_and_patch(layer, layer->mute_img, SWITCH_FALSE);
							}
						} 

						
						tmp = switch_img_write_text_img(layer->screen_w, layer->screen_h, SWITCH_TRUE, "VIDEO MUTED");
						switch_img_patch(canvas->img, tmp, layer->x_pos, layer->y_pos);
						switch_img_free(&tmp);

						layer->mute_patched = 1;
					}
				}


				if (img) {

					if (img != layer->cur_img) {
						switch_img_free(&layer->cur_img);
						layer->cur_img = img;
					}

						
					img = NULL;
					layer->tagged = 1;

					if (switch_core_media_bug_count(imember->session, "patch:video")) {
						layer->bugged = 1;
					}
				}				
			}

			switch_mutex_unlock(canvas->mutex);

			if (img && img != imember->avatar_png_img) {
				switch_img_free(&img);
			}
				
			if (imember->session) {
				switch_core_session_rwunlock(imember->session);
			}
		}
		
		switch_mutex_unlock(conference->member_mutex);

		if (conference_test_flag(conference, CFLAG_PERSONAL_CANVAS)) {
			layout_group_t *lg = NULL;
			video_layout_t *vlayout = NULL;
			conference_member_t *omember;
			
			if (video_key_freq && (now - last_key_time) > video_key_freq) {
				need_keyframe = SWITCH_TRUE;
				last_key_time = now;
			}

			switch_mutex_lock(conference->member_mutex);

			for (imember = conference->members; imember; imember = imember->next) {
				
				if (!imember->session || !switch_channel_test_flag(imember->channel, CF_VIDEO) ||
					switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS) {
					continue;
				}

				if (switch_channel_test_flag(imember->channel, CF_VIDEO_REFRESH_REQ)) {
					switch_channel_clear_flag(imember->channel, CF_VIDEO_REFRESH_REQ);
					need_keyframe = SWITCH_TRUE;
				}
				
				if (count_changed) {
					int total = conference->members_with_video;

					if (!conference_test_flag(conference, CFLAG_VIDEO_REQUIRED_FOR_CANVAS)) {
						total += conference->members_with_avatar;
					}
					
					if (imember->video_flow != SWITCH_MEDIA_FLOW_SENDONLY) {
						total--;
					}

					if (total < 1) total = 1;

					if (conference->video_layout_group && (lg = switch_core_hash_find(conference->layout_group_hash, conference->video_layout_group))) {
						if ((vlayout = find_best_layout(conference, lg, total + file_count))) {
							init_canvas_layers(conference, imember->canvas, vlayout);
						}
					}
				}

				if (imember->video_flow != SWITCH_MEDIA_FLOW_SENDONLY) {
					pop_next_image(imember, &imember->pcanvas_img);
				}

				switch_core_session_rwunlock(imember->session);
			}

			if (check_async_file) {
				if (switch_core_file_read_video(&conference->async_fnode->fh, &file_frame, SVR_BLOCK | SVR_FLUSH) == SWITCH_STATUS_SUCCESS) {
					if ((async_file_img = file_frame.img)) {
						file_imgs[j++] = async_file_img;
					}
				}
			}
			
			if (check_file) {
				if (switch_core_file_read_video(&conference->fnode->fh, &file_frame, SVR_BLOCK | SVR_FLUSH) == SWITCH_STATUS_SUCCESS) {
					if ((normal_file_img = file_frame.img)) {
						file_imgs[j++] = normal_file_img;
					}
				}
			}			

			for (imember = conference->members; imember; imember = imember->next) {
				int i = 0;
				
				if (!imember->session || !switch_channel_test_flag(imember->channel, CF_VIDEO || imember->video_flow == SWITCH_MEDIA_FLOW_SENDONLY) ||
					switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS) {
					continue;
				}
				
				for (omember = conference->members; omember; omember = omember->next) {
					mcu_layer_t *layer = NULL;
					switch_image_t *use_img = NULL;

					if (!omember->session || !switch_channel_test_flag(omember->channel, CF_VIDEO) || omember->video_flow == SWITCH_MEDIA_FLOW_SENDONLY) {
						continue;
					}

					if (conference->members_with_video + conference->members_with_avatar != 1 && imember == omember) {
						continue;
					}
					
					if (i < imember->canvas->total_layers) {
						layer = &imember->canvas->layers[i++];
						if (layer->member_id != omember->id) {
							const char *var = NULL;
							
							layer->mute_patched = 0;
							layer->avatar_patched = 0;
							switch_img_free(&layer->banner_img);
							switch_img_free(&layer->logo_img);
							
							if (layer->geometry.audio_position) {
								conf_api_sub_position(omember, NULL, layer->geometry.audio_position);
							}
							
							var = NULL;
							if (omember->video_banner_text || 
								(var = switch_channel_get_variable_dup(omember->channel, "video_banner_text", SWITCH_FALSE, -1))) {
								layer_set_banner(omember, layer, var);
							}
							
							var = NULL;
							if (omember->video_logo || 
								(var = switch_channel_get_variable_dup(omember->channel, "video_logo_path", SWITCH_FALSE, -1))) {
								layer_set_logo(omember, layer, var);
							}
						}

						layer->member_id = omember->id;
					}
					
					if (!layer && omember->al) {
						conf_api_sub_position(omember, NULL, "0:0:0");
					}
					
					use_img = omember->pcanvas_img;
					
					if (layer) {
						
						if (use_img && !omember->avatar_png_img) {
							layer->avatar_patched = 0;
						} else {
							if (!layer->avatar_patched) {
								scale_and_patch(layer, omember->avatar_png_img, SWITCH_FALSE);
								layer->avatar_patched = 1;
							}
							use_img = NULL;
							layer = NULL;
						}

						if (layer) {
							if (member_test_flag(imember, MFLAG_CAN_BE_SEEN)) {
								layer->mute_patched = 0;
							} else {
								if (!layer->mute_patched) {
									switch_image_t *tmp;
									scale_and_patch(layer, imember->video_mute_img ? imember->video_mute_img : omember->pcanvas_img, SWITCH_FALSE);
									tmp = switch_img_write_text_img(layer->screen_w, layer->screen_h, SWITCH_TRUE, "VIDEO MUTED");
									switch_img_patch(imember->canvas->img, tmp, layer->x_pos, layer->y_pos);
									switch_img_free(&tmp);
									layer->mute_patched = 1;
								}
							
								use_img = NULL;
								layer = NULL;
							}
						}
						
						if (layer && use_img) {
							scale_and_patch(layer, use_img, SWITCH_FALSE);
						}
					}

					check_auto_bitrate(omember, layer);
				}

				for (j = 0; j < file_count; j++) {
					switch_image_t *img = file_imgs[j];

					if (i < imember->canvas->total_layers) {
						layer = &imember->canvas->layers[i++];
						scale_and_patch(layer, img, SWITCH_FALSE);
					}
				}
				
				switch_core_session_rwunlock(imember->session);
			}

			switch_img_free(&normal_file_img);
			switch_img_free(&async_file_img);

			for (imember = conference->members; imember; imember = imember->next) {
				switch_frame_t *dupframe;
				
				if (!imember->session || !switch_channel_test_flag(imember->channel, CF_VIDEO) ||
					switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS) {
					continue;
				}

				if (need_refresh) {
					switch_core_session_request_video_refresh(imember->session);
				}

				if (need_keyframe) {
					switch_core_media_gen_key_frame(imember->session);
				}

				switch_set_flag(&write_frame, SFF_RAW_RTP);
				write_frame.img = imember->canvas->img;
				write_frame.packet = packet;
				write_frame.data = ((uint8_t *)packet) + 12;
				write_frame.datalen = 0;
				write_frame.buflen = SWITCH_RTP_MAX_BUF_LEN - 12;
				write_frame.packetlen = 0;
			
				if (switch_frame_buffer_dup(imember->fb, &write_frame, &dupframe) == SWITCH_STATUS_SUCCESS) {
					switch_queue_push(imember->mux_out_queue, dupframe);
					dupframe = NULL;
				}

				switch_core_session_rwunlock(imember->session);
			}
			
			switch_mutex_unlock(conference->member_mutex);
		} else {

			if (canvas->canvas_id == 0) {
				if (conference->async_fnode) {
					if (conference->async_fnode->layer_id > -1) { 
						patch_fnode(canvas, conference->async_fnode);
					} else {
						fnode_check_video(conference->async_fnode);
					}
				}
			
				if (conference->fnode) {
					if (conference->fnode->layer_id > -1) {
						patch_fnode(canvas, conference->fnode);
					} else {
						fnode_check_video(conference->fnode);
					}
				}
			}
		
			if (!conference->playing_video_file) {
				for (i = 0; i < canvas->total_layers; i++) {
					mcu_layer_t *layer = &canvas->layers[i];

					if (!layer->mute_patched && (layer->member_id > -1 || layer->fnode) && layer->cur_img && (layer->tagged || layer->geometry.overlap)) {
						if (canvas->refresh) {
							layer->refresh = 1;
							canvas->refresh++;
						}

						if (layer->cur_img) {
							scale_and_patch(layer, NULL, SWITCH_FALSE);
						}
					
						layer->tagged = 0;
					}
				
					layer->bugged = 0;
				}
			}

			if (canvas->refresh > 1) {
				canvas->refresh = 0;
			}

			if (canvas->send_keyframe > 0) {
				if (canvas->send_keyframe == 1 || (canvas->send_keyframe % 10) == 0) {
					need_keyframe = SWITCH_TRUE;
					need_refresh = SWITCH_TRUE;
				}
				canvas->send_keyframe--;
			}

			if (video_key_freq && (now - last_key_time) > video_key_freq) {
				need_keyframe = SWITCH_TRUE;
				last_key_time = now;
			}
			
			write_img = canvas->img;
			timestamp = canvas->timer.samplecount;

			if (conference->playing_video_file) {
				if (switch_core_file_read_video(&conference->fnode->fh, &write_frame, SVR_BLOCK | SVR_FLUSH) == SWITCH_STATUS_SUCCESS) {
					switch_img_free(&file_img);

					if (canvas->play_file) {
						canvas->send_keyframe = 1;
						canvas->play_file = 0;
					
						canvas->timer.interval = 1;
						canvas->timer.samples = 90;
					}

					write_img = file_img = write_frame.img;

					switch_core_timer_sync(&canvas->timer);
					timestamp = canvas->timer.samplecount;
				}
			} else if (file_img) {
				switch_img_free(&file_img);
			}

			write_frame.img = write_img;
		
			if (conference->canvas_count == 1) {
				check_video_recording(conference, &write_frame);
			}

			if (conference->canvas_count > 1) {
				switch_image_t *img_copy = NULL;

				switch_img_copy(write_img, &img_copy);

				if (switch_queue_trypush(canvas->video_queue, img_copy) != SWITCH_STATUS_SUCCESS) {
					switch_img_free(&img_copy);
				}
			}

			if (min_members && conference_test_flag(conference, CFLAG_MINIMIZE_VIDEO_ENCODING)) {
				for (i = 0; write_codecs[i] && switch_core_codec_ready(&write_codecs[i]->codec) && i < MAX_MUX_CODECS; i++) {
					write_codecs[i]->frame.img = write_img;
					write_canvas_image_to_codec_group(conference, canvas, write_codecs[i], i, 
													  timestamp, need_refresh, need_keyframe, need_reset);

					if (canvas->video_write_bandwidth) {
						switch_core_codec_control(&write_codecs[i]->codec, SCC_VIDEO_BANDWIDTH, SCCT_INT, &canvas->video_write_bandwidth, NULL, NULL);
						canvas->video_write_bandwidth = 0;
					}

				}
			}

			switch_mutex_lock(conference->member_mutex);
			for (imember = conference->members; imember; imember = imember->next) {
				switch_frame_t *dupframe;

				if (imember->watching_canvas_id != canvas->canvas_id) continue;

				if (conference_test_flag(conference, CFLAG_MINIMIZE_VIDEO_ENCODING) && !member_test_flag(imember, MFLAG_NO_MINIMIZE_ENCODING)) {
					continue;
				}

				if (!imember->session || !switch_channel_test_flag(imember->channel, CF_VIDEO) ||
					switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS) {
					continue;
				}

				if (need_refresh) {
					switch_core_session_request_video_refresh(imember->session);
				}

				if (need_keyframe) {
					switch_core_media_gen_key_frame(imember->session);
				}

				switch_set_flag(&write_frame, SFF_RAW_RTP);
				write_frame.img = write_img;
				write_frame.packet = packet;
				write_frame.data = ((uint8_t *)packet) + 12;
				write_frame.datalen = 0;
				write_frame.buflen = SWITCH_RTP_MAX_BUF_LEN - 12;
				write_frame.packetlen = 0;
			
				//switch_core_session_write_video_frame(imember->session, &write_frame, SWITCH_IO_FLAG_NONE, 0);
			
				if (switch_frame_buffer_dup(imember->fb, &write_frame, &dupframe) == SWITCH_STATUS_SUCCESS) {
					switch_queue_push(imember->mux_out_queue, dupframe);
					dupframe = NULL;
				}

				if (imember->session) {
					switch_core_session_rwunlock(imember->session);					
				}
			}

			switch_mutex_unlock(conference->member_mutex);
		} // NOT PERSONAL
	}

	switch_img_free(&file_img);

	for (i = 0; i < MCU_MAX_LAYERS; i++) {
		layer = &canvas->layers[i];

		switch_mutex_lock(canvas->mutex);
		switch_img_free(&layer->cur_img);
		switch_img_free(&layer->img);
		layer->banner_patched = 0;
		switch_img_free(&layer->banner_img);
		switch_img_free(&layer->logo_img);
		switch_img_free(&layer->logo_text_img);
		switch_img_free(&layer->mute_img);
		switch_mutex_unlock(canvas->mutex);

		if (layer->txthandle) {
			switch_img_txt_handle_destroy(&layer->txthandle);
		}
	}

	for (i = 0; i < MAX_MUX_CODECS; i++) {
		if (write_codecs[i] && switch_core_codec_ready(&write_codecs[i]->codec)) {
			switch_core_codec_destroy(&write_codecs[i]->codec);
		}
	}

	switch_core_timer_destroy(&canvas->timer);
	destroy_canvas(&canvas);

	return NULL;
}

static void pop_next_canvas_image(mcu_canvas_t *canvas, switch_image_t **imgP)
{
	switch_image_t *img = *imgP;
	int size = 0;
	void *pop;

	switch_img_free(&img);

	do {
		if (switch_queue_trypop(canvas->video_queue, &pop) == SWITCH_STATUS_SUCCESS && pop) {
			switch_img_free(&img);
			img = (switch_image_t *)pop;
		} else {
			break;
		}
		size = switch_queue_size(canvas->video_queue);
	} while(size > canvas->conference->video_fps.fps / 2);

	*imgP = img;
}

static void *SWITCH_THREAD_FUNC conference_super_video_muxing_thread_run(switch_thread_t *thread, void *obj)
{
	mcu_canvas_t *canvas = (mcu_canvas_t *) obj;
	conference_obj_t *conference = canvas->conference;
	conference_member_t *imember;
	switch_codec_t *check_codec = NULL;
	codec_set_t *write_codecs[MAX_MUX_CODECS] = { 0 };
	int buflen = SWITCH_RTP_MAX_BUF_LEN;
	int i = 0;
	switch_time_t last_key_time = 0;
	uint32_t video_key_freq = 10000000;
	mcu_layer_t *layer = NULL;
	switch_frame_t write_frame = { 0 };
	uint8_t *packet = NULL;
	switch_image_t *write_img = NULL;
	uint32_t timestamp = 0;
	int last_used_canvases[MAX_CANVASES] = { 0 };


	canvas->video_timer_reset = 1;
	
	packet = switch_core_alloc(conference->pool, SWITCH_RTP_MAX_BUF_LEN);

	while (globals.running && !conference_test_flag(conference, CFLAG_DESTRUCT) && conference_test_flag(conference, CFLAG_VIDEO_MUXING)) {
		switch_bool_t need_refresh = SWITCH_FALSE, need_keyframe = SWITCH_FALSE, need_reset = SWITCH_FALSE;
		switch_time_t now;
		int min_members = 0;
		int count_changed = 0;
		int  layer_idx = 0, j = 0;
		switch_image_t *img = NULL;
		int used_canvases = 0;

		switch_mutex_lock(canvas->mutex);
		if (canvas->new_vlayout) {
			init_canvas_layers(conference, canvas, NULL);
		}
		switch_mutex_unlock(canvas->mutex);
		
		if (canvas->video_timer_reset) {
			canvas->video_timer_reset = 0;

			if (canvas->timer.interval) {
				switch_core_timer_destroy(&canvas->timer);
			}
			
			switch_core_timer_init(&canvas->timer, "soft", conference->video_fps.ms, conference->video_fps.samples, NULL);
			canvas->send_keyframe = 1;
		}

		if (!conference->playing_video_file) {
			switch_core_timer_next(&canvas->timer);
		}

		now = switch_micro_time_now();

		if (canvas->send_keyframe > 0) {
			if (canvas->send_keyframe == 1 || (canvas->send_keyframe % 10) == 0) {
				need_keyframe = SWITCH_TRUE;
				need_refresh = SWITCH_TRUE;
			}
			canvas->send_keyframe--;
		}

		if (video_key_freq && (now - last_key_time) > video_key_freq) {
			need_keyframe = SWITCH_TRUE;
			last_key_time = now;
		}

		for (j = 0; j < conference->canvas_count; j++) {
			mcu_canvas_t *jcanvas = (mcu_canvas_t *) conference->canvases[j];

			if (jcanvas->layers_used > 0 || conference->super_canvas_show_all_layers) {
				used_canvases++;
			}

			if (jcanvas->layers_used != last_used_canvases[j]) {
				count_changed++;
			}
			
			last_used_canvases[j] = jcanvas->layers_used;
		}
		
		if (count_changed) {
			int total = used_canvases;
			layout_group_t *lg = NULL;
			video_layout_t *vlayout = NULL;

			if (total < 1) total = 1;

			if ((lg = switch_core_hash_find(conference->layout_group_hash, CONFERENCE_MUX_DEFAULT_SUPER_LAYOUT))) {
				if ((vlayout = find_best_layout(conference, lg, total))) {
					init_canvas_layers(conference, canvas, vlayout);
				}
			}
		}

		switch_mutex_lock(conference->member_mutex);
		
		for (imember = conference->members; imember; imember = imember->next) {
			int i;
			
			if (!imember->session || (!switch_channel_test_flag(imember->channel, CF_VIDEO) && !imember->avatar_png_img) || 
				conference_test_flag(conference, CFLAG_PERSONAL_CANVAS) || switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS) {
				continue;
			}		

			if (imember->watching_canvas_id == canvas->canvas_id && switch_channel_test_flag(imember->channel, CF_VIDEO_REFRESH_REQ)) {
				switch_channel_clear_flag(imember->channel, CF_VIDEO_REFRESH_REQ);
				need_keyframe = SWITCH_TRUE;
			}
			
			if (conference_test_flag(conference, CFLAG_MINIMIZE_VIDEO_ENCODING) &&
				imember->watching_canvas_id > -1 && imember->watching_canvas_id == canvas->canvas_id && 
				!member_test_flag(imember, MFLAG_NO_MINIMIZE_ENCODING)) {
				min_members++;
				
				if (switch_channel_test_flag(imember->channel, CF_VIDEO)) {				
					if (imember->video_codec_index < 0 && (check_codec = switch_core_session_get_video_write_codec(imember->session))) {
						for (i = 0; write_codecs[i] && switch_core_codec_ready(&write_codecs[i]->codec) && i < MAX_MUX_CODECS; i++) {
							if (check_codec->implementation->codec_id == write_codecs[i]->codec.implementation->codec_id) {
								imember->video_codec_index = i;
								imember->video_codec_id = check_codec->implementation->codec_id;
								need_refresh = SWITCH_TRUE;
								break;
							}
						}

						if (imember->video_codec_index < 0) {
							write_codecs[i] = switch_core_alloc(conference->pool, sizeof(codec_set_t));
						
							if (switch_core_codec_copy(check_codec, &write_codecs[i]->codec, 
													   &conference->video_codec_settings, conference->pool) == SWITCH_STATUS_SUCCESS) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
												  "Setting up video write codec %s at slot %d\n", write_codecs[i]->codec.implementation->iananame, i);

								imember->video_codec_index = i;
								imember->video_codec_id = check_codec->implementation->codec_id;
								need_refresh = SWITCH_TRUE;
								write_codecs[i]->frame.packet = switch_core_alloc(conference->pool, buflen);
								write_codecs[i]->frame.data = ((uint8_t *)write_codecs[i]->frame.packet) + 12;
								write_codecs[i]->frame.packetlen = buflen;
								write_codecs[i]->frame.buflen = buflen - 12;
								switch_set_flag((&write_codecs[i]->frame), SFF_RAW_RTP);

							}
						}
					}

					if (imember->video_codec_index < 0) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Write Codec Error\n");
						switch_core_session_rwunlock(imember->session);
						continue;
					}
				}
			}

			switch_core_session_rwunlock(imember->session);
		}
		
		switch_mutex_unlock(conference->member_mutex);

		layer_idx = 0;

		for (j = 0; j < conference->canvas_count; j++) {
			mcu_canvas_t *jcanvas = (mcu_canvas_t *) conference->canvases[j];
			
			pop_next_canvas_image(jcanvas, &img);

			if (!jcanvas->layers_used && !conference->super_canvas_show_all_layers) {
				switch_img_free(&img);
				continue;
			}
			
			if (layer_idx < canvas->total_layers) {
				layer = &canvas->layers[layer_idx++];

				if (layer->member_id != jcanvas->canvas_id) {
					layer->member_id = jcanvas->canvas_id;
					switch_img_free(&layer->cur_img);
				}
				
				if (canvas->refresh) {
					layer->refresh = 1;
					canvas->refresh++;
				}

				if (img) {

					if (conference->super_canvas_label_layers) {
						char str[80] = "";
						switch_image_t *tmp;
						const char *format = "#cccccc:#142e55:FreeSans.ttf:4%:";
						
						switch_snprintf(str, sizeof(str), "%sCanvas %d", format, jcanvas->canvas_id + 1);
						tmp = switch_img_write_text_img(img->d_w, img->d_h, SWITCH_TRUE, str);
						switch_img_patch(img, tmp, 0, 0);
						switch_img_free(&tmp);
					}

					switch_img_free(&layer->cur_img);
					layer->cur_img = img;
					img = NULL;
				}

				scale_and_patch(layer, NULL, SWITCH_FALSE);
			}

			switch_img_free(&img);
		}
			
		if (canvas->refresh > 1) {
			canvas->refresh = 0;
		}

		write_img = canvas->img;
		timestamp = canvas->timer.samplecount;

		if (!write_img) continue;

		write_frame.img = write_img;
		check_video_recording(conference, &write_frame);

		if (min_members && conference_test_flag(conference, CFLAG_MINIMIZE_VIDEO_ENCODING)) {
			for (i = 0; write_codecs[i] && switch_core_codec_ready(&write_codecs[i]->codec) && i < MAX_MUX_CODECS; i++) {
				write_codecs[i]->frame.img = write_img;
				write_canvas_image_to_codec_group(conference, canvas, write_codecs[i], i, timestamp, need_refresh, need_keyframe, need_reset);
				
				if (canvas->video_write_bandwidth) {
					switch_core_codec_control(&write_codecs[i]->codec, SCC_VIDEO_BANDWIDTH, SCCT_INT, &canvas->video_write_bandwidth, NULL, NULL);
					canvas->video_write_bandwidth = 0;
				}
			}
		}
		
		switch_mutex_lock(conference->member_mutex);
		for (imember = conference->members; imember; imember = imember->next) {
			switch_frame_t *dupframe;

			if (imember->watching_canvas_id != canvas->canvas_id) continue;

			if (conference_test_flag(conference, CFLAG_MINIMIZE_VIDEO_ENCODING) && !member_test_flag(imember, MFLAG_NO_MINIMIZE_ENCODING)) {
				continue;
			}

			if (!imember->session || !switch_channel_test_flag(imember->channel, CF_VIDEO) ||
				switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS) {
				continue;
			}

			if (need_refresh) {
				switch_core_session_request_video_refresh(imember->session);
			}

			if (need_keyframe) {
				switch_core_media_gen_key_frame(imember->session);
			}

			switch_set_flag(&write_frame, SFF_RAW_RTP);
			write_frame.img = write_img;
			write_frame.packet = packet;
			write_frame.data = ((uint8_t *)packet) + 12;
			write_frame.datalen = 0;
			write_frame.buflen = SWITCH_RTP_MAX_BUF_LEN - 12;
			write_frame.packetlen = 0;
			
			//switch_core_session_write_video_frame(imember->session, &write_frame, SWITCH_IO_FLAG_NONE, 0);
			
			if (switch_frame_buffer_dup(imember->fb, &write_frame, &dupframe) == SWITCH_STATUS_SUCCESS) {
				switch_queue_push(imember->mux_out_queue, dupframe);
				dupframe = NULL;
			}

			if (imember->session) {
				switch_core_session_rwunlock(imember->session);					
			}
		}

		switch_mutex_unlock(conference->member_mutex);
	}

	for (i = 0; i < MCU_MAX_LAYERS; i++) {
		layer = &canvas->layers[i];

		switch_mutex_lock(canvas->mutex);
		switch_img_free(&layer->cur_img);
		switch_img_free(&layer->img);
		layer->banner_patched = 0;
		switch_img_free(&layer->banner_img);
		switch_img_free(&layer->logo_img);
		switch_img_free(&layer->logo_text_img);
		switch_img_free(&layer->mute_img);
		switch_mutex_unlock(canvas->mutex);

		if (layer->txthandle) {
			switch_img_txt_handle_destroy(&layer->txthandle);
		}
	}

	for (i = 0; i < MAX_MUX_CODECS; i++) {
		if (write_codecs[i] && switch_core_codec_ready(&write_codecs[i]->codec)) {
			switch_core_codec_destroy(&write_codecs[i]->codec);
		}
	}

	switch_core_timer_destroy(&canvas->timer);
	destroy_canvas(&canvas);

	return NULL;
}



static al_handle_t *create_al(switch_memory_pool_t *pool)
{
	al_handle_t *al;

	al = switch_core_alloc(pool, sizeof(al_handle_t));
	switch_mutex_init(&al->mutex, SWITCH_MUTEX_NESTED, pool);

	return al;
}

#ifndef OPENAL_POSITIONING
static void gen_arc(conference_obj_t *conference, switch_stream_handle_t *stream)
{
}
static void process_al(al_handle_t *al, void *data, switch_size_t datalen, int rate)
{
}

#else
static void gen_arc(conference_obj_t *conference, switch_stream_handle_t *stream)
{
	float offset;
	float pos;
	float radius;
	float x, z;
	float div = 3.14159f / 180;
	conference_member_t *member;
	uint32_t count = 0;

	if (!conference->count) {
		return;
	}

	switch_mutex_lock(conference->member_mutex);
	for (member = conference->members; member; member = member->next) {
		if (member->channel && member_test_flag(member, MFLAG_CAN_SPEAK) && !member_test_flag(member, MFLAG_NO_POSITIONAL)) {
			count++;
		}
	}

	if (count < 3) {
		for (member = conference->members; member; member = member->next) {
			if (member->channel && !member_test_flag(member, MFLAG_NO_POSITIONAL) && member->al) {

				member->al->pos_x = 0;
				member->al->pos_y = 0;
				member->al->pos_z = 0;
				member->al->setpos = 1;

				if (stream) {
					stream->write_function(stream, "Member %d (%s) 0.0:0.0:0.0\n", member->id, switch_channel_get_name(member->channel));
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Member %d (%s) 0.0:0.0:0.0\n", 
									  member->id, switch_channel_get_name(member->channel));
				}
			}
		}

		goto end;
	}

	offset = 180 / (count - 1);

	radius = 1.0f;

	pos = -90.0f;
	
	for (member = conference->members; member; member = member->next) {

		if (!member->channel || member_test_flag(member, MFLAG_NO_POSITIONAL) || !member_test_flag(member, MFLAG_CAN_SPEAK)) {
			continue;
		}

		if (!member->al) {
			member->al = create_al(member->pool);
		}
		member_set_flag(member, MFLAG_POSITIONAL);

		if (pos == 0) {
			x = 0;
			z = radius;
		} else if (pos == -90) {
			z = 0;
			x = radius * -1;
		} else if (pos == 90) {
			z = 0;
			x = radius;
		} else if (pos < 0) {
			z = cos((90+pos) * div) * radius;
			x = sin((90+pos) * div) * radius * -1.0f;
		} else {
			x = cos(pos * div) * radius;
			z = sin(pos * div) * radius;
		}

		member->al->pos_x = x;
		member->al->pos_y = 0;
		member->al->pos_z = z;
		member->al->setpos = 1;

		if (stream) {
			stream->write_function(stream, "Member %d (%s) %0.2f:0.0:%0.2f\n", member->id, switch_channel_get_name(member->channel), x, z);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Member %d (%s) %0.2f:0.0:%0.2f\n", 
							  member->id, switch_channel_get_name(member->channel), x, z);
		}

		pos += offset;
	}
		
 end:

	switch_mutex_unlock(conference->member_mutex);

	return;

}


#define ALC_HRTF_SOFT  0x1992

static void process_al(al_handle_t *al, void *data, switch_size_t datalen, int rate)
{

	if (rate != 48000) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Only 48khz is supported.\n");
		return;
	}

	if (!al->device) {
		ALCint contextAttr[] = {
			ALC_FORMAT_CHANNELS_SOFT, ALC_STEREO_SOFT,
			ALC_FORMAT_TYPE_SOFT, ALC_SHORT_SOFT,
			ALC_FREQUENCY, rate,
			ALC_HRTF_SOFT, AL_TRUE,
			0
		};

		switch_mutex_lock(globals.setup_mutex);
		if ((al->device = alcLoopbackOpenDeviceSOFT(NULL))) {
			static const ALshort silence[16] = { 0 };
			float orient[6] = { /*fwd:*/ 0., 0., -1., /*up:*/ 0., 1., 0. };
			
			al->context = alcCreateContext(al->device, contextAttr);
			alcSetThreadContext(al->context);
			
			/* listener at origin, facing down -z (ears at 0.0m height) */
			alListener3f( AL_POSITION, 0. ,0, 0. );
			alListener3f( AL_VELOCITY, 0., 0., 0. );
			alListenerfv( AL_ORIENTATION, orient );

			
			alGenSources(1, &al->source);
			alSourcef( al->source, AL_PITCH, 1.);
			alSourcef( al->source, AL_GAIN, 1.);
			alGenBuffers(2, al->buffer_in);

			alBufferData(al->buffer_in[0], AL_FORMAT_MONO16, data, datalen, rate);
			//alBufferData(al->buffer_in[0], AL_FORMAT_MONO16, NULL, 0, rate);
			alBufferData(al->buffer_in[1], AL_FORMAT_MONO16, silence, sizeof(silence), rate);
			alSourceQueueBuffers(al->source, 2, al->buffer_in);
			alSourcePlay(al->source);
		}
		switch_mutex_unlock(globals.setup_mutex);
	}

	if (al->device) {
		ALint processed = 0, state = 0;
		
		//alcSetThreadContext(al->context);
		alGetSourcei(al->source, AL_SOURCE_STATE, &state);
		alGetSourcei(al->source, AL_BUFFERS_PROCESSED, &processed);
				
		if (al->setpos) {
			al->setpos = 0;
			alSource3f(al->source, AL_POSITION, al->pos_x, al->pos_y, al->pos_z);
			//alSource3f(al->source, AL_VELOCITY, .01, 0., 0.);
		}
		
		if (processed > 0) {
			ALuint bufid;
			alSourceUnqueueBuffers(al->source, 1, &bufid);
			alBufferData(bufid, AL_FORMAT_MONO16, data, datalen, rate);
			alSourceQueueBuffers(al->source, 1, &bufid);
		}

		if (state != AL_PLAYING) {
			alSourcePlay(al->source);
		}
		
		alcRenderSamplesSOFT(al->device, data, datalen / 2);
	}
}
#endif

static void conference_cdr_del(conference_member_t *member)
{
	if (member->channel) {
		switch_channel_get_variables(member->channel, &member->cdr_node->var_event);
	}
	if (member->cdr_node) {
		member->cdr_node->leave_time = switch_epoch_time_now(NULL);
		memcpy(member->cdr_node->mflags, member->flags, sizeof(member->flags));
		member->cdr_node->member = NULL;
	}
}

static void conference_cdr_add(conference_member_t *member)
{
	conference_cdr_node_t *np;
	switch_caller_profile_t *cp;
	switch_channel_t *channel;

	np = switch_core_alloc(member->conference->pool, sizeof(*np));

	np->next = member->conference->cdr_nodes;
	member->conference->cdr_nodes = member->cdr_node = np;
	member->cdr_node->join_time = switch_epoch_time_now(NULL);
	member->cdr_node->member = member;

	if (!member->session) {
		member->cdr_node->record_path = switch_core_strdup(member->conference->pool, member->rec_path);
		return;
	}

	channel = switch_core_session_get_channel(member->session);

	if (!(cp = switch_channel_get_caller_profile(channel))) {
		return;
	}

	member->cdr_node->cp = switch_caller_profile_dup(member->conference->pool, cp);

	member->cdr_node->id = member->id;
	

	
}

static void conference_cdr_rejected(conference_obj_t *conference, switch_channel_t *channel, cdr_reject_reason_t reason)
{
	conference_cdr_reject_t *rp;
	switch_caller_profile_t *cp;

	rp = switch_core_alloc(conference->pool, sizeof(*rp));

	rp->next = conference->cdr_rejected;
	conference->cdr_rejected = rp;
	rp->reason = reason;
	rp->reject_time = switch_epoch_time_now(NULL);

	if (!(cp = switch_channel_get_caller_profile(channel))) {
		return;
	}

	rp->cp = switch_caller_profile_dup(conference->pool, cp);
}

static const char *audio_flow(conference_member_t *member)
{
	const char *flow = "sendrecv";

	if (!member_test_flag(member, MFLAG_CAN_SPEAK)) {
		flow = "recvonly";
	}

	if (member->channel && switch_channel_test_flag(member->channel, CF_HOLD)) {
		flow = member_test_flag(member, MFLAG_CAN_SPEAK) ? "sendonly" : "inactive";
	}

	return flow;
}

static char *conference_rfc4579_render(conference_obj_t *conference, switch_event_t *event, switch_event_t *revent)
{
	switch_xml_t xml, x_tag, x_tag1, x_tag2, x_tag3, x_tag4;
	char tmp[30];
	const char *domain;	const char *name;
	char *dup_domain = NULL;
	char *uri;
	int off = 0, off1 = 0, off2 = 0, off3 = 0, off4 = 0;
	conference_cdr_node_t *np;
	char *tmpp = tmp;
	char *xml_text = NULL;

	if (!(xml = switch_xml_new("conference-info"))) {
		abort();
	}
	
	switch_mutex_lock(conference->mutex);
	switch_snprintf(tmp, sizeof(tmp), "%u", conference->doc_version);
	conference->doc_version++;
	switch_mutex_unlock(conference->mutex);

	if (!event || !(name = switch_event_get_header(event, "conference-name"))) {
		if (!(name = conference->name)) {
			name = "conference";
		}
	}

	if (!event || !(domain = switch_event_get_header(event, "conference-domain"))) {
		if (!(domain = conference->domain)) {
			dup_domain = switch_core_get_domain(SWITCH_TRUE);
			if (!(domain = dup_domain)) {
				domain = "cluecon.com";
			}
		}
	}
		
	switch_xml_set_attr_d(xml, "version", tmpp);

	switch_xml_set_attr_d(xml, "state", "full");
	switch_xml_set_attr_d(xml, "xmlns", "urn:ietf:params:xml:ns:conference-info");
	

	uri = switch_mprintf("sip:%s@%s", name, domain);
	switch_xml_set_attr_d(xml, "entity", uri);

	if (!(x_tag = switch_xml_add_child_d(xml, "conference-description", off++))) {
		abort();
	}
	
	if (!(x_tag1 = switch_xml_add_child_d(x_tag, "display-text", off1++))) {
		abort();
	}
	switch_xml_set_txt_d(x_tag1, conference->desc ? conference->desc : "FreeSWITCH Conference");


	if (!(x_tag1 = switch_xml_add_child_d(x_tag, "conf-uris", off1++))) {
		abort();
	}

	if (!(x_tag2 = switch_xml_add_child_d(x_tag1, "entry", off2++))) {
		abort();
	}

	if (!(x_tag3 = switch_xml_add_child_d(x_tag2, "uri", off3++))) {
		abort();
	}
	switch_xml_set_txt_d(x_tag3, uri);



	if (!(x_tag = switch_xml_add_child_d(xml, "conference-state", off++))) {
		abort();
	}
	if (!(x_tag1 = switch_xml_add_child_d(x_tag, "user-count", off1++))) {
		abort();
	}
	switch_snprintf(tmp, sizeof(tmp), "%u", conference->count);
	switch_xml_set_txt_d(x_tag1, tmpp);	

#if 0
	if (conference->count == 0) {
		switch_event_add_header(revent, SWITCH_STACK_BOTTOM, "notfound", "true");
	}
#endif

	if (!(x_tag1 = switch_xml_add_child_d(x_tag, "active", off1++))) {
		abort();
	}
	switch_xml_set_txt_d(x_tag1, "true");	
	
	off1 = off2 = off3 = off4 = 0;

	if (!(x_tag = switch_xml_add_child_d(xml, "users", off++))) {
		abort();
	}
	
	switch_mutex_lock(conference->member_mutex);
	
	for (np = conference->cdr_nodes; np; np = np->next) {
		char *user_uri = NULL;
		switch_channel_t *channel = NULL;
		
		if (!np->cp || (np->member && !np->member->session) || np->leave_time) { /* for now we'll remove participants when the leave */
			continue;
		}

		if (np->member && np->member->session) {
			channel = switch_core_session_get_channel(np->member->session);
		}

		if (!(x_tag1 = switch_xml_add_child_d(x_tag, "user", off1++))) {
			abort();
		}

		if (channel) {
			const char *uri = switch_channel_get_variable_dup(channel, "conference_invite_uri", SWITCH_FALSE, -1);

			if (uri) {
				user_uri = strdup(uri);
			}
		}
		
		if (!user_uri) {
			user_uri = switch_mprintf("sip:%s@%s", np->cp->caller_id_number, domain);
		}
		
		
		switch_xml_set_attr_d(x_tag1, "state", "full");
		switch_xml_set_attr_d(x_tag1, "entity", user_uri);

		if (!(x_tag2 = switch_xml_add_child_d(x_tag1, "display-text", off2++))) {
			abort();
		}
		switch_xml_set_txt_d(x_tag2, np->cp->caller_id_name);
		

		if (!(x_tag2 = switch_xml_add_child_d(x_tag1, "endpoint", off2++))) {
			abort();
		}
		switch_xml_set_attr_d(x_tag2, "entity", user_uri);
		
		if (!(x_tag3 = switch_xml_add_child_d(x_tag2, "display-text", off3++))) {
			abort();
		}
		switch_xml_set_txt_d(x_tag3, np->cp->caller_id_name);


		if (!(x_tag3 = switch_xml_add_child_d(x_tag2, "status", off3++))) {
			abort();
		}
		switch_xml_set_txt_d(x_tag3, np->leave_time ? "disconnected" : "connected");


		if (!(x_tag3 = switch_xml_add_child_d(x_tag2, "joining-info", off3++))) {
			abort();
		}
		if (!(x_tag4 = switch_xml_add_child_d(x_tag3, "when", off4++))) {
			abort();
		} else {
			switch_time_exp_t tm;
			switch_size_t retsize;
			const char *fmt = "%Y-%m-%dT%H:%M:%S%z";
			char *p;

			switch_time_exp_lt(&tm, (switch_time_t) conference->start_time * 1000000);
			switch_strftime_nocheck(tmp, &retsize, sizeof(tmp), fmt, &tm);			
			p = end_of_p(tmpp) -1;
			snprintf(p, 4, ":00");


			switch_xml_set_txt_d(x_tag4, tmpp);
		}


		

		/** ok so this is in the rfc but not the xsd 
		if (!(x_tag3 = switch_xml_add_child_d(x_tag2, "joining-method", off3++))) {
			abort();
		}
		switch_xml_set_txt_d(x_tag3, np->cp->direction == SWITCH_CALL_DIRECTION_INBOUND ? "dialed-in" : "dialed-out");
		*/

		if (np->member) {
			const char *var;
			//char buf[1024];

			//switch_snprintf(buf, sizeof(buf), "conf_%s_%s_%s", conference->name, conference->domain, np->cp->caller_id_number);
			//switch_channel_set_variable(channel, "conference_call_key", buf);

			if (!(x_tag3 = switch_xml_add_child_d(x_tag2, "media", off3++))) {
				abort();
			}
			
			snprintf(tmp, sizeof(tmp), "%ua", np->member->id);
			switch_xml_set_attr_d(x_tag3, "id", tmpp);


			if (!(x_tag4 = switch_xml_add_child_d(x_tag3, "type", off4++))) {
				abort();
			}
			switch_xml_set_txt_d(x_tag4, "audio");

			if ((var = switch_channel_get_variable(channel, "rtp_use_ssrc"))) {
				if (!(x_tag4 = switch_xml_add_child_d(x_tag3, "src-id", off4++))) {
					abort();
				}
				switch_xml_set_txt_d(x_tag4, var);
			}
			
			if (!(x_tag4 = switch_xml_add_child_d(x_tag3, "status", off4++))) {
				abort();
			}
			switch_xml_set_txt_d(x_tag4, audio_flow(np->member));
			
			
			if (switch_channel_test_flag(channel, CF_VIDEO)) {
				off4 = 0;

				if (!(x_tag3 = switch_xml_add_child_d(x_tag2, "media", off3++))) {
					abort();
				}
				
				snprintf(tmp, sizeof(tmp), "%uv", np->member->id);
				switch_xml_set_attr_d(x_tag3, "id", tmpp);


				if (!(x_tag4 = switch_xml_add_child_d(x_tag3, "type", off4++))) {
					abort();
				}
				switch_xml_set_txt_d(x_tag4, "video");

				if ((var = switch_channel_get_variable(channel, "rtp_use_video_ssrc"))) {
					if (!(x_tag4 = switch_xml_add_child_d(x_tag3, "src-id", off4++))) {
						abort();
					}
					switch_xml_set_txt_d(x_tag4, var);
				}
				
				if (!(x_tag4 = switch_xml_add_child_d(x_tag3, "status", off4++))) {
					abort();
				}
				switch_xml_set_txt_d(x_tag4, switch_channel_test_flag(channel, CF_HOLD) ? "sendonly" : "sendrecv");
				
			}
		}
		
		switch_safe_free(user_uri);
	}

	switch_mutex_unlock(conference->member_mutex);

	off1 = off2 = off3 = off4 = 0;

	xml_text = switch_xml_toxml(xml, SWITCH_TRUE);
	switch_xml_free(xml);
	
	switch_safe_free(dup_domain);
	switch_safe_free(uri);	

	return xml_text;
}

static void conference_cdr_render(conference_obj_t *conference)
{
	switch_xml_t cdr, x_ptr, x_member, x_members, x_conference, x_cp, x_flags, x_tag, x_rejected, x_attempt;
	conference_cdr_node_t *np;
	conference_cdr_reject_t *rp;
	int cdr_off = 0, conf_off = 0;
	char str[512];
	char *path = NULL, *xml_text;
	int fd;

	if (zstr(conference->log_dir) && (conference->cdr_event_mode == CDRE_NONE)) return;

	if (!conference->cdr_nodes && !conference->cdr_rejected) return;

	if (!(cdr = switch_xml_new("cdr"))) {
		abort();
	}

	if (!(x_conference = switch_xml_add_child_d(cdr, "conference", cdr_off++))) {
		abort();
	}
	
	if (!(x_ptr = switch_xml_add_child_d(x_conference, "name", conf_off++))) {
		abort();
	}
	switch_xml_set_txt_d(x_ptr, conference->name);
	
	if (!(x_ptr = switch_xml_add_child_d(x_conference, "hostname", conf_off++))) {
		abort();
	}
	switch_xml_set_txt_d(x_ptr, switch_core_get_hostname());
	
	if (!(x_ptr = switch_xml_add_child_d(x_conference, "rate", conf_off++))) {
		abort();
	}
	switch_snprintf(str, sizeof(str), "%d", conference->rate);
	switch_xml_set_txt_d(x_ptr, str);
	
	if (!(x_ptr = switch_xml_add_child_d(x_conference, "interval", conf_off++))) {
		abort();
	}
	switch_snprintf(str, sizeof(str), "%d", conference->interval);
	switch_xml_set_txt_d(x_ptr, str);
	
	
	if (!(x_ptr = switch_xml_add_child_d(x_conference, "start_time", conf_off++))) {
		abort();
	}
	switch_xml_set_attr_d(x_ptr, "type", "UNIX-epoch");
	switch_snprintf(str, sizeof(str), "%ld", (long)conference->start_time);
	switch_xml_set_txt_d(x_ptr, str);
	
	
	if (!(x_ptr = switch_xml_add_child_d(x_conference, "end_time", conf_off++))) {
		abort();
	}
	switch_xml_set_attr_d(x_ptr, "endconf_forced", conference_test_flag(conference, CFLAG_ENDCONF_FORCED) ? "true" : "false");
	switch_xml_set_attr_d(x_ptr, "type", "UNIX-epoch");
	switch_snprintf(str, sizeof(str), "%ld", (long)conference->end_time);
	switch_xml_set_txt_d(x_ptr, str);



	if (!(x_members = switch_xml_add_child_d(x_conference, "members", conf_off++))) {
		abort();
	}

	for (np = conference->cdr_nodes; np; np = np->next) {
		int member_off = 0;
		int flag_off = 0;


		if (!(x_member = switch_xml_add_child_d(x_members, "member", conf_off++))) {
			abort();
		}

		switch_xml_set_attr_d(x_member, "type", np->cp ? "caller" : "recording_node");
		
		if (!(x_ptr = switch_xml_add_child_d(x_member, "join_time", member_off++))) {
			abort();
		}
		switch_xml_set_attr_d(x_ptr, "type", "UNIX-epoch");
		switch_snprintf(str, sizeof(str), "%ld", (long) np->join_time);
		switch_xml_set_txt_d(x_ptr, str);


		if (!(x_ptr = switch_xml_add_child_d(x_member, "leave_time", member_off++))) {
			abort();
		}
		switch_xml_set_attr_d(x_ptr, "type", "UNIX-epoch");
		switch_snprintf(str, sizeof(str), "%ld", (long) np->leave_time);
		switch_xml_set_txt_d(x_ptr, str);

		if (np->cp) {
			x_flags = switch_xml_add_child_d(x_member, "flags", member_off++);
			switch_assert(x_flags);

			x_tag = switch_xml_add_child_d(x_flags, "is_moderator", flag_off++);
			switch_xml_set_txt_d(x_tag, cdr_test_mflag(np, MFLAG_MOD) ? "true" : "false");

			x_tag = switch_xml_add_child_d(x_flags, "end_conference", flag_off++);
			switch_xml_set_txt_d(x_tag, cdr_test_mflag(np, MFLAG_ENDCONF) ? "true" : "false");

			x_tag = switch_xml_add_child_d(x_flags, "was_kicked", flag_off++);
			switch_xml_set_txt_d(x_tag, cdr_test_mflag(np, MFLAG_KICKED) ? "true" : "false");

			x_tag = switch_xml_add_child_d(x_flags, "is_ghost", flag_off++);
			switch_xml_set_txt_d(x_tag, cdr_test_mflag(np, MFLAG_GHOST) ? "true" : "false");

			if (!(x_cp = switch_xml_add_child_d(x_member, "caller_profile", member_off++))) {
				abort();
			}
			switch_ivr_set_xml_profile_data(x_cp, np->cp, 0);
		}

		if (!zstr(np->record_path)) {
			if (!(x_ptr = switch_xml_add_child_d(x_member, "record_path", member_off++))) {
				abort();
			}
			switch_xml_set_txt_d(x_ptr, np->record_path);
		}


	}

	if (!(x_rejected = switch_xml_add_child_d(x_conference, "rejected", conf_off++))) {
		abort();
	}

	for (rp = conference->cdr_rejected; rp; rp = rp->next) {
		int attempt_off = 0;
		int tag_off = 0;

		if (!(x_attempt = switch_xml_add_child_d(x_rejected, "attempt", attempt_off++))) {
			abort();
		}

		if (!(x_ptr = switch_xml_add_child_d(x_attempt, "reason", tag_off++))) {
			abort();
		}
		if (rp->reason == CDRR_LOCKED) {
			switch_xml_set_txt_d(x_ptr, "conference_locked");
		} else if (rp->reason == CDRR_MAXMEMBERS) {
			switch_xml_set_txt_d(x_ptr, "max_members_reached");
		} else 	if (rp->reason == CDRR_PIN) {
			switch_xml_set_txt_d(x_ptr, "invalid_pin");
		}

		if (!(x_ptr = switch_xml_add_child_d(x_attempt, "reject_time", tag_off++))) {
			abort();
		}
		switch_xml_set_attr_d(x_ptr, "type", "UNIX-epoch");
		switch_snprintf(str, sizeof(str), "%ld", (long) rp->reject_time);
		switch_xml_set_txt_d(x_ptr, str);

		if (rp->cp) {
			if (!(x_cp = switch_xml_add_child_d(x_attempt, "caller_profile", attempt_off++))) {
				abort();
			}
			switch_ivr_set_xml_profile_data(x_cp, rp->cp, 0);
		}
	}
	
	xml_text = switch_xml_toxml(cdr, SWITCH_TRUE);

	
	if (!zstr(conference->log_dir)) {
		path = switch_mprintf("%s%s%s.cdr.xml", conference->log_dir, SWITCH_PATH_SEPARATOR, conference->uuid_str);
	


#ifdef _MSC_VER
		if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) > -1) {
#else
		if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) > -1) {
#endif
			int wrote;
			wrote = write(fd, xml_text, (unsigned) strlen(xml_text));
			wrote++;
			close(fd);
			fd = -1;
		} else {
			char ebuf[512] = { 0 };
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error writing [%s][%s]\n",
					path, switch_strerror_r(errno, ebuf, sizeof(ebuf)));
		}

		if (conference->cdr_event_mode != CDRE_NONE) {
			switch_event_t *event;

			if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_CDR) == SWITCH_STATUS_SUCCESS)
		//	if (switch_event_create(&event, SWITCH_EVENT_CDR) == SWITCH_STATUS_SUCCESS)
			{
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CDR-Source", CONF_EVENT_CDR);
				if (conference->cdr_event_mode == CDRE_AS_CONTENT) {
					switch_event_set_body(event, xml_text);
				} else {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CDR-Path", path);
				}
				switch_event_fire(&event);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not create CDR event");
			}
		}
	}

   	switch_safe_free(path);
	switch_safe_free(xml_text);
	switch_xml_free(cdr);
}
	
static cJSON *conference_json_render(conference_obj_t *conference, cJSON *req)
{
	char tmp[30];
	const char *domain;	const char *name;
	char *dup_domain = NULL;
	char *uri;
	conference_cdr_node_t *np;
	char *tmpp = tmp;
	cJSON *json = cJSON_CreateObject(), *jusers = NULL, *jold_users = NULL, *juser = NULL, *jvars = NULL;

	switch_assert(json);
	
	switch_mutex_lock(conference->mutex);
	switch_snprintf(tmp, sizeof(tmp), "%u", conference->doc_version);
	conference->doc_version++;
	switch_mutex_unlock(conference->mutex);

	if (!(name = conference->name)) {
		name = "conference";
	}

	if (!(domain = conference->domain)) {
		dup_domain = switch_core_get_domain(SWITCH_TRUE);
		if (!(domain = dup_domain)) {
			domain = "cluecon.com";
		}
	}
	

	uri = switch_mprintf("%s@%s", name, domain);
	json_add_child_string(json, "entity", uri);  
	json_add_child_string(json, "conferenceDescription", conference->desc ? conference->desc : "FreeSWITCH Conference");  
	json_add_child_string(json, "conferenceState", "active");  
	switch_snprintf(tmp, sizeof(tmp), "%u", conference->count);
	json_add_child_string(json, "userCount", tmp);  
	
	jusers = json_add_child_array(json, "users");
	jold_users = json_add_child_array(json, "oldUsers");
	
	switch_mutex_lock(conference->member_mutex);
	
	for (np = conference->cdr_nodes; np; np = np->next) {
		char *user_uri = NULL;
		switch_channel_t *channel = NULL;
		switch_time_exp_t tm;
		switch_size_t retsize;
		const char *fmt = "%Y-%m-%dT%H:%M:%S%z";
		char *p;
		
		if (np->record_path || !np->cp) {
			continue;
		}

		//if (!np->cp || (np->member && !np->member->session) || np->leave_time) { /* for now we'll remove participants when they leave */
		//continue;
		//}

		if (np->member && np->member->session) {
			channel = switch_core_session_get_channel(np->member->session);
		}

		juser = cJSON_CreateObject();

		if (channel) {
			const char *uri = switch_channel_get_variable_dup(channel, "conference_invite_uri", SWITCH_FALSE, -1);

			if (uri) {
				user_uri = strdup(uri);
			}
		}
		
		if (np->cp) {

			if (!user_uri) {
				user_uri = switch_mprintf("%s@%s", np->cp->caller_id_number, domain);
			}
		
			json_add_child_string(juser, "entity", user_uri);
			json_add_child_string(juser, "displayText", np->cp->caller_id_name);
		}

		//if (np->record_path) {
			//json_add_child_string(juser, "recordingPATH", np->record_path);
		//}

		json_add_child_string(juser, "status", np->leave_time ? "disconnected" : "connected");

		switch_time_exp_lt(&tm, (switch_time_t) conference->start_time * 1000000);
		switch_strftime_nocheck(tmp, &retsize, sizeof(tmp), fmt, &tm);			
		p = end_of_p(tmpp) -1;
		snprintf(p, 4, ":00");

		json_add_child_string(juser, "joinTime", tmpp);

		snprintf(tmp, sizeof(tmp), "%u", np->id);
		json_add_child_string(juser, "memberId", tmp);

		jvars = cJSON_CreateObject();

		if (!np->member && np->var_event) {
			switch_json_add_presence_data_cols(np->var_event, jvars, "PD-");
		} else if (np->member) {
			const char *var;
			const char *prefix = NULL;
			switch_event_t *var_event = NULL;
			switch_event_header_t *hp;
			int all = 0;

			switch_channel_get_variables(channel, &var_event);

			if ((prefix = switch_event_get_header(var_event, "json_conf_var_prefix"))) {
				all = strcasecmp(prefix, "__all__");
			} else {
				prefix = "json_";
			}

			for(hp = var_event->headers; hp; hp = hp->next) {
				if (all || !strncasecmp(hp->name, prefix, strlen(prefix))) {
					json_add_child_string(jvars, hp->name, hp->value);
				}
			}
			
			switch_json_add_presence_data_cols(var_event, jvars, "PD-");

			switch_event_destroy(&var_event);

			if ((var = switch_channel_get_variable(channel, "rtp_use_ssrc"))) {
				json_add_child_string(juser, "rtpAudioSSRC", var);
			}
			
			json_add_child_string(juser, "rtpAudioDirection", audio_flow(np->member));
			
			
			if (switch_channel_test_flag(channel, CF_VIDEO)) {
				if ((var = switch_channel_get_variable(channel, "rtp_use_video_ssrc"))) {
					json_add_child_string(juser, "rtpVideoSSRC", var);
				}
				
				json_add_child_string(juser, "rtpVideoDirection", switch_channel_test_flag(channel, CF_HOLD) ? "sendonly" : "sendrecv");
			}
		}

		if (jvars) {
			json_add_child_obj(juser, "variables", jvars);
		}

		cJSON_AddItemToArray(np->leave_time ? jold_users : jusers, juser);
	
		switch_safe_free(user_uri);
	}

	switch_mutex_unlock(conference->member_mutex);

	switch_safe_free(dup_domain);
	switch_safe_free(uri);	

	return json;
}

static void conference_mod_event_channel_handler(const char *event_channel, cJSON *json, const char *key, switch_event_channel_id_t id)
{
	cJSON *data, *addobj = NULL; 
	const char *action = NULL;
	char *value = NULL;
	cJSON *jid = 0;
	char *conf_name = strdup(event_channel + 15);
	int cid = 0;
	char *p;
	switch_stream_handle_t stream = { 0 };
	char *exec = NULL;
	cJSON *msg, *jdata, *jvalue;
	char *argv[10] = {0};
	int argc = 0;

	if (conf_name && (p = strchr(conf_name, '@'))) {
		*p = '\0';
	}

	if ((data = cJSON_GetObjectItem(json, "data"))) {
		action = cJSON_GetObjectCstr(data, "command");
		if ((jid = cJSON_GetObjectItem(data, "id"))) {
			cid = jid->valueint;
		}

		if ((jvalue = cJSON_GetObjectItem(data, "value"))) {

			if (jvalue->type == cJSON_Array) {
				int i;
				argc = cJSON_GetArraySize(jvalue);
				if (argc > 10) argc = 10;

				for (i = 0; i < argc; i++) {
					cJSON *str = cJSON_GetArrayItem(jvalue, i);
					if (str->type == cJSON_String) {
						argv[i] = str->valuestring;
					}
				}
			} else if (jvalue->type == cJSON_String) { 
				value = jvalue->valuestring;
				argv[argc++] = value;
			}
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "conf %s CMD %s [%s] %d\n", conf_name, key, action, cid);

	if (zstr(action)) {
		goto end;
	}

	SWITCH_STANDARD_STREAM(stream);
	
	if (!strcasecmp(action, "kick") || 
		!strcasecmp(action, "mute") || 
		!strcasecmp(action, "unmute") || 
		!strcasecmp(action, "tmute") ||
		!strcasecmp(action, "vmute") || 
		!strcasecmp(action, "unvmute") || 
		!strcasecmp(action, "tvmute")  
		) {
		exec = switch_mprintf("%s %s %d", conf_name, action, cid);
	} else if (!strcasecmp(action, "volume_in") || 
			   !strcasecmp(action, "volume_out") || 
			   !strcasecmp(action, "vid-res-id") || 
			   !strcasecmp(action, "vid-floor") || 
			   !strcasecmp(action, "vid-layer") ||
			   !strcasecmp(action, "vid-canvas") ||
			   !strcasecmp(action, "vid-watching-canvas") ||
			   !strcasecmp(action, "vid-banner")) {
		exec = switch_mprintf("%s %s %d %s", conf_name, action, cid, argv[0]);
	} else if (!strcasecmp(action, "play") || !strcasecmp(action, "stop")) {
		exec = switch_mprintf("%s %s %s", conf_name, action, argv[0]);
	} else if (!strcasecmp(action, "recording") || !strcasecmp(action, "vid-layout") || !strcasecmp(action, "vid-write-png")) {

		if (!argv[1]) {
			argv[1] = "all";
		}

		exec = switch_mprintf("%s %s %s %s", conf_name, action, argv[0], argv[1]);

	} else if (!strcasecmp(action, "transfer") && cid) {
		conference_member_t *member;
		conference_obj_t *conference;

		exec = switch_mprintf("%s %s %s", argv[0], switch_str_nil(argv[1]), switch_str_nil(argv[2]));
		stream.write_function(&stream, "+OK Call transferred to %s", argv[0]);

		if ((conference = conference_find(conf_name, NULL))) {
			if ((member = conference_member_get(conference, cid))) {
				switch_ivr_session_transfer(member->session, argv[0], argv[1], argv[2]);
				switch_thread_rwlock_unlock(member->rwlock);
			}
			switch_thread_rwlock_unlock(conference->rwlock); 
		}
		goto end;
	} else if (!strcasecmp(action, "list-videoLayouts")) {
		switch_hash_index_t *hi;
		void *val;
		const void *vvar;
		cJSON *array = cJSON_CreateArray();
		conference_obj_t *conference = NULL;
		if ((conference = conference_find(conf_name, NULL))) {
			switch_mutex_lock(globals.setup_mutex);
			if (conference->layout_hash) {
				for (hi = switch_core_hash_first(conference->layout_hash); hi; hi = switch_core_hash_next(&hi)) {
					switch_core_hash_this(hi, &vvar, NULL, &val);
					cJSON_AddItemToArray(array, cJSON_CreateString((char *)vvar));
				}
			}
			switch_mutex_unlock(globals.setup_mutex);
			switch_thread_rwlock_unlock(conference->rwlock);
		}
		addobj = array;
	}

	if (exec) {
		conf_api_main(exec, NULL, &stream);
	}

 end:

	msg = cJSON_CreateObject();
	jdata = json_add_child_obj(msg, "data", NULL);

	cJSON_AddItemToObject(msg, "eventChannel", cJSON_CreateString(event_channel));
	cJSON_AddItemToObject(jdata, "action", cJSON_CreateString("response"));
	
	if (addobj) {
		cJSON_AddItemToObject(jdata, "conf-command", cJSON_CreateString(action));
		cJSON_AddItemToObject(jdata, "response", cJSON_CreateString("OK"));
		cJSON_AddItemToObject(jdata, "responseData", addobj);
	} else if (exec) {
		cJSON_AddItemToObject(jdata, "conf-command", cJSON_CreateString(exec));
		cJSON_AddItemToObject(jdata, "response", cJSON_CreateString((char *)stream.data));
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ALERT,"RES [%s][%s]\n", exec, (char *)stream.data);
	} else {
		cJSON_AddItemToObject(jdata, "error", cJSON_CreateString("Invalid Command"));
	}

	switch_event_channel_broadcast(event_channel, &msg, __FILE__, globals.event_channel_id);


	switch_safe_free(stream.data);
	switch_safe_free(exec);

	switch_safe_free(conf_name);

}

static void conference_la_event_channel_handler(const char *event_channel, cJSON *json, const char *key, switch_event_channel_id_t id)
{
	switch_live_array_parse_json(json, globals.event_channel_id);
}

static void conference_event_channel_handler(const char *event_channel, cJSON *json, const char *key, switch_event_channel_id_t id)
{
	char *domain = NULL, *name = NULL;
	conference_obj_t *conference = NULL;
	cJSON *data, *reply = NULL, *conf_desc = NULL;
	const char *action = NULL;
	char *dup = NULL;

	if ((data = cJSON_GetObjectItem(json, "data"))) {
		action = cJSON_GetObjectCstr(data, "action");
	}

	if (!action) action = "";

	reply = cJSON_Duplicate(json, 1);
	cJSON_DeleteItemFromObject(reply, "data");

	if ((name = strchr(event_channel, '.'))) {
		dup = strdup(name + 1);
		switch_assert(dup);
		name = dup;

		if ((domain = strchr(name, '@'))) {
			*domain++ = '\0';
		}
	}
	
	if (!strcasecmp(action, "bootstrap")) {
		if (!zstr(name) && (conference = conference_find(name, domain))) { 
			conf_desc = conference_json_render(conference, json);
		} else {
			conf_desc = cJSON_CreateObject();
			json_add_child_string(conf_desc, "conferenceDescription", "FreeSWITCH Conference");
			json_add_child_string(conf_desc, "conferenceState", "inactive");
			json_add_child_array(conf_desc, "users");
			json_add_child_array(conf_desc, "oldUsers");
		}
	} else {
		conf_desc = cJSON_CreateObject();
		json_add_child_string(conf_desc, "error", "Invalid action");
	}

	json_add_child_string(conf_desc, "action", "conferenceDescription");
	
	cJSON_AddItemToObject(reply, "data", conf_desc);

	switch_safe_free(dup);

	switch_event_channel_broadcast(event_channel, &reply, modname, globals.event_channel_id);
}


static switch_status_t conference_add_event_data(conference_obj_t *conference, switch_event_t *event)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Conference-Name", conference->name);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Conference-Size", "%u", conference->count);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Conference-Ghosts", "%u", conference->count_ghosts);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Conference-Profile-Name", conference->profile_name);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Conference-Unique-ID", conference->uuid_str);

	return status;
}

static switch_status_t conference_add_event_member_data(conference_member_t *member, switch_event_t *event)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!member)
		return status;

	if (member->conference) {
		status = conference_add_event_data(member->conference, event);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Floor", "%s", (member == member->conference->floor_holder) ? "true" : "false" );
	}

	if (member->session) {
		switch_channel_t *channel = switch_core_session_get_channel(member->session);

		if (member->verbose_events) {
			switch_channel_event_set_data(channel, event);
		} else {
			switch_channel_event_set_basic_data(channel, event);
		}
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Video", "%s",
								switch_channel_test_flag(switch_core_session_get_channel(member->session), CF_VIDEO) ? "true" : "false" );

	}

	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Hear", "%s", member_test_flag(member, MFLAG_CAN_HEAR) ? "true" : "false" );
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "See", "%s", member_test_flag(member, MFLAG_CAN_BE_SEEN) ? "true" : "false" );
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Speak", "%s", member_test_flag(member, MFLAG_CAN_SPEAK) ? "true" : "false" );
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Talking", "%s", member_test_flag(member, MFLAG_TALKING) ? "true" : "false" );
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Mute-Detect", "%s", member_test_flag(member, MFLAG_MUTE_DETECT) ? "true" : "false" );
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Member-ID", "%u", member->id);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Member-Type", "%s", member_test_flag(member, MFLAG_MOD) ? "moderator" : "member");
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Member-Ghost", "%s", member_test_flag(member, MFLAG_GHOST) ? "true" : "false");
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Energy-Level", "%d", member->energy_level);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Current-Energy", "%d", member->score);

	return status;
}

/* Return a Distinct ID # */
static uint32_t next_member_id(void)
{
	uint32_t id;

	switch_mutex_lock(globals.id_mutex);
	id = ++globals.id_pool;
	switch_mutex_unlock(globals.id_mutex);

	return id;
}

/* if other_member has a relationship with member, produce it */
static conference_relationship_t *member_get_relationship(conference_member_t *member, conference_member_t *other_member)
{
	conference_relationship_t *rel = NULL, *global = NULL;

	if (member == NULL || other_member == NULL || member->relationships == NULL)
		return NULL;

	lock_member(member);
	lock_member(other_member);

	for (rel = member->relationships; rel; rel = rel->next) {
		if (rel->id == other_member->id) {
			break;
		}

		/* 0 matches everyone. (We will still test the others because a real match carries more clout) */
		if (rel->id == 0) {
			global = rel;
		}
	}

	unlock_member(other_member);
	unlock_member(member);

	return rel ? rel : global;
}

/* traverse the conference member list for the specified member id and return it's pointer */
static conference_member_t *conference_member_get(conference_obj_t *conference, uint32_t id)
{
	conference_member_t *member = NULL;

	switch_assert(conference != NULL);
	if (!id) {
		return NULL;
	}

	switch_mutex_lock(conference->member_mutex);
	for (member = conference->members; member; member = member->next) {

		if (member_test_flag(member, MFLAG_NOCHANNEL)) {
			continue;
		}

		if (member->id == id) {
			break;
		}
	}

	if (member) {
		if (!member_test_flag(member, MFLAG_INTREE) || 
			member_test_flag(member, MFLAG_KICKED) || 
			(member->session && !switch_channel_up(switch_core_session_get_channel(member->session)))) {

			/* member is kicked or hanging up so forget it */
			member = NULL;
		}
	}

	if (member) {
		if (switch_thread_rwlock_tryrdlock(member->rwlock) != SWITCH_STATUS_SUCCESS) {
			/* if you cant readlock it's way to late to do anything */
			member = NULL;
		}
	}

	switch_mutex_unlock(conference->member_mutex);

	return member;
}

/* stop the specified recording */
static switch_status_t conference_record_stop(conference_obj_t *conference, switch_stream_handle_t *stream, char *path)
{
	conference_member_t *member = NULL;
	int count = 0;

	switch_assert(conference != NULL);
	switch_mutex_lock(conference->member_mutex);
	for (member = conference->members; member; member = member->next) {
		if (member_test_flag(member, MFLAG_NOCHANNEL) && (!path || !strcmp(path, member->rec_path))) {
			if (!conference_test_flag(conference, CFLAG_CONF_RESTART_AUTO_RECORD) && member->rec && member->rec->autorec) {
				stream->write_function(stream, "Stopped AUTO recording file %s (Auto Recording Now Disabled)\n", member->rec_path);
				conference->auto_record = 0;
			} else {
				stream->write_function(stream, "Stopped recording file %s\n", member->rec_path);
			}

			member_clear_flag_locked(member, MFLAG_RUNNING);
			count++;

		}
	}

	conference->record_count -= count;

	switch_mutex_unlock(conference->member_mutex);
	return count;
}
/* stop/pause/resume the specified recording */
static switch_status_t conference_record_action(conference_obj_t *conference, char *path, recording_action_type_t action)
{
	conference_member_t *member = NULL;
	int count = 0;
	//switch_file_handle_t *fh = NULL;

	switch_assert(conference != NULL);
	switch_mutex_lock(conference->member_mutex);
	for (member = conference->members; member; member = member->next)
	{
		if (member_test_flag(member, MFLAG_NOCHANNEL) && (!path || !strcmp(path, member->rec_path)))
		{
			//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,	"Action: %d\n", action);
			switch (action)
			{
				case REC_ACTION_STOP:
						member_clear_flag_locked(member, MFLAG_RUNNING);
						count++;
						break;
				case REC_ACTION_PAUSE:
						member_set_flag_locked(member, MFLAG_PAUSE_RECORDING);
						count = 1;
						break;
				case REC_ACTION_RESUME:
						member_clear_flag_locked(member, MFLAG_PAUSE_RECORDING);
						count = 1;
						break;
					}
				}
			}
	switch_mutex_unlock(conference->member_mutex);
	return count;
}


/* Add a custom relationship to a member */
static conference_relationship_t *member_add_relationship(conference_member_t *member, uint32_t id)
{
	conference_relationship_t *rel = NULL;

	if (member == NULL || id == 0 || !(rel = switch_core_alloc(member->pool, sizeof(*rel))))
		return NULL;

	rel->id = id;


	lock_member(member);
	switch_mutex_lock(member->conference->member_mutex);
	member->conference->relationship_total++;
	switch_mutex_unlock(member->conference->member_mutex);
	rel->next = member->relationships;
	member->relationships = rel;
	unlock_member(member);

	return rel;
}

/* Remove a custom relationship from a member */
static switch_status_t member_del_relationship(conference_member_t *member, uint32_t id)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	conference_relationship_t *rel, *last = NULL;

	if (member == NULL)
		return status;

	lock_member(member);
	for (rel = member->relationships; rel; rel = rel->next) {
		if (id == 0 || rel->id == id) {
			/* we just forget about rel here cos it was allocated by the member's pool 
			   it will be freed when the member is */
			conference_member_t *omember;


			status = SWITCH_STATUS_SUCCESS;
			if (last) {
				last->next = rel->next;
			} else {
				member->relationships = rel->next;
			}

			if ((rel->flags & RFLAG_CAN_SEND_VIDEO)) {
				member_clear_flag(member, MFLAG_RECEIVING_VIDEO);
				if ((omember = conference_member_get(member->conference, rel->id))) {
					member_clear_flag(omember, MFLAG_RECEIVING_VIDEO);
					switch_thread_rwlock_unlock(omember->rwlock);
				}
			}

			switch_mutex_lock(member->conference->member_mutex);
			member->conference->relationship_total--;
			switch_mutex_unlock(member->conference->member_mutex);

			continue;
		}

		last = rel;
	}
	unlock_member(member);

	return status;
}

static void send_json_event(conference_obj_t *conference)
{
	cJSON *event, *conf_desc = NULL;
	char *name = NULL, *domain = NULL, *dup_domain = NULL;
	char *event_channel = NULL;

	if (!conference_test_flag(conference, CFLAG_JSON_EVENTS)) {
		return;
	}

	conf_desc = conference_json_render(conference, NULL);

	if (!(name = conference->name)) {
		name = "conference";
	}

	if (!(domain = conference->domain)) {
		dup_domain = switch_core_get_domain(SWITCH_TRUE);
		if (!(domain = dup_domain)) {
			domain = "cluecon.com";
		}
	}

	event_channel = switch_mprintf("conference.%q@%q", name, domain);

	event = cJSON_CreateObject();

	json_add_child_string(event, "eventChannel", event_channel);  
	cJSON_AddItemToObject(event, "data", conf_desc);
	
	switch_event_channel_broadcast(event_channel, &event, modname, globals.event_channel_id);

	switch_safe_free(dup_domain);
	switch_safe_free(event_channel);
}

static void send_rfc_event(conference_obj_t *conference)
{
	switch_event_t *event;
	char *body;
	char *name = NULL, *domain = NULL, *dup_domain = NULL;
	
	if (!conference_test_flag(conference, CFLAG_RFC4579)) {
		return;
	}

	if (!(name = conference->name)) {
		name = "conference";
	}

	if (!(domain = conference->domain)) {
		dup_domain = switch_core_get_domain(SWITCH_TRUE);
		if (!(domain = dup_domain)) {
			domain = "cluecon.com";
		}
	}


	if (switch_event_create(&event, SWITCH_EVENT_CONFERENCE_DATA) == SWITCH_STATUS_SUCCESS) {
		event->flags |= EF_UNIQ_HEADERS;

		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "conference-name", name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "conference-domain", domain);

		body = conference_rfc4579_render(conference, NULL, event);
		switch_event_add_body(event, "%s", body);
		free(body);
		switch_event_fire(&event);
	}

	switch_safe_free(dup_domain);

}



static void send_conference_notify(conference_obj_t *conference, const char *status, const char *call_id, switch_bool_t final)
{
	switch_event_t *event;
	char *name = NULL, *domain = NULL, *dup_domain = NULL;
	
	if (!conference_test_flag(conference, CFLAG_RFC4579)) {
		return;
	}

	if (!(name = conference->name)) {
		name = "conference";
	}

	if (!(domain = conference->domain)) {
		dup_domain = switch_core_get_domain(SWITCH_TRUE);
		if (!(domain = dup_domain)) {
			domain = "cluecon.com";
		}
	}


	if (switch_event_create(&event, SWITCH_EVENT_CONFERENCE_DATA) == SWITCH_STATUS_SUCCESS) {
		event->flags |= EF_UNIQ_HEADERS;

		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "conference-name", name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "conference-domain", domain);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "conference-event", "refer");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call_id", call_id);

		if (final) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "final", "true");
		}


		switch_event_add_body(event, "%s", status);
		switch_event_fire(&event);
	}

	switch_safe_free(dup_domain);

}


static void member_update_status_field(conference_member_t *member)
{
	char *str, *vstr = "", display[128] = "", *json_display = NULL;
	cJSON *json, *audio, *video;

	if (!member->conference->la || !member->json || 
		!member->status_field || switch_channel_test_flag(member->channel, CF_VIDEO_ONLY) || member_test_flag(member, MFLAG_SECOND_SCREEN)) {
		return;
	}

	switch_live_array_lock(member->conference->la);

	if (conference_test_flag(member->conference, CFLAG_JSON_STATUS)) {
		json = cJSON_CreateObject();
		audio = cJSON_CreateObject();
		cJSON_AddItemToObject(audio, "muted", cJSON_CreateBool(!member_test_flag(member, MFLAG_CAN_SPEAK))); 
		cJSON_AddItemToObject(audio, "onHold", cJSON_CreateBool(switch_channel_test_flag(member->channel, CF_HOLD))); 
		cJSON_AddItemToObject(audio, "talking", cJSON_CreateBool(member_test_flag(member, MFLAG_TALKING)));
		cJSON_AddItemToObject(audio, "floor", cJSON_CreateBool(member == member->conference->floor_holder));
		cJSON_AddItemToObject(audio, "energyScore", cJSON_CreateNumber(member->score));
		cJSON_AddItemToObject(json, "audio", audio);
		
		if (switch_channel_test_flag(member->channel, CF_VIDEO) || member->avatar_png_img) {
			video = cJSON_CreateObject();
			cJSON_AddItemToObject(video, "avatarPresented", cJSON_CreateBool(!!member->avatar_png_img));
			cJSON_AddItemToObject(video, "mediaFlow", cJSON_CreateString(member->video_flow == SWITCH_MEDIA_FLOW_SENDONLY ? "sendOnly" : "sendRecv"));
			cJSON_AddItemToObject(video, "muted", cJSON_CreateBool(!member_test_flag(member, MFLAG_CAN_BE_SEEN)));
			cJSON_AddItemToObject(video, "floor", cJSON_CreateBool(member && member->id == member->conference->video_floor_holder));
			if (member && member->id == member->conference->video_floor_holder && conference_test_flag(member->conference, CFLAG_VID_FLOOR_LOCK)) {
				cJSON_AddItemToObject(video, "floorLocked", cJSON_CreateTrue());
			}
			cJSON_AddItemToObject(video, "reservationID", member->video_reservation_id ? 
								  cJSON_CreateString(member->video_reservation_id) : cJSON_CreateNull());

			cJSON_AddItemToObject(video, "videoLayerID", cJSON_CreateNumber(member->video_layer_id));
			
			cJSON_AddItemToObject(json, "video", video);
		} else {
			cJSON_AddItemToObject(json, "video", cJSON_CreateFalse());
		}

		json_display = cJSON_PrintUnformatted(json);
		cJSON_Delete(json);
	} else {
		if (!member_test_flag(member, MFLAG_CAN_SPEAK)) {
			str = "MUTE";
		} else if (switch_channel_test_flag(member->channel, CF_HOLD)) {
			str = "HOLD";
		} else if (member == member->conference->floor_holder) {
			if (member_test_flag(member, MFLAG_TALKING)) {
				str = "TALKING (FLOOR)";
			} else {
				str = "FLOOR";
			}
		} else if (member_test_flag(member, MFLAG_TALKING)) {
			str = "TALKING";
		} else {
			str = "ACTIVE";
		}
	
		if (switch_channel_test_flag(member->channel, CF_VIDEO)) {
			if (!member_test_flag(member, MFLAG_CAN_BE_SEEN)) {
				vstr = " VIDEO (BLIND)";
			} else {
				vstr = " VIDEO";
				if (member && member->id == member->conference->video_floor_holder) {
					vstr = " VIDEO (FLOOR)";
				}
			}
		}

		switch_snprintf(display, sizeof(display), "%s%s", str, vstr);
	}

	if (json_display) {
		member->status_field->valuestring = json_display;
	} else {
		free(member->status_field->valuestring);
		member->status_field->valuestring = strdup(display);
	}

	switch_live_array_add(member->conference->la, switch_core_session_get_uuid(member->session), -1, &member->json, SWITCH_FALSE);
	switch_live_array_unlock(member->conference->la);
}

static void adv_la(conference_obj_t *conference, conference_member_t *member, switch_bool_t join)
{

	//if (member->video_flow == SWITCH_MEDIA_FLOW_SENDONLY) {
	switch_channel_set_flag(member->channel, CF_VIDEO_REFRESH_REQ);
	switch_core_media_gen_key_frame(member->session);
		//}

	if (conference && conference->la && member->session && 
		!switch_channel_test_flag(member->channel, CF_VIDEO_ONLY)) {
		cJSON *msg, *data;
		const char *uuid = switch_core_session_get_uuid(member->session);
		const char *cookie = switch_channel_get_variable(member->channel, "event_channel_cookie");
		const char *event_channel = cookie ? cookie : uuid;
		switch_event_t *variables;
		switch_event_header_t *hp;

		msg = cJSON_CreateObject();
		data = json_add_child_obj(msg, "pvtData", NULL);

		cJSON_AddItemToObject(msg, "eventChannel", cJSON_CreateString(event_channel));
		cJSON_AddItemToObject(msg, "eventType", cJSON_CreateString("channelPvtData"));

		cJSON_AddItemToObject(data, "action", cJSON_CreateString(join ? "conference-liveArray-join" : "conference-liveArray-part"));
		cJSON_AddItemToObject(data, "laChannel", cJSON_CreateString(conference->la_event_channel));
		cJSON_AddItemToObject(data, "laName", cJSON_CreateString(conference->la_name));
		cJSON_AddItemToObject(data, "role", cJSON_CreateString(member_test_flag(member, MFLAG_MOD) ? "moderator" : "participant"));
		cJSON_AddItemToObject(data, "chatID", cJSON_CreateString(conference->chat_id));
		cJSON_AddItemToObject(data, "canvasCount", cJSON_CreateNumber(conference->canvas_count));
		
		if (member_test_flag(member, MFLAG_SECOND_SCREEN)) {
			cJSON_AddItemToObject(data, "secondScreen", cJSON_CreateTrue());
		}
		
		if (member_test_flag(member, MFLAG_MOD)) {
			cJSON_AddItemToObject(data, "modChannel", cJSON_CreateString(conference->mod_event_channel));
		}
		
		switch_core_get_variables(&variables); 
		for (hp = variables->headers; hp; hp = hp->next) {
			if (!strncasecmp(hp->name, "conf_verto_", 11)) {
				char *var = hp->name + 11;
				if (var) {
					cJSON_AddItemToObject(data, var, cJSON_CreateString(hp->value));
				}
			}
		}
		switch_event_destroy(&variables);

		switch_event_channel_broadcast(event_channel, &msg, modname, globals.event_channel_id);

		if (cookie) {
			switch_event_channel_permission_modify(cookie, conference->la_event_channel, join);
			switch_event_channel_permission_modify(cookie, conference->mod_event_channel, join);
		}
	}
}

#ifndef OPENAL_POSITIONING
static switch_status_t parse_position(al_handle_t *al, const char *data) 
{
	return SWITCH_STATUS_FALSE;
}

#else 
static switch_status_t parse_position(al_handle_t *al, const char *data) 
{
	char *args[3];
	int num;
	char *dup;
	

	dup = strdup((char *)data);
	switch_assert(dup);
			
	if ((num = switch_split(dup, ':', args)) != 3) {
		return SWITCH_STATUS_FALSE;
	}

	al->pos_x = atof(args[0]);
	al->pos_y = atof(args[1]);
	al->pos_z = atof(args[2]);
	al->setpos = 1;

	switch_safe_free(dup);

	return SWITCH_STATUS_SUCCESS;
}
#endif

#ifndef OPENAL_POSITIONING
static switch_status_t member_parse_position(conference_member_t *member, const char *data)
{
	return SWITCH_STATUS_FALSE;
}
#else
static switch_status_t member_parse_position(conference_member_t *member, const char *data)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (member->al) {
		status = parse_position(member->al, data);
	}

	return status;
	
}
#endif

static void find_video_floor(conference_member_t *member, switch_bool_t entering)
{
	conference_member_t *imember;
	conference_obj_t *conference = member->conference;

	if (!entering) {
		if (member->id == conference->video_floor_holder) {
			conference_set_video_floor_holder(conference, NULL, SWITCH_FALSE);
		} else if (member->id == conference->last_video_floor_holder) {
			conference->last_video_floor_holder = 0;
		}
	}

	switch_mutex_lock(conference->member_mutex);	
	for (imember = conference->members; imember; imember = imember->next) {

		if (!(imember->session)) {
			continue;
		}

		if (imember->video_flow == SWITCH_MEDIA_FLOW_SENDONLY && !imember->avatar_png_img) {
			continue;
		}

		if (!switch_channel_test_flag(imember->channel, CF_VIDEO) && !imember->avatar_png_img) {
			continue;
		}

		if (!entering && imember->id == member->id) {
			continue;
		}

		if (conference->floor_holder && imember == conference->floor_holder) {
			conference_set_video_floor_holder(conference, imember, 0);
			continue;
		}

		if (!conference->video_floor_holder) {
			conference_set_video_floor_holder(conference, imember, 0);
			continue;
		}

		if (!conference->last_video_floor_holder) {
			conference->last_video_floor_holder = imember->id;
			switch_core_session_request_video_refresh(imember->session);
			continue;
		}

	}
	switch_mutex_unlock(conference->member_mutex);	

	if (conference->last_video_floor_holder == conference->video_floor_holder) {
		conference->last_video_floor_holder = 0;
	}
}

static void reset_member_codec_index(conference_member_t *member)
{
	member->video_codec_index = -1;
}


/* Gain exclusive access and add the member to the list */
static switch_status_t conference_add_member(conference_obj_t *conference, conference_member_t *member)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_event_t *event;
	char msg[512];				/* conference count announcement */
	call_list_t *call_list = NULL;
	switch_channel_t *channel;
	const char *controls = NULL, *position = NULL, *var = NULL;


	switch_assert(conference != NULL);
	switch_assert(member != NULL);

	switch_mutex_lock(conference->mutex);
	switch_mutex_lock(member->audio_in_mutex);
	switch_mutex_lock(member->audio_out_mutex);
	lock_member(member);
	switch_mutex_lock(conference->member_mutex);

	if (member->rec) {
		conference->recording_members++;
	}

	member->join_time = switch_epoch_time_now(NULL);
	member->conference = conference;
	member->next = conference->members;
	member->energy_level = conference->energy_level;
	member->score_iir = 0;
	member->verbose_events = conference->verbose_events;
	member->video_layer_id = -1;


	switch_queue_create(&member->dtmf_queue, 100, member->pool);

	if (conference_test_flag(conference, CFLAG_PERSONAL_CANVAS)) {
		video_layout_t *vlayout = NULL;

		switch_mutex_lock(conference->canvas_mutex);
		if ((vlayout = get_layout(conference, conference->video_layout_name, conference->video_layout_group))) {
			init_canvas(conference, vlayout, &member->canvas);
			init_canvas_layers(conference, member->canvas, vlayout);
		}
		switch_mutex_unlock(conference->canvas_mutex);
	}

	if (member->video_flow == SWITCH_MEDIA_FLOW_SENDONLY) {
		member_clear_flag_locked(member, MFLAG_CAN_BE_SEEN);
	}

	conference->members = member;
	member_set_flag_locked(member, MFLAG_INTREE);
	switch_mutex_unlock(conference->member_mutex);
	conference_cdr_add(member);

	
	if (!member_test_flag(member, MFLAG_NOCHANNEL)) {
		if (member_test_flag(member, MFLAG_GHOST)) {
			conference->count_ghosts++;
		} else {
			conference->count++;
		}

		if (member_test_flag(member, MFLAG_ENDCONF)) {
			if (conference->end_count++) {
				conference->endconf_time = 0;
			}
		}

		conference_send_presence(conference);

		channel = switch_core_session_get_channel(member->session);
		member->video_flow = switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO);

		check_avatar(member, SWITCH_FALSE);

		if ((var = switch_channel_get_variable_dup(member->channel, "video_initial_canvas", SWITCH_FALSE, -1))) {
			int id = atoi(var) - 1;
			if (id < conference->canvas_count) {
				member->canvas_id = id;
			}
		}

		if ((var = switch_channel_get_variable_dup(member->channel, "video_initial_watching_canvas", SWITCH_FALSE, -1))) {
			int id = atoi(var) - 1;

			if (id == 0) {
				id = conference->canvas_count;
			}

			if (id <= conference->canvas_count && conference->canvases[id]) {
				member->watching_canvas_id = id;
			}
		}

		reset_member_codec_index(member);

		if ((var = switch_channel_get_variable_dup(member->channel, "video_mute_png", SWITCH_FALSE, -1))) {
			member->video_mute_png = switch_core_strdup(member->pool, var);
			member->video_mute_img = switch_img_read_png(member->video_mute_png, SWITCH_IMG_FMT_I420);
		}

		if ((var = switch_channel_get_variable_dup(member->channel, "video_reservation_id", SWITCH_FALSE, -1))) {
			member->video_reservation_id = switch_core_strdup(member->pool, var);
		}

		if ((var = switch_channel_get_variable(channel, "video_use_dedicated_encoder")) && switch_true(var)) {
			member_set_flag_locked(member, MFLAG_NO_MINIMIZE_ENCODING);
		}

		switch_channel_set_variable_printf(channel, "conference_member_id", "%d", member->id);
		switch_channel_set_variable_printf(channel, "conference_moderator", "%s", member_test_flag(member, MFLAG_MOD) ? "true" : "false");
		switch_channel_set_variable_printf(channel, "conference_ghost", "%s", member_test_flag(member, MFLAG_GHOST) ? "true" : "false");
		switch_channel_set_variable(channel, "conference_recording", conference->record_filename);
		switch_channel_set_variable(channel, CONFERENCE_UUID_VARIABLE, conference->uuid_str);

		if (switch_channel_test_flag(channel, CF_VIDEO)) {
			/* Tell the channel to request a fresh vid frame */
			switch_core_session_video_reinit(member->session);
		}

		if (!switch_channel_get_variable(channel, "conference_call_key")) {
			char *key = switch_core_session_sprintf(member->session, "conf_%s_%s_%s", 
													conference->name, conference->domain, switch_channel_get_variable(channel, "caller_id_number"));
			switch_channel_set_variable(channel, "conference_call_key", key);
		}


		if (conference_test_flag(conference, CFLAG_WAIT_MOD) && member_test_flag(member, MFLAG_MOD)) {
			conference_clear_flag(conference, CFLAG_WAIT_MOD);
		}

		if (conference->count > 1) {
			if ((conference->moh_sound && !conference_test_flag(conference, CFLAG_WAIT_MOD)) ||
					(conference_test_flag(conference, CFLAG_WAIT_MOD) && !switch_true(switch_channel_get_variable(channel, "conference_permanent_wait_mod_moh")))) {
				/* stop MoH if any */
				conference_stop_file(conference, FILE_STOP_ASYNC);
			}

			if (!switch_channel_test_app_flag_key("conf_silent", channel, CONF_SILENT_REQ) && !zstr(conference->enter_sound)) {
				const char * enter_sound = switch_channel_get_variable(channel, "conference_enter_sound");
				if (conference_test_flag(conference, CFLAG_ENTER_SOUND) && !member_test_flag(member, MFLAG_SILENT)) {
					if (!zstr(enter_sound)) {
				     	conference_play_file(conference, (char *)enter_sound, CONF_DEFAULT_LEADIN,
							switch_core_session_get_channel(member->session), 0);
			        } else {
						conference_play_file(conference, conference->enter_sound, CONF_DEFAULT_LEADIN, switch_core_session_get_channel(member->session), 0);
					}
				}
			}
		}


		call_list = (call_list_t *) switch_channel_get_private(channel, "_conference_autocall_list_");

		if (call_list) {
			char saymsg[1024];
			switch_snprintf(saymsg, sizeof(saymsg), "Auto Calling %d parties", call_list->iteration);
			conference_member_say(member, saymsg, 0);
		} else {

			if (!switch_channel_test_app_flag_key("conf_silent", channel, CONF_SILENT_REQ)) {
				/* announce the total number of members in the conference */
				if (conference->count >= conference->announce_count && conference->announce_count > 1) {
					switch_snprintf(msg, sizeof(msg), "There are %d callers", conference->count);
					conference_member_say(member, msg, CONF_DEFAULT_LEADIN);
				} else if (conference->count == 1 && !conference->perpetual_sound && !conference_test_flag(conference, CFLAG_WAIT_MOD)) {
					/* as long as its not a bridge_to conference, announce if person is alone */
					if (!conference_test_flag(conference, CFLAG_BRIDGE_TO)) {
						if (conference->alone_sound  && !member_test_flag(member, MFLAG_GHOST)) {
							conference_stop_file(conference, FILE_STOP_ASYNC);
							conference_play_file(conference, conference->alone_sound, CONF_DEFAULT_LEADIN,
												 switch_core_session_get_channel(member->session), 0);
						} else {
							switch_snprintf(msg, sizeof(msg), "You are currently the only person in this conference.");
							conference_member_say(member, msg, CONF_DEFAULT_LEADIN);
						}
					}
				}
			}
		}

		if (conference->min && conference->count >= conference->min) {
			conference_set_flag(conference, CFLAG_ENFORCE_MIN);
		}

		if (!switch_channel_test_app_flag_key("conf_silent", channel, CONF_SILENT_REQ) &&
			switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_add_event_member_data(member, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "add-member");
			switch_event_fire(&event);
		}

		switch_channel_clear_app_flag_key("conf_silent", channel, CONF_SILENT_REQ);
		switch_channel_set_app_flag_key("conf_silent", channel, CONF_SILENT_DONE);


		if ((position = switch_channel_get_variable(channel, "conference_position"))) {

			if (conference->channels == 2) {
				if (member_test_flag(member, MFLAG_NO_POSITIONAL)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
									  "%s has positional audio blocked.\n", switch_channel_get_name(channel));
				} else {
					if (member_parse_position(member, position) != SWITCH_STATUS_SUCCESS) { 
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,	"%s invalid position data\n", switch_channel_get_name(channel));
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,	"%s position data set\n", switch_channel_get_name(channel));
					}
					
					member_set_flag(member, MFLAG_POSITIONAL);
					member->al = create_al(member->pool);
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,	"%s cannot set position data on mono conference.\n", switch_channel_get_name(channel));
			}
		}
		
		

		controls = switch_channel_get_variable(channel, "conference_controls");

		if (zstr(controls)) {
			if (!member_test_flag(member, MFLAG_MOD) || !conference->moderator_controls) {
				controls = conference->caller_controls;
			} else {
				controls = conference->moderator_controls;
			}
		}

		if (zstr(controls)) {
			controls = "default";
		}

		if (strcasecmp(controls, "none")) {
			switch_ivr_dmachine_create(&member->dmachine, "mod_conference", NULL, 
									   conference->ivr_dtmf_timeout, conference->ivr_input_timeout, NULL, NULL, NULL);
			member_bind_controls(member, controls);
		}
		
	}

	unlock_member(member);
	switch_mutex_unlock(member->audio_out_mutex);
	switch_mutex_unlock(member->audio_in_mutex);

	if (conference->la && member->channel && !switch_channel_test_flag(member->channel, CF_VIDEO_ONLY)) {
		if (!member_test_flag(member, MFLAG_SECOND_SCREEN)) {
			member->json = cJSON_CreateArray();
			cJSON_AddItemToArray(member->json, cJSON_CreateStringPrintf("%0.4d", member->id));
			cJSON_AddItemToArray(member->json, cJSON_CreateString(switch_channel_get_variable(member->channel, "caller_id_number")));
			cJSON_AddItemToArray(member->json, cJSON_CreateString(switch_channel_get_variable(member->channel, "caller_id_name")));
			
			cJSON_AddItemToArray(member->json, cJSON_CreateStringPrintf("%s@%s",
																		switch_channel_get_variable(member->channel, "original_read_codec"),
																		switch_channel_get_variable(member->channel, "original_read_rate")
																		));



			
			member->status_field = cJSON_CreateString("");
			cJSON_AddItemToArray(member->json, member->status_field);
			
			cJSON_AddItemToArray(member->json, cJSON_CreateNull());
			
			member_update_status_field(member);
			//switch_live_array_add_alias(conference->la, switch_core_session_get_uuid(member->session), "conference");
		}

		adv_la(conference, member, SWITCH_TRUE);

		if (!member_test_flag(member, MFLAG_SECOND_SCREEN)) { 
			switch_live_array_add(conference->la, switch_core_session_get_uuid(member->session), -1, &member->json, SWITCH_FALSE);
		}
	}


	if (conference_test_flag(conference, CFLAG_POSITIONAL)) {
		gen_arc(conference, NULL);
	}


	send_rfc_event(conference);
	send_json_event(conference);

	switch_mutex_unlock(conference->mutex);
	status = SWITCH_STATUS_SUCCESS;

	find_video_floor(member, SWITCH_TRUE);


	if (member_test_flag(member, MFLAG_JOIN_VID_FLOOR)) {
		conference_set_video_floor_holder(conference, member, SWITCH_TRUE);
		conference_set_flag(member->conference, CFLAG_VID_FLOOR_LOCK);

		if (test_eflag(conference, EFLAG_FLOOR_CHANGE)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "conference %s OK video floor %d %s\n",
							  conference->name, member->id, switch_channel_get_name(member->channel));
		}
	}

	return status;
}

static void conference_set_video_floor_holder(conference_obj_t *conference, conference_member_t *member, switch_bool_t force)
{
	switch_event_t *event;
	conference_member_t *imember = NULL;
	int old_id = 0;
	uint32_t old_member = 0;

	if (!member) {
		conference_clear_flag(conference, CFLAG_VID_FLOOR_LOCK);
	}

	if ((!force && conference_test_flag(conference, CFLAG_VID_FLOOR_LOCK))) {
		return;
	}
	
	if (member && member->video_flow == SWITCH_MEDIA_FLOW_SENDONLY && !member->avatar_png_img) {
		return;
	}

	if (conference->video_floor_holder) {
		if (member && conference->video_floor_holder == member->id) {
			return;
		} else {			
			if (member) {
				conference->last_video_floor_holder = conference->video_floor_holder;
			}
			
			if (conference->last_video_floor_holder && (imember = conference_member_get(conference, conference->last_video_floor_holder))) {
				switch_core_session_request_video_refresh(imember->session);

				if (member_test_flag(imember, MFLAG_VIDEO_BRIDGE)) {
					conference_set_flag(conference, CFLAG_VID_FLOOR_LOCK);
				}		
				switch_thread_rwlock_unlock(imember->rwlock);
				imember = NULL;
			}
			
			old_member = conference->video_floor_holder;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Dropping video floor %d\n", old_member);
		}
	}


	if (!member) {
		switch_mutex_lock(conference->member_mutex);
		for (imember = conference->members; imember; imember = imember->next) {
			if (imember->id != conference->video_floor_holder && imember->channel && switch_channel_test_flag(imember->channel, CF_VIDEO)) {
				member = imember;
				break;
			}
		}
		switch_mutex_unlock(conference->member_mutex);
	}

	//VIDFLOOR
	if (conference->canvas_count == 1 && member && conference->canvas && conference->canvas->layout_floor_id > -1) {
		attach_video_layer(member, conference->canvas, conference->canvas->layout_floor_id);
	}

	if (member) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Adding video floor %s\n",
						  switch_channel_get_name(member->channel));

		check_flush(member);
		switch_core_session_video_reinit(member->session);
		conference->video_floor_holder = member->id;
		member_update_status_field(member);
	} else {
		conference->video_floor_holder = 0;
	}

	if (old_member) {
		conference_member_t *old_member_p = NULL;

		old_id = old_member;

		if ((old_member_p = conference_member_get(conference, old_id))) {
			member_update_status_field(old_member_p);
			switch_thread_rwlock_unlock(old_member_p->rwlock);
		}
	}

	switch_mutex_lock(conference->member_mutex);
	for (imember = conference->members; imember; imember = imember->next) {
		if (!imember->channel || !switch_channel_test_flag(imember->channel, CF_VIDEO)) {
			continue;
		}

		switch_channel_set_flag(imember->channel, CF_VIDEO_BREAK);
		switch_core_session_kill_channel(imember->session, SWITCH_SIG_BREAK);
		switch_core_session_video_reinit(imember->session);
	}
	switch_mutex_unlock(conference->member_mutex);

	conference_set_flag(conference, CFLAG_FLOOR_CHANGE);

	if (test_eflag(conference, EFLAG_FLOOR_CHANGE)) {
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT);
		conference_add_event_data(conference, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "video-floor-change");
		if (old_id) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Old-ID", "%d", old_id);
		} else {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Old-ID", "none");
		}
		if (conference->video_floor_holder) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "New-ID", "%d", conference->video_floor_holder);
		} else {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "New-ID", "none");
		}
		switch_event_fire(&event);
	}

}

static void conference_set_floor_holder(conference_obj_t *conference, conference_member_t *member)
{
	switch_event_t *event;
	conference_member_t *old_member = NULL;
	int old_id = 0;

	if (conference->floor_holder) {
		if (conference->floor_holder == member) {
			return;
		} else {
			old_member = conference->floor_holder;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Dropping floor %s\n", 
							  switch_channel_get_name(old_member->channel));

		}
	}

	switch_mutex_lock(conference->mutex);
	if (member) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Adding floor %s\n", 
						  switch_channel_get_name(member->channel));

		conference->floor_holder = member;
		member_update_status_field(member);
	} else {
		conference->floor_holder = NULL;
	}


	if (old_member) {
		old_id = old_member->id;
		member_update_status_field(old_member);
		old_member->floor_packets = 0;
	}

	conference_set_flag(conference, CFLAG_FLOOR_CHANGE);
	switch_mutex_unlock(conference->mutex);

	if (test_eflag(conference, EFLAG_FLOOR_CHANGE)) {
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT);
		conference_add_event_data(conference, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "floor-change");
		if (old_id) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Old-ID", "%d", old_id);
		} else {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Old-ID", "none");
		}

		if (conference->floor_holder) {
 			conference_add_event_member_data(conference->floor_holder, event);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "New-ID", "%d", conference->floor_holder->id);
		} else {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "New-ID", "none");
		}

		switch_event_fire(&event);
	}

}

#ifdef OPENAL_POSITIONING
static void close_al(al_handle_t *al)
{
	if (!al) return;

	switch_mutex_lock(globals.setup_mutex);	
	if (al->source) {
		alDeleteSources(1, &al->source);
		al->source = 0;
	}

	if (al->buffer_in[0]) {
		alDeleteBuffers(2, al->buffer_in);
		al->buffer_in[0] = 0;
		al->buffer_in[1] = 0;
	}

	if (al->context) {
		alcDestroyContext(al->context);
		al->context = 0;
	}

	if (al->device) {
		alcCloseDevice(al->device);
		al->device = NULL;
	}
	switch_mutex_unlock(globals.setup_mutex);
}
#endif

static switch_status_t conference_file_close(conference_obj_t *conference, conference_file_node_t *node)
{
	switch_event_t *event;
	conference_member_t *member = NULL;

	if (test_eflag(conference, EFLAG_PLAY_FILE_DONE) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		
		conference_add_event_data(conference, event);

		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "seconds", "%ld", (long) node->fh.samples_in / node->fh.native_rate);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "milliseconds", "%ld", (long) node->fh.samples_in / (node->fh.native_rate / 1000));
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "samples", "%ld", (long) node->fh.samples_in);

		if (node->fh.params) {
			switch_event_merge(event, node->fh.params);
		}

		if (node->member_id) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "play-file-member-done");
		
			if ((member = conference_member_get(conference, node->member_id))) {
				conference_add_event_member_data(member, event);
				switch_thread_rwlock_unlock(member->rwlock);
			}

		} else {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "play-file-done");
		}

		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "File", node->file);

		if (node->async) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Async", "true");
		}

		switch_event_fire(&event);
	}

#ifdef OPENAL_POSITIONING	
	if (node->al && node->al->device) {
		close_al(node->al);
	}
#endif
	if (switch_core_file_has_video(&node->fh) && conference->canvas) {
		conference->canvas->timer.interval = conference->video_fps.ms;
		conference->canvas->timer.samples = conference->video_fps.samples;
		switch_core_timer_sync(&conference->canvas->timer);
		conference->canvas->send_keyframe = 1;
		conference->playing_video_file = 0;
	}
	return switch_core_file_close(&node->fh);
}

/* Gain exclusive access and remove the member from the list */
static switch_status_t conference_del_member(conference_obj_t *conference, conference_member_t *member)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	conference_member_t *imember, *last = NULL;
	switch_event_t *event;
	conference_file_node_t *member_fnode;
	switch_speech_handle_t *member_sh;
	const char *exit_sound = NULL;

	switch_assert(conference != NULL);
	switch_assert(member != NULL);

	switch_thread_rwlock_wrlock(member->rwlock);

	if (member->session && (exit_sound = switch_channel_get_variable(switch_core_session_get_channel(member->session), "conference_exit_sound"))) {
		conference_play_file(conference, (char *)exit_sound, CONF_DEFAULT_LEADIN,
							 switch_core_session_get_channel(member->session), 0);
	}


	lock_member(member);

	member_del_relationship(member, 0);

	conference_cdr_del(member);

#ifdef OPENAL_POSITIONING	
	if (member->al && member->al->device) {
		close_al(member->al);
	}
#endif

	if (member->canvas) {
		destroy_canvas(&member->canvas);
	}

	member_fnode = member->fnode;
	member_sh = member->sh;
	member->fnode = NULL;
	member->sh = NULL;
	unlock_member(member);

	if (member->dmachine) {
		switch_ivr_dmachine_destroy(&member->dmachine);
	}

	member->avatar_patched = 0;
	switch_img_free(&member->avatar_png_img);
	switch_img_free(&member->video_mute_img);
	switch_img_free(&member->pcanvas_img);
	switch_mutex_lock(conference->mutex);
	switch_mutex_lock(conference->member_mutex);
	switch_mutex_lock(member->audio_in_mutex);
	switch_mutex_lock(member->audio_out_mutex);
	lock_member(member);
	member_clear_flag(member, MFLAG_INTREE);

	if (member->rec) {
		conference->recording_members--;
	}
	
	for (imember = conference->members; imember; imember = imember->next) {
		if (imember == member) {
			if (last) {
				last->next = imember->next;
			} else {
				conference->members = imember->next;
			}
			break;
		}
		last = imember;
	}

	switch_thread_rwlock_unlock(member->rwlock);
	
	/* Close Unused Handles */
	if (member_fnode) {
		conference_file_node_t *fnode, *cur;
		switch_memory_pool_t *pool;

		fnode = member_fnode;
		while (fnode) {
			cur = fnode;
			fnode = fnode->next;

			if (cur->type != NODE_TYPE_SPEECH) {
				conference_file_close(conference, cur);
			}

			pool = cur->pool;
			switch_core_destroy_memory_pool(&pool);
		}
	}

	if (member_sh) {
		switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;
		switch_core_speech_close(&member->lsh, &flags);
	}

	if (member == member->conference->floor_holder) {
		conference_set_floor_holder(member->conference, NULL);
	}

	if (member->id == member->conference->video_floor_holder) {
		conference_clear_flag(member->conference, CFLAG_VID_FLOOR_LOCK);
		if (member->conference->last_video_floor_holder) {
			member->conference->video_floor_holder = member->conference->last_video_floor_holder;
			member->conference->last_video_floor_holder = 0;
		}
		member->conference->video_floor_holder = 0;
	}

	if (!member_test_flag(member, MFLAG_NOCHANNEL)) {
		switch_channel_t *channel = switch_core_session_get_channel(member->session);
		if (member_test_flag(member, MFLAG_GHOST)) {
			conference->count_ghosts--;
		} else {
			conference->count--;
		}

		if (member_test_flag(member, MFLAG_ENDCONF)) {
			if (!--conference->end_count) {
				//conference_set_flag_locked(conference, CFLAG_DESTRUCT);
				conference->endconf_time = switch_epoch_time_now(NULL);
			}
		}

		conference_send_presence(conference);
		switch_channel_set_variable(channel, "conference_call_key", NULL);

		if ((conference->min && conference_test_flag(conference, CFLAG_ENFORCE_MIN) && (conference->count + conference->count_ghosts) < conference->min)
			|| (conference_test_flag(conference, CFLAG_DYNAMIC) && (conference->count + conference->count_ghosts == 0))) {
			conference_set_flag(conference, CFLAG_DESTRUCT);
		} else {
			if (!switch_true(switch_channel_get_variable(channel, "conference_permanent_wait_mod_moh")) && conference_test_flag(conference, CFLAG_WAIT_MOD)) {
				/* Stop MOH if any */
				conference_stop_file(conference, FILE_STOP_ASYNC);
			}
			if (!exit_sound && conference->exit_sound && conference_test_flag(conference, CFLAG_EXIT_SOUND) && !member_test_flag(member, MFLAG_SILENT)) {
				conference_play_file(conference, conference->exit_sound, 0, channel, 0);
			}
			if (conference->count == 1 && conference->alone_sound && !conference_test_flag(conference, CFLAG_WAIT_MOD) && !member_test_flag(member, MFLAG_GHOST)) {
				conference_stop_file(conference, FILE_STOP_ASYNC);
				conference_play_file(conference, conference->alone_sound, 0, channel, 0);
			}
		}

		if (test_eflag(conference, EFLAG_DEL_MEMBER) &&
			switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_add_event_member_data(member, event);
			conference_add_event_data(conference, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "del-member");
			switch_event_fire(&event);
		}
	}

	find_video_floor(member, SWITCH_FALSE);
	detach_video_layer(member);

	member->conference = NULL;

	switch_mutex_unlock(conference->member_mutex);
	unlock_member(member);
	switch_mutex_unlock(member->audio_out_mutex);
	switch_mutex_unlock(member->audio_in_mutex);


	if (conference->la && member->session && !switch_channel_test_flag(member->channel, CF_VIDEO_ONLY)) {
		switch_live_array_del(conference->la, switch_core_session_get_uuid(member->session));
		//switch_live_array_clear_alias(conference->la, switch_core_session_get_uuid(member->session), "conference");
		adv_la(conference, member, SWITCH_FALSE);
	}

	send_rfc_event(conference);
	send_json_event(conference);

	if (conference_test_flag(conference, CFLAG_POSITIONAL)) {
		gen_arc(conference, NULL);
	}

	if (member->session) {
		switch_core_media_hard_mute(member->session, SWITCH_FALSE);
	}

	switch_mutex_unlock(conference->mutex);
	status = SWITCH_STATUS_SUCCESS;

	return status;
}

static void conference_write_video_frame(conference_obj_t *conference, conference_member_t *floor_holder, switch_frame_t *vid_frame)
{
	conference_member_t *imember;
	int want_refresh = 0;
	unsigned char buf[SWITCH_RTP_MAX_BUF_LEN] = "";
	switch_frame_t tmp_frame = { 0 };

	if (switch_test_flag(vid_frame, SFF_CNG) || !vid_frame->packet) {
		return;
	}
	
	if (conference_test_flag(conference, CFLAG_FLOOR_CHANGE)) {
		conference_clear_flag(conference, CFLAG_FLOOR_CHANGE);
	}

	if (vid_frame->img && conference->canvas) {
		switch_image_t *frame_img = NULL, *tmp_img = NULL;
		int x,y;

		switch_img_copy(vid_frame->img, &tmp_img);
		switch_img_fit(&tmp_img, conference->canvas->width, conference->canvas->height);
		frame_img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, conference->canvas->width, conference->canvas->height, 1);
		reset_image(frame_img, &conference->canvas->bgcolor);
		switch_img_find_position(POS_CENTER_MID, frame_img->d_w, frame_img->d_h, tmp_img->d_w, tmp_img->d_h, &x, &y);
		switch_img_patch(frame_img, tmp_img, x, y);
		tmp_frame.packet = buf;
		tmp_frame.data = buf + 12;
		tmp_frame.img = frame_img;
		switch_img_free(&tmp_img);
	}


	switch_mutex_lock(conference->member_mutex);	
	for (imember = conference->members; imember; imember = imember->next) {
		switch_core_session_t *isession = imember->session;
		
		if (!isession || switch_core_session_read_lock(isession) != SWITCH_STATUS_SUCCESS) {
			continue;
		}
		
		if (switch_channel_test_flag(imember->channel, CF_VIDEO_REFRESH_REQ)) {
			want_refresh++;
			switch_channel_clear_flag(imember->channel, CF_VIDEO_REFRESH_REQ);
		}
		
		if (isession && switch_channel_test_flag(imember->channel, CF_VIDEO)) {
			int send_frame = 0;

			if (conference->canvas && conference_test_flag(imember->conference, CFLAG_VIDEO_BRIDGE_FIRST_TWO)) {
				if (switch_channel_test_flag(imember->channel, CF_VIDEO) && (conference->members_with_video == 1 || imember != floor_holder)) {
					send_frame = 1;
				}
			} else if (!member_test_flag(imember, MFLAG_RECEIVING_VIDEO) && 
				(conference_test_flag(conference, CFLAG_VID_FLOOR_LOCK) || 
				 !(imember->id == imember->conference->video_floor_holder && imember->conference->last_video_floor_holder))) {
				send_frame = 1;
			}

			if (send_frame) {
				if (vid_frame->img) {
					if (conference->canvas) {
						tmp_frame.packet = buf;
						tmp_frame.packetlen = sizeof(buf) - 12;
						tmp_frame.data = buf + 12;
						switch_core_session_write_video_frame(imember->session, &tmp_frame, SWITCH_IO_FLAG_NONE, 0);
					} else {
						switch_core_session_write_video_frame(imember->session, vid_frame, SWITCH_IO_FLAG_NONE, 0);
					}
				} else {
					switch_assert(vid_frame->packetlen <= SWITCH_RTP_MAX_BUF_LEN);
					tmp_frame = *vid_frame;
					tmp_frame.packet = buf;
					tmp_frame.data = buf + 12;
					memcpy(tmp_frame.packet, vid_frame->packet, vid_frame->packetlen);
					tmp_frame.packetlen = vid_frame->packetlen;
					tmp_frame.datalen = vid_frame->datalen;
					switch_core_session_write_video_frame(imember->session, &tmp_frame, SWITCH_IO_FLAG_NONE, 0);
				}
			}
		}
		
		switch_core_session_rwunlock(isession);
	}
	switch_mutex_unlock(conference->member_mutex);

	switch_img_free(&tmp_frame.img);

	if (want_refresh && floor_holder->session) {
		switch_core_session_request_video_refresh(floor_holder->session);
	}
}

static switch_status_t video_thread_callback(switch_core_session_t *session, switch_frame_t *frame, void *user_data)
{
	//switch_channel_t *channel = switch_core_session_get_channel(session);
	//char *name = switch_channel_get_name(channel);
	conference_member_t *member = (conference_member_t *)user_data;
	conference_relationship_t *rel = NULL, *last = NULL;

	switch_assert(member);

	if (switch_test_flag(frame, SFF_CNG) || !frame->packet) {
		return SWITCH_STATUS_SUCCESS;
	}
	
	
	if (switch_thread_rwlock_tryrdlock(member->conference->rwlock) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}


	if (conference_test_flag(member->conference, CFLAG_VIDEO_BRIDGE_FIRST_TWO)) {
		if (member->conference->members_with_video < 3) {
			conference_write_video_frame(member->conference, member, frame);
			check_video_recording(member->conference, frame);
			switch_thread_rwlock_unlock(member->conference->rwlock);
			return SWITCH_STATUS_SUCCESS; 
		}
	}


	if (conference_test_flag(member->conference, CFLAG_VIDEO_MUXING)) {
		switch_image_t *img_copy = NULL;

		if (frame->img && (member->video_layer_id > -1 || member->canvas) && member_test_flag(member, MFLAG_CAN_BE_SEEN) &&
			!member->conference->playing_video_file && switch_queue_size(member->video_queue) < member->conference->video_fps.fps) {
			switch_img_copy(frame->img, &img_copy);
			switch_queue_push(member->video_queue, img_copy);
		}

		switch_thread_rwlock_unlock(member->conference->rwlock);
		return SWITCH_STATUS_SUCCESS;
	}

	for (rel = member->relationships; rel; rel = rel->next) {
		conference_member_t *imember;
		if (!(rel->flags & RFLAG_CAN_SEND_VIDEO)) continue;

		if ((imember = conference_member_get(member->conference, rel->id)) && member_test_flag(imember, MFLAG_RECEIVING_VIDEO)) {
			//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s %d->%d %d\n", name, member->id, imember->id, frame->datalen);
			switch_core_session_write_video_frame(imember->session, frame, SWITCH_IO_FLAG_NONE, 0);
			switch_thread_rwlock_unlock(imember->rwlock);
		} else { /* Stale .. Remove */
			if (last) {
				last->next = rel->next;
			} else {
				member->relationships = rel->next;
			}			

			switch_mutex_lock(member->conference->member_mutex);
			member->conference->relationship_total--;
			switch_mutex_unlock(member->conference->member_mutex);

			continue;
		}

		last = rel;
	}


	if (member) {
		if (member->id == member->conference->video_floor_holder) {
			conference_write_video_frame(member->conference, member, frame);
			check_video_recording(member->conference, frame);
		} else if (!conference_test_flag(member->conference, CFLAG_VID_FLOOR_LOCK) && member->id == member->conference->last_video_floor_holder) {
			conference_member_t *fmember;

			if ((fmember = conference_member_get(member->conference, member->conference->video_floor_holder))) {
				switch_core_session_write_video_frame(fmember->session, frame, SWITCH_IO_FLAG_NONE, 0);
				switch_thread_rwlock_unlock(fmember->rwlock);
			}
		}
	}

	switch_thread_rwlock_unlock(member->conference->rwlock);

	return SWITCH_STATUS_SUCCESS;
}

static void conference_command_handler(switch_live_array_t *la, const char *cmd, const char *sessid, cJSON *jla, void *user_data)
{
}

/* Main monitor thread (1 per distinct conference room) */
static void *SWITCH_THREAD_FUNC conference_thread_run(switch_thread_t *thread, void *obj)
{
	conference_obj_t *conference = (conference_obj_t *) obj;
	conference_member_t *imember, *omember;
	uint32_t samples = switch_samples_per_packet(conference->rate, conference->interval);
	uint32_t bytes = samples * 2 * conference->channels;
	uint8_t ready = 0, total = 0;
	switch_timer_t timer = { 0 };
	switch_event_t *event;
	uint8_t *file_frame;
	uint8_t *async_file_frame;
	int16_t *bptr;
	uint32_t x = 0;
	int32_t z = 0;
	int member_score_sum = 0;
	int divisor = 0;
	conference_cdr_node_t *np;

	if (!(divisor = conference->rate / 8000)) {
		divisor = 1;
	}

	file_frame = switch_core_alloc(conference->pool, SWITCH_RECOMMENDED_BUFFER_SIZE);
	async_file_frame = switch_core_alloc(conference->pool, SWITCH_RECOMMENDED_BUFFER_SIZE);

	if (switch_core_timer_init(&timer, conference->timer_name, conference->interval, samples, conference->pool) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Setup timer success interval: %u  samples: %u\n", conference->interval, samples);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Timer Setup Failed.  Conference Cannot Start\n");
		return NULL;
	}

	switch_mutex_lock(globals.hash_mutex);
	globals.threads++;
	switch_mutex_unlock(globals.hash_mutex);

	conference->auto_recording = 0;
	conference->record_count = 0;



	switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT);
	conference_add_event_data(conference, event); 
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "conference-create");
	switch_event_fire(&event);

	if (conference_test_flag(conference, CFLAG_LIVEARRAY_SYNC)) {
		char *p;

		if (strchr(conference->name, '@')) {
			conference->la_event_channel = switch_core_sprintf(conference->pool, "conference-liveArray.%s", conference->name);
			conference->mod_event_channel = switch_core_sprintf(conference->pool, "conference-mod.%s", conference->name);
		} else {
			conference->la_event_channel = switch_core_sprintf(conference->pool, "conference-liveArray.%s@%s", conference->name, conference->domain);
			conference->mod_event_channel = switch_core_sprintf(conference->pool, "conference-mod.%s@%s", conference->name, conference->domain);
		}

		conference->la_name = switch_core_strdup(conference->pool, conference->name);
		if ((p = strchr(conference->la_name, '@'))) {
			*p = '\0';
		}

		switch_live_array_create(conference->la_event_channel, conference->la_name, globals.event_channel_id, &conference->la);
		switch_live_array_set_user_data(conference->la, conference);
		switch_live_array_set_command_handler(conference->la, conference_command_handler);
	}


	while (globals.running && !conference_test_flag(conference, CFLAG_DESTRUCT)) {
		switch_size_t file_sample_len = samples;
		switch_size_t file_data_len = samples * 2 * conference->channels;
		int has_file_data = 0, members_with_video = 0, members_with_avatar = 0;
		uint32_t conf_energy = 0;
		int nomoh = 0;
		conference_member_t *floor_holder;

		/* Sync the conference to a single timing source */
		if (switch_core_timer_next(&timer) != SWITCH_STATUS_SUCCESS) {
			conference_set_flag(conference, CFLAG_DESTRUCT);
			break;
		}

		switch_mutex_lock(conference->mutex);
		has_file_data = ready = total = 0;

		floor_holder = conference->floor_holder;
		
		/* Read one frame of audio from each member channel and save it for redistribution */
		for (imember = conference->members; imember; imember = imember->next) {
			uint32_t buf_read = 0;
			total++;
			imember->read = 0;

			if (member_test_flag(imember, MFLAG_RUNNING) && imember->session) {
				switch_channel_t *channel = switch_core_session_get_channel(imember->session);

				if ((!floor_holder || (imember->score_iir > SCORE_IIR_SPEAKING_MAX && (floor_holder->score_iir < SCORE_IIR_SPEAKING_MIN)))) {// &&
					//(!conference_test_flag(conference, CFLAG_VID_FLOOR) || switch_channel_test_flag(channel, CF_VIDEO))) {
					floor_holder = imember;
				}
				
				if (switch_channel_ready(channel) && switch_channel_test_flag(channel, CF_VIDEO) && imember->video_flow != SWITCH_MEDIA_FLOW_SENDONLY) {
					members_with_video++;
				}

				if (imember->avatar_png_img && !switch_channel_test_flag(channel, CF_VIDEO)) {
					members_with_avatar++;
				}

				if (member_test_flag(imember, MFLAG_NOMOH)) {
					nomoh++;
				}
			}

			member_clear_flag_locked(imember, MFLAG_HAS_AUDIO);
			switch_mutex_lock(imember->audio_in_mutex);

			if (switch_buffer_inuse(imember->audio_buffer) >= bytes
				&& (buf_read = (uint32_t) switch_buffer_read(imember->audio_buffer, imember->frame, bytes))) {
				imember->read = buf_read;
				member_set_flag_locked(imember, MFLAG_HAS_AUDIO);
				ready++;
			}
			switch_mutex_unlock(imember->audio_in_mutex);
		}
		
		conference->members_with_video = members_with_video;
		conference->members_with_avatar = members_with_avatar;

		if (floor_holder != conference->floor_holder) {
			conference_set_floor_holder(conference, floor_holder);
		}

		if (conference->perpetual_sound && !conference->async_fnode) {
			conference_play_file(conference, conference->perpetual_sound, CONF_DEFAULT_LEADIN, NULL, 1);
		} else if (conference->moh_sound && ((nomoh == 0 && conference->count == 1) 
									 || conference_test_flag(conference, CFLAG_WAIT_MOD)) && !conference->async_fnode && !conference->fnode) {
			conference_play_file(conference, conference->moh_sound, CONF_DEFAULT_LEADIN, NULL, 1);
		}


		/* Find if no one talked for more than x number of second */
		if (conference->terminate_on_silence && conference->count > 1) {
			int is_talking = 0;

			for (imember = conference->members; imember; imember = imember->next) {
				if (switch_epoch_time_now(NULL) - imember->join_time <= conference->terminate_on_silence) {
					is_talking++;
				} else if (imember->last_talking != 0 && switch_epoch_time_now(NULL) - imember->last_talking <= conference->terminate_on_silence) {
					is_talking++;
				}
			}
			if (is_talking == 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Conference has been idle for over %d seconds, terminating\n", conference->terminate_on_silence);
				conference_set_flag(conference, CFLAG_DESTRUCT);
			}
		}

		/* Start auto recording if there's the minimum number of required participants. */
		if (conference->auto_record && !conference->auto_recording && (conference->count >= conference->min_recording_participants)) {
			conference->auto_recording++;
			conference->record_count++;
			imember = conference->members;
			if (imember) {
				switch_channel_t *channel = switch_core_session_get_channel(imember->session);
				char *rfile = switch_channel_expand_variables(channel, conference->auto_record);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Auto recording file: %s\n", rfile);
				launch_conference_record_thread(conference, rfile, SWITCH_TRUE);

				if (rfile != conference->auto_record) {
					conference->record_filename = switch_core_strdup(conference->pool, rfile);
					switch_safe_free(rfile);
				} else {
					conference->record_filename = switch_core_strdup(conference->pool, conference->auto_record);
				}
				/* Set the conference recording variable for each member */
				for (omember = conference->members; omember; omember = omember->next) {
					if (!omember->session) continue;
					channel = switch_core_session_get_channel(omember->session);
					switch_channel_set_variable(channel, "conference_recording", conference->record_filename);
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Auto Record Failed.  No members in conference.\n");
			}
		}

		/* If a file or speech event is being played */
		if (conference->fnode && !switch_test_flag(conference->fnode, NFLAG_PAUSE)) {
			/* Lead in time */
			if (conference->fnode->leadin) {
				conference->fnode->leadin--;
			} else if (!conference->fnode->done) {
				file_sample_len = samples;
				
				if (conference->fnode->type == NODE_TYPE_SPEECH) {
					switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_BLOCKING;
					switch_size_t speech_len = file_data_len;
					
					if (conference->fnode->al) {
						speech_len /= 2;
					}
					
					if (switch_core_speech_read_tts(conference->fnode->sh, file_frame, &speech_len, &flags) == SWITCH_STATUS_SUCCESS) {
						
						if (conference->fnode->al) {
							process_al(conference->fnode->al, file_frame, speech_len, conference->rate);
						}

						file_sample_len = file_data_len / 2 / conference->fnode->sh->channels;

						
					} else {
						file_sample_len = file_data_len = 0;
					}
				} else if (conference->fnode->type == NODE_TYPE_FILE) {
					switch_core_file_read(&conference->fnode->fh, file_frame, &file_sample_len);
					if (conference->fnode->fh.vol) {
						switch_change_sln_volume_granular((void *)file_frame, (uint32_t)file_sample_len * conference->fnode->fh.channels, 
														  conference->fnode->fh.vol);
					}
					if (conference->fnode->al) {
						process_al(conference->fnode->al, file_frame, file_sample_len * 2, conference->fnode->fh.samplerate);
					}
				}

				if (file_sample_len <= 0) {
					conference->fnode->done++;
				} else {
					has_file_data = 1;
				}
			}
		}

		if (conference->async_fnode) {
			/* Lead in time */
			if (conference->async_fnode->leadin) {
				conference->async_fnode->leadin--;
			} else if (!conference->async_fnode->done) {
				file_sample_len = samples;
				switch_core_file_read(&conference->async_fnode->fh, async_file_frame, &file_sample_len);
				if (conference->async_fnode->al) {
					process_al(conference->async_fnode->al, file_frame, file_sample_len * 2, conference->async_fnode->fh.samplerate);
				}
				if (file_sample_len <= 0) {
					conference->async_fnode->done++;
				} else {
					if (has_file_data) {
						switch_size_t x;
						for (x = 0; x < file_sample_len * conference->channels; x++) {
							int32_t z;
							int16_t *muxed;

							muxed = (int16_t *) file_frame;
							bptr = (int16_t *) async_file_frame;
							z = muxed[x] + bptr[x];
							switch_normalize_to_16bit(z);
							muxed[x] = (int16_t) z;
						}
					} else {
						memcpy(file_frame, async_file_frame, file_sample_len * 2 * conference->channels);
						has_file_data = 1;
					}
				}
			}
		}
		
		if (ready || has_file_data) {
			/* Use more bits in the main_frame to preserve the exact sum of the audio samples. */
			int main_frame[SWITCH_RECOMMENDED_BUFFER_SIZE] = { 0 };
			int16_t write_frame[SWITCH_RECOMMENDED_BUFFER_SIZE] = { 0 };


			/* Init the main frame with file data if there is any. */
			bptr = (int16_t *) file_frame;
			if (has_file_data && file_sample_len) {

				for (x = 0; x < bytes / 2; x++) {
					if (x <= file_sample_len * conference->channels) {
						main_frame[x] = (int32_t) bptr[x];
					} else {
						memset(&main_frame[x], 255, sizeof(main_frame[x]));
					}
				}
			}

			member_score_sum = 0;
			conference->mux_loop_count = 0;
			conference->member_loop_count = 0;


			/* Copy audio from every member known to be producing audio into the main frame. */
			for (omember = conference->members; omember; omember = omember->next) {
				conference->member_loop_count++;
				
				if (!(member_test_flag(omember, MFLAG_RUNNING) && member_test_flag(omember, MFLAG_HAS_AUDIO))) {
					continue;
				}

				if (conference->agc_level) {
					if (member_test_flag(omember, MFLAG_TALKING) && member_test_flag(omember, MFLAG_CAN_SPEAK)) {
						member_score_sum += omember->score;
						conference->mux_loop_count++;
					}
				}
				
				bptr = (int16_t *) omember->frame;
				for (x = 0; x < omember->read / 2; x++) {
					main_frame[x] += (int32_t) bptr[x];
				}
			}

			if (conference->agc_level && conference->member_loop_count) {
				conf_energy = 0;
			
				for (x = 0; x < bytes / 2; x++) {
					z = abs(main_frame[x]);
					switch_normalize_to_16bit(z);
					conf_energy += (int16_t) z;
				}
				
				conference->score = conf_energy / ((bytes / 2) / divisor) / conference->member_loop_count;

				conference->avg_tally += conference->score;
				conference->avg_score = conference->avg_tally / ++conference->avg_itt;
				if (!conference->avg_itt) conference->avg_tally = conference->score;
			}
			
			/* Create write frame once per member who is not deaf for each sample in the main frame
			   check if our audio is involved and if so, subtract it from the sample so we don't hear ourselves.
			   Since main frame was 32 bit int, we did not lose any detail, now that we have to convert to 16 bit we can
			   cut it off at the min and max range if need be and write the frame to the output buffer.
			 */
			for (omember = conference->members; omember; omember = omember->next) {
				switch_size_t ok = 1;

				if (!member_test_flag(omember, MFLAG_RUNNING)) {
					continue;
				}

				if (!member_test_flag(omember, MFLAG_CAN_HEAR)) {
					switch_mutex_lock(omember->audio_out_mutex);
					memset(write_frame, 255, bytes);
					ok = switch_buffer_write(omember->mux_buffer, write_frame, bytes);
					switch_mutex_unlock(omember->audio_out_mutex);
					continue;
				}

				bptr = (int16_t *) omember->frame;
				
				for (x = 0; x < bytes / 2 ; x++) {
					z = main_frame[x];

					/* bptr[x] represents my own contribution to this audio sample */
					if (member_test_flag(omember, MFLAG_HAS_AUDIO) && x <= omember->read / 2) {
						z -= (int32_t) bptr[x];
					}

					/* when there are relationships, we have to do more work by scouring all the members to see if there are any 
					   reasons why we should not be hearing a paticular member, and if not, delete their samples as well.
					 */
					if (conference->relationship_total) {
						for (imember = conference->members; imember; imember = imember->next) {
							if (imember != omember && member_test_flag(imember, MFLAG_HAS_AUDIO)) {
								conference_relationship_t *rel;
								switch_size_t found = 0;
								int16_t *rptr = (int16_t *) imember->frame;
								for (rel = imember->relationships; rel; rel = rel->next) {
									if ((rel->id == omember->id || rel->id == 0) && !switch_test_flag(rel, RFLAG_CAN_SPEAK)) {
										z -= (int32_t) rptr[x];
										found = 1;
										break;
									}
								}
								if (!found) {
									for (rel = omember->relationships; rel; rel = rel->next) {
										if ((rel->id == imember->id || rel->id == 0) && !switch_test_flag(rel, RFLAG_CAN_HEAR)) {
											z -= (int32_t) rptr[x];
											break;
										}
									}
								}

							}
						}
					}

					/* Now we can convert to 16 bit. */
					switch_normalize_to_16bit(z);
					write_frame[x] = (int16_t) z;
				}
				
				switch_mutex_lock(omember->audio_out_mutex);
				ok = switch_buffer_write(omember->mux_buffer, write_frame, bytes);
				switch_mutex_unlock(omember->audio_out_mutex);

				if (!ok) {
					switch_mutex_unlock(conference->mutex);
					goto end;
				}
			}
		} else { /* There is no source audio.  Push silence into all of the buffers */
			int16_t write_frame[SWITCH_RECOMMENDED_BUFFER_SIZE] = { 0 };

			if (conference->comfort_noise_level) {
				switch_generate_sln_silence(write_frame, samples, conference->channels, conference->comfort_noise_level);
			} else {
				memset(write_frame, 255, bytes);
			}
			
			for (omember = conference->members; omember; omember = omember->next) {
				switch_size_t ok = 1;
				
				if (!member_test_flag(omember, MFLAG_RUNNING)) {
					continue;
				}
				
				switch_mutex_lock(omember->audio_out_mutex);
				ok = switch_buffer_write(omember->mux_buffer, write_frame, bytes);
				switch_mutex_unlock(omember->audio_out_mutex);

				if (!ok) {
					switch_mutex_unlock(conference->mutex);
					goto end;
				}
			}
		}

		if (conference->async_fnode && conference->async_fnode->done) {
			switch_memory_pool_t *pool;

			if (conference->canvas && conference->async_fnode->layer_id > -1 ) {
				canvas_del_fnode_layer(conference, conference->async_fnode);
			}

			conference_file_close(conference, conference->async_fnode);
			pool = conference->async_fnode->pool;
			conference->async_fnode = NULL;
			switch_core_destroy_memory_pool(&pool);
		}

		if (conference->fnode && conference->fnode->done) {
			conference_file_node_t *fnode;
			switch_memory_pool_t *pool;

			if (conference->canvas && conference->fnode->layer_id > -1 ) {
				canvas_del_fnode_layer(conference, conference->fnode);
			}
			
			if (conference->fnode->type != NODE_TYPE_SPEECH) {
				conference_file_close(conference, conference->fnode);
			}

			fnode = conference->fnode;
			conference->fnode = conference->fnode->next;

			if (conference->fnode) {
				fnode_check_video(conference->fnode);
			}


			pool = fnode->pool;
			fnode = NULL;
			switch_core_destroy_memory_pool(&pool);
		}

		if (!conference->end_count && conference->endconf_time &&
				switch_epoch_time_now(NULL) - conference->endconf_time > conference->endconf_grace_time) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Conference %s: endconf grace time exceeded (%u)\n",
					conference->name, conference->endconf_grace_time);
			conference_set_flag(conference, CFLAG_DESTRUCT | CFLAG_ENDCONF_FORCED);
		}

		switch_mutex_unlock(conference->mutex);
	}
	/* Rinse ... Repeat */
  end:

	if (conference_test_flag(conference, CFLAG_OUTCALL)) {
		conference->cancel_cause = SWITCH_CAUSE_ORIGINATOR_CANCEL;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Ending pending outcall channels for Conference: '%s'\n", conference->name);
		while(conference->originating) {
			switch_yield(200000);
		}
	}

	conference_send_presence(conference);

	switch_mutex_lock(conference->mutex);
	conference_stop_file(conference, FILE_STOP_ASYNC);
	conference_stop_file(conference, FILE_STOP_ALL);
	
	for (np = conference->cdr_nodes; np; np = np->next) {
		if (np->var_event) {
			switch_event_destroy(&np->var_event);
		}
	}


	/* Close Unused Handles */
	if (conference->fnode) {
		conference_file_node_t *fnode, *cur;
		switch_memory_pool_t *pool;

		fnode = conference->fnode;
		while (fnode) {
			cur = fnode;
			fnode = fnode->next;

			if (cur->type != NODE_TYPE_SPEECH) {
				conference_file_close(conference, cur);
			}

			pool = cur->pool;
			switch_core_destroy_memory_pool(&pool);
		}
		conference->fnode = NULL;
	}

	if (conference->async_fnode) {
		switch_memory_pool_t *pool;
		conference_file_close(conference, conference->async_fnode);
		pool = conference->async_fnode->pool;
		conference->async_fnode = NULL;
		switch_core_destroy_memory_pool(&pool);
	}

	switch_mutex_lock(conference->member_mutex);
	for (imember = conference->members; imember; imember = imember->next) {
		switch_channel_t *channel;

		if (!member_test_flag(imember, MFLAG_NOCHANNEL)) {
			channel = switch_core_session_get_channel(imember->session);

			if (!switch_false(switch_channel_get_variable(channel, "hangup_after_conference"))) {
				/* add this little bit to preserve the bridge cause code in case of an early media call that */
				/* never answers */
				if (conference_test_flag(conference, CFLAG_ANSWERED)) {
					switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
				} else {
					/* put actual cause code from outbound channel hangup here */
					switch_channel_hangup(channel, conference->bridge_hangup_cause);
				}
			}
		}

		member_clear_flag_locked(imember, MFLAG_RUNNING);
	}
	switch_mutex_unlock(conference->member_mutex);
	switch_mutex_unlock(conference->mutex);

	if (conference->vh[0].up == 1) {
		conference->vh[0].up = -1;
	}

	if (conference->vh[1].up == 1) {
		conference->vh[1].up = -1;
	}
	
	while (conference->vh[0].up || conference->vh[1].up) {
		switch_cond_next();
	}

	switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT);
	conference_add_event_data(conference, event); 
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "conference-destroy");
	switch_event_fire(&event);
	
	switch_core_timer_destroy(&timer);
	switch_mutex_lock(globals.hash_mutex);
	if (conference_test_flag(conference, CFLAG_INHASH)) {
		switch_core_hash_delete(globals.conference_hash, conference->name);
	}
	switch_mutex_unlock(globals.hash_mutex);


	conference_clear_flag(conference, CFLAG_VIDEO_MUXING);

	for (x = 0; x <= conference->canvas_count; x++) {
		if (conference->canvases[x] && conference->canvases[x]->video_muxing_thread) {
			switch_status_t st = 0;
			switch_thread_join(&st, conference->canvases[x]->video_muxing_thread);
			conference->canvases[x]->video_muxing_thread = NULL;
		}
	}

	/* Wait till everybody is out */
	conference_clear_flag_locked(conference, CFLAG_RUNNING);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Write Lock ON\n");
	switch_thread_rwlock_wrlock(conference->rwlock);
	switch_thread_rwlock_unlock(conference->rwlock);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Write Lock OFF\n");

	if (conference->la) {
		switch_live_array_destroy(&conference->la);
	}

	if (conference->sh) {
		switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;
		switch_core_speech_close(&conference->lsh, &flags);
		conference->sh = NULL;
	}

	conference->end_time = switch_epoch_time_now(NULL);
	conference_cdr_render(conference);

	switch_mutex_lock(globals.setup_mutex);
	if (conference->layout_hash) {
		switch_core_hash_destroy(&conference->layout_hash);
	}
	switch_mutex_unlock(globals.setup_mutex);

	if (conference->layout_group_hash) {
		switch_core_hash_destroy(&conference->layout_group_hash);
	}


	if (conference->pool) {
		switch_memory_pool_t *pool = conference->pool;
		switch_core_destroy_memory_pool(&pool);
	}

	switch_mutex_lock(globals.hash_mutex);
	globals.threads--;
	switch_mutex_unlock(globals.hash_mutex);

	return NULL;
}

static void conference_loop_fn_floor_toggle(conference_member_t *member, caller_control_action_t *action)
{
	if (member == NULL) return;

	conf_api_sub_floor(member, NULL, NULL);
}

static void conference_loop_fn_vid_floor_toggle(conference_member_t *member, caller_control_action_t *action)
{
	if (member == NULL) return;

	conf_api_sub_vid_floor(member, NULL, NULL);
}

static void conference_loop_fn_vid_floor_force(conference_member_t *member, caller_control_action_t *action)
{
	if (member == NULL) return;

	conf_api_sub_vid_floor(member, NULL, "force");
}

static void conference_loop_fn_mute_toggle(conference_member_t *member, caller_control_action_t *action)
{
	if (member == NULL)
		return;

	if (member_test_flag(member, MFLAG_CAN_SPEAK)) {
		conf_api_sub_mute(member, NULL, NULL);
	} else {
		conf_api_sub_unmute(member, NULL, NULL);
		if (!member_test_flag(member, MFLAG_CAN_HEAR)) {
			conf_api_sub_undeaf(member, NULL, NULL);
		}
	}
}

static void conference_loop_fn_mute_on(conference_member_t *member, caller_control_action_t *action)
{
	if (member_test_flag(member, MFLAG_CAN_SPEAK)) {
		conf_api_sub_mute(member, NULL, NULL);
	}
}

static void conference_loop_fn_mute_off(conference_member_t *member, caller_control_action_t *action)
{
	if (!member_test_flag(member, MFLAG_CAN_SPEAK)) {
		conf_api_sub_unmute(member, NULL, NULL);
		if (!member_test_flag(member, MFLAG_CAN_HEAR)) {
			conf_api_sub_undeaf(member, NULL, NULL);
		}
	}
}

static void conference_loop_fn_vmute_snap(conference_member_t *member, caller_control_action_t *action)
{
	vmute_snap(member, SWITCH_FALSE);
}

static void conference_loop_fn_vmute_snapoff(conference_member_t *member, caller_control_action_t *action)
{
	vmute_snap(member, SWITCH_TRUE);
}

static void conference_loop_fn_vmute_toggle(conference_member_t *member, caller_control_action_t *action)
{
	if (member == NULL)
		return;

	if (member_test_flag(member, MFLAG_CAN_BE_SEEN)) {
		conf_api_sub_vmute(member, NULL, NULL);
	} else {
		conf_api_sub_unvmute(member, NULL, NULL);
	}
}

static void conference_loop_fn_vmute_on(conference_member_t *member, caller_control_action_t *action)
{
	if (member_test_flag(member, MFLAG_CAN_BE_SEEN)) {
		conf_api_sub_vmute(member, NULL, NULL);
	}
}

static void conference_loop_fn_vmute_off(conference_member_t *member, caller_control_action_t *action)
{
	if (!member_test_flag(member, MFLAG_CAN_BE_SEEN)) {
		conf_api_sub_unvmute(member, NULL, NULL);
	}
}

static void conference_loop_fn_lock_toggle(conference_member_t *member, caller_control_action_t *action)
{
	switch_event_t *event;

	if (member == NULL)
		return;

	if (conference_test_flag(member->conference, CFLAG_WAIT_MOD) && !member_test_flag(member, MFLAG_MOD) )
		return; 

	if (!conference_test_flag(member->conference, CFLAG_LOCKED)) {
		if (member->conference->is_locked_sound) {
			conference_play_file(member->conference, member->conference->is_locked_sound, CONF_DEFAULT_LEADIN, NULL, 0);
		}

		conference_set_flag_locked(member->conference, CFLAG_LOCKED);
		if (test_eflag(member->conference, EFLAG_LOCK) &&
			switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_add_event_data(member->conference, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "lock");
			switch_event_fire(&event);
		}
	} else {
		if (member->conference->is_unlocked_sound) {
			conference_play_file(member->conference, member->conference->is_unlocked_sound, CONF_DEFAULT_LEADIN, NULL, 0);
		}

		conference_clear_flag_locked(member->conference, CFLAG_LOCKED);
		if (test_eflag(member->conference, EFLAG_UNLOCK) &&
			switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_add_event_data(member->conference, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "unlock");
			switch_event_fire(&event);
		}
	}

}

static void conference_loop_fn_deafmute_toggle(conference_member_t *member, caller_control_action_t *action)
{
	if (member == NULL)
		return;

	if (member_test_flag(member, MFLAG_CAN_SPEAK)) {
		conf_api_sub_mute(member, NULL, NULL);
		if (member_test_flag(member, MFLAG_CAN_HEAR)) {
			conf_api_sub_deaf(member, NULL, NULL);
		}
	} else {
		conf_api_sub_unmute(member, NULL, NULL);
		if (!member_test_flag(member, MFLAG_CAN_HEAR)) {
			conf_api_sub_undeaf(member, NULL, NULL);
		}
	}
}

static void conference_loop_fn_energy_up(conference_member_t *member, caller_control_action_t *action)
{
	char msg[512], str[30] = "";
	switch_event_t *event;
	char *p;

	if (member == NULL)
		return;


	member->energy_level += 200;
	if (member->energy_level > 1800) {
		member->energy_level = 1800;
	}

	if (test_eflag(member->conference, EFLAG_ENERGY_LEVEL) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_member_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "energy-level");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "New-Level", "%d", member->energy_level);
		switch_event_fire(&event);
	}

	//switch_snprintf(msg, sizeof(msg), "Energy level %d", member->energy_level);
	//conference_member_say(member, msg, 0);

	switch_snprintf(str, sizeof(str), "%d", abs(member->energy_level) / 200);
	for (p = str; p && *p; p++) {
		switch_snprintf(msg, sizeof(msg), "digits/%c.wav", *p);
		conference_member_play_file(member, msg, 0, SWITCH_TRUE);
	}


	

}

static void conference_loop_fn_energy_equ_conf(conference_member_t *member, caller_control_action_t *action)
{
	char msg[512], str[30] = "", *p;
	switch_event_t *event;

	if (member == NULL)
		return;

	member->energy_level = member->conference->energy_level;

	if (test_eflag(member->conference, EFLAG_ENERGY_LEVEL) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_member_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "energy-level");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "New-Level", "%d", member->energy_level);
		switch_event_fire(&event);
	}

	//switch_snprintf(msg, sizeof(msg), "Energy level %d", member->energy_level);
	//conference_member_say(member, msg, 0);

	switch_snprintf(str, sizeof(str), "%d", abs(member->energy_level) / 200);
	for (p = str; p && *p; p++) {
		switch_snprintf(msg, sizeof(msg), "digits/%c.wav", *p);
		conference_member_play_file(member, msg, 0, SWITCH_TRUE);
	}
	
}

static void conference_loop_fn_energy_dn(conference_member_t *member, caller_control_action_t *action)
{
	char msg[512], str[30] = "", *p;
	switch_event_t *event;

	if (member == NULL)
		return;

	member->energy_level -= 200;
	if (member->energy_level < 0) {
		member->energy_level = 0;
	}

	if (test_eflag(member->conference, EFLAG_ENERGY_LEVEL) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_member_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "energy-level");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "New-Level", "%d", member->energy_level);
		switch_event_fire(&event);
	}

	//switch_snprintf(msg, sizeof(msg), "Energy level %d", member->energy_level);
	//conference_member_say(member, msg, 0);

	switch_snprintf(str, sizeof(str), "%d", abs(member->energy_level) / 200);
	for (p = str; p && *p; p++) {
		switch_snprintf(msg, sizeof(msg), "digits/%c.wav", *p);
		conference_member_play_file(member, msg, 0, SWITCH_TRUE);
	}
	
}

static void conference_loop_fn_volume_talk_up(conference_member_t *member, caller_control_action_t *action)
{
	char msg[512];
	switch_event_t *event;

	if (member == NULL)
		return;

	member->volume_out_level++;
	switch_normalize_volume(member->volume_out_level);

	if (test_eflag(member->conference, EFLAG_VOLUME_LEVEL) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_member_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "volume-level");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "New-Level", "%d", member->volume_out_level);
		switch_event_fire(&event);
	}

	//switch_snprintf(msg, sizeof(msg), "Volume level %d", member->volume_out_level);
	//conference_member_say(member, msg, 0);

	if (member->volume_out_level < 0) {
		switch_snprintf(msg, sizeof(msg), "currency/negative.wav", member->volume_out_level);
		conference_member_play_file(member, msg, 0, SWITCH_TRUE);
	}

	switch_snprintf(msg, sizeof(msg), "digits/%d.wav", abs(member->volume_out_level));
	conference_member_play_file(member, msg, 0, SWITCH_TRUE);

}

static void conference_loop_fn_volume_talk_zero(conference_member_t *member, caller_control_action_t *action)
{
	char msg[512];
	switch_event_t *event;

	if (member == NULL)
		return;

	member->volume_out_level = 0;

	if (test_eflag(member->conference, EFLAG_VOLUME_LEVEL) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_member_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "volume-level");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "New-Level", "%d", member->volume_out_level);
		switch_event_fire(&event);
	}

	//switch_snprintf(msg, sizeof(msg), "Volume level %d", member->volume_out_level);
	//conference_member_say(member, msg, 0);


	if (member->volume_out_level < 0) {
		switch_snprintf(msg, sizeof(msg), "currency/negative.wav", member->volume_out_level);
		conference_member_play_file(member, msg, 0, SWITCH_TRUE);
	}

	switch_snprintf(msg, sizeof(msg), "digits/%d.wav", abs(member->volume_out_level));
	conference_member_play_file(member, msg, 0, SWITCH_TRUE);
}

static void conference_loop_fn_volume_talk_dn(conference_member_t *member, caller_control_action_t *action)
{
	char msg[512];
	switch_event_t *event;

	if (member == NULL)
		return;

	member->volume_out_level--;
	switch_normalize_volume(member->volume_out_level);

	if (test_eflag(member->conference, EFLAG_VOLUME_LEVEL) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_member_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "volume-level");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "New-Level", "%d", member->volume_out_level);
		switch_event_fire(&event);
	}

	//switch_snprintf(msg, sizeof(msg), "Volume level %d", member->volume_out_level);
	//conference_member_say(member, msg, 0);

	if (member->volume_out_level < 0) {
		switch_snprintf(msg, sizeof(msg), "currency/negative.wav", member->volume_out_level);
		conference_member_play_file(member, msg, 0, SWITCH_TRUE);
	}

	switch_snprintf(msg, sizeof(msg), "digits/%d.wav", abs(member->volume_out_level));
	conference_member_play_file(member, msg, 0, SWITCH_TRUE);
}

static void conference_loop_fn_volume_listen_up(conference_member_t *member, caller_control_action_t *action)
{
	char msg[512];
	switch_event_t *event;

	if (member == NULL)
		return;

	member->volume_in_level++;
	switch_normalize_volume(member->volume_in_level);

	if (test_eflag(member->conference, EFLAG_GAIN_LEVEL) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_member_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "gain-level");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "New-Level", "%d", member->volume_in_level);
		switch_event_fire(&event);
	}

	//switch_snprintf(msg, sizeof(msg), "Gain level %d", member->volume_in_level);
	//conference_member_say(member, msg, 0);

	if (member->volume_in_level < 0) {
		switch_snprintf(msg, sizeof(msg), "currency/negative.wav", member->volume_in_level);
		conference_member_play_file(member, msg, 0, SWITCH_TRUE);
	}

	switch_snprintf(msg, sizeof(msg), "digits/%d.wav", abs(member->volume_in_level));
	conference_member_play_file(member, msg, 0, SWITCH_TRUE);

}

static void conference_loop_fn_volume_listen_zero(conference_member_t *member, caller_control_action_t *action)
{
	char msg[512];
	switch_event_t *event;

	if (member == NULL)
		return;

	member->volume_in_level = 0;

	if (test_eflag(member->conference, EFLAG_GAIN_LEVEL) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_member_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "gain-level");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "New-Level", "%d", member->volume_in_level);
		switch_event_fire(&event);
	}

	//switch_snprintf(msg, sizeof(msg), "Gain level %d", member->volume_in_level);
	//conference_member_say(member, msg, 0);

	if (member->volume_in_level < 0) {
		switch_snprintf(msg, sizeof(msg), "currency/negative.wav", member->volume_in_level);
		conference_member_play_file(member, msg, 0, SWITCH_TRUE);
	}

	switch_snprintf(msg, sizeof(msg), "digits/%d.wav", abs(member->volume_in_level));
	conference_member_play_file(member, msg, 0, SWITCH_TRUE);
	
}

static void conference_loop_fn_volume_listen_dn(conference_member_t *member, caller_control_action_t *action)
{
	char msg[512];
	switch_event_t *event;

	if (member == NULL)
		return;

	member->volume_in_level--;
	switch_normalize_volume(member->volume_in_level);

	if (test_eflag(member->conference, EFLAG_GAIN_LEVEL) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_member_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "gain-level");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "New-Level", "%d", member->volume_in_level);
		switch_event_fire(&event);
	}

	//switch_snprintf(msg, sizeof(msg), "Gain level %d", member->volume_in_level);
	//conference_member_say(member, msg, 0);

	if (member->volume_in_level < 0) {
		switch_snprintf(msg, sizeof(msg), "currency/negative.wav", member->volume_in_level);
		conference_member_play_file(member, msg, 0, SWITCH_TRUE);
	}

    switch_snprintf(msg, sizeof(msg), "digits/%d.wav", abs(member->volume_in_level));
    conference_member_play_file(member, msg, 0, SWITCH_TRUE);
}

static void conference_loop_fn_event(conference_member_t *member, caller_control_action_t *action)
{
	switch_event_t *event;
	if (test_eflag(member->conference, EFLAG_DTMF) && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_member_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "dtmf");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "DTMF-Key", action->binded_dtmf);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Data", action->expanded_data);
		switch_event_fire(&event);
	}
}

static void conference_loop_fn_transfer(conference_member_t *member, caller_control_action_t *action)
{
	char *exten = NULL;
	char *dialplan = "XML";
	char *context = "default";

	char *argv[3] = { 0 };
	int argc;
	char *mydata = NULL;
	switch_event_t *event;

	if (test_eflag(member->conference, EFLAG_DTMF) && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_member_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "transfer");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Dialplan", action->expanded_data);
		switch_event_fire(&event);
	}
	member_clear_flag_locked(member, MFLAG_RUNNING);

	if ((mydata = switch_core_session_strdup(member->session, action->expanded_data))) {
		if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
			if (argc > 0) {
				exten = argv[0];
			}
			if (argc > 1) {
				dialplan = argv[1];
			}
			if (argc > 2) {
				context = argv[2];
			}

		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_ERROR, "Empty transfer string [%s]\n", (char *) action->expanded_data);
			goto done;
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_ERROR, "Unable to allocate memory to duplicate transfer data.\n");
		goto done;
	}
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_DEBUG, "Transfering to: %s, %s, %s\n", exten, dialplan, context);

	switch_ivr_session_transfer(member->session, exten, dialplan, context);

  done:
	return;
}

static void conference_loop_fn_exec_app(conference_member_t *member, caller_control_action_t *action)
{
	char *app = NULL;
	char *arg = "";

	char *argv[2] = { 0 };
	int argc;
	char *mydata = NULL;
	switch_event_t *event = NULL;
	switch_channel_t *channel = NULL;

	if (!action->expanded_data) return;

	if (test_eflag(member->conference, EFLAG_DTMF) && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_member_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "execute_app");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Application", action->expanded_data);
		switch_event_fire(&event);
	}

	mydata = strdup(action->expanded_data);
	switch_assert(mydata);

	if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		if (argc > 0) {
			app = argv[0];
		}
		if (argc > 1) {
			arg = argv[1];
		}

	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_ERROR, "Empty execute app string [%s]\n", 
						  (char *) action->expanded_data);
		goto done;
	}

	if (!app) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_ERROR, "Unable to find application.\n");
		goto done;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_DEBUG, "Execute app: %s, %s\n", app, arg);

	channel = switch_core_session_get_channel(member->session);

	switch_channel_set_app_flag(channel, CF_APP_TAGGED);
	switch_core_session_set_read_codec(member->session, NULL);
	switch_core_session_execute_application(member->session, app, arg);
	switch_core_session_set_read_codec(member->session, &member->read_codec);
	switch_channel_clear_app_flag(channel, CF_APP_TAGGED);

  done:

	switch_safe_free(mydata);

	return;
}

static void conference_loop_fn_hangup(conference_member_t *member, caller_control_action_t *action)
{
	member_clear_flag_locked(member, MFLAG_RUNNING);
}


static int noise_gate_check(conference_member_t *member)
{
	int r = 0;


	if (member->conference->agc_level && member->agc_volume_in_level != 0) {
		int target_score = 0;

		target_score = (member->energy_level + (25 * member->agc_volume_in_level));

		if (target_score < 0) target_score = 0;

		r = (int)member->score > target_score;
		
	} else {
		r = (int32_t)member->score > member->energy_level;
	}

	return r;
}

static void clear_avg(conference_member_t *member)
{

	member->avg_score = 0;
	member->avg_itt = 0;
	member->avg_tally = 0;
	member->agc_concur = 0;
}

static void check_agc_levels(conference_member_t *member)
{
	int x = 0;

	if (!member->avg_score) return;
	
	if ((int)member->avg_score < member->conference->agc_level - 100) {
		member->agc_volume_in_level++;
		switch_normalize_volume_granular(member->agc_volume_in_level);
		x = 1;
	} else if ((int)member->avg_score > member->conference->agc_level + 100) {
		member->agc_volume_in_level--;
		switch_normalize_volume_granular(member->agc_volume_in_level);
		x = -1;
	}

	if (x) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG7,
						  "AGC %s:%d diff:%d level:%d cur:%d avg:%d vol:%d %s\n", 
						  member->conference->name,
						  member->id, member->conference->agc_level - member->avg_score, member->conference->agc_level, 
						  member->score, member->avg_score, member->agc_volume_in_level, x > 0 ? "+++" : "---");
		
		clear_avg(member);
	}
}

static void member_check_channels(switch_frame_t *frame, conference_member_t *member, switch_bool_t in)
{
	if (member->conference->channels != member->read_impl.number_of_channels || member_test_flag(member, MFLAG_POSITIONAL)) {
		uint32_t rlen;
		int from, to;

		if (in) {
			to = member->conference->channels;
			from = member->read_impl.number_of_channels;
		} else {
			from = member->conference->channels;
			to = member->read_impl.number_of_channels;
		}

		rlen = frame->datalen / 2 / from; 

		if (in && frame->rate == 48000 && ((from == 1 && to == 2) || (from == 2 && to == 2)) && member_test_flag(member, MFLAG_POSITIONAL)) {
			if (from == 2 && to == 2) {
				switch_mux_channels((int16_t *) frame->data, rlen, 2, 1);
				frame->datalen /= 2;
				rlen = frame->datalen / 2;
			}
			
			process_al(member->al, frame->data, frame->datalen, frame->rate);
		} else {
			switch_mux_channels((int16_t *) frame->data, rlen, from, to);
		}
		
		frame->datalen = rlen * 2 * to;
		
	}
}

/* marshall frames from the call leg to the conference thread for muxing to other call legs */
static void *SWITCH_THREAD_FUNC conference_loop_input(switch_thread_t *thread, void *obj)
{
    switch_event_t *event;
	conference_member_t *member = obj;
	switch_channel_t *channel;
	switch_status_t status;
	switch_frame_t *read_frame = NULL;
	uint32_t hangover = 40, hangunder = 5, hangover_hits = 0, hangunder_hits = 0, diff_level = 400;
	switch_core_session_t *session = member->session;
	uint32_t flush_len, loops = 0;
	switch_frame_t tmp_frame = { 0 };

	if (switch_core_session_read_lock(session) != SWITCH_STATUS_SUCCESS) {
		goto end;
	}

	switch_assert(member != NULL);

	member_clear_flag_locked(member, MFLAG_TALKING);

	channel = switch_core_session_get_channel(session);

	switch_core_session_get_read_impl(session, &member->read_impl);

	switch_channel_audio_sync(channel);

	flush_len = switch_samples_per_packet(member->conference->rate, member->conference->interval) * 2 * member->conference->channels * (500 / member->conference->interval);

	/* As long as we have a valid read, feed that data into an input buffer where the conference thread will take it 
	   and mux it with any audio from other channels. */

	while (member_test_flag(member, MFLAG_RUNNING) && switch_channel_ready(channel)) {

		if (switch_channel_ready(channel) && switch_channel_test_app_flag(channel, CF_APP_TAGGED)) {
			switch_yield(100000);
			continue;
		}

		/* Read a frame. */
		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

		switch_mutex_lock(member->read_mutex);

		/* end the loop, if appropriate */
		if (!SWITCH_READ_ACCEPTABLE(status) || !member_test_flag(member, MFLAG_RUNNING)) {
			switch_mutex_unlock(member->read_mutex);
			break;
		}

		if (switch_channel_test_flag(channel, CF_VIDEO) && !member_test_flag(member, MFLAG_ACK_VIDEO)) {
			member_set_flag_locked(member, MFLAG_ACK_VIDEO);
			check_avatar(member, SWITCH_FALSE);
			switch_core_session_video_reinit(member->session);
			conference_set_video_floor_holder(member->conference, member, SWITCH_FALSE);
		} else if (member_test_flag(member, MFLAG_ACK_VIDEO) && !switch_channel_test_flag(channel, CF_VIDEO)) {
			check_avatar(member, SWITCH_FALSE);
		}

		/* if we have caller digits, feed them to the parser to find an action */
		if (switch_channel_has_dtmf(channel)) {
			char dtmf[128] = "";
		
			switch_channel_dequeue_dtmf_string(channel, dtmf, sizeof(dtmf));

			if (member_test_flag(member, MFLAG_DIST_DTMF)) {
				conference_send_all_dtmf(member, member->conference, dtmf);
			} else if (member->dmachine) {
				char *p;
				char str[2] = "";
				for (p = dtmf; p && *p; p++) {
					str[0] = *p;
					switch_ivr_dmachine_feed(member->dmachine, str, NULL);
				}
			}
		} else if (member->dmachine) {
			switch_ivr_dmachine_ping(member->dmachine, NULL);
		}
		
		if (switch_queue_size(member->dtmf_queue)) {
			switch_dtmf_t *dt;
			void *pop;
			
			if (switch_queue_trypop(member->dtmf_queue, &pop) == SWITCH_STATUS_SUCCESS) {
				dt = (switch_dtmf_t *) pop;
				switch_core_session_send_dtmf(member->session, dt);
				free(dt);
			}
		}
				
		if (switch_test_flag(read_frame, SFF_CNG)) {
			if (member->conference->agc_level) {
				member->nt_tally++;
			}

			if (hangunder_hits) {
				hangunder_hits--;
			}
			if (member_test_flag(member, MFLAG_TALKING)) {
				if (++hangover_hits >= hangover) {
					hangover_hits = hangunder_hits = 0;
					member_clear_flag_locked(member, MFLAG_TALKING);
					member_update_status_field(member);
					check_agc_levels(member);
					clear_avg(member);
					member->score_iir = 0;
					member->floor_packets = 0;

					if (test_eflag(member->conference, EFLAG_STOP_TALKING) &&
						switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
						conference_add_event_member_data(member, event);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "stop-talking");
						switch_event_fire(&event);
					}
				}
			}

			goto do_continue;
		}

		if (member->nt_tally > (int32_t)(member->read_impl.actual_samples_per_second / member->read_impl.samples_per_packet) * 3) {
			member->agc_volume_in_level = 0;
			clear_avg(member);
		}

		/* Check for input volume adjustments */
		if (!member->conference->agc_level) {
			member->conference->agc_level = 0;
			clear_avg(member);
		}
		

		/* if the member can speak, compute the audio energy level and */
		/* generate events when the level crosses the threshold        */
		if ((member_test_flag(member, MFLAG_CAN_SPEAK) || member_test_flag(member, MFLAG_MUTE_DETECT))) {
			uint32_t energy = 0, i = 0, samples = 0, j = 0;
			int16_t *data;
			int agc_period = (member->read_impl.actual_samples_per_second / member->read_impl.samples_per_packet) / 4;
			

			data = read_frame->data;
			member->score = 0;

			if (member->volume_in_level) {
				switch_change_sln_volume(read_frame->data, (read_frame->datalen / 2) * member->conference->channels, member->volume_in_level);
			}
			
			if (member->agc_volume_in_level) {
				switch_change_sln_volume_granular(read_frame->data, (read_frame->datalen / 2) * member->conference->channels, member->agc_volume_in_level);
			}
			
			if ((samples = read_frame->datalen / sizeof(*data) / member->read_impl.number_of_channels)) {
				for (i = 0; i < samples; i++) {
					energy += abs(data[j]);
					j += member->read_impl.number_of_channels;
				}
				
				member->score = energy / samples;
			}

			if (member->vol_period) {
				member->vol_period--;
			}
			
			if (member->conference->agc_level && member->score && 
				member_test_flag(member, MFLAG_CAN_SPEAK) &&
				noise_gate_check(member)
				) {
				int last_shift = abs((int)(member->last_score - member->score));
				
				if (member->score && member->last_score && last_shift > 900) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG7,
									  "AGC %s:%d drop anomalous shift of %d\n", 
									  member->conference->name,
									  member->id, last_shift);

				} else {
					member->avg_tally += member->score;
					member->avg_itt++;
					if (!member->avg_itt) member->avg_itt++;
					member->avg_score = member->avg_tally / member->avg_itt;
				}

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG7,
								  "AGC %s:%d diff:%d level:%d cur:%d avg:%d vol:%d\n", 
								  member->conference->name,
								  member->id, member->conference->agc_level - member->avg_score, member->conference->agc_level, 
								  member->score, member->avg_score, member->agc_volume_in_level);
				
				if (++member->agc_concur >= agc_period) {
					if (!member->vol_period) {
						check_agc_levels(member);
					}
					member->agc_concur = 0;
				}
			} else {
				member->nt_tally++;
			}

			member->score_iir = (int) (((1.0 - SCORE_DECAY) * (float) member->score) + (SCORE_DECAY * (float) member->score_iir));

			if (member->score_iir > SCORE_MAX_IIR) {
				member->score_iir = SCORE_MAX_IIR;
			}
			
			if (noise_gate_check(member)) {
				uint32_t diff = member->score - member->energy_level;
				if (hangover_hits) {
					hangover_hits--;
				}

				if (member->conference->agc_level) {
					member->nt_tally = 0;
				}

				if (member == member->conference->floor_holder) {
					member->floor_packets++;
				}

				if (diff >= diff_level || ++hangunder_hits >= hangunder) { 

					hangover_hits = hangunder_hits = 0;
					member->last_talking = switch_epoch_time_now(NULL);

					if (!member_test_flag(member, MFLAG_TALKING)) {
						member_set_flag_locked(member, MFLAG_TALKING);
						member_update_status_field(member);
						member->floor_packets = 0;

						if (test_eflag(member->conference, EFLAG_START_TALKING) && member_test_flag(member, MFLAG_CAN_SPEAK) &&
							switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
							conference_add_event_member_data(member, event);
							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "start-talking");
							switch_event_fire(&event);
						}

						if (member_test_flag(member, MFLAG_MUTE_DETECT) && !member_test_flag(member, MFLAG_CAN_SPEAK)) {

							if (!zstr(member->conference->mute_detect_sound)) {
								member_set_flag(member, MFLAG_INDICATE_MUTE_DETECT);
							}

							if (test_eflag(member->conference, EFLAG_MUTE_DETECT) &&
								switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
								conference_add_event_member_data(member, event);
								switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "mute-detect");
								switch_event_fire(&event);
							}
						}
					}
				}
			} else {
				if (hangunder_hits) {
					hangunder_hits--;
				}

				if (member->conference->agc_level) {
					member->nt_tally++;
				}

				if (member_test_flag(member, MFLAG_TALKING) && member_test_flag(member, MFLAG_CAN_SPEAK)) {
					switch_event_t *event;
					if (++hangover_hits >= hangover) {
						hangover_hits = hangunder_hits = 0;
						member_clear_flag_locked(member, MFLAG_TALKING);
						member_update_status_field(member);
						check_agc_levels(member);
						clear_avg(member);
						
						if (test_eflag(member->conference, EFLAG_STOP_TALKING) &&
							switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
							conference_add_event_member_data(member, event);
							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "stop-talking");
							switch_event_fire(&event);
						}
					}
				}
			}


			member->last_score = member->score;

			if (member == member->conference->floor_holder) {
				if (member->id != member->conference->video_floor_holder && 
					(member->floor_packets > member->conference->video_floor_packets || member->energy_level == 0)) {
					conference_set_video_floor_holder(member->conference, member, SWITCH_FALSE);
				}
			}
		}

		loops++;

		if (switch_channel_test_flag(member->channel, CF_CONFERENCE_RESET_MEDIA)) {
			switch_channel_clear_flag(member->channel, CF_CONFERENCE_RESET_MEDIA);

			if (loops > 500) {
				member->loop_loop = 1;

				if (setup_media(member, member->conference)) {
					switch_mutex_unlock(member->read_mutex);
					break;
				}
			}

		}

		/* skip frames that are not actual media or when we are muted or silent */
		if ((member_test_flag(member, MFLAG_TALKING) || member->energy_level == 0 || conference_test_flag(member->conference, CFLAG_AUDIO_ALWAYS)) 
			&& member_test_flag(member, MFLAG_CAN_SPEAK) &&	!conference_test_flag(member->conference, CFLAG_WAIT_MOD)
			&& (member->conference->count > 1 || (member->conference->record_count && member->conference->count >= member->conference->min_recording_participants))) {
			switch_audio_resampler_t *read_resampler = member->read_resampler;
			void *data;
			uint32_t datalen;

			if (read_resampler) {
				int16_t *bptr = (int16_t *) read_frame->data;
				int len = (int) read_frame->datalen;

				switch_resample_process(read_resampler, bptr, len / 2 / member->read_impl.number_of_channels);
				memcpy(member->resample_out, read_resampler->to, read_resampler->to_len * 2 * member->read_impl.number_of_channels);
				len = read_resampler->to_len * 2 * member->read_impl.number_of_channels;
				datalen = len;
				data = member->resample_out;
			} else {
				data = read_frame->data;
				datalen = read_frame->datalen;
			}

			tmp_frame.data = data;
			tmp_frame.datalen = datalen;
			tmp_frame.rate = member->conference->rate;
			member_check_channels(&tmp_frame, member, SWITCH_TRUE);
			

			if (datalen) {
				switch_size_t ok = 1;

				/* Write the audio into the input buffer */
				switch_mutex_lock(member->audio_in_mutex);
				if (switch_buffer_inuse(member->audio_buffer) > flush_len) {
					switch_buffer_toss(member->audio_buffer, tmp_frame.datalen);
				}
				ok = switch_buffer_write(member->audio_buffer, tmp_frame.data, tmp_frame.datalen);
				switch_mutex_unlock(member->audio_in_mutex);
				if (!ok) {
					switch_mutex_unlock(member->read_mutex);
					break;
				}
			}
		}

	  do_continue:

		switch_mutex_unlock(member->read_mutex);

	}

	if (switch_queue_size(member->dtmf_queue)) {
		switch_dtmf_t *dt;
		void *pop;

		while (switch_queue_trypop(member->dtmf_queue, &pop) == SWITCH_STATUS_SUCCESS) {
			dt = (switch_dtmf_t *) pop;
			free(dt);
		}
	}


	switch_resample_destroy(&member->read_resampler);
	switch_core_session_rwunlock(session);

 end:

	member_clear_flag_locked(member, MFLAG_ITHREAD);

	return NULL;
}


static void member_add_file_data(conference_member_t *member, int16_t *data, switch_size_t file_data_len)
{
	switch_size_t file_sample_len;
	int16_t file_frame[SWITCH_RECOMMENDED_BUFFER_SIZE] = { 0 };


	switch_mutex_lock(member->fnode_mutex);

	if (!member->fnode) {
		goto done;
	}

	file_sample_len = file_data_len / 2 / member->conference->channels;

	/* if we are done, clean it up */
	if (member->fnode->done) {
		conference_file_node_t *fnode;
		switch_memory_pool_t *pool;

		if (member->fnode->type != NODE_TYPE_SPEECH) {
			conference_file_close(member->conference, member->fnode);
		}

		fnode = member->fnode;
		member->fnode = member->fnode->next;

		pool = fnode->pool;
		fnode = NULL;
		switch_core_destroy_memory_pool(&pool);
	} else if(!switch_test_flag(member->fnode, NFLAG_PAUSE)) {
		/* skip this frame until leadin time has expired */
		if (member->fnode->leadin) {
			member->fnode->leadin--;
		} else {
			if (member->fnode->type == NODE_TYPE_SPEECH) {
				switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_BLOCKING;
				switch_size_t speech_len = file_data_len;

				if (member->fnode->al) {
					speech_len /= 2;
				}
				
				if (switch_core_speech_read_tts(member->fnode->sh, file_frame, &speech_len, &flags) == SWITCH_STATUS_SUCCESS) {
					file_sample_len = file_data_len / 2 / member->conference->channels;					
				} else {
					file_sample_len = file_data_len = 0;
				}
			} else if (member->fnode->type == NODE_TYPE_FILE) {
				switch_core_file_read(&member->fnode->fh, file_frame, &file_sample_len);
				file_data_len = file_sample_len * 2 * member->fnode->fh.channels;
			}

			if (file_sample_len <= 0) {
				member->fnode->done++;
			} else {			/* there is file node data to mix into the frame */
				uint32_t i;
				int32_t sample;

				/* Check for output volume adjustments */
				if (member->volume_out_level) {
					switch_change_sln_volume(file_frame, (uint32_t)file_sample_len * member->conference->channels, member->volume_out_level);
				}

				if (member->fnode->al) {
					process_al(member->fnode->al, file_frame, file_sample_len * 2, member->conference->rate);
				}

				for (i = 0; i < (int)file_sample_len * member->conference->channels; i++) {
					if (member->fnode->mux) {
						sample = data[i] + file_frame[i];
						switch_normalize_to_16bit(sample);
						data[i] = (int16_t)sample;
					} else {
						data[i] = file_frame[i];
					}
				}

			}
		}
	}

 done:

	switch_mutex_unlock(member->fnode_mutex);
}



/* launch an input thread for the call leg */
static void launch_conference_loop_input(conference_member_t *member, switch_memory_pool_t *pool)
{
	switch_threadattr_t *thd_attr = NULL;

	if (member == NULL || member->input_thread)
		return;

	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	member_set_flag_locked(member, MFLAG_ITHREAD);
	if (switch_thread_create(&member->input_thread, thd_attr, conference_loop_input, member, pool) != SWITCH_STATUS_SUCCESS) {
		member_clear_flag_locked(member, MFLAG_ITHREAD);
	}
}

/* marshall frames from the conference (or file or tts output) to the call leg */
/* NB. this starts the input thread after some initial setup for the call leg */
static void conference_loop_output(conference_member_t *member)
{
	switch_channel_t *channel;
	switch_frame_t write_frame = { 0 };
	uint8_t *data = NULL;
	switch_timer_t timer = { 0 };
	uint32_t interval;
	uint32_t samples;
	//uint32_t csamples;
	uint32_t tsamples;
	uint32_t flush_len;
	uint32_t low_count, bytes;
	call_list_t *call_list, *cp;
	switch_codec_implementation_t read_impl = { 0 };
	int sanity;
	switch_status_t st;

	switch_core_session_get_read_impl(member->session, &read_impl);


	channel = switch_core_session_get_channel(member->session);
	interval = read_impl.microseconds_per_packet / 1000;
	samples = switch_samples_per_packet(member->conference->rate, interval);
	//csamples = samples;
	tsamples = member->orig_read_impl.samples_per_packet;
	low_count = 0;
	bytes = samples * 2 * member->conference->channels;
	call_list = NULL;
	cp = NULL;

	member->loop_loop = 0;

	switch_assert(member->conference != NULL);

	flush_len = switch_samples_per_packet(member->conference->rate, member->conference->interval) * 2 * member->conference->channels * (500 / member->conference->interval);

	if (switch_core_timer_init(&timer, member->conference->timer_name, interval, tsamples, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_ERROR, "Timer Setup Failed.  Conference Cannot Start\n");
		return;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_DEBUG, "Setup timer %s success interval: %u  samples: %u\n",
					  member->conference->timer_name, interval, tsamples);

	
	write_frame.data = data = switch_core_session_alloc(member->session, SWITCH_RECOMMENDED_BUFFER_SIZE);
	write_frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;


	write_frame.codec = &member->write_codec;

	/* Start the input thread */
	launch_conference_loop_input(member, switch_core_session_get_pool(member->session));

	if ((call_list = switch_channel_get_private(channel, "_conference_autocall_list_"))) {
		const char *cid_name = switch_channel_get_variable(channel, "conference_auto_outcall_caller_id_name");
		const char *cid_num = switch_channel_get_variable(channel, "conference_auto_outcall_caller_id_number");
		const char *toval = switch_channel_get_variable(channel, "conference_auto_outcall_timeout");
		const char *flags = switch_channel_get_variable(channel, "conference_auto_outcall_flags");
		const char *profile = switch_channel_get_variable(channel, "conference_auto_outcall_profile");
		const char *ann = switch_channel_get_variable(channel, "conference_auto_outcall_announce");
		const char *prefix = switch_channel_get_variable(channel, "conference_auto_outcall_prefix");
		const char *maxwait = switch_channel_get_variable(channel, "conference_auto_outcall_maxwait");
		const char *delimiter_val = switch_channel_get_variable(channel, "conference_auto_outcall_delimiter");
		int to = 60;
		int wait_sec = 2;
		int loops = 0;

		if (ann && !switch_channel_test_app_flag_key("conf_silent", channel, CONF_SILENT_REQ)) {
			member->conference->special_announce = switch_core_strdup(member->conference->pool, ann);
		}

		switch_channel_set_private(channel, "_conference_autocall_list_", NULL);

		conference_set_flag(member->conference, CFLAG_OUTCALL);

		if (toval) {
			to = atoi(toval);
			if (to < 10 || to > 500) {
				to = 60;
			}
		}

		for (cp = call_list; cp; cp = cp->next) {
			int argc;
			char *argv[512] = { 0 };
			char *cpstr = strdup(cp->string);
			int x = 0;

			switch_assert(cpstr);
			if (!zstr(delimiter_val) && strlen(delimiter_val) == 1) {
				char delimiter = *delimiter_val;
				argc = switch_separate_string(cpstr, delimiter, argv, (sizeof(argv) / sizeof(argv[0])));
			} else {
				argc = switch_separate_string(cpstr, ',', argv, (sizeof(argv) / sizeof(argv[0])));
			}
			for (x = 0; x < argc; x++) {
				char *dial_str = switch_mprintf("%s%s", switch_str_nil(prefix), argv[x]);
				switch_assert(dial_str);
				conference_outcall_bg(member->conference, NULL, NULL, dial_str, to, switch_str_nil(flags), cid_name, cid_num, NULL, 
									  profile, &member->conference->cancel_cause, NULL);
				switch_safe_free(dial_str);
			}
			switch_safe_free(cpstr);
		}

		if (maxwait) {
			int tmp = atoi(maxwait);
			if (tmp > 0) {
				wait_sec = tmp;
			}
		}


		loops = wait_sec * 10;
		
		switch_channel_set_app_flag(channel, CF_APP_TAGGED);
		do {
			switch_ivr_sleep(member->session, 100, SWITCH_TRUE, NULL);
		} while(switch_channel_up(channel) && (member->conference->originating && --loops));
		switch_channel_clear_app_flag(channel, CF_APP_TAGGED);

		if (!switch_channel_ready(channel)) {
			member->conference->cancel_cause = SWITCH_CAUSE_ORIGINATOR_CANCEL;
			goto end;
		}
			
		conference_member_play_file(member, "tone_stream://%(500,0,640)", 0, SWITCH_TRUE);
	}
	
	if (!conference_test_flag(member->conference, CFLAG_ANSWERED)) {
		switch_channel_answer(channel);
	}


	sanity = 2000;
	while(!member_test_flag(member, MFLAG_ITHREAD) && sanity > 0) {
		switch_cond_next();
		sanity--;
	}

	/* Fair WARNING, If you expect the caller to hear anything or for digit handling to be processed,      */
	/* you better not block this thread loop for more than the duration of member->conference->timer_name!  */
	while (!member->loop_loop && member_test_flag(member, MFLAG_RUNNING) && member_test_flag(member, MFLAG_ITHREAD)
		   && switch_channel_ready(channel)) {
		switch_event_t *event;
		int use_timer = 0;
		switch_buffer_t *use_buffer = NULL;
		uint32_t mux_used = 0;

		switch_mutex_lock(member->write_mutex);

		
		if (switch_channel_test_flag(member->channel, CF_CONFERENCE_ADV)) {
			if (member->conference->la) {
				adv_la(member->conference, member, SWITCH_TRUE);
			}
			switch_channel_clear_flag(member->channel, CF_CONFERENCE_ADV);
		}


		if (switch_core_session_dequeue_event(member->session, &event, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
			if (event->event_id == SWITCH_EVENT_MESSAGE) {
				char *from = switch_event_get_header(event, "from");
				char *to = switch_event_get_header(event, "to");
				char *body = switch_event_get_body(event);
				
				if (to && from && body) {
					if (strchr(to, '+') && strncmp(to, CONF_CHAT_PROTO, strlen(CONF_CHAT_PROTO))) {
						switch_event_del_header(event, "to");
						switch_event_add_header(event, SWITCH_STACK_BOTTOM,
												"to", "%s+%s@%s", CONF_CHAT_PROTO, member->conference->name, member->conference->domain);
					} else {
						switch_event_del_header(event, "to");
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "to", "%s", member->conference->name);
					}
					chat_send(event);
				}
			}
			switch_event_destroy(&event);
		}

		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			/* test to see if outbound channel has answered */
			if (switch_channel_test_flag(channel, CF_ANSWERED) && !conference_test_flag(member->conference, CFLAG_ANSWERED)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_DEBUG,
								  "Outbound conference channel answered, setting CFLAG_ANSWERED\n");
				conference_set_flag(member->conference, CFLAG_ANSWERED);
			}
		} else {
			if (conference_test_flag(member->conference, CFLAG_ANSWERED) && !switch_channel_test_flag(channel, CF_ANSWERED)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_DEBUG, "CLFAG_ANSWERED set, answering inbound channel\n");
				switch_channel_answer(channel);
			}
		}

		use_buffer = NULL;
		mux_used = (uint32_t) switch_buffer_inuse(member->mux_buffer);
		
		use_timer = 1;

		if (mux_used) {
			if (mux_used < bytes) {
				if (++low_count >= 5) {
					/* partial frame sitting around this long is useless and builds delay */
					member_set_flag_locked(member, MFLAG_FLUSH_BUFFER);
				}
			} else if (mux_used > flush_len) {
				/* getting behind, clear the buffer */
				member_set_flag_locked(member, MFLAG_FLUSH_BUFFER);
			}
		}

		if (switch_channel_test_app_flag(channel, CF_APP_TAGGED)) {
			member_set_flag_locked(member, MFLAG_FLUSH_BUFFER);
		} else if (mux_used >= bytes) {
			/* Flush the output buffer and write all the data (presumably muxed) back to the channel */
			switch_mutex_lock(member->audio_out_mutex);
			write_frame.data = data;
			use_buffer = member->mux_buffer;
			low_count = 0;

			if ((write_frame.datalen = (uint32_t) switch_buffer_read(use_buffer, write_frame.data, bytes))) {
				if (write_frame.datalen) {
					write_frame.samples = write_frame.datalen / 2 / member->conference->channels;
				   
				   if( !member_test_flag(member, MFLAG_CAN_HEAR)) {
				      memset(write_frame.data, 255, write_frame.datalen);
				   } else if (member->volume_out_level) { /* Check for output volume adjustments */
					   switch_change_sln_volume(write_frame.data, write_frame.samples * member->conference->channels, member->volume_out_level);
				   }

				   write_frame.timestamp = timer.samplecount;

				   if (member->fnode) {
					   member_add_file_data(member, write_frame.data, write_frame.datalen);
				   }

				   member_check_channels(&write_frame, member, SWITCH_FALSE);
				   
				   if (switch_core_session_write_frame(member->session, &write_frame, SWITCH_IO_FLAG_NONE, 0) != SWITCH_STATUS_SUCCESS) {
					   switch_mutex_unlock(member->audio_out_mutex);
					   break;
				   }
				}
			}

			switch_mutex_unlock(member->audio_out_mutex);
		}

		if (member_test_flag(member, MFLAG_FLUSH_BUFFER)) {
			if (switch_buffer_inuse(member->mux_buffer)) {
				switch_mutex_lock(member->audio_out_mutex);
				switch_buffer_zero(member->mux_buffer);
				switch_mutex_unlock(member->audio_out_mutex);
			}
			member_clear_flag_locked(member, MFLAG_FLUSH_BUFFER);
		}

		switch_mutex_unlock(member->write_mutex);


		if (member_test_flag(member, MFLAG_INDICATE_MUTE)) {
			if (!zstr(member->conference->muted_sound)) {
				conference_member_play_file(member, member->conference->muted_sound, 0, SWITCH_TRUE);
			} else {
				char msg[512];
				
				switch_snprintf(msg, sizeof(msg), "Muted");
				conference_member_say(member, msg, 0);
			}
			member_clear_flag(member, MFLAG_INDICATE_MUTE);
		}

		if (member_test_flag(member, MFLAG_INDICATE_MUTE_DETECT)) {
			if (!zstr(member->conference->mute_detect_sound)) {
				conference_member_play_file(member, member->conference->mute_detect_sound, 0, SWITCH_TRUE);
			} else {
				char msg[512];
				
				switch_snprintf(msg, sizeof(msg), "Currently Muted");
				conference_member_say(member, msg, 0);
			}
			member_clear_flag(member, MFLAG_INDICATE_MUTE_DETECT);
		}
		
		if (member_test_flag(member, MFLAG_INDICATE_UNMUTE)) {
			if (!zstr(member->conference->unmuted_sound)) {
				conference_member_play_file(member, member->conference->unmuted_sound, 0, SWITCH_TRUE);
			} else {
				char msg[512];
				
				switch_snprintf(msg, sizeof(msg), "Un-Muted");
				conference_member_say(member, msg, 0);
			}
			member_clear_flag(member, MFLAG_INDICATE_UNMUTE);
		}

		if (switch_core_session_private_event_count(member->session)) {
			switch_channel_set_app_flag(channel, CF_APP_TAGGED);
			switch_ivr_parse_all_events(member->session);
			switch_channel_clear_app_flag(channel, CF_APP_TAGGED);
			member_set_flag_locked(member, MFLAG_FLUSH_BUFFER);
			switch_core_session_set_read_codec(member->session, &member->read_codec);
		} else {
			switch_ivr_parse_all_messages(member->session);
		}

		if (use_timer) {
			switch_core_timer_next(&timer);
		} else {
			switch_cond_next();
		}

	} /* Rinse ... Repeat */

 end:

	if (!member->loop_loop) {
		member_clear_flag_locked(member, MFLAG_RUNNING);

		/* Wait for the input thread to end */
		if (member->input_thread) {
			switch_thread_join(&st, member->input_thread);
			member->input_thread = NULL;
		}
	}

	switch_core_timer_destroy(&timer);

	if (member->loop_loop) {
		return;
	}

	switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(channel), SWITCH_LOG_DEBUG, "Channel leaving conference, cause: %s\n",
					  switch_channel_cause2str(switch_channel_get_cause(channel)));

	/* if it's an outbound channel, store the release cause in the conference struct, we might need it */
	if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
		member->conference->bridge_hangup_cause = switch_channel_get_cause(channel);
	}
}

/* Sub-Routine called by a record entity inside a conference */
static void *SWITCH_THREAD_FUNC conference_record_thread_run(switch_thread_t *thread, void *obj)
{
	int16_t *data_buf;
	conference_member_t smember = { 0 }, *member;
	conference_record_t *rp, *last = NULL, *rec = (conference_record_t *) obj;
	conference_obj_t *conference = rec->conference;
	uint32_t samples = switch_samples_per_packet(conference->rate, conference->interval);
	uint32_t mux_used;
	char *vval;
	switch_timer_t timer = { 0 };
	uint32_t rlen;
	switch_size_t data_buf_len;
	switch_event_t *event;
	switch_size_t len = 0;
	int flags = 0;

	data_buf_len = samples * sizeof(int16_t);

	switch_zmalloc(data_buf, data_buf_len);

	if (switch_thread_rwlock_tryrdlock(conference->rwlock) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Read Lock Fail\n");
		return NULL;
	}

	data_buf_len = samples * sizeof(int16_t) * conference->channels;
	switch_zmalloc(data_buf, data_buf_len);

	switch_mutex_lock(globals.hash_mutex);
	globals.threads++;
	switch_mutex_unlock(globals.hash_mutex);

	member = &smember;

	member->flags[MFLAG_CAN_HEAR] = member->flags[MFLAG_NOCHANNEL] = member->flags[MFLAG_RUNNING] = 1;

	member->conference = conference;
	member->native_rate = conference->rate;
	member->rec = rec;
	member->rec_path = rec->path;
	member->rec_time = switch_epoch_time_now(NULL);
	member->rec->fh.channels = 1;
	member->rec->fh.samplerate = conference->rate;
	member->id = next_member_id();
	member->pool = rec->pool;

	member->frame_size = SWITCH_RECOMMENDED_BUFFER_SIZE;
	member->frame = switch_core_alloc(member->pool, member->frame_size);
	member->mux_frame = switch_core_alloc(member->pool, member->frame_size);


	switch_mutex_init(&member->write_mutex, SWITCH_MUTEX_NESTED, rec->pool);
	switch_mutex_init(&member->flag_mutex, SWITCH_MUTEX_NESTED, rec->pool);
	switch_mutex_init(&member->fnode_mutex, SWITCH_MUTEX_NESTED, rec->pool);
	switch_mutex_init(&member->audio_in_mutex, SWITCH_MUTEX_NESTED, rec->pool);
	switch_mutex_init(&member->audio_out_mutex, SWITCH_MUTEX_NESTED, rec->pool);
	switch_mutex_init(&member->read_mutex, SWITCH_MUTEX_NESTED, rec->pool);
	switch_thread_rwlock_create(&member->rwlock, rec->pool);

	/* Setup an audio buffer for the incoming audio */
	if (switch_buffer_create_dynamic(&member->audio_buffer, CONF_DBLOCK_SIZE, CONF_DBUFFER_SIZE, 0) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error Creating Audio Buffer!\n");
		goto end;
	}

	/* Setup an audio buffer for the outgoing audio */
	if (switch_buffer_create_dynamic(&member->mux_buffer, CONF_DBLOCK_SIZE, CONF_DBUFFER_SIZE, 0) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error Creating Audio Buffer!\n");
		goto end;
	}

	if (conference->canvas) {
		conference->canvas->send_keyframe = 1;
	}

	member->rec->fh.pre_buffer_datalen = SWITCH_DEFAULT_FILE_BUFFER_LEN;

	flags = SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_DATA_SHORT;

	if (conference->members_with_video && conference_test_flag(conference, CFLAG_TRANSCODE_VIDEO)) {
		flags |= SWITCH_FILE_FLAG_VIDEO;
		if (conference->canvas) {
			char *orig_path = rec->path;
			rec->path = switch_core_sprintf(rec->pool, "{channels=%d,samplerate=%d,vw=%d,vh=%d,fps=%0.2f}%s", 
											conference->channels,
											conference->rate,
											conference->canvas->width,
											conference->canvas->height,
											conference->video_fps.fps,
											orig_path);
		}
	}

	if (switch_core_file_open(&member->rec->fh, rec->path, (uint8_t) conference->channels, conference->rate, flags, rec->pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening File [%s]\n", rec->path);

		if (test_eflag(conference, EFLAG_RECORD) &&
			switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_add_event_data(conference, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "start-recording");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Path", rec->path);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Error", "File could not be opened for recording");
			switch_event_fire(&event);
		}

		goto end;
	}

	switch_mutex_lock(conference->mutex);
	if (conference->video_floor_holder) {
		conference_member_t *member;
		if ((member = conference_member_get(conference, conference->video_floor_holder))) {
			if (member->session) {
				switch_core_session_video_reinit(member->session);
			}
			switch_thread_rwlock_unlock(member->rwlock);
		}
	}
	switch_mutex_unlock(conference->mutex);

	if (switch_core_timer_init(&timer, conference->timer_name, conference->interval, samples, rec->pool) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Setup timer success interval: %u  samples: %u\n", conference->interval, samples);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Timer Setup Failed.  Conference Cannot Start\n");
		goto end;
	}

	if ((vval = switch_mprintf("Conference %s", conference->name))) {
		switch_core_file_set_string(&member->rec->fh, SWITCH_AUDIO_COL_STR_TITLE, vval);
		switch_safe_free(vval);
	}

	switch_core_file_set_string(&member->rec->fh, SWITCH_AUDIO_COL_STR_ARTIST, "FreeSWITCH mod_conference Software Conference Module");

	if (test_eflag(conference, EFLAG_RECORD) &&
			switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_data(conference, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "start-recording");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Path", rec->path);
		switch_event_fire(&event);
	}

	if (conference_add_member(conference, member) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Joining Conference\n");
		goto end;
	}

	while (member_test_flag(member, MFLAG_RUNNING) && conference_test_flag(conference, CFLAG_RUNNING) && (conference->count + conference->count_ghosts)) {

		len = 0;

		mux_used = (uint32_t) switch_buffer_inuse(member->mux_buffer);

		if (member_test_flag(member, MFLAG_FLUSH_BUFFER)) {
			if (mux_used) {
				switch_mutex_lock(member->audio_out_mutex);
				switch_buffer_zero(member->mux_buffer);
				switch_mutex_unlock(member->audio_out_mutex);
				mux_used = 0;
			}
			member_clear_flag_locked(member, MFLAG_FLUSH_BUFFER);
		}

	again:

		if (switch_test_flag((&member->rec->fh), SWITCH_FILE_PAUSE)) {
			member_set_flag_locked(member, MFLAG_FLUSH_BUFFER);
			goto loop;
		}

		if (mux_used >= data_buf_len) {
			/* Flush the output buffer and write all the data (presumably muxed) to the file */
			switch_mutex_lock(member->audio_out_mutex);
			//low_count = 0;

			if ((rlen = (uint32_t) switch_buffer_read(member->mux_buffer, data_buf, data_buf_len))) {
				len = (switch_size_t) rlen / sizeof(int16_t) / conference->channels;
			}
			switch_mutex_unlock(member->audio_out_mutex);
		}

		if (len == 0) {
			mux_used = (uint32_t) switch_buffer_inuse(member->mux_buffer);
				
			if (mux_used >= data_buf_len) {
				goto again;
			}

			memset(data_buf, 255, (switch_size_t) data_buf_len);
			len = (switch_size_t) samples;
		}

		if (!member_test_flag(member, MFLAG_PAUSE_RECORDING)) {
			if (!len || switch_core_file_write(&member->rec->fh, data_buf, &len) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Write Failed\n");
				member_clear_flag_locked(member, MFLAG_RUNNING);
			}
		}

	loop:

		switch_core_timer_next(&timer);
	}							/* Rinse ... Repeat */

  end:
	
	for(;;) {
		switch_mutex_lock(member->audio_out_mutex);
		rlen = (uint32_t) switch_buffer_read(member->mux_buffer, data_buf, data_buf_len);
		switch_mutex_unlock(member->audio_out_mutex);
		
		if (rlen > 0) {
			len = (switch_size_t) rlen / sizeof(int16_t)/ conference->channels;
			switch_core_file_write(&member->rec->fh, data_buf, &len); 
		} else {
			break;
		}
	}

	switch_safe_free(data_buf);
	switch_core_timer_destroy(&timer);
	conference_del_member(conference, member);

	if (conference->canvas) {
		conference->canvas->send_keyframe = 1;
	}

	switch_buffer_destroy(&member->audio_buffer);
	switch_buffer_destroy(&member->mux_buffer);
	member_clear_flag_locked(member, MFLAG_RUNNING);
	if (switch_test_flag((&member->rec->fh), SWITCH_FILE_OPEN)) {
		switch_mutex_lock(conference->mutex);
		switch_mutex_unlock(conference->mutex);
		switch_core_file_close(&member->rec->fh);
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Recording of %s Stopped\n", rec->path);
	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_data(conference, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "stop-recording");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Path", rec->path);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Samples-Out", "%ld", (long) member->rec->fh.samples_out);  
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Samplerate", "%ld", (long) member->rec->fh.samplerate);  
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Milliseconds-Elapsed", "%ld", (long) member->rec->fh.samples_out / (member->rec->fh.samplerate / 1000));  
		switch_event_fire(&event);
	}

	if (rec->autorec && conference->auto_recording) {
		conference->auto_recording--;
	}

	switch_mutex_lock(conference->flag_mutex);
	for (rp = conference->rec_node_head; rp; rp = rp->next) {
		if (rec == rp) {
			if (last) {
				last->next = rp->next;
			} else {
				conference->rec_node_head = rp->next;
			}
		}
	}
	switch_mutex_unlock(conference->flag_mutex);


	if (rec->pool) {
		switch_memory_pool_t *pool = rec->pool;
		rec = NULL;
		switch_core_destroy_memory_pool(&pool);
	}

	switch_mutex_lock(globals.hash_mutex);
	globals.threads--;
	switch_mutex_unlock(globals.hash_mutex);

	switch_thread_rwlock_unlock(conference->rwlock);
	return NULL;
}

/* Make files stop playing in a conference either the current one or all of them */
static uint32_t conference_stop_file(conference_obj_t *conference, file_stop_t stop)
{
	uint32_t count = 0;
	conference_file_node_t *nptr;

	switch_assert(conference != NULL);

	switch_mutex_lock(conference->mutex);

	if (stop == FILE_STOP_ALL) {
		for (nptr = conference->fnode; nptr; nptr = nptr->next) {
			nptr->done++;
			count++;
		}
		if (conference->async_fnode) {
			conference->async_fnode->done++;
			count++;
		}
	} else if (stop == FILE_STOP_ASYNC) {
		if (conference->async_fnode) {
			conference->async_fnode->done++;
			count++;
		}
	} else {
		if (conference->fnode) {
			conference->fnode->done++;
			count++;
		}
	}

	switch_mutex_unlock(conference->mutex);

	return count;
}

/* stop playing a file for the member of the conference */
static uint32_t conference_member_stop_file(conference_member_t *member, file_stop_t stop)
{
	conference_file_node_t *nptr;
	uint32_t count = 0;

	if (member == NULL)
		return count;


	switch_mutex_lock(member->fnode_mutex);

	if (stop == FILE_STOP_ALL) {
		for (nptr = member->fnode; nptr; nptr = nptr->next) {
			nptr->done++;
			count++;
		}
	} else {
		if (member->fnode) {
			member->fnode->done++;
			count++;
		}
	}

	switch_mutex_unlock(member->fnode_mutex);

	return count;
}

static void conference_send_all_dtmf(conference_member_t *member, conference_obj_t *conference, const char *dtmf)
{
	conference_member_t *imember;

	switch_mutex_lock(conference->mutex);
	switch_mutex_lock(conference->member_mutex);

	for (imember = conference->members; imember; imember = imember->next) {
		/* don't send to self */
		if (imember->id == member->id) {
			continue;
		}
		if (imember->session) {
			const char *p;
			for (p = dtmf; p && *p; p++) {
				switch_dtmf_t *dt, digit = { *p, SWITCH_DEFAULT_DTMF_DURATION };
				
				switch_zmalloc(dt, sizeof(*dt));
				*dt = digit;
				switch_queue_push(imember->dtmf_queue, dt);
				switch_core_session_kill_channel(imember->session, SWITCH_SIG_BREAK);
			}
		}
	}

	switch_mutex_unlock(conference->member_mutex);
	switch_mutex_unlock(conference->mutex);
}

static void canvas_del_fnode_layer(conference_obj_t *conference, conference_file_node_t *fnode)
{
	mcu_canvas_t *canvas = conference->canvases[fnode->canvas_id];
	
	switch_mutex_lock(canvas->mutex);
	if (fnode->layer_id > -1) {
		mcu_layer_t *xlayer = &canvas->layers[fnode->layer_id];
		
		fnode->layer_id = -1;
		fnode->canvas_id = -1;
		xlayer->fnode = NULL;
		reset_layer(xlayer);
	}
	switch_mutex_unlock(canvas->mutex);
}

static void canvas_set_fnode_layer(mcu_canvas_t *canvas, conference_file_node_t *fnode, int idx)
{
	mcu_layer_t *layer = NULL;
	mcu_layer_t *xlayer = NULL;

	switch_mutex_lock(canvas->mutex);

	if (idx == -1) {
		int i;

		if (canvas->layout_floor_id > -1) {
			idx = canvas->layout_floor_id;
			xlayer = &canvas->layers[idx];

			if (xlayer->fnode) {
				idx = -1;
			}
		}

		if (idx < 0) {
			for (i = 0; i < canvas->total_layers; i++) {
				xlayer = &canvas->layers[i];

				if (xlayer->fnode || xlayer->geometry.res_id || xlayer->member_id) {
					continue;
				}
				
				idx = i;
				break;
			}
		}
	}

	if (idx < 0) goto end;
	
	layer = &canvas->layers[idx];

	layer->fnode = fnode;
	fnode->layer_id = idx;
	fnode->canvas_id = canvas->canvas_id;

	if (layer->member_id > -1) {
		conference_member_t *member;

		if ((member = conference_member_get(canvas->conference, layer->member_id))) {
			detach_video_layer(member);
			switch_thread_rwlock_unlock(member->rwlock);
		}
	}
	
 end:

	switch_mutex_unlock(canvas->mutex);
}

/* Play a file in the conference room */
static switch_status_t conference_play_file(conference_obj_t *conference, char *file, uint32_t leadin, switch_channel_t *channel, uint8_t async)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	conference_file_node_t *fnode, *nptr = NULL;
	switch_memory_pool_t *pool;
	uint32_t count;
	char *dfile = NULL, *expanded = NULL;
	int say = 0;
	uint8_t channels = (uint8_t) conference->channels;
	int bad_params = 0;
	int flags = 0;

	switch_assert(conference != NULL);

	if (zstr(file)) {
		return SWITCH_STATUS_NOTFOUND;
	}

	switch_mutex_lock(conference->mutex);
	switch_mutex_lock(conference->member_mutex);
	count = conference->count;
	switch_mutex_unlock(conference->member_mutex);
	switch_mutex_unlock(conference->mutex);

	if (!count) {
		return SWITCH_STATUS_FALSE;
	}

	if (channel) {
		if ((expanded = switch_channel_expand_variables(channel, file)) != file) {
			file = expanded;
		} else {
			expanded = NULL;
		}
	}

	if (!strncasecmp(file, "say:", 4)) {
		say = 1;
	}

	if (!async && say) {
		status = conference_say(conference, file + 4, leadin);
		goto done;
	}

	if (!switch_is_file_path(file)) {
		if (!say && conference->sound_prefix) {
			char *params_portion = NULL;
			char *file_portion = NULL;
			switch_separate_file_params(file, &file_portion, &params_portion);

			if (params_portion) {
				dfile = switch_mprintf("%s%s%s%s", params_portion, conference->sound_prefix, SWITCH_PATH_SEPARATOR, file_portion);
			} else {
				dfile = switch_mprintf("%s%s%s", conference->sound_prefix, SWITCH_PATH_SEPARATOR, file_portion);
			}

			file = dfile;
			switch_safe_free(file_portion);
			switch_safe_free(params_portion);

		} else if (!async) {
			status = conference_say(conference, file, leadin);
			goto done;
		} else {
			goto done;
		}
	}

	/* Setup a memory pool to use. */
	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool Failure\n");
		status = SWITCH_STATUS_MEMERR;
		goto done;
	}

	/* Create a node object */
	if (!(fnode = switch_core_alloc(pool, sizeof(*fnode)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Alloc Failure\n");
		switch_core_destroy_memory_pool(&pool);
		status = SWITCH_STATUS_MEMERR;
		goto done;
	}

	fnode->conference = conference;
	fnode->layer_id = -1;
	fnode->type = NODE_TYPE_FILE;
	fnode->leadin = leadin;

	if (switch_stristr("position=", file)) {
		/* positional requires mono input */
		fnode->fh.channels = channels = 1;
	}
	
 retry:

	flags = SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT;

	if (conference->members_with_video && conference_test_flag(conference, CFLAG_TRANSCODE_VIDEO)) {
		flags |= SWITCH_FILE_FLAG_VIDEO;
	}

	/* Open the file */
	fnode->fh.pre_buffer_datalen = SWITCH_DEFAULT_FILE_BUFFER_LEN;

	if (switch_core_file_open(&fnode->fh, file, channels, conference->rate, flags, pool) != SWITCH_STATUS_SUCCESS) {
		switch_event_t *event;

		if (test_eflag(conference, EFLAG_PLAY_FILE) &&
			switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_add_event_data(conference, event);
			
			if (fnode->fh.params) {
				switch_event_merge(event, conference->fnode->fh.params);
			}
			
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "play-file");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "File", file);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Async", async ? "true" : "false");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Error", "File could not be played");
			switch_event_fire(&event);
		}

		switch_core_destroy_memory_pool(&pool);
		status = SWITCH_STATUS_NOTFOUND;
		goto done;
	}

	if (fnode->fh.params) {
		const char *vol = switch_event_get_header(fnode->fh.params, "vol");
		const char *position = switch_event_get_header(fnode->fh.params, "position");

		if (!zstr(vol)) {
			fnode->fh.vol = atoi(vol);
		}

		if (!bad_params && !zstr(position) && conference->channels == 2) {
			fnode->al = create_al(pool);
			if (parse_position(fnode->al, position) != SWITCH_STATUS_SUCCESS) {
				switch_core_file_close(&fnode->fh);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Position Data.\n");
				fnode->al = NULL;
				channels = (uint8_t)conference->channels;
				bad_params = 1;
				goto retry;
			}
		}
	}

	fnode->pool = pool;
	fnode->async = async;
	fnode->file = switch_core_strdup(fnode->pool, file);
	
	if (!conference->fnode || (async && !conference->async_fnode)) {
		fnode_check_video(fnode);
	}

	/* Queue the node */
	switch_mutex_lock(conference->mutex);

	if (async) {
		if (conference->async_fnode) {
			nptr = conference->async_fnode;
		}
		conference->async_fnode = fnode;

		if (nptr) {
			switch_memory_pool_t *tmppool;
			conference_file_close(conference, nptr);
			tmppool = nptr->pool;
			switch_core_destroy_memory_pool(&tmppool);
		}

	} else {
		for (nptr = conference->fnode; nptr && nptr->next; nptr = nptr->next);

		if (nptr) {
			nptr->next = fnode;
		} else {
			conference->fnode = fnode;
		}
	}

	switch_mutex_unlock(conference->mutex);

  done:

	switch_safe_free(expanded);
	switch_safe_free(dfile);

	return status;
}

/* Play a file in the conference room to a member */
static switch_status_t conference_member_play_file(conference_member_t *member, char *file, uint32_t leadin, switch_bool_t mux)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *dfile = NULL, *expanded = NULL;
	conference_file_node_t *fnode, *nptr = NULL;
	switch_memory_pool_t *pool;
	int channels = member->conference->channels;
	int bad_params = 0;

	if (member == NULL || file == NULL || member_test_flag(member, MFLAG_KICKED))
		return status;

	if ((expanded = switch_channel_expand_variables(switch_core_session_get_channel(member->session), file)) != file) {
		file = expanded;
	} else {
		expanded = NULL;
	}
	if (!strncasecmp(file, "say:", 4)) {
		if (!zstr(file + 4)) {
			status = conference_member_say(member, file + 4, leadin);
		}
		goto done;
	}
	if (!switch_is_file_path(file)) {
		if (member->conference->sound_prefix) {
			if (!(dfile = switch_mprintf("%s%s%s", member->conference->sound_prefix, SWITCH_PATH_SEPARATOR, file))) {
				goto done;
			}
			file = dfile;
		} else if (!zstr(file)) {
			status = conference_member_say(member, file, leadin);
			goto done;
		}
	}
	/* Setup a memory pool to use. */
	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_CRIT, "Pool Failure\n");
		status = SWITCH_STATUS_MEMERR;
		goto done;
	}
	/* Create a node object */
	if (!(fnode = switch_core_alloc(pool, sizeof(*fnode)))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_CRIT, "Alloc Failure\n");
		switch_core_destroy_memory_pool(&pool);
		status = SWITCH_STATUS_MEMERR;
		goto done;
	}

	fnode->conference = member->conference;
	fnode->layer_id = -1;
	fnode->type = NODE_TYPE_FILE;
	fnode->leadin = leadin;
	fnode->mux = mux;
	fnode->member_id = member->id;

	if (switch_stristr("position=", file)) {
		/* positional requires mono input */
		fnode->fh.channels = channels = 1;
	}

 retry:

	/* Open the file */
	fnode->fh.pre_buffer_datalen = SWITCH_DEFAULT_FILE_BUFFER_LEN;
	if (switch_core_file_open(&fnode->fh,
							  file, (uint8_t) channels, member->conference->rate, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT,
							  pool) != SWITCH_STATUS_SUCCESS) {
		switch_core_destroy_memory_pool(&pool);
		status = SWITCH_STATUS_NOTFOUND;
		goto done;
	}
	fnode->pool = pool;
	fnode->file = switch_core_strdup(fnode->pool, file);

	if (fnode->fh.params) {
		const char *position = switch_event_get_header(fnode->fh.params, "position");
		
		if (!bad_params && !zstr(position) && member->conference->channels == 2) {
			fnode->al = create_al(pool);
			if (parse_position(fnode->al, position) != SWITCH_STATUS_SUCCESS) {
				switch_core_file_close(&fnode->fh);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_ERROR, "Invalid Position Data.\n");
				fnode->al = NULL;
				channels = member->conference->channels;
				bad_params = 1;
				goto retry;
			}
		}
	}

	/* Queue the node */
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_DEBUG, "Queueing file '%s' for play\n", file);
	switch_mutex_lock(member->fnode_mutex);
	for (nptr = member->fnode; nptr && nptr->next; nptr = nptr->next);
	if (nptr) {
		nptr->next = fnode;
	} else {
		member->fnode = fnode;
	}
	switch_mutex_unlock(member->fnode_mutex);
	status = SWITCH_STATUS_SUCCESS;

  done:

	switch_safe_free(expanded);
	switch_safe_free(dfile);

	return status;
}

/* Say some thing with TTS in the conference room */
static switch_status_t conference_member_say(conference_member_t *member, char *text, uint32_t leadin)
{
	conference_obj_t *conference = member->conference;
	conference_file_node_t *fnode, *nptr;
	switch_memory_pool_t *pool;
	switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *fp = NULL;
	int channels = member->conference->channels;
	switch_event_t *params = NULL;
	const char *position = NULL;

	if (member == NULL || zstr(text))
		return SWITCH_STATUS_FALSE;

	switch_assert(conference != NULL);

	if (!(conference->tts_engine && conference->tts_voice)) {
		return SWITCH_STATUS_SUCCESS;
	}

	/* Setup a memory pool to use. */
	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_CRIT, "Pool Failure\n");
		return SWITCH_STATUS_MEMERR;
	}

	/* Create a node object */
	if (!(fnode = switch_core_alloc(pool, sizeof(*fnode)))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_CRIT, "Alloc Failure\n");
		switch_core_destroy_memory_pool(&pool);
		return SWITCH_STATUS_MEMERR;
	}

	fnode->conference = conference;

	fnode->layer_id = -1;

	if (*text == '{') {
		char *new_fp;
		
		fp = switch_core_strdup(pool, text);
		switch_assert(fp);

		if (!switch_event_create_brackets(fp, '{', '}', ',', &params, &new_fp, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
			new_fp = fp;
		}

		text = new_fp;
	}

	fnode->type = NODE_TYPE_SPEECH;
	fnode->leadin = leadin;
	fnode->pool = pool;


	if (params && (position = switch_event_get_header(params, "position"))) {
		if (conference->channels != 2) {
			position = NULL;
		} else {
			channels = 1;
			fnode->al = create_al(pool);
			if (parse_position(fnode->al, position) != SWITCH_STATUS_SUCCESS) {
				fnode->al = NULL;
				channels = conference->channels;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Position Data.\n");
			}
		}
	}


	if (member->sh && member->last_speech_channels != channels) {
		switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;
		switch_core_speech_close(&member->lsh, &flags);
		member->sh = NULL;
	}

	if (!member->sh) {
		memset(&member->lsh, 0, sizeof(member->lsh));
		if (switch_core_speech_open(&member->lsh, conference->tts_engine, conference->tts_voice,
									conference->rate, conference->interval, channels, &flags, switch_core_session_get_pool(member->session)) !=
			SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_ERROR, "Invalid TTS module [%s]!\n", conference->tts_engine);
			status = SWITCH_STATUS_FALSE;
			goto end;
		}
		member->last_speech_channels = channels;
		member->sh = &member->lsh;
	}

	/* Queue the node */
	switch_mutex_lock(member->fnode_mutex);
	for (nptr = member->fnode; nptr && nptr->next; nptr = nptr->next);

	if (nptr) {
		nptr->next = fnode;
	} else {
		member->fnode = fnode;
	}

	fnode->sh = member->sh;
	/* Begin Generation */
	switch_sleep(200000);

	if (*text == '#') {
		char *tmp = (char *) text + 1;
		char *vp = tmp, voice[128] = "";
		if ((tmp = strchr(tmp, '#'))) {
			text = tmp + 1;
			switch_copy_string(voice, vp, (tmp - vp) + 1);
			switch_core_speech_text_param_tts(fnode->sh, "voice", voice);
		}
	} else {
		switch_core_speech_text_param_tts(fnode->sh, "voice", conference->tts_voice);
	}

	switch_core_speech_feed_tts(fnode->sh, text, &flags);
	switch_mutex_unlock(member->fnode_mutex);

	status = SWITCH_STATUS_SUCCESS;

 end:

	if (params) {
		switch_event_destroy(&params);
	}

	return status;
}

/* Say some thing with TTS in the conference room */
static switch_status_t conference_say(conference_obj_t *conference, const char *text, uint32_t leadin)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	conference_file_node_t *fnode, *nptr;
	switch_memory_pool_t *pool;
	switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;
	uint32_t count;
	switch_event_t *params = NULL;
	char *fp = NULL;
	int channels;
	const char *position = NULL;

	switch_assert(conference != NULL);

	channels = conference->channels;

	if (zstr(text)) {
		return SWITCH_STATUS_GENERR;
	}


	switch_mutex_lock(conference->mutex);
	switch_mutex_lock(conference->member_mutex);
	count = conference->count;
	if (!(conference->tts_engine && conference->tts_voice)) {
		count = 0;
	}
	switch_mutex_unlock(conference->member_mutex);
	switch_mutex_unlock(conference->mutex);

	if (!count) {
		return SWITCH_STATUS_FALSE;
	}

	/* Setup a memory pool to use. */
	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool Failure\n");
		return SWITCH_STATUS_MEMERR;
	}

	/* Create a node object */
	if (!(fnode = switch_core_alloc(pool, sizeof(*fnode)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Alloc Failure\n");
		switch_core_destroy_memory_pool(&pool);
		return SWITCH_STATUS_MEMERR;
	}

	fnode->conference = conference;
	fnode->layer_id = -1;

	if (*text == '{') {
		char *new_fp;
		
		fp = switch_core_strdup(pool, text);
		switch_assert(fp);

		if (!switch_event_create_brackets(fp, '{', '}', ',', &params, &new_fp, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
			new_fp = fp;
		}

		text = new_fp;
	}


	fnode->type = NODE_TYPE_SPEECH;
	fnode->leadin = leadin;

	if (params && (position = switch_event_get_header(params, "position"))) {
		if (conference->channels != 2) {
			position = NULL;
		} else {
			channels = 1;
			fnode->al = create_al(pool);
			if (parse_position(fnode->al, position) != SWITCH_STATUS_SUCCESS) {
				fnode->al = NULL;
				channels = conference->channels;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Position Data.\n");
			}
		}
	}

	if (conference->sh && conference->last_speech_channels != channels) {
		switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;
		switch_core_speech_close(&conference->lsh, &flags);
		conference->sh = NULL;
	}

	if (!conference->sh) {
		memset(&conference->lsh, 0, sizeof(conference->lsh));
		if (switch_core_speech_open(&conference->lsh, conference->tts_engine, conference->tts_voice,
									conference->rate, conference->interval, channels, &flags, NULL) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid TTS module [%s]!\n", conference->tts_engine);
			status = SWITCH_STATUS_FALSE;
			goto end;
		}
		conference->last_speech_channels = channels;
		conference->sh = &conference->lsh;
	}

	fnode->pool = pool;

	/* Queue the node */
	switch_mutex_lock(conference->mutex);
	for (nptr = conference->fnode; nptr && nptr->next; nptr = nptr->next);

	if (nptr) {
		nptr->next = fnode;
	} else {
		conference->fnode = fnode;
	}

	fnode->sh = conference->sh;
	if (*text == '#') {
		char *tmp = (char *) text + 1;
		char *vp = tmp, voice[128] = "";
		if ((tmp = strchr(tmp, '#'))) {
			text = tmp + 1;
			switch_copy_string(voice, vp, (tmp - vp) + 1);
			switch_core_speech_text_param_tts(fnode->sh, "voice", voice);
		}
	} else {
		switch_core_speech_text_param_tts(fnode->sh, "voice", conference->tts_voice);
	}

	/* Begin Generation */
	switch_sleep(200000);
	switch_core_speech_feed_tts(fnode->sh, (char *) text, &flags);
	switch_mutex_unlock(conference->mutex);
	status = SWITCH_STATUS_SUCCESS;

 end:

	if (params) {
		switch_event_destroy(&params);
	}

	return status;
}

/* send a message to every member of the conference */
static void chat_message_broadcast(conference_obj_t *conference, switch_event_t *event)
{
	conference_member_t *member = NULL;
	switch_event_t *processed;

	switch_assert(conference != NULL);
	switch_event_create(&processed, SWITCH_EVENT_CHANNEL_DATA);

	switch_mutex_lock(conference->member_mutex);
	for (member = conference->members; member; member = member->next) {
		if (member->session && !member_test_flag(member, MFLAG_NOCHANNEL)) {
			const char *presence_id = switch_channel_get_variable(member->channel, "presence_id");
			const char *chat_proto = switch_channel_get_variable(member->channel, "chat_proto");
			switch_event_t *reply = NULL;
			
			if (presence_id && chat_proto) {
				if (switch_event_get_header(processed, presence_id)) {
					continue;
				}
				switch_event_dup(&reply, event);
				switch_event_add_header_string(reply, SWITCH_STACK_BOTTOM, "to", presence_id);
				switch_event_add_header_string(reply, SWITCH_STACK_BOTTOM, "conference_name", conference->name);
				switch_event_add_header_string(reply, SWITCH_STACK_BOTTOM, "conference_domain", conference->domain);
				
				switch_event_set_body(reply, switch_event_get_body(event));
				
				switch_core_chat_deliver(chat_proto, &reply);
				switch_event_add_header_string(processed, SWITCH_STACK_BOTTOM, presence_id, "true");
			}
		}
	}
	switch_event_destroy(&processed);
	switch_mutex_unlock(conference->member_mutex);
}

/* execute a callback for every member of the conference */
static void conference_member_itterator(conference_obj_t *conference, switch_stream_handle_t *stream, uint8_t non_mod, conf_api_member_cmd_t pfncallback, void *data)
{
	conference_member_t *member = NULL;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);
	switch_assert(pfncallback != NULL);

	switch_mutex_lock(conference->member_mutex);
	for (member = conference->members; member; member = member->next) {
		if (!(non_mod && member_test_flag(member, MFLAG_MOD))) {
			if (member->session && !member_test_flag(member, MFLAG_NOCHANNEL)) {
				pfncallback(member, stream, data);
			}
		} else {
			stream->write_function(stream, "Skipping moderator (member id %d).\n", member->id);
		}	
	}
	switch_mutex_unlock(conference->member_mutex);
}


static switch_status_t list_conferences(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	switch_console_callback_match_t *my_matches = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_hash_index_t *hi;
	void *val;
	const void *vvar;

	switch_mutex_lock(globals.hash_mutex);
	for (hi = switch_core_hash_first(globals.conference_hash); hi; hi = switch_core_hash_next(&hi)) {
		switch_core_hash_this(hi, &vvar, NULL, &val);
		switch_console_push_match(&my_matches, (const char *) vvar);		
	}
	switch_mutex_unlock(globals.hash_mutex);
	
	if (my_matches) {
		*matches = my_matches;
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
}

static void conference_list_pretty(conference_obj_t *conference, switch_stream_handle_t *stream)
{
	conference_member_t *member = NULL;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	switch_mutex_lock(conference->member_mutex);

	for (member = conference->members; member; member = member->next) {
		switch_channel_t *channel;
		switch_caller_profile_t *profile;

		if (member_test_flag(member, MFLAG_NOCHANNEL)) {
			continue;
		}
		channel = switch_core_session_get_channel(member->session);
		profile = switch_channel_get_caller_profile(channel);

		stream->write_function(stream, "%u) %s (%s)\n", member->id, profile->caller_id_name, profile->caller_id_number);
	}

	switch_mutex_unlock(conference->member_mutex);
}

static void conference_list(conference_obj_t *conference, switch_stream_handle_t *stream, char *delim)
{
	conference_member_t *member = NULL;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);
	switch_assert(delim != NULL);

	switch_mutex_lock(conference->member_mutex);

	for (member = conference->members; member; member = member->next) {
		switch_channel_t *channel;
		switch_caller_profile_t *profile;
		char *uuid;
		char *name;
		uint32_t count = 0;

		if (member_test_flag(member, MFLAG_NOCHANNEL)) {
			continue;
		}

		uuid = switch_core_session_get_uuid(member->session);
		channel = switch_core_session_get_channel(member->session);
		profile = switch_channel_get_caller_profile(channel);
		name = switch_channel_get_name(channel);

		stream->write_function(stream, "%u%s%s%s%s%s%s%s%s%s",
							   member->id, delim, name, delim, uuid, delim, profile->caller_id_name, delim, profile->caller_id_number, delim);

		if (member_test_flag(member, MFLAG_CAN_HEAR)) {
			stream->write_function(stream, "hear");
			count++;
		}

		if (member_test_flag(member, MFLAG_CAN_SPEAK)) {
			stream->write_function(stream, "%s%s", count ? "|" : "", "speak");
			count++;
		}

		if (member_test_flag(member, MFLAG_TALKING)) {
			stream->write_function(stream, "%s%s", count ? "|" : "", "talking");
			count++;
		}

		if (switch_channel_test_flag(switch_core_session_get_channel(member->session), CF_VIDEO)) {
			stream->write_function(stream, "%s%s", count ? "|" : "", "video");
			count++;
		}

		if (member == member->conference->floor_holder) {
			stream->write_function(stream, "%s%s", count ? "|" : "", "floor");
			count++;
		}

		if (member->id == member->conference->video_floor_holder) {
			stream->write_function(stream, "%s%s", count ? "|" : "", "vid-floor");
			count++;
		}

		if (member_test_flag(member, MFLAG_MOD)) {
			stream->write_function(stream, "%s%s", count ? "|" : "", "moderator");
			count++;
		}

		if (member_test_flag(member, MFLAG_GHOST)) {
			stream->write_function(stream, "%s%s", count ? "|" : "", "ghost");
			count++;
		}

		stream->write_function(stream, "%s%d%s%d%s%d%s%d\n", delim,
							   member->volume_in_level, 
							   delim,
							   member->agc_volume_in_level,
							   delim, member->volume_out_level, delim, member->energy_level);
	}

	switch_mutex_unlock(conference->member_mutex);
}

static void conference_list_count_only(conference_obj_t *conference, switch_stream_handle_t *stream)
{
	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	stream->write_function(stream, "%d", conference->count);
}

static switch_status_t conf_api_sub_agc(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	int level;
	int on = 0;

	if (argc == 2) {
		stream->write_function(stream, "+OK CURRENT AGC LEVEL IS %d\n", conference->agc_level);
		return SWITCH_STATUS_SUCCESS;
	}


	if (!(on = !strcasecmp(argv[2], "on"))) {
		stream->write_function(stream, "+OK AGC DISABLED\n");
		conference->agc_level = 0;
		return SWITCH_STATUS_SUCCESS;
	}
	
	if (argc > 3) {
		level = atoi(argv[3]);
	} else {
		level = DEFAULT_AGC_LEVEL;
	}

	if (level > conference->energy_level) {
		conference->avg_score = 0;
		conference->avg_itt = 0;
		conference->avg_tally = 0;
		conference->agc_level = level;

		if (stream) {
			stream->write_function(stream, "OK AGC ENABLED %d\n", conference->agc_level);
		}
		
	} else {
		if (stream) {
			stream->write_function(stream, "-ERR invalid level\n");
		}
	}




	return SWITCH_STATUS_SUCCESS;
		
}

static switch_status_t conf_api_sub_mute(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	member_clear_flag_locked(member, MFLAG_CAN_SPEAK);
	member_clear_flag_locked(member, MFLAG_TALKING);

	if (member->session && !member_test_flag(member, MFLAG_MUTE_DETECT)) {
		switch_core_media_hard_mute(member->session, SWITCH_TRUE);
	}

	if (!(data) || !strstr((char *) data, "quiet")) {
		member_set_flag(member, MFLAG_INDICATE_MUTE);
	}
	member->score_iir = 0;

	if (stream != NULL) {
		stream->write_function(stream, "OK mute %u\n", member->id);
	}

	if (test_eflag(member->conference, EFLAG_MUTE_MEMBER) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_member_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "mute-member");
		switch_event_fire(&event);
	}

	if (conference_test_flag(member->conference, CFLAG_POSITIONAL)) {
		gen_arc(member->conference, NULL);
	}

	member_update_status_field(member);

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t conf_api_sub_tmute(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	if (member_test_flag(member, MFLAG_CAN_SPEAK)) {
		return conf_api_sub_mute(member, stream, data);
	}

	return conf_api_sub_unmute(member, stream, data);
}


static switch_status_t conf_api_sub_unmute(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	member_set_flag_locked(member, MFLAG_CAN_SPEAK);

	if (member->session && !member_test_flag(member, MFLAG_MUTE_DETECT)) {
		switch_core_media_hard_mute(member->session, SWITCH_FALSE);
	}

	if (!(data) || !strstr((char *) data, "quiet")) {
		member_set_flag(member, MFLAG_INDICATE_UNMUTE);
	}

	if (stream != NULL) {
		stream->write_function(stream, "OK unmute %u\n", member->id);
	}

	if (test_eflag(member->conference, EFLAG_UNMUTE_MEMBER) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_member_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "unmute-member");
		switch_event_fire(&event);
	}

	if (conference_test_flag(member->conference, CFLAG_POSITIONAL)) {
		gen_arc(member->conference, NULL);
	}

	member_update_status_field(member);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t conf_api_sub_vmute_snap(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_bool_t clear = SWITCH_FALSE;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	if (member->video_flow == SWITCH_MEDIA_FLOW_SENDONLY) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (!member->conference->canvas) {
		stream->write_function(stream, "Conference is not in mixing mode\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (stream != NULL) {
		stream->write_function(stream, "OK vmute image snapped %u\n", member->id);
	}

	if (data && !strcasecmp((char *)data, "clear")) {
		clear = SWITCH_TRUE;
	}

	vmute_snap(member, clear);
	
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t conf_api_sub_vmute(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	if (member->video_flow == SWITCH_MEDIA_FLOW_SENDONLY) {
		return SWITCH_STATUS_SUCCESS;
	}

	member_clear_flag_locked(member, MFLAG_CAN_BE_SEEN);
	reset_video_bitrate_counters(member);
	
	if (member->channel) {
		switch_channel_set_flag(member->channel, CF_VIDEO_PAUSE_READ);
		switch_core_session_request_video_refresh(member->session);
		switch_channel_video_sync(member->channel);
	}

	if (!(data) || !strstr((char *) data, "quiet")) {
		member_set_flag(member, MFLAG_INDICATE_MUTE);
	}

	if (stream != NULL) {
		stream->write_function(stream, "OK vmute %u\n", member->id);
	}

	if (test_eflag(member->conference, EFLAG_MUTE_MEMBER) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_member_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "vmute-member");
		switch_event_fire(&event);
	}

	member_update_status_field(member);

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t conf_api_sub_tvmute(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	if (member_test_flag(member, MFLAG_CAN_BE_SEEN)) {
		return conf_api_sub_vmute(member, stream, data);
	}

	return conf_api_sub_unvmute(member, stream, data);
}


static switch_status_t conf_api_sub_unvmute(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;
	mcu_layer_t *layer = NULL;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;
	
	if (member->video_flow == SWITCH_MEDIA_FLOW_SENDONLY) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (member->conference->canvas) {
		switch_mutex_lock(member->conference->canvas->mutex);
		layer = &member->conference->canvas->layers[member->video_layer_id];
		clear_layer(layer);
		switch_mutex_unlock(member->conference->canvas->mutex);
	}

	member_set_flag_locked(member, MFLAG_CAN_BE_SEEN);
	reset_video_bitrate_counters(member);

	if (member->channel) {
		switch_channel_clear_flag(member->channel, CF_VIDEO_PAUSE_READ);
		switch_channel_video_sync(member->channel);
	}

	if (!(data) || !strstr((char *) data, "quiet")) {
		member_set_flag(member, MFLAG_INDICATE_UNMUTE);
	}

	if (stream != NULL) {
		stream->write_function(stream, "OK unvmute %u\n", member->id);
	}

	if (test_eflag(member->conference, EFLAG_UNMUTE_MEMBER) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_member_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "unvmute-member");
		switch_event_fire(&event);
	}


	member_update_status_field(member);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t conf_api_sub_deaf(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	member_clear_flag_locked(member, MFLAG_CAN_HEAR);
	if (stream != NULL) {
		stream->write_function(stream, "OK deaf %u\n", member->id);
	}
	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_member_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "deaf-member");
		switch_event_fire(&event);
	}

	if (conference_test_flag(member->conference, CFLAG_POSITIONAL)) {
		gen_arc(member->conference, NULL);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t conf_api_sub_undeaf(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	member_set_flag_locked(member, MFLAG_CAN_HEAR);
	if (stream != NULL) {
		stream->write_function(stream, "OK undeaf %u\n", member->id);
	}
	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_member_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "undeaf-member");
		switch_event_fire(&event);
	}

	if (conference_test_flag(member->conference, CFLAG_POSITIONAL)) {
		gen_arc(member->conference, NULL);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t conf_api_sub_hup(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;

	if (member == NULL) {
		return SWITCH_STATUS_GENERR;
	}
	
	member_clear_flag(member, MFLAG_RUNNING);

	if (member->conference && test_eflag(member->conference, EFLAG_HUP_MEMBER)) {
		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_add_event_member_data(member, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "hup-member");
			switch_event_fire(&event);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t conf_api_sub_kick(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;

	if (member == NULL) {
		return SWITCH_STATUS_GENERR;
	}
	
	member_clear_flag(member, MFLAG_RUNNING);
	member_set_flag_locked(member, MFLAG_KICKED);
	switch_core_session_kill_channel(member->session, SWITCH_SIG_BREAK);

	if (data && member->session) {
		member->kicked_sound = switch_core_session_strdup(member->session, (char *) data);
	}

	if (stream != NULL) {
		stream->write_function(stream, "OK kicked %u\n", member->id);
	}

	if (member->conference && test_eflag(member->conference, EFLAG_KICK_MEMBER)) {
		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_add_event_member_data(member, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "kick-member");
			switch_event_fire(&event);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t conf_api_sub_dtmf(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;
	char *dtmf = (char *) data;

	if (member == NULL) {
		stream->write_function(stream, "Invalid member!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (zstr(dtmf)) {
		stream->write_function(stream, "Invalid input!\n");
		return SWITCH_STATUS_GENERR;
	} else {
		char *p;

		for(p = dtmf; p && *p; p++) {
			switch_dtmf_t *dt, digit = { *p, SWITCH_DEFAULT_DTMF_DURATION };
		
			switch_zmalloc(dt, sizeof(*dt));
			*dt = digit;
		
			switch_queue_push(member->dtmf_queue, dt);
			switch_core_session_kill_channel(member->session, SWITCH_SIG_BREAK);
		}
	}

	if (stream != NULL) {
		stream->write_function(stream, "OK sent %s to %u\n", (char *) data, member->id);
	}

	if (test_eflag(member->conference, EFLAG_DTMF_MEMBER) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_member_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "dtmf-member");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Digits", dtmf);
		switch_event_fire(&event);
	}

	return SWITCH_STATUS_SUCCESS;
}

static int get_canvas_id(conference_member_t *member, const char *val, switch_bool_t watching)
{
	int index = -1;
	int cur;
	
	if (watching) {
		cur = member->watching_canvas_id;
	} else {
		cur = member->canvas_id;
	}

	if (!val) {
		return -1;
	}

	if (switch_is_number(val)) {
		index = atoi(val) - 1;
		
		if (index < 0) {
			index = 0;
		}
	} else {
		index = cur;

		if (!strcasecmp(val, "next")) {
			index++;
		} else if (!strcasecmp(val, "prev")) {
			index--;
		}
	}
		
	if (watching) {
		if (index > member->conference->canvas_count || !member->conference->canvases[index]) {
			index = 0;
		} else if (index < 0) {
			index = member->conference->canvas_count;
		}
	} else {
		if (index >= member->conference->canvas_count || !member->conference->canvases[index]) {
			index = 0;
		} else if (index < 0) {
			index = member->conference->canvas_count;
		}
	}
	
	if (index > MAX_CANVASES || index < 0) {
		return -1;
	}
	
	if (member->conference->canvas_count > 1) {
		if (index > member->conference->canvas_count) {
			return -1;
		}
	} else {
		if (index >= member->conference->canvas_count) {
			return -1;
		}
	}

	return index;
}

static switch_status_t conf_api_sub_watching_canvas(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	int index;
	char *val = (char *) data;

	if (member->conference->canvas_count == 1) {
		stream->write_function(stream, "-ERR Only 1 Canvas\n");
		return SWITCH_STATUS_SUCCESS;
	}

	index = get_canvas_id(member, val, SWITCH_TRUE);

	if (index < 0) {
		stream->write_function(stream, "-ERR Invalid DATA\n");
		return SWITCH_STATUS_SUCCESS;
	}

	member->watching_canvas_id = index;
	reset_member_codec_index(member);
	switch_core_session_request_video_refresh(member->session);
	switch_core_media_gen_key_frame(member->session);
	member->conference->canvases[index]->send_keyframe = 10;
	member->conference->canvases[index]->refresh = 1;
	stream->write_function(stream, "+OK watching canvas %d\n", index + 1);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t conf_api_sub_canvas(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	int index;
	char *val = (char *) data;
	mcu_canvas_t *canvas = NULL;

	if (member->conference->canvas_count == 1) {
		stream->write_function(stream, "-ERR Only 1 Canvas\n");
		return SWITCH_STATUS_SUCCESS;
	}

	switch_mutex_lock(member->conference->canvas_mutex);

	index = get_canvas_id(member, val, SWITCH_FALSE);

	if (index < 0) {
		stream->write_function(stream, "-ERR Invalid DATA\n");
		switch_mutex_unlock(member->conference->canvas_mutex);
		return SWITCH_STATUS_SUCCESS;
	}

	detach_video_layer(member);
	member->canvas_id = index;
	
	canvas = member->conference->canvases[member->canvas_id];
	attach_video_layer(member, canvas, index);
	reset_member_codec_index(member);
	switch_mutex_unlock(member->conference->canvas_mutex);

	switch_core_session_request_video_refresh(member->session);
	switch_core_media_gen_key_frame(member->session);
	member->conference->canvases[index]->send_keyframe = 10;
	member->conference->canvases[index]->refresh = 1;
	stream->write_function(stream, "+OK canvas %d\n", member->canvas_id + 1);

	return SWITCH_STATUS_SUCCESS;
}



static switch_status_t conf_api_sub_layer(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	int index = -1;
	mcu_canvas_t *canvas = NULL;
	char *val = (char *) data;

	if (!val) {
		stream->write_function(stream, "-ERR Invalid DATA\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (member->canvas_id < 0) {
		stream->write_function(stream, "-ERR Invalid Canvas\n");
		return SWITCH_STATUS_FALSE;
	}


	switch_mutex_lock(member->conference->canvas_mutex);

	if (switch_is_number(val)) {
		index = atoi(val) - 1;
		
		if (index < 0) {
			index = 0;
		}
	} else {
		index = member->video_layer_id;

		if (index < 0) index = 0;

		if (!strcasecmp(val, "next")) {
			index++;
		} else if (!strcasecmp(val, "prev")) {
			index--;
		}
	}

	canvas = member->conference->canvases[member->canvas_id];

	if (index >= canvas->total_layers) {
		index = 0;
	}

	if (index < 0) {
		index = canvas->total_layers - 1;
	}

	attach_video_layer(member, canvas, index);
	switch_mutex_unlock(member->conference->canvas_mutex);

	switch_core_session_request_video_refresh(member->session);
	switch_core_media_gen_key_frame(member->session);
	canvas->send_keyframe = 10;
	canvas->refresh = 1;
	stream->write_function(stream, "+OK layer %d\n", member->video_layer_id + 1);

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t conf_api_sub_energy(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;

	if (member == NULL) {
		return SWITCH_STATUS_GENERR;
	}

	if (data) {
		lock_member(member);
		if (!strcasecmp(data, "up")) {
			member->energy_level += 200;
			if (member->energy_level > 1800) {
				member->energy_level = 1800;
			}
		} else if (!strcasecmp(data, "down")) {
			member->energy_level -= 200;
			if (member->energy_level < 0) {
				member->energy_level = 0;
			}
		} else {
			member->energy_level = atoi((char *) data);
		}
		unlock_member(member);
	}
	if (stream != NULL) {
		stream->write_function(stream, "Energy %u = %d\n", member->id, member->energy_level);
	}
	if (test_eflag(member->conference, EFLAG_ENERGY_LEVEL_MEMBER) &&
		data && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_member_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "energy-level-member");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Energy-Level", "%d", member->energy_level);
		switch_event_fire(&event);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t conf_api_sub_auto_position(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
#ifdef OPENAL_POSITIONING
	char *arg = NULL;
	int set = 0;

	if (argc > 2) {
		arg = argv[2];
	}


	if (!zstr(arg)) {
		if (!strcasecmp(arg, "on")) {
			conference_set_flag(conference, CFLAG_POSITIONAL);
			set = 1;
		} else if (!strcasecmp(arg, "off")) {
			conference_clear_flag(conference, CFLAG_POSITIONAL);
		}
	}

	if (set && conference_test_flag(conference, CFLAG_POSITIONAL)) {
		gen_arc(conference, stream);
	}

	stream->write_function(stream, "+OK positioning %s\n", conference_test_flag(conference, CFLAG_POSITIONAL) ? "on" : "off");
	
#else
	stream->write_function(stream, "-ERR not supported\n");
	
#endif

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t conf_api_sub_position(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
#ifndef OPENAL_POSITIONING
	if (stream) stream->write_function(stream, "-ERR not supported\n");
#else
	switch_event_t *event;

	if (member == NULL) {
		return SWITCH_STATUS_GENERR;
	}

	if (member_test_flag(member, MFLAG_NO_POSITIONAL)) {
		if (stream) stream->write_function(stream,
										   "%s has positional audio blocked.\n", switch_channel_get_name(member->channel));
		return SWITCH_STATUS_SUCCESS;
	}

	if (!member->al) {
		if (!member_test_flag(member, MFLAG_POSITIONAL) && member->conference->channels == 2) {
			member_set_flag(member, MFLAG_POSITIONAL);
			member->al = create_al(member->pool);
		} else {
		
			if (stream) {
				stream->write_function(stream, "Positional audio not avalilable %d\n", member->conference->channels);
			}
			return SWITCH_STATUS_FALSE;
		}
	}


	if (data) {
		if (member_parse_position(member, data) != SWITCH_STATUS_SUCCESS) {
			if (stream) {
				stream->write_function(stream, "invalid input!\n");
			}
			return SWITCH_STATUS_FALSE;
		}
	}
	

	if (stream != NULL) {
		stream->write_function(stream, "Position %u = %0.2f:%0.2f:%0.2f\n", member->id, member->al->pos_x, member->al->pos_y, member->al->pos_z);
	}

	if (test_eflag(member->conference, EFLAG_SET_POSITION_MEMBER) &&
		data && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_member_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "set-position-member");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Position", "%0.2f:%0.2f:%0.2f", member->al->pos_x, member->al->pos_y, member->al->pos_z);
		switch_event_fire(&event);
	}

#endif

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t conf_api_sub_volume_in(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	if (data) {
		lock_member(member);
		if (!strcasecmp(data, "up")) {
			member->volume_in_level++;
			switch_normalize_volume(member->volume_in_level);
		} else if (!strcasecmp(data, "down")) {
			member->volume_in_level--;
			switch_normalize_volume(member->volume_in_level);
		} else {
			member->volume_in_level = atoi((char *) data);
			switch_normalize_volume(member->volume_in_level);
		}
		unlock_member(member);

	}
	if (stream != NULL) {
		stream->write_function(stream, "Volume IN %u = %d\n", member->id, member->volume_in_level);
	}
	if (test_eflag(member->conference, EFLAG_VOLUME_IN_MEMBER) &&
		data && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_member_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "volume-in-member");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Volume-Level", "%d", member->volume_in_level);
		switch_event_fire(&event);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t conf_api_sub_volume_out(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	switch_event_t *event;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	if (data) {
		lock_member(member);
		if (!strcasecmp(data, "up")) {
			member->volume_out_level++;
			switch_normalize_volume(member->volume_out_level);
		} else if (!strcasecmp(data, "down")) {
			member->volume_out_level--;
			switch_normalize_volume(member->volume_out_level);
		} else {
			member->volume_out_level = atoi((char *) data);
			switch_normalize_volume(member->volume_out_level);
		}
		unlock_member(member);
	}
	if (stream != NULL) {
		stream->write_function(stream, "Volume OUT %u = %d\n", member->id, member->volume_out_level);
	}
	if (test_eflag(member->conference, EFLAG_VOLUME_OUT_MEMBER) && data &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_member_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "volume-out-member");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Volume-Level", "%d", member->volume_out_level);
		switch_event_fire(&event);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t conf_api_sub_vid_bandwidth(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	int32_t i, video_write_bandwidth;

	if (!conference_test_flag(conference, CFLAG_MINIMIZE_VIDEO_ENCODING)) {
		stream->write_function(stream, "Bandwidth control not available.\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (!argv[2]) {
		stream->write_function(stream, "Invalid input\n");
		return SWITCH_STATUS_SUCCESS;
	}

	video_write_bandwidth = switch_parse_bandwidth_string(argv[2]);
	for (i = 0; i >= conference->canvas_count; i++) {
		if (conference->canvases[i]) {
			conference->canvases[i]->video_write_bandwidth = video_write_bandwidth;
		}
	}

	stream->write_function(stream, "Set Bandwidth %d\n", video_write_bandwidth);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t conf_api_sub_vid_fps(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	float fps = 0;

	if (!conference->canvas) {
		stream->write_function(stream, "Conference is not in mixing mode\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (!argv[2]) {
		stream->write_function(stream, "Current FPS [%0.2f]\n", conference->video_fps.fps);
		return SWITCH_STATUS_SUCCESS;
	}

	fps = atof(argv[2]);

	if (conference_set_fps(conference, fps)) {
		stream->write_function(stream, "FPS set to [%s]\n", argv[2]);
	} else {
		stream->write_function(stream, "Invalid FPS [%s]\n", argv[2]);
	}

	return SWITCH_STATUS_SUCCESS;
	
}

static switch_status_t conf_api_sub_write_png(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	mcu_canvas_t *canvas = NULL;

	if (!argv[2]) {
		stream->write_function(stream, "Invalid input\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (!conference->canvas_count) {
		stream->write_function(stream, "Conference is not in mixing mode\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (conference->canvas_count > 1) {
		/* pick super canvas */
		canvas = conference->canvases[conference->canvas_count];
	} else {
		canvas = conference->canvases[0];
	}

	switch_mutex_lock(canvas->mutex);
	status = switch_img_write_png(canvas->img, argv[2]);
	switch_mutex_unlock(canvas->mutex);

	stream->write_function(stream, "%s\n", status == SWITCH_STATUS_SUCCESS ? "+OK" : "-ERR");

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t conf_api_sub_vid_layout(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	video_layout_t *vlayout = NULL;
	int idx = 0;

	if (!argv[2]) {
		stream->write_function(stream, "Invalid input\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (!conference->canvas) {
		stream->write_function(stream, "Conference is not in mixing mode\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (!strcasecmp(argv[2], "list")) {
		switch_hash_index_t *hi;
		void *val;
		const void *vvar;
		for (hi = switch_core_hash_first(conference->layout_hash); hi; hi = switch_core_hash_next(&hi)) {
			switch_core_hash_this(hi, &vvar, NULL, &val);
			stream->write_function(stream, "%s\n", (char *)vvar);
		}
		return SWITCH_STATUS_SUCCESS;
	}

	if (!strcasecmp(argv[2], "group")) {
		layout_group_t *lg = NULL;

		if (!argv[3]) {
				stream->write_function(stream, "Group name not specified.\n");
				return SWITCH_STATUS_SUCCESS;
		} else {
			if (((lg = switch_core_hash_find(conference->layout_group_hash, argv[3])))) {
				vlayout = find_best_layout(conference, lg, 0);
			}

			if (!vlayout) {
				stream->write_function(stream, "Invalid group layout [%s]\n", argv[3]);
				return SWITCH_STATUS_SUCCESS;
			}
			
			stream->write_function(stream, "Change to layout group [%s]\n", argv[3]);
			conference->video_layout_group = switch_core_strdup(conference->pool, argv[3]);

			if (argv[4]) {
				idx = atoi(argv[4]);
			}
		}
	}

	if (!vlayout && (vlayout = switch_core_hash_find(conference->layout_hash, argv[2]))) {
		conference->video_layout_group = NULL;
		if (argv[3]) {
			idx = atoi(argv[3]);
		}
	}

	if (!vlayout) {
		stream->write_function(stream, "Invalid layout [%s]\n", argv[2]);
		return SWITCH_STATUS_SUCCESS;
	}
	
	if (idx < 0 || idx > conference->canvas_count - 1) idx = 0;
	
	stream->write_function(stream, "Change canvas %d to layout [%s]\n", idx, vlayout->name);

	switch_mutex_lock(conference->member_mutex);
	conference->canvases[idx]->new_vlayout = vlayout;
	switch_mutex_unlock(conference->member_mutex);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t conf_api_sub_list(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	int ret_status = SWITCH_STATUS_GENERR;
	int count = 0;
	switch_hash_index_t *hi;
	void *val;
	char *d = ";";
	int pretty = 0;
	int summary = 0;
	int countonly = 0;
	int argofs = (argc >= 2 && strcasecmp(argv[1], "list") == 0);	/* detect being called from chat vs. api */

	if (argv[1 + argofs]) {
		if (argv[2 + argofs] && !strcasecmp(argv[1 + argofs], "delim")) {
			d = argv[2 + argofs];

			if (*d == '"') {
				if (++d) {
					char *p;
					if ((p = strchr(d, '"'))) {
						*p = '\0';
					}
				} else {
					d = ";";
				}
			}
		} else if (strcasecmp(argv[1 + argofs], "pretty") == 0) {
			pretty = 1;
		} else if (strcasecmp(argv[1 + argofs], "summary") == 0) {
			summary = 1;
		} else if (strcasecmp(argv[1 + argofs], "count") == 0) {
			countonly = 1;
		}
	}

	if (conference == NULL) {
		switch_mutex_lock(globals.hash_mutex);
		for (hi = switch_core_hash_first(globals.conference_hash); hi; hi = switch_core_hash_next(&hi)) {
			int fcount = 0;
			switch_core_hash_this(hi, NULL, NULL, &val);
			conference = (conference_obj_t *) val;

			stream->write_function(stream, "Conference %s (%u member%s rate: %u%s flags: ",
								   conference->name,
								   conference->count,
								   conference->count == 1 ? "" : "s", conference->rate, conference_test_flag(conference, CFLAG_LOCKED) ? " locked" : "");

			if (conference_test_flag(conference, CFLAG_LOCKED)) {
				stream->write_function(stream, "%slocked", fcount ? "|" : "");
				fcount++;
			}

			if (conference_test_flag(conference, CFLAG_DESTRUCT)) {
				stream->write_function(stream, "%sdestruct", fcount ? "|" : "");
				fcount++;
			}

			if (conference_test_flag(conference, CFLAG_WAIT_MOD)) {
				stream->write_function(stream, "%swait_mod", fcount ? "|" : "");
				fcount++;
			}

			if (conference_test_flag(conference, CFLAG_AUDIO_ALWAYS)) {
				stream->write_function(stream, "%saudio_always", fcount ? "|" : "");
				fcount++;
			}

			if (conference_test_flag(conference, CFLAG_RUNNING)) {
				stream->write_function(stream, "%srunning", fcount ? "|" : "");
				fcount++;
			}

			if (conference_test_flag(conference, CFLAG_ANSWERED)) {
				stream->write_function(stream, "%sanswered", fcount ? "|" : "");
				fcount++;
			}

			if (conference_test_flag(conference, CFLAG_ENFORCE_MIN)) {
				stream->write_function(stream, "%senforce_min", fcount ? "|" : "");
				fcount++;
			}

			if (conference_test_flag(conference, CFLAG_BRIDGE_TO)) {
				stream->write_function(stream, "%sbridge_to", fcount ? "|" : "");
				fcount++;
			}

			if (conference_test_flag(conference, CFLAG_DYNAMIC)) {
				stream->write_function(stream, "%sdynamic", fcount ? "|" : "");
				fcount++;
			}

			if (conference_test_flag(conference, CFLAG_EXIT_SOUND)) {
				stream->write_function(stream, "%sexit_sound", fcount ? "|" : "");
				fcount++;
			}

			if (conference_test_flag(conference, CFLAG_ENTER_SOUND)) {
				stream->write_function(stream, "%senter_sound", fcount ? "|" : "");
				fcount++;
			}

			if (conference->record_count > 0) {
				stream->write_function(stream, "%srecording", fcount ? "|" : "");
				fcount++;
			}

			if (conference_test_flag(conference, CFLAG_VID_FLOOR)) {
				stream->write_function(stream, "%svideo_floor_only", fcount ? "|" : "");
				fcount++;
			}

			if (conference_test_flag(conference, CFLAG_RFC4579)) {
				stream->write_function(stream, "%svideo_rfc4579", fcount ? "|" : "");
				fcount++;
			}

			if (conference_test_flag(conference, CFLAG_LIVEARRAY_SYNC)) {
				stream->write_function(stream, "%slivearray_sync", fcount ? "|" : "");
				fcount++;
			}

			if (conference_test_flag(conference, CFLAG_VID_FLOOR_LOCK)) {
				stream->write_function(stream, "%svideo_floor_lock", fcount ? "|" : "");
				fcount++;
			}

			if (conference_test_flag(conference, CFLAG_TRANSCODE_VIDEO)) {
				stream->write_function(stream, "%stranscode_video", fcount ? "|" : "");
				fcount++;
			}

			if (conference_test_flag(conference, CFLAG_VIDEO_MUXING)) {
				stream->write_function(stream, "%svideo_muxing", fcount ? "|" : "");
				fcount++;
			}

			if (conference_test_flag(conference, CFLAG_MINIMIZE_VIDEO_ENCODING)) {
				stream->write_function(stream, "%sminimize_video_encoding", fcount ? "|" : "");
				fcount++;
			}

			if (conference_test_flag(conference, CFLAG_MANAGE_INBOUND_VIDEO_BITRATE)) {
				stream->write_function(stream, "%smanage_inbound_bitrate", fcount ? "|" : "");
				fcount++;
			}

			if (conference_test_flag(conference, CFLAG_JSON_STATUS)) {
				stream->write_function(stream, "%sjson_status", fcount ? "|" : "");
				fcount++;
			}

			if (conference_test_flag(conference, CFLAG_VIDEO_BRIDGE_FIRST_TWO)) {
				stream->write_function(stream, "%svideo_bridge_first_two", fcount ? "|" : "");
				fcount++;
			}

			if (conference_test_flag(conference, CFLAG_VIDEO_REQUIRED_FOR_CANVAS)) {
				stream->write_function(stream, "%svideo_required_for_canvas", fcount ? "|" : "");
				fcount++;
			}

			if (conference_test_flag(conference, CFLAG_PERSONAL_CANVAS)) {
				stream->write_function(stream, "%spersonal_canvas", fcount ? "|" : "");
				fcount++;
			}

			if (!fcount) {
				stream->write_function(stream, "none");
			}

			stream->write_function(stream, ")\n");

			count++;
			if (!summary) {
				if (pretty) {
					conference_list_pretty(conference, stream);
				} else {
					conference_list(conference, stream, d);
				}
			}
		}
		switch_mutex_unlock(globals.hash_mutex);
	} else {
		count++;
		if (countonly) {
			conference_list_count_only(conference, stream);
		} else if (pretty) {
			conference_list_pretty(conference, stream);
		} else {
			conference_list(conference, stream, d);
		}
	}

	if (!count) {
		stream->write_function(stream, "No active conferences.\n");
	}

	ret_status = SWITCH_STATUS_SUCCESS;

	return ret_status;
}

static switch_status_t conf_api_sub_floor(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	switch_mutex_lock(member->conference->mutex);

	if (member->conference->floor_holder == member) {
		conference_set_floor_holder(member->conference, NULL);
		if (stream != NULL) {
			stream->write_function(stream, "OK floor none\n");
		}
	} else if (member->conference->floor_holder == NULL) {
		conference_set_floor_holder(member->conference, member);
		if (stream != NULL) {
			stream->write_function(stream, "OK floor %u\n", member->id);
		}
	} else {
		if (stream != NULL) {
			stream->write_function(stream, "ERR floor is held by %u\n", member->conference->floor_holder->id);
		}
	}

	switch_mutex_unlock(member->conference->mutex);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t conf_api_sub_clear_vid_floor(conference_obj_t *conference, switch_stream_handle_t *stream, void *data)
{

	switch_mutex_lock(conference->mutex);
	conference_clear_flag(conference, CFLAG_VID_FLOOR_LOCK);
	//conference_set_video_floor_holder(conference, NULL);
	switch_mutex_unlock(conference->mutex);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t conf_api_sub_vid_mute_img(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	char *text = (char *) data;
	mcu_layer_t *layer = NULL;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	if (!switch_channel_test_flag(member->channel, CF_VIDEO)) {
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(layer->canvas->mutex);

	if (member->video_layer_id == -1 || !layer->canvas) {
		goto end;
	}

	member->video_mute_png = NULL;
	layer = &layer->canvas->layers[member->video_layer_id];

	if (text) {
		switch_img_free(&layer->mute_img);
	}

	if (text && strcasecmp(text, "clear")) {
		member->video_mute_png = switch_core_strdup(member->pool, text);
	}

 end:

	stream->write_function(stream, "%s\n", member->video_mute_png ? member->video_mute_png : "_undef_");

	switch_mutex_unlock(layer->canvas->mutex);

	return SWITCH_STATUS_SUCCESS;

}


static switch_status_t conf_api_sub_vid_logo_img(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	char *text = (char *) data;
	mcu_layer_t *layer = NULL;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	if (!switch_channel_test_flag(member->channel, CF_VIDEO)) {
		return SWITCH_STATUS_FALSE;
	}

	if (member->video_layer_id == -1 || !member->conference->canvas) {
		goto end;
	}



	layer = &member->conference->canvas->layers[member->video_layer_id];

	switch_mutex_lock(layer->canvas->mutex);
	
	if (strcasecmp(text, "clear")) {
		member->video_logo = switch_core_strdup(member->pool, text);
	}

	layer_set_logo(member, layer, text);

 end:

	stream->write_function(stream, "+OK\n");

	switch_mutex_unlock(layer->canvas->mutex);

	return SWITCH_STATUS_SUCCESS;

}

static switch_status_t conf_api_sub_vid_res_id(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	char *text = (char *) data;
	//mcu_layer_t *layer = NULL;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	if (!switch_channel_test_flag(member->channel, CF_VIDEO)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!member->conference->canvas) {
		stream->write_function(stream, "-ERR conference is not in mixing mode\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (zstr(text)) {
		stream->write_function(stream, "-ERR missing arg\n");
		return SWITCH_STATUS_SUCCESS;
	}

	switch_mutex_lock(member->conference->canvas->mutex);

	//layer = &member->conference->canvas->layers[member->video_layer_id];
	
	if (!strcasecmp(text, "clear") || (member->video_reservation_id && !strcasecmp(text, member->video_reservation_id))) {
		member->video_reservation_id = NULL;
		stream->write_function(stream, "+OK reservation_id cleared\n");
	} else {
		member->video_reservation_id = switch_core_strdup(member->pool, text);
		stream->write_function(stream, "+OK reservation_id %s\n", text);
	}

	detach_video_layer(member);

	switch_mutex_unlock(member->conference->canvas->mutex);

	return SWITCH_STATUS_SUCCESS;

}

static switch_status_t conf_api_sub_vid_banner(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	mcu_layer_t *layer = NULL;
	char *text = (char *) data;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	switch_url_decode(text);

	if (!switch_channel_test_flag(member->channel, CF_VIDEO)) {
		stream->write_function(stream, "Channel %s does not have video capability!\n", switch_channel_get_name(member->channel));
		return SWITCH_STATUS_SUCCESS;
	}

	switch_mutex_lock(member->conference->mutex);

	if (member->video_layer_id == -1 || !member->conference->canvas) {
		stream->write_function(stream, "Channel %s is not in a video layer\n", switch_channel_get_name(member->channel));
		goto end;
	}

	if (zstr(text)) {
		stream->write_function(stream, "No text supplied\n", switch_channel_get_name(member->channel));
		goto end;
	}

	layer = &member->conference->canvas->layers[member->video_layer_id];

	member->video_banner_text = switch_core_strdup(member->pool, text);

	layer_set_banner(member, layer, NULL);

	stream->write_function(stream, "+OK\n");

 end:

	switch_mutex_unlock(member->conference->mutex);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t conf_api_sub_vid_floor(conference_member_t *member, switch_stream_handle_t *stream, void *data)
{
	int force = 0;

	if (member == NULL)
		return SWITCH_STATUS_GENERR;

	if (!switch_channel_test_flag(member->channel, CF_VIDEO) && !member->avatar_png_img) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Channel %s does not have video capability!\n", switch_channel_get_name(member->channel));
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(member->conference->mutex);

	if (data && switch_stristr("force", (char *) data)) {
		force = 1;
	}

	if (member->conference->video_floor_holder == member->id && conference_test_flag(member->conference, CFLAG_VID_FLOOR_LOCK)) {
		conference_clear_flag(member->conference, CFLAG_VID_FLOOR_LOCK);

		conference_set_floor_holder(member->conference, member);
		if (stream == NULL) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "conference %s OK video floor auto\n", member->conference->name);
		} else {
			stream->write_function(stream, "OK floor none\n");
		} 
		
	} else if (force || member->conference->video_floor_holder == 0) {
		conference_set_flag(member->conference, CFLAG_VID_FLOOR_LOCK);
		conference_set_video_floor_holder(member->conference, member, SWITCH_TRUE);
		if (test_eflag(member->conference, EFLAG_FLOOR_CHANGE)) {
			if (stream == NULL) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "conference %s OK video floor %d %s\n", 
								  member->conference->name, member->id, switch_channel_get_name(member->channel));
			} else {
				stream->write_function(stream, "OK floor %u\n", member->id);
			}
		}
	} else {
		if (stream == NULL) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "conference %s floor already held by %d %s\n", 
							  member->conference->name, member->id, switch_channel_get_name(member->channel));
		} else {
			stream->write_function(stream, "ERR floor is held by %u\n", member->conference->video_floor_holder);
		}
	}

	switch_mutex_unlock(member->conference->mutex);

	return SWITCH_STATUS_SUCCESS;
}

static switch_xml_t add_x_tag(switch_xml_t x_member, const char *name, const char *value, int off)
{
	switch_size_t dlen;
	char *data;
	switch_xml_t x_tag;

	if (!value) {
		return 0;
	}

	dlen = strlen(value) * 3 + 1;

	x_tag = switch_xml_add_child_d(x_member, name, off);
	switch_assert(x_tag);

	switch_zmalloc(data, dlen);

	switch_url_encode(value, data, dlen);
	switch_xml_set_txt_d(x_tag, data);
	free(data);

	return x_tag;
}

static void conference_xlist(conference_obj_t *conference, switch_xml_t x_conference, int off)
{
	conference_member_t *member = NULL;
	switch_xml_t x_member = NULL, x_members = NULL, x_flags;
	int moff = 0;
	char i[30] = "";
	char *ival = i;
	switch_assert(conference != NULL);
	switch_assert(x_conference != NULL);

	switch_xml_set_attr_d(x_conference, "name", conference->name);
	switch_snprintf(i, sizeof(i), "%d", conference->count);
	switch_xml_set_attr_d(x_conference, "member-count", ival);
	switch_snprintf(i, sizeof(i), "%d", conference->count_ghosts);
	switch_xml_set_attr_d(x_conference, "ghost-count", ival);
	switch_snprintf(i, sizeof(i), "%u", conference->rate);
	switch_xml_set_attr_d(x_conference, "rate", ival);
	switch_xml_set_attr_d(x_conference, "uuid", conference->uuid_str);
		
	if (conference_test_flag(conference, CFLAG_LOCKED)) {
		switch_xml_set_attr_d(x_conference, "locked", "true");
	}

	if (conference_test_flag(conference, CFLAG_DESTRUCT)) {
		switch_xml_set_attr_d(x_conference, "destruct", "true");
	}

	if (conference_test_flag(conference, CFLAG_WAIT_MOD)) {
		switch_xml_set_attr_d(x_conference, "wait_mod", "true");
	}

	if (conference_test_flag(conference, CFLAG_AUDIO_ALWAYS)) {
		switch_xml_set_attr_d(x_conference, "audio_always", "true");
	}

	if (conference_test_flag(conference, CFLAG_RUNNING)) {
		switch_xml_set_attr_d(x_conference, "running", "true");
	}

	if (conference_test_flag(conference, CFLAG_ANSWERED)) {
		switch_xml_set_attr_d(x_conference, "answered", "true");
	}

	if (conference_test_flag(conference, CFLAG_ENFORCE_MIN)) {
		switch_xml_set_attr_d(x_conference, "enforce_min", "true");
	}

	if (conference_test_flag(conference, CFLAG_BRIDGE_TO)) {
		switch_xml_set_attr_d(x_conference, "bridge_to", "true");
	}

	if (conference_test_flag(conference, CFLAG_DYNAMIC)) {
		switch_xml_set_attr_d(x_conference, "dynamic", "true");
	}

	if (conference_test_flag(conference, CFLAG_EXIT_SOUND)) {
		switch_xml_set_attr_d(x_conference, "exit_sound", "true");
	}
	
	if (conference_test_flag(conference, CFLAG_ENTER_SOUND)) {
		switch_xml_set_attr_d(x_conference, "enter_sound", "true");
	}
	
	if (conference->max_members > 0) {
		switch_snprintf(i, sizeof(i), "%d", conference->max_members);
		switch_xml_set_attr_d(x_conference, "max_members", ival);
	}

	if (conference->record_count > 0) {
		switch_xml_set_attr_d(x_conference, "recording", "true");
	}

	if (conference->endconf_grace_time > 0) {
		switch_snprintf(i, sizeof(i), "%u", conference->endconf_grace_time);
		switch_xml_set_attr_d(x_conference, "endconf_grace_time", ival);
	}

	if (conference_test_flag(conference, CFLAG_VID_FLOOR)) {
		switch_xml_set_attr_d(x_conference, "video_floor_only", "true");
	}

	if (conference_test_flag(conference, CFLAG_RFC4579)) {
		switch_xml_set_attr_d(x_conference, "video_rfc4579", "true");
	}

	switch_snprintf(i, sizeof(i), "%d", switch_epoch_time_now(NULL) - conference->run_time);
	switch_xml_set_attr_d(x_conference, "run_time", ival);

	if (conference->agc_level) {
		char tmp[30] = "";
		switch_snprintf(tmp, sizeof(tmp), "%d", conference->agc_level);
		switch_xml_set_attr_d_buf(x_conference, "agc", tmp);
	}

	x_members = switch_xml_add_child_d(x_conference, "members", 0);
	switch_assert(x_members);

	switch_mutex_lock(conference->member_mutex);

	for (member = conference->members; member; member = member->next) {
		switch_channel_t *channel;
		switch_caller_profile_t *profile;
		char *uuid;
		//char *name;
		uint32_t count = 0;
		switch_xml_t x_tag;
		int toff = 0;
		char tmp[50] = "";

		if (member_test_flag(member, MFLAG_NOCHANNEL)) {
			if (member->rec_path) {
				x_member = switch_xml_add_child_d(x_members, "member", moff++);
				switch_assert(x_member);
				switch_xml_set_attr_d(x_member, "type", "recording_node");
				/* or:
				x_member = switch_xml_add_child_d(x_members, "recording_node", moff++);
				*/

				x_tag = switch_xml_add_child_d(x_member, "record_path", count++);
				if (member_test_flag(member, MFLAG_PAUSE_RECORDING)) {
					switch_xml_set_attr_d(x_tag, "status", "paused");
				}
				switch_xml_set_txt_d(x_tag, member->rec_path);

				x_tag = switch_xml_add_child_d(x_member, "join_time", count++);
				switch_xml_set_attr_d(x_tag, "type", "UNIX-epoch");
				switch_snprintf(i, sizeof(i), "%d", member->rec_time);
				switch_xml_set_txt_d(x_tag, i);
			}
			continue;
		}

		uuid = switch_core_session_get_uuid(member->session);
		channel = switch_core_session_get_channel(member->session);
		profile = switch_channel_get_caller_profile(channel);
		//name = switch_channel_get_name(channel);


		x_member = switch_xml_add_child_d(x_members, "member", moff++);
		switch_assert(x_member);
		switch_xml_set_attr_d(x_member, "type", "caller");

		switch_snprintf(i, sizeof(i), "%d", member->id);

		add_x_tag(x_member, "id", i, toff++);
		add_x_tag(x_member, "uuid", uuid, toff++);
		add_x_tag(x_member, "caller_id_name", profile->caller_id_name, toff++);
		add_x_tag(x_member, "caller_id_number", profile->caller_id_number, toff++);


		switch_snprintf(i, sizeof(i), "%d", switch_epoch_time_now(NULL) - member->join_time);
		add_x_tag(x_member, "join_time", i, toff++);
		
		switch_snprintf(i, sizeof(i), "%d", switch_epoch_time_now(NULL) - member->last_talking);
		add_x_tag(x_member, "last_talking", member->last_talking ? i : "N/A", toff++);

		switch_snprintf(i, sizeof(i), "%d", member->energy_level);
		add_x_tag(x_member, "energy", i, toff++);

		switch_snprintf(i, sizeof(i), "%d", member->volume_in_level);
		add_x_tag(x_member, "volume_in", i, toff++);

		switch_snprintf(i, sizeof(i), "%d", member->volume_out_level);
		add_x_tag(x_member, "volume_out", i, toff++);
		
		x_flags = switch_xml_add_child_d(x_member, "flags", count++);
		switch_assert(x_flags);

		x_tag = switch_xml_add_child_d(x_flags, "can_hear", count++);
		switch_xml_set_txt_d(x_tag, member_test_flag(member, MFLAG_CAN_HEAR) ? "true" : "false");

		x_tag = switch_xml_add_child_d(x_flags, "can_speak", count++);
		switch_xml_set_txt_d(x_tag, member_test_flag(member, MFLAG_CAN_SPEAK) ? "true" : "false");

		x_tag = switch_xml_add_child_d(x_flags, "mute_detect", count++);
		switch_xml_set_txt_d(x_tag, member_test_flag(member, MFLAG_MUTE_DETECT) ? "true" : "false");

		x_tag = switch_xml_add_child_d(x_flags, "talking", count++);
		switch_xml_set_txt_d(x_tag, member_test_flag(member, MFLAG_TALKING) ? "true" : "false");

		x_tag = switch_xml_add_child_d(x_flags, "has_video", count++);
		switch_xml_set_txt_d(x_tag, switch_channel_test_flag(switch_core_session_get_channel(member->session), CF_VIDEO) ? "true" : "false");

		x_tag = switch_xml_add_child_d(x_flags, "video_bridge", count++);
		switch_xml_set_txt_d(x_tag, member_test_flag(member, MFLAG_VIDEO_BRIDGE) ? "true" : "false");

		x_tag = switch_xml_add_child_d(x_flags, "has_floor", count++);
		switch_xml_set_txt_d(x_tag, (member == member->conference->floor_holder) ? "true" : "false");

		x_tag = switch_xml_add_child_d(x_flags, "is_moderator", count++);
		switch_xml_set_txt_d(x_tag, member_test_flag(member, MFLAG_MOD) ? "true" : "false");

		x_tag = switch_xml_add_child_d(x_flags, "end_conference", count++);
		switch_xml_set_txt_d(x_tag, member_test_flag(member, MFLAG_ENDCONF) ? "true" : "false");

		x_tag = switch_xml_add_child_d(x_flags, "is_ghost", count++);
		switch_xml_set_txt_d(x_tag, member_test_flag(member, MFLAG_GHOST) ? "true" : "false");

		switch_snprintf(tmp, sizeof(tmp), "%d", member->volume_out_level);
		add_x_tag(x_member, "output-volume", tmp, toff++);

		switch_snprintf(tmp, sizeof(tmp), "%d", member->agc_volume_in_level ? member->agc_volume_in_level : member->volume_in_level);
		add_x_tag(x_member, "input-volume", tmp, toff++);

		switch_snprintf(tmp, sizeof(tmp), "%d", member->agc_volume_in_level);
		add_x_tag(x_member, "auto-adjusted-input-volume", tmp, toff++);

	}

	switch_mutex_unlock(conference->member_mutex);
}
static switch_status_t conf_api_sub_xml_list(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	int count = 0;
	switch_hash_index_t *hi;
	void *val;
	switch_xml_t x_conference, x_conferences;
	int off = 0;
	char *ebuf;

	x_conferences = switch_xml_new("conferences");
	switch_assert(x_conferences);

	if (conference == NULL) {
		switch_mutex_lock(globals.hash_mutex);
		for (hi = switch_core_hash_first(globals.conference_hash); hi; hi = switch_core_hash_next(&hi)) {
			switch_core_hash_this(hi, NULL, NULL, &val);
			conference = (conference_obj_t *) val;

			x_conference = switch_xml_add_child_d(x_conferences, "conference", off++);
			switch_assert(conference);

			count++;
			conference_xlist(conference, x_conference, off);

		}
		switch_mutex_unlock(globals.hash_mutex);
	} else {
		x_conference = switch_xml_add_child_d(x_conferences, "conference", off++);
		switch_assert(conference);
		count++;
		conference_xlist(conference, x_conference, off);
	}


	ebuf = switch_xml_toxml(x_conferences, SWITCH_TRUE);

	stream->write_function(stream, "%s", ebuf);

	switch_xml_free(x_conferences);
	free(ebuf);

	return SWITCH_STATUS_SUCCESS;
}

static void switch_fnode_toggle_pause(conference_file_node_t *fnode, switch_stream_handle_t *stream)
{
	if (fnode) { 
		if (switch_test_flag(fnode, NFLAG_PAUSE)) {
				stream->write_function(stream, "+OK Resume\n");
			switch_clear_flag(fnode, NFLAG_PAUSE);
			} else {
				stream->write_function(stream, "+OK Pause\n");
			switch_set_flag(fnode, NFLAG_PAUSE);
			}
		}
}

static switch_status_t conf_api_sub_pause_play(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	if (argc == 2) {
		switch_mutex_lock(conference->mutex);
		switch_fnode_toggle_pause(conference->fnode, stream);
		switch_mutex_unlock(conference->mutex);

		return SWITCH_STATUS_SUCCESS;
	}

	if (argc == 3) {
		uint32_t id = atoi(argv[2]);
		conference_member_t *member;

		if ((member = conference_member_get(conference, id))) {
			switch_mutex_lock(member->fnode_mutex);
			switch_fnode_toggle_pause(member->fnode, stream);
			switch_mutex_unlock(member->fnode_mutex);
			switch_thread_rwlock_unlock(member->rwlock);
			return SWITCH_STATUS_SUCCESS;
		} else {
			stream->write_function(stream, "Member: %u not found.\n", id);
		}
	}

	return SWITCH_STATUS_GENERR;
}

static void switch_fnode_seek(conference_file_node_t *fnode, switch_stream_handle_t *stream, char *arg)
{
	if (fnode && fnode->type == NODE_TYPE_FILE) { 
		unsigned int samps = 0;
		unsigned int pos = 0;
		
		if (*arg == '+' || *arg == '-') {
				int step;
				int32_t target;
			if (!(step = atoi(arg))) {
					step = 1000;
				}
					
			samps = step * (fnode->fh.native_rate / 1000);
			target = (int32_t)fnode->fh.pos + samps;
				
				if (target < 0) {
					target = 0;
				}
					
				stream->write_function(stream, "+OK seek to position %d\n", target);
			switch_core_file_seek(&fnode->fh, &pos, target, SEEK_SET);
				
			} else {
			samps = switch_atoui(arg) * (fnode->fh.native_rate / 1000);
				stream->write_function(stream, "+OK seek to position %d\n", samps);
			switch_core_file_seek(&fnode->fh, &pos, samps, SEEK_SET);
			}
		}
}

static switch_status_t conf_api_sub_file_seek(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	if (argc == 3) {
		switch_mutex_lock(conference->mutex);
		switch_fnode_seek(conference->fnode, stream, argv[2]);
		switch_mutex_unlock(conference->mutex);
			
		return SWITCH_STATUS_SUCCESS;
	}

	if (argc == 4) {
		uint32_t id = atoi(argv[3]);
		conference_member_t *member = conference_member_get(conference, id);
		if (member == NULL) {
		    stream->write_function(stream, "Member: %u not found.\n", id);
		    return SWITCH_STATUS_GENERR;
		}
		
		switch_mutex_lock(member->fnode_mutex);
		switch_fnode_seek(member->fnode, stream, argv[2]);
		switch_mutex_unlock(member->fnode_mutex);
		switch_thread_rwlock_unlock(member->rwlock);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_GENERR;
}

static switch_status_t conf_api_sub_play(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	int ret_status = SWITCH_STATUS_GENERR;
	switch_event_t *event;
	uint8_t async = 0;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	if ((argc == 4 && !strcasecmp(argv[3], "async")) || (argc == 5 && !strcasecmp(argv[4], "async"))) {
		argc--;
		async++;
	}

	if (argc == 3) {
		if (conference_play_file(conference, argv[2], 0, NULL, async) == SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "(play) Playing file %s\n", argv[2]);
			if (test_eflag(conference, EFLAG_PLAY_FILE) &&
				switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
				conference_add_event_data(conference, event);

				if (conference->fnode && conference->fnode->fh.params) {
					switch_event_merge(event, conference->fnode->fh.params);
				}
				
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "play-file");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "File", argv[2]);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Async", async ? "true" : "false");
				switch_event_fire(&event);
			}
		} else {
			stream->write_function(stream, "(play) File: %s not found.\n", argv[2] ? argv[2] : "(unspecified)");
		}
		ret_status = SWITCH_STATUS_SUCCESS;
	} else if (argc >= 4) {
		uint32_t id = atoi(argv[3]);
		conference_member_t *member;
		switch_bool_t mux = SWITCH_TRUE;

		if (argc > 4 && !strcasecmp(argv[4], "nomux")) {
			mux = SWITCH_FALSE;
		}

		if ((member = conference_member_get(conference, id))) {
			if (conference_member_play_file(member, argv[2], 0, mux) == SWITCH_STATUS_SUCCESS) {
				stream->write_function(stream, "(play) Playing file %s to member %u\n", argv[2], id);
				if (test_eflag(conference, EFLAG_PLAY_FILE_MEMBER) &&
					switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
					conference_add_event_member_data(member, event);

					if (member->fnode->fh.params) {
						switch_event_merge(event, member->fnode->fh.params);
					}

					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "play-file-member");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "File", argv[2]);
					switch_event_fire(&event);
				}
			} else {
				stream->write_function(stream, "(play) File: %s not found.\n", argv[2] ? argv[2] : "(unspecified)");
			}
			switch_thread_rwlock_unlock(member->rwlock);
			ret_status = SWITCH_STATUS_SUCCESS;
		} else {
			stream->write_function(stream, "Member: %u not found.\n", id);
		}
	}

	return ret_status;
}

static switch_status_t conf_api_sub_say(conference_obj_t *conference, switch_stream_handle_t *stream, const char *text)
{
	switch_event_t *event;

	if (zstr(text)) {
		stream->write_function(stream, "(say) Error! No text.\n");
		return SWITCH_STATUS_GENERR;
	}

	if (conference_say(conference, text, 0) != SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "(say) Error!\n");
		return SWITCH_STATUS_GENERR;
	}

	stream->write_function(stream, "(say) OK\n");
	if (test_eflag(conference, EFLAG_SPEAK_TEXT) && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_data(conference, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "speak-text");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Text", text);
		switch_event_fire(&event);
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t conf_api_sub_saymember(conference_obj_t *conference, switch_stream_handle_t *stream, const char *text)
{
	int ret_status = SWITCH_STATUS_GENERR;
	char *expanded = NULL;
	char *start_text = NULL;
	char *workspace = NULL;
	uint32_t id = 0;
	conference_member_t *member = NULL;
	switch_event_t *event;

	if (zstr(text)) {
		stream->write_function(stream, "(saymember) No Text!\n");
		goto done;
	}

	if (!(workspace = strdup(text))) {
		stream->write_function(stream, "(saymember) Memory Error!\n");
		goto done;
	}

	if ((start_text = strchr(workspace, ' '))) {
		*start_text++ = '\0';
		text = start_text;
	}

	id = atoi(workspace);

	if (!id || zstr(text)) {
		stream->write_function(stream, "(saymember) No Text!\n");
		goto done;
	}

	if (!(member = conference_member_get(conference, id))) {
		stream->write_function(stream, "(saymember) Unknown Member %u!\n", id);
		goto done;
	}

	if ((expanded = switch_channel_expand_variables(switch_core_session_get_channel(member->session), (char *) text)) != text) {
		text = expanded;
	} else {
		expanded = NULL;
	}

	if (!text || conference_member_say(member, (char *) text, 0) != SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "(saymember) Error!\n");
		goto done;
	}

	stream->write_function(stream, "(saymember) OK\n");
	if (test_eflag(member->conference, EFLAG_SPEAK_TEXT_MEMBER) &&
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_member_data(member, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "speak-text-member");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Text", text);
		switch_event_fire(&event);
	}
	ret_status = SWITCH_STATUS_SUCCESS;

  done:

	if (member) {
		switch_thread_rwlock_unlock(member->rwlock);
	}

	switch_safe_free(workspace);
	switch_safe_free(expanded);
	return ret_status;
}

static switch_status_t conf_api_sub_stop(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	uint8_t current = 0, all = 0, async = 0;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	if (argc > 2) {
		current = strcasecmp(argv[2], "current") ? 0 : 1;
		all = strcasecmp(argv[2], "all") ? 0 : 1;
		async = strcasecmp(argv[2], "async") ? 0 : 1;
	} else {
		all = 1;
	}

	if (!(current || all || async))
		return SWITCH_STATUS_GENERR;

	if (argc == 4) {
		uint32_t id = atoi(argv[3]);
		conference_member_t *member;

		if ((member = conference_member_get(conference, id))) {
			uint32_t stopped = conference_member_stop_file(member, async ? FILE_STOP_ASYNC : current ? FILE_STOP_CURRENT : FILE_STOP_ALL);
			stream->write_function(stream, "Stopped %u files.\n", stopped);
			switch_thread_rwlock_unlock(member->rwlock);
		} else {
			stream->write_function(stream, "Member: %u not found.\n", id);
		}
	} else {
		uint32_t stopped = conference_stop_file(conference, async ? FILE_STOP_ASYNC : current ? FILE_STOP_CURRENT : FILE_STOP_ALL);
		stream->write_function(stream, "Stopped %u files.\n", stopped);
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t conf_api_sub_relate(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	uint8_t nospeak = 0, nohear = 0, sendvideo = 0, clear = 0;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	if (argc <= 3) {
		conference_member_t *member;

		switch_mutex_lock(conference->mutex);

		if (conference->relationship_total) {
			int member_id = 0;

			if (argc == 3) member_id = atoi(argv[2]);

			for (member = conference->members; member; member = member->next) {
				conference_relationship_t *rel;

				if (member_id > 0 && member->id != member_id) continue;

				for (rel = member->relationships; rel; rel = rel->next) {
					stream->write_function(stream, "%d -> %d %s%s%s\n", member->id, rel->id,
						(rel->flags & RFLAG_CAN_SPEAK) ? "SPEAK " : "NOSPEAK ",
						(rel->flags & RFLAG_CAN_HEAR) ? "HEAR " : "NOHEAR ",
						(rel->flags & RFLAG_CAN_SEND_VIDEO) ? "SENDVIDEO " : "NOSENDVIDEO ");
				}
			}
		} else {
			stream->write_function(stream, "No relationships\n");
		}
		switch_mutex_unlock(conference->mutex);
		return SWITCH_STATUS_SUCCESS;
	}

	if (argc <= 4)
		return SWITCH_STATUS_GENERR;

	nospeak = strstr(argv[4], "nospeak") ? 1 : 0;
	nohear = strstr(argv[4], "nohear") ? 1 : 0;
	sendvideo = strstr(argv[4], "sendvideo") ? 1 : 0;

	if (!strcasecmp(argv[4], "clear")) {
		clear = 1;
	}

	if (!(clear || nospeak || nohear || sendvideo)) {
		return SWITCH_STATUS_GENERR;
	}

	if (clear) {
		conference_member_t *member = NULL, *other_member = NULL;
		uint32_t id = atoi(argv[2]);
		uint32_t oid = atoi(argv[3]);

		if ((member = conference_member_get(conference, id))) {
			member_del_relationship(member, oid);
			other_member = conference_member_get(conference, oid);

			if (other_member) {
				if (member_test_flag(other_member, MFLAG_RECEIVING_VIDEO)) {
					member_clear_flag(other_member, MFLAG_RECEIVING_VIDEO);
					if (conference->floor_holder) {
						switch_core_session_request_video_refresh(conference->floor_holder->session);
					}
				}
				switch_thread_rwlock_unlock(other_member->rwlock);
			}

			stream->write_function(stream, "relationship %u->%u cleared.\n", id, oid);
			switch_thread_rwlock_unlock(member->rwlock);
		} else {
			stream->write_function(stream, "relationship %u->%u not found.\n", id, oid);
		}
		return SWITCH_STATUS_SUCCESS;
	}

	if (nospeak || nohear || sendvideo) {
		conference_member_t *member = NULL, *other_member = NULL;
		uint32_t id = atoi(argv[2]);
		uint32_t oid = atoi(argv[3]);

		if ((member = conference_member_get(conference, id))) {
			other_member = conference_member_get(conference, oid);
		}

		if (member && other_member) {
			conference_relationship_t *rel = NULL;

			if (sendvideo && member_test_flag(other_member, MFLAG_RECEIVING_VIDEO) && (! (nospeak || nohear))) {
				stream->write_function(stream, "member %d already receiving video", oid);
				goto skip;
			}

			if ((rel = member_get_relationship(member, other_member))) {
				rel->flags = 0;
			} else {
				rel = member_add_relationship(member, oid);
			}

			if (rel) {
				switch_set_flag(rel, RFLAG_CAN_SPEAK | RFLAG_CAN_HEAR);
				if (nospeak) {
					switch_clear_flag(rel, RFLAG_CAN_SPEAK);
					member_clear_flag_locked(member, MFLAG_TALKING);
				}
				if (nohear) {
					switch_clear_flag(rel, RFLAG_CAN_HEAR);
				}
				if (sendvideo) {
					switch_set_flag(rel, RFLAG_CAN_SEND_VIDEO);
					member_set_flag(other_member, MFLAG_RECEIVING_VIDEO);
					switch_core_session_request_video_refresh(member->session);
				}

				stream->write_function(stream, "ok %u->%u %s set\n", id, oid, argv[4]);
			} else {
				stream->write_function(stream, "error!\n");
			}
		} else {
			stream->write_function(stream, "relationship %u->%u not found.\n", id, oid);
		}

skip:
		if (member) {
			switch_thread_rwlock_unlock(member->rwlock);
		}

		if (other_member) {
			switch_thread_rwlock_unlock(other_member->rwlock);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t conf_api_sub_lock(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	switch_event_t *event;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	if (conference->is_locked_sound) {
		conference_play_file(conference, conference->is_locked_sound, CONF_DEFAULT_LEADIN, NULL, 0);
	}

	conference_set_flag_locked(conference, CFLAG_LOCKED);
	stream->write_function(stream, "OK %s locked\n", argv[0]);
	if (test_eflag(conference, EFLAG_LOCK) && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_data(conference, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "lock");
		switch_event_fire(&event);
	}

	return 0;
}

static switch_status_t conf_api_sub_unlock(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	switch_event_t *event;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	if (conference->is_unlocked_sound) {
		conference_play_file(conference, conference->is_unlocked_sound, CONF_DEFAULT_LEADIN, NULL, 0);
	}

	conference_clear_flag_locked(conference, CFLAG_LOCKED);
	stream->write_function(stream, "OK %s unlocked\n", argv[0]);
	if (test_eflag(conference, EFLAG_UNLOCK) && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
		conference_add_event_data(conference, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "unlock");
		switch_event_fire(&event);
	}

	return 0;
}

static switch_status_t conf_api_sub_exit_sound(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	switch_event_t *event;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);
	
	if (argc <= 2) {
		stream->write_function(stream, "Not enough args\n");
		return SWITCH_STATUS_GENERR;
	}
	
	if ( !strcasecmp(argv[2], "on") ) {
		conference_set_flag_locked(conference, CFLAG_EXIT_SOUND);
		stream->write_function(stream, "OK %s exit sounds on (%s)\n", argv[0], conference->exit_sound);
		if (test_eflag(conference, EFLAG_LOCK) && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_add_event_data(conference, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "exit-sounds-on");
			switch_event_fire(&event);
		}
	} else if ( !strcasecmp(argv[2], "off") || !strcasecmp(argv[2], "none") ) {
		conference_clear_flag_locked(conference, CFLAG_EXIT_SOUND);
		stream->write_function(stream, "OK %s exit sounds off (%s)\n", argv[0], conference->exit_sound);
		if (test_eflag(conference, EFLAG_LOCK) && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_add_event_data(conference, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "exit-sounds-off");
			switch_event_fire(&event);
		}		
	} else if ( !strcasecmp(argv[2], "file") ) {
		if (! argv[3]) {
			stream->write_function(stream, "No filename specified\n");
		} else {
			/* TODO: if possible, verify file exists before setting it */
			stream->write_function(stream,"Old exit sound: [%s]\n", conference->exit_sound);
			conference->exit_sound = switch_core_strdup(conference->pool, argv[3]);
			stream->write_function(stream, "OK %s exit sound file set to %s\n", argv[0], conference->exit_sound);
			if (test_eflag(conference, EFLAG_LOCK) && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
				conference_add_event_data(conference, event);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "exit-sound-file-changed");
				switch_event_fire(&event);
			}			
		}
	} else {
		stream->write_function(stream, "Bad args\n");
		return SWITCH_STATUS_GENERR;		
	}
	
	return 0;
}


static switch_status_t conf_api_sub_enter_sound(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	switch_event_t *event;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);
	
	if (argc <= 2) {
		stream->write_function(stream, "Not enough args\n");
		return SWITCH_STATUS_GENERR;
	}
	
	if ( !strcasecmp(argv[2], "on") ) {
		conference_set_flag_locked(conference, CFLAG_ENTER_SOUND);
		stream->write_function(stream, "OK %s enter sounds on (%s)\n", argv[0], conference->enter_sound);
		if (test_eflag(conference, EFLAG_LOCK) && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_add_event_data(conference, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "enter-sounds-on");
			switch_event_fire(&event);
		}
	} else if ( !strcasecmp(argv[2], "off") || !strcasecmp(argv[2], "none") ) {
		conference_clear_flag_locked(conference, CFLAG_ENTER_SOUND);
		stream->write_function(stream, "OK %s enter sounds off (%s)\n", argv[0], conference->enter_sound);
		if (test_eflag(conference, EFLAG_LOCK) && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_add_event_data(conference, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "enter-sounds-off");
			switch_event_fire(&event);
		}		
	} else if ( !strcasecmp(argv[2], "file") ) {
		if (! argv[3]) {
			stream->write_function(stream, "No filename specified\n");
		} else {
			/* TODO: verify file exists before setting it */
			conference->enter_sound = switch_core_strdup(conference->pool, argv[3]);
			stream->write_function(stream, "OK %s enter sound file set to %s\n", argv[0], conference->enter_sound);
			if (test_eflag(conference, EFLAG_LOCK) && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
				conference_add_event_data(conference, event);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "enter-sound-file-changed");
				switch_event_fire(&event);
			}			
		}
	} else {
		stream->write_function(stream, "Bad args\n");
		return SWITCH_STATUS_GENERR;		
	}
	
	return 0;
}


static switch_status_t conf_api_sub_dial(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	switch_call_cause_t cause;
	char *tmp;

	switch_assert(stream != NULL);

	if (argc <= 2) {
		stream->write_function(stream, "Bad Args\n");
		return SWITCH_STATUS_GENERR;
	}

	if (conference && argv[2] && strstr(argv[2], "vlc/")) {
		tmp = switch_core_sprintf(conference->pool, "{vlc_rate=%d,vlc_channels=%d,vlc_interval=%d}%s", 
								  conference->rate, conference->channels, conference->interval, argv[2]);
		argv[2] = tmp;
	}

	if (conference) {
		conference_outcall(conference, NULL, NULL, argv[2], 60, NULL, argv[4], argv[3], NULL, &cause, NULL, NULL);
	} else {
		conference_outcall(NULL, argv[0], NULL, argv[2], 60, NULL, argv[4], argv[3], NULL, &cause, NULL, NULL);
	}
	stream->write_function(stream, "Call Requested: result: [%s]\n", switch_channel_cause2str(cause));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t conf_api_sub_bgdial(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	switch_uuid_t uuid;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];

	switch_assert(stream != NULL);

	if (argc <= 2) {
		stream->write_function(stream, "Bad Args\n");
		return SWITCH_STATUS_GENERR;
	}

	switch_uuid_get(&uuid);
	switch_uuid_format(uuid_str, &uuid);

	if (conference) {
		conference_outcall_bg(conference, NULL, NULL, argv[2], 60, NULL, argv[4], argv[3], uuid_str, NULL, NULL, NULL);
	} else {
		conference_outcall_bg(NULL, argv[0], NULL, argv[2], 60, NULL, argv[4], argv[3], uuid_str, NULL, NULL, NULL);
	}

	stream->write_function(stream, "OK Job-UUID: %s\n", uuid_str);

	return SWITCH_STATUS_SUCCESS;
}



static switch_status_t conf_api_sub_transfer(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	switch_status_t ret_status = SWITCH_STATUS_SUCCESS;
	char *conf_name = NULL, *profile_name;
	switch_event_t *params = NULL;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);
	
	if (argc > 3 && !zstr(argv[2])) {
		int x;

		conf_name = strdup(argv[2]);

		if ((profile_name = strchr(conf_name, '@'))) {
			*profile_name++ = '\0';
		} else {
			profile_name = "default";
		}

		for (x = 3; x < argc; x++) {
			conference_member_t *member = NULL;
			uint32_t id = atoi(argv[x]);
			switch_channel_t *channel;
			switch_event_t *event;
			char *xdest = NULL;
			
			if (!id || !(member = conference_member_get(conference, id))) {
				stream->write_function(stream, "No Member %u in conference %s.\n", id, conference->name);
				continue;
			}

			channel = switch_core_session_get_channel(member->session);
			xdest = switch_core_session_sprintf(member->session, "conference:%s@%s", conf_name, profile_name);
			switch_ivr_session_transfer(member->session, xdest, "inline", NULL);
			
			switch_channel_set_variable(channel, "last_transfered_conference", conf_name);

			stream->write_function(stream, "OK Member '%d' sent to conference %s.\n", member->id, argv[2]);
			
			/* tell them what happened */
			if (test_eflag(conference, EFLAG_TRANSFER) &&
				switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
				conference_add_event_member_data(member, event);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Old-Conference-Name", conference->name);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "New-Conference-Name", argv[3]);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "transfer");
				switch_event_fire(&event);
			}

			switch_thread_rwlock_unlock(member->rwlock);
		}
	} else {
		ret_status = SWITCH_STATUS_GENERR;
	}

	if (params) {
		switch_event_destroy(&params);
	}

	switch_safe_free(conf_name);

	return ret_status;
}

static switch_status_t conf_api_sub_check_record(conference_obj_t *conference, switch_stream_handle_t *stream, int arc, char **argv)
{
	conference_record_t *rec;
	int x = 0;

	switch_mutex_lock(conference->flag_mutex);
	for (rec = conference->rec_node_head; rec; rec = rec->next) {
		stream->write_function(stream, "Record file %s%s%s\n", rec->path, rec->autorec ? " " : "", rec->autorec ? "(Auto)" : "");
		x++;
	}

	if (!x) {
		stream->write_function(stream, "Conference is not being recorded.\n");
	}
	switch_mutex_unlock(conference->flag_mutex);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t conf_api_sub_record(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	if (argc <= 2) {
		return SWITCH_STATUS_GENERR;
	}

	stream->write_function(stream, "Record file %s\n", argv[2]);
	conference->record_filename = switch_core_strdup(conference->pool, argv[2]);
	conference->record_count++;
	launch_conference_record_thread(conference, argv[2], SWITCH_FALSE);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t conf_api_sub_norecord(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	int all, before = conference->record_count, ttl = 0;
	switch_event_t *event;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	if (argc <= 2)
		return SWITCH_STATUS_GENERR;

	all = (strcasecmp(argv[2], "all") == 0);

	if (!conference_record_stop(conference, stream, all ? NULL : argv[2]) && !all) {
		stream->write_function(stream, "non-existant recording '%s'\n", argv[2]);
	} else {
		if (test_eflag(conference, EFLAG_RECORD) &&
				switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_add_event_data(conference, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "stop-recording");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Path", all ? "all" : argv[2]);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Other-Recordings", conference->record_count ? "true" : "false");
			switch_event_fire(&event);
		}
	}

	ttl = before - conference->record_count;
	stream->write_function(stream, "Stopped recording %d file%s\n", ttl, ttl == 1 ? "" : "s");

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t conf_api_sub_pauserec(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	switch_event_t *event;
	recording_action_type_t action;

	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	if (argc <= 2)
		return SWITCH_STATUS_GENERR;

	if (strcasecmp(argv[1], "pause") == 0) {
		action = REC_ACTION_PAUSE;
	} else if (strcasecmp(argv[1], "resume") == 0) {
		action = REC_ACTION_RESUME;
	} else {
		return SWITCH_STATUS_GENERR;
	}
	stream->write_function(stream, "%s recording file %s\n",
			action == REC_ACTION_PAUSE ? "Pause" : "Resume", argv[2]);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,	"%s recording file %s\n",
			action == REC_ACTION_PAUSE ? "Pause" : "Resume", argv[2]);

	if (!conference_record_action(conference, argv[2], action)) {
		stream->write_function(stream, "non-existant recording '%s'\n", argv[2]);
	} else {
		if (test_eflag(conference, EFLAG_RECORD) && switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS)
		{
			conference_add_event_data(conference, event);
			if (action == REC_ACTION_PAUSE) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "pause-recording");
			} else {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "resume-recording");
			}
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Path", argv[2]);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Other-Recordings", conference->record_count ? "true" : "false");
			switch_event_fire(&event);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t conf_api_sub_recording(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	if (argc > 2 && argc <= 3) {
		if (strcasecmp(argv[2], "stop") == 0 || strcasecmp(argv[2], "check") == 0) {
			argv[3] = "all";
			argc++;
		}
	}

	if (argc <= 3) {
		/* It means that old syntax is used */
		return conf_api_sub_record(conference,stream,argc,argv);
	} else {
		/* for new syntax call existing functions with fixed parameter list */
		if (strcasecmp(argv[2], "start") == 0) {
			argv[1] = argv[2];
			argv[2] = argv[3];
			return conf_api_sub_record(conference,stream,4,argv);
		} else if (strcasecmp(argv[2], "stop") == 0) {
			argv[1] = argv[2];
			argv[2] = argv[3];
			return conf_api_sub_norecord(conference,stream,4,argv);
		} else if (strcasecmp(argv[2], "check") == 0) {
			argv[1] = argv[2];
			argv[2] = argv[3];
			return conf_api_sub_check_record(conference,stream,4,argv);
		} else if (strcasecmp(argv[2], "pause") == 0) {
			argv[1] = argv[2];
			argv[2] = argv[3];
			return conf_api_sub_pauserec(conference,stream,4,argv);
		} else if (strcasecmp(argv[2], "resume") == 0) {
			argv[1] = argv[2];
			argv[2] = argv[3];
			return conf_api_sub_pauserec(conference,stream,4,argv);
		} else {
			return SWITCH_STATUS_GENERR;
		}
	}
}

static switch_status_t conf_api_sub_file_vol(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	if (argc >= 1) {
		conference_file_node_t *fnode;
		int vol = 0;
		int ok = 0;

		if (argc < 2) {
			stream->write_function(stream, "missing args\n");
			return SWITCH_STATUS_GENERR;
		}

		switch_mutex_lock(conference->mutex);

		fnode = conference->fnode;

		vol = atoi(argv[2]);

		if (argc > 3) {
			if (strcasecmp(argv[3], "async")) {
				fnode = conference->async_fnode;
			}
		}

		if (fnode && fnode->type == NODE_TYPE_FILE) {
			fnode->fh.vol = vol;
			ok = 1;
		}
		switch_mutex_unlock(conference->mutex);
		

		if (ok) {
			stream->write_function(stream, "volume changed\n");
			return SWITCH_STATUS_SUCCESS;
		} else {
			stream->write_function(stream, "File not playing\n");
			return SWITCH_STATUS_GENERR;
		}


	} else {
		stream->write_function(stream, "Invalid parameters:\n");
		return SWITCH_STATUS_GENERR;
	}
}

static switch_status_t conf_api_sub_pin(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv)
{
	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	if ((argc == 4) && (!strcmp(argv[2], "mod"))) {
		conference->mpin = switch_core_strdup(conference->pool, argv[3]);
		stream->write_function(stream, "Moderator Pin for conference %s set: %s\n", argv[0], conference->mpin);
		return SWITCH_STATUS_SUCCESS;
	} else if ((argc == 3) && (!strcmp(argv[1], "pin"))) {
		conference->pin = switch_core_strdup(conference->pool, argv[2]);
		stream->write_function(stream, "Pin for conference %s set: %s\n", argv[0], conference->pin);
		return SWITCH_STATUS_SUCCESS;
	} else if (argc == 2 && (!strcmp(argv[1], "nopin"))) {
		conference->pin = NULL;
		stream->write_function(stream, "Pin for conference %s deleted\n", argv[0]);
		return SWITCH_STATUS_SUCCESS;
	} else {
		stream->write_function(stream, "Invalid parameters:\n");
		return SWITCH_STATUS_GENERR;
	}
}

static switch_status_t conf_api_sub_get(conference_obj_t *conference,
		switch_stream_handle_t *stream, int argc, char **argv) {
	int ret_status = SWITCH_STATUS_GENERR;

	if (argc != 3) {
		ret_status = SWITCH_STATUS_FALSE;
	} else {
		ret_status = SWITCH_STATUS_SUCCESS;
		if (strcasecmp(argv[2], "run_time") == 0) {
			stream->write_function(stream, "%ld",
					switch_epoch_time_now(NULL) - conference->run_time);
		} else if (strcasecmp(argv[2], "count") == 0) {
			stream->write_function(stream, "%d",
					conference->count);
		} else if (strcasecmp(argv[2], "count_ghosts") == 0) {
			stream->write_function(stream, "%d",
					conference->count_ghosts);
		} else if (strcasecmp(argv[2], "max_members") == 0) {
			stream->write_function(stream, "%d",
					conference->max_members);
		} else if (strcasecmp(argv[2], "rate") == 0) {
			stream->write_function(stream, "%d",
					conference->rate);
		} else if (strcasecmp(argv[2], "profile_name") == 0) {
			stream->write_function(stream, "%s",
					conference->profile_name);
		} else if (strcasecmp(argv[2], "sound_prefix") == 0) {
			stream->write_function(stream, "%s",
					conference->sound_prefix);
		} else if (strcasecmp(argv[2], "caller_id_name") == 0) {
			stream->write_function(stream, "%s",
					conference->caller_id_name);
		} else if (strcasecmp(argv[2], "caller_id_number") == 0) {
			stream->write_function(stream, "%s",
					conference->caller_id_number);
		} else if (strcasecmp(argv[2], "is_locked") == 0) {
			stream->write_function(stream, "%s",
					conference_test_flag(conference, CFLAG_LOCKED) ? "locked" : "");
		} else if (strcasecmp(argv[2], "endconf_grace_time") == 0) {
			stream->write_function(stream, "%d",
					conference->endconf_grace_time);
		} else if (strcasecmp(argv[2], "uuid") == 0) {
			stream->write_function(stream, "%s",
					conference->uuid_str);
		} else if (strcasecmp(argv[2], "wait_mod") == 0) {
			stream->write_function(stream, "%s",
					conference_test_flag(conference, CFLAG_WAIT_MOD) ? "true" : "");
		} else {
			ret_status = SWITCH_STATUS_FALSE;
		}
	}

	return ret_status;
}

static switch_status_t conf_api_sub_set(conference_obj_t *conference,
		switch_stream_handle_t *stream, int argc, char **argv) {
	int ret_status = SWITCH_STATUS_GENERR;

	if (argc != 4 || zstr(argv[3])) {
		ret_status = SWITCH_STATUS_FALSE;
	} else {
		ret_status = SWITCH_STATUS_SUCCESS;
		if (strcasecmp(argv[2], "max_members") == 0) {
			int new_max = atoi(argv[3]);
			if (new_max >= 0) {
				stream->write_function(stream, "%d", conference->max_members);
				conference->max_members = new_max;
			} else {
				ret_status = SWITCH_STATUS_FALSE;
			}
		} else 	if (strcasecmp(argv[2], "sound_prefix") == 0) {
			stream->write_function(stream, "%s",conference->sound_prefix);
			conference->sound_prefix = switch_core_strdup(conference->pool, argv[3]);
		} else 	if (strcasecmp(argv[2], "caller_id_name") == 0) {
			stream->write_function(stream, "%s",conference->caller_id_name);
			conference->caller_id_name = switch_core_strdup(conference->pool, argv[3]);
		} else 	if (strcasecmp(argv[2], "caller_id_number") == 0) {
			stream->write_function(stream, "%s",conference->caller_id_number);
			conference->caller_id_number = switch_core_strdup(conference->pool, argv[3]);
		} else if (strcasecmp(argv[2], "endconf_grace_time") == 0) {
				int new_gt = atoi(argv[3]);
				if (new_gt >= 0) {
					stream->write_function(stream, "%d", conference->endconf_grace_time);
					conference->endconf_grace_time = new_gt;
				} else {
					ret_status = SWITCH_STATUS_FALSE;
				}
		} else {
			ret_status = SWITCH_STATUS_FALSE;
		}
	}

	return ret_status;
}

typedef enum {
	CONF_API_COMMAND_LIST = 0,
	CONF_API_COMMAND_ENERGY,
	CONF_API_COMMAND_VOLUME_IN,
	CONF_API_COMMAND_VOLUME_OUT,
	CONF_API_COMMAND_PLAY,
	CONF_API_COMMAND_SAY,
	CONF_API_COMMAND_SAYMEMBER,
	CONF_API_COMMAND_STOP,
	CONF_API_COMMAND_DTMF,
	CONF_API_COMMAND_KICK,
	CONF_API_COMMAND_MUTE,
	CONF_API_COMMAND_UNMUTE,
	CONF_API_COMMAND_DEAF,
	CONF_API_COMMAND_UNDEAF,
	CONF_API_COMMAND_RELATE,
	CONF_API_COMMAND_LOCK,
	CONF_API_COMMAND_UNLOCK,
	CONF_API_COMMAND_DIAL,
	CONF_API_COMMAND_BGDIAL,
	CONF_API_COMMAND_TRANSFER,
	CONF_API_COMMAND_RECORD,
	CONF_API_COMMAND_NORECORD,
	CONF_API_COMMAND_EXIT_SOUND,
	CONF_API_COMMAND_ENTER_SOUND,
	CONF_API_COMMAND_PIN,
	CONF_API_COMMAND_NOPIN,
	CONF_API_COMMAND_GET,
	CONF_API_COMMAND_SET,
} api_command_type_t;

/* API Interface Function sub-commands */
/* Entries in this list should be kept in sync with the enum above */
static api_command_t conf_api_sub_commands[] = {
	{"list", (void_fn_t) & conf_api_sub_list, CONF_API_SUB_ARGS_SPLIT, "list", "[delim <string>]|[count]"},
	{"xml_list", (void_fn_t) & conf_api_sub_xml_list, CONF_API_SUB_ARGS_SPLIT, "xml_list", ""},
	{"energy", (void_fn_t) & conf_api_sub_energy, CONF_API_SUB_MEMBER_TARGET, "energy", "<member_id|all|last|non_moderator> [<newval>]"},
	{"vid-canvas", (void_fn_t) & conf_api_sub_canvas, CONF_API_SUB_MEMBER_TARGET, "vid-canvas", "<member_id|all|last|non_moderator> [<newval>]"},
	{"vid-watching-canvas", (void_fn_t) & conf_api_sub_watching_canvas, CONF_API_SUB_MEMBER_TARGET, "vid-watching-canvas", "<member_id|all|last|non_moderator> [<newval>]"},
	{"vid-layer", (void_fn_t) & conf_api_sub_layer, CONF_API_SUB_MEMBER_TARGET, "vid-layer", "<member_id|all|last|non_moderator> [<newval>]"},
	{"volume_in", (void_fn_t) & conf_api_sub_volume_in, CONF_API_SUB_MEMBER_TARGET, "volume_in", "<member_id|all|last|non_moderator> [<newval>]"},
	{"volume_out", (void_fn_t) & conf_api_sub_volume_out, CONF_API_SUB_MEMBER_TARGET, "volume_out", "<member_id|all|last|non_moderator> [<newval>]"},
	{"position", (void_fn_t) & conf_api_sub_position, CONF_API_SUB_MEMBER_TARGET, "position", "<member_id> <x>:<y>:<z>"},
	{"auto-3d-position", (void_fn_t) & conf_api_sub_auto_position, CONF_API_SUB_ARGS_SPLIT, "auto-3d-position", "[on|off]"},
	{"play", (void_fn_t) & conf_api_sub_play, CONF_API_SUB_ARGS_SPLIT, "play", "<file_path> [async|<member_id> [nomux]]"},
	{"pause_play", (void_fn_t) & conf_api_sub_pause_play, CONF_API_SUB_ARGS_SPLIT, "pause", "[<member_id>]"},
	{"file_seek", (void_fn_t) & conf_api_sub_file_seek, CONF_API_SUB_ARGS_SPLIT, "file_seek", "[+-]<val> [<member_id>]"},
	{"say", (void_fn_t) & conf_api_sub_say, CONF_API_SUB_ARGS_AS_ONE, "say", "<text>"},
	{"saymember", (void_fn_t) & conf_api_sub_saymember, CONF_API_SUB_ARGS_AS_ONE, "saymember", "<member_id> <text>"},
	{"stop", (void_fn_t) & conf_api_sub_stop, CONF_API_SUB_ARGS_SPLIT, "stop", "<[current|all|async|last]> [<member_id>]"},
	{"dtmf", (void_fn_t) & conf_api_sub_dtmf, CONF_API_SUB_MEMBER_TARGET, "dtmf", "<[member_id|all|last|non_moderator]> <digits>"},
	{"kick", (void_fn_t) & conf_api_sub_kick, CONF_API_SUB_MEMBER_TARGET, "kick", "<[member_id|all|last|non_moderator]> [<optional sound file>]"},
	{"hup", (void_fn_t) & conf_api_sub_hup, CONF_API_SUB_MEMBER_TARGET, "hup", "<[member_id|all|last|non_moderator]>"},
	{"mute", (void_fn_t) & conf_api_sub_mute, CONF_API_SUB_MEMBER_TARGET, "mute", "<[member_id|all]|last|non_moderator> [<quiet>]"},
	{"tmute", (void_fn_t) & conf_api_sub_tmute, CONF_API_SUB_MEMBER_TARGET, "tmute", "<[member_id|all]|last|non_moderator> [<quiet>]"},
	{"unmute", (void_fn_t) & conf_api_sub_unmute, CONF_API_SUB_MEMBER_TARGET, "unmute", "<[member_id|all]|last|non_moderator> [<quiet>]"},
	{"vmute", (void_fn_t) & conf_api_sub_vmute, CONF_API_SUB_MEMBER_TARGET, "vmute", "<[member_id|all]|last|non_moderator> [<quiet>]"},
	{"tvmute", (void_fn_t) & conf_api_sub_tvmute, CONF_API_SUB_MEMBER_TARGET, "tvmute", "<[member_id|all]|last|non_moderator> [<quiet>]"},
	{"vmute-snap", (void_fn_t) & conf_api_sub_vmute_snap, CONF_API_SUB_MEMBER_TARGET, "vmute-snap", "<[member_id|all]|last|non_moderator>"},
	{"unvmute", (void_fn_t) & conf_api_sub_unvmute, CONF_API_SUB_MEMBER_TARGET, "unvmute", "<[member_id|all]|last|non_moderator> [<quiet>]"},
	{"deaf", (void_fn_t) & conf_api_sub_deaf, CONF_API_SUB_MEMBER_TARGET, "deaf", "<[member_id|all]|last|non_moderator>"},
	{"undeaf", (void_fn_t) & conf_api_sub_undeaf, CONF_API_SUB_MEMBER_TARGET, "undeaf", "<[member_id|all]|last|non_moderator>"},
	{"relate", (void_fn_t) & conf_api_sub_relate, CONF_API_SUB_ARGS_SPLIT, "relate", "<member_id> <other_member_id> [nospeak|nohear|clear]"},
	{"lock", (void_fn_t) & conf_api_sub_lock, CONF_API_SUB_ARGS_SPLIT, "lock", ""},
	{"unlock", (void_fn_t) & conf_api_sub_unlock, CONF_API_SUB_ARGS_SPLIT, "unlock", ""},
	{"agc", (void_fn_t) & conf_api_sub_agc, CONF_API_SUB_ARGS_SPLIT, "agc", ""},
	{"dial", (void_fn_t) & conf_api_sub_dial, CONF_API_SUB_ARGS_SPLIT, "dial", "<endpoint_module_name>/<destination> <callerid number> <callerid name>"},
	{"bgdial", (void_fn_t) & conf_api_sub_bgdial, CONF_API_SUB_ARGS_SPLIT, "bgdial", "<endpoint_module_name>/<destination> <callerid number> <callerid name>"},
	{"transfer", (void_fn_t) & conf_api_sub_transfer, CONF_API_SUB_ARGS_SPLIT, "transfer", "<conference_name> <member id> [...<member id>]"},
	{"record", (void_fn_t) & conf_api_sub_record, CONF_API_SUB_ARGS_SPLIT, "record", "<filename>"},
	{"chkrecord", (void_fn_t) & conf_api_sub_check_record, CONF_API_SUB_ARGS_SPLIT, "chkrecord", "<confname>"},
	{"norecord", (void_fn_t) & conf_api_sub_norecord, CONF_API_SUB_ARGS_SPLIT, "norecord", "<[filename|all]>"},
	{"pause", (void_fn_t) & conf_api_sub_pauserec, CONF_API_SUB_ARGS_SPLIT, "pause", "<filename>"},
	{"resume", (void_fn_t) & conf_api_sub_pauserec, CONF_API_SUB_ARGS_SPLIT, "resume", "<filename>"},
	{"recording", (void_fn_t) & conf_api_sub_recording, CONF_API_SUB_ARGS_SPLIT, "recording", "[start|stop|check|pause|resume] [<filename>|all]"},
	{"exit_sound", (void_fn_t) & conf_api_sub_exit_sound, CONF_API_SUB_ARGS_SPLIT, "exit_sound", "on|off|none|file <filename>"},
	{"enter_sound", (void_fn_t) & conf_api_sub_enter_sound, CONF_API_SUB_ARGS_SPLIT, "enter_sound", "on|off|none|file <filename>"},
	{"pin", (void_fn_t) & conf_api_sub_pin, CONF_API_SUB_ARGS_SPLIT, "pin", "<pin#>"},
	{"nopin", (void_fn_t) & conf_api_sub_pin, CONF_API_SUB_ARGS_SPLIT, "nopin", ""},
	{"get", (void_fn_t) & conf_api_sub_get, CONF_API_SUB_ARGS_SPLIT, "get", "<parameter-name>"},
	{"set", (void_fn_t) & conf_api_sub_set, CONF_API_SUB_ARGS_SPLIT, "set", "<max_members|sound_prefix|caller_id_name|caller_id_number|endconf_grace_time> <value>"},
	{"file-vol", (void_fn_t) & conf_api_sub_file_vol, CONF_API_SUB_ARGS_SPLIT, "file-vol", "<vol#>"},
	{"floor", (void_fn_t) & conf_api_sub_floor, CONF_API_SUB_MEMBER_TARGET, "floor", "<member_id|last>"},
	{"vid-floor", (void_fn_t) & conf_api_sub_vid_floor, CONF_API_SUB_MEMBER_TARGET, "vid-floor", "<member_id|last> [force]"},
	{"vid-banner", (void_fn_t) & conf_api_sub_vid_banner, CONF_API_SUB_MEMBER_TARGET, "vid-banner", "<member_id|last> <text>"},
	{"vid-mute-img", (void_fn_t) & conf_api_sub_vid_mute_img, CONF_API_SUB_MEMBER_TARGET, "vid-mute-img", "<member_id|last> [<path>|clear]"},
	{"vid-logo-img", (void_fn_t) & conf_api_sub_vid_logo_img, CONF_API_SUB_MEMBER_TARGET, "vid-logo-img", "<member_id|last> [<path>|clear]"},
	{"vid-res-id", (void_fn_t) & conf_api_sub_vid_res_id, CONF_API_SUB_MEMBER_TARGET, "vid-res-id", "<member_id|last> <val>|clear"},
	{"clear-vid-floor", (void_fn_t) & conf_api_sub_clear_vid_floor, CONF_API_SUB_ARGS_AS_ONE, "clear-vid-floor", ""},
	{"vid-layout", (void_fn_t) & conf_api_sub_vid_layout, CONF_API_SUB_ARGS_SPLIT, "vid-layout", "<layout name>|group <group name> [<canvas id>]"},
	{"vid-write-png", (void_fn_t) & conf_api_sub_write_png, CONF_API_SUB_ARGS_SPLIT, "vid-write-png", "<path>"},
	{"vid-fps", (void_fn_t) & conf_api_sub_vid_fps, CONF_API_SUB_ARGS_SPLIT, "vid-fps", "<fps>"},
	{"vid-bandwidth", (void_fn_t) & conf_api_sub_vid_bandwidth, CONF_API_SUB_ARGS_SPLIT, "vid-bandwidth", "<BW>"}
};

#define CONFFUNCAPISIZE (sizeof(conf_api_sub_commands)/sizeof(conf_api_sub_commands[0]))

switch_status_t conf_api_dispatch(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv, const char *cmdline, int argn)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	uint32_t i, found = 0;
	switch_assert(conference != NULL);
	switch_assert(stream != NULL);

	/* loop through the command table to find a match */
	for (i = 0; i < CONFFUNCAPISIZE && !found; i++) {
		if (strcasecmp(argv[argn], conf_api_sub_commands[i].pname) == 0) {
			found = 1;
			switch (conf_api_sub_commands[i].fntype) {

				/* commands that we've broken the command line into arguments for */
			case CONF_API_SUB_ARGS_SPLIT:
				{
					conf_api_args_cmd_t pfn = (conf_api_args_cmd_t) conf_api_sub_commands[i].pfnapicmd;

					if (pfn(conference, stream, argc, argv) != SWITCH_STATUS_SUCCESS) {
						/* command returned error, so show syntax usage */
						stream->write_function(stream, "%s %s", conf_api_sub_commands[i].pcommand, conf_api_sub_commands[i].psyntax);
					}
				}
				break;

				/* member specific command that can be iterated */
			case CONF_API_SUB_MEMBER_TARGET:
				{
					uint32_t id = 0;
					uint8_t all = 0;
					uint8_t last = 0;
					uint8_t non_mod = 0;

					if (argv[argn + 1]) {
						if (!(id = atoi(argv[argn + 1]))) {
							all = strcasecmp(argv[argn + 1], "all") ? 0 : 1;
							non_mod = strcasecmp(argv[argn + 1], "non_moderator") ? 0 : 1;
							last = strcasecmp(argv[argn + 1], "last") ? 0 : 1;
						}
					}

					if (all || non_mod) {
						conference_member_itterator(conference, stream, non_mod, (conf_api_member_cmd_t) conf_api_sub_commands[i].pfnapicmd, argv[argn + 2]);
					} else if (last) {
						conference_member_t *member = NULL;
						conference_member_t *last_member = NULL;

						switch_mutex_lock(conference->member_mutex);

						/* find last (oldest) member */
						member = conference->members;
						while (member != NULL) {
							if (last_member == NULL || member->id > last_member->id) {
								last_member = member;
							}
							member = member->next;
						}

						/* exec functio on last (oldest) member */
						if (last_member != NULL && last_member->session && !member_test_flag(last_member, MFLAG_NOCHANNEL)) {
							conf_api_member_cmd_t pfn = (conf_api_member_cmd_t) conf_api_sub_commands[i].pfnapicmd;
							pfn(last_member, stream, argv[argn + 2]);
						}

						switch_mutex_unlock(conference->member_mutex);
					} else if (id) {
						conf_api_member_cmd_t pfn = (conf_api_member_cmd_t) conf_api_sub_commands[i].pfnapicmd;
						conference_member_t *member = conference_member_get(conference, id);

						if (member != NULL) {
							pfn(member, stream, argv[argn + 2]);
							switch_thread_rwlock_unlock(member->rwlock);
						} else {
							stream->write_function(stream, "Non-Existant ID %u\n", id);
						}
					} else {
						stream->write_function(stream, "%s %s", conf_api_sub_commands[i].pcommand, conf_api_sub_commands[i].psyntax);
					}
				}
				break;

				/* commands that deals with all text after command */
			case CONF_API_SUB_ARGS_AS_ONE:
				{
					conf_api_text_cmd_t pfn = (conf_api_text_cmd_t) conf_api_sub_commands[i].pfnapicmd;
					char *start_text;
					const char *modified_cmdline = cmdline;
					const char *cmd = conf_api_sub_commands[i].pname;

					if (!zstr(modified_cmdline) && (start_text = strstr(modified_cmdline, cmd))) {
						modified_cmdline = start_text + strlen(cmd);
						while (modified_cmdline && (*modified_cmdline == ' ' || *modified_cmdline == '\t')) {
							modified_cmdline++;
						}
					}

					/* call the command handler */
					if (pfn(conference, stream, modified_cmdline) != SWITCH_STATUS_SUCCESS) {
						/* command returned error, so show syntax usage */
						stream->write_function(stream, "%s %s", conf_api_sub_commands[i].pcommand, conf_api_sub_commands[i].psyntax);
					}
				}
				break;
			}
		}
	}

	if (!found) {
		stream->write_function(stream, "Conference command '%s' not found.\n", argv[argn]);
	} else {
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
}

/* API Interface Function */
SWITCH_STANDARD_API(conf_api_main)
{
	char *lbuf = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *http = NULL, *type = NULL;
	int argc;
	char *argv[25] = { 0 };

	if (!cmd) {
		cmd = "help";
	}

	if (stream->param_event) {
		http = switch_event_get_header(stream->param_event, "http-host");
		type = switch_event_get_header(stream->param_event, "content-type");
	}

	if (http) {
		/* Output must be to a web browser */
		if (type && !strcasecmp(type, "text/html")) {
			stream->write_function(stream, "<pre>\n");
		}
	}

	if (!(lbuf = strdup(cmd))) {
		return status;
	}

	argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	/* try to find a command to execute */
	if (argc && argv[0]) {
		conference_obj_t *conference = NULL;

		if ((conference = conference_find(argv[0], NULL))) {
			if (argc >= 2) {
				conf_api_dispatch(conference, stream, argc, argv, cmd, 1);
			} else {
				stream->write_function(stream, "Conference command, not specified.\nTry 'help'\n");
			}
			switch_thread_rwlock_unlock(conference->rwlock);

		} else if (argv[0]) {
			/* special case the list command, because it doesn't require a conference argument */
			if (strcasecmp(argv[0], "list") == 0) {
				conf_api_sub_list(NULL, stream, argc, argv);
			} else if (strcasecmp(argv[0], "xml_list") == 0) {
				conf_api_sub_xml_list(NULL, stream, argc, argv);
			} else if (strcasecmp(argv[0], "help") == 0 || strcasecmp(argv[0], "commands") == 0) {
				stream->write_function(stream, "%s\n", api_syntax);
			} else if (argv[1] && strcasecmp(argv[1], "dial") == 0) {
				if (conf_api_sub_dial(NULL, stream, argc, argv) != SWITCH_STATUS_SUCCESS) {
					/* command returned error, so show syntax usage */
					stream->write_function(stream, "%s %s", conf_api_sub_commands[CONF_API_COMMAND_DIAL].pcommand, 
										   conf_api_sub_commands[CONF_API_COMMAND_DIAL].psyntax);
				}
			} else if (argv[1] && strcasecmp(argv[1], "bgdial") == 0) {
				if (conf_api_sub_bgdial(NULL, stream, argc, argv) != SWITCH_STATUS_SUCCESS) {
					/* command returned error, so show syntax usage */
					stream->write_function(stream, "%s %s", conf_api_sub_commands[CONF_API_COMMAND_BGDIAL].pcommand,
										   conf_api_sub_commands[CONF_API_COMMAND_BGDIAL].psyntax);
				}
			} else {
				stream->write_function(stream, "Conference %s not found\n", argv[0]);
			}
		}

	} else {
		int i;

		for (i = 0; i < CONFFUNCAPISIZE; i++) {
			stream->write_function(stream, "<conf name> %s %s\n", conf_api_sub_commands[i].pcommand, conf_api_sub_commands[i].psyntax);
		}
	}

	
	switch_safe_free(lbuf);

	return status;
}

/* generate an outbound call from the conference */
static switch_status_t conference_outcall(conference_obj_t *conference,
										  char *conference_name,
										  switch_core_session_t *session,
										  char *bridgeto, uint32_t timeout, 
										  char *flags, char *cid_name, 
										  char *cid_num,
										  char *profile,
										  switch_call_cause_t *cause,
										  switch_call_cause_t *cancel_cause, switch_event_t *var_event)
{
	switch_core_session_t *peer_session = NULL;
	switch_channel_t *peer_channel;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_channel_t *caller_channel = NULL;
	char appdata[512];
	int rdlock = 0;
	switch_bool_t have_flags = SWITCH_FALSE;
	const char *outcall_flags;
	int track = 0;
	const char *call_id = NULL;

	if (var_event && switch_true(switch_event_get_header(var_event, "conference_track_status"))) {
		track++;
		call_id = switch_event_get_header(var_event, "conference_track_call_id");
	}

	*cause = SWITCH_CAUSE_NORMAL_CLEARING;

	if (conference == NULL) {
		char *dialstr = switch_mprintf("{ignore_early_media=true}%s", bridgeto);
		status = switch_ivr_originate(NULL, &peer_session, cause, dialstr, 60, NULL, cid_name, cid_num, NULL, var_event, SOF_NO_LIMITS, NULL);
		switch_safe_free(dialstr);

		if (status != SWITCH_STATUS_SUCCESS) {
			return status;
		}

		peer_channel = switch_core_session_get_channel(peer_session);
		rdlock = 1;
		goto callup;
	}

	conference_name = conference->name;

	if (switch_thread_rwlock_tryrdlock(conference->rwlock) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Read Lock Fail\n");
		return SWITCH_STATUS_FALSE;
	}

	if (session != NULL) {
		caller_channel = switch_core_session_get_channel(session);
	}

	if (zstr(cid_name)) {
		cid_name = conference->caller_id_name;
	}

	if (zstr(cid_num)) {
		cid_num = conference->caller_id_number;
	}

	/* establish an outbound call leg */

	switch_mutex_lock(conference->mutex);
	conference->originating++;
	switch_mutex_unlock(conference->mutex);

	if (track) {
		send_conference_notify(conference, "SIP/2.0 100 Trying\r\n", call_id, SWITCH_FALSE);
	}


	status = switch_ivr_originate(session, &peer_session, cause, bridgeto, timeout, NULL, cid_name, cid_num, NULL, var_event, SOF_NO_LIMITS, cancel_cause);
	switch_mutex_lock(conference->mutex);
	conference->originating--;
	switch_mutex_unlock(conference->mutex);

	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot create outgoing channel, cause: %s\n",
						  switch_channel_cause2str(*cause));
		if (caller_channel) {
			switch_channel_hangup(caller_channel, *cause);
		}

		if (track) {
			send_conference_notify(conference, "SIP/2.0 481 Failure\r\n", call_id, SWITCH_TRUE);
		}

		goto done;
	}

	if (track) {
		send_conference_notify(conference, "SIP/2.0 200 OK\r\n", call_id, SWITCH_TRUE);
	}

	rdlock = 1;
	peer_channel = switch_core_session_get_channel(peer_session);

	/* make sure the conference still exists */
	if (!conference_test_flag(conference, CFLAG_RUNNING)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Conference is gone now, nevermind..\n");
		if (caller_channel) {
			switch_channel_hangup(caller_channel, SWITCH_CAUSE_NO_ROUTE_DESTINATION);
		}
		switch_channel_hangup(peer_channel, SWITCH_CAUSE_NO_ROUTE_DESTINATION);
		goto done;
	}

	if (caller_channel && switch_channel_test_flag(peer_channel, CF_ANSWERED)) {
		switch_channel_answer(caller_channel);
	}

  callup:

	/* if the outbound call leg is ready */
	if (switch_channel_test_flag(peer_channel, CF_ANSWERED) || switch_channel_test_flag(peer_channel, CF_EARLY_MEDIA)) {
		switch_caller_extension_t *extension = NULL;

		/* build an extension name object */
		if ((extension = switch_caller_extension_new(peer_session, conference_name, conference_name)) == 0) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Memory Error!\n");
			status = SWITCH_STATUS_MEMERR;
			goto done;
		}

		if ((outcall_flags = switch_channel_get_variable(peer_channel, "outcall_flags"))) {
			if (!zstr(outcall_flags)) {
				flags = (char *)outcall_flags;
			}
		}

		if (flags && strcasecmp(flags, "none")) {
			have_flags = SWITCH_TRUE;
		}
		/* add them to the conference */

		switch_snprintf(appdata, sizeof(appdata), "%s%s%s%s%s%s", conference_name,
				profile?"@":"", profile?profile:"",
				have_flags?"+flags{":"", have_flags?flags:"", have_flags?"}":"");
		switch_caller_extension_add_application(peer_session, extension, (char *) global_app_name, appdata);

		switch_channel_set_caller_extension(peer_channel, extension);
		switch_channel_set_state(peer_channel, CS_EXECUTE);

	} else {
		switch_channel_hangup(peer_channel, SWITCH_CAUSE_NO_ANSWER);
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

  done:
	if (conference) {
		switch_thread_rwlock_unlock(conference->rwlock);
	}
	if (rdlock && peer_session) {
		switch_core_session_rwunlock(peer_session);
	}

	return status;
}

struct bg_call {
	conference_obj_t *conference;
	switch_core_session_t *session;
	char *bridgeto;
	uint32_t timeout;
	char *flags;
	char *cid_name;
	char *cid_num;
	char *conference_name;
	char *uuid;
	char *profile;
	switch_call_cause_t *cancel_cause;
	switch_event_t *var_event;
	switch_memory_pool_t *pool;
};

static void *SWITCH_THREAD_FUNC conference_outcall_run(switch_thread_t *thread, void *obj)
{
	struct bg_call *call = (struct bg_call *) obj;

	if (call) {
		switch_call_cause_t cause;
		switch_event_t *event;


		conference_outcall(call->conference, call->conference_name,
						   call->session, call->bridgeto, call->timeout, 
						   call->flags, call->cid_name, call->cid_num, call->profile, &cause, call->cancel_cause, call->var_event);

		if (call->conference && test_eflag(call->conference, EFLAG_BGDIAL_RESULT) &&
			switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			conference_add_event_data(call->conference, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "bgdial-result");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Result", switch_channel_cause2str(cause));
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Job-UUID", call->uuid);
			switch_event_fire(&event);
		}

		if (call->var_event) {
			switch_event_destroy(&call->var_event);
		}

		switch_safe_free(call->bridgeto);
		switch_safe_free(call->flags);
		switch_safe_free(call->cid_name);
		switch_safe_free(call->cid_num);
		switch_safe_free(call->conference_name);
		switch_safe_free(call->uuid);
		switch_safe_free(call->profile);
		if (call->pool) {
			switch_core_destroy_memory_pool(&call->pool);
		}
		switch_safe_free(call);
	}

	return NULL;
}

static switch_status_t conference_outcall_bg(conference_obj_t *conference,
											 char *conference_name,
											 switch_core_session_t *session, char *bridgeto, uint32_t timeout, const char *flags, const char *cid_name,
											 const char *cid_num, const char *call_uuid, const char *profile, switch_call_cause_t *cancel_cause, switch_event_t **var_event)
{
	struct bg_call *call = NULL;
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_memory_pool_t *pool = NULL;

	if (!(call = malloc(sizeof(*call))))
		return SWITCH_STATUS_MEMERR;

	memset(call, 0, sizeof(*call));
	call->conference = conference;
	call->session = session;
	call->timeout = timeout;
	call->cancel_cause = cancel_cause;

	if (var_event) {
		call->var_event = *var_event;
		var_event = NULL;
	}

	if (conference) {
		pool = conference->pool;
	} else {
		switch_core_new_memory_pool(&pool);
		call->pool = pool;
	}

	if (bridgeto) {
		call->bridgeto = strdup(bridgeto);
	}
	if (flags) {
		call->flags = strdup(flags);
	}
	if (cid_name) {
		call->cid_name = strdup(cid_name);
	}
	if (cid_num) {
		call->cid_num = strdup(cid_num);
	}

	if (conference_name) {
		call->conference_name = strdup(conference_name);
	}

	if (call_uuid) {
		call->uuid = strdup(call_uuid);
	}

        if (profile) {
                call->profile = strdup(profile);
        }

	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, conference_outcall_run, call, pool);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Launching BG Thread for outcall\n");

	return SWITCH_STATUS_SUCCESS;
}

/* Play a file */
static switch_status_t conference_local_play_file(conference_obj_t *conference, switch_core_session_t *session, char *path, uint32_t leadin, void *buf,
												  uint32_t buflen)
{
	uint32_t x = 0;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_channel_t *channel;
	char *expanded = NULL;
	switch_input_args_t args = { 0 }, *ap = NULL;

	if (buf) {
		args.buf = buf;
		args.buflen = buflen;
		ap = &args;
	}

	/* generate some space infront of the file to be played */
	for (x = 0; x < leadin; x++) {
		switch_frame_t *read_frame;
		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}
	}

	/* if all is well, really play the file */
	if (status == SWITCH_STATUS_SUCCESS) {
		char *dpath = NULL;

		channel = switch_core_session_get_channel(session);
		if ((expanded = switch_channel_expand_variables(channel, path)) != path) {
			path = expanded;
		} else {
			expanded = NULL;
		}

		if (!strncasecmp(path, "say:", 4)) {
			if (!(conference->tts_engine && conference->tts_voice)) {
				status = SWITCH_STATUS_FALSE;
			} else {
				status = switch_ivr_speak_text(session, conference->tts_engine, conference->tts_voice, path + 4, ap);
			}
			goto done;
		}

		if (!switch_is_file_path(path) && conference->sound_prefix) {
			if (!(dpath = switch_mprintf("%s%s%s", conference->sound_prefix, SWITCH_PATH_SEPARATOR, path))) {
				status = SWITCH_STATUS_MEMERR;
				goto done;
			}
			path = dpath;
		}

		status = switch_ivr_play_file(session, NULL, path, ap);
		switch_safe_free(dpath);
	}

  done:
	switch_safe_free(expanded);

	return status;
}

static void set_mflags(const char *flags, member_flag_t *f)
{
	if (flags) {
		char *dup = strdup(flags);
		char *p;
		char *argv[10] = { 0 };
		int i, argc = 0;

		f[MFLAG_CAN_SPEAK] = f[MFLAG_CAN_HEAR] = f[MFLAG_CAN_BE_SEEN] = 1;

		for (p = dup; p && *p; p++) {
			if (*p == ',') {
				*p = '|';
			}
		}

		argc = switch_separate_string(dup, '|', argv, (sizeof(argv) / sizeof(argv[0])));

		for (i = 0; i < argc && argv[i]; i++) {
			if (!strcasecmp(argv[i], "mute")) {
				f[MFLAG_CAN_SPEAK] = 0;
				f[MFLAG_TALKING] = 0;
			} else if (!strcasecmp(argv[i], "deaf")) {
				f[MFLAG_CAN_HEAR] = 0;
			} else if (!strcasecmp(argv[i], "mute-detect")) {
				f[MFLAG_MUTE_DETECT] = 1;
			} else if (!strcasecmp(argv[i], "dist-dtmf")) {
				f[MFLAG_DIST_DTMF] = 1;
			} else if (!strcasecmp(argv[i], "moderator")) {
				f[MFLAG_MOD] = 1;
			} else if (!strcasecmp(argv[i], "nomoh")) {
				f[MFLAG_NOMOH] = 1;
			} else if (!strcasecmp(argv[i], "endconf")) {
				f[MFLAG_ENDCONF] = 1;
			} else if (!strcasecmp(argv[i], "mintwo")) {
				f[MFLAG_MINTWO] = 1;
			} else if (!strcasecmp(argv[i], "video-bridge")) {
				f[MFLAG_VIDEO_BRIDGE] = 1;
			} else if (!strcasecmp(argv[i], "ghost")) {
				f[MFLAG_GHOST] = 1;
			} else if (!strcasecmp(argv[i], "join-only")) {
				f[MFLAG_JOIN_ONLY] = 1;
			} else if (!strcasecmp(argv[i], "positional")) {
				f[MFLAG_POSITIONAL] = 1;
			} else if (!strcasecmp(argv[i], "no-positional")) {
				f[MFLAG_NO_POSITIONAL] = 1;
			} else if (!strcasecmp(argv[i], "join-vid-floor")) {
				f[MFLAG_JOIN_VID_FLOOR] = 1;
			} else if (!strcasecmp(argv[i], "no-minimize-encoding")) {
				f[MFLAG_NO_MINIMIZE_ENCODING] = 1;
			} else if (!strcasecmp(argv[i], "second-screen")) {
				f[MFLAG_SECOND_SCREEN] = 1;
				f[MFLAG_CAN_SPEAK] = 0;
				f[MFLAG_TALKING] = 0;
				f[MFLAG_CAN_HEAR] = 0;
				f[MFLAG_SILENT] = 1;
			}
		}

		free(dup);
	}
}



static void set_cflags(const char *flags, conference_flag_t *f)
{
	if (flags) {
		char *dup = strdup(flags);
		char *p;
		char *argv[10] = { 0 };
		int i, argc = 0;

		for (p = dup; p && *p; p++) {
			if (*p == ',') {
				*p = '|';
			}
		}

		argc = switch_separate_string(dup, '|', argv, (sizeof(argv) / sizeof(argv[0])));

		for (i = 0; i < argc && argv[i]; i++) {
			if (!strcasecmp(argv[i], "wait-mod")) {
				f[CFLAG_WAIT_MOD] = 1;
			} else if (!strcasecmp(argv[i], "video-floor-only")) {
				f[CFLAG_VID_FLOOR] = 1;
			} else if (!strcasecmp(argv[i], "audio-always")) {
				f[CFLAG_AUDIO_ALWAYS] = 1;
			} else if (!strcasecmp(argv[i], "restart-auto-record")) {
				f[CFLAG_CONF_RESTART_AUTO_RECORD] = 1;
			} else if (!strcasecmp(argv[i], "json-events")) {
				f[CFLAG_JSON_EVENTS] = 1;
			} else if (!strcasecmp(argv[i], "livearray-sync")) {
				f[CFLAG_LIVEARRAY_SYNC] = 1;
			} else if (!strcasecmp(argv[i], "livearray-json-status")) {
				f[CFLAG_JSON_STATUS] = 1;
			} else if (!strcasecmp(argv[i], "rfc-4579")) {
				f[CFLAG_RFC4579] = 1;
			} else if (!strcasecmp(argv[i], "auto-3d-position")) {
				f[CFLAG_POSITIONAL] = 1;
			} else if (!strcasecmp(argv[i], "minimize-video-encoding")) {
				f[CFLAG_MINIMIZE_VIDEO_ENCODING] = 1;
			} else if (!strcasecmp(argv[i], "video-bridge-first-two")) {
				f[CFLAG_VIDEO_BRIDGE_FIRST_TWO] = 1;
			} else if (!strcasecmp(argv[i], "video-required-for-canvas")) {
				f[CFLAG_VIDEO_REQUIRED_FOR_CANVAS] = 1;
			} else if (!strcasecmp(argv[i], "manage-inbound-video-bitrate")) {
				f[CFLAG_MANAGE_INBOUND_VIDEO_BITRATE] = 1;
			} else if (!strcasecmp(argv[i], "video-muxing-personal-canvas")) {
				f[CFLAG_PERSONAL_CANVAS] = 1;
			}
		}		

		free(dup);
	}
}


static void clear_eflags(char *events, uint32_t *f)
{
	char buf[512] = "";
	char *next = NULL;
	char *event = buf;

	if (events) {
		switch_copy_string(buf, events, sizeof(buf));

		while (event) {
			next = strchr(event, ',');
			if (next) {
				*next++ = '\0';
			}

			if (!strcmp(event, "add-member")) {
				*f &= ~EFLAG_ADD_MEMBER;
			} else if (!strcmp(event, "del-member")) {
				*f &= ~EFLAG_DEL_MEMBER;
			} else if (!strcmp(event, "energy-level")) {
				*f &= ~EFLAG_ENERGY_LEVEL;
			} else if (!strcmp(event, "volume-level")) {
				*f &= ~EFLAG_VOLUME_LEVEL;
			} else if (!strcmp(event, "gain-level")) {
				*f &= ~EFLAG_GAIN_LEVEL;
			} else if (!strcmp(event, "dtmf")) {
				*f &= ~EFLAG_DTMF;
			} else if (!strcmp(event, "stop-talking")) {
				*f &= ~EFLAG_STOP_TALKING;
			} else if (!strcmp(event, "start-talking")) {
				*f &= ~EFLAG_START_TALKING;
			} else if (!strcmp(event, "mute-detect")) {
				*f &= ~EFLAG_MUTE_DETECT;
			} else if (!strcmp(event, "mute-member")) {
				*f &= ~EFLAG_MUTE_MEMBER;
			} else if (!strcmp(event, "unmute-member")) {
				*f &= ~EFLAG_UNMUTE_MEMBER;
			} else if (!strcmp(event, "kick-member")) {
				*f &= ~EFLAG_KICK_MEMBER;
			} else if (!strcmp(event, "dtmf-member")) {
				*f &= ~EFLAG_DTMF_MEMBER;
			} else if (!strcmp(event, "energy-level-member")) {
				*f &= ~EFLAG_ENERGY_LEVEL_MEMBER;
			} else if (!strcmp(event, "volume-in-member")) {
				*f &= ~EFLAG_VOLUME_IN_MEMBER;
			} else if (!strcmp(event, "volume-out-member")) {
				*f &= ~EFLAG_VOLUME_OUT_MEMBER;
			} else if (!strcmp(event, "play-file")) {
				*f &= ~EFLAG_PLAY_FILE;
			} else if (!strcmp(event, "play-file-done")) {
				*f &= ~EFLAG_PLAY_FILE_DONE;
			} else if (!strcmp(event, "play-file-member")) {
				*f &= ~EFLAG_PLAY_FILE_MEMBER;
			} else if (!strcmp(event, "speak-text")) {
				*f &= ~EFLAG_SPEAK_TEXT;
			} else if (!strcmp(event, "speak-text-member")) {
				*f &= ~EFLAG_SPEAK_TEXT_MEMBER;
			} else if (!strcmp(event, "lock")) {
				*f &= ~EFLAG_LOCK;
			} else if (!strcmp(event, "unlock")) {
				*f &= ~EFLAG_UNLOCK;
			} else if (!strcmp(event, "transfer")) {
				*f &= ~EFLAG_TRANSFER;
			} else if (!strcmp(event, "bgdial-result")) {
				*f &= ~EFLAG_BGDIAL_RESULT;
			} else if (!strcmp(event, "floor-change")) {
				*f &= ~EFLAG_FLOOR_CHANGE;
			} else if (!strcmp(event, "record")) {
				*f &= ~EFLAG_RECORD;
			}

			event = next;
		}
	}
}

SWITCH_STANDARD_APP(conference_auto_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	call_list_t *call_list, *np;

	call_list = switch_channel_get_private(channel, "_conference_autocall_list_");

	if (zstr(data)) {
		call_list = NULL;
	} else {
		np = switch_core_session_alloc(session, sizeof(*np));
		switch_assert(np != NULL);

		np->string = switch_core_session_strdup(session, data);
		if (call_list) {
			np->next = call_list;
			np->iteration = call_list->iteration + 1;
		} else {
			np->iteration = 1;
		}
		call_list = np;
	}
	switch_channel_set_private(channel, "_conference_autocall_list_", call_list);
}


static int setup_media(conference_member_t *member, conference_obj_t *conference)
{
	switch_codec_implementation_t read_impl = { 0 };

	switch_mutex_lock(member->audio_out_mutex);

	switch_core_session_get_read_impl(member->session, &read_impl);

	if (switch_core_codec_ready(&member->read_codec)) {
		switch_core_codec_destroy(&member->read_codec);
		memset(&member->read_codec, 0, sizeof(member->read_codec));
	}

	if (switch_core_codec_ready(&member->write_codec)) {
		switch_core_codec_destroy(&member->write_codec);
		memset(&member->write_codec, 0, sizeof(member->write_codec));
	}

	if (member->read_resampler) {
		switch_resample_destroy(&member->read_resampler);
	}

	switch_core_session_get_read_impl(member->session, &member->orig_read_impl);
	member->native_rate = read_impl.samples_per_second;

	/* Setup a Signed Linear codec for reading audio. */
	if (switch_core_codec_init(&member->read_codec,
							   "L16",
							   NULL, NULL, read_impl.actual_samples_per_second, read_impl.microseconds_per_packet / 1000,
							   read_impl.number_of_channels, 
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, member->pool) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_DEBUG,
						  "Raw Codec Activation Success L16@%uhz %d channel %dms\n",
						  read_impl.actual_samples_per_second, read_impl.number_of_channels, read_impl.microseconds_per_packet / 1000);

	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_DEBUG, "Raw Codec Activation Failed L16@%uhz %d channel %dms\n",
						  read_impl.actual_samples_per_second, read_impl.number_of_channels, read_impl.microseconds_per_packet / 1000);

		goto done;
	}

	if (!member->frame_size) {
		member->frame_size = SWITCH_RECOMMENDED_BUFFER_SIZE;
		member->frame = switch_core_alloc(member->pool, member->frame_size);
		member->mux_frame = switch_core_alloc(member->pool, member->frame_size);
	}

	if (read_impl.actual_samples_per_second != conference->rate) {
		if (switch_resample_create(&member->read_resampler,
								   read_impl.actual_samples_per_second,
								   conference->rate, member->frame_size, SWITCH_RESAMPLE_QUALITY, read_impl.number_of_channels) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_CRIT, "Unable to create resampler!\n");
			goto done;
		}


		member->resample_out = switch_core_alloc(member->pool, member->frame_size);
		member->resample_out_len = member->frame_size;

		/* Setup an audio buffer for the resampled audio */
		if (!member->resample_buffer && switch_buffer_create_dynamic(&member->resample_buffer, CONF_DBLOCK_SIZE, CONF_DBUFFER_SIZE, CONF_DBUFFER_MAX)
			!= SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_CRIT, "Memory Error Creating Audio Buffer!\n");
			goto done;
		}
	}


	/* Setup a Signed Linear codec for writing audio. */
	if (switch_core_codec_init(&member->write_codec,
							   "L16",
							   NULL,
							   NULL,
							   conference->rate,
							   read_impl.microseconds_per_packet / 1000,
							   read_impl.number_of_channels, 
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, member->pool) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_DEBUG,
						  "Raw Codec Activation Success L16@%uhz %d channel %dms\n", 
						  conference->rate, conference->channels, read_impl.microseconds_per_packet / 1000);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_DEBUG, "Raw Codec Activation Failed L16@%uhz %d channel %dms\n",
						  conference->rate, conference->channels, read_impl.microseconds_per_packet / 1000);
		goto codec_done2;
	}

	/* Setup an audio buffer for the incoming audio */
	if (!member->audio_buffer && switch_buffer_create_dynamic(&member->audio_buffer, CONF_DBLOCK_SIZE, CONF_DBUFFER_SIZE, CONF_DBUFFER_MAX) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_CRIT, "Memory Error Creating Audio Buffer!\n");
		goto codec_done1;
	}

	/* Setup an audio buffer for the outgoing audio */
	if (!member->mux_buffer && switch_buffer_create_dynamic(&member->mux_buffer, CONF_DBLOCK_SIZE, CONF_DBUFFER_SIZE, CONF_DBUFFER_MAX) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member->session), SWITCH_LOG_CRIT, "Memory Error Creating Audio Buffer!\n");
		goto codec_done1;
	}

	switch_mutex_unlock(member->audio_out_mutex);

	return 0;

  codec_done1:
	switch_core_codec_destroy(&member->read_codec);
  codec_done2:
	switch_core_codec_destroy(&member->write_codec);
  done:

	switch_mutex_unlock(member->audio_out_mutex);

	return -1;


}

static void merge_mflags(member_flag_t *a, member_flag_t *b)
{
	int x;

	for (x = 0; x < MFLAG_MAX; x++) {
		if (b[x]) a[x] = 1;
	}
}


static const char *combine_flag_var(switch_core_session_t *session, const char *var_name) 
{
	switch_event_header_t *hp;
	switch_event_t *event, *cevent;
	char *ret = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	switch_core_get_variables(&event);
	switch_channel_get_variables(channel, &cevent);
	switch_event_merge(event, cevent);

	
	for (hp = event->headers; hp; hp = hp->next) {
		char *var = hp->name;
		char *val = hp->value;

		if (!strcasecmp(var, var_name)) {
			if (hp->idx) {
				int i;
				for (i = 0; i < hp->idx; i++) {
					if (zstr(ret)) {
						ret = switch_core_session_sprintf(session, "%s", hp->array[i]);
					} else {
						ret = switch_core_session_sprintf(session, "%s|%s", ret, hp->array[i]);
					}
				}
			} else {
				if (zstr(ret)) {
					ret = switch_core_session_sprintf(session, "%s", val);
				} else {
					ret = switch_core_session_sprintf(session, "%s|%s", ret, val);
				}
			}
		}
	}
	

	switch_event_destroy(&event);
	switch_event_destroy(&cevent);

	return ret;

}

#define validate_pin(buf, pin, mpin) \
	pin_valid = (!zstr(pin) && strcmp(buf, pin) == 0);	\
	if (!pin_valid && !zstr(mpin) && strcmp(buf, mpin) == 0) {			\
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Moderator PIN found!\n"); \
		pin_valid = 1; \
		mpin_matched = 1; \
	} 
/* Application interface function that is called from the dialplan to join the channel to a conference */
SWITCH_STANDARD_APP(conference_function)
{
	switch_codec_t *read_codec = NULL;
	//uint32_t flags = 0;
	conference_member_t member = { 0 };
	conference_obj_t *conference = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *mydata = NULL;
	char *conf_name = NULL;
	char *bridge_prefix = "bridge:";
	char *flags_prefix = "+flags{";
	char *bridgeto = NULL;
	char *profile_name = NULL;
	switch_xml_t cxml = NULL, cfg = NULL, profiles = NULL;
	const char *flags_str, *v_flags_str;
	const char *cflags_str, *v_cflags_str;
	member_flag_t mflags[MFLAG_MAX] = { 0 };
	switch_core_session_message_t msg = { 0 };
	uint8_t rl = 0, isbr = 0;
	char *dpin = "";
	const char *mdpin = "";
	conf_xml_cfg_t xml_cfg = { 0 };
	switch_event_t *params = NULL;
	int locked = 0;
	int mpin_matched = 0;
	uint32_t *mid;

	if (!switch_channel_test_app_flag_key("conf_silent", channel, CONF_SILENT_DONE) &&
		(switch_channel_test_flag(channel, CF_RECOVERED) || switch_true(switch_channel_get_variable(channel, "conference_silent_entry")))) {
		switch_channel_set_app_flag_key("conf_silent", channel, CONF_SILENT_REQ);
	}

	switch_core_session_video_reset(session);

	switch_channel_set_flag(channel, CF_CONFERENCE);

	if (switch_channel_answer(channel) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Channel answer failed.\n");
        goto end;
	}

	/* Save the original read codec. */
	if (!(read_codec = switch_core_session_get_read_codec(session))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Channel has no media!\n");
		goto end;
	}


	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Invalid arguments\n");
		goto end;
	}

	mydata = switch_core_session_strdup(session, data);

	if (!mydata) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Pool Failure\n");
		goto end;
	}

	if ((flags_str = strstr(mydata, flags_prefix))) {
		char *p;
		*((char *) flags_str) = '\0';
		flags_str += strlen(flags_prefix);
		if ((p = strchr(flags_str, '}'))) {
			*p = '\0';
		}
	}

	//if ((v_flags_str = switch_channel_get_variable(channel, "conference_member_flags"))) {
	if ((v_flags_str = combine_flag_var(session, "conference_member_flags"))) {
		if (zstr(flags_str)) {
			flags_str = v_flags_str;
		} else {
			flags_str = switch_core_session_sprintf(session, "%s|%s", flags_str, v_flags_str);
		}
	}

    cflags_str = flags_str;

	//if ((v_cflags_str = switch_channel_get_variable(channel, "conference_flags"))) {
	if ((v_cflags_str = combine_flag_var(session, "conference_flags"))) {
		if (zstr(cflags_str)) {
			cflags_str = v_cflags_str;
		} else {
			cflags_str = switch_core_session_sprintf(session, "%s|%s", cflags_str, v_cflags_str);
		}
	}

	/* is this a bridging conference ? */
	if (!strncasecmp(mydata, bridge_prefix, strlen(bridge_prefix))) {
		isbr = 1;
		mydata += strlen(bridge_prefix);
		if ((bridgeto = strchr(mydata, ':'))) {
			*bridgeto++ = '\0';
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Config Error!\n");
			goto done;
		}
	}

	conf_name = mydata;

	/* eat all leading spaces on conference name, which can cause problems */
	while (*conf_name == ' ') {
		conf_name++;
	}

	/* is there a conference pin ? */
	if ((dpin = strchr(conf_name, '+'))) {
		*dpin++ = '\0';
	} else dpin = "";

	/* is there profile specification ? */
	if ((profile_name = strrchr(conf_name, '@'))) {
		*profile_name++ = '\0';
	} else {
		profile_name = "default";
	}

#if 0
	if (0) {
		member.dtmf_parser = conference->dtmf_parser;
	} else {

	}
#endif

	if (switch_channel_test_flag(channel, CF_RECOVERED)) {
		const char *check = switch_channel_get_variable(channel, "last_transfered_conference");
		
		if (!zstr(check)) {
			conf_name = (char *) check;
		}
	}

	switch_event_create(&params, SWITCH_EVENT_COMMAND);
	switch_assert(params);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "conf_name", conf_name);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "profile_name", profile_name);

	/* Open the config from the xml registry */
	if (!(cxml = switch_xml_open_cfg(global_cf_name, &cfg, params))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Open of %s failed\n", global_cf_name);
		goto done;
	}

	if ((profiles = switch_xml_child(cfg, "profiles"))) {
		xml_cfg.profile = switch_xml_find_child(profiles, "profile", "name", profile_name);
	}

	/* if this is a bridging call, and it's not a duplicate, build a */
	/* conference object, and skip pin handling, and locked checking */

	if (!locked) {
		switch_mutex_lock(globals.setup_mutex);
		locked = 1;
	}

	if (isbr) {
		char *uuid = switch_core_session_get_uuid(session);

		if (!strcmp(conf_name, "_uuid_")) {
			conf_name = uuid;
		}

		if ((conference = conference_find(conf_name, NULL))) {
			switch_thread_rwlock_unlock(conference->rwlock);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Conference %s already exists!\n", conf_name);
			goto done;
		}

		/* Create the conference object. */
		conference = conference_new(conf_name, xml_cfg, session, NULL);

		if (!conference) {
			goto done;
		}

		set_cflags(cflags_str, conference->flags);

		if (locked) {
			switch_mutex_unlock(globals.setup_mutex);
			locked = 0;
		}

		switch_channel_set_variable(channel, "conference_name", conference->name);

		/* Set the minimum number of members (once you go above it you cannot go below it) */
		conference->min = 2;

		/* Indicate the conference is dynamic */
		conference_set_flag_locked(conference, CFLAG_DYNAMIC);

		/* Indicate the conference has a bridgeto party */
		conference_set_flag_locked(conference, CFLAG_BRIDGE_TO);

		/* Start the conference thread for this conference */
		launch_conference_thread(conference);

	} else {
		int enforce_security =  switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_INBOUND;
		const char *pvar = switch_channel_get_variable(channel, "conference_enforce_security");

		if (pvar) {
			enforce_security = switch_true(pvar);
		}

		if ((conference = conference_find(conf_name, NULL))) {
			if (locked) {
				switch_mutex_unlock(globals.setup_mutex);
				locked = 0;
			}
		}

		/* if the conference exists, get the pointer to it */
		if (!conference) {
			const char *max_members_str;
			const char *endconf_grace_time_str;
			const char *auto_record_str;

			/* no conference yet, so check for join-only flag */
			if (flags_str) {
				set_mflags(flags_str, mflags);

				if (!(mflags[MFLAG_CAN_SPEAK])) {
					if (!(mflags[MFLAG_MUTE_DETECT])) {
						switch_core_media_hard_mute(session, SWITCH_TRUE);
					}
				}

				if (mflags[MFLAG_JOIN_ONLY]) {
					switch_event_t *event;
					switch_xml_t jos_xml;
					char *val;
					/* send event */
					switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT);
					switch_channel_event_set_basic_data(channel, event);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Conference-Name", conf_name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Conference-Profile-Name", profile_name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "rejected-join-only");
					switch_event_fire(&event);
					/* check what sound file to play */
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Cannot create a conference since join-only flag is set\n");
					jos_xml = switch_xml_find_child(xml_cfg.profile, "param", "name", "join-only-sound");
					if (jos_xml && (val = (char *) switch_xml_attr_soft(jos_xml, "value"))) {
							switch_channel_answer(channel);
							switch_ivr_play_file(session, NULL, val, NULL);
					}
					if (!switch_false(switch_channel_get_variable(channel, "hangup_after_conference"))) {
						switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
					}
					goto done;
				}
			}

			/* couldn't find the conference, create one */
			conference = conference_new(conf_name, xml_cfg, session, NULL);

			if (!conference) {
				goto done;
			}

			set_cflags(cflags_str, conference->flags);

			if (locked) {
				switch_mutex_unlock(globals.setup_mutex);
				locked = 0;
			}

			switch_channel_set_variable(channel, "conference_name", conference->name);

			/* Set MOH from variable if not set */
			if (zstr(conference->moh_sound)) {
				conference->moh_sound = switch_core_strdup(conference->pool, switch_channel_get_variable(channel, "conference_moh_sound"));
			}

			/* Set perpetual-sound from variable if not set */
			if (zstr(conference->perpetual_sound)) {
				conference->perpetual_sound = switch_core_strdup(conference->pool, switch_channel_get_variable(channel, "conference_perpetual_sound"));
			}

			/* Override auto-record profile parameter from variable */
			if (!zstr(auto_record_str = switch_channel_get_variable(channel, "conference_auto_record"))) {
				conference->auto_record = switch_core_strdup(conference->pool, auto_record_str);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
								  "conference_auto_record set from variable to %s\n", auto_record_str);
			}

			/* Set the minimum number of members (once you go above it you cannot go below it) */
			conference->min = 1;

			/* check for variable used to specify override for max_members */
			if (!zstr(max_members_str = switch_channel_get_variable(channel, "conference_max_members"))) {
				uint32_t max_members_val;
				errno = 0;		/* sanity first */
				max_members_val = strtol(max_members_str, NULL, 0);	/* base 0 lets 0x... for hex 0... for octal and base 10 otherwise through */
				if (errno == ERANGE || errno == EINVAL || (int32_t) max_members_val < 0 || max_members_val == 1) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
									  "conference_max_members variable %s is invalid, not setting a limit\n", max_members_str);
				} else {
					conference->max_members = max_members_val;
				}
			}

			/* check for variable to override endconf_grace_time profile value */
			if (!zstr(endconf_grace_time_str = switch_channel_get_variable(channel, "conference_endconf_grace_time"))) {
				uint32_t grace_time_val;
				errno = 0;		/* sanity first */
				grace_time_val = strtol(endconf_grace_time_str, NULL, 0);	/* base 0 lets 0x... for hex 0... for octal and base 10 otherwise through */
				if (errno == ERANGE || errno == EINVAL || (int32_t) grace_time_val < 0) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
									  "conference_endconf_grace_time variable %s is invalid, not setting a time limit\n", endconf_grace_time_str);
				} else {
					conference->endconf_grace_time = grace_time_val;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
									  "conference endconf_grace_time set from variable to %d\n", grace_time_val);
				}
			}

			/* Indicate the conference is dynamic */
			conference_set_flag_locked(conference, CFLAG_DYNAMIC);

			/* acquire a read lock on the thread so it can't leave without us */
			if (switch_thread_rwlock_tryrdlock(conference->rwlock) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Read Lock Fail\n");
				goto done;
			}

			rl++;
			
			/* Start the conference thread for this conference */
			launch_conference_thread(conference);
		} else {				/* setup user variable */
			switch_channel_set_variable(channel, "conference_name", conference->name);
			rl++;
		}

		/* Moderator PIN as a channel variable */
		mdpin = switch_channel_get_variable(channel, "conference_moderator_pin");

		if (zstr(dpin) && conference->pin) {
			dpin = conference->pin;
		}
		if (zstr(mdpin) && conference->mpin) {
			mdpin = conference->mpin;
		}


		/* if this is not an outbound call, deal with conference pins */
		if (enforce_security && (!zstr(dpin) || !zstr(mdpin))) {
			char pin_buf[80] = "";
			int pin_retries = conference->pin_retries;
			int pin_valid = 0;
			switch_status_t status = SWITCH_STATUS_SUCCESS;
			char *supplied_pin_value;

			/* Answer the channel */
			switch_channel_answer(channel);

			/* look for PIN in channel variable first.  If not present or invalid revert to prompting user */
			supplied_pin_value = switch_core_strdup(conference->pool, switch_channel_get_variable(channel, "supplied_pin"));
			if (!zstr(supplied_pin_value)) {
				char *supplied_pin_value_start;
				int i = 0;
				if ((supplied_pin_value_start = (char *) switch_stristr(cf_pin_url_param_name, supplied_pin_value))) {
					/* pin supplied as a URL parameter, move pointer to start of actual pin value */
					supplied_pin_value = supplied_pin_value_start + strlen(cf_pin_url_param_name);
				}
				while (*supplied_pin_value != 0 && *supplied_pin_value != ';') {
					pin_buf[i++] = *supplied_pin_value++;
				}

				validate_pin(pin_buf, dpin, mdpin);
				memset(pin_buf, 0, sizeof(pin_buf));
			}

			if (!conference->pin_sound) {
				conference->pin_sound = switch_core_strdup(conference->pool, "conference/conf-pin.wav");
			}

			if (!conference->bad_pin_sound) {
				conference->bad_pin_sound = switch_core_strdup(conference->pool, "conference/conf-bad-pin.wav");
			}

			while (!pin_valid && pin_retries && status == SWITCH_STATUS_SUCCESS) {
				size_t dpin_length = dpin ? strlen(dpin) : 0;
				size_t mdpin_length = mdpin ? strlen(mdpin) : 0;
				int maxpin = dpin_length > mdpin_length ? (int)dpin_length : (int)mdpin_length;
				switch_status_t pstatus = SWITCH_STATUS_FALSE;

				/* be friendly */
				if (conference->pin_sound) {
					pstatus = conference_local_play_file(conference, session, conference->pin_sound, 20, pin_buf, sizeof(pin_buf));
				} else if (conference->tts_engine && conference->tts_voice) {
					pstatus =
						switch_ivr_speak_text(session, conference->tts_engine, conference->tts_voice, "please enter the conference pin number", NULL);
				} else {
					pstatus = switch_ivr_speak_text(session, "flite", "slt", "please enter the conference pin number", NULL);
				}

				if (pstatus != SWITCH_STATUS_SUCCESS && pstatus != SWITCH_STATUS_BREAK) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Cannot ask the user for a pin, ending call\n");
					switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				}

				/* wait for them if neccessary */
				if ((int)strlen(pin_buf) < maxpin) {
					char *buf = pin_buf + strlen(pin_buf);
					char term = '\0';

					status = switch_ivr_collect_digits_count(session,
															 buf,
															 sizeof(pin_buf) - strlen(pin_buf), maxpin - strlen(pin_buf), "#", &term, 10000, 0, 0);
					if (status == SWITCH_STATUS_TIMEOUT) {
						status = SWITCH_STATUS_SUCCESS;
					}
				}

				if (status == SWITCH_STATUS_SUCCESS) {
					validate_pin(pin_buf, dpin, mdpin);
				}

				if (!pin_valid) {
					/* zero the collected pin */
					memset(pin_buf, 0, sizeof(pin_buf));

					/* more friendliness */
					if (conference->bad_pin_sound) {
						conference_local_play_file(conference, session, conference->bad_pin_sound, 20, NULL, 0);
					}
					switch_channel_flush_dtmf(channel);
				}
				pin_retries--;
			}

			if (!pin_valid) {
				conference_cdr_rejected(conference, channel, CDRR_PIN);
				goto done;
			}
		}

		if (conference->special_announce && !switch_channel_test_app_flag_key("conf_silent", channel, CONF_SILENT_REQ)) {
			conference_local_play_file(conference, session, conference->special_announce, CONF_DEFAULT_LEADIN, NULL, 0);
		}

		/* don't allow more callers if the conference is locked, unless we invited them */
		if (conference_test_flag(conference, CFLAG_LOCKED) && enforce_security) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Conference %s is locked.\n", conf_name);
			conference_cdr_rejected(conference, channel, CDRR_LOCKED);
			if (conference->locked_sound) {
				/* Answer the channel */
				switch_channel_answer(channel);
				conference_local_play_file(conference, session, conference->locked_sound, 20, NULL, 0);
			}
			goto done;
		}

		/* dont allow more callers than the max_members allows for -- I explicitly didnt allow outbound calls
		 * someone else can add that (see above) if they feel that outbound calls should be able to violate the
		 * max_members limit
		 */
		if ((conference->max_members > 0) && (conference->count >= conference->max_members)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Conference %s is full.\n", conf_name);
			conference_cdr_rejected(conference, channel, CDRR_MAXMEMBERS);
			if (conference->maxmember_sound) {
				/* Answer the channel */
				switch_channel_answer(channel);
				conference_local_play_file(conference, session, conference->maxmember_sound, 20, NULL, 0);
			}
			goto done;
		}

	}

	/* Release the config registry handle */
	if (cxml) {
		switch_xml_free(cxml);
		cxml = NULL;
	}

	/* if we're using "bridge:" make an outbound call and bridge it in */
	if (!zstr(bridgeto) && strcasecmp(bridgeto, "none")) {
		switch_call_cause_t cause;
		if (conference_outcall(conference, NULL, session, bridgeto, 60, NULL, NULL, NULL, NULL, &cause, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
			goto done;
		}
	} else {
		/* if we're not using "bridge:" set the conference answered flag */
		/* and this isn't an outbound channel, answer the call */
		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_INBOUND)
			conference_set_flag(conference, CFLAG_ANSWERED);
	}

	member.session = session;
	member.channel = switch_core_session_get_channel(session);
	member.pool = switch_core_session_get_pool(session);

	/* Prepare MUTEXS */
	switch_mutex_init(&member.flag_mutex, SWITCH_MUTEX_NESTED, member.pool);
	switch_mutex_init(&member.write_mutex, SWITCH_MUTEX_NESTED, member.pool);
	switch_mutex_init(&member.read_mutex, SWITCH_MUTEX_NESTED, member.pool);
	switch_mutex_init(&member.fnode_mutex, SWITCH_MUTEX_NESTED, member.pool);
	switch_mutex_init(&member.audio_in_mutex, SWITCH_MUTEX_NESTED, member.pool);
	switch_mutex_init(&member.audio_out_mutex, SWITCH_MUTEX_NESTED, member.pool);
	switch_thread_rwlock_create(&member.rwlock, member.pool);

	if (setup_media(&member, conference)) {
		//flags = 0;
		goto done;
	}


	if (!(mid = switch_channel_get_private(channel, "__confmid"))) {
		mid = switch_core_session_alloc(session, sizeof(*mid));
		*mid = next_member_id();
		switch_channel_set_private(channel, "__confmid", mid);
	}

	switch_channel_set_variable_printf(channel, "conference_member_id", "%u", *mid);
	member.id = *mid;


	/* Install our Signed Linear codec so we get the audio in that format */
	switch_core_session_set_read_codec(member.session, &member.read_codec);

	
	memcpy(mflags, conference->mflags, sizeof(mflags));
	
	set_mflags(flags_str, mflags);
	mflags[MFLAG_RUNNING] = 1;

	if (!(mflags[MFLAG_CAN_SPEAK])) {
		if (!(mflags[MFLAG_MUTE_DETECT])) {
			switch_core_media_hard_mute(member.session, SWITCH_TRUE);
		}
	}

	if (mpin_matched) {
		mflags[MFLAG_MOD] = 1;
	}

	merge_mflags(member.flags, mflags);


	if (mflags[MFLAG_MINTWO]) {
		conference->min = 2;
	}


	if (conference->conf_video_mode == CONF_VIDEO_MODE_MUX) {
		switch_queue_create(&member.video_queue, 200, member.pool);
		switch_queue_create(&member.mux_out_queue, 200, member.pool);
		switch_frame_buffer_create(&member.fb);
	}
	
	/* Add the caller to the conference */
	if (conference_add_member(conference, &member) != SWITCH_STATUS_SUCCESS) {
		switch_core_codec_destroy(&member.read_codec);
		goto done;
	}

	if (conference->conf_video_mode == CONF_VIDEO_MODE_MUX) {
		launch_conference_video_muxing_write_thread(&member);
	}
	
	msg.from = __FILE__;

	/* Tell the channel we are going to be in a bridge */
	msg.message_id = SWITCH_MESSAGE_INDICATE_BRIDGE;
	switch_core_session_receive_message(session, &msg);

	if (conference_test_flag(conference, CFLAG_TRANSCODE_VIDEO)) {
		switch_channel_set_flag(channel, CF_VIDEO_DECODED_READ);
		switch_core_media_gen_key_frame(session);
	}
	
	/* Chime in the core video thread */
	switch_core_session_set_video_read_callback(session, video_thread_callback, (void *)&member);

	if (switch_channel_test_flag(channel, CF_VIDEO_ONLY)) {
		while(member_test_flag((&member), MFLAG_RUNNING) && switch_channel_ready(channel)) {
			switch_yield(100000);
		}
	} else {

		/* Run the conference loop */
		do {
			conference_loop_output(&member);
		} while (member.loop_loop);
	}

	switch_core_session_set_video_read_callback(session, NULL, NULL);

	switch_channel_set_private(channel, "_conference_autocall_list_", NULL);

	/* Tell the channel we are no longer going to be in a bridge */
	msg.message_id = SWITCH_MESSAGE_INDICATE_UNBRIDGE;
	switch_core_session_receive_message(session, &msg);

	/* Remove the caller from the conference */
	conference_del_member(member.conference, &member);

	/* Put the original codec back */
	switch_core_session_set_read_codec(member.session, NULL);

	/* Clean Up. */

  done:

	if (member.video_muxing_write_thread) {
		switch_status_t st = SWITCH_STATUS_SUCCESS;
		switch_queue_push(member.mux_out_queue, NULL);
		switch_thread_join(&st, member.video_muxing_write_thread);
		member.video_muxing_write_thread = NULL;
	}

	
	if (locked) {
		switch_mutex_unlock(globals.setup_mutex);
	}

	if (member.read_resampler) {
		switch_resample_destroy(&member.read_resampler);
	}

	switch_event_destroy(&params);
	switch_buffer_destroy(&member.resample_buffer);
	switch_buffer_destroy(&member.audio_buffer);
	switch_buffer_destroy(&member.mux_buffer);

	if (member.fb) {
		switch_frame_buffer_destroy(&member.fb);
	}

	if (conference) {
		switch_mutex_lock(conference->mutex);
		if (conference_test_flag(conference, CFLAG_DYNAMIC) && conference->count == 0) {
			conference_set_flag_locked(conference, CFLAG_DESTRUCT);
		}
		switch_mutex_unlock(conference->mutex);
	}

	/* Release the config registry handle */
	if (cxml) {
		switch_xml_free(cxml);
	}

	if (conference && member_test_flag(&member, MFLAG_KICKED) && conference->kicked_sound) {
		char *toplay = NULL;
		char *dfile = NULL;
		char *expanded = NULL;
		char *src = member.kicked_sound ? member.kicked_sound : conference->kicked_sound;


		if (!strncasecmp(src, "say:", 4)) {
			if (conference->tts_engine && conference->tts_voice) {
				switch_ivr_speak_text(session, conference->tts_engine, conference->tts_voice, src + 4, NULL);
			}
		} else {
			if ((expanded = switch_channel_expand_variables(switch_core_session_get_channel(session), src)) != src) {
				toplay = expanded;
			} else {
				expanded = NULL;
				toplay = src; 
			}

			if (!switch_is_file_path(toplay) && conference->sound_prefix) {
				dfile = switch_mprintf("%s%s%s", conference->sound_prefix, SWITCH_PATH_SEPARATOR, toplay);
				switch_assert(dfile);
				toplay = dfile;
			}

			switch_ivr_play_file(session, NULL, toplay, NULL);
			switch_safe_free(dfile);
			switch_safe_free(expanded);
		}
	}

	switch_core_session_reset(session, SWITCH_TRUE, SWITCH_TRUE);

	/* release the readlock */
	if (rl) {
		switch_thread_rwlock_unlock(conference->rwlock);
	}

	switch_channel_set_variable(channel, "last_transfered_conference", NULL);

 end:

	switch_channel_clear_flag(channel, CF_CONFERENCE);

	switch_core_session_video_reset(session);
}



static void launch_conference_video_muxing_write_thread(conference_member_t *member)
{
	switch_threadattr_t *thd_attr = NULL;
	switch_mutex_lock(globals.hash_mutex);
	if (!member->video_muxing_write_thread) { 
		switch_threadattr_create(&thd_attr, member->pool);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		switch_thread_create(&member->video_muxing_write_thread, thd_attr, conference_video_muxing_write_thread_run, member, member->pool);
	}
	switch_mutex_unlock(globals.hash_mutex);
}
static void launch_conference_video_muxing_thread(conference_obj_t *conference, mcu_canvas_t *canvas, int super)
{
	switch_threadattr_t *thd_attr = NULL;

	switch_mutex_lock(globals.hash_mutex);
	if (!canvas->video_muxing_thread) { 
		switch_threadattr_create(&thd_attr, conference->pool);
		switch_threadattr_priority_set(thd_attr, SWITCH_PRI_REALTIME);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		conference_set_flag(conference, CFLAG_VIDEO_MUXING);
		switch_thread_create(&canvas->video_muxing_thread, thd_attr, 
							 super ? conference_super_video_muxing_thread_run : conference_video_muxing_thread_run, canvas, conference->pool);
	}
	switch_mutex_unlock(globals.hash_mutex);
}

/* Create a thread for the conference and launch it */
static void launch_conference_thread(conference_obj_t *conference)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	conference_set_flag_locked(conference, CFLAG_RUNNING);
	switch_threadattr_create(&thd_attr, conference->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_priority_set(thd_attr, SWITCH_PRI_REALTIME);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_mutex_lock(globals.hash_mutex);
	switch_mutex_unlock(globals.hash_mutex);
	switch_thread_create(&thread, thd_attr, conference_thread_run, conference, conference->pool);
}

static void launch_conference_record_thread(conference_obj_t *conference, char *path, switch_bool_t autorec)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_memory_pool_t *pool;
	conference_record_t *rec;

	/* Setup a memory pool to use. */
	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool Failure\n");
	}

	/* Create a node object */
	if (!(rec = switch_core_alloc(pool, sizeof(*rec)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Alloc Failure\n");
		switch_core_destroy_memory_pool(&pool);
		return;
	}

	rec->conference = conference;
	rec->path = switch_core_strdup(pool, path);
	rec->pool = pool;
	rec->autorec = autorec;

	switch_mutex_lock(conference->flag_mutex);
	rec->next = conference->rec_node_head;
	conference->rec_node_head = rec;
	switch_mutex_unlock(conference->flag_mutex);

	switch_threadattr_create(&thd_attr, rec->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, conference_record_thread_run, rec, rec->pool);
}

static switch_status_t chat_send(switch_event_t *message_event)
{
	char name[512] = "", *p, *lbuf = NULL;
	conference_obj_t *conference = NULL;
	switch_stream_handle_t stream = { 0 };
	const char *proto;
	const char *from; 
	const char *to;
	//const char *subject;
	const char *body;
	//const char *type;
	const char *hint;

	proto = switch_event_get_header(message_event, "proto");
	from = switch_event_get_header(message_event, "from");
	to = switch_event_get_header(message_event, "to");
	body = switch_event_get_body(message_event);
	hint = switch_event_get_header(message_event, "hint");


	if ((p = strchr(to, '+'))) {
		to = ++p;
	}

	if (!body) {
		return SWITCH_STATUS_SUCCESS;
	}

	if ((p = strchr(to, '@'))) {
		switch_copy_string(name, to, ++p - to);
	} else {
		switch_copy_string(name, to, sizeof(name));
	}

	if (!(conference = conference_find(name, NULL))) {
		switch_core_chat_send_args(proto, CONF_CHAT_PROTO, to, hint && strchr(hint, '/') ? hint : from, "", 
								   "Conference not active.", NULL, NULL, SWITCH_FALSE);
		return SWITCH_STATUS_FALSE;
	}

	SWITCH_STANDARD_STREAM(stream);

	if (body != NULL && (lbuf = strdup(body))) {
		/* special case list */
		if (conference->broadcast_chat_messages) {
			chat_message_broadcast(conference, message_event);
		} else if (switch_stristr("list", lbuf)) {
			conference_list_pretty(conference, &stream);
			/* provide help */
		} else {
			return SWITCH_STATUS_SUCCESS;
		}
	}

	switch_safe_free(lbuf);

	if (!conference->broadcast_chat_messages) {
		switch_core_chat_send_args(proto, CONF_CHAT_PROTO, to, hint && strchr(hint, '/') ? hint : from, "", stream.data, NULL, NULL, SWITCH_FALSE);
	}

	switch_safe_free(stream.data);
	switch_thread_rwlock_unlock(conference->rwlock);

	return SWITCH_STATUS_SUCCESS;
}

static conference_obj_t *conference_find(char *name, char *domain)
{
	conference_obj_t *conference;

	switch_mutex_lock(globals.hash_mutex);
	if ((conference = switch_core_hash_find(globals.conference_hash, name))) {
		if (conference_test_flag(conference, CFLAG_DESTRUCT)) {
			switch_core_hash_delete(globals.conference_hash, conference->name);
			conference_clear_flag(conference, CFLAG_INHASH);
			conference = NULL;
		} else if (!zstr(domain) && conference->domain && strcasecmp(domain, conference->domain)) {
			conference = NULL;
		}
	}
	if (conference) {
		if (switch_thread_rwlock_tryrdlock(conference->rwlock) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Read Lock Fail\n");
			conference = NULL;
		}
	}
	switch_mutex_unlock(globals.hash_mutex);

	return conference;
}

/* create a new conferene with a specific profile */
static conference_obj_t *conference_new(char *name, conf_xml_cfg_t cfg, switch_core_session_t *session, switch_memory_pool_t *pool)
{
	conference_obj_t *conference;
	switch_xml_t xml_kvp;
	char *timer_name = NULL;
	char *domain = NULL;
	char *desc = NULL;
	char *name_domain = NULL;
	char *tts_engine = NULL;
	char *tts_voice = NULL;
	char *enter_sound = NULL;
	char *sound_prefix = NULL;
	char *exit_sound = NULL;
	char *alone_sound = NULL;
	char *muted_sound = NULL;
	char *mute_detect_sound = NULL;
	char *unmuted_sound = NULL;
	char *locked_sound = NULL;
	char *is_locked_sound = NULL;
	char *is_unlocked_sound = NULL;
	char *kicked_sound = NULL;
	char *join_only_sound = NULL;
	char *pin = NULL;
	char *mpin = NULL;
	char *pin_sound = NULL;
	char *bad_pin_sound = NULL;
	char *energy_level = NULL;
	char *auto_gain_level = NULL;
	char *caller_id_name = NULL;
	char *caller_id_number = NULL;
	char *caller_controls = NULL;
	char *moderator_controls = NULL;
	char *member_flags = NULL;
	char *conference_flags = NULL;
	char *perpetual_sound = NULL;
	char *moh_sound = NULL;
	char *outcall_templ = NULL;
	char *video_layout_name = NULL;
	char *video_layout_group = NULL;
	char *video_canvas_size = NULL;
	char *video_canvas_bgcolor = NULL;
	char *video_super_canvas_bgcolor = NULL;
	char *video_letterbox_bgcolor = NULL;
	char *video_codec_bandwidth = NULL;
	char *no_video_avatar = NULL;
	conf_video_mode_t conf_video_mode = CONF_VIDEO_MODE_PASSTHROUGH;
	float fps = 15.0f;
	uint32_t max_members = 0;
	uint32_t announce_count = 0;
	char *maxmember_sound = NULL;
	uint32_t rate = 8000, interval = 20;
	uint32_t channels = 1;
	int broadcast_chat_messages = 1;
	int comfort_noise_level = 0;
	int pin_retries = 3;
	int ivr_dtmf_timeout = 500;
	int ivr_input_timeout = 0;
	int video_canvas_count = 0;
	int video_super_canvas_label_layers = 0;
	int video_super_canvas_show_all_layers = 0;
	char *suppress_events = NULL;
	char *verbose_events = NULL;
	char *auto_record = NULL;
	int min_recording_participants = 1;
	char *conference_log_dir = NULL;
	char *cdr_event_mode = NULL;
	char *terminate_on_silence = NULL;
	char *endconf_grace_time = NULL;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH+1];
	switch_uuid_t uuid;
	switch_codec_implementation_t read_impl = { 0 };
	switch_channel_t *channel = NULL;
	const char *force_rate = NULL, *force_interval = NULL, *force_channels = NULL, *presence_id = NULL;
	uint32_t force_rate_i = 0, force_interval_i = 0, force_channels_i = 0, video_auto_floor_msec = 0;

	/* Validate the conference name */
	if (zstr(name)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Record! no name.\n");
		return NULL;
	}

	if (session) {
		uint32_t tmp;

		switch_core_session_get_read_impl(session, &read_impl);
		channel = switch_core_session_get_channel(session);
		
		presence_id = switch_channel_get_variable(channel, "presence_id");

		if ((force_rate = switch_channel_get_variable(channel, "conference_force_rate"))) {
			if (!strcasecmp(force_rate, "auto")) {
				force_rate_i = read_impl.actual_samples_per_second;
			} else {
				tmp = atoi(force_rate);

				if (tmp == 8000 || tmp == 12000 || tmp == 16000 || tmp == 24000 || tmp == 32000 || tmp == 44100 || tmp == 48000) {
					force_rate_i = rate = tmp;
				}
			}
		}

		if ((force_channels = switch_channel_get_variable(channel, "conference_force_channels"))) {
			if (!strcasecmp(force_channels, "auto")) {
				force_rate_i = read_impl.number_of_channels;
			} else {
				tmp = atoi(force_channels);

				if (tmp == 1 || tmp == 2) {
					force_channels_i = channels = tmp;
				}
			}
		}

		if ((force_interval = switch_channel_get_variable(channel, "conference_force_interval"))) {
			if (!strcasecmp(force_interval, "auto")) {
				force_interval_i = read_impl.microseconds_per_packet / 1000;
			} else {
				tmp = atoi(force_interval);
				
				if (SWITCH_ACCEPTABLE_INTERVAL(tmp)) {
					force_interval_i = interval = tmp;
				}
			}
		}
	}

	switch_mutex_lock(globals.hash_mutex);

	/* parse the profile tree for param values */
	if (cfg.profile)
		for (xml_kvp = switch_xml_child(cfg.profile, "param"); xml_kvp; xml_kvp = xml_kvp->next) {
			char *var = (char *) switch_xml_attr_soft(xml_kvp, "name");
			char *val = (char *) switch_xml_attr_soft(xml_kvp, "value");
			char buf[128] = "";
			char *p;

			if (strchr(var, '_')) {
				switch_copy_string(buf, var, sizeof(buf));
				for (p = buf; *p; p++) {
					if (*p == '_') {
						*p = '-';
					}
				}
				var = buf;
			}

			if (!force_rate_i && !strcasecmp(var, "rate") && !zstr(val)) {
				uint32_t tmp = atoi(val);
				if (session && tmp == 0) {
					if (!strcasecmp(val, "auto")) {
						rate = read_impl.actual_samples_per_second;
					}
				} else {
					if (tmp == 8000 || tmp == 12000 || tmp == 16000 || tmp == 24000 || tmp == 32000 || tmp == 44100 || tmp == 48000) {
						rate = tmp;
					}
				}
			} else if (!force_channels_i && !strcasecmp(var, "channels") && !zstr(val)) {
				uint32_t tmp = atoi(val);
				if (session && tmp == 0) {
					if (!strcasecmp(val, "auto")) {
						channels = read_impl.number_of_channels;
					}
				} else {
					if (tmp == 1 || tmp == 2) {
						channels = tmp;
					}
				}
			} else if (!strcasecmp(var, "domain") && !zstr(val)) {
				domain = val;
			} else if (!strcasecmp(var, "description") && !zstr(val)) {
				desc = val;
			} else if (!force_interval_i && !strcasecmp(var, "interval") && !zstr(val)) {
				uint32_t tmp = atoi(val);

				if (session && tmp == 0) {
					if (!strcasecmp(val, "auto")) {
						interval = read_impl.microseconds_per_packet / 1000;
					}
				} else {
					if (SWITCH_ACCEPTABLE_INTERVAL(tmp)) {
						interval = tmp;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
										  "Interval must be multipe of 10 and less than %d, Using default of 20\n", SWITCH_MAX_INTERVAL);
					}
				}
			} else if (!strcasecmp(var, "timer-name") && !zstr(val)) {
				timer_name = val;
			} else if (!strcasecmp(var, "tts-engine") && !zstr(val)) {
				tts_engine = val;
			} else if (!strcasecmp(var, "tts-voice") && !zstr(val)) {
				tts_voice = val;
			} else if (!strcasecmp(var, "enter-sound") && !zstr(val)) {
				enter_sound = val;
			} else if (!strcasecmp(var, "outcall-templ") && !zstr(val)) {
				outcall_templ = val;
			} else if (!strcasecmp(var, "video-layout-name") && !zstr(val)) {
				video_layout_name = val;
			} else if (!strcasecmp(var, "video-canvas-count") && !zstr(val)) {
				video_canvas_count = atoi(val);
			} else if (!strcasecmp(var, "video-super-canvas-label-layers") && !zstr(val)) {
				video_super_canvas_label_layers = atoi(val);
			} else if (!strcasecmp(var, "video-super-canvas-show-all-layers") && !zstr(val)) {
				video_super_canvas_show_all_layers = atoi(val);
			} else if (!strcasecmp(var, "video-canvas-bgcolor") && !zstr(val)) {
				video_canvas_bgcolor= val;
			} else if (!strcasecmp(var, "video-super-canvas-bgcolor") && !zstr(val)) {
				video_super_canvas_bgcolor= val;
			} else if (!strcasecmp(var, "video-letterbox-bgcolor") && !zstr(val)) {
				video_letterbox_bgcolor= val;
			} else if (!strcasecmp(var, "video-canvas-size") && !zstr(val)) {
				video_canvas_size = val;
			} else if (!strcasecmp(var, "video-fps") && !zstr(val)) {
				fps = atof(val);
			} else if (!strcasecmp(var, "video-codec-bandwidth") && !zstr(val)) {
				video_codec_bandwidth = val;
			} else if (!strcasecmp(var, "video-no-video-avatar") && !zstr(val)) {
				no_video_avatar = val;
			} else if (!strcasecmp(var, "exit-sound") && !zstr(val)) {
				exit_sound = val;
			} else if (!strcasecmp(var, "alone-sound") && !zstr(val)) {
				alone_sound = val;
			} else if (!strcasecmp(var, "perpetual-sound") && !zstr(val)) {
				perpetual_sound = val;
			} else if (!strcasecmp(var, "moh-sound") && !zstr(val)) {
				moh_sound = val;
			} else if (!strcasecmp(var, "muted-sound") && !zstr(val)) {
				muted_sound = val;
			} else if (!strcasecmp(var, "mute-detect-sound") && !zstr(val)) {
				mute_detect_sound = val;
			} else if (!strcasecmp(var, "unmuted-sound") && !zstr(val)) {
				unmuted_sound = val;
			} else if (!strcasecmp(var, "locked-sound") && !zstr(val)) {
				locked_sound = val;
			} else if (!strcasecmp(var, "is-locked-sound") && !zstr(val)) {
				is_locked_sound = val;
			} else if (!strcasecmp(var, "is-unlocked-sound") && !zstr(val)) {
				is_unlocked_sound = val;
			} else if (!strcasecmp(var, "member-flags") && !zstr(val)) {
				member_flags = val;
			} else if (!strcasecmp(var, "conference-flags") && !zstr(val)) {
				conference_flags = val;
			} else if (!strcasecmp(var, "cdr-log-dir") && !zstr(val)) {
				conference_log_dir = val;
			} else if (!strcasecmp(var, "cdr-event-mode") && !zstr(val)) {
				cdr_event_mode = val;
			} else if (!strcasecmp(var, "kicked-sound") && !zstr(val)) {
				kicked_sound = val;
			} else if (!strcasecmp(var, "join-only-sound") && !zstr(val)) {
				join_only_sound = val;
			} else if (!strcasecmp(var, "pin") && !zstr(val)) {
				pin = val;
			} else if (!strcasecmp(var, "moderator-pin") && !zstr(val)) {
				mpin = val;
			} else if (!strcasecmp(var, "pin-retries") && !zstr(val)) {
				int tmp = atoi(val);
				if (tmp >= 0) {
					pin_retries = tmp;
				}
			} else if (!strcasecmp(var, "pin-sound") && !zstr(val)) {
				pin_sound = val;
			} else if (!strcasecmp(var, "bad-pin-sound") && !zstr(val)) {
				bad_pin_sound = val;
			} else if (!strcasecmp(var, "energy-level") && !zstr(val)) {
				energy_level = val;
			} else if (!strcasecmp(var, "auto-gain-level") && !zstr(val)) {
				auto_gain_level = val;
			} else if (!strcasecmp(var, "caller-id-name") && !zstr(val)) {
				caller_id_name = val;
			} else if (!strcasecmp(var, "caller-id-number") && !zstr(val)) {
				caller_id_number = val;
			} else if (!strcasecmp(var, "caller-controls") && !zstr(val)) {
				caller_controls = val;
                       } else if (!strcasecmp(var, "ivr-dtmf-timeout") && !zstr(val)) {
                               ivr_dtmf_timeout = atoi(val);
                               if (ivr_dtmf_timeout < 500) {
                                       switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "not very smart value for ivr-dtmf-timeout found (%d), defaulting to 500ms\n", ivr_dtmf_timeout);
                                       ivr_dtmf_timeout = 500;
                               }
                       } else if (!strcasecmp(var, "ivr-input-timeout") && !zstr(val)) {
                               ivr_input_timeout = atoi(val);
                               if (ivr_input_timeout != 0 && ivr_input_timeout < 500) {
                                       switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "not very smart value for ivr-input-timeout found (%d), defaulting to 500ms\n", ivr_input_timeout);
                                       ivr_input_timeout = 5000;
                               }
			} else if (!strcasecmp(var, "moderator-controls") && !zstr(val)) {
				moderator_controls = val;
			} else if (!strcasecmp(var, "broadcast-chat-messages") && !zstr(val)) {
				broadcast_chat_messages = switch_true(val);
			} else if (!strcasecmp(var, "comfort-noise") && !zstr(val)) {
				int tmp;
				tmp = atoi(val);
				if (tmp > 1 && tmp < 10000) {
					comfort_noise_level = tmp;
				} else if (switch_true(val)) {
					comfort_noise_level = 1400;
				}
			} else if (!strcasecmp(var, "video-auto-floor-msec") && !zstr(val)) {
				int tmp;
				tmp = atoi(val);

				if (tmp > 0) {
					video_auto_floor_msec = tmp;
				}
			} else if (!strcasecmp(var, "sound-prefix") && !zstr(val)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "override sound-prefix with: %s\n", val);
				sound_prefix = val;
			} else if (!strcasecmp(var, "max-members") && !zstr(val)) {
				errno = 0;		/* sanity first */
				max_members = strtol(val, NULL, 0);	/* base 0 lets 0x... for hex 0... for octal and base 10 otherwise through */
				if (errno == ERANGE || errno == EINVAL || (int32_t) max_members < 0 || max_members == 1) {
					/* a negative wont work well, and its foolish to have a conference limited to 1 person unless the outbound 
					 * stuff is added, see comments above
					 */
					max_members = 0;	/* set to 0 to disable max counts */
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "max-members %s is invalid, not setting a limit\n", val);
				}
			} else if (!strcasecmp(var, "max-members-sound") && !zstr(val)) {
				maxmember_sound = val;
			} else if (!strcasecmp(var, "announce-count") && !zstr(val)) {
				errno = 0;		/* safety first */
				announce_count = strtol(val, NULL, 0);
				if (errno == ERANGE || errno == EINVAL) {
					announce_count = 0;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "announce-count is invalid, not anouncing member counts\n");
				}
			} else if (!strcasecmp(var, "suppress-events") && !zstr(val)) {
				suppress_events = val;
			} else if (!strcasecmp(var, "verbose-events") && !zstr(val)) {
				verbose_events = val;
			} else if (!strcasecmp(var, "auto-record") && !zstr(val)) {
				auto_record = val;
			} else if (!strcasecmp(var, "min-required-recording-participants") && !zstr(val)) {
				if (!strcmp(val, "1")) {
					min_recording_participants = 1;
				} else if (!strcmp(val, "2")) {
					min_recording_participants = 2;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "min-required-recording-participants is invalid, leaving set to %d\n", min_recording_participants);
				}
			} else if (!strcasecmp(var, "terminate-on-silence") && !zstr(val)) {
				terminate_on_silence = val;
			} else if (!strcasecmp(var, "endconf-grace-time") && !zstr(val)) {
				endconf_grace_time = val;
			} else if (!strcasecmp(var, "video-mode") && !zstr(val)) {
				if (!strcasecmp(val, "passthrough")) {
					conf_video_mode = CONF_VIDEO_MODE_PASSTHROUGH;
				} else if (!strcasecmp(val, "transcode")) {
					conf_video_mode = CONF_VIDEO_MODE_TRANSCODE;
				} else if (!strcasecmp(val, "mux")) {
					conf_video_mode = CONF_VIDEO_MODE_MUX;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "video-mode invalid, valid settings are 'passthrough', 'transcode' and 'mux'\n");
				}
			}
		}

	/* Set defaults and various paramaters */

	/* Timer module to use */
	if (zstr(timer_name)) {
		timer_name = "soft";
	}

	/* Caller ID Name */
	if (zstr(caller_id_name)) {
		caller_id_name = (char *) global_app_name;
	}

	/* Caller ID Number */
	if (zstr(caller_id_number)) {
		caller_id_number = SWITCH_DEFAULT_CLID_NUMBER;
	}

	if (!pool) {
		/* Setup a memory pool to use. */
		if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool Failure\n");
			conference = NULL;
			goto end;
		}
	}

	/* Create the conference object. */
	if (!(conference = switch_core_alloc(pool, sizeof(*conference)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
		conference = NULL;
		goto end;
	}

	conference->start_time = switch_epoch_time_now(NULL);

	/* initialize the conference object with settings from the specified profile */
	conference->pool = pool;
	conference->profile_name = switch_core_strdup(conference->pool, cfg.profile ? switch_xml_attr_soft(cfg.profile, "name") : "none");
	if (timer_name) {
		conference->timer_name = switch_core_strdup(conference->pool, timer_name);
	}
	if (tts_engine) {
		conference->tts_engine = switch_core_strdup(conference->pool, tts_engine);
	}
	if (tts_voice) {
		conference->tts_voice = switch_core_strdup(conference->pool, tts_voice);
	}

	conference->comfort_noise_level = comfort_noise_level;
	conference->pin_retries = pin_retries;
	conference->caller_id_name = switch_core_strdup(conference->pool, caller_id_name);
	conference->caller_id_number = switch_core_strdup(conference->pool, caller_id_number);
	conference->caller_controls = switch_core_strdup(conference->pool, caller_controls);
	conference->moderator_controls = switch_core_strdup(conference->pool, moderator_controls);
	conference->broadcast_chat_messages = broadcast_chat_messages;

	
	conference->conf_video_mode = conf_video_mode;

	if (!switch_core_has_video() && (conference->conf_video_mode == CONF_VIDEO_MODE_MUX || conference->conf_video_mode == CONF_VIDEO_MODE_TRANSCODE)) {
		conference->conf_video_mode = CONF_VIDEO_MODE_PASSTHROUGH;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "video-mode invalid, only valid setting is 'passthrough' due to no video capabilities\n");
	}
	
	if (conference->conf_video_mode == CONF_VIDEO_MODE_MUX) {
		int canvas_w = 0, canvas_h = 0;
		if (video_canvas_size) {
			char *p;
			
			if ((canvas_w = atoi(video_canvas_size))) {
				if ((p = strchr(video_canvas_size, 'x'))) {
					p++;
					if (*p) {
						canvas_h = atoi(p);
					}
				}
			}
		}

		if (canvas_w < 320 || canvas_h < 180) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "%s video-canvas-size, falling back to %ux%u\n",
							  video_canvas_size ? "Invalid" : "Unspecified", CONFERENCE_CANVAS_DEFAULT_WIDTH, CONFERENCE_CANVAS_DEFAULT_HIGHT);
			canvas_w = CONFERENCE_CANVAS_DEFAULT_WIDTH;
			canvas_h = CONFERENCE_CANVAS_DEFAULT_HIGHT;
		}
		
		
		conference_parse_layouts(conference, canvas_w, canvas_h);
		
		if (!video_canvas_bgcolor) {
			video_canvas_bgcolor = "#333333";
		}

		if (!video_super_canvas_bgcolor) {
			video_super_canvas_bgcolor = "#068df3";
		}

		if (!video_letterbox_bgcolor) {
			video_letterbox_bgcolor = "#000000";
		}

		if (no_video_avatar) {
			conference->no_video_avatar = switch_core_strdup(conference->pool, no_video_avatar);
		}

		conference->video_canvas_bgcolor = switch_core_strdup(conference->pool, video_canvas_bgcolor);
		conference->video_super_canvas_bgcolor = switch_core_strdup(conference->pool, video_super_canvas_bgcolor);
		conference->video_letterbox_bgcolor = switch_core_strdup(conference->pool, video_letterbox_bgcolor);

		if (fps) {
			conference_set_fps(conference, fps);
		}

		if (!conference->video_fps.ms) {
			conference_set_fps(conference, 30);
		}

		if (video_codec_bandwidth) {
			if (!strcasecmp(video_codec_bandwidth, "auto")) {
				conference->video_codec_settings.video.bandwidth = switch_calc_bitrate(canvas_w, canvas_h, 2, conference->video_fps.fps);
			} else {
				conference->video_codec_settings.video.bandwidth = switch_parse_bandwidth_string(video_codec_bandwidth);
			}
		}
		
		if (zstr(video_layout_name)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "No video-layout-name specified, using " CONFERENCE_MUX_DEFAULT_LAYOUT "\n");
			video_layout_name = CONFERENCE_MUX_DEFAULT_LAYOUT;
		}

		if (!strncasecmp(video_layout_name, "group:", 6)) {
			video_layout_group = video_layout_name + 6;
		}
		if (video_layout_name) {
			conference->video_layout_name = switch_core_strdup(conference->pool, video_layout_name);
		}

		if (video_layout_group) {
			conference->video_layout_group = switch_core_strdup(conference->pool, video_layout_group);
		}

		if (!get_layout(conference, video_layout_name, video_layout_group)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid video-layout-name specified, using " CONFERENCE_MUX_DEFAULT_LAYOUT "\n");
			video_layout_name = CONFERENCE_MUX_DEFAULT_LAYOUT;
			video_layout_group = video_layout_name + 6;
			conference->video_layout_name = switch_core_strdup(conference->pool, video_layout_name);
			conference->video_layout_group = switch_core_strdup(conference->pool, video_layout_group);
		}

		if (!get_layout(conference, video_layout_name, video_layout_group)) {
			conference->video_layout_name = conference->video_layout_group = video_layout_group = video_layout_name = NULL;
			conference->conf_video_mode = CONF_VIDEO_MODE_TRANSCODE;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid conference layout settings, falling back to transcode mode\n");
		} else {
			conference->canvas_width = canvas_w;
			conference->canvas_height = canvas_h;
		}
	}

	if (conference->conf_video_mode == CONF_VIDEO_MODE_TRANSCODE || conference->conf_video_mode == CONF_VIDEO_MODE_MUX) {
		conference_set_flag(conference, CFLAG_TRANSCODE_VIDEO);
	}

	if (outcall_templ) {
		conference->outcall_templ = switch_core_strdup(conference->pool, outcall_templ);
	}
	conference->run_time = switch_epoch_time_now(NULL);

	if (!zstr(conference_log_dir)) {
		char *path;

		if (!strcmp(conference_log_dir, "auto")) {
			path = switch_core_sprintf(conference->pool, "%s%sconference_cdr", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR);
		} else if (!switch_is_file_path(conference_log_dir)) {
			path = switch_core_sprintf(conference->pool, "%s%s%s", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR, conference_log_dir);
		} else {
			path = switch_core_strdup(conference->pool, conference_log_dir);
		}

		switch_dir_make_recursive(path, SWITCH_DEFAULT_DIR_PERMS, conference->pool);
		conference->log_dir = path;

	}
	
	if (!zstr(cdr_event_mode)) {
		if (!strcmp(cdr_event_mode, "content")) {
			conference->cdr_event_mode = CDRE_AS_CONTENT;
		} else if (!strcmp(cdr_event_mode, "file")) {
			if (!zstr(conference->log_dir)) {
				conference->cdr_event_mode = CDRE_AS_FILE;
			} else {
				conference->cdr_event_mode = CDRE_NONE;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "'cdr-log-dir' parameter not set; CDR event mode 'file' ignored");
			}
		} else {
			conference->cdr_event_mode = CDRE_NONE;
		}
	}

	if (!zstr(perpetual_sound)) {
		conference->perpetual_sound = switch_core_strdup(conference->pool, perpetual_sound);
	}

	conference->mflags[MFLAG_CAN_SPEAK] = conference->mflags[MFLAG_CAN_HEAR] = conference->mflags[MFLAG_CAN_BE_SEEN] = 1;

	if (!zstr(moh_sound) && switch_is_moh(moh_sound)) {
		conference->moh_sound = switch_core_strdup(conference->pool, moh_sound);
	}

	if (member_flags) {
		set_mflags(member_flags, conference->mflags);
	}

	if (conference_flags) {
		set_cflags(conference_flags, conference->flags);
	}

	if (!zstr(sound_prefix)) {
		conference->sound_prefix = switch_core_strdup(conference->pool, sound_prefix);
	} else {
		const char *val;
		if ((val = switch_channel_get_variable(channel, "sound_prefix")) && !zstr(val)) {
			/* if no sound_prefix was set, use the channel sound_prefix */
			conference->sound_prefix = switch_core_strdup(conference->pool, val);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "using channel sound prefix: %s\n", conference->sound_prefix);
		}
	}

	if (!zstr(enter_sound)) {
		conference->enter_sound = switch_core_strdup(conference->pool, enter_sound);
	}

	if (!zstr(exit_sound)) {
		conference->exit_sound = switch_core_strdup(conference->pool, exit_sound);
	}

	if (!zstr(muted_sound)) {
		conference->muted_sound = switch_core_strdup(conference->pool, muted_sound);
	}

	if (zstr(mute_detect_sound)) {
		if (!zstr(muted_sound)) {
			conference->mute_detect_sound = switch_core_strdup(conference->pool, muted_sound);
		}
	} else {
		conference->mute_detect_sound = switch_core_strdup(conference->pool, mute_detect_sound);
	}

	if (!zstr(unmuted_sound)) {
		conference->unmuted_sound = switch_core_strdup(conference->pool, unmuted_sound);
	}

	if (!zstr(kicked_sound)) {
		conference->kicked_sound = switch_core_strdup(conference->pool, kicked_sound);
	}

	if (!zstr(join_only_sound)) {
		conference->join_only_sound = switch_core_strdup(conference->pool, join_only_sound);
	}

	if (!zstr(pin_sound)) {
		conference->pin_sound = switch_core_strdup(conference->pool, pin_sound);
	}

	if (!zstr(bad_pin_sound)) {
		conference->bad_pin_sound = switch_core_strdup(conference->pool, bad_pin_sound);
	}

	if (!zstr(pin)) {
		conference->pin = switch_core_strdup(conference->pool, pin);
	}

	if (!zstr(mpin)) {
		conference->mpin = switch_core_strdup(conference->pool, mpin);
	}

	if (!zstr(alone_sound)) {
		conference->alone_sound = switch_core_strdup(conference->pool, alone_sound);
	}

	if (!zstr(locked_sound)) {
		conference->locked_sound = switch_core_strdup(conference->pool, locked_sound);
	}

	if (!zstr(is_locked_sound)) {
		conference->is_locked_sound = switch_core_strdup(conference->pool, is_locked_sound);
	}

	if (!zstr(is_unlocked_sound)) {
		conference->is_unlocked_sound = switch_core_strdup(conference->pool, is_unlocked_sound);
	}

	if (!zstr(energy_level)) {
		conference->energy_level = atoi(energy_level);
		if (conference->energy_level < 0) {
			conference->energy_level = 0;
		}
	}

	if (!zstr(auto_gain_level)) {
		int level = 0;

		if (switch_true(auto_gain_level) && !switch_is_number(auto_gain_level)) {
			level = DEFAULT_AGC_LEVEL;
		} else {
			level = atoi(auto_gain_level);
		}

		if (level > 0 && level > conference->energy_level) {
			conference->agc_level = level;
		}
	}

	if (!zstr(maxmember_sound)) {
		conference->maxmember_sound = switch_core_strdup(conference->pool, maxmember_sound);
	}
	/* its going to be 0 by default, set to a value otherwise so this should be safe */
	conference->max_members = max_members;
	conference->announce_count = announce_count;

	conference->name = switch_core_strdup(conference->pool, name);

	if ((name_domain = strchr(conference->name, '@'))) {
		name_domain++;
		conference->domain = switch_core_strdup(conference->pool, name_domain);
	} else if (domain) {
		conference->domain = switch_core_strdup(conference->pool, domain);
	} else if (presence_id && (name_domain = strchr(presence_id, '@'))) {
		name_domain++;
		conference->domain = switch_core_strdup(conference->pool, name_domain);
	} else {
		conference->domain = "cluecon.com";
	}

	conference->chat_id = switch_core_sprintf(conference->pool, "conf+%s@%s", conference->name, conference->domain);

	conference->channels = channels;
	conference->rate = rate;
	conference->interval = interval;
	conference->ivr_dtmf_timeout = ivr_dtmf_timeout;
	conference->ivr_input_timeout = ivr_input_timeout;

	if (video_auto_floor_msec) {
		conference->video_floor_packets = video_auto_floor_msec / conference->interval;
	}

	conference->eflags = 0xFFFFFFFF;

	if (!zstr(suppress_events)) {
		clear_eflags(suppress_events, &conference->eflags);
	}

	if (!zstr(auto_record)) {
		conference->auto_record = switch_core_strdup(conference->pool, auto_record);
	}

	conference->min_recording_participants = min_recording_participants;

	if (!zstr(desc)) {
		conference->desc = switch_core_strdup(conference->pool, desc);
	}

	if (!zstr(terminate_on_silence)) {
		conference->terminate_on_silence = atoi(terminate_on_silence);
	}
	if (!zstr(endconf_grace_time)) {
		conference->endconf_grace_time = atoi(endconf_grace_time);
	}

	if (!zstr(verbose_events) && switch_true(verbose_events)) {
		conference->verbose_events = 1;
	}

	/* Create the conference unique identifier */
	switch_uuid_get(&uuid);
	switch_uuid_format(uuid_str, &uuid);
	conference->uuid_str = switch_core_strdup(conference->pool, uuid_str);

	/* Set enter sound and exit sound flags so that default is on */
	conference_set_flag(conference, CFLAG_ENTER_SOUND);
	conference_set_flag(conference, CFLAG_EXIT_SOUND);
	
	/* Activate the conference mutex for exclusivity */
	switch_mutex_init(&conference->mutex, SWITCH_MUTEX_NESTED, conference->pool);
	switch_mutex_init(&conference->flag_mutex, SWITCH_MUTEX_NESTED, conference->pool);
	switch_thread_rwlock_create(&conference->rwlock, conference->pool);
	switch_mutex_init(&conference->member_mutex, SWITCH_MUTEX_NESTED, conference->pool);
	switch_mutex_init(&conference->canvas_mutex, SWITCH_MUTEX_NESTED, conference->pool);

	switch_mutex_lock(globals.hash_mutex);
	conference_set_flag(conference, CFLAG_INHASH);
	switch_core_hash_insert(globals.conference_hash, conference->name, conference);
	switch_mutex_unlock(globals.hash_mutex);

	conference->super_canvas_label_layers = video_super_canvas_label_layers;
	conference->super_canvas_show_all_layers = video_super_canvas_show_all_layers;

	if (video_canvas_count < 1) video_canvas_count = 1;
	
	if (conference->conf_video_mode == CONF_VIDEO_MODE_MUX) {
		video_layout_t *vlayout = get_layout(conference, conference->video_layout_name, conference->video_layout_group);
		
		if (!vlayout) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot find layout\n");
			conference->video_layout_name = conference->video_layout_group = NULL;
			conference_clear_flag(conference, CFLAG_VIDEO_MUXING);
		} else {
			int j;

			for (j = 0; j < video_canvas_count; j++) {
				mcu_canvas_t *canvas = NULL;

				switch_mutex_lock(conference->canvas_mutex);
				init_canvas(conference, vlayout, &canvas);
				attach_canvas(conference, canvas, 0);
				launch_conference_video_muxing_thread(conference, canvas, 0);
				switch_mutex_unlock(conference->canvas_mutex);
			}

			if (conference->canvas_count > 1) {
				video_layout_t *svlayout = get_layout(conference, NULL, CONFERENCE_MUX_DEFAULT_SUPER_LAYOUT);
				mcu_canvas_t *canvas = NULL;

				if (svlayout) {
					switch_mutex_lock(conference->canvas_mutex);
					init_canvas(conference, svlayout, &canvas);
					set_canvas_bgcolor(canvas, conference->video_super_canvas_bgcolor);
					attach_canvas(conference, canvas, 1);
					launch_conference_video_muxing_thread(conference, canvas, 1);
					switch_mutex_unlock(conference->canvas_mutex);
				}
			}
		}
	}

  end:

	switch_mutex_unlock(globals.hash_mutex);

	return conference;
}

static void conference_send_presence(conference_obj_t *conference)
{
	switch_event_t *event;

	if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", CONF_CHAT_PROTO);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", conference->name);
		if (strchr(conference->name, '@')) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", conference->name);
		} else {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", conference->name, conference->domain);
		}

		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alt_event_type", "dialog");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_count", "%d", EC++);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "unique-id", conference->name);

		if (conference->count) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "force-status", "Active (%d caller%s)", conference->count, conference->count == 1 ? "" : "s");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-state", "CS_ROUTING");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "answer-state", conference->count == 1 ? "early" : "confirmed");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "presence-call-direction", conference->count == 1 ? "outbound" : "inbound");
		} else {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "force-status", "Inactive");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-state", "CS_HANGUP");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "answer-state", "terminated");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call-direction", "inbound");
		}



		switch_event_fire(&event);
	}
	
}
#if 0
static uint32_t kickall_matching_var(conference_obj_t *conference, const char *var, const char *val)
{
	conference_member_t *member = NULL;
	const char *vval = NULL;
	uint32_t r = 0;

	switch_mutex_lock(conference->mutex);
	switch_mutex_lock(conference->member_mutex);

	for (member = conference->members; member; member = member->next) {
		switch_channel_t *channel = NULL;

		if (member_test_flag(member, MFLAG_NOCHANNEL)) {
			continue;
		}

		channel = switch_core_session_get_channel(member->session);		
		vval = switch_channel_get_variable(channel, var);

		if (vval && !strcmp(vval, val)) {
			member_set_flag_locked(member, MFLAG_KICKED);
			member_clear_flag_locked(member, MFLAG_RUNNING);
			switch_core_session_kill_channel(member->session, SWITCH_SIG_BREAK);
			r++;
		}

	}	

	switch_mutex_unlock(conference->member_mutex);
	switch_mutex_unlock(conference->mutex);

	return r;
}
#endif

static void call_setup_event_handler(switch_event_t *event)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	conference_obj_t *conference = NULL;
	char *conf = switch_event_get_header(event, "Target-Component");
	char *domain = switch_event_get_header(event, "Target-Domain");
	char *dial_str = switch_event_get_header(event, "Request-Target");
	char *dial_uri = switch_event_get_header(event, "Request-Target-URI");
	char *action = switch_event_get_header(event, "Request-Action");
	char *ext = switch_event_get_header(event, "Request-Target-Extension");
	char *ext_domain = switch_event_get_header(event, "Request-Target-Domain");
	char *full_url = switch_event_get_header(event, "full_url");
	char *call_id = switch_event_get_header(event, "Request-Call-ID");

	if (!ext) ext = dial_str;

	if (!zstr(conf) && !zstr(dial_str) && !zstr(action) && (conference = conference_find(conf, domain))) {
		switch_event_t *var_event;
		switch_event_header_t *hp;

		if (conference_test_flag(conference, CFLAG_RFC4579)) {
			char *key = switch_mprintf("conf_%s_%s_%s_%s", conference->name, conference->domain, ext, ext_domain);
			char *expanded = NULL, *ostr = dial_str;;
			
			if (!strcasecmp(action, "call")) {
				if((conference->max_members > 0) && (conference->count >= conference->max_members)) {
					// Conference member limit has been reached; do not proceed with setup request
					status = SWITCH_STATUS_FALSE;
				} else {
					if (switch_event_create_plain(&var_event, SWITCH_EVENT_CHANNEL_DATA) != SWITCH_STATUS_SUCCESS) {
						abort();
					}

					for(hp = event->headers; hp; hp = hp->next) {
						if (!strncasecmp(hp->name, "var_", 4)) {
							switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, hp->name + 4, hp->value);
						}
					}

					switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "conference_call_key", key);
					switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "conference_destination_number", ext);

					switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "conference_invite_uri", dial_uri);

					switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "conference_track_status", "true");
					switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "conference_track_call_id", call_id);
					switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "sip_invite_domain", domain);
					switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "sip_invite_contact_params", "~isfocus");

					if (!strncasecmp(ostr, "url+", 4)) {
						ostr += 4;
					} else if (!switch_true(full_url) && conference->outcall_templ) {
						if ((expanded = switch_event_expand_headers(var_event, conference->outcall_templ))) {
							ostr = expanded;
						}
					}

					status = conference_outcall_bg(conference, NULL, NULL, ostr, 60, NULL, NULL, NULL, NULL, NULL, NULL, &var_event);

					if (expanded && expanded != conference->outcall_templ) {
						switch_safe_free(expanded);
					}
				}
				
			} else if (!strcasecmp(action, "end")) {
				if (switch_core_session_hupall_matching_var("conference_call_key", key, SWITCH_CAUSE_NORMAL_CLEARING)) {
					send_conference_notify(conference, "SIP/2.0 200 OK\r\n", call_id, SWITCH_TRUE);
				} else {
					send_conference_notify(conference, "SIP/2.0 481 Failure\r\n", call_id, SWITCH_TRUE);
				}
				status = SWITCH_STATUS_SUCCESS;
			}

			switch_safe_free(key);
		} else { // Conference found but doesn't support referral.
			status = SWITCH_STATUS_FALSE;
		}


		switch_thread_rwlock_unlock(conference->rwlock);
	} else { // Couldn't find associated conference.  Indicate failure on refer subscription
		status = SWITCH_STATUS_FALSE;
	}

	if(status != SWITCH_STATUS_SUCCESS) {
		// Unable to setup call, need to generate final NOTIFY
		if (switch_event_create(&event, SWITCH_EVENT_CONFERENCE_DATA) == SWITCH_STATUS_SUCCESS) {
			event->flags |= EF_UNIQ_HEADERS;

			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "conference-name", conf);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "conference-domain", domain);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "conference-event", "refer");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call_id", call_id);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "final", "true");
			switch_event_add_body(event, "%s", "SIP/2.0 481 Failure\r\n");
			switch_event_fire(&event);
		}
	}
	
}

static void conf_data_event_handler(switch_event_t *event)
{
	switch_event_t *revent;
	char *name = switch_event_get_header(event, "conference-name");
	char *domain = switch_event_get_header(event, "conference-domain");
	conference_obj_t *conference = NULL;
	char *body = NULL;

	if (!zstr(name) && (conference = conference_find(name, domain))) {
		if (conference_test_flag(conference, CFLAG_RFC4579)) {
			switch_event_dup(&revent, event);
			revent->event_id = SWITCH_EVENT_CONFERENCE_DATA;
			revent->flags |= EF_UNIQ_HEADERS;
			switch_event_add_header(revent, SWITCH_STACK_TOP, "Event-Name", "CONFERENCE_DATA");

			body = conference_rfc4579_render(conference, event, revent);
			switch_event_add_body(revent, "%s", body);
			switch_event_fire(&revent);
			switch_safe_free(body);
		}
		switch_thread_rwlock_unlock(conference->rwlock);
	}
}


static void pres_event_handler(switch_event_t *event)
{
	char *to = switch_event_get_header(event, "to");
	char *domain_name = NULL;
	char *dup_to = NULL, *conf_name, *dup_conf_name = NULL;
	conference_obj_t *conference;

	if (!to || strncasecmp(to, "conf+", 5) || !strchr(to, '@')) {
		return;
	}

	if (!(dup_to = strdup(to))) {
		return;
	}
	

	conf_name = dup_to + 5;

	if ((domain_name = strchr(conf_name, '@'))) {
		*domain_name++ = '\0';
	}

	dup_conf_name = switch_mprintf("%q@%q", conf_name, domain_name);
	

	if ((conference = conference_find(conf_name, NULL)) || (conference = conference_find(dup_conf_name, NULL))) {
		if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", CONF_CHAT_PROTO);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", conference->name);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", conference->name, conference->domain);


			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "force-status", "Active (%d caller%s)", conference->count, conference->count == 1 ? "" : "s");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alt_event_type", "dialog");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_count", "%d", EC++);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "unique-id", conf_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-state", "CS_ROUTING");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "answer-state", conference->count == 1 ? "early" : "confirmed");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call-direction", conference->count == 1 ? "outbound" : "inbound");
			switch_event_fire(&event);
		}
		switch_thread_rwlock_unlock(conference->rwlock);
	} else if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", CONF_CHAT_PROTO);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", conf_name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", to);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "force-status", "Idle");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", "unknown");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alt_event_type", "dialog");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_count", "%d", EC++);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "unique-id", conf_name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-state", "CS_HANGUP");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "answer-state", "terminated");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call-direction", "inbound");
		switch_event_fire(&event);
	}

	switch_safe_free(dup_to);
	switch_safe_free(dup_conf_name);
}

static void send_presence(switch_event_types_t id)
{
	switch_xml_t cxml, cfg, advertise, room;
	switch_event_t *params = NULL;

	switch_event_create(&params, SWITCH_EVENT_COMMAND);
	switch_assert(params);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "presence", "true");


	/* Open the config from the xml registry */
	if (!(cxml = switch_xml_open_cfg(global_cf_name, &cfg, params))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", global_cf_name);
		goto done;
	}

	if ((advertise = switch_xml_child(cfg, "advertise"))) {
		for (room = switch_xml_child(advertise, "room"); room; room = room->next) {
			char *name = (char *) switch_xml_attr_soft(room, "name");
			char *status = (char *) switch_xml_attr_soft(room, "status");
			switch_event_t *event;

			if (name && switch_event_create(&event, id) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", CONF_CHAT_PROTO);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", name);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", name);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "force-status", status ? status : "Available");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", "unknown");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
				switch_event_fire(&event);
			}
		}
	}

  done:
	switch_event_destroy(&params);

	/* Release the config registry handle */
	if (cxml) {
		switch_xml_free(cxml);
		cxml = NULL;
	}
}

typedef void (*conf_key_callback_t) (conference_member_t *, struct caller_control_actions *);

typedef struct {
	conference_member_t *member;
	caller_control_action_t action;
	conf_key_callback_t handler;
} key_binding_t;


static switch_status_t dmachine_dispatcher(switch_ivr_dmachine_match_t *match)
{
	key_binding_t *binding = match->user_data;
	switch_channel_t *channel;

	if (!binding) return SWITCH_STATUS_FALSE;

	channel = switch_core_session_get_channel(binding->member->session);
	switch_channel_set_variable(channel, "conference_last_matching_digits", match->match_digits);

	if (binding->action.data) {
		binding->action.expanded_data = switch_channel_expand_variables(channel, binding->action.data);
	}

	binding->handler(binding->member, &binding->action);

	if (binding->action.expanded_data != binding->action.data) {
		free(binding->action.expanded_data);
		binding->action.expanded_data = NULL;
	}

	member_set_flag_locked(binding->member, MFLAG_FLUSH_BUFFER);

	return SWITCH_STATUS_SUCCESS;
}

static void do_binding(conference_member_t *member, conf_key_callback_t handler, const char *digits, const char *data)
{
	key_binding_t *binding;

	binding = switch_core_alloc(member->pool, sizeof(*binding));
	binding->member = member;

	binding->action.binded_dtmf = switch_core_strdup(member->pool, digits);

	if (data) {
		binding->action.data = switch_core_strdup(member->pool, data);
	}

	binding->handler = handler;
	switch_ivr_dmachine_bind(member->dmachine, "conf", digits, 0, dmachine_dispatcher, binding);
	
}

struct _mapping {
	const char *name;
	conf_key_callback_t handler;
};

static struct _mapping control_mappings[] = {
    {"mute", conference_loop_fn_mute_toggle},
    {"mute on", conference_loop_fn_mute_on},
    {"mute off", conference_loop_fn_mute_off},
    {"vmute", conference_loop_fn_vmute_toggle},
    {"vmute on", conference_loop_fn_vmute_on},
    {"vmute off", conference_loop_fn_vmute_off},
    {"vmute snap", conference_loop_fn_vmute_snap},
    {"vmute snapoff", conference_loop_fn_vmute_snapoff},
    {"deaf mute", conference_loop_fn_deafmute_toggle},
    {"energy up", conference_loop_fn_energy_up},
    {"energy equ", conference_loop_fn_energy_equ_conf},
    {"energy dn", conference_loop_fn_energy_dn},
    {"vol talk up", conference_loop_fn_volume_talk_up},
    {"vol talk zero", conference_loop_fn_volume_talk_zero},
    {"vol talk dn", conference_loop_fn_volume_talk_dn},
    {"vol listen up", conference_loop_fn_volume_listen_up},
    {"vol listen zero", conference_loop_fn_volume_listen_zero},
    {"vol listen dn", conference_loop_fn_volume_listen_dn},
    {"hangup", conference_loop_fn_hangup},
    {"event", conference_loop_fn_event},
    {"lock", conference_loop_fn_lock_toggle},
    {"transfer", conference_loop_fn_transfer},
    {"execute_application", conference_loop_fn_exec_app},
    {"floor", conference_loop_fn_floor_toggle},
    {"vid-floor", conference_loop_fn_vid_floor_toggle},
    {"vid-floor-force", conference_loop_fn_vid_floor_force}
};
#define MAPPING_LEN (sizeof(control_mappings)/sizeof(control_mappings[0]))

static void member_bind_controls(conference_member_t *member, const char *controls)
{
	switch_xml_t cxml, cfg, xgroups, xcontrol;
	switch_event_t *params;
	int i;

	switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "Conf-Name", member->conference->name);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "Action", "request-controls");
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "Controls", controls);

	if (!(cxml = switch_xml_open_cfg(global_cf_name, &cfg, params))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", global_cf_name);
		goto end;
	}

	if (!(xgroups = switch_xml_child(cfg, "caller-controls"))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't find caller-controls in %s\n", global_cf_name);
		goto end;
	}

	if (!(xgroups = switch_xml_find_child(xgroups, "group", "name", controls))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't find group '%s' in caller-controls section of %s\n", switch_str_nil(controls), global_cf_name);
		goto end;
	}


	for (xcontrol = switch_xml_child(xgroups, "control"); xcontrol; xcontrol = xcontrol->next) {
        const char *key = switch_xml_attr(xcontrol, "action");
        const char *digits = switch_xml_attr(xcontrol, "digits");
        const char *data = switch_xml_attr_soft(xcontrol, "data");

		if (zstr(key) || zstr(digits)) continue;

		for(i = 0; i < MAPPING_LEN; i++) {
			if (!strcasecmp(key, control_mappings[i].name)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s binding '%s' to '%s'\n", 
								  switch_core_session_get_name(member->session), digits, key);

				do_binding(member, control_mappings[i].handler, digits, data);
			}
		}
	}

 end:

	/* Release the config registry handle */
	if (cxml) {
		switch_xml_free(cxml);
		cxml = NULL;
	}
	
	if (params) switch_event_destroy(&params);
	
}




/* Called by FreeSWITCH when the module loads */
SWITCH_MODULE_LOAD_FUNCTION(mod_conference_load)
{
	uint32_t i;
	size_t nl, ol = 0;
	char *p = NULL, *tmp = NULL;
	switch_chat_interface_t *chat_interface;
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char cmd_str[256];

	memset(&globals, 0, sizeof(globals));

	/* Connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	switch_console_add_complete_func("::conference::list_conferences", list_conferences);
	

	switch_event_channel_bind("conference", conference_event_channel_handler, &globals.event_channel_id);
	switch_event_channel_bind("conference-liveArray", conference_la_event_channel_handler, &globals.event_channel_id);
	switch_event_channel_bind("conference-mod", conference_mod_event_channel_handler, &globals.event_channel_id);

	/* build api interface help ".syntax" field string */
	p = strdup("");
	for (i = 0; i < CONFFUNCAPISIZE; i++) {
		nl = strlen(conf_api_sub_commands[i].pcommand) + strlen(conf_api_sub_commands[i].psyntax) + 5;

		switch_snprintf(cmd_str, sizeof(cmd_str), "add conference ::conference::list_conferences %s", conf_api_sub_commands[i].pcommand);
		switch_console_set_complete(cmd_str);

		if (p != NULL) {
			ol = strlen(p);
		}
		tmp = realloc(p, ol + nl);
		if (tmp != NULL) {
			p = tmp;
			strcat(p, "\t\t");
			strcat(p, conf_api_sub_commands[i].pcommand);
			if (!zstr(conf_api_sub_commands[i].psyntax)) {
				strcat(p, " ");
				strcat(p, conf_api_sub_commands[i].psyntax);
			}
			if (i < CONFFUNCAPISIZE - 1) {
				strcat(p, "\n");
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't realloc\n");
			return SWITCH_STATUS_TERM;
		}

	}
	api_syntax = p;

	/* create/register custom event message type */
	if (switch_event_reserve_subclass(CONF_EVENT_MAINT) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", CONF_EVENT_MAINT);
		return SWITCH_STATUS_TERM;
	}

	/* Setup the pool */
	globals.conference_pool = pool;

	/* Setup a hash to store conferences by name */
	switch_core_hash_init(&globals.conference_hash);
	switch_mutex_init(&globals.conference_mutex, SWITCH_MUTEX_NESTED, globals.conference_pool);
	switch_mutex_init(&globals.id_mutex, SWITCH_MUTEX_NESTED, globals.conference_pool);
	switch_mutex_init(&globals.hash_mutex, SWITCH_MUTEX_NESTED, globals.conference_pool);
	switch_mutex_init(&globals.setup_mutex, SWITCH_MUTEX_NESTED, globals.conference_pool);

	/* Subscribe to presence request events */
	if (switch_event_bind(modname, SWITCH_EVENT_PRESENCE_PROBE, SWITCH_EVENT_SUBCLASS_ANY, pres_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't subscribe to presence request events!\n");
	}

	if (switch_event_bind(modname, SWITCH_EVENT_CONFERENCE_DATA_QUERY, SWITCH_EVENT_SUBCLASS_ANY, conf_data_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't subscribe to conference data query events!\n");
	}

	if (switch_event_bind(modname, SWITCH_EVENT_CALL_SETUP_REQ, SWITCH_EVENT_SUBCLASS_ANY, call_setup_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't subscribe to conference data query events!\n");
	}

	SWITCH_ADD_API(api_interface, "conference", "Conference module commands", conf_api_main, p);
	SWITCH_ADD_APP(app_interface, global_app_name, global_app_name, NULL, conference_function, NULL, SAF_NONE);
	SWITCH_ADD_APP(app_interface, "conference_set_auto_outcall", "conference_set_auto_outcall", NULL, conference_auto_function, NULL, SAF_NONE);
	SWITCH_ADD_CHAT(chat_interface, CONF_CHAT_PROTO, chat_send);

	send_presence(SWITCH_EVENT_PRESENCE_IN);

	globals.running = 1;
	/* indicate that the module should continue to be loaded */
	return status;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_conference_shutdown)
{
	if (globals.running) {

		/* signal all threads to shutdown */
		globals.running = 0;

		switch_event_channel_unbind(NULL, conference_event_channel_handler);
		switch_event_channel_unbind(NULL, conference_la_event_channel_handler);

		switch_console_del_complete_func("::conference::list_conferences");

		/* wait for all threads */
		while (globals.threads) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Waiting for %d threads\n", globals.threads);
			switch_yield(100000);
		}

		switch_event_unbind_callback(pres_event_handler);
		switch_event_unbind_callback(conf_data_event_handler);
		switch_event_unbind_callback(call_setup_event_handler);
		switch_event_free_subclass(CONF_EVENT_MAINT);

		/* free api interface help ".syntax" field string */
		switch_safe_free(api_syntax);
	}
	switch_core_hash_destroy(&globals.conference_hash);

	return SWITCH_STATUS_SUCCESS;
}

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
