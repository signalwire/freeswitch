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
 * William King <william.king@quentustech.com>
 *
 * mod_conference.c -- Software Conference Bridge
 *
 */

#ifndef MOD_CONFERENCE_H
#define MOD_CONFERENCE_H

#include <switch.h>

/* DEFINES */

#ifdef OPENAL_POSITIONING
#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#endif

#define DEFAULT_LAYER_TIMEOUT 10
#define DEFAULT_AGC_LEVEL 1100
#define CONFERENCE_UUID_VARIABLE "conference_uuid"

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

#define lock_member(_member) switch_mutex_lock(_member->write_mutex); switch_mutex_lock(_member->read_mutex)
#define unlock_member(_member) switch_mutex_unlock(_member->read_mutex); switch_mutex_unlock(_member->write_mutex)

//#define lock_member(_member) switch_mutex_lock(_member->write_mutex)
//#define unlock_member(_member) switch_mutex_unlock(_member->write_mutex)

#define CONFFUNCAPISIZE (sizeof(conf_api_sub_commands)/sizeof(conf_api_sub_commands[0]))

#define MAX_MUX_CODECS 10

#define ALC_HRTF_SOFT  0x1992

#define validate_pin(buf, pin, mpin) \
	pin_valid = (!zstr(pin) && strcmp(buf, pin) == 0);	\
	if (!pin_valid && !zstr(mpin) && strcmp(buf, mpin) == 0) {			\
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Moderator PIN found!\n"); \
		pin_valid = 1; \
		mpin_matched = 1; \
	} 

/* STRUCTS */

struct conf_fps {
	float fps;
	int ms;
	int samples;
};


typedef enum {
	CONF_SILENT_REQ = (1 << 0),
	CONF_SILENT_DONE = (1 << 1)
} conf_app_flag_t;

extern char *mod_conference_cf_name;
extern char *api_syntax;
extern int EC;

typedef enum {
	FILE_STOP_CURRENT,
	FILE_STOP_ALL,
	FILE_STOP_ASYNC
} file_stop_t;

/* Global Values */
typedef struct conf_globals_s {
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
} conf_globals_t;

extern conf_globals_t conf_globals;

/* forward declaration for conference_obj and caller_control */
struct conference_member;
typedef struct conference_member conf_member_t;

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

typedef struct conf_cdr_node_s {
	switch_caller_profile_t *cp;
	char *record_path;
	switch_time_t join_time;
	switch_time_t leave_time;
	member_flag_t mflags[MFLAG_MAX];
	uint32_t id;
	conf_member_t *member;
	switch_event_t *var_event;
	struct conf_cdr_node_s *next;
} conf_cdr_node_t;

typedef enum {
	CDRR_LOCKED = 1,
	CDRR_PIN,
	CDRR_MAXMEMBERS
} cdr_reject_reason_t;

typedef struct conf_cdr_reject_s {
	switch_caller_profile_t *cp;
	switch_time_t reject_time;
	cdr_reject_reason_t reason;
	struct conf_cdr_reject_s *next;
} conf_cdr_reject_t;

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

void conf_al_close(al_handle_t *al);
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
	conf_member_t *member_a;
	conf_member_t *member_b;
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
} conf_record_t;

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
	conf_member_t *members;
	conf_member_t *floor_holder;
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
	conf_cdr_node_t *cdr_nodes;
	conf_cdr_reject_t *cdr_rejected;
	switch_time_t start_time;
	switch_time_t end_time;
	char *log_dir;
	cdr_event_mode_t cdr_event_mode;
	struct vid_helper vh[2];
	struct vid_helper mh;
	conf_record_t *rec_node_head;
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
	conf_record_t *rec;
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
	conf_cdr_node_t *cdr_node;
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
	int layer_timeout;
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

typedef struct codec_set_s {
	switch_codec_t codec;
	switch_frame_t frame;
	uint8_t *packet;
} codec_set_t;

typedef void (*conf_key_callback_t) (conf_member_t *, struct caller_control_actions *);

typedef struct {
	conf_member_t *member;
	caller_control_action_t action;
	conf_key_callback_t handler;
} key_binding_t;

struct _mapping {
	const char *name;
	conf_key_callback_t handler;
};

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

/* FUNCTION DEFINITIONS */


switch_bool_t conf_utils_test_flag(conference_obj_t *conference, conference_flag_t flag);
conference_relationship_t *conf_member_get_relationship(conf_member_t *member, conf_member_t *other_member);

uint32_t next_member_id(void);
void conf_utils_set_cflags(const char *flags, conference_flag_t *f);
void conf_utils_set_mflags(const char *flags, member_flag_t *f);
void conf_utils_merge_mflags(member_flag_t *a, member_flag_t *b);
void conf_utils_clear_eflags(char *events, uint32_t *f);
void conf_event_pres_handler(switch_event_t *event);
void conf_data_event_handler(switch_event_t *event);
void conf_event_call_setup_handler(switch_event_t *event);
void conf_member_add_file_data(conf_member_t *member, int16_t *data, switch_size_t file_data_len);
void conf_send_notify(conference_obj_t *conference, const char *status, const char *call_id, switch_bool_t final);
switch_status_t conference_file_close(conference_obj_t *conference, conference_file_node_t *node);
void *SWITCH_THREAD_FUNC conf_record_thread_run(switch_thread_t *thread, void *obj);

void conf_al_gen_arc(conference_obj_t *conference, switch_stream_handle_t *stream);
void conf_al_process(al_handle_t *al, void *data, switch_size_t datalen, int rate);

void conf_utils_member_set_flag_locked(conf_member_t *member, member_flag_t flag);
void conf_utils_member_set_flag(conf_member_t *member, member_flag_t flag);

void conf_member_update_status_field(conf_member_t *member);
void conf_video_vmute_snap(conf_member_t *member, switch_bool_t clear);
void conf_video_reset_video_bitrate_counters(conf_member_t *member);
void conf_video_clear_layer(mcu_layer_t *layer);
int conf_member_get_canvas_id(conf_member_t *member, const char *val, switch_bool_t watching);
void conf_video_reset_member_codec_index(conf_member_t *member);
void conf_video_detach_video_layer(conf_member_t *member);
void conf_utils_set_flag(conference_obj_t *conference, conference_flag_t flag);
void conf_utils_set_flag_locked(conference_obj_t *conference, conference_flag_t flag);
void conf_utils_clear_flag(conference_obj_t *conference, conference_flag_t flag);
void conf_utils_clear_flag_locked(conference_obj_t *conference, conference_flag_t flag);
switch_status_t conf_loop_dmachine_dispatcher(switch_ivr_dmachine_match_t *match);

int conf_member_setup_media(conf_member_t *member, conference_obj_t *conference);

al_handle_t *conf_al_create(switch_memory_pool_t *pool);
switch_status_t conf_member_parse_position(conf_member_t *member, const char *data);
video_layout_t *conf_video_find_best_layout(conference_obj_t *conference, layout_group_t *lg, uint32_t count);
void conf_list_count_only(conference_obj_t *conference, switch_stream_handle_t *stream);
void conf_member_set_floor_holder(conference_obj_t *conference, conf_member_t *member);
void conf_utils_member_clear_flag(conf_member_t *member, member_flag_t flag);
void conf_utils_member_clear_flag_locked(conf_member_t *member, member_flag_t flag);
switch_status_t conf_video_attach_video_layer(conf_member_t *member, mcu_canvas_t *canvas, int idx);
int conf_video_set_fps(conference_obj_t *conference, float fps);
void conf_video_layer_set_logo(conf_member_t *member, mcu_layer_t *layer, const char *path);
void conf_video_layer_set_banner(conf_member_t *member, mcu_layer_t *layer, const char *text);
void conf_fnode_seek(conference_file_node_t *fnode, switch_stream_handle_t *stream, char *arg);
uint32_t conf_member_stop_file(conf_member_t *member, file_stop_t stop);
switch_bool_t conf_utils_member_test_flag(conf_member_t *member, member_flag_t flag);
void conf_list_pretty(conference_obj_t *conference, switch_stream_handle_t *stream);
switch_status_t conf_record_stop(conference_obj_t *conference, switch_stream_handle_t *stream, char *path);
switch_status_t conf_record_action(conference_obj_t *conference, char *path, recording_action_type_t action);
void conf_xlist(conference_obj_t *conference, switch_xml_t x_conference, int off);
void conf_event_send_json(conference_obj_t *conference);
void conf_event_send_rfc(conference_obj_t *conference);
void conf_member_update_status_field(conf_member_t *member);
void conf_event_la_command_handler(switch_live_array_t *la, const char *cmd, const char *sessid, cJSON *jla, void *user_data);
void conf_event_adv_la(conference_obj_t *conference, conf_member_t *member, switch_bool_t join);
switch_status_t conf_video_init_canvas(conference_obj_t *conference, video_layout_t *vlayout, mcu_canvas_t **canvasP);
switch_status_t conf_video_attach_canvas(conference_obj_t *conference, mcu_canvas_t *canvas, int super);
void conf_video_init_canvas_layers(conference_obj_t *conference, mcu_canvas_t *canvas, video_layout_t *vlayout);
switch_status_t conf_video_attach_video_layer(conf_member_t *member, mcu_canvas_t *canvas, int idx);
void conf_video_reset_video_bitrate_counters(conf_member_t *member);
void conf_video_layer_set_banner(conf_member_t *member, mcu_layer_t *layer, const char *text);
void conf_video_layer_set_logo(conf_member_t *member, mcu_layer_t *layer, const char *path);
void conf_video_detach_video_layer(conf_member_t *member);
void conf_video_check_used_layers(mcu_canvas_t *canvas);
void conf_video_set_canvas_letterbox_bgcolor(mcu_canvas_t *canvas, char *color);
void conf_video_set_canvas_bgcolor(mcu_canvas_t *canvas, char *color);
void conf_video_scale_and_patch(mcu_layer_t *layer, switch_image_t *ximg, switch_bool_t freeze);
void conf_video_reset_layer(mcu_layer_t *layer);
void conf_video_clear_layer(mcu_layer_t *layer);
void conf_video_reset_image(switch_image_t *img, switch_rgb_color_t *color);
void conf_video_parse_layouts(conference_obj_t *conference, int WIDTH, int HEIGHT);
int conf_video_set_fps(conference_obj_t *conference, float fps);
video_layout_t *conf_video_get_layout(conference_obj_t *conference, const char *video_layout_name, const char *video_layout_group);
void conf_video_check_avatar(conf_member_t *member, switch_bool_t force);
void conf_video_find_floor(conf_member_t *member, switch_bool_t entering);
void conf_video_destroy_canvas(mcu_canvas_t **canvasP);
void conf_video_fnode_check(conference_file_node_t *fnode);
switch_status_t conf_al_parse_position(al_handle_t *al, const char *data);
switch_status_t conf_video_thread_callback(switch_core_session_t *session, switch_frame_t *frame, void *user_data);
void *SWITCH_THREAD_FUNC conf_video_muxing_write_thread_run(switch_thread_t *thread, void *obj);
void conf_member_check_agc_levels(conf_member_t *member);
void conf_member_clear_avg(conf_member_t *member);
int conf_member_noise_gate_check(conf_member_t *member);
void conf_member_check_channels(switch_frame_t *frame, conf_member_t *member, switch_bool_t in);

void conf_fnode_toggle_pause(conference_file_node_t *fnode, switch_stream_handle_t *stream);

// static conference_relationship_t *conf_member_get_relationship(conf_member_t *member, conf_member_t *other_member);
// static void conference_list(conference_obj_t *conference, switch_stream_handle_t *stream, char *delim);

conference_relationship_t *conf_member_add_relationship(conf_member_t *member, uint32_t id);
conf_member_t *conf_member_get(conference_obj_t *conference, uint32_t id);

switch_status_t conf_member_del_relationship(conf_member_t *member, uint32_t id);
switch_status_t conf_member_add(conference_obj_t *conference, conf_member_t *member);
switch_status_t conf_member_del(conference_obj_t *conference, conf_member_t *member);
void *SWITCH_THREAD_FUNC conf_thread_run(switch_thread_t *thread, void *obj);
void *SWITCH_THREAD_FUNC conf_video_muxing_thread_run(switch_thread_t *thread, void *obj);
void *SWITCH_THREAD_FUNC conf_video_super_muxing_thread_run(switch_thread_t *thread, void *obj);
void conf_loop_output(conf_member_t *member);
uint32_t conference_file_stop(conference_obj_t *conference, file_stop_t stop);
switch_status_t conference_file_play(conference_obj_t *conference, char *file, uint32_t leadin, switch_channel_t *channel, uint8_t async);
void conf_member_send_all_dtmf(conf_member_t *member, conference_obj_t *conference, const char *dtmf);
switch_status_t conf_say(conference_obj_t *conference, const char *text, uint32_t leadin);
conference_obj_t *conf_find(char *name, char *domain);
void conf_member_bind_controls(conf_member_t *member, const char *controls);
void conf_send_presence(conference_obj_t *conference);
void conf_video_set_floor_holder(conference_obj_t *conference, conf_member_t *member, switch_bool_t force);
void conf_video_canvas_del_fnode_layer(conference_obj_t *conference, conference_file_node_t *fnode);
void conf_video_canvas_set_fnode_layer(mcu_canvas_t *canvas, conference_file_node_t *fnode, int idx);
void conference_list(conference_obj_t *conference, switch_stream_handle_t *stream, char *delim);
const char *conf_utils_combine_flag_var(switch_core_session_t *session, const char *var_name);
int conf_loop_mapping_len();

switch_status_t conf_outcall(conference_obj_t *conference,
				   char *conference_name,
				   switch_core_session_t *session,
				   char *bridgeto, uint32_t timeout, 
				   char *flags, 
				   char *cid_name, 
				   char *cid_num,
				   char *profile,
				   switch_call_cause_t *cause,
				   switch_call_cause_t *cancel_cause, switch_event_t *var_event);
switch_status_t conf_outcall_bg(conference_obj_t *conference,
				      char *conference_name,
				      switch_core_session_t *session, char *bridgeto, uint32_t timeout, const char *flags, const char *cid_name,
				      const char *cid_num, const char *call_uuid, const char *profile, switch_call_cause_t *cancel_cause, 
				      switch_event_t **var_event);

void conf_video_launch_muxing_thread(conference_obj_t *conference, mcu_canvas_t *canvas, int super);
void conf_launch_thread(conference_obj_t *conference);
void conf_video_launch_muxing_write_thread(conf_member_t *member);
void *SWITCH_THREAD_FUNC conf_loop_input(switch_thread_t *thread, void *obj);
switch_status_t conference_file_local_play(conference_obj_t *conference, switch_core_session_t *session, char *path, uint32_t leadin, void *buf,
												  uint32_t buflen);
switch_status_t conf_member_play_file(conf_member_t *member, char *file, uint32_t leadin, switch_bool_t mux);
switch_status_t conf_member_say(conf_member_t *member, char *text, uint32_t leadin);
uint32_t conf_member_stop_file(conf_member_t *member, file_stop_t stop);
conference_obj_t *conf_new(char *name, conf_xml_cfg_t cfg, switch_core_session_t *session, switch_memory_pool_t *pool);
switch_status_t chat_send(switch_event_t *message_event);
								 

void conf_record_launch_thread(conference_obj_t *conference, char *path, switch_bool_t autorec);

typedef switch_status_t (*conf_api_args_cmd_t) (conference_obj_t *, switch_stream_handle_t *, int, char **);
typedef switch_status_t (*conf_api_member_cmd_t) (conf_member_t *, switch_stream_handle_t *, void *);
typedef switch_status_t (*conf_api_text_cmd_t) (conference_obj_t *, switch_stream_handle_t *, const char *);

switch_status_t conf_event_add_data(conference_obj_t *conference, switch_event_t *event);
switch_status_t conf_member_add_event_data(conf_member_t *member, switch_event_t *event);

cJSON *conf_cdr_json_render(conference_obj_t *conference, cJSON *req);
char *conf_cdr_rfc4579_render(conference_obj_t *conference, switch_event_t *event, switch_event_t *revent);
void conf_cdr_del(conf_member_t *member);
void conf_cdr_add(conf_member_t *member);
void conf_cdr_rejected(conference_obj_t *conference, switch_channel_t *channel, cdr_reject_reason_t reason);
void conf_cdr_render(conference_obj_t *conference);
void conf_event_channel_handler(const char *event_channel, cJSON *json, const char *key, switch_event_channel_id_t id);
void conf_event_la_channel_handler(const char *event_channel, cJSON *json, const char *key, switch_event_channel_id_t id);
void conf_event_mod_channel_handler(const char *event_channel, cJSON *json, const char *key, switch_event_channel_id_t id);


void conf_member_itterator(conference_obj_t *conference, switch_stream_handle_t *stream, uint8_t non_mod, conf_api_member_cmd_t pfncallback, void *data);

switch_status_t conf_api_sub_mute(conf_member_t *member, switch_stream_handle_t *stream, void *data);
switch_status_t conf_api_sub_tmute(conf_member_t *member, switch_stream_handle_t *stream, void *data);
switch_status_t conf_api_sub_unmute(conf_member_t *member, switch_stream_handle_t *stream, void *data);
switch_status_t conf_api_sub_vmute(conf_member_t *member, switch_stream_handle_t *stream, void *data);
switch_status_t conf_api_sub_tvmute(conf_member_t *member, switch_stream_handle_t *stream, void *data);
switch_status_t conf_api_sub_unvmute(conf_member_t *member, switch_stream_handle_t *stream, void *data);
switch_status_t conf_api_sub_deaf(conf_member_t *member, switch_stream_handle_t *stream, void *data);
switch_status_t conf_api_sub_undeaf(conf_member_t *member, switch_stream_handle_t *stream, void *data);
switch_status_t conf_api_sub_floor(conf_member_t *member, switch_stream_handle_t *stream, void *data);
switch_status_t conf_api_sub_vid_floor(conf_member_t *member, switch_stream_handle_t *stream, void *data);
switch_status_t conf_api_sub_clear_vid_floor(conference_obj_t *conference, switch_stream_handle_t *stream, void *data);
switch_status_t conf_api_sub_position(conf_member_t *member, switch_stream_handle_t *stream, void *data);
switch_status_t conf_api_sub_conf_video_vmute_snap(conf_member_t *member, switch_stream_handle_t *stream, void *data);
switch_status_t conf_api_sub_dtmf(conf_member_t *member, switch_stream_handle_t *stream, void *data);
switch_status_t conf_api_sub_pause_play(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_play(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_say(conference_obj_t *conference, switch_stream_handle_t *stream, const char *text);
switch_status_t conf_api_sub_dial(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_agc(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_bgdial(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_auto_position(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_saymember(conference_obj_t *conference, switch_stream_handle_t *stream, const char *text);
switch_status_t conf_api_sub_check_record(conference_obj_t *conference, switch_stream_handle_t *stream, int arc, char **argv);
switch_status_t conf_api_sub_check_record(conference_obj_t *conference, switch_stream_handle_t *stream, int arc, char **argv);
switch_status_t conf_api_sub_volume_in(conf_member_t *member, switch_stream_handle_t *stream, void *data);
switch_status_t conf_api_sub_file_seek(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_stop(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_hup(conf_member_t *member, switch_stream_handle_t *stream, void *data);
switch_status_t conf_api_sub_pauserec(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_volume_out(conf_member_t *member, switch_stream_handle_t *stream, void *data);
switch_status_t conf_api_sub_lock(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_unlock(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_relate(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_pin(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_exit_sound(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_vid_banner(conf_member_t *member, switch_stream_handle_t *stream, void *data);
switch_status_t conf_api_sub_enter_sound(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_set(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_vid_res_id(conf_member_t *member, switch_stream_handle_t *stream, void *data);
switch_status_t conf_api_sub_get(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_vid_mute_img(conf_member_t *member, switch_stream_handle_t *stream, void *data);
switch_status_t conf_api_sub_vid_logo_img(conf_member_t *member, switch_stream_handle_t *stream, void *data);
switch_status_t conf_api_sub_vid_fps(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_write_png(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_file_vol(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_recording(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_vid_layout(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_list(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_xml_list(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_energy(conf_member_t *member, switch_stream_handle_t *stream, void *data);
switch_status_t conf_api_sub_watching_canvas(conf_member_t *member, switch_stream_handle_t *stream, void *data);
switch_status_t conf_api_sub_canvas(conf_member_t *member, switch_stream_handle_t *stream, void *data);
switch_status_t conf_api_sub_layer(conf_member_t *member, switch_stream_handle_t *stream, void *data);
switch_status_t conf_api_sub_kick(conf_member_t *member, switch_stream_handle_t *stream, void *data);
switch_status_t conf_api_sub_transfer(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_record(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_norecord(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_sub_vid_bandwidth(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv);
switch_status_t conf_api_dispatch(conference_obj_t *conference, switch_stream_handle_t *stream, int argc, char **argv, const char *cmdline, int argn);
switch_status_t conf_api_sub_syntax(char **syntax);
switch_status_t conf_api_main_real(const char *cmd, switch_core_session_t *session, switch_stream_handle_t *stream);


void conf_loop_mute_on(conf_member_t *member, caller_control_action_t *action);
void conf_loop_mute_toggle(conf_member_t *member, caller_control_action_t *action);
void conf_loop_energy_dn(conf_member_t *member, caller_control_action_t *action);
void conf_loop_energy_equ_conf(conf_member_t *member, caller_control_action_t *action);
void conf_loop_volume_talk_zero(conf_member_t *member, caller_control_action_t *action);
void conf_loop_volume_talk_up(conf_member_t *member, caller_control_action_t *action);
void conf_loop_volume_listen_dn(conf_member_t *member, caller_control_action_t *action);
void conf_loop_lock_toggle(conf_member_t *member, caller_control_action_t *action);
void conf_loop_volume_listen_up(conf_member_t *member, caller_control_action_t *action);
void conf_loop_volume_listen_zero(conf_member_t *member, caller_control_action_t *action);
void conf_loop_volume_talk_dn(conf_member_t *member, caller_control_action_t *action);
void conf_loop_energy_up(conf_member_t *member, caller_control_action_t *action);
void conf_loop_floor_toggle(conf_member_t *member, caller_control_action_t *action);
void conf_loop_vid_floor_toggle(conf_member_t *member, caller_control_action_t *action);
void conf_loop_energy_up(conf_member_t *member, caller_control_action_t *action);
void conf_loop_floor_toggle(conf_member_t *member, caller_control_action_t *action);
void conf_loop_vid_floor_force(conf_member_t *member, caller_control_action_t *action);
void conf_loop_vmute_off(conf_member_t *member, caller_control_action_t *action);
void conf_loop_conf_video_vmute_snap(conf_member_t *member, caller_control_action_t *action);
void conf_loop_conf_video_vmute_snapoff(conf_member_t *member, caller_control_action_t *action);
void conf_loop_vmute_toggle(conf_member_t *member, caller_control_action_t *action);
void conf_loop_vmute_on(conf_member_t *member, caller_control_action_t *action);
void conf_loop_deafmute_toggle(conf_member_t *member, caller_control_action_t *action);
void conf_loop_hangup(conf_member_t *member, caller_control_action_t *action);
void conf_loop_transfer(conf_member_t *member, caller_control_action_t *action);
void conf_loop_mute_off(conf_member_t *member, caller_control_action_t *action);
void conf_loop_event(conf_member_t *member, caller_control_action_t *action);
void conf_loop_transfer(conf_member_t *member, caller_control_action_t *action);
void conf_loop_exec_app(conf_member_t *member, caller_control_action_t *action);



/* Global Structs */


/* API Interface Function sub-commands */
/* Entries in this list should be kept in sync with the enum above */
extern api_command_t conf_api_sub_commands[];
extern struct _mapping control_mappings[];


#endif /* MOD_CONFERENCE_H */
